// forza3 - crash minidumps

#pragma once

namespace forza3 {

// Writes <exe dir>/crashdumps/forza3_<time>.dmp on an unhandled host exception
// (including guest access violations the SDK can't service). Windows only.
void InstallCrashDumpHandler();

}  // namespace forza3
