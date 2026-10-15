// forza3 - default game and user data locations

#include "default_paths.h"

#include <rex/cvar.h>
#include <rex/filesystem.h>
#include <rex/logging.h>

#include <system_error>

namespace forza3 {

namespace {

// "game" (a release: the player extracts the disc next to the exe) or
// "assets" (the repository), in the exe folder or up to five parents (covers
// out\build\<preset>\).
std::filesystem::path FindAssets() {
  std::error_code ec;
  std::filesystem::path dir = rex::filesystem::GetExecutableFolder();
  for (int i = 0; i < 6 && !dir.empty(); ++i) {
    for (const char* name : {"game", "assets"}) {
      if (std::filesystem::is_regular_file(dir / name / "default.xex", ec)) {
        return dir / name;
      }
    }
    std::filesystem::path parent = dir.parent_path();
    if (parent == dir) break;
    dir = parent;
  }
  return {};
}

}  // namespace

void ApplyDefaultPaths(rex::PathConfig& paths) {
  if (paths.game_data_root.empty()) {
    paths.game_data_root = FindAssets();
    if (paths.game_data_root.empty()) {
      REXLOG_ERROR("No game\\default.xex or assets\\default.xex found next to forza3.exe");
      return;  // The SDK reports the missing --game_data_root.
    }
    REXLOG_INFO("Game data: {}", paths.game_data_root.string());
  }

  if (rex::cvar::GetFlagByName("user_data_root").empty()) {
    std::filesystem::path old_user = paths.user_data_root;
    paths.user_data_root = paths.game_data_root.parent_path() / "userdata";
    // The SDK derives the cache from the user folder unless it was given.
    if (rex::cvar::GetFlagByName("cache_root").empty() && paths.cache_root == old_user / "cache") {
      paths.cache_root = paths.user_data_root / "cache";
    }
    REXLOG_INFO("User data: {}", paths.user_data_root.string());
  }
}

}  // namespace forza3
