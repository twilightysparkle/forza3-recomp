// forza3 - in-game settings menu (F2)
//
// A curated front end over the SDK cvars that matter for playing FM3: display,
// graphics, keyboard controls, language and sound, with plain labels and a note
// on each setting that only applies after a restart. The SDK's F4 overlay
// still lists every cvar for anything not covered here.
//
// Changes apply immediately where the SDK allows it. "Save" writes the managed
// settings that differ from the shipped forza3.toml to forza3.user.toml, which
// is loaded on top of it at startup, so rebuilding never loses them.

#include "settings_menu.h"

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/ui/imgui_dialog.h>
#include <rex/ui/imgui_drawer.h>
#include <rex/ui/keybinds.h>
#include <rex/ui/virtual_key.h>

#include <imgui.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include <algorithm>
#include <atomic>
#include <cfloat>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace forza3 {

namespace {

using rex::ui::VirtualKey;

constexpr const char* kToggleKey = "F2";
constexpr double kHintSeconds = 15.0;

// The SDK's overlays use a 10 px bitmap font, too small for a game menu; the
// renderer bakes fonts at their load size, so the menu loads its own.
constexpr float kFontSize = 17.0f;
ImFont* g_font = nullptr;

// Pushes the menu font for the current scope (if it loaded).
struct MenuFont {
  MenuFont() {
    if (g_font) ImGui::PushFont(g_font, g_font->LegacySize);
  }
  ~MenuFont() {
    if (g_font) ImGui::PopFont();
  }
};

// --- Managed settings ---------------------------------------------------------

struct Choice {
  const char* value;
  const char* label;
};

struct Binding {
  const char* cvar;
  const char* label;
};

struct BindingGroup {
  const char* title;
  std::vector<Binding> bindings;
};

const std::vector<BindingGroup>& BindingGroups() {
  static const std::vector<BindingGroup> groups = {
      {"Driving",
       {{"keybind_right_trigger", "Throttle (right trigger)"},
        {"keybind_left_trigger", "Brake / reverse (left trigger)"},
        {"keybind_lstick_left", "Steer left"},
        {"keybind_lstick_right", "Steer right"},
        {"keybind_lstick_up", "Left stick up"},
        {"keybind_lstick_down", "Left stick down"},
        {"keybind_lstick_press", "Left stick click"}}},
      {"Buttons",
       {{"keybind_a", "A (confirm)"},
        {"keybind_b", "B (back)"},
        {"keybind_x", "X"},
        {"keybind_y", "Y"},
        {"keybind_left_shoulder", "Left bumper"},
        {"keybind_right_shoulder", "Right bumper"},
        {"keybind_back", "Back"},
        {"keybind_start", "Start (pause)"},
        {"keybind_guide", "Guide"}}},
      {"Camera (right stick)",
       {{"keybind_rstick_up", "Look up"},
        {"keybind_rstick_down", "Look down"},
        {"keybind_rstick_left", "Look left"},
        {"keybind_rstick_right", "Look right"},
        {"keybind_rstick_press", "Right stick click"}}},
      {"D-pad (menus)",
       {{"keybind_dpad_up", "Up"},
        {"keybind_dpad_down", "Down"},
        {"keybind_dpad_left", "Left"},
        {"keybind_dpad_right", "Right"}}},
  };
  return groups;
}

// Everything Save may write. Keybinds are appended from BindingGroups().
constexpr const char* kSettings[] = {
    "fullscreen",       "window_width",          "window_height",
    "vsync",            "present_letterbox",     "present_effect",
    "draw_resolution_scale_x", "draw_resolution_scale_y", "anisotropic_override",
    "swap_post_effect", "readback_resolve",      "mnk_mode",
    "mnk_mouse",        "mnk_sensitivity",       "user_language",
    "audio_mute",
};

// Names shown in the "restart to apply" note.
const std::map<std::string, const char*, std::less<>> kRestartNames = {
    {"window_width", "window size"},
    {"window_height", "window size"},
    {"present_effect", "upscaling filter"},
    {"draw_resolution_scale_x", "internal resolution"},
    {"draw_resolution_scale_y", "internal resolution"},
    {"swap_post_effect", "anti-aliasing"},
    {"user_language", "language"},
};

const std::vector<std::string>& ManagedNames() {
  static const std::vector<std::string> names = [] {
    std::vector<std::string> v(std::begin(kSettings), std::end(kSettings));
    for (const auto& group : BindingGroups()) {
      for (const auto& binding : group.bindings) {
        v.emplace_back(binding.cvar);
      }
    }
    return v;
  }();
  return names;
}

using Snapshot = std::map<std::string, std::string, std::less<>>;

Snapshot g_shipped;  // forza3.toml (and command line) values.
Snapshot g_saved;    // Values as of the last load or save.
Snapshot g_boot;     // Values this process started with.
std::atomic<bool> g_open{false};

std::string Get(std::string_view name) { return rex::cvar::GetFlagByName(name); }

void Set(std::string_view name, std::string_view value) {
  if (Get(name) != value) {
    rex::cvar::SetFlagByName(name, value);
  }
}

// Only registered cvars: a plugin's cvars don't exist until it is loaded.
Snapshot TakeSnapshot() {
  Snapshot s;
  for (const auto& name : ManagedNames()) {
    if (rex::cvar::GetFlagInfo(name)) {
      s[name] = Get(name);
    }
  }
  return s;
}

void Restore(const Snapshot& s) {
  for (const auto& [name, value] : s) {
    Set(name, value);
  }
}

enum class Apply { kLive, kRestart };

Apply ApplyOf(std::string_view name) {
  // The language is read when the game boots, despite being a live cvar.
  if (name == "user_language") {
    return Apply::kRestart;
  }
  const auto* info = rex::cvar::GetFlagInfo(name);
  return info && info->lifecycle == rex::cvar::Lifecycle::kRequiresRestart ? Apply::kRestart
                                                                           : Apply::kLive;
}

std::string TomlValue(std::string_view name, const std::string& value) {
  const auto* info = rex::cvar::GetFlagInfo(name);
  if (!info || info->type != rex::cvar::FlagType::String) {
    return value;
  }
  std::string quoted = "\"";
  for (char c : value) {
    if (c == '"' || c == '\\') {
      quoted += '\\';
    }
    quoted += c;
  }
  return quoted + "\"";
}

bool SaveUserSettings(std::string& status) {
  std::string out =
      "# Saved by the forza3 settings menu (F2). Loaded on top of forza3.toml;\n"
      "# delete this file to go back to the shipped settings.\n";
  for (const auto& name : ManagedNames()) {
    auto shipped = g_shipped.find(name);
    if (shipped == g_shipped.end()) {
      continue;  // Not registered.
    }
    std::string value = Get(name);
    if (value != shipped->second) {
      out += name + " = " + TomlValue(name, value) + "\n";
    }
  }
  std::ofstream file(UserSettingsPath(), std::ios::binary | std::ios::trunc);
  if (!file || !(file << out)) {
    status = "Could not write " + UserSettingsPath().string();
    REXLOG_ERROR("Settings: {}", status);
    return false;
  }
  g_saved = TakeSnapshot();
  status = "Saved";
  REXLOG_INFO("Settings: saved {}", UserSettingsPath().string());
  return true;
}

void Relaunch() {
#ifdef _WIN32
  wchar_t exe[MAX_PATH];
  if (!GetModuleFileNameW(nullptr, exe, MAX_PATH)) {
    return;
  }
  std::wstring command_line = GetCommandLineW();
  SetEnvironmentVariableW(L"FORZA3_RESTART_WAIT_PID",
                          std::to_wstring(GetCurrentProcessId()).c_str());
  STARTUPINFOW startup_info = {sizeof(startup_info)};
  PROCESS_INFORMATION process_info = {};
  if (CreateProcessW(exe, command_line.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr,
                     &startup_info, &process_info)) {
    CloseHandle(process_info.hThread);
    CloseHandle(process_info.hProcess);
  } else {
    REXLOG_ERROR("Settings: relaunch failed ({})", GetLastError());
  }
  SetEnvironmentVariableW(L"FORZA3_RESTART_WAIT_PID", nullptr);
#endif
}

// --- Key capture ------------------------------------------------------------------

VirtualKey ToVirtualKey(ImGuiKey key) {
  if (key >= ImGuiKey_A && key <= ImGuiKey_Z) {
    return VirtualKey(uint16_t(VirtualKey::kA) + (key - ImGuiKey_A));
  }
  if (key >= ImGuiKey_0 && key <= ImGuiKey_9) {
    return VirtualKey(uint16_t(VirtualKey::k0) + (key - ImGuiKey_0));
  }
  if (key >= ImGuiKey_F1 && key <= ImGuiKey_F24) {
    return VirtualKey(uint16_t(VirtualKey::kF1) + (key - ImGuiKey_F1));
  }
  if (key >= ImGuiKey_Keypad0 && key <= ImGuiKey_Keypad9) {
    return VirtualKey(uint16_t(VirtualKey::kNumpad0) + (key - ImGuiKey_Keypad0));
  }
  switch (key) {
    case ImGuiKey_Space: return VirtualKey::kSpace;
    case ImGuiKey_Enter: return VirtualKey::kReturn;
    case ImGuiKey_KeypadEnter: return VirtualKey::kReturn;
    case ImGuiKey_Tab: return VirtualKey::kTab;
    case ImGuiKey_Backspace: return VirtualKey::kBack;
    case ImGuiKey_Delete: return VirtualKey::kDelete;
    case ImGuiKey_Insert: return VirtualKey::kInsert;
    case ImGuiKey_Home: return VirtualKey::kHome;
    case ImGuiKey_End: return VirtualKey::kEnd;
    case ImGuiKey_PageUp: return VirtualKey::kPrior;
    case ImGuiKey_PageDown: return VirtualKey::kNext;
    case ImGuiKey_LeftArrow: return VirtualKey::kLeft;
    case ImGuiKey_RightArrow: return VirtualKey::kRight;
    case ImGuiKey_UpArrow: return VirtualKey::kUp;
    case ImGuiKey_DownArrow: return VirtualKey::kDown;
    case ImGuiKey_CapsLock: return VirtualKey::kCapital;
    case ImGuiKey_GraveAccent: return VirtualKey::kOem3;
    case ImGuiKey_Minus: return VirtualKey::kOemMinus;
    case ImGuiKey_Equal: return VirtualKey::kOemPlus;
    case ImGuiKey_LeftBracket: return VirtualKey::kOem4;
    case ImGuiKey_RightBracket: return VirtualKey::kOem6;
    case ImGuiKey_Backslash: return VirtualKey::kOem5;
    case ImGuiKey_Semicolon: return VirtualKey::kOem1;
    case ImGuiKey_Apostrophe: return VirtualKey::kOem7;
    case ImGuiKey_Comma: return VirtualKey::kOemComma;
    case ImGuiKey_Period: return VirtualKey::kOemPeriod;
    case ImGuiKey_Slash: return VirtualKey::kOem2;
    case ImGuiKey_KeypadDecimal: return VirtualKey::kDecimal;
    case ImGuiKey_KeypadDivide: return VirtualKey::kDivide;
    case ImGuiKey_KeypadMultiply: return VirtualKey::kMultiply;
    case ImGuiKey_KeypadSubtract: return VirtualKey::kSubtract;
    case ImGuiKey_KeypadAdd: return VirtualKey::kAdd;
    default: return VirtualKey::kNone;
  }
}

std::vector<std::string> SplitKeys(std::string_view value) {
  std::vector<std::string> keys;
  while (!value.empty()) {
    size_t comma = value.find(',');
    std::string_view token = value.substr(0, comma);
    while (!token.empty() && token.front() == ' ') token.remove_prefix(1);
    while (!token.empty() && token.back() == ' ') token.remove_suffix(1);
    if (!token.empty()) {
      keys.emplace_back(token);
    }
    value = comma == std::string_view::npos ? std::string_view() : value.substr(comma + 1);
  }
  return keys;
}

std::string JoinKeys(const std::vector<std::string>& keys) {
  std::string out;
  for (const auto& key : keys) {
    out += (out.empty() ? "" : ",") + key;
  }
  return out;
}

// What else uses a key: other bindings, or the overlay hotkeys.
std::string KeyConflicts(const char* cvar) {
  static const std::pair<const char*, const char*> kHotkeys[] = {
      {"F2", "this menu"},          {"F3", "performance overlay"},
      {"F4", "advanced settings"},  {"F7", "achievements"},
      {"Backtick", "console"},
  };
  std::string conflicts;
  for (const auto& key : SplitKeys(Get(cvar))) {
    for (const auto& group : BindingGroups()) {
      for (const auto& other : group.bindings) {
        if (std::string_view(other.cvar) == cvar) continue;
        for (const auto& other_key : SplitKeys(Get(other.cvar))) {
          if (other_key == key) {
            conflicts += key + " is also " + other.label + "\n";
          }
        }
      }
    }
    for (const auto& [hotkey, what] : kHotkeys) {
      if (key == hotkey) {
        conflicts += key + " also opens the " + what + "\n";
      }
    }
  }
  return conflicts;
}

// --- Dialog --------------------------------------------------------------------------

const ImVec4 kRestartColor(1.0f, 0.68f, 0.28f, 1.0f);
const ImVec4 kWarningColor(1.0f, 0.45f, 0.35f, 1.0f);

// "Press F2 for settings" in the corner for the first seconds after boot.
class HintDialog final : public rex::ui::ImGuiDialog {
 public:
  explicit HintDialog(rex::ui::ImGuiDrawer* drawer) : ImGuiDialog(drawer) {}

 protected:
  void OnDraw(ImGuiIO& io) override;

 private:
  double first_draw_time_ = -1.0;
};

// Exists only while open, like the SDK overlays: the ImGui drawer listens to
// window input only while it has dialogs.
class SettingsMenuDialog final : public rex::ui::ImGuiDialog {
 public:
  SettingsMenuDialog(rex::ui::ImGuiDrawer* drawer, std::function<void()> quit);
  ~SettingsMenuDialog() override;

 protected:
  void OnDraw(ImGuiIO& io) override;

 private:
  void DrawMenu();
  void DrawDisplay();
  void DrawGraphics();
  void DrawControls();
  void DrawGame();
  void DrawFooter();
  void CaptureKey();

  // Two-column rows: label (with help and restart note) | control.
  bool BeginRows(const char* id);
  void Label(const char* label, const char* help, std::initializer_list<const char*> cvars);
  void CheckboxRow(const char* label, const char* help, const char* cvar);
  void ComboRow(const char* label, const char* help, const char* cvar,
                std::span<const Choice> choices);

  std::function<void()> quit_;
  std::string capturing_;  // Keybind cvar waiting for a key.
  bool capture_adds_ = false;
  // After a capture, ignore binding buttons briefly, so Enter or Space (also
  // the keyboard-navigation activate keys) can't restart the capture.
  double capture_cooldown_until_ = 0.0;
  std::string status_;
  int tab_ = 0;
  int select_tab_ = -1;  // PgUp / PgDn request.
  bool appearing_ = true;
  bool focus_first_ = true;  // Keyboard focus to the page's first setting.
  bool nav_was_enabled_ = false;
};

SettingsMenuDialog* g_menu = nullptr;

SettingsMenuDialog::SettingsMenuDialog(rex::ui::ImGuiDrawer* drawer, std::function<void()> quit)
    : ImGuiDialog(drawer), quit_(std::move(quit)) {
  g_menu = this;
  g_open.store(true, std::memory_order_relaxed);
  // Arrows / Enter / Space work in the menu (game input is off meanwhile).
  ImGuiIO& io = GetIO();
  nav_was_enabled_ = (io.ConfigFlags & ImGuiConfigFlags_NavEnableKeyboard) != 0;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  REXLOG_INFO("Settings: menu opened");
}

SettingsMenuDialog::~SettingsMenuDialog() {
  if (g_menu == this) {
    g_menu = nullptr;
    g_open.store(false, std::memory_order_relaxed);
  }
  if (!nav_was_enabled_) {
    GetIO().ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
  }
  REXLOG_INFO("Settings: menu closed");
}

void SettingsMenuDialog::OnDraw(ImGuiIO& /*io*/) {
  {
    MenuFont font;
    DrawMenu();
  }
  if (!g_open.load(std::memory_order_relaxed)) {
    Close();  // Deletes this after drawing.
  }
}

void HintDialog::OnDraw(ImGuiIO& /*io*/) {
  double now = ImGui::GetTime();
  if (first_draw_time_ < 0.0) {
    first_draw_time_ = now;
  }
  double shown = now - first_draw_time_;
  if (shown >= kHintSeconds || g_menu) {
    Close();
    return;
  }
  float alpha = float(std::min(1.0, (kHintSeconds - shown) / 2.0));
  MenuFont font;
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(
      ImVec2(viewport->WorkPos.x + 16.0f, viewport->WorkPos.y + viewport->WorkSize.y - 16.0f),
      ImGuiCond_Always, ImVec2(0.0f, 1.0f));
  ImGui::SetNextWindowBgAlpha(0.6f * alpha);
  ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
  if (ImGui::Begin("##forza3_settings_hint", nullptr,
                   ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                       ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoFocusOnAppearing |
                       ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoSavedSettings)) {
    ImGui::Text("Press %s for settings", kToggleKey);
  }
  ImGui::End();
  ImGui::PopStyleVar();
}

void SettingsMenuDialog::DrawMenu() {
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  ImVec2 size = g_font ? ImVec2(900.0f, 620.0f) : ImVec2(660.0f, 540.0f);
  size.x = std::min(size.x, viewport->WorkSize.x * 0.95f);
  size.y = std::min(size.y, viewport->WorkSize.y * 0.92f);
  ImGui::SetNextWindowSize(size, ImGuiCond_Appearing);
  ImGui::SetNextWindowBgAlpha(0.94f);
  if (appearing_) {
    ImGui::SetNextWindowFocus();
    appearing_ = false;
  }
  bool open = true;
  if (ImGui::Begin("Forza Motorsport 3 Settings###forza3_settings", &open,
                   ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings)) {
    float footer = ImGui::GetFrameHeightWithSpacing() * 2.0f + ImGui::GetStyle().ItemSpacing.y;
    if (ImGui::BeginTabBar("##tabs")) {
      struct Tab {
        const char* name;
        void (SettingsMenuDialog::*draw)();
      };
      static const Tab kTabs[] = {
          {"Display", &SettingsMenuDialog::DrawDisplay},
          {"Graphics", &SettingsMenuDialog::DrawGraphics},
          {"Controls", &SettingsMenuDialog::DrawControls},
          {"Game & sound", &SettingsMenuDialog::DrawGame},
      };
      constexpr int kTabCount = int(std::size(kTabs));
      if (capturing_.empty() && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
        if (ImGui::IsKeyPressed(ImGuiKey_PageDown, false)) {
          select_tab_ = (tab_ + 1) % kTabCount;
        } else if (ImGui::IsKeyPressed(ImGuiKey_PageUp, false)) {
          select_tab_ = (tab_ + kTabCount - 1) % kTabCount;
        }
      }
      for (int i = 0; i < kTabCount; ++i) {
        const auto& tab = kTabs[i];
        if (ImGui::BeginTabItem(tab.name, nullptr,
                                i == select_tab_ ? ImGuiTabItemFlags_SetSelected : 0)) {
          if (tab_ != i) {
            focus_first_ = true;
          }
          tab_ = i;
          ImGui::BeginChild("##page", ImVec2(0.0f, -footer), ImGuiChildFlags_NavFlattened);
          (this->*tab.draw)();
          ImGui::EndChild();
          ImGui::EndTabItem();
        }
      }
      select_tab_ = -1;
      ImGui::EndTabBar();
    }
    DrawFooter();
  }
  ImGui::End();
  if (!open) {
    g_open.store(false, std::memory_order_relaxed);
  }
}

bool SettingsMenuDialog::BeginRows(const char* id) {
  if (!ImGui::BeginTable(id, 2, ImGuiTableFlags_SizingStretchProp)) {
    return false;
  }
  ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthStretch, 0.5f);
  ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch, 0.5f);
  return true;
}

void SettingsMenuDialog::Label(const char* label, const char* help,
                               std::initializer_list<const char*> cvars) {
  ImGui::TableNextRow();
  ImGui::TableSetColumnIndex(0);
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(label);
  if (help) {
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("%s", help);
    }
  }
  for (const char* cvar : cvars) {
    if (ApplyOf(cvar) == Apply::kRestart) {
      ImGui::SameLine();
      ImGui::TextColored(kRestartColor, "restart");
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Takes effect after restarting the game");
      }
      break;
    }
  }
  ImGui::TableSetColumnIndex(1);
  ImGui::SetNextItemWidth(-FLT_MIN);
  if (focus_first_) {
    ImGui::SetKeyboardFocusHere();
    focus_first_ = false;
  }
}

void SettingsMenuDialog::CheckboxRow(const char* label, const char* help, const char* cvar) {
  Label(label, help, {cvar});
  ImGui::PushID(cvar);
  bool value = Get(cvar) == "true";
  if (ImGui::Checkbox("##v", &value)) {
    Set(cvar, value ? "true" : "false");
  }
  ImGui::PopID();
}

void SettingsMenuDialog::ComboRow(const char* label, const char* help, const char* cvar,
                                  std::span<const Choice> choices) {
  Label(label, help, {cvar});
  std::string current = Get(cvar);
  std::string preview = "Custom (" + current + ")";
  for (const auto& choice : choices) {
    if (current == choice.value) {
      preview = choice.label;
    }
  }
  ImGui::PushID(cvar);
  if (ImGui::BeginCombo("##v", preview.c_str())) {
    for (const auto& choice : choices) {
      bool selected = current == choice.value;
      if (ImGui::Selectable(choice.label, selected)) {
        Set(cvar, choice.value);
      }
      if (selected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }
  ImGui::PopID();
}

void SettingsMenuDialog::DrawDisplay() {
  if (!BeginRows("##display")) return;

  CheckboxRow("Fullscreen", "Borderless fullscreen on the current monitor.", "fullscreen");

  // Window size: two cvars, one combo.
  {
    static const std::pair<int, int> kSizes[] = {
        {1280, 720}, {1600, 900}, {1920, 1080}, {2560, 1440}, {3840, 2160}};
    Label("Window size", "Size of the game window when not fullscreen.",
          {"window_width", "window_height"});
    std::string width = Get("window_width"), height = Get("window_height");
    std::string preview =
        width == "0" ? std::string("Default") : width + " x " + height;
    if (ImGui::BeginCombo("##window_size", preview.c_str())) {
      for (const auto& [w, h] : kSizes) {
        std::string label = std::to_string(w) + " x " + std::to_string(h);
        bool selected = width == std::to_string(w) && height == std::to_string(h);
        if (ImGui::Selectable(label.c_str(), selected)) {
          Set("window_width", std::to_string(w));
          Set("window_height", std::to_string(h));
        }
      }
      ImGui::EndCombo();
    }
  }

  CheckboxRow("Vertical sync",
              "Ties presentation and the game's frame timing to 60 Hz. Turning it off "
              "may make the game run faster than intended.",
              "vsync");
  CheckboxRow("Keep 16:9 (black bars)",
              "Letterbox the picture when the window is not 16:9 instead of stretching it.",
              "present_letterbox");
  static const Choice kEffects[] = {
      {"bilinear", "Bilinear (smooth)"},
      {"cas", "AMD CAS (sharpened)"},
      {"fsr", "AMD FSR 1 (upscaled, sharpened)"},
  };
  ComboRow("Upscaling filter",
           "How the 720p picture is scaled to the window. Matters most for large "
           "windows and fullscreen.",
           "present_effect", kEffects);
  ImGui::EndTable();
}

void SettingsMenuDialog::DrawGraphics() {
  if (!BeginRows("##graphics")) return;

  // Internal resolution: draw_resolution_scale_x and _y together.
  {
    static const Choice kScales[] = {
        {"1", "1x  (1280 x 720, original)"},
        {"2", "2x  (2560 x 1440)"},
        {"3", "3x  (3840 x 2160)"},
    };
    Label("Internal resolution",
          "Renders at a multiple of the Xbox 360's 720p. Sharper, but needs a strong GPU "
          "and much more video memory. Experimental in this port: if anything looks "
          "wrong, go back to 1x.",
          {"draw_resolution_scale_x"});
    std::string current = Get("draw_resolution_scale_x");
    std::string preview = current + "x";
    for (const auto& scale : kScales) {
      if (current == scale.value) preview = scale.label;
    }
    if (ImGui::BeginCombo("##resolution_scale", preview.c_str())) {
      for (const auto& scale : kScales) {
        if (ImGui::Selectable(scale.label, current == scale.value)) {
          Set("draw_resolution_scale_x", scale.value);
          Set("draw_resolution_scale_y", scale.value);
        }
      }
      ImGui::EndCombo();
    }
  }

  static const Choice kAniso[] = {
      {"-1", "Game decides"}, {"0", "Off"}, {"2", "2x"},
      {"3", "4x"},            {"4", "8x"},  {"5", "16x"},
  };
  ComboRow("Texture filtering (anisotropic)",
           "Keeps the road and ground textures sharp at shallow angles. Cheap on "
           "modern GPUs.",
           "anisotropic_override", kAniso);

  static const Choice kAntiAliasing[] = {
      {"none", "Off"}, {"fxaa", "FXAA"}, {"fxaa_extreme", "FXAA (strong)"}};
  ComboRow("Extra anti-aliasing",
           "Post-process smoothing on top of the game's own anti-aliasing. Slightly "
           "blurs the picture.",
           "swap_post_effect", kAntiAliasing);

  static const Choice kReadback[] = {
      {"none", "Off (fastest, road flickers)"},
      {"fast", "Every frame"},
      {"some", "Balanced (recommended)"},
      {"full", "Exact (slow)"},
  };
  ComboRow("Render-to-texture readback",
           "Copies GPU-rendered textures back to the game's memory. FM3 needs this "
           "for its shadow maps: with it off the road flickers and shows block "
           "patterns while driving.",
           "readback_resolve", kReadback);
  ImGui::EndTable();
}

void SettingsMenuDialog::CaptureKey() {
  if (capturing_.empty()) return;
  if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
    capturing_.clear();
    capture_cooldown_until_ = ImGui::GetTime() + 0.3;
    return;
  }
  ImGuiIO& io = ImGui::GetIO();
  for (int k = ImGuiKey_NamedKey_BEGIN; k < ImGuiKey_NamedKey_END; ++k) {
    auto key = ImGuiKey(k);
    if (!ImGui::IsKeyPressed(key, false)) continue;
    VirtualKey vk = ToVirtualKey(key);
    std::string name = vk == VirtualKey::kNone ? std::string() : rex::ui::VirtualKeyToString(vk);
    if (name.empty() || name == kToggleKey) continue;
    std::string token;
    if (io.KeyCtrl) token += "Ctrl+";
    if (io.KeyShift) token += "Shift+";
    if (io.KeyAlt) token += "Alt+";
    token += name;
    std::vector<std::string> keys = capture_adds_ ? SplitKeys(Get(capturing_))
                                                  : std::vector<std::string>();
    if (std::find(keys.begin(), keys.end(), token) == keys.end()) {
      keys.push_back(token);
    }
    Set(capturing_, JoinKeys(keys));
    capturing_.clear();
    capture_cooldown_until_ = ImGui::GetTime() + 0.3;
    return;
  }
}

void SettingsMenuDialog::DrawControls() {
  if (BeginRows("##input")) {
    CheckboxRow("Keyboard controls",
                "Play with the keys below. Turn it off if you play with a controller.",
                "mnk_mode");
    CheckboxRow("Mouse moves the camera",
                "Use the mouse as the right stick (look around).", "mnk_mouse");
    Label("Mouse sensitivity", nullptr, {"mnk_sensitivity"});
    {
      bool mouse = Get("mnk_mouse") == "true";
      ImGui::BeginDisabled(!mouse);
      float sensitivity = float(std::atof(Get("mnk_sensitivity").c_str()));
      if (ImGui::SliderFloat("##sensitivity", &sensitivity, 0.1f, 5.0f, "%.2f")) {
        Set("mnk_sensitivity", std::to_string(sensitivity));
      }
      ImGui::EndDisabled();
    }
    ImGui::EndTable();
  }

  CaptureKey();
  bool cooling_down = ImGui::GetTime() < capture_cooldown_until_;
  bool keyboard = Get("mnk_mode") == "true";
  ImGui::Spacing();
  ImGui::TextDisabled(
      "Keys can't be pressed partway, so throttle, brake and steering are all-or-nothing.\n"
      "A key with Shift/Ctrl/Alt only fires with exactly those modifiers held.");
  ImGui::BeginDisabled(!keyboard);
  for (const auto& group : BindingGroups()) {
    ImGui::SeparatorText(group.title);
    if (!ImGui::BeginTable(group.title, 3, ImGuiTableFlags_SizingStretchProp)) continue;
    ImGui::TableSetupColumn("action", ImGuiTableColumnFlags_WidthStretch, 0.36f);
    ImGui::TableSetupColumn("keys", ImGuiTableColumnFlags_WidthStretch, 0.34f);
    ImGui::TableSetupColumn("edit", ImGuiTableColumnFlags_WidthFixed);
    for (const auto& binding : group.bindings) {
      ImGui::PushID(binding.cvar);
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::AlignTextToFramePadding();
      ImGui::TextUnformatted(binding.label);

      ImGui::TableSetColumnIndex(1);
      ImGui::AlignTextToFramePadding();
      std::string keys = Get(binding.cvar);
      if (capturing_ == binding.cvar) {
        ImGui::TextColored(kRestartColor, capture_adds_ ? "Press a key to add (Esc: cancel)"
                                                        : "Press a key (Esc: cancel)");
      } else if (keys.empty()) {
        ImGui::TextDisabled("not bound");
      } else {
        std::string shown = keys;
        for (size_t i = 0; (i = shown.find(',', i)) != std::string::npos; i += 3) {
          shown.replace(i, 1, " / ");
        }
        ImGui::TextUnformatted(shown.c_str());
        std::string conflicts = KeyConflicts(binding.cvar);
        if (!conflicts.empty()) {
          ImGui::SameLine();
          ImGui::TextColored(kWarningColor, "(!)");
          if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", conflicts.c_str());
          }
        }
      }

      ImGui::TableSetColumnIndex(2);
      if (ImGui::SmallButton("Set") && !cooling_down) {
        capturing_ = binding.cvar;
        capture_adds_ = false;
      }
      if (ImGui::IsItemHovered()) ImGui::SetTooltip("Replace with one key");
      ImGui::SameLine();
      if (ImGui::SmallButton("Add") && !cooling_down) {
        capturing_ = binding.cvar;
        capture_adds_ = true;
      }
      if (ImGui::IsItemHovered()) ImGui::SetTooltip("Add another key for this action");
      ImGui::SameLine();
      if (ImGui::SmallButton("Clear")) {
        Set(binding.cvar, "");
      }
      ImGui::SameLine();
      ImGui::BeginDisabled(keys == g_shipped[binding.cvar]);
      if (ImGui::SmallButton("Default")) {
        Set(binding.cvar, g_shipped[binding.cvar]);
      }
      ImGui::EndDisabled();
      ImGui::PopID();
    }
    ImGui::EndTable();
  }
  ImGui::EndDisabled();
}

void SettingsMenuDialog::DrawGame() {
  if (BeginRows("##game")) {
    static const Choice kLanguages[] = {
        {"1", "English"},   {"2", "Japanese"},          {"3", "German"},
        {"4", "French"},    {"5", "Spanish"},           {"6", "Italian"},
        {"7", "Korean"},    {"8", "Chinese (Traditional)"}, {"9", "Portuguese"},
        {"10", "Chinese (Simplified)"}, {"11", "Polish"}, {"12", "Russian"},
    };
    ComboRow("Language",
             "Console language reported to the game. Falls back to English if the "
             "game has no text for it.",
             "user_language", kLanguages);
    CheckboxRow("Mute sound", nullptr, "audio_mute");
    ImGui::EndTable();
  }

  ImGui::SeparatorText("Hotkeys");
  static const std::pair<const char*, const char*> kHotkeys[] = {
      {"F2", "This menu"},
      {"F3", "Performance overlay (FPS, frame times)"},
      {"F4", "Advanced settings (every emulator option)"},
      {"F7", "Achievements"},
      {"` (Backtick)", "Log console"},
      {"In this menu", "Mouse, or arrows + Enter/Space; PgUp/PgDn switch tabs"},
  };
  if (ImGui::BeginTable("##hotkeys", 2, ImGuiTableFlags_SizingFixedFit)) {
    for (const auto& [key, what] : kHotkeys) {
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextUnformatted(key);
      ImGui::TableSetColumnIndex(1);
      ImGui::TextDisabled("%s", what);
    }
    ImGui::EndTable();
  }
  ImGui::Spacing();
  ImGui::TextDisabled("Settings are saved to %s", UserSettingsPath().filename().string().c_str());
}

void SettingsMenuDialog::DrawFooter() {
  Snapshot now = TakeSnapshot();
  bool dirty = now != g_saved;

  std::vector<std::string> restart;
  for (const auto& [name, value] : now) {
    auto it = kRestartNames.find(name);
    if (it != kRestartNames.end() && value != g_boot[name] &&
        std::find(restart.begin(), restart.end(), it->second) == restart.end()) {
      restart.emplace_back(it->second);
    }
  }

  ImGui::Separator();
  if (!restart.empty()) {
    std::string list;
    for (const auto& name : restart) list += (list.empty() ? "" : ", ") + name;
    ImGui::TextColored(kRestartColor, "Restart the game to apply: %s", list.c_str());
  } else if (dirty) {
    ImGui::TextDisabled("Unsaved changes (already active, lost on exit unless saved)");
  } else {
    ImGui::TextDisabled("%s", status_.c_str());
  }

  if (ImGui::Button("Save")) {
    SaveUserSettings(status_);
  }
  if (!restart.empty()) {
    ImGui::SameLine();
    if (ImGui::Button("Save and restart") && SaveUserSettings(status_)) {
      Relaunch();
      if (quit_) quit_();
    }
  }
  ImGui::SameLine();
  ImGui::BeginDisabled(!dirty);
  if (ImGui::Button("Undo changes")) {
    Restore(g_saved);
    status_.clear();
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::BeginDisabled(now == g_shipped);
  if (ImGui::Button("Reset to defaults")) {
    Restore(g_shipped);
    status_.clear();
  }
  ImGui::EndDisabled();
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    ImGui::SetTooltip("Back to the shipped forza3.toml settings (Save to keep)");
  }
  ImGui::SameLine();
  float close_width = ImGui::CalcTextSize("Close").x + ImGui::GetStyle().FramePadding.x * 2.0f;
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                       std::max(0.0f, ImGui::GetContentRegionAvail().x - close_width));
  if (ImGui::Button("Close")) {
    g_open.store(false, std::memory_order_relaxed);
  }
}

std::function<void()> g_quit;
rex::ui::ImGuiDrawer* g_drawer = nullptr;

}  // namespace

std::filesystem::path UserSettingsPath() {
#ifdef _WIN32
  wchar_t exe[MAX_PATH];
  if (GetModuleFileNameW(nullptr, exe, MAX_PATH)) {
    return std::filesystem::path(exe).parent_path() / "forza3.user.toml";
  }
#endif
  return "forza3.user.toml";
}

void AddSettingsMenuFont(ImFontAtlas* atlas) {
  // Bahnschrift (Windows 10+) suits a racing game; Segoe UI as a fallback.
  for (const char* path : {"C:/Windows/Fonts/bahnschrift.ttf", "C:/Windows/Fonts/segoeui.ttf"}) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) continue;
    ImFontConfig config;
    config.OversampleH = 2;
    config.OversampleV = 1;
    g_font = atlas->AddFontFromFileTTF(path, kFontSize, &config);
    if (g_font) {
      REXLOG_INFO("Settings: menu font {}", path);
      return;
    }
  }
}

void LoadUserSettings() {
  g_shipped = TakeSnapshot();
  std::error_code ec;
  if (std::filesystem::exists(UserSettingsPath(), ec)) {
    rex::cvar::LoadConfig(UserSettingsPath());
    REXLOG_INFO("Settings: loaded {}", UserSettingsPath().string());
  }
  g_saved = TakeSnapshot();
  g_boot = g_saved;
}

void CreateSettingsMenu(rex::ui::ImGuiDrawer* drawer, std::function<void()> quit) {
  if (!drawer || g_drawer) return;
  g_drawer = drawer;
  g_quit = std::move(quit);
  new HintDialog(drawer);  // Deletes itself when done.
  rex::ui::RegisterBind("bind_forza3_settings", kToggleKey, "Toggle the Forza 3 settings menu",
                        [] {
                          if (g_menu) {
                            delete g_menu;
                          } else {
                            new SettingsMenuDialog(g_drawer, g_quit);
                          }
                        });
}

bool IsSettingsMenuOpen() { return g_open.load(std::memory_order_relaxed); }

void WaitForRestartedInstance() {
#ifdef _WIN32
  wchar_t pid_text[32];
  if (!GetEnvironmentVariableW(L"FORZA3_RESTART_WAIT_PID", pid_text, 32)) {
    return;
  }
  SetEnvironmentVariableW(L"FORZA3_RESTART_WAIT_PID", nullptr);
  DWORD pid = DWORD(std::wcstoul(pid_text, nullptr, 10));
  if (HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, pid)) {
    WaitForSingleObject(process, 60000);
    CloseHandle(process);
  }
#endif
}

}  // namespace forza3
