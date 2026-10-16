# Forza3Launcher: profiles, settings, disc image import, PLAY. Built next to forza3.exe.
#
# SDL3 (window, input, dialogs, GPU) comes from the ReXGlue SDK install, as a
# static library and headers; Dear ImGui draws through SDL's GPU API. ImGui is
# fetched at the release the SDK uses (the SDK has no SDL backends for it).

include(FetchContent)
FetchContent_Declare(imgui
    URL https://github.com/ocornut/imgui/archive/refs/tags/v1.92.5.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(imgui)

find_library(FORZA3_SDL3_STATIC SDL3-static REQUIRED)
find_path(FORZA3_SDL3_INCLUDE SDL3/SDL.h REQUIRED)

# Version shown in the launcher: the newest CHANGELOG.md entry.
file(STRINGS "${PROJECT_SOURCE_DIR}/CHANGELOG.md" _forza3_changelog REGEX "^## \\[[0-9]+\\.[0-9]+\\.[0-9]+\\]")
list(GET _forza3_changelog 0 _forza3_latest)
string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+" FORZA3_VERSION "${_forza3_latest}")

add_executable(Forza3Launcher WIN32
    src/main.cpp
    src/iso.cpp
    src/profiles.cpp
    src/settings.cpp
    src/verify.cpp
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlgpu3.cpp)

# The SDK include dir also carries imgui.h: keep the fetched copy first.
target_include_directories(Forza3Launcher BEFORE PRIVATE
    ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_include_directories(Forza3Launcher PRIVATE ${FORZA3_SDL3_INCLUDE})
target_compile_definitions(Forza3Launcher PRIVATE
    FORZA3_VERSION="${FORZA3_VERSION}" UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(Forza3Launcher PRIVATE
    ${FORZA3_SDL3_STATIC}
    bcrypt setupapi winmm imm32 version ole32 oleaut32 cfgmgr32 user32 gdi32 shell32
    advapi32 uuid hid)

# Same folder as the game, plus the file list imports are verified against.
set_target_properties(Forza3Launcher PROPERTIES RUNTIME_OUTPUT_DIRECTORY $<TARGET_FILE_DIR:forza3>)
add_custom_command(TARGET Forza3Launcher POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/data/game_files.sha256
        $<TARGET_FILE_DIR:forza3>/game_files.sha256)
