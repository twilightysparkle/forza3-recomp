@echo off
rem Usage: build.cmd [relwithdebinfo|release|debug]   (default: relwithdebinfo)
rem Loads MSVC/Windows SDK libraries, uses LLVM clang 20+, then configures and
rem builds. Codegen from assets\default.xex runs as part of the build.
rem debug only links if the SDK was also installed in Debug (_ITERATOR_DEBUG_LEVEL).
setlocal
set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=relwithdebinfo"
set "PRESET=win-amd64-%CONFIG%"

call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
set "PATH=C:\Program Files\LLVM\bin;C:\Program Files\CMake\bin;%PATH%"
cd /d "%~dp0" || exit /b 1

if not exist "assets\default.xex" (
  echo assets\default.xex is missing. Extract Forza Motorsport 3 from your own disc into assets\ first.
  exit /b 1
)

cmake --preset %PRESET% -DCMAKE_PREFIX_PATH="%~dp0..\rexglue-sdk\out\install\win-amd64" || exit /b 1
cmake --build --preset %PRESET% || exit /b 1
