// Forza3Launcher - game file verification against game_files.sha256.

#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "task.h"

namespace launcher {

struct ManifestEntry {
  std::string path;    // '/'-separated, relative to the game folder.
  uint64_t size = 0;
  std::string sha256;  // Lower-case hex.
  bool bonus = false;  // Under "Disc 2/": a file of the bonus content disc.
};

// Parses `<sha256> <size> <path>` lines ('#' starts a comment). The list is
// made from the files of known-good disc images (tools/make_manifest.py).
std::vector<ManifestEntry> LoadManifest(const std::filesystem::path& file);

struct VerifyReport {
  std::vector<std::string> missing;
  std::vector<std::string> wrong_size;
  std::vector<std::string> wrong_hash;
  size_t checked = 0;
  bool ok() const { return missing.empty() && wrong_size.empty() && wrong_hash.empty(); }
};

// Checks every file in `manifest` (callers pick the discs). Returns false
// when cancelled. Progress counts bytes.
bool VerifyGame(const std::filesystem::path& game_dir, const std::vector<ManifestEntry>& manifest,
                TaskProgress& progress, VerifyReport& report);

// SHA-256 of a file as lower-case hex, or empty on error / cancel.
std::string Sha256File(const std::filesystem::path& file, TaskProgress* progress);

}  // namespace launcher
