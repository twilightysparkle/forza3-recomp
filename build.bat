@echo off
rem Build forza3 with clang inside the MSVC x64 environment.
rem   build.bat [release|relwithdebinfo|debug]   (default: release)
rem Needs Visual Studio 2022 (or Build Tools) with the C++ workload, LLVM
rem (clang), git and Python. Builds the patched ReXGlue SDK into _sdk\ on first
rem run (scripts\get-sdk.ps1). Without assets\default.xex the committed
rem generated\ code is compiled as it is; with it, codegen regenerates it.
rem debug only links against a Debug SDK build (_ITERATOR_DEBUG_LEVEL).
setlocal
set PRESET=win-amd64-%~1
if "%~1"=="" set PRESET=win-amd64-release
cd /d "%~dp0"

set VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set VSDIR=%%i
if "%VSDIR%"=="" (
  echo Visual Studio 2022 / Build Tools with the C++ workload not found.
  exit /b 1
)
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1

rem clang from LLVM; CMake and Ninja from PATH, else the copies VS ships.
set PATH=%ProgramFiles%\LLVM\bin;%PATH%;%VSDIR%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%VSDIR%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja
where clang++ >nul 2>nul || (echo clang++ not found: install LLVM ^(winget install LLVM.LLVM^). & exit /b 1)
where git >nul 2>nul || (echo git not found: install Git ^(winget install Git.Git^). & exit /b 1)

powershell -NoProfile -ExecutionPolicy Bypass -File scripts\get-sdk.ps1 || exit /b 1

cmake --preset %PRESET% -DCMAKE_PREFIX_PATH="%~dp0_sdk\win-amd64" || exit /b 1
cmake --build --preset %PRESET% || exit /b 1
