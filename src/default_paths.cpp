// forza3 - default game and user data locations

#include "default_paths.h"

#include <rex/cvar.h>
#include <rex/filesystem.h>
#include <rex/logging.h>

#include <system_error>

namespace forza3 {

namespace {

// The exe folder and up to five parents (covers out\build\<preset>\).
std::filesystem::path FindAssets() {
  std::error_code ec;
  std::filesystem::path dir = rex::filesystem::GetExecutableFolder();
  for (int i = 0; i < 6 && !dir.empty(); ++i) {
    if (std::filesystem::is_regular_file(dir / "assets" / "default.xex", ec)) {
      return dir / "assets";
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
      REXLOG_ERROR("No assets\\default.xex found next to forza3.exe or in a parent folder");
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
