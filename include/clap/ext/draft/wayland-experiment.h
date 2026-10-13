#pragma once

#include <clap/ext/gui.h>
#include <clap/plugin.h>

/// @page Wayland Experiment
///
/// Experimental extensions for plugin windows on Wayland, and for the keyboard on X11 and
/// Wayland.
///
/// Wayland doesn't let a client embed a window from another client, so there are two approaches:
/// - floating windows (CLAP_WINDOW_API_WAYLAND): the plugin's window is a toplevel of its own, on
///   the plugin's own connection to the system compositor. It is simple for the host and for the
///   plugin.
/// - a nested compositor (CLAP_WINDOW_API_WAYLAND_SURFACE_EMBEDDING_V1): the host runs a Wayland
///   compositor of its own, through which the plugin's surface is embedded into the host's window.
///   It feels more integrated, but the host has to implement a compositor.
///
/// A plugin may support both, and the host chooses with clap_plugin_gui->is_api_supported().
///
/// In both cases, the host and the plugin don't share any Wayland object or library type, only
/// strings and sockets:
/// - each side can use any Wayland implementation and any toolkit
/// - the plugin's GUI may run in another process
///
/// The extensions and window APIs which carry a Wayland protocol are named after it, including
/// its version. A new version of a protocol would come with a new extension; a plugin may
/// implement both.
///
/// clap_host_gui_xkb_key_forwarding lets the plugin forward to the host the keys which its
/// window received but didn't consume, so that the host's shortcuts keep working, for example
/// Space to start the playback. It works for X11 and Wayland, which both describe the keys with
/// XKB.
///
/// The compositor may offer only some of the protocols, and the host and the plugin may each
/// support only some of the extensions: they are independent.
///
/// A host which is an X11 client, even under XWayland, can't use the Wayland window APIs: it uses
/// CLAP_WINDOW_API_X11 instead. The window API given to clap_plugin_gui->create() and the
/// host's window must belong to the same window system.
///
/// Floating windows
/// ----------------
///
/// The plugin opens its own connection to the system compositor and creates its own
/// xdg_toplevel. A bug on one side can't break the other side's connection.
///
/// - clap_plugin_gui_xdg_foreign_v2 (xdg-foreign-unstable-v2) keeps the plugin's window above the
///   host's window
/// - clap_plugin_gui_xdg_activation_v1 (xdg-activation-v1) lets the plugin's window take the focus
///
/// Showing the GUI works as follows:
///  1. clap_plugin_gui->is_api_supported(plugin, CLAP_WINDOW_API_WAYLAND, true)
///  2. clap_plugin_gui->create(plugin, CLAP_WINDOW_API_WAYLAND, true)
///  3. clap_plugin_gui_xdg_foreign_v2->set_transient(plugin, handle)
///  4. clap_plugin_gui->suggest_title(plugin, title)
///  5. clap_plugin_gui_xdg_activation_v1->set_activation_token(plugin, token)
///  6. clap_plugin_gui->show(plugin)
///
/// clap_plugin_gui->set_transient() isn't defined for CLAP_WINDOW_API_WAYLAND: the plugin
/// returns false, and the host uses clap_plugin_gui_xdg_foreign_v2 instead.
///
/// Nested compositor
/// -----------------
///
/// See CLAP_WINDOW_API_WAYLAND_SURFACE_EMBEDDING_V1, clap_plugin_gui_wayland_nested_compositor
/// and clap_host_gui_wayland_nested_compositor below.

#ifdef __cplusplus
extern "C" {
#endif

/////////////////////////////
// xdg-foreign-unstable-v2 //
/////////////////////////////

static CLAP_CONSTEXPR const char CLAP_EXT_GUI_XDG_FOREIGN_V2[] = "clap.gui-xdg-foreign-v2/1";

/// The host exports its toplevel with zxdg_exporter_v2.export_toplevel(), and passes the handle
/// from the zxdg_exported_v2.handle event to the plugin. The plugin imports it with
/// zxdg_importer_v2.import_toplevel(), then calls zxdg_imported_v2.set_parent_of() with its own
/// toplevel, and the compositor keeps the plugin's window above the host's one.
///
/// The handle is an opaque string, created by the compositor. It is valid as long as the host
/// keeps its zxdg_exported_v2 object and its toplevel alive, and only within the current
/// compositor session: it must not be saved. One handle can be given to several plugins.
///
/// When the host destroys its zxdg_exported_v2 object, the plugin's zxdg_imported_v2 receives
/// the destroyed event and its window has no parent anymore. The host should rather call
/// set_transient() with a new handle, or with NULL, before destroying it.
typedef struct clap_plugin_gui_xdg_foreign_v2 {
   // Keeps the plugin's floating window above the host's toplevel.
   //
   // handle is from zxdg_exported_v2.handle, null-terminated, and only valid during this call.
   // NULL removes the parent.
   //
   // May be called before or after clap_plugin_gui->show(), and again whenever the host's
   // window changes. Before show() lets the compositor place the plugin's window relative to
   // the host's one.
   //
   // Returns false if the plugin can't use the handle, for example if the compositor has no
   // zxdg_importer_v2, or if the GUI isn't a floating Wayland window.
   // [main-thread & floating]
   bool(CLAP_ABI *set_transient)(const clap_plugin_t *plugin, const char *handle);
} clap_plugin_gui_xdg_foreign_v2_t;

///////////////////////
// xdg-activation-v1 //
///////////////////////

static CLAP_CONSTEXPR const char CLAP_EXT_GUI_XDG_ACTIVATION_V1[] = "clap.gui-xdg-activation-v1/1";

/// A Wayland client can't give the focus to its own window, or raise it: the compositor decides,
/// and usually only when the user asked for it. When the user asks the host for the plugin's
/// window, the host creates an activation token from that input event, and passes it to the
/// plugin:
/// - xdg_activation_v1.get_activation_token()
/// - xdg_activation_token_v1.set_serial() with the serial of the input event, and the seat
/// - xdg_activation_token_v1.set_surface() with the surface which received it
/// - xdg_activation_token_v1.commit(), then the token comes with the done event
///
/// The plugin calls xdg_activation_v1.activate() with the token and its own surface, once its
/// window is mapped. A token can only be used once.
///
/// Without a token, the compositor may show the plugin's window without the focus, or only
/// notify the user that it is ready.
///
/// A plugin whose GUI runs in another process can pass the token to it, for example with the
/// XDG_ACTIVATION_TOKEN environment variable, as when launching an application.
typedef struct clap_plugin_gui_xdg_activation_v1 {
   // Gives an activation token for the plugin's floating window.
   //
   // token is from xdg_activation_token_v1.done, null-terminated, and only valid during this
   // call.
   //
   // Before clap_plugin_gui->show(), the plugin activates its window when it is shown. While the
   // window is shown, the plugin activates it right away, which may raise it.
   // [main-thread & floating]
   void(CLAP_ABI *set_activation_token)(const clap_plugin_t *plugin, const char *token);
} clap_plugin_gui_xdg_activation_v1_t;

///////////////////////
// Nested compositor //
///////////////////////

/// The plugin's window is embedded into the host's window, through a Wayland compositor which
/// the host runs. That compositor forwards the requests of the plugin to the system compositor,
/// for example with https://github.com/cclsoftware/wayland-server-delegate, and offers the
/// clap_surface_embedding_v1 protocol to place the plugin's surface into the host's window.
///
/// The protocol is a draft too, in protocols/clap-surface-embedding-v1.xml at the root of the
/// clap-wayland-experiment repository. It is based on the draft which jwolffpsl proposed in
/// https://github.com/free-audio/clap/issues/474#issuecomment-4177525720
///
/// The plugin connects to the host's compositor instead of the system compositor, with a socket
/// which it gets from clap_host_gui_wayland_nested_compositor. Then it uses Wayland as usual:
/// - it creates a wl_surface, and calls clap_surface_embedding_v1.get_embedded_surface() with the
///   embedding token from clap_plugin_gui_wayland_nested_compositor->set_parent()
/// - for its dialogs and popups, it uses the standard protocols on the same connection, with the
///   parents from clap_embedded_surface_v1.get_parent_toplevel() and get_parent_surface()
///
/// The parents are xdg-shell objects, so in C the plugin includes the header which
/// wayland-scanner generates for xdg-shell before the one for clap_surface_embedding_v1. CMake
/// projects can generate the code with clap_wayland_experiment_target_protocols().
///
/// Showing the GUI works as follows:
///  1. clap_plugin_gui->is_api_supported(plugin, api, false)
///  2. clap_plugin_gui->create(plugin, api, false)
///  3. clap_plugin_gui->can_resize(), then get_size() or set_size()
///  4. clap_plugin_gui_wayland_nested_compositor->set_parent(plugin, token)
///  5. clap_plugin_gui->show(plugin)
/// where api is CLAP_WINDOW_API_WAYLAND_SURFACE_EMBEDDING_V1.
///
/// clap_plugin_gui->set_parent() isn't defined for this window API: the plugin returns false,
/// and the host uses clap_plugin_gui_wayland_nested_compositor instead. This keeps the experiment
/// out of clap_plugin_gui's specification.
///
/// The plugin returns true from is_api_supported() only if the host has
/// clap_host_gui_wayland_nested_compositor.

// Embedding through the host's compositor, with clap_surface_embedding_v1.
//
// Uses logical size, don't call clap_plugin_gui->set_scale(): the plugin gets the scale from the
// compositor, like any Wayland client.
static const CLAP_CONSTEXPR char CLAP_WINDOW_API_WAYLAND_SURFACE_EMBEDDING_V1[] =
   "wayland-surface-embedding-v1";

// For both clap_plugin_gui_wayland_nested_compositor and clap_host_gui_wayland_nested_compositor
static CLAP_CONSTEXPR const char CLAP_EXT_GUI_WAYLAND_NESTED_COMPOSITOR[] =
   "clap.gui-wayland-nested-compositor/1";

typedef struct clap_plugin_gui_wayland_nested_compositor {
   // Embeds the plugin's window into the host's window, through the host's compositor.
   //
   // token is the embedding token for the protocol of the window API given to
   // clap_plugin_gui->create(): with CLAP_WINDOW_API_WAYLAND_SURFACE_EMBEDDING_V1, the plugin
   // passes it to clap_surface_embedding_v1.get_embedded_surface(). It is null-terminated, and
   // only valid during this call.
   //
   // The token tells the host's compositor where to embed the surface, so one connection can
   // serve several plugin instances. If the plugin isn't connected to the host's compositor yet,
   // it connects with clap_host_gui_wayland_nested_compositor->open_connection().
   //
   // Returns true on success.
   // [main-thread & !floating]
   bool(CLAP_ABI *set_parent)(const clap_plugin_t *plugin, const char *token);
} clap_plugin_gui_wayland_nested_compositor_t;

typedef struct clap_host_gui_wayland_nested_compositor {
   // Opens a new connection to the host's Wayland compositor.
   //
   // Returns the file descriptor of a connected socket, or -1 on failure.
   //
   // The plugin becomes the owner of the file descriptor, and is responsible for closing it. It
   // may connect with wl_display_connect_to_fd(), then wl_display_disconnect() closes it. It may
   // also pass it to another process, for example in the WAYLAND_SOCKET environment variable,
   // and close its own copy.
   //
   // All the connections lead to the same compositor, so a plugin may use one connection for all
   // its instances in this host.
   // [main-thread]
   int(CLAP_ABI *open_connection)(const clap_host_t *host);
} clap_host_gui_wayland_nested_compositor_t;

////////////////////////
// XKB key forwarding //
////////////////////////

static CLAP_CONSTEXPR const char CLAP_EXT_GUI_XKB_KEY_FORWARDING[] =
   "clap.gui-xkb-key-forwarding/1";

/// When the plugin's window has the keyboard focus, the window system sends the keys to the
/// plugin only: on Wayland a client can't pass an input event to another client's window, and on
/// X11 the plugin usually has its own connection to the server. The host would miss its own
/// shortcuts, for example Space to start the playback.
///
/// So the plugin forwards to the host the keys which it doesn't consume, and only those:
/// - a key which the plugin uses isn't forwarded, for example while one of its text fields is
///   being edited, or when the key is one of the plugin's own shortcuts
/// - a key which the plugin ignores is forwarded, and the host handles it as if it had received
///   it itself
///
/// This is for CLAP_WINDOW_API_X11, CLAP_WINDOW_API_WAYLAND and
/// CLAP_WINDOW_API_WAYLAND_SURFACE_EMBEDDING_V1, floating or embedded. A host with a nested
/// compositor sees the keys before the plugin, but only the plugin knows which ones it consumes.
///
/// The host sees the keys in pairs:
/// - the plugin forwards the repeats and the release of a key only if it forwarded its press
/// - if the plugin loses the keyboard focus while a forwarded key is pressed, it forwards a
///   release for it
///
/// The host must not send the forwarded keys back to the plugin, for example with XEmbed.

// Same values as wl_keyboard.key_state
enum clap_xkb_key_state {
   CLAP_XKB_KEY_RELEASED = 0,
   CLAP_XKB_KEY_PRESSED = 1,

   // The key is still pressed. On Wayland, the clients make the repeats themselves from
   // wl_keyboard.repeat_info, unless they bound wl_keyboard version 10 or later.
   CLAP_XKB_KEY_REPEATED = 2,
};

// The effective modifiers, as xkb_state_mod_name_is_active() with XKB_STATE_MODS_EFFECTIVE.
// The lock modifiers aren't there: the keysym already reflects them.
enum clap_xkb_modifier {
   CLAP_XKB_MOD_SHIFT = 1 << 0, // "Shift"
   CLAP_XKB_MOD_CTRL = 1 << 1,  // "Control"
   CLAP_XKB_MOD_ALT = 1 << 2,   // "Mod1"
   CLAP_XKB_MOD_SUPER = 1 << 3, // "Mod4"
};

typedef struct clap_xkb_key_event {
   // The XKB keycode, as xkbcommon uses it: on Wayland, the key from wl_keyboard.key plus 8; on
   // X11, the keycode as is.
   uint32_t keycode;

   // The key's symbol with the current modifiers and layout, as xkb_state_key_get_one_sym()
   // returns it, for example XKB_KEY_space. XKB_KEY_NoSymbol (0) if there is none.
   uint32_t keysym;

   // Bitmask of clap_xkb_modifier
   uint32_t modifiers;

   // One of clap_xkb_key_state
   uint32_t state;
} clap_xkb_key_event_t;

typedef struct clap_host_gui_xkb_key_forwarding {
   // Forwards a key which the plugin's window received while it had the keyboard focus, and
   // which the plugin didn't consume. The plugin never forwards the keys which it consumes.
   //
   // event is only valid during this call.
   // [main-thread]
   void(CLAP_ABI *forward_key)(const clap_host_t *host, const clap_xkb_key_event_t *event);
} clap_host_gui_xkb_key_forwarding_t;

#ifdef __cplusplus
}
#endif
