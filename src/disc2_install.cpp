// forza3 - Disc 2 content install
//
// FM3's Disc 2 holds no code the port needs: it is 8 marketplace-style content
// packages (STFS, type 00000002) plus an installer XEX that copies them to the
// HDD. The game's own "Install Disc 2" flow needs a tray swap, so instead the
// packages are installed directly through the SDK's content manager, the way
// hl360-Forza2 installs its bonus-disc DLC.

#include "disc2_install.h"

#include <rex/logging.h>
#include <rex/runtime.h>
#include <rex/system/kernel_state.h>
#include <rex/system/xam/content_manager.h>

#include <algorithm>
#include <chrono>
#include <system_error>
#include <vector>

namespace forza3 {

namespace {

constexpr const char* kTitleId = "4D53084D";
constexpr const char* kMarketplaceType = "00000002";

}  // namespace

void InstallDisc2Content(rex::Runtime* runtime, const std::filesystem::path& game_root,
                         const std::filesystem::path& user_root) {
  namespace fs = std::filesystem;
  std::error_code ec;

  // Disc 2 extracted next to disc 1: <game_root>/Disc 2/Content/0000000000000000/4D53084D/00000002
  auto source = game_root / "Disc 2" / "Content" / "0000000000000000" / kTitleId / kMarketplaceType;
  if (!fs::is_directory(source, ec)) {
    REXLOG_INFO("Disc 2: no packages at {}; skipping install", source.string());
    return;
  }

  auto* content = runtime && runtime->kernel_state() ? runtime->kernel_state()->content_manager()
                                                     : nullptr;
  if (!content) {
    REXLOG_WARN("Disc 2: content manager unavailable; skipping install");
    return;
  }

  // InstallContent writes the .header last, so its presence marks a finished
  // install; a partial folder without one is retried on the next launch.
  auto headers = user_root / "0000000000000000" / kTitleId / "Headers" / kMarketplaceType;

  std::vector<fs::path> packages;
  for (const auto& entry : fs::directory_iterator(source, ec)) {
    if (entry.is_regular_file()) {
      packages.push_back(entry.path());
    }
  }
  std::sort(packages.begin(), packages.end());

  size_t installed = 0, skipped = 0, failed = 0;
  for (const auto& package : packages) {
    auto name = package.filename().string();
    if (fs::exists(headers / (name + ".header"), ec)) {
      ++skipped;
      continue;
    }
    REXLOG_INFO("Disc 2: installing {} ({} MB)...", name, fs::file_size(package, ec) >> 20);
    auto start = std::chrono::steady_clock::now();
    auto result = content->InstallContent(package);
    auto secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    if (result == 0) {
      REXLOG_INFO("Disc 2: installed {} in {:.1f}s", name, secs);
      ++installed;
    } else {
      REXLOG_WARN("Disc 2: install of {} failed: {:08X}", name, result);
      ++failed;
    }
  }
  REXLOG_INFO("Disc 2: {} installed, {} already present, {} failed (of {})", installed, skipped,
              failed, packages.size());
}

}  // namespace forza3
