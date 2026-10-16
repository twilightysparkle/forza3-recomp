// Forza3Launcher - the player's settings, shared with the game.

#include "settings.h"

#include <cstdlib>
#include <fstream>

namespace launcher {

namespace {

// SDK defaults of the keys the launcher manages and forza3.toml may leave out.
const std::map<std::string, std::string>& SdkDefaults() {
  static const std::map<std::string, std::string> defaults = {
      {"fullscreen", "true"},          {"window_width", "0"},
      {"window_height", "0"},          {"vsync", "true"},
      {"present_letterbox", "true"},   {"present_effect", "bilinear"},
      {"draw_resolution_scale_x", "1"}, {"draw_resolution_scale_y", "1"},
      {"anisotropic_override", "3"},   {"swap_post_effect", "none"},
      {"readback_resolve", "none"},    {"mnk_mode", "false"},
      {"mnk_mouse", "false"},          {"mnk_sensitivity", "1.0"},
      {"user_language", "1"},          {"audio_mute", "false"},
  };
  return defaults;
}

// Keys written as TOML strings (the rest are booleans and numbers).
bool IsStringKey(const std::string& key) {
  return key.rfind("keybind_", 0) == 0 || key == "present_effect" ||
         key == "swap_post_effect" || key == "readback_resolve";
}

std::string Trim(std::string s) {
  size_t a = s.find_first_not_of(" \t\r");
  size_t b = s.find_last_not_of(" \t\r");
  return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}

// `key = value` lines; strips comments outside quotes and the quotes.
void ReadToml(const std::filesystem::path& file, std::map<std::string, std::string>& out) {
  std::ifstream in(file);
  std::string line;
  while (std::getline(in, line)) {
    size_t eq = line.find('=');
    if (eq == std::string::npos || Trim(line).rfind('#', 0) == 0) continue;
    std::string key = Trim(line.substr(0, eq));
    std::string rest = Trim(line.substr(eq + 1));
    std::string value;
    if (!rest.empty() && rest[0] == '"') {
      for (size_t i = 1; i < rest.size() && rest[i] != '"'; ++i) {
        if (rest[i] == '\\' && i + 1 < rest.size()) ++i;
        value += rest[i];
      }
    } else {
      size_t hash = rest.find('#');
      value = Trim(hash == std::string::npos ? rest : rest.substr(0, hash));
    }
    if (!key.empty() && key.find(' ') == std::string::npos) out[key] = value;
  }
}

}  // namespace

const std::vector<BindDef>& BindDefs() {
  static const std::vector<BindDef> defs = {
      {"keybind_right_trigger", "Throttle", "Driving"},
      {"keybind_left_trigger", "Brake / reverse", "Driving"},
      {"keybind_lstick_left", "Steer left", "Driving"},
      {"keybind_lstick_right", "Steer right", "Driving"},
      {"keybind_lstick_up", "Left stick up", "Driving"},
      {"keybind_lstick_down", "Left stick down", "Driving"},
      {"keybind_lstick_press", "Left stick click", "Driving"},
      {"keybind_a", "A (accept)", "Buttons"},
      {"keybind_b", "B (back)", "Buttons"},
      {"keybind_x", "X", "Buttons"},
      {"keybind_y", "Y", "Buttons"},
      {"keybind_left_shoulder", "Left bumper", "Buttons"},
      {"keybind_right_shoulder", "Right bumper", "Buttons"},
      {"keybind_back", "Back", "Buttons"},
      {"keybind_start", "Start (pause)", "Buttons"},
      {"keybind_rstick_up", "Look up", "Camera"},
      {"keybind_rstick_down", "Look down", "Camera"},
      {"keybind_rstick_left", "Look left", "Camera"},
      {"keybind_rstick_right", "Look right", "Camera"},
      {"keybind_rstick_press", "Right stick click", "Camera"},
      {"keybind_dpad_up", "D-pad up", "Menus"},
      {"keybind_dpad_down", "D-pad down", "Menus"},
      {"keybind_dpad_left", "D-pad left", "Menus"},
      {"keybind_dpad_right", "D-pad right", "Menus"},
  };
  return defs;
}

void Settings::Load(const std::filesystem::path& exe_dir) {
  user_file_ = exe_dir / "forza3.user.toml";
  shipped_ = SdkDefaults();
  ReadToml(exe_dir / "forza3.toml", shipped_);
  values_ = shipped_;
  ReadToml(user_file_, values_);
  dirty_ = false;
}

bool Settings::Save() const {
  std::string out =
      "# Saved by the forza3 launcher and settings menu (F2). Loaded on top of\n"
      "# forza3.toml; delete this file to go back to the shipped settings.\n";
  for (const auto& [key, value] : values_) {
    auto shipped = shipped_.find(key);
    if (shipped != shipped_.end() && shipped->second == value) continue;
    if (IsStringKey(key)) {
      std::string quoted;
      for (char c : value) {
        if (c == '"' || c == '\\') quoted += '\\';
        quoted += c;
      }
      out += key + " = \"" + quoted + "\"\n";
    } else {
      out += key + " = " + value + "\n";
    }
  }
  std::ofstream file(user_file_, std::ios::binary | std::ios::trunc);
  return file && (file << out);
}

std::string Settings::Get(const std::string& key) const {
  auto it = values_.find(key);
  return it == values_.end() ? std::string() : it->second;
}

void Settings::Set(const std::string& key, const std::string& value) {
  if (Get(key) == value) return;
  values_[key] = value;
  dirty_ = true;
}

int Settings::GetInt(const std::string& key) const { return std::atoi(Get(key).c_str()); }

double Settings::GetDouble(const std::string& key) const { return std::atof(Get(key).c_str()); }

void Settings::Reset(const std::string& key) { Set(key, Shipped(key)); }

const std::string& Settings::Shipped(const std::string& key) const {
  static const std::string empty;
  auto it = shipped_.find(key);
  return it == shipped_.end() ? empty : it->second;
}

}  // namespace launcher
