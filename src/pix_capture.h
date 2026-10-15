// forza3 - opt-in PIX GPU capture support

#pragma once

namespace forza3 {

// Loads PIX's GPU capturer when --pix_gpu_capturer is set. Must run before the
// D3D12 device is created (OnPreSetup).
void LoadPixGpuCapturerIfRequested();

// If the capturer is loaded, binds F9 to capture the next frame to
// <exe dir>/pixcaptures/forza3_<time>.wpix.
void RegisterPixCaptureHotkey();

}  // namespace forza3
