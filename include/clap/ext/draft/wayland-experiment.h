#pragma once

#include <clap/plugin.h>

/// @page Wayland Experiment
///
/// Experimental extensions for floating plugin windows on Wayland, and for the keyboard on X11
/// and Wayland.
///
/// Wayland can't embed a window from another connection, so with CLAP_WINDOW_API_WAYLAND the
/// plugin's window is always floating. The plugin opens its own connection to the compositor and
/// creates its own xdg_toplevel. The host and the plugin don't share any Wayland object, only
/// strings which the compositor creates:
/// - a bug on one side can't break the other side's connection
/// - each side can use any Wayland library and any toolkit
/// - the plugin's GUI may run in another process
///
/// The extensions which carry a Wayland protocol are named after it, including its version:
/// - clap_plugin_gui_xdg_foreign_v2 (xdg-foreign-unstable-v2) keeps the plugin's window above the
///   host's window
/// - clap_plugin_gui_xdg_activation_v1 (xdg-activation-v1) lets the plugin's window take the focus
///
/// A new version of a protocol would come with a new extension; a plugin may implement both.
///
/// clap_host_gui_xkb_key_forwarding lets the plugin forward to the host the keys which its
/// window received but didn't consume, so that the host's shortcuts keep working, for example
/// Space to start the playback. It works for X11 and Wayland, which both describe the keys with
/// XKB.
///
/// The compositor may offer only some of the protocols, and the host and the plugin may each
/// support only some of the extensions: they are independent.
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
/// A host which is an X11 client, even under XWayland, can't use the xdg extensions: it uses
/// CLAP_WINDOW_API_X11 instead. The window API given to clap_plugin_gui->create() and the
/// host's window must belong to the same window system.

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

////////////////////////
// XKB key forwarding //
////////////////////////

static CLAP_CONSTEXPR const char CLAP_EXT_GUI_XKB_KEY_FORWARDING[] = "clap.gui-xkb-key-forwarding/1";

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
/// This is for CLAP_WINDOW_API_X11 and CLAP_WINDOW_API_WAYLAND, floating or embedded.
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
