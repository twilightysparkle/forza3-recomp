// Forza3Launcher - Xbox 360 disc image (XDVDFS) extraction.

#pragma once

#include <filesystem>
#include <string>

#include "task.h"

namespace launcher {

enum class DiscKind {
  kFailed,  // Not an Xbox disc image, unreadable, or cancelled.
  kGame,    // Disc 1: default.xex and the game data.
  kBonus,   // Disc 2: the bonus content packages (a Content folder).
};

// Extracts an Xbox 360 disc image into a folder. Reads the XDVDFS file system
// in any common layout (XGD1/2/3 redump images, or trimmed XISO). Disc 1 goes
// whole into `game_dir`; Disc 2 (identified by its Content folder) into
// `game_dir`/Disc 2, which the game installs from on its next start. The
// $SystemUpdate folder is skipped. Runs on the caller's thread and reports
// through `progress` (bytes); `error` says why on kFailed.
DiscKind ExtractIso(const std::filesystem::path& iso, const std::filesystem::path& game_dir,
                    TaskProgress& progress, std::string& error);

}  // namespace launcher
