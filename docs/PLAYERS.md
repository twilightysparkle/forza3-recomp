Forza Motorsport 3 - Native PC Port
===================================

A static recompilation of Forza Motorsport 3 (Xbox 360) for Windows. No game
files are included: you need your own copy of the game.

Setting up
----------

1. Extract Forza Motorsport 3 Disc 1 into the game\ folder, so that
   game\default.xex exists. Tested with the Ultimate Collection
   (title ID 4D53084D).
2. Optional: extract Disc 2 into game\Disc 2\. Its bonus content is installed
   automatically the first time you play; no disc swap is needed.
3. Run forza3.exe.

Your saves and the installed Disc 2 content are kept in userdata\, next to
game\. Back that folder up to keep your progress.

Settings
--------

Press F2 in game for the settings menu: display (fullscreen, window size,
vertical sync, upscaling), graphics (internal resolution, texture filtering,
anti-aliasing), controls (keyboard and mouse, every key can be changed),
language and sound. Settings marked "restart" apply after restarting the game;
the menu offers "Save and restart". Your settings are saved to
forza3.user.toml; delete it to go back to the defaults.

Other keys: F3 performance overlay, F4 every emulator option, F7 achievements.

Controls
--------

Any Xbox-compatible controller works. Default keyboard layout:

  Accelerate            W
  Brake / reverse       S
  Steer                 A / D
  Accept                Space or Enter
  Back                  E, Backspace or Escape
  Menu navigation       Arrow keys
  Look around           Shift + arrow keys
  Start / pause         P or F1

Problems
--------

This is an early work in progress: career mode and most tracks have not been
tested. If the game crashes, the dump in crashdumps\ and the log in logs\ help
to find the cause.

Forza Motorsport is a trademark of Microsoft. This project is not affiliated
with or endorsed by Microsoft, Xbox Game Studios or Turn 10 Studios. Licenses
of the components it uses are in THIRD_PARTY_NOTICES.txt.
