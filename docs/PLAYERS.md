Forza Motorsport 3 - Native PC Port
===================================

A static recompilation of Forza Motorsport 3 (Xbox 360) for Windows. No game
files are included: you need your own Forza Motorsport 3 Ultimate Collection
disc images (.iso), Disc 1 and Disc 2.

Getting started
---------------

1. Run Forza3Launcher.exe.
2. Go to Install -> Choose ISO and import and pick your Disc 1 image. The game
   is extracted to the game\ folder and every file is checked against
   game_files.sha256. It takes a minute or two. Do the same with your Disc 2
   image (the bonus content). If the game is already extracted, use Change
   folder instead.
3. Go to Profiles and create your profile.
4. Press PLAY.

Your saves are in userdata\profiles\<profile>\. Back that folder up to keep
your progress.

The launcher
------------

  Profiles          Create, choose and delete profiles. Each has its own saves;
                    the name is your gamertag in the game.
  Graphics          Fullscreen or window, window size, vertical sync, upscaling,
                    internal resolution, texture filtering, anti-aliasing.
  Controls          Keyboard and mouse, and key remapping.
  Language & sound  Game language and mute.
  Install           Import the discs, change the game folder, verify the files.

Changes are saved automatically to forza3.user.toml. In game, F2 opens the
same settings (F4 lists every emulator option). The cover art is not shipped:
put any cover.png next to Forza3Launcher.exe and it fills the left column.

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
tested. If the game closes with an error, the launcher tells you; the files in
crashdumps\ and logs\ help to find the cause.

Forza Motorsport is a trademark of Microsoft. This project is not affiliated
with or endorsed by Microsoft, Xbox Game Studios or Turn 10 Studios. Licenses
of the components it uses are in THIRD_PARTY_NOTICES.txt.
