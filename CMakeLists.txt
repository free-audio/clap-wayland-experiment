# Experimental CLAP extensions for Wayland
add_library(clap-wayland-experiment INTERFACE)
target_include_directories(clap-wayland-experiment INTERFACE include)
target_link_libraries(clap-wayland-experiment INTERFACE clap)

# The Wayland protocols drafted along with the extensions
set(CLAP_WAYLAND_EXPERIMENT_PROTOCOLS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/protocols"
    CACHE INTERNAL "Wayland protocols drafted along with the CLAP extensions")

# clap_wayland_experiment_target_protocols(<target> [CLIENT] [SERVER])
#
# Generates the code of the protocols with the system's wayland-scanner, and adds it to the
# target: CLIENT for plugins, SERVER for hosts which run a nested compositor, or both.
#
# The headers are named after the protocols, for example
# clap-surface-embedding-v1-client-protocol.h. The protocols use xdg-shell's interfaces, so the
# target also needs xdg-shell's generated code, and must include xdg-shell's header before these
# ones. The target links libwayland itself.
function(clap_wayland_experiment_target_protocols target)
  cmake_parse_arguments(PARSE_ARGV 1 arg "CLIENT;SERVER" "" "")
  if(NOT arg_CLIENT AND NOT arg_SERVER)
    message(FATAL_ERROR
            "clap_wayland_experiment_target_protocols(${target}): CLIENT or SERVER is missing")
  endif()

  # The generated code is C. The language must be enabled in the caller's directory, the global
  # list of languages also has the ones enabled by subdirectories.
  if(NOT CMAKE_C_COMPILER_LOADED)
    message(FATAL_ERROR "clap_wayland_experiment_target_protocols(${target}): "
                        "enable the C language, for example with project(... C CXX)")
  endif()

  find_program(WAYLAND_SCANNER wayland-scanner REQUIRED)

  # One directory per target, so that targets in the same directory don't share the outputs
  set(output_dir "${CMAKE_CURRENT_BINARY_DIR}/${target}-clap-wayland-experiment")
  file(MAKE_DIRECTORY "${output_dir}")

  foreach(name clap-surface-embedding-v1)
    set(xml "${CLAP_WAYLAND_EXPERIMENT_PROTOCOLS_DIR}/${name}.xml")
    set(outputs "${output_dir}/${name}-protocol.c")
    set(commands COMMAND "${WAYLAND_SCANNER}" --strict private-code "${xml}"
                         "${output_dir}/${name}-protocol.c")

    if(arg_CLIENT)
      list(APPEND outputs "${output_dir}/${name}-client-protocol.h")
      list(APPEND commands COMMAND "${WAYLAND_SCANNER}" --strict client-header "${xml}"
                                   "${output_dir}/${name}-client-protocol.h")
    endif()

    if(arg_SERVER)
      list(APPEND outputs "${output_dir}/${name}-server-protocol.h")
      list(APPEND commands COMMAND "${WAYLAND_SCANNER}" --strict server-header "${xml}"
                                   "${output_dir}/${name}-server-protocol.h")
    endif()

    add_custom_command(OUTPUT ${outputs} ${commands} DEPENDS "${xml}" VERBATIM)
    target_sources(${target} PRIVATE ${outputs})
  endforeach()

  target_include_directories(${target} PRIVATE "${output_dir}")
endfunction()
