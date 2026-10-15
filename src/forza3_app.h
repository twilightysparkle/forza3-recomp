// forza3 - ReXGlue Recompiled Project
//
// Customize your app by overriding virtual hooks from rex::ReXApp.

#pragma once

#include <rex/input/input_system.h>
#include <rex/rex_app.h>
#include <rex/system/gpu_plugin.h>
#include <rex/ui/imgui_drawer.h>

#include "crash_dump.h"
#include "default_paths.h"
#include "disc2_install.h"
#include "pix_capture.h"
#include "settings_menu.h"
#include "utility_drive.h"

class Forza3App : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext& ctx) {
    forza3::WaitForRestartedInstance();
    return std::unique_ptr<Forza3App>(new Forza3App(ctx, "forza3",
        PPCImageConfig));
  }

  // The SDK's gpu_plugin cvar defaults to empty (no GPU emulation), which
  // leaves the game submitting to a ring buffer nobody reads. Default to the
  // Xenos plugin staged by rexglue_setup_target(... GPU_PLUGINS xenos).
  void OnPreSetup(rex::RuntimeConfig& config) override {
    forza3::LoadPixGpuCapturerIfRequested();
    if (config.gpu_plugin.empty()) {
      config.gpu_plugin = "xenos";
    }
    // Load the plugin here rather than letting the SDK do it right after this
    // hook, so its GPU cvars exist before the menu's saved settings are
    // applied. (If it fails, the SDK retries and reports the error.)
    if (!config.graphics) {
      config.graphics = rex::system::LoadGpuPlugin(config.gpu_plugin);
    }
    // forza3.toml is loaded by now; the menu's saved settings go on top,
    // before the window and GPU read them.
    forza3::LoadUserSettings();
  }

  // XEX loaded (title id known) and the game thread not started yet: install
  // the Disc 2 packages before the title first enumerates its content.
  void OnPreLaunchModule() override {
    forza3::InstallCrashDumpHandler();
    forza3::RegisterPixCaptureHotkey();
    forza3::MountUtilityDrive(runtime(), user_data_root());
    forza3::InstallDisc2Content(runtime(), game_data_root(), user_data_root());
  }

  void OnConfigureFonts(ImFontAtlas* atlas) override {
    forza3::AddSettingsMenuFont(atlas);
  }

  // Run with no arguments: find assets\ and default userdata\ beside it.
  void OnConfigurePaths(rex::PathConfig& paths) override {
    forza3::ApplyDefaultPaths(paths);
  }

  // F2 settings menu.
  void OnCreateDialogs(rex::ui::ImGuiDrawer* drawer) override {
    forza3::CreateSettingsMenu(drawer, [this] {
      app_context().CallInUIThreadDeferred([this] {
        if (window()) {
          window()->RequestClose();
        }
      });
    });
  }

  // The game gets no controller input while the settings menu is open, so
  // keys pressed in it (rebinding, arrows) don't drive the car. Otherwise
  // the same rule as the SDK's: input unless the mouse is over an overlay.
  void OnPostSetup() override {
    auto* input = static_cast<rex::input::InputSystem*>(runtime()->input_system());
    if (input && imgui_drawer()) {
      input->SetActiveCallback([this] {
        return !forza3::IsSettingsMenuOpen() && !imgui_drawer()->GetIO().WantCaptureMouse;
      });
    }
  }

  // Override virtual hooks for customization:
  // void OnPostInitLogging() override {}
  // void OnLoadXexImage(std::string& xex_image) override {}
  // void OnPostLoadXexImage() override {}
  // std::unique_ptr<rex::ui::ImGuiDialog> CreateAchievementsOverlay() override;
  // std::unique_ptr<rex::ui::AchievementNotificationDialog>
  // CreateAchievementNotificationDialog() override;
  // void OnShutdown() override {}
};
