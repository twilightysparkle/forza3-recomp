// forza3 - in-game settings menu (F2)

#pragma once

#include <filesystem>
#include <functional>

struct ImFontAtlas;

namespace rex::ui {
class ImGuiDrawer;
}

namespace forza3 {

// <exe dir>\forza3.user.toml: the menu's saved settings. Kept apart from the
// shipped forza3.toml, which the build overwrites.
std::filesystem::path UserSettingsPath();

// Before the window and GPU exist (OnPreSetup): remembers the shipped values
// of the settings the menu manages, then applies forza3.user.toml on top.
void LoadUserSettings();

// OnConfigureFonts: adds the menu's (larger, TTF) font to the atlas.
void AddSettingsMenuFont(ImFontAtlas* atlas);

// OnCreateDialogs: creates the menu and binds F2 to toggle it. `quit` closes
// the game for "Save and restart".
void CreateSettingsMenu(rex::ui::ImGuiDrawer* drawer, std::function<void()> quit);

// True while the menu is open; the game gets no controller input then.
bool IsSettingsMenuOpen();

// A copy launched by "Save and restart" waits here (at startup) for the old
// process to exit, so the two never share files.
void WaitForRestartedInstance();

}  // namespace forza3
