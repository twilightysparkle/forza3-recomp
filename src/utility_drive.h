// forza3 - Xbox 360 utility drive (cache:)

#pragma once

#include <filesystem>

namespace rex {
class Runtime;
}

namespace forza3 {

// Mounts a writable host folder (<user_root>/utility_drive) as the title's
// utility drive and links cache:, cache0: and cache1: to it.
void MountUtilityDrive(rex::Runtime* runtime, const std::filesystem::path& user_root);

}  // namespace forza3
