// forza3 - Xbox 360 utility drive (cache:)
//
// FM3 calls XMountUtilityDrive (statically linked XAPI) and then streams its
// replay to cache:\replay_stream. The SDK stubs the kernel calls that mount
// code makes, but registers no device behind "cache:", so the open fails and
// the game dereferences the null stream (sub_828678F0) when a race loads.
// Back the utility drive with a writable host folder, as Xenia does.

#include "utility_drive.h"

#include <rex/filesystem/devices/host_path_device.h>
#include <rex/filesystem/vfs.h>
#include <rex/logging.h>
#include <rex/runtime.h>

#include <memory>
#include <system_error>

namespace forza3 {

void MountUtilityDrive(rex::Runtime* runtime, const std::filesystem::path& user_root) {
  auto* vfs = runtime ? runtime->file_system() : nullptr;
  if (!vfs) {
    REXLOG_WARN("Utility drive: no file system; cache: not mounted");
    return;
  }

  // Not <user_root>/cache: the SDK keeps its shader cache there.
  auto host_path = user_root / "utility_drive";
  std::error_code ec;
  std::filesystem::create_directories(host_path, ec);

  constexpr const char* kMountPath = "\\CACHE";
  auto device = std::make_unique<rex::filesystem::HostPathDevice>(kMountPath, host_path, false);
  if (!device->Initialize() || !vfs->RegisterDevice(std::move(device))) {
    REXLOG_WARN("Utility drive: failed to mount {}", host_path.string());
    return;
  }
  // Retail titles use cache:, some older XDK code cache0:/cache1:.
  for (const char* link : {"cache:", "cache0:", "cache1:"}) {
    vfs->RegisterSymbolicLink(link, kMountPath);
  }
  REXLOG_INFO("Utility drive: cache: -> {}", host_path.string());
}

}  // namespace forza3
