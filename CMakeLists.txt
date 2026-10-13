# Experimental extensions for floating windows on Wayland
add_library(clap-wayland-experiment INTERFACE)
target_include_directories(clap-wayland-experiment INTERFACE include)
target_link_libraries(clap-wayland-experiment INTERFACE clap)
