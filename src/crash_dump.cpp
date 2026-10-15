// forza3 - crash minidumps
//
// Guest faults the SDK can't service (e.g. a null-pointer read in recompiled
// code) are logged only as "Unhandled guest access violation" with the data
// address, then the process dies. A minidump plus forza3.pdb gives the host
// stack, whose sub_XXXXXXXX frames name the guest functions involved:
//   lldb forza3.exe -c crashdumps\forza3_<time>.dmp -o "thread backtrace all"

#include "crash_dump.h"

#ifdef _WIN32

#include <windows.h>
// dbghelp.h needs windows.h first.
#include <dbghelp.h>

#include <rex/logging.h>

#include <atomic>
#include <cstdio>
#include <filesystem>
#include <string>

#pragma comment(lib, "dbghelp.lib")

namespace forza3 {

namespace {

LPTOP_LEVEL_EXCEPTION_FILTER g_previous_filter = nullptr;
std::atomic<bool> g_dumping{false};

std::filesystem::path DumpDirectory() {
  wchar_t exe[MAX_PATH];
  DWORD n = GetModuleFileNameW(nullptr, exe, MAX_PATH);
  if (n == 0 || n == MAX_PATH) {
    return std::filesystem::current_path() / "crashdumps";
  }
  return std::filesystem::path(exe).parent_path() / "crashdumps";
}

LONG WINAPI WriteCrashDump(EXCEPTION_POINTERS* info) {
  // One dump per process: a fault while dumping must not recurse.
  if (!g_dumping.exchange(true)) {
    std::error_code ec;
    auto dir = DumpDirectory();
    std::filesystem::create_directories(dir, ec);

    SYSTEMTIME t;
    GetLocalTime(&t);
    wchar_t name[64];
    swprintf_s(name, L"forza3_%04u%02u%02u_%02u%02u%02u.dmp", t.wYear, t.wMonth, t.wDay, t.wHour,
               t.wMinute, t.wSecond);
    auto path = dir / name;

    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE) {
      MINIDUMP_EXCEPTION_INFORMATION mei{GetCurrentThreadId(), info, FALSE};
      // Thread stacks and the memory they point at, without the 4 GB guest arena.
      auto type = MINIDUMP_TYPE(MiniDumpWithThreadInfo | MiniDumpWithIndirectlyReferencedMemory |
                                MiniDumpWithUnloadedModules);
      BOOL ok = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file, type, &mei,
                                  nullptr, nullptr);
      CloseHandle(file);
      REXLOG_ERROR("Unhandled exception {:08X} at {}; minidump {} {}",
                   info->ExceptionRecord->ExceptionCode, info->ExceptionRecord->ExceptionAddress,
                   ok ? "written to" : "FAILED for", path.string());
    }
  }
  return g_previous_filter ? g_previous_filter(info) : EXCEPTION_CONTINUE_SEARCH;
}

}  // namespace

void InstallCrashDumpHandler() {
  g_previous_filter = SetUnhandledExceptionFilter(WriteCrashDump);
}

}  // namespace forza3

#else

namespace forza3 {
void InstallCrashDumpHandler() {}
}  // namespace forza3

#endif
