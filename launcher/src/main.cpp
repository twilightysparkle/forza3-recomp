// Forza3Launcher - profiles, settings and game import for the recompiled
// Forza Motorsport 3. Sits next to forza3.exe; writes forza3.user.toml (read by
// the game after its own forza3.toml) and starts the game.
//
// Layout: the cover art fills a column on the left, with the profile picker,
// task progress and PLAY under it; settings live in tabs on the right, grouped
// into cards. The cover is read from cover.png next to the exe at runtime - it
// is not part of the program (without it a panel is drawn instead).
//
// Files next to the exe:
//   game\                    Disc 1, with Disc 2 in game\Disc 2
//   userdata\                profiles, shared Disc 2 content, shader cache
//   game_files.sha256        hashes of the files on the discs
//   forza3.launcher.ini      the launcher's own state (game folder, profile)

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>

#include <windows.h>
#include <shellapi.h>

#include <algorithm>
#include <atomic>
#include <cfloat>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#include "iso.h"
#include "profiles.h"
#include "settings.h"
#include "task.h"
#include "verify.h"

#ifndef FORZA3_VERSION
#define FORZA3_VERSION "dev"
#endif

namespace launcher {
namespace {

namespace fs = std::filesystem;

// --- Look -----------------------------------------------------------------------

const ImVec4 kBg(0.055f, 0.058f, 0.068f, 1);
const ImVec4 kCard(0.095f, 0.10f, 0.115f, 1);
const ImVec4 kControl(0.16f, 0.17f, 0.19f, 1);
const ImVec4 kText(0.92f, 0.93f, 0.95f, 1);
const ImVec4 kDim(0.58f, 0.60f, 0.65f, 1);
const ImVec4 kAccent(0.85f, 0.14f, 0.11f, 1);
const ImVec4 kAccentHover(0.95f, 0.24f, 0.19f, 1);
const ImVec4 kGood(0.38f, 0.80f, 0.47f, 1);
const ImVec4 kBad(0.95f, 0.45f, 0.35f, 1);

float g_scale = 1.0f;
float S(float v) { return v * g_scale; }

// --- Paths and the launcher's own state ------------------------------------------

fs::path ExeDir() {
  wchar_t path[MAX_PATH];
  GetModuleFileNameW(nullptr, path, MAX_PATH);
  return fs::path(path).parent_path();
}

std::string Utf8(const fs::path& path) {
  auto u8 = path.u8string();
  return std::string(u8.begin(), u8.end());
}

fs::path FromUtf8(const std::string& text) { return fs::path(std::u8string(text.begin(), text.end())); }

bool HasGame(const fs::path& dir) {
  std::error_code ec;
  return !dir.empty() && fs::is_regular_file(dir / "default.xex", ec);
}

bool HasBonus(const fs::path& dir) {
  std::error_code ec;
  return fs::is_directory(dir / "Disc 2" / "Content", ec);
}

struct State {
  std::string game_dir;  // Empty: <exe dir>\game.
  std::string profile;
  bool verified = false;  // The installed files passed the last verification.

  static fs::path File() { return ExeDir() / "forza3.launcher.ini"; }
  void Load() {
    std::ifstream in(File());
    std::string line;
    while (std::getline(in, line)) {
      if (!line.empty() && line.back() == '\r') line.pop_back();
      size_t eq = line.find('=');
      if (eq == std::string::npos) continue;
      std::string key = line.substr(0, eq), value = line.substr(eq + 1);
      if (key == "game_dir") game_dir = value;
      if (key == "profile") profile = value;
      if (key == "verified") verified = value == "1";
    }
  }
  void Save() const {
    std::ofstream out(File(), std::ios::trunc);
    out << "game_dir=" << game_dir << "\nprofile=" << profile << "\nverified=" << (verified ? 1 : 0)
        << "\n";
  }
};

// --- Key names (the game's keybind syntax) -----------------------------------------

std::string KeyName(SDL_Keycode key) {
  if (key >= SDLK_A && key <= SDLK_Z) return std::string(1, char('A' + (key - SDLK_A)));
  if (key >= SDLK_0 && key <= SDLK_9) return std::string(1, char('0' + (key - SDLK_0)));
  if (key >= SDLK_F1 && key <= SDLK_F12) return "F" + std::to_string(1 + key - SDLK_F1);
  if (key >= SDLK_KP_1 && key <= SDLK_KP_9) return "Numpad" + std::to_string(1 + key - SDLK_KP_1);
  switch (key) {
    case SDLK_KP_0: return "Numpad0";
    case SDLK_SPACE: return "Space";
    case SDLK_RETURN: return "Return";
    case SDLK_TAB: return "Tab";
    case SDLK_BACKSPACE: return "Backspace";
    case SDLK_DELETE: return "Delete";
    case SDLK_INSERT: return "Insert";
    case SDLK_HOME: return "Home";
    case SDLK_END: return "End";
    case SDLK_PAGEUP: return "PageUp";
    case SDLK_PAGEDOWN: return "PageDown";
    case SDLK_LEFT: return "Left";
    case SDLK_RIGHT: return "Right";
    case SDLK_UP: return "Up";
    case SDLK_DOWN: return "Down";
    case SDLK_GRAVE: return "Backtick";
    case SDLK_MINUS: return "Minus";
    case SDLK_EQUALS: return "Plus";
    case SDLK_COMMA: return "Comma";
    case SDLK_PERIOD: return "Period";
    case SDLK_SEMICOLON: return "Semicolon";
    case SDLK_SLASH: return "Slash";
    case SDLK_BACKSLASH: return "Backslash";
    case SDLK_LEFTBRACKET: return "LBracket";
    case SDLK_RIGHTBRACKET: return "RBracket";
    case SDLK_APOSTROPHE: return "Quote";
    case SDLK_KP_ENTER: return "NumpadEnter";
    case SDLK_KP_PLUS: return "NumpadPlus";
    case SDLK_KP_MINUS: return "NumpadMinus";
    case SDLK_KP_MULTIPLY: return "NumpadStar";
    case SDLK_KP_DIVIDE: return "NumpadSlash";
    case SDLK_CAPSLOCK: return "CapsLock";
    case SDLK_LSHIFT: case SDLK_RSHIFT: return "Shift";
    case SDLK_LCTRL: case SDLK_RCTRL: return "Control";
    case SDLK_LALT: case SDLK_RALT: return "Alt";
    default: return {};
  }
}

std::vector<std::string> SplitKeys(const std::string& value) {
  std::vector<std::string> keys;
  size_t start = 0;
  while (start <= value.size()) {
    size_t comma = value.find(',', start);
    std::string key = value.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
    while (!key.empty() && key.front() == ' ') key.erase(key.begin());
    while (!key.empty() && key.back() == ' ') key.pop_back();
    if (!key.empty()) keys.push_back(key);
    if (comma == std::string::npos) break;
    start = comma + 1;
  }
  return keys;
}

// --- App ---------------------------------------------------------------------------

enum class Tab { kProfiles, kGraphics, kControls, kAudio, kInstall };
enum class Discs { kAll, kGame, kBonus };

struct Cover {
  SDL_GPUTexture* texture = nullptr;
  int width = 0, height = 0;
};

struct App {
  SDL_Window* window = nullptr;
  SDL_GPUDevice* gpu = nullptr;
  ImFont* title_font = nullptr;
  Cover cover;

  State state;
  Settings settings;
  Profiles profiles{ExeDir() / "userdata"};
  std::vector<ManifestEntry> manifest;
  Tab tab = Tab::kProfiles;

  std::vector<std::string> profile_names;
  char new_profile[32] = {};
  std::string profile_error;
  std::string confirm_delete;

  std::string capturing;  // keybind cvar waiting for a key
  bool capture_adds = false;

  TaskProgress task;
  std::thread worker;
  std::mutex result_mutex;
  std::string task_title;
  std::string task_result;
  bool task_result_ok = true;

  std::mutex dialog_mutex;
  std::optional<std::string> picked_iso;
  std::optional<std::string> picked_folder;

  std::atomic<bool> game_running{false};
  std::atomic<bool> game_exited{false};
  std::atomic<DWORD> game_exit_code{0};
  std::thread game_waiter;
  std::string game_error;

  fs::path GameDir() const {
    return state.game_dir.empty() ? ExeDir() / "game" : FromUtf8(state.game_dir);
  }
  bool Installed() const { return HasGame(GameDir()) && HasBonus(GameDir()); }

  void RefreshProfiles() {
    profile_names = profiles.List();
    if (std::find(profile_names.begin(), profile_names.end(), state.profile) == profile_names.end()) {
      state.profile = profile_names.empty() ? "" : profile_names.front();
      state.Save();
    }
  }
};

// --- Background tasks ----------------------------------------------------------------

void FinishTask(App& app, bool ok, std::string message) {
  std::lock_guard lock(app.result_mutex);
  app.task_result = std::move(message);
  app.task_result_ok = ok;
  app.task.state = ok ? TaskProgress::State::kDone : TaskProgress::State::kFailed;
}

void StartTask(App& app, std::string title, std::function<void(App&)> work) {
  if (app.task.Busy()) return;
  if (app.worker.joinable()) app.worker.join();
  app.task.cancel = false;
  app.task.done = 0;
  app.task.total = 0;
  app.task.SetStatus("");
  app.task.state = TaskProgress::State::kRunning;
  app.task_title = std::move(title);
  {
    std::lock_guard lock(app.result_mutex);
    app.task_result.clear();
  }
  app.worker = std::thread([&app, work = std::move(work)] { work(app); });
}

// Verifies the files of `which` disc(s); returns the message for the player.
std::string RunVerify(App& app, const fs::path& game_dir, Discs which, bool& ok) {
  std::vector<ManifestEntry> list;
  for (const auto& entry : app.manifest) {
    if (which == Discs::kAll || (which == Discs::kBonus) == entry.bonus) list.push_back(entry);
  }
  if (list.empty()) {
    ok = false;
    return "game_files.sha256 is missing next to the launcher: the files can't be verified.";
  }
  VerifyReport report;
  if (!VerifyGame(game_dir, list, app.task, report)) {
    ok = false;
    return "Verification cancelled.";
  }
  ok = report.ok();
  if (ok) return "All " + std::to_string(report.checked) + " files verified.";
  std::string message = std::to_string(report.missing.size() + report.wrong_size.size() +
                                       report.wrong_hash.size()) +
                        " of " + std::to_string(report.checked) +
                        " files are missing or don't match. Import your disc image again.";
  const std::vector<std::string>* lists[] = {&report.missing, &report.wrong_size, &report.wrong_hash};
  int shown = 0;
  for (const auto* names : lists) {
    for (const auto& name : *names) {
      if (shown++ < 5) message += "\n  " + name;
    }
  }
  return message;
}

void ImportIso(App& app, fs::path iso) {
  StartTask(app, "IMPORTING", [iso](App& app) {
    fs::path game_dir = ExeDir() / "game";
    std::string error;
    DiscKind kind = ExtractIso(iso, game_dir, app.task, error);
    if (kind == DiscKind::kFailed) return FinishTask(app, false, "Import failed: " + error);
    bool game = kind == DiscKind::kGame;
    if (game) app.state.game_dir.clear();  // The launcher's own game\ folder.
    bool ok = false;
    std::string verify = RunVerify(app, game_dir, game ? Discs::kGame : Discs::kBonus, ok);
    app.state.verified = ok && HasGame(game_dir) && HasBonus(game_dir);
    app.state.Save();
    std::string what = game ? "Disc 1 imported." : "Disc 2 imported.";
    std::string next;
    if (ok && !HasBonus(game_dir)) next = " Now import Disc 2.";
    if (ok && !HasGame(game_dir)) next = " Now import Disc 1.";
    FinishTask(app, ok, what + " " + verify + next);
  });
}

void StartVerify(App& app) {
  StartTask(app, "VERIFYING FILES", [](App& app) {
    bool ok = false;
    std::string message = RunVerify(app, app.GameDir(), Discs::kAll, ok);
    app.state.verified = ok;
    app.state.Save();
    FinishTask(app, ok, message);
  });
}

// --- Dialogs ---------------------------------------------------------------------------

void PickIso(App& app) {
  static const SDL_DialogFileFilter kFilters[] = {{"Xbox 360 disc image (*.iso)", "iso"},
                                                   {"All files", "*"}};
  SDL_ShowOpenFileDialog(
      [](void* user, const char* const* files, int) {
        auto* app = static_cast<App*>(user);
        if (files && files[0]) {
          std::lock_guard lock(app->dialog_mutex);
          app->picked_iso = files[0];
        }
      },
      &app, app.window, kFilters, 2, nullptr, false);
}

void PickFolder(App& app) {
  SDL_ShowOpenFolderDialog(
      [](void* user, const char* const* files, int) {
        auto* app = static_cast<App*>(user);
        if (files && files[0]) {
          std::lock_guard lock(app->dialog_mutex);
          app->picked_folder = files[0];
        }
      },
      &app, app.window, nullptr, false);
}

void HandleDialogs(App& app) {
  std::optional<std::string> iso, folder;
  {
    std::lock_guard lock(app.dialog_mutex);
    iso.swap(app.picked_iso);
    folder.swap(app.picked_folder);
  }
  if (iso) ImportIso(app, FromUtf8(*iso));
  if (folder) {
    fs::path dir = FromUtf8(*folder);
    std::lock_guard lock(app.result_mutex);
    if (HasGame(dir)) {
      app.state.game_dir = Utf8(dir);
      app.state.verified = false;
      app.state.Save();
      app.task_result = HasBonus(dir) ? "Using the game in " + Utf8(dir) + ". Verify its files."
                                      : "Using the game in " + Utf8(dir) +
                                            ". Its Disc 2 folder is missing: import Disc 2.";
      app.task_result_ok = HasBonus(dir);
    } else {
      app.task_result = "That folder has no default.xex: choose the folder Disc 1 was extracted to.";
      app.task_result_ok = false;
    }
  }
}

// --- The game ------------------------------------------------------------------------

std::wstring Quote(const fs::path& path) { return L"\"" + path.wstring() + L"\""; }

void Play(App& app) {
  fs::path exe = ExeDir() / "forza3.exe";
  std::string error;
  app.profiles.LinkSharedContent(app.state.profile, error);
  std::wstring command = Quote(exe) + L" --game_data_root=" + Quote(app.GameDir()) +
                         L" --user_data_root=" + Quote(app.profiles.ProfileDir(app.state.profile)) +
                         L" --cache_root=" + Quote(app.profiles.CacheDir()) + L" --user_name=" +
                         Quote(FromUtf8(app.state.profile));
  STARTUPINFOW startup = {sizeof(startup)};
  PROCESS_INFORMATION process = {};
  if (!CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, FALSE, 0, nullptr,
                      ExeDir().c_str(), &startup, &process)) {
    app.game_error = "Could not start forza3.exe (error " + std::to_string(GetLastError()) + ").";
    return;
  }
  CloseHandle(process.hThread);
  app.game_error.clear();
  app.game_running = true;
  SDL_MinimizeWindow(app.window);
  if (app.game_waiter.joinable()) app.game_waiter.join();
  app.game_waiter = std::thread([&app, handle = process.hProcess] {
    WaitForSingleObject(handle, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(handle, &code);
    CloseHandle(handle);
    app.game_exit_code = code;
    app.game_exited = true;
  });
}

void HandleGameExit(App& app) {
  if (!app.game_exited.exchange(false)) return;
  app.game_running = false;
  SDL_RestoreWindow(app.window);
  SDL_RaiseWindow(app.window);
  // Settings may have been changed in game (F2).
  app.settings.Load(ExeDir());
  if (DWORD code = app.game_exit_code; code != 0) {
    char hex[16];
    std::snprintf(hex, sizeof(hex), "0x%08lX", static_cast<unsigned long>(code));
    app.game_error = std::string("The game closed with an error (") + hex +
                     "). The files in crashdumps\\ and logs\\ show what happened; please attach "
                     "them when you report it.";
  }
}

// --- Widgets -------------------------------------------------------------------------

void TextDim(const std::string& text) {
  ImGui::PushStyleColor(ImGuiCol_Text, kDim);
  ImGui::PushTextWrapPos(0);
  ImGui::TextUnformatted(text.c_str());
  ImGui::PopTextWrapPos();
  ImGui::PopStyleColor();
}

void TextColor(const ImVec4& color, const std::string& text) {
  ImGui::PushStyleColor(ImGuiCol_Text, color);
  ImGui::PushTextWrapPos(0);
  ImGui::TextUnformatted(text.c_str());
  ImGui::PopTextWrapPos();
  ImGui::PopStyleColor();
}

bool PrimaryButton(const char* label, ImVec2 size) {
  ImGui::PushStyleColor(ImGuiCol_Button, kAccent);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kAccentHover);
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, kAccent);
  ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
  bool pressed = ImGui::Button(label, size);
  ImGui::PopStyleColor(4);
  return pressed;
}

// A rounded panel with a title; its rows are a label | control table.
void BeginCard(const char* id, const char* title) {
  ImGui::PushStyleColor(ImGuiCol_ChildBg, kCard);
  ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, S(8));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(S(18), S(14)));
  ImGui::BeginChild(id, ImVec2(0, 0),
                    ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding |
                        ImGuiChildFlags_NavFlattened);
  ImGui::PushStyleColor(ImGuiCol_Text, kDim);
  ImGui::TextUnformatted(title);
  ImGui::PopStyleColor();
  ImGui::Spacing();
}

void EndCard() {
  ImGui::EndChild();
  ImGui::PopStyleVar(2);
  ImGui::PopStyleColor();
  ImGui::Spacing();
}

bool BeginRows(const char* id) {
  if (!ImGui::BeginTable(id, 2, ImGuiTableFlags_SizingStretchProp)) return false;
  ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthStretch, 0.45f);
  ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch, 0.55f);
  return true;
}

void Row(const char* label, const char* help = nullptr, bool restart = false) {
  ImGui::TableNextRow();
  ImGui::TableSetColumnIndex(0);
  ImGui::AlignTextToFramePadding();
  ImGui::TextUnformatted(label);
  if (help && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", help);
  if (restart) {
    ImGui::SameLine();
    ImGui::TextColored(kDim, "(next start)");
  }
  ImGui::TableSetColumnIndex(1);
  ImGui::SetNextItemWidth(-FLT_MIN);
}

void CheckboxRow(App& app, const char* label, const char* key, const char* help = nullptr) {
  Row(label, help);
  bool value = app.settings.GetBool(key);
  ImGui::PushID(key);
  if (ImGui::Checkbox("##v", &value)) app.settings.SetBool(key, value);
  ImGui::PopID();
}

struct Choice {
  const char* value;
  const char* label;
};

void ComboRow(App& app, const char* label, const char* key, std::initializer_list<Choice> choices,
              const char* help = nullptr, bool restart = false) {
  Row(label, help, restart);
  std::string current = app.settings.Get(key);
  const char* preview = current.c_str();
  for (const auto& c : choices) {
    if (current == c.value) preview = c.label;
  }
  ImGui::PushID(key);
  if (ImGui::BeginCombo("##v", preview)) {
    for (const auto& c : choices) {
      if (ImGui::Selectable(c.label, current == c.value)) app.settings.Set(key, c.value);
    }
    ImGui::EndCombo();
  }
  ImGui::PopID();
}

// --- Left column: cover, profile, play ------------------------------------------------

float CoverWidth() { return S(340); }

void DrawLeftColumn(App& app) {
  float width = CoverWidth();
  ImGui::BeginChild("##left", ImVec2(width, 0), ImGuiChildFlags_NavFlattened,
                    ImGuiWindowFlags_NoScrollbar);
  ImDrawList* dl = ImGui::GetWindowDrawList();
  ImVec2 p0 = ImGui::GetCursorScreenPos();
  float aspect = app.cover.texture ? float(app.cover.height) / float(app.cover.width) : 1.25f;
  float cover_h = std::min(width * aspect, ImGui::GetContentRegionAvail().y - S(230));
  ImVec2 p1(p0.x + width, p0.y + cover_h);
  if (app.cover.texture) {
    dl->AddImage(ImTextureRef((ImTextureID)(intptr_t)app.cover.texture), p0, p1);
  } else {
    // No cover.png: a panel in the game's colours.
    dl->AddRectFilledMultiColor(p0, p1, IM_COL32(28, 30, 36, 255), IM_COL32(28, 30, 36, 255),
                                IM_COL32(70, 12, 10, 255), IM_COL32(70, 12, 10, 255));
    ImGui::SetCursorScreenPos(ImVec2(p0.x + S(24), p1.y - S(130)));
    if (app.title_font) ImGui::PushFont(app.title_font, app.title_font->LegacySize);
    ImGui::TextColored(kAccent, "FORZA");
    ImGui::SetCursorPosX(S(24));
    ImGui::TextUnformatted("MOTORSPORT 3");
    if (app.title_font) ImGui::PopFont();
    ImGui::SetCursorPosX(S(24));
    ImGui::TextColored(kDim, "Native PC Port %s", FORZA3_VERSION);
  }
  ImGui::SetCursorScreenPos(ImVec2(p0.x, p1.y + S(16)));

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(S(16), 0));
  ImGui::BeginChild("##controls", ImVec2(0, 0),
                    ImGuiChildFlags_AlwaysUseWindowPadding | ImGuiChildFlags_NavFlattened);
  float inner = ImGui::GetContentRegionAvail().x;

  ImGui::TextColored(kDim, "PROFILE");
  ImGui::SetNextItemWidth(inner);
  if (ImGui::BeginCombo("##profile", app.state.profile.empty() ? "No profile" : app.state.profile.c_str())) {
    for (const auto& name : app.profile_names) {
      if (ImGui::Selectable(name.c_str(), name == app.state.profile)) {
        app.state.profile = name;
        app.state.Save();
      }
    }
    if (ImGui::Selectable("Manage profiles...")) app.tab = Tab::kProfiles;
    ImGui::EndCombo();
  }
  ImGui::Spacing();

  // Status / progress.
  if (app.task.Busy()) {
    ImGui::TextColored(kDim, "%s", app.task_title.c_str());
    uint64_t total = app.task.total, done = app.task.done;
    char overlay[48];
    std::snprintf(overlay, sizeof(overlay), "%.1f / %.1f GB", done / 1e9, total / 1e9);
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, kAccent);
    ImGui::ProgressBar(total ? float(double(done) / double(total)) : 0.0f, ImVec2(inner, 0),
                       total ? overlay : "");
    ImGui::PopStyleColor();
  } else if (!app.Installed()) {
    TextColor(kBad, HasGame(app.GameDir()) ? "Disc 2 is not installed." : "The game is not installed.");
    if (ImGui::SmallButton("Go to Install")) app.tab = Tab::kInstall;
  } else if (!app.state.verified) {
    TextDim("Installed, not verified yet (Install tab).");
  } else {
    TextColor(kGood, "Ready to race.");
  }

  ImGui::SetCursorPosY(ImGui::GetWindowHeight() - S(62) - S(16));
  bool can_play = app.Installed() && !app.state.profile.empty() && !app.task.Busy() &&
                  !app.game_running;
  ImGui::BeginDisabled(!can_play);
  if (app.title_font) ImGui::PushFont(app.title_font, app.title_font->LegacySize * 0.8f);
  bool play = PrimaryButton(app.game_running ? "PLAYING" : "PLAY", ImVec2(inner, S(62)));
  if (app.title_font) ImGui::PopFont();
  ImGui::EndDisabled();
  if (play && can_play) Play(app);
  if (!can_play && !app.game_running && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    ImGui::SetTooltip("%s", !HasGame(app.GameDir())    ? "Import Disc 1 first (Install)"
                            : !HasBonus(app.GameDir()) ? "Import Disc 2 first (Install)"
                            : app.state.profile.empty() ? "Create a profile first (Profiles)"
                                                        : "Busy");
  }
  ImGui::EndChild();
  ImGui::PopStyleVar();
  ImGui::EndChild();
}

// --- Tabs ----------------------------------------------------------------------------

void TabProfiles(App& app) {
  BeginCard("##profiles", "PROFILES");
  TextDim("Each profile has its own saves. Its name is your gamertag in the game.");
  ImGui::Spacing();
  for (const auto& name : app.profile_names) {
    ImGui::PushID(name.c_str());
    if (ImGui::Selectable(name.c_str(), name == app.state.profile, 0,
                          ImVec2(ImGui::GetContentRegionAvail().x - S(90), 0))) {
      app.state.profile = name;
      app.state.Save();
    }
    ImGui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - S(80));
    if (ImGui::SmallButton("Delete")) {
      app.confirm_delete = name;
      ImGui::OpenPopup("Delete profile?");
    }
    if (ImGui::BeginPopupModal("Delete profile?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::Text("Delete \"%s\" and its saves?", app.confirm_delete.c_str());
      ImGui::TextColored(kDim, "It is moved to userdata\\deleted-profiles, not erased.");
      ImGui::Spacing();
      if (PrimaryButton("Delete", ImVec2(S(120), 0))) {
        if (app.profiles.Delete(app.confirm_delete, app.profile_error)) {
          app.profile_error.clear();
          app.RefreshProfiles();
        }
        ImGui::CloseCurrentPopup();
      }
      ImGui::SameLine();
      if (ImGui::Button("Keep", ImVec2(S(120), 0))) ImGui::CloseCurrentPopup();
      ImGui::EndPopup();
    }
    ImGui::PopID();
  }
  if (app.profile_names.empty()) TextDim("No profiles yet: create one below.");
  EndCard();

  BeginCard("##new", "NEW PROFILE");
  ImGui::SetNextItemWidth(S(300));
  bool enter = ImGui::InputTextWithHint("##name", "Name: 1-15 letters, digits, spaces",
                                        app.new_profile, 16, ImGuiInputTextFlags_EnterReturnsTrue);
  ImGui::SameLine();
  if (ImGui::Button("Create", ImVec2(S(120), 0)) || enter) {
    std::string name = app.new_profile;
    if (app.profiles.Create(name, app.profile_error)) {
      app.profile_error.clear();
      app.state.profile = name;
      app.state.Save();
      app.new_profile[0] = 0;
      app.RefreshProfiles();
    }
  }
  if (!app.profile_error.empty()) TextColor(kBad, app.profile_error);
  EndCard();
}

void TabGraphics(App& app) {
  BeginCard("##display", "DISPLAY");
  if (BeginRows("##rows")) {
    CheckboxRow(app, "Fullscreen", "fullscreen");
    Row("Window size", "Used when not fullscreen.", true);
    {
      static const std::pair<int, int> kSizes[] = {
          {1280, 720}, {1600, 900}, {1920, 1080}, {2560, 1440}, {3840, 2160}};
      std::string w = app.settings.Get("window_width"), h = app.settings.Get("window_height");
      std::string preview = w + " x " + h;
      if (ImGui::BeginCombo("##size", preview.c_str())) {
        for (auto [sw, sh] : kSizes) {
          std::string label = std::to_string(sw) + " x " + std::to_string(sh);
          if (ImGui::Selectable(label.c_str(), w == std::to_string(sw) && h == std::to_string(sh))) {
            app.settings.Set("window_width", std::to_string(sw));
            app.settings.Set("window_height", std::to_string(sh));
          }
        }
        ImGui::EndCombo();
      }
    }
    CheckboxRow(app, "Vertical sync", "vsync",
                "Keeps the game's 60 Hz timing. Off may make the game run faster than intended.");
    CheckboxRow(app, "Keep 16:9 (black bars)", "present_letterbox");
    ComboRow(app, "Upscaling filter", "present_effect",
             {{"bilinear", "Bilinear (smooth)"}, {"cas", "AMD CAS (sharpened)"},
              {"fsr", "AMD FSR 1 (upscaled, sharpened)"}},
             "How the picture is scaled to the window.", true);
    ImGui::EndTable();
  }
  EndCard();

  BeginCard("##quality", "QUALITY");
  if (BeginRows("##rows")) {
    Row("Internal resolution", "Renders at a multiple of the Xbox 360's 720p. Experimental.", true);
    {
      static const Choice kScales[] = {{"1", "1x  (1280 x 720, original)"},
                                       {"2", "2x  (2560 x 1440)"},
                                       {"3", "3x  (3840 x 2160)"}};
      std::string current = app.settings.Get("draw_resolution_scale_x");
      const char* preview = current.c_str();
      for (const auto& c : kScales) {
        if (current == c.value) preview = c.label;
      }
      if (ImGui::BeginCombo("##scale", preview)) {
        for (const auto& c : kScales) {
          if (ImGui::Selectable(c.label, current == c.value)) {
            app.settings.Set("draw_resolution_scale_x", c.value);
            app.settings.Set("draw_resolution_scale_y", c.value);
          }
        }
        ImGui::EndCombo();
      }
    }
    ComboRow(app, "Texture filtering", "anisotropic_override",
             {{"-1", "Game decides"}, {"0", "Off"}, {"2", "2x"}, {"3", "4x"}, {"4", "8x"},
              {"5", "16x"}},
             "Anisotropic filtering: sharper road at shallow angles.");
    ComboRow(app, "Extra anti-aliasing", "swap_post_effect",
             {{"none", "Off"}, {"fxaa", "FXAA"}, {"fxaa_extreme", "FXAA (strong)"}}, nullptr, true);
    ComboRow(app, "Render-to-texture readback", "readback_resolve",
             {{"none", "Off (fastest, road flickers)"}, {"fast", "Every frame"},
              {"some", "Balanced (recommended)"}, {"full", "Exact (slow)"}},
             "FM3 needs this for its shadow maps: off makes the road flicker.");
    ImGui::EndTable();
  }
  EndCard();
  TextDim("More options in game: press F2 (F4 lists every emulator option).");
}

void TabControls(App& app) {
  BeginCard("##input", "KEYBOARD AND MOUSE");
  if (BeginRows("##rows")) {
    CheckboxRow(app, "Keyboard controls", "mnk_mode",
                "Play with the keys below. Turn off when playing with a controller.");
    CheckboxRow(app, "Mouse moves the camera", "mnk_mouse");
    Row("Mouse sensitivity");
    float sensitivity = float(app.settings.GetDouble("mnk_sensitivity"));
    ImGui::BeginDisabled(!app.settings.GetBool("mnk_mouse"));
    if (ImGui::SliderFloat("##sens", &sensitivity, 0.1f, 5.0f, "%.2f")) {
      char text[32];
      std::snprintf(text, sizeof(text), "%.2f", sensitivity);
      app.settings.Set("mnk_sensitivity", text);
    }
    ImGui::EndDisabled();
    ImGui::EndTable();
  }
  TextDim("Any Xbox controller works without setup. Keys are all-or-nothing, so throttle, brake "
          "and steering are digital.");
  EndCard();

  const char* group = nullptr;
  bool open = false;
  for (const auto& def : BindDefs()) {
    if (!group || std::strcmp(group, def.group) != 0) {
      if (open) {
        ImGui::EndTable();
      }
      if (group) EndCard();
      group = def.group;
      std::string id = std::string("##") + group, title = group;
      std::transform(title.begin(), title.end(), title.begin(), ::toupper);
      BeginCard(id.c_str(), title.c_str());
      open = ImGui::BeginTable("##binds", 3, ImGuiTableFlags_SizingStretchProp);
      if (open) {
        ImGui::TableSetupColumn("a", ImGuiTableColumnFlags_WidthStretch, 0.34f);
        ImGui::TableSetupColumn("k", ImGuiTableColumnFlags_WidthStretch, 0.36f);
        ImGui::TableSetupColumn("b", ImGuiTableColumnFlags_WidthFixed);
      }
    }
    if (!open) continue;
    ImGui::PushID(def.cvar);
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(def.label);
    ImGui::TableSetColumnIndex(1);
    ImGui::AlignTextToFramePadding();
    std::string keys = app.settings.Get(def.cvar);
    if (app.capturing == def.cvar) {
      ImGui::TextColored(kAccentHover, app.capture_adds ? "Press a key to add (Esc: cancel)"
                                                        : "Press a key (Esc: cancel)");
    } else if (keys.empty()) {
      ImGui::TextColored(kDim, "not set");
    } else {
      std::string shown;
      for (const auto& k : SplitKeys(keys)) shown += (shown.empty() ? "" : "  /  ") + k;
      ImGui::TextUnformatted(shown.c_str());
    }
    ImGui::TableSetColumnIndex(2);
    if (ImGui::SmallButton("Set")) {
      app.capturing = def.cvar;
      app.capture_adds = false;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Add")) {
      app.capturing = def.cvar;
      app.capture_adds = true;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Clear")) app.settings.Set(def.cvar, "");
    ImGui::SameLine();
    ImGui::BeginDisabled(keys == app.settings.Shipped(def.cvar));
    if (ImGui::SmallButton("Default")) app.settings.Reset(def.cvar);
    ImGui::EndDisabled();
    ImGui::PopID();
  }
  if (open) ImGui::EndTable();
  if (group) EndCard();
}

// A key press while a binding waits for one (SDL key event, UI thread).
void CaptureKey(App& app, const SDL_KeyboardEvent& key) {
  if (app.capturing.empty() || key.repeat) return;
  std::string cvar = app.capturing;
  app.capturing.clear();
  if (key.key == SDLK_ESCAPE) return;
  std::string name = KeyName(key.key);
  if (name.empty()) return;
  std::string token;
  bool is_modifier = name == "Shift" || name == "Control" || name == "Alt";
  if (!is_modifier) {
    if (key.mod & SDL_KMOD_CTRL) token += "Ctrl+";
    if (key.mod & SDL_KMOD_SHIFT) token += "Shift+";
    if (key.mod & SDL_KMOD_ALT) token += "Alt+";
  }
  token += name;
  std::vector<std::string> keys = app.capture_adds ? SplitKeys(app.settings.Get(cvar))
                                                   : std::vector<std::string>();
  if (std::find(keys.begin(), keys.end(), token) == keys.end()) keys.push_back(token);
  std::string value;
  for (const auto& k : keys) value += (value.empty() ? "" : ",") + k;
  app.settings.Set(cvar, value);
}

void TabAudio(App& app) {
  BeginCard("##language", "LANGUAGE");
  if (BeginRows("##rows")) {
    ComboRow(app, "Game language", "user_language",
             {{"1", "English"}, {"2", "Japanese"}, {"3", "German"}, {"4", "French"},
              {"5", "Spanish"}, {"6", "Italian"}, {"7", "Korean"}, {"8", "Chinese (Traditional)"},
              {"9", "Portuguese"}, {"10", "Chinese (Simplified)"}, {"11", "Polish"},
              {"12", "Russian"}},
             "Falls back to English if the game has no text for it.", true);
    ImGui::EndTable();
  }
  EndCard();
  BeginCard("##sound", "SOUND");
  if (BeginRows("##rows")) {
    CheckboxRow(app, "Mute", "audio_mute");
    ImGui::EndTable();
  }
  EndCard();
}

void TabInstall(App& app) {
  fs::path game = app.GameDir();
  BeginCard("##game", "GAME");
  if (BeginRows("##rows")) {
    Row("Disc 1 (game)");
    TextColor(HasGame(game) ? kGood : kBad, HasGame(game) ? "Installed" : "Not installed");
    Row("Disc 2 (bonus content)");
    TextColor(HasBonus(game) ? kGood : kBad, HasBonus(game) ? "Installed" : "Not installed");
    Row("Files");
    if (!app.Installed()) {
      TextDim("-");
    } else {
      TextColor(app.state.verified ? kGood : kDim, app.state.verified ? "Verified" : "Not verified");
    }
    Row("Folder");
    TextDim(Utf8(game));
    ImGui::EndTable();
  }
  EndCard();

  BeginCard("##import", "IMPORT");
  TextDim("Pick your Forza Motorsport 3 disc images (.iso) one at a time: Disc 1 (the game) and "
          "Disc 2 (the bonus content, needed too). Each is extracted to the game\\ folder next to "
          "the launcher and every file is checked against game_files.sha256. It takes a minute "
          "or two per disc.");
  ImGui::Spacing();
  ImGui::BeginDisabled(app.task.Busy() || app.game_running);
  if (PrimaryButton("Choose ISO and import", ImVec2(S(260), S(40)))) PickIso(app);
  ImGui::Spacing();
  TextDim("Already extracted? Point the launcher at the folder holding default.xex (with its "
          "Disc 2 folder inside).");
  if (ImGui::Button("Change folder", ImVec2(S(170), 0))) PickFolder(app);
  ImGui::SameLine();
  ImGui::BeginDisabled(!HasGame(game));
  if (ImGui::Button("Verify files", ImVec2(S(170), 0))) StartVerify(app);
  ImGui::EndDisabled();
  ImGui::EndDisabled();

  if (app.task.Busy()) {
    ImGui::Spacing();
    TextDim(app.task.Status());
    if (ImGui::Button("Cancel")) app.task.cancel = true;
  } else {
    std::lock_guard lock(app.result_mutex);
    if (!app.task_result.empty()) {
      ImGui::Spacing();
      TextColor(app.task_result_ok ? kGood : kBad, app.task_result);
    }
  }
  EndCard();
}

void TabBar(App& app) {
  static const std::pair<Tab, const char*> kTabs[] = {{Tab::kProfiles, "Profiles"},
                                                      {Tab::kGraphics, "Graphics"},
                                                      {Tab::kControls, "Controls"},
                                                      {Tab::kAudio, "Language & sound"},
                                                      {Tab::kInstall, "Install"}};
  ImDrawList* dl = ImGui::GetWindowDrawList();
  for (const auto& [tab, label] : kTabs) {
    bool active = app.tab == tab;
    ImGui::PushStyleColor(ImGuiCol_Text, active ? kText : kDim);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.05f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1, 1, 1, 0.08f));
    if (ImGui::Button(label)) {
      app.tab = tab;
      app.capturing.clear();
    }
    ImGui::PopStyleColor(4);
    if (active) {
      ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
      dl->AddRectFilled(ImVec2(a.x + S(8), b.y - S(3)), ImVec2(b.x - S(8), b.y),
                        ImGui::GetColorU32(kAccent));
    }
    ImGui::SameLine(0, S(6));
  }
  ImGui::NewLine();
  ImGui::Separator();
  ImGui::Spacing();
}

void DrawUi(App& app) {
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->WorkPos);
  ImGui::SetNextWindowSize(viewport->WorkSize);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::Begin("##main", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings);
  ImGui::PopStyleVar();
  DrawLeftColumn(app);
  ImGui::SameLine(0, 0);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(S(26), S(18)));
  ImGui::BeginChild("##right", ImVec2(0, 0),
                    ImGuiChildFlags_AlwaysUseWindowPadding | ImGuiChildFlags_NavFlattened);
  TabBar(app);
  ImGui::BeginChild("##tab", ImVec2(0, 0), ImGuiChildFlags_NavFlattened);
  switch (app.tab) {
    case Tab::kProfiles: TabProfiles(app); break;
    case Tab::kGraphics: TabGraphics(app); break;
    case Tab::kControls: TabControls(app); break;
    case Tab::kAudio: TabAudio(app); break;
    case Tab::kInstall: TabInstall(app); break;
  }
  if (!app.game_error.empty()) {
    ImGui::Spacing();
    TextColor(kBad, app.game_error);
    if (ImGui::Button("Open logs folder")) {
      ShellExecuteW(nullptr, L"open", (ExeDir() / "logs").c_str(), nullptr, nullptr, SW_SHOW);
    }
  }
  ImGui::EndChild();
  ImGui::EndChild();
  ImGui::PopStyleVar();
  ImGui::End();
}

void SetupStyle(App& app) {
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
  ImFont* body = nullptr;
  for (const char* path : {"C:/Windows/Fonts/bahnschrift.ttf", "C:/Windows/Fonts/segoeui.ttf"}) {
    std::error_code ec;
    if (fs::exists(path, ec)) {
      body = io.Fonts->AddFontFromFileTTF(path, S(18));
      app.title_font = io.Fonts->AddFontFromFileTTF(path, S(36));
      break;
    }
  }
  if (!body) io.Fonts->AddFontDefault();

  ImGuiStyle& style = ImGui::GetStyle();
  ImGui::StyleColorsDark(&style);
  style.FramePadding = ImVec2(12, 7);
  style.ItemSpacing = ImVec2(10, 9);
  style.FrameRounding = 5;
  style.PopupRounding = 6;
  style.GrabRounding = 5;
  style.WindowBorderSize = 0;
  style.ChildBorderSize = 0;
  style.ScaleAllSizes(g_scale);
  ImVec4* c = style.Colors;
  c[ImGuiCol_Text] = kText;
  c[ImGuiCol_TextDisabled] = kDim;
  c[ImGuiCol_WindowBg] = kBg;
  c[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
  c[ImGuiCol_PopupBg] = ImVec4(0.12f, 0.125f, 0.14f, 0.98f);
  c[ImGuiCol_ModalWindowDimBg] = ImVec4(0, 0, 0, 0.6f);
  c[ImGuiCol_FrameBg] = kControl;
  c[ImGuiCol_FrameBgHovered] = ImVec4(0.21f, 0.22f, 0.25f, 1);
  c[ImGuiCol_FrameBgActive] = ImVec4(0.24f, 0.25f, 0.28f, 1);
  c[ImGuiCol_Button] = kControl;
  c[ImGuiCol_ButtonHovered] = ImVec4(0.22f, 0.23f, 0.26f, 1);
  c[ImGuiCol_ButtonActive] = kAccent;
  c[ImGuiCol_Header] = ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.30f);
  c[ImGuiCol_HeaderHovered] = ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.45f);
  c[ImGuiCol_HeaderActive] = kAccent;
  c[ImGuiCol_CheckMark] = kAccent;
  c[ImGuiCol_SliderGrab] = kAccent;
  c[ImGuiCol_SliderGrabActive] = kAccentHover;
  c[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
  c[ImGuiCol_ScrollbarGrab] = ImVec4(1, 1, 1, 0.12f);
  c[ImGuiCol_Separator] = ImVec4(1, 1, 1, 0.07f);
  c[ImGuiCol_NavCursor] = kAccentHover;
}

// cover.png next to the exe, uploaded as an SDL GPU texture.
void LoadCover(App& app) {
  fs::path file = ExeDir() / "cover.png";
  std::error_code ec;
  if (!fs::exists(file, ec)) return;
  SDL_Surface* loaded = SDL_LoadPNG(Utf8(file).c_str());
  if (!loaded) return;
  SDL_Surface* rgba = SDL_ConvertSurface(loaded, SDL_PIXELFORMAT_ABGR8888);
  SDL_DestroySurface(loaded);
  if (!rgba) return;
  SDL_GPUTextureCreateInfo info = {};
  info.type = SDL_GPU_TEXTURETYPE_2D;
  info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
  info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
  info.width = Uint32(rgba->w);
  info.height = Uint32(rgba->h);
  info.layer_count_or_depth = 1;
  info.num_levels = 1;
  SDL_GPUTexture* texture = SDL_CreateGPUTexture(app.gpu, &info);
  SDL_GPUTransferBufferCreateInfo transfer_info = {};
  transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  transfer_info.size = Uint32(rgba->w * rgba->h * 4);
  SDL_GPUTransferBuffer* transfer = SDL_CreateGPUTransferBuffer(app.gpu, &transfer_info);
  if (texture && transfer) {
    auto* dst = static_cast<uint8_t*>(SDL_MapGPUTransferBuffer(app.gpu, transfer, false));
    for (int y = 0; y < rgba->h; ++y) {
      std::memcpy(dst + size_t(y) * rgba->w * 4,
                  static_cast<uint8_t*>(rgba->pixels) + size_t(y) * rgba->pitch, size_t(rgba->w) * 4);
    }
    SDL_UnmapGPUTransferBuffer(app.gpu, transfer);
    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(app.gpu);
    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cmd);
    SDL_GPUTextureTransferInfo src = {};
    src.transfer_buffer = transfer;
    SDL_GPUTextureRegion region = {};
    region.texture = texture;
    region.w = info.width;
    region.h = info.height;
    region.d = 1;
    SDL_UploadToGPUTexture(copy, &src, &region, false);
    SDL_EndGPUCopyPass(copy);
    SDL_SubmitGPUCommandBuffer(cmd);
    app.cover = {texture, rgba->w, rgba->h};
  }
  if (transfer) SDL_ReleaseGPUTransferBuffer(app.gpu, transfer);
  SDL_DestroySurface(rgba);
}

}  // namespace
}  // namespace launcher

// Forza3Launcher --extract-iso <image.iso> <game folder>: the import without
// the window, for tools/make_manifest.py and tests. Exit code 0 on success.
int ExtractCommand(const char* iso, const char* game_dir) {
  using namespace launcher;
  FILE* stream = nullptr;
  if (AttachConsole(ATTACH_PARENT_PROCESS)) {
    freopen_s(&stream, "CONOUT$", "w", stdout);
    freopen_s(&stream, "CONOUT$", "w", stderr);
  }
  TaskProgress progress;
  std::string error;
  DiscKind kind = ExtractIso(FromUtf8(iso), FromUtf8(game_dir), progress, error);
  if (kind == DiscKind::kFailed) {
    std::fprintf(stderr, "extract failed: %s\n", error.c_str());
    return 1;
  }
  std::printf("extracted %s (%.2f GB)\n", kind == DiscKind::kGame ? "Disc 1" : "Disc 2",
              progress.done.load() / 1e9);
  return 0;
}

int main(int argc, char** argv) {
  using namespace launcher;
  if (argc == 4 && std::strcmp(argv[1], "--extract-iso") == 0) {
    return ExtractCommand(argv[2], argv[3]);
  }
  auto fail = [](const char* what) {
    std::string message = std::string(what) + ": " + SDL_GetError();
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Forza Motorsport 3", message.c_str(), nullptr);
    return 1;
  };
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMEPAD)) return fail("SDL_Init");
  g_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
  if (g_scale <= 0) g_scale = 1.0f;

  App app;
  app.window = SDL_CreateWindow("Forza Motorsport 3", int(S(1180)), int(S(800)),
                                SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_RESIZABLE);
  if (!app.window) return fail("Could not create the window");
  SDL_SetWindowMinimumSize(app.window, int(S(980)), int(S(680)));
  app.gpu = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL |
                                    SDL_GPU_SHADERFORMAT_METALLIB,
                                false, nullptr);
  if (!app.gpu) return fail("Could not start the GPU");
  if (!SDL_ClaimWindowForGPUDevice(app.gpu, app.window)) return fail("Could not use the window");
  SDL_SetGPUSwapchainParameters(app.gpu, app.window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
                                SDL_GPU_PRESENTMODE_VSYNC);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  SetupStyle(app);
  ImGui_ImplSDL3_InitForSDLGPU(app.window);
  ImGui_ImplSDLGPU3_InitInfo init = {};
  init.Device = app.gpu;
  init.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(app.gpu, app.window);
  init.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
  ImGui_ImplSDLGPU3_Init(&init);
  LoadCover(app);

  app.state.Load();
  app.settings.Load(ExeDir());
  app.manifest = LoadManifest(ExeDir() / "game_files.sha256");
  app.profiles.MigrateLegacy("Player");
  app.RefreshProfiles();
  if (!app.Installed()) app.tab = Tab::kInstall;
  else if (app.profile_names.empty()) app.tab = Tab::kProfiles;

  bool running = true;
  while (running) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_EVENT_KEY_DOWN && !app.capturing.empty()) {
        CaptureKey(app, event.key);
        continue;  // Not a key for the UI.
      }
      ImGui_ImplSDL3_ProcessEvent(&event);
      if (event.type == SDL_EVENT_QUIT) running = false;
    }
    HandleDialogs(app);
    HandleGameExit(app);
    if (app.settings.dirty()) {
      app.settings.Save();  // Saved automatically.
      app.settings.ClearDirty();
    }
    if (SDL_GetWindowFlags(app.window) & SDL_WINDOW_MINIMIZED) {
      SDL_Delay(50);
      continue;
    }

    ImGui_ImplSDLGPU3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    DrawUi(app);
    ImGui::Render();
    ImDrawData* draw_data = ImGui::GetDrawData();

    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(app.gpu);
    SDL_GPUTexture* swapchain = nullptr;
    SDL_WaitAndAcquireGPUSwapchainTexture(cmd, app.window, &swapchain, nullptr, nullptr);
    if (swapchain && draw_data->DisplaySize.x > 0 && draw_data->DisplaySize.y > 0) {
      ImGui_ImplSDLGPU3_PrepareDrawData(draw_data, cmd);
      SDL_GPUColorTargetInfo target = {};
      target.texture = swapchain;
      target.clear_color = SDL_FColor{kBg.x, kBg.y, kBg.z, 1};
      target.load_op = SDL_GPU_LOADOP_CLEAR;
      target.store_op = SDL_GPU_STOREOP_STORE;
      SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(cmd, &target, 1, nullptr);
      ImGui_ImplSDLGPU3_RenderDrawData(draw_data, cmd, pass);
      SDL_EndGPURenderPass(pass);
    }
    SDL_SubmitGPUCommandBuffer(cmd);
  }

  app.task.cancel = true;
  if (app.worker.joinable()) app.worker.join();
  if (app.game_waiter.joinable()) app.game_waiter.detach();
  SDL_WaitForGPUIdle(app.gpu);
  ImGui_ImplSDL3_Shutdown();
  ImGui_ImplSDLGPU3_Shutdown();
  ImGui::DestroyContext();
  if (app.cover.texture) SDL_ReleaseGPUTexture(app.gpu, app.cover.texture);
  SDL_ReleaseWindowFromGPUDevice(app.gpu, app.window);
  SDL_DestroyGPUDevice(app.gpu);
  SDL_DestroyWindow(app.window);
  SDL_Quit();
  return 0;
}
