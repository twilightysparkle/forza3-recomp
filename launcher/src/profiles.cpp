// Forza3Launcher - local profiles, each with its own save data.

#include "profiles.h"

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <ctime>
#include <system_error>

namespace launcher {

namespace {

constexpr const char* kSharedContent = "0000000000000000";

// Directory junctions need no administrator rights, unlike symbolic links.
bool CreateJunction(const std::filesystem::path& link, const std::filesystem::path& target) {
  std::wstring command = L"cmd.exe /c mklink /J \"" + link.wstring() + L"\" \"" +
                         target.wstring() + L"\"";
  STARTUPINFOW startup = {sizeof(startup)};
  startup.dwFlags = STARTF_USESHOWWINDOW;
  startup.wShowWindow = SW_HIDE;
  PROCESS_INFORMATION process = {};
  if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr,
                      nullptr, &startup, &process)) {
    return false;
  }
  WaitForSingleObject(process.hProcess, 10000);
  DWORD code = 1;
  GetExitCodeProcess(process.hProcess, &code);
  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);
  return code == 0;
}

void MoveIfPresent(const std::filesystem::path& from, const std::filesystem::path& to) {
  std::error_code ec;
  if (std::filesystem::exists(from, ec) && !std::filesystem::exists(to, ec)) {
    std::filesystem::create_directories(to.parent_path(), ec);
    std::filesystem::rename(from, to, ec);
  }
}

}  // namespace

bool IsValidProfileName(const std::string& name) {
  if (name.empty() || name.size() > 15 || name.front() == ' ' || name.back() == ' ') return false;
  for (size_t i = 0; i < name.size(); ++i) {
    unsigned char c = static_cast<unsigned char>(name[i]);
    if (c == ' ') {
      if (name[i - 1] == ' ') return false;
    } else if (!std::isalnum(c) || c > 127) {
      return false;
    }
  }
  return true;
}

Profiles::Profiles(std::filesystem::path userdata) : userdata_(std::move(userdata)) {}

std::filesystem::path Profiles::ProfileDir(const std::string& name) const {
  return userdata_ / "profiles" / name;
}

std::vector<std::string> Profiles::List() const {
  std::vector<std::string> names;
  std::error_code ec;
  for (const auto& entry : std::filesystem::directory_iterator(userdata_ / "profiles", ec)) {
    if (entry.is_directory(ec)) {
      std::string name = entry.path().filename().string();
      if (IsValidProfileName(name)) names.push_back(name);
    }
  }
  std::sort(names.begin(), names.end(), [](const std::string& a, const std::string& b) {
    return _stricmp(a.c_str(), b.c_str()) < 0;
  });
  return names;
}

bool Profiles::LinkSharedContent(const std::string& name, std::string& error) const {
  std::error_code ec;
  std::filesystem::path shared = userdata_ / "shared" / kSharedContent;
  std::filesystem::create_directories(shared, ec);
  std::filesystem::path link = ProfileDir(name) / kSharedContent;
  if (std::filesystem::exists(link, ec)) return true;
  if (!CreateJunction(link, shared)) {
    error = "Could not link the shared Disc 2 content into the profile.";
    return false;
  }
  return true;
}

bool Profiles::Create(const std::string& name, std::string& error) {
  if (!IsValidProfileName(name)) {
    error = "Use 1-15 letters, digits and single spaces.";
    return false;
  }
  for (const auto& existing : List()) {
    if (_stricmp(existing.c_str(), name.c_str()) == 0) {
      error = "A profile with that name already exists.";
      return false;
    }
  }
  std::error_code ec;
  std::filesystem::create_directories(ProfileDir(name), ec);
  if (ec) {
    error = "Could not create the profile folder: " + ec.message();
    return false;
  }
  return LinkSharedContent(name, error);
}

bool Profiles::Delete(const std::string& name, std::string& error) {
  std::error_code ec;
  std::filesystem::path from = ProfileDir(name);
  if (!std::filesystem::exists(from, ec)) return true;
  // Kept, not erased: a deleted profile's saves can be restored by hand.
  char stamp[32];
  std::time_t now = std::time(nullptr);
  std::tm local = {};
  localtime_s(&local, &now);
  std::strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", &local);
  std::filesystem::path to = userdata_ / "deleted-profiles" / (name + "-" + stamp);
  std::filesystem::create_directories(to.parent_path(), ec);
  std::filesystem::rename(from, to, ec);
  if (ec) {
    error = "Could not delete the profile (is the game running?): " + ec.message();
    return false;
  }
  return true;
}

void Profiles::MigrateLegacy(const std::string& name) {
  std::error_code ec;
  if (!List().empty()) return;
  bool legacy = false;
  for (const auto& entry : std::filesystem::directory_iterator(userdata_, ec)) {
    std::string n = entry.path().filename().string();
    // Profile save folders are 16 hex digits (the XUID), not all zeros.
    if (n.size() == 16 && n != kSharedContent &&
        std::all_of(n.begin(), n.end(), [](char c) { return std::isxdigit(uint8_t(c)); })) {
      legacy = true;
    }
  }
  if (!legacy) return;
  std::filesystem::path profile = ProfileDir(name);
  std::filesystem::create_directories(profile, ec);
  std::vector<std::filesystem::path> items;  // Listed first: moving while iterating.
  for (const auto& entry : std::filesystem::directory_iterator(userdata_, ec)) {
    items.push_back(entry.path());
  }
  for (const auto& path : items) {
    std::string n = path.filename().string();
    if (n == "profiles" || n == "shared" || n == "cache" || n == "deleted-profiles") continue;
    if (n == kSharedContent) {
      MoveIfPresent(path, userdata_ / "shared" / kSharedContent);
    } else {
      MoveIfPresent(path, profile / n);
    }
  }
  std::string error;
  LinkSharedContent(name, error);
}

}  // namespace launcher
