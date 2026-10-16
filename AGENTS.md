# AGENTS.md

Notes for coding agents working on this repository: a static recompilation of
Forza Motorsport 3 (Xbox 360, title `4D53084D`, Ultimate Collection) with the
ReXGlue SDK v0.10.0, plus `Forza3Launcher`. Read [README.md](README.md) for the
player-facing picture; this file is about working on the code.

## Ground rules

- **Never commit game data.** No `default.xex`, disc images, extracted files,
  textures, screenshots or videos of the game, texture or frame dumps. `assets/`,
  `game/`, `userdata/`, `run/`, `*.iso` and `*.xex` are ignored; keep it that way.
- **`generated/default/` is a machine translation of the game's code.** It is
  committed (as hl360-Forza2 does) so clones build without the game; it is not
  covered by the project's MIT license. Don't hand-edit it; change
  `forza3_manifest.toml` or wrap a function from `src/` instead. Codegen's
  `codegen.d` is ignored (it holds absolute local paths); the stamps and
  `codegen.partition.json` are committed with the sources.
- Hashes are fine to commit (`launcher/data/game_files.sha256`); file contents are not.
- The project's own code is MIT ([LICENSE](LICENSE)). Code taken from elsewhere
  needs a compatible license and its notice in `docs/THIRD_PARTY_NOTICES.txt`.
  hl360-Forza2 (the design reference) has no license: take ideas, not code.

## Build

Windows only. Needs Visual Studio 2022 (or Build Tools) with the C++ workload,
LLVM (clang 20+) and Git.

```
build.bat release          :: default; or relwithdebinfo (symbols) / debug
tools\package_release.ps1  :: dist\Forza3-Native-<version>-windows-x64.zip
```

- The first `build.bat` builds the **patched SDK** into `_sdk\` through
  `scripts\get-sdk.ps1` (clone v0.10.0 with submodules, `git apply`
  `patches\rexglue-sdk\*.patch`, copy `patches\rexglue-sdk\shaders\`, build,
  install). That takes a long time; it reruns only when the patch set's hash
  changes. Don't delete `_sdk\`.
- `_sdk\src\out\` caches the absolute path it was configured in: if `_sdk\` is
  moved, delete `_sdk\src\out\` and let `get-sdk.ps1` reconfigure.
- Without `assets\default.xex`, codegen is swapped for `cmake\codegen_skipped.*`
  and the committed `generated\` code is built. With it, codegen regenerates
  `generated\default\` (commit the result if the manifest changed).
- Outputs: `out\build\win-amd64-<config>\` holds `forza3.exe`,
  `Forza3Launcher.exe`, the SDK DLLs, `forza3.toml` and `game_files.sha256`.
  `forza3_config` copies the config and refreshes the SDK DLLs on every build.

## Run and test

- `run.bat` runs the game against `assets\` (logs to `run.log`). `forza3.exe`
  without arguments finds `game\` or `assets\` and uses `userdata\` beside it.
- `Forza3Launcher.exe --extract-iso <image.iso> <dir>` runs the importer
  headless (exit code 0 on success).
- **Scripted sessions:** dot-source `tools\drive.ps1` (PowerShell) for
  `Start-Forza3`, `Connect-Forza3 [-name Forza3Launcher]`, `Send-Key`,
  `Save-Shot`, `Enter-FirstRace`, `Test-Forza3`. `Start-Forza3` launches the
  `relwithdebinfo` build; captures land in `run\`.
- Keys are posted with `PostMessage`, so the window needs no focus. **Posted
  mouse clicks do not reach SDL windows**: drive UIs with the keyboard (the F2
  menu and the launcher both support arrows, Tab, Enter/Space; PgUp/PgDn switch
  tabs in the F2 menu).
- `Save-Shot` sets per-monitor DPI awareness; without it, on a scaled display
  captures are silently cropped to the top-left of the window.
- To reach a race from boot: `Enter-FirstRace` (title, profile, Casual, R8
  showcase, opening race). The opening race's starting grid is the standard
  check for the car shadow and road flicker fixes.
- There is no unit test suite. Verify changes by building, running the game to
  the relevant screen and capturing it; for the launcher, run it from a copy of
  the release zip with a game folder next to it.

## Where things are

| Area | Files |
|---|---|
| App hooks (paths, plugin load, dialogs, input gating) | `src/forza3_app.h` |
| Default paths, no-argument start | `src/default_paths.cpp` |
| In-game F2 settings menu, `forza3.user.toml` | `src/settings_menu.cpp` |
| Disc 2 content install | `src/disc2_install.cpp` |
| `cache:` device for the race replay stream | `src/utility_drive.cpp` |
| Crash dumps, PIX captures | `src/crash_dump.cpp`, `src/pix_capture.cpp` |
| Launcher UI, ISO import, verification, profiles, settings | `launcher/src/` |
| Shipped runtime settings and key layout | `config/forza3.toml` |
| SDK fixes | `patches/rexglue-sdk/` (`0100` shadow fix, `0101` profile name) |
| Analysis scripts (XEX, pointer scans, fibers) | `tools/analysis/` |

## Changing the SDK

1. Edit the source in `_sdk\src\` (it is a clean v0.10.0 checkout with the
   patches applied).
2. Write the change as a new patch: `git -C _sdk\src diff -- <files> >
   patches\rexglue-sdk\01NN-<name>.patch` (only your hunks; the tree already
   carries the other patches).
3. Rebuild incrementally with
   `cmake --build _sdk\src\out\build\win-amd64 --config Release --target install`
   (in a VS x64 environment), or let `build.bat` rebuild everything.
4. Check the patch applies to a clean tree: `git worktree add` a v0.10.0
   checkout and `git apply --check` all patches in order.

The resolve shader bytecode in `patches\rexglue-sdk\shaders\` comes from
`tools\build_resolve_shaders.py` (upstream Xenia sources with xenia-canary's
number-format packing; canary's own shaders use a different EDRAM MSAA layout
and make every MSAA resolve blocky). Rerun it only to change those shaders.

## Gotchas

- **Line endings:** `.gitattributes` keeps sources LF, `.bat/.cmd/.ps1` CRLF.
  Scripted edits through Python in text mode on Windows write CRLF and mangle
  `\r` / `\n` escapes inside C++ string literals; prefer the editor tool for
  code, and check `'\r'` literals after scripted edits.
- **PATH:** the developer's PATH contains a stray `"`. `cmd` stops searching at
  it, so `build.bat` puts its tools at the front of PATH; keep it that way.
- **GPU plugin cvars** (graphics settings) don't exist until
  `rexgpu-xenos.dll` is loaded; `Forza3App::OnPreSetup` loads it before the
  user settings are applied.
- **ImGui 1.92:** `PushFont(font, 0)` keeps the *current* size; pass
  `font->LegacySize` (or a size) to get a bigger font.
- **SDK SDL3** is static and built without SDL's 2D renderer; the launcher
  draws through the SDL GPU API (`imgui_impl_sdlgpu3`).
- Settings shared by the launcher and the F2 menu live in `forza3.user.toml`;
  both write only the keys that differ from the shipped `forza3.toml`.

## Commits and releases

- Branch `main` on `twilightysparkle/forza3-recomp`. Keep
  `CHANGELOG.md` updated; the version shown by the launcher and used for the
  release zip is its newest `## [x.y.z]` entry.
- Releases: `build.bat release`, `tools\package_release.ps1`, then
  `gh release create vX.Y.Z dist\Forza3-Native-X.Y.Z-windows-x64.zip`. Ask the
  owner before publishing one.
