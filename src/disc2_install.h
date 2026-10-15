// forza3 - Disc 2 content install

#pragma once

#include <filesystem>

namespace rex {
class Runtime;
}

namespace forza3 {

// Installs the Disc 2 content packages from <game_root>/Disc 2 into the
// runtime's content folder (<user_root>/0000000000000000/4D53084D/00000002).
// Already-installed packages are skipped, so this is cheap after the first run.
void InstallDisc2Content(rex::Runtime* runtime, const std::filesystem::path& game_root,
                         const std::filesystem::path& user_root);

}  // namespace forza3
