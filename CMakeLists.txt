# forza3 - ReXGlue Recompiled Project
#
# SDK boilerplate lives in generated/rexglue.cmake.

cmake_minimum_required(VERSION 3.25)
project(forza3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(generated/rexglue.cmake)

# Sources
set(FORZA3_SOURCES
    src/main.cpp
    src/disc2_install.cpp
    src/crash_dump.cpp
    src/default_paths.cpp
    src/utility_drive.cpp
    src/pix_capture.cpp
    src/settings_menu.cpp
)

if(WIN32)
    add_executable(forza3 WIN32 ${FORZA3_SOURCES})
else()
    add_executable(forza3 ${FORZA3_SOURCES})
endif()

rexglue_setup_target(forza3 GPU_PLUGINS xenos)

# Runtime settings (window, keyboard, graphics) read from <exe dir>/forza3.toml.
# A separate always-run target, so a config-only edit is deployed without a relink.
add_custom_target(forza3_config ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/config/forza3.toml
        $<TARGET_FILE_DIR:forza3>/forza3.toml
    VERBATIM)
add_dependencies(forza3_config forza3)
