<div align="center">

# Forza Motorsport 3 — Native PC Port

**A static recompilation of the Xbox 360 racing game for modern Windows PCs.**

[![Platform](https://img.shields.io/badge/Platform-Windows_x64-0078D4?style=for-the-badge&logo=windows&logoColor=white)](#requirements)
[![SDK](https://img.shields.io/badge/ReXGlue_SDK-v0.10.0-6E40C9?style=for-the-badge)](https://github.com/rexglue/rexglue-sdk)
[![Game data](https://img.shields.io/badge/Game_files-Not_included-EF4444?style=for-the-badge)](#legal-notice)
[![Status](https://img.shields.io/badge/Status-Work_in_progress-F59E0B?style=for-the-badge)](#known-issues)

[Getting started](#getting-started) · [Settings menu](#the-settings-menu-f2) · [Changelog](CHANGELOG.md) · [Building from source](#building-from-source) · [Report an issue](../../issues)

</div>

Play *Forza Motorsport 3* (Xbox 360, 2009) natively on Windows. The game's
PowerPC code is statically recompiled ahead of time into C++ and then compiled
for x86-64: there is no CPU emulation and no JIT compiler. Graphics are
rendered through Direct3D 12.

> [!IMPORTANT]
> No copyrighted game files are included. You must provide your own Xbox 360
> copy of *Forza Motorsport 3*.

> [!WARNING]
> This is an early work in progress with no releases yet. The game boots,
> menus work, the Disc 2 content is installed, and races are drivable with the
> correct car shadows. Career mode, online features and most tracks have not
> been tested. Xbox Live is not available.

## Highlights

- Native x86-64 execution through static PowerPC-to-C++ recompilation.
- Direct3D 12 rendering, with internal resolutions up to 3x (2160p, experimental).
- In-game settings menu (**F2**): display, graphics, key remapping, language and sound.
- Anisotropic filtering, FXAA, AMD FSR and AMD CAS.
- Xbox-compatible controllers and fully remappable keyboard controls.
- Disc 2 (Ultimate Collection bonus content) installed automatically, no disc swap.
- Starts with no arguments: finds the game and keeps saves next to it.
- Fixes for the car shadow and road flickering bugs in the emulated GPU.

## Requirements

- Windows 10 or 11, 64-bit
- A GPU with Direct3D 12 support
- About 20 GB of free disk space (11 GB extracted game, 3.5 GB installed Disc 2 content, the build)
- Your own *Forza Motorsport 3* disc, extracted. Tested with the
  **Ultimate Collection** (title ID `4D53084D`, Disc 1 media ID `6F74359D`,
  version 8). Disc 2 is optional but recommended.
- To build: Visual Studio 2022 (or Build Tools) with the C++ workload, LLVM
  and Git (see [Building from source](#building-from-source))

## Getting started

1. [Build the port](#building-from-source): run **`build.bat`**.
2. Extract **Disc 1** into `assets\`, so that `assets\default.xex` exists.
3. Optionally extract **Disc 2** into `assets\Disc 2\`. Its 8 content packages
   are installed into your save folder on first launch (the game's own
   "insert Disc 2" step needs a tray swap, so the port does it for you).
4. Run **`run.bat`**, or `out\build\win-amd64-release\forza3.exe` directly
   (double-click works). It finds `assets\` by itself and keeps your saves in
   `userdata\` beside it.

To play elsewhere, `tools\package_release.ps1` makes a self-contained zip in
`dist\`: extract it anywhere, put Disc 1 in its `game\` folder and run
`forza3.exe` (see [`docs/PLAYERS.md`](docs/PLAYERS.md), shipped as `README.txt`).

On first launch a hint in the corner reminds you that **F2** opens the settings
menu.

## The settings menu (F2)

| Tab | What you can do |
|---|---|
| **Display** | Fullscreen, window size, vertical sync, keep 16:9 (black bars), upscaling filter (bilinear, AMD CAS, AMD FSR 1). |
| **Graphics** | Internal resolution (1x / 2x / 3x), anisotropic filtering, extra FXAA, render-to-texture readback (leave it on *Balanced*). |
| **Controls** | Keyboard controls on or off, mouse camera and sensitivity, and every key binding: set, add an alternative key, clear or reset, with a warning when a key is used twice. |
| **Game & sound** | Console language, mute, and a list of hotkeys. |

Most changes apply immediately. Settings marked *restart* take effect after a
restart: the footer lists them and offers **Save and restart**. **Save** keeps
your changes in `forza3.user.toml`; **Reset to defaults** goes back to the
shipped settings. The game ignores controller input while the menu is open, and
it works with the mouse or the keyboard (arrows, Enter/Space, PgUp/PgDn to
switch tabs).

Other hotkeys: **F3** performance overlay, **F4** every emulator option,
**F7** achievements, **`** (Backtick) log console.

## Controls

Any Xbox-compatible controller works without setup. Default keyboard layout
(keys are all-or-nothing, so throttle, brake and steering are digital):

| Action | Key |
|---|---|
| Accelerate | W |
| Brake / reverse | S |
| Steer | A / D |
| Accept (A) | Space, Enter |
| Back (B) | E, Backspace, Escape |
| X / Y | Q or X / R or Y |
| Bumpers (LB / RB) | V / C |
| Menu navigation (D-pad) | Arrow keys |
| Look around (right stick) | Shift + arrow keys |
| Back button | Tab |
| Start / pause | P, F1 |

All keys can be changed in the **F2** menu → **Controls**.

## Where things are

| What | Where |
|---|---|
| Saves, profile, installed Disc 2 content | `userdata\`, next to `assets\` |
| Your settings | `forza3.user.toml`, next to the exe |
| Shipped settings | `forza3.toml`, next to the exe (copied from `config\` by the build) |
| Logs | `logs\`, next to the exe |
| Crash dumps | `crashdumps\`, next to the exe |

`--game_data_root=<dir>` and `--user_data_root=<dir>` override the game and
save folders.

## Known issues

- Only the opening race and a few exhibition laps have been played; career
  mode, most tracks and cars, and replays are untested.
- The items of the *Racing Game Experience* menu were occasionally missing.
- The first time a track or car is shown there can be stutter while its
  shaders compile.
- The graphics fixes are made for the Direct3D 12 backend; Vulkan is not patched.
- The 2x and 3x internal resolutions are experimental.
- Xbox Live features are not available.

If the game crashes, a dump is written to `crashdumps\`. Please open an
[issue](../../issues) and include it with the log from `logs\`.

---

## Building from source

Requires Visual Studio 2022 (or Build Tools) with the C++ workload, LLVM (clang
20 or newer) and Git:

```
winget install Microsoft.VisualStudio.2022.BuildTools --override "--add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
winget install LLVM.LLVM Git.Git
build.bat release          :: or relwithdebinfo for symbolized crash dumps
tools\package_release.ps1  :: builds dist\Forza3-Native-<version>-windows-x64.zip
```

On its first run `build.bat` builds the [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk)
into `_sdk\` ([`scripts/get-sdk.ps1`](scripts/get-sdk.ps1)): it clones v0.10.0
with its submodules, applies the fixes in [`patches/rexglue-sdk/`](patches/rexglue-sdk)
and the rebuilt resolve shaders, then builds and installs it. That takes a
while once; it is redone only when the patches change. (The official SDK zip
can't be used because of those fixes.)

The recompiled code in `generated/` is committed, so a clone builds without the
game. To regenerate it, put your `default.xex` in `assets\` and build again.
`run.bat` starts the build against `assets\` and logs to `run.log`.

The resolve shaders in `patches/rexglue-sdk/shaders/` are generated by
[`tools/build_resolve_shaders.py`](tools/build_resolve_shaders.py) from upstream
[Xenia](https://github.com/xenia-project/xenia) and
[xenia-canary](https://github.com/xenia-canary/xenia-canary) sources (needs the
Windows SDK's FXC); rerun it only to change them.

### Repository layout

| Path | Contents |
|---|---|
| [`build.bat`](build.bat), [`run.bat`](run.bat) | Build (and the SDK on first run); developer run |
| [`forza3_manifest.toml`](forza3_manifest.toml) | `rexglue codegen` configuration: functions the analysis missed, fiber replacements, setjmp/longjmp |
| [`generated/`](generated) | The game's program code translated to C++ (machine-generated, committed) |
| [`src/`](src) | Port code on top of the recompiled game: app setup, settings menu, Disc 2 install, crash dumps, fixes and diagnostics |
| [`config/forza3.toml`](config/forza3.toml) | Runtime settings the game needs and the default key layout, copied next to the exe |
| [`patches/rexglue-sdk/`](patches/rexglue-sdk) | SDK fixes: nine from [ForzaHorizon1-ReImagined](https://github.com/Dyotson/ForzaHorizon1-ReImagined) (all matching upstream Xenia), `0100` for FM3's shadows, and its rebuilt resolve shaders |
| [`tools/`](tools) | Packaging, the resolve shader build, texture decoding, scripted test sessions (`drive.ps1`); [`tools/analysis/`](tools/analysis) has the XEX scripts used to find what codegen missed |
| [`docs/`](docs) | Player notes (`PLAYERS.md`, shipped as `README.txt`), third-party notices |
| [`cmake/`](cmake), [`scripts/`](scripts) | Codegen stand-in for clones without the game; the SDK build |
| `assets/`, `userdata/`, `_sdk/` | Your game files, your saves, the built SDK (not in the repository) |

### How it was ported

| Piece | Where |
|---|---|
| PowerPC → C++ translation | `rexglue codegen`, driven by [`forza3_manifest.toml`](forza3_manifest.toml) |
| Functions the analysis missed: ~15 entry points, plus 193 found only through data and code-built pointers (vtables, callbacks) | `[entrypoint.functions]` in the manifest, found with `tools/analysis/ptrscan.py` |
| XAPI fibers (the title's job system) mapped to host fibers | `[entrypoint.rexcrt]` in the manifest, found with `tools/analysis/fibers.py` |
| libjpeg's `longjmp` out of decode errors (crashed in the R8 showcase) | `setjmp_address` / `longjmp_address` in the manifest |
| Race replay stream to `cache:\` (null dereference at race start) | [`src/utility_drive.cpp`](src/utility_drive.cpp) |
| Disc 2 content packages installed without a disc swap | [`src/disc2_install.cpp`](src/disc2_install.cpp) |
| Road flickering and block patterns while driving | `readback_resolve = "some"` in [`config/forza3.toml`](config/forza3.toml) |
| Dark shadow block around the car: signed-integer resolves and integer texture fetches | SDK patch [`0100`](patches/rexglue-sdk/0100-resolve-and-fetch-integer-number-formats.patch) and [`tools/build_resolve_shaders.py`](tools/build_resolve_shaders.py) |
| Settings menu, key remapping, `forza3.user.toml` | [`src/settings_menu.cpp`](src/settings_menu.cpp) |
| Start without arguments | [`src/default_paths.cpp`](src/default_paths.cpp) |
| Guest call chain on crashes | [`src/crash_dump.cpp`](src/crash_dump.cpp) |
| GPU frame captures (`--pix_gpu_capturer=true`, then F9) | [`src/pix_capture.cpp`](src/pix_capture.cpp) |

Every recompiled function is a weak `sub_<address>` that forwards to
`__imp__sub_<address>`, so any of them can be wrapped or replaced from `src/`.

<details>
<summary>Notes on the two graphics fixes</summary>

- **Road flickering.** Without resolve readback, CPU writes near FM3's shadow
  maps make the emulator re-upload those pages from stale guest memory, so the
  shadow maps get corrupted in page-sized blocks (grid and dash patterns on the
  road, flashing while driving).
- **Car shadow block.** FM3 renders a top-down variance shadow map of the car
  (k_16_16, 4x MSAA), resolves it with exp bias +10 as *SignedInteger*, then
  samples it as signed *integer* data with exp_adjust −15 and tests `>= 0.99`
  to build a mask the ground shader multiplies in. The SDK's resolve shaders
  ignored the destination number format (packing every texel as an unsigned
  fraction, so the map saturated to −1), and its texture fetches ignored the
  integer number format (returning 32767 as 1.0). Both were found by diffing
  frame dumps against an instrumented xenia-canary build, which renders the
  shadow correctly. The rebuilt resolve shaders come from upstream Xenia
  sources, whose EDRAM MSAA sample layout matches the SDK's.

</details>

## Legal notice

This repository contains no game assets: no executables, disc images, textures,
audio or video. `generated/` is a machine translation of the game's program
code. You must own the game to use this port. Forza Motorsport is a trademark of Microsoft; this project is not
affiliated with or endorsed by Microsoft, Xbox Game Studios or Turn 10 Studios.
Third-party licenses are in [`docs/THIRD_PARTY_NOTICES.txt`](docs/THIRD_PARTY_NOTICES.txt),
included in the player zip.

## Credits

- [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk) — recompiler and runtime
- [Xenia](https://github.com/xenia-project/xenia) and [xenia-canary](https://github.com/xenia-canary/xenia-canary) — the emulation research ReXGlue builds on, and the reference for the shadow fix
- [hl360-Forza2](https://github.com/0herculabs/hl360-Forza2) — the Forza 2 port this project follows
- [ForzaHorizon1-ReImagined](https://github.com/Dyotson/ForzaHorizon1-ReImagined) — SDK fixes shared with Forza Horizon
