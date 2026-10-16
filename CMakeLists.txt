# forza3 - ReXGlue Recompiled Project
#
# SDK boilerplate lives in generated/rexglue.cmake.

cmake_minimum_required(VERSION 3.25)
project(forza3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(generated/rexglue.cmake)

# The build runs `rexglue codegen` on assets/default.xex first. The game is not
# part of the repository, so without it codegen is swapped for a no-op and the
# committed generated/ sources are compiled as they are. Put your default.xex
# into assets/ to regenerate them.
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/default.xex")
    if(WIN32)
        set(_forza3_codegen_stub "${CMAKE_CURRENT_SOURCE_DIR}/cmake/codegen_skipped.cmd")
    else()
        set(_forza3_codegen_stub "${CMAKE_CURRENT_SOURCE_DIR}/cmake/codegen_skipped.sh")
    endif()
    foreach(_config "" _DEBUG _RELEASE _RELWITHDEBINFO)
        set_target_properties(rex::rexglue PROPERTIES IMPORTED_LOCATION${_config} "${_forza3_codegen_stub}")
    endforeach()
    message(STATUS "forza3: no assets/default.xex - building the committed generated/ code")
endif()

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

# Launcher: disc image import, profiles, PLAY (launcher/). Windows only.
if(WIN32)
    add_subdirectory(launcher)
endif()

# Runtime settings (window, keyboard, graphics) read from <exe dir>/forza3.toml,
# plus the SDK's DLLs (staged by rexglue_setup_target only when the exe
# relinks; refreshed here so a rebuilt SDK is always picked up). A separate
# always-run target, so these deploy without a relink.
get_target_property(_forza3_sdk_runtime rex::runtime IMPORTED_LOCATION_RELEASE)
get_filename_component(_forza3_sdk_bin "${_forza3_sdk_runtime}" DIRECTORY)
add_custom_target(forza3_config ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/config/forza3.toml
        $<TARGET_FILE_DIR:forza3>/forza3.toml
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${_forza3_sdk_bin}/rexruntime.dll ${_forza3_sdk_bin}/rexgpu-xenos.dll
        $<TARGET_FILE_DIR:forza3>
    VERBATIM)
add_dependencies(forza3_config forza3)
