@echo off
rem Developer run: the game straight from the build tree, against assets\ and
rem userdata\, logging to run.log. Extra arguments go to the exe (for example
rem --log_level=debug). Set FORZA3_BUILD=relwithdebinfo for the build with
rem symbols (crash dumps resolve guest functions).
setlocal
if "%FORZA3_BUILD%"=="" set FORZA3_BUILD=release
"%~dp0out\build\win-amd64-%FORZA3_BUILD%\forza3.exe" --log_file="%~dp0run.log" %*
