# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.12.4)

project(gui_guider)
add_compile_options(-O2)

option(CONFIG_GG_USE_G2D "Enable G2D" OFF)

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(PKG_WAYLAND REQUIRED wayland-client wayland-cursor xkbcommon)
pkg_check_modules(PKG_LIBDRM REQUIRED libdrm)
pkg_check_modules(WAYLAND_PROTOCOLS REQUIRED wayland-protocols>=1.25)

# Wayland scanner setup
find_program(WAYLAND_SCANNER_EXECUTABLE NAMES wayland-scanner REQUIRED)
if(NOT DEFINED WAYLAND_PROTOCOLS_BASE)
    pkg_get_variable(WAYLAND_PROTOCOLS_BASE wayland-protocols pkgdatadir)
endif()

# Wayland protocol generation macro
macro(wayland_generate protocol_xml_file output_dir output_name target)
    if(NOT EXISTS ${protocol_xml_file})
        message(FATAL_ERROR "Protocol XML file not found: ${protocol_xml_file}")
    endif()
    
    set(output_file_noext "${output_dir}/${output_name}")
    add_custom_command(OUTPUT "${output_file_noext}.h"
        COMMAND "${WAYLAND_SCANNER_EXECUTABLE}" client-header "${protocol_xml_file}" "${output_file_noext}.h"
        DEPENDS "${protocol_xml_file}"
        VERBATIM)

    add_custom_command(OUTPUT "${output_file_noext}.c"
        COMMAND "${WAYLAND_SCANNER_EXECUTABLE}" private-code "${protocol_xml_file}" "${output_file_noext}.c"
        DEPENDS "${protocol_xml_file}"
        VERBATIM)

    set_property(TARGET ${target} APPEND PROPERTY SOURCES "${output_file_noext}.h" "${output_file_noext}.c")
endmacro()

# Setup Wayland protocols directory
set(WAYLAND_PROTOCOLS_DIR "${CMAKE_CURRENT_BINARY_DIR}/wl_protocols")
file(MAKE_DIRECTORY ${WAYLAND_PROTOCOLS_DIR})

# Generate Wayland protocols
add_custom_target(generate_xdg_shell_protocol ALL)
wayland_generate("${WAYLAND_PROTOCOLS_BASE}/stable/xdg-shell/xdg-shell.xml" 
                 "${WAYLAND_PROTOCOLS_DIR}" "wayland_xdg_shell" generate_xdg_shell_protocol)

set(LINUX_DMABUF_PROTOCOL_XML "${WAYLAND_PROTOCOLS_BASE}/stable/linux-dmabuf/linux-dmabuf-v1.xml")
if(NOT EXISTS "${LINUX_DMABUF_PROTOCOL_XML}")
    set(LINUX_DMABUF_PROTOCOL_XML "${WAYLAND_PROTOCOLS_BASE}/unstable/linux-dmabuf/linux-dmabuf-unstable-v1.xml")
endif()

add_custom_target(generate_linux_dmabuf_protocol ALL)
wayland_generate("${LINUX_DMABUF_PROTOCOL_XML}" 
                 "${WAYLAND_PROTOCOLS_DIR}" "wayland_linux_dmabuf" generate_linux_dmabuf_protocol)

# Collect source files
set(COMMON_SOURCES mouse_cursor_icon.c)
file(GLOB_RECURSE CUSTOM_SOURCES ${CMAKE_SOURCE_DIR}/../../custom/*.c ${CMAKE_SOURCE_DIR}/../../custom/*.cpp)
file(GLOB_RECURSE GENERATED_SOURCES ${CMAKE_SOURCE_DIR}/../../generated/*.c)
set(SOURCES ${COMMON_SOURCES} ${CUSTOM_SOURCES} ${GENERATED_SOURCES})

# Add conditional sources
if(NOT EXISTS ${CMAKE_SOURCE_DIR}/../../custom/real_time_edge)
    list(APPEND SOURCES main.c)
endif()

# Create executable
add_executable(gui_guider 
    ${SOURCES} 
    ${WAYLAND_PROTOCOLS_DIR}/wayland_xdg_shell.c 
    ${WAYLAND_PROTOCOLS_DIR}/wayland_linux_dmabuf.c
)

# Add LVGL subdirectory
if(EXISTS ${CMAKE_SOURCE_DIR}/../lvgl)
    add_subdirectory(${CMAKE_SOURCE_DIR}/../lvgl lvgl)
    target_include_directories(lvgl PUBLIC
	    ${WAYLAND_PROTOCOLS_DIR}
	    ${PKG_LIBDRM_INCLUDE_DIRS}
            ${CMAKE_SOURCE_DIR}/../../custom
    )
    add_dependencies(lvgl generate_xdg_shell_protocol generate_linux_dmabuf_protocol)
endif()

# Set include directories
target_include_directories(gui_guider PRIVATE
    ${CMAKE_SOURCE_DIR}/../../generated
    ${CMAKE_SOURCE_DIR}/../../generated/events
    ${CMAKE_SOURCE_DIR}/../../custom
)

# Link libraries
target_link_libraries(gui_guider PUBLIC 
    lvgl 
    ${PKG_WAYLAND_LIBRARIES} 
    ${PKG_LIBDRM_LIBRARIES} 
    m
)

# Optional video support
if(EXISTS ${CMAKE_SOURCE_DIR}/../../generated/gg_video.c)
    target_link_libraries(gui_guider PUBLIC openh264)
endif()

# Optional camera support
if(EXISTS ${CMAKE_SOURCE_DIR}/../../custom/camera_support.cpp)
    pkg_check_modules(PKG_OPENCV REQUIRED opencv4)
    target_link_libraries(gui_guider PUBLIC ${PKG_OPENCV_LIBRARIES})
    target_include_directories(gui_guider PRIVATE ${PKG_OPENCV_INCLUDE_DIRS})
endif()

# Optional G2D support
if(CONFIG_GG_USE_G2D)
    target_link_libraries(gui_guider PUBLIC g2d)
endif()

# Install target
install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/gui_guider DESTINATION bin)
