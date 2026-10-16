# Changelog

## [0.1.0] - 2026-10-15

First playable build: the game boots, menus work and races are drivable.

### Added

- `Forza3Launcher.exe`: install from the disc images (Install -> Choose ISO
  and import, for Disc 1 and Disc 2, both required), with every extracted file
  checked against `game_files.sha256`; named profiles with separate saves (the
  name is the in-game gamertag) and shared Disc 2 content; graphics, controls
  (key remapping), language and sound settings; PLAY.
- Static recompilation of Forza Motorsport 3 (Ultimate Collection, title
  `4D53084D`) with the ReXGlue SDK v0.10.0, Direct3D 12 rendering.
- In-game settings menu (F2): display, graphics, key remapping, language and
  sound, saved to `forza3.user.toml`, with "Save and restart".
- Keyboard and mouse controls, remappable; Xbox controllers work as is.
- Disc 2 bonus content installed automatically, without the disc swap.
- Starts without arguments: finds `game\` or `assets\` and keeps saves in
  `userdata\` beside it.
- Crash dumps in `crashdumps\`; optional PIX GPU captures (F9).
- `build.bat` builds the patched SDK into `_sdk\` on first run; `run.bat`;
  `tools\package_release.ps1` for player zips.

### Fixed

- Crash in the R8 showcase: libjpeg's `longjmp` is now mapped
  (`setjmp_address` / `longjmp_address`).
- Crash at race start: the replay stream's `cache:` device is mounted.
- Road flickering and block patterns while driving (`readback_resolve`).
- Dark shadow block that rotated with the car: signed-integer resolves and
  integer texture fetches in the SDK (patch `0100`).
