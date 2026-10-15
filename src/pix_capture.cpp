// forza3 - opt-in PIX GPU capture support
//
// With --pix_gpu_capturer, loads PIX's WinPixGpuCapturer.dll before the GPU
// plugin creates its D3D12 device, so a running game can be captured with
//   pixtool attach <pid> take-capture --open save-capture frame.wpix
// Off by default: the capturer adds overhead and is only for debugging.

#include "pix_capture.h"

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/ui/keybinds.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <system_error>

REXCVAR_DEFINE_BOOL(pix_gpu_capturer, false, "Debug",
                    "Load PIX's WinPixGpuCapturer.dll at startup so pixtool can attach");
// Host presents don't line up with guest frames (the GPU thread submits in
// batches), so one host frame holds only part of a game frame.
REXCVAR_DEFINE_INT32(pix_capture_frames, 4, "Debug", "Host frames per PIX capture (F9)");

namespace forza3 {

void LoadPixGpuCapturerIfRequested() {
#ifdef _WIN32
  if (!REXCVAR_GET(pix_gpu_capturer)) {
    return;
  }
  if (GetModuleHandleW(L"WinPixGpuCapturer.dll")) {
    return;  // Already injected (launched from PIX).
  }
  // Newest PIX install: C:\Program Files\Microsoft PIX\<version>\WinPixGpuCapturer.dll
  std::filesystem::path newest;
  std::error_code ec;
  for (const auto& entry :
       std::filesystem::directory_iterator(L"C:\\Program Files\\Microsoft PIX", ec)) {
    auto dll = entry.path() / L"WinPixGpuCapturer.dll";
    if (entry.is_directory() && std::filesystem::exists(dll, ec) &&
        (newest.empty() || entry.path().filename() > newest.parent_path().filename())) {
      newest = dll;
    }
  }
  if (newest.empty()) {
    REXLOG_WARN("PIX: WinPixGpuCapturer.dll not found under C:\\Program Files\\Microsoft PIX");
    return;
  }
  if (LoadLibraryW(newest.c_str())) {
    REXLOG_INFO("PIX: loaded {}", newest.string());
  } else {
    REXLOG_WARN("PIX: failed to load {} ({})", newest.string(), GetLastError());
  }
#endif
}

void RegisterPixCaptureHotkey() {
#ifdef _WIN32
  if (!GetModuleHandleW(L"WinPixGpuCapturer.dll")) {
    return;
  }
  // pixtool can't attach to a process it didn't launch, so captures are
  // programmatic: F9 captures the next frame to <exe dir>/pixcaptures/.
  rex::ui::RegisterBind("bind_pix_capture", "F9", "Take a PIX GPU capture of the next frame", [] {
    // The bind repeats while the key is held; one capture per press, and none
    // while a capture is still being written (that takes tens of seconds).
    static ULONGLONG last_ms = 0;
    ULONGLONG now_ms = GetTickCount64();
    if (last_ms && now_ms - last_ms < 60000) {
      return;
    }
    last_ms = now_ms;
    // pix3_win.h's PIXGpuCaptureNextFrames wraps this export.
    using CaptureFn = HRESULT(WINAPI*)(PCWSTR, UINT32);
    auto capture = reinterpret_cast<CaptureFn>(
        GetProcAddress(GetModuleHandleW(L"WinPixGpuCapturer.dll"), "CaptureNextFrame"));
    if (!capture) {
      REXLOG_WARN("PIX: CaptureNextFrame not exported by the capturer");
      return;
    }
    wchar_t exe[MAX_PATH];
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    auto dir = std::filesystem::path(exe).parent_path() / L"pixcaptures";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    SYSTEMTIME t;
    GetLocalTime(&t);
    wchar_t name[64];
    swprintf_s(name, L"forza3_%02u%02u%02u.wpix", t.wHour, t.wMinute, t.wSecond);
    auto path = dir / name;
    HRESULT hr = capture(path.c_str(), uint32_t(std::max(1, REXCVAR_GET(pix_capture_frames))));
    REXLOG_INFO("PIX: capture of next frame to {} -> {:08X}", path.string(), uint32_t(hr));
  });
#endif
}

}  // namespace forza3
