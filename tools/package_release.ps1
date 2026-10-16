# Package a build into dist\Forza3-Native-<version>-windows-x64.zip for players.
#
# usage: tools\package_release.ps1 [-Build release] [-Version 0.1.0]
# Run build.bat (same build) first. The version defaults to the newest entry in
# CHANGELOG.md. The zip holds the launcher and the game, the SDK DLLs, the Visual
# C++ runtime, forza3.toml, game_files.sha256, the player notes, third-party
# notices and SHA256SUMS.txt. No game data: players import their own discs
# with the launcher.
param([string]$Build = "release", [string]$Version = "")
$ErrorActionPreference = "Stop"

$root = Split-Path $PSScriptRoot
$bin = Join-Path $root "out\build\win-amd64-$Build"
if (-not (Test-Path (Join-Path $bin "forza3.exe"))) {
  throw "No build in ${bin}: run build.bat $Build first"
}
if (-not $Version) {
  $line = Select-String -Path (Join-Path $root "CHANGELOG.md") -Pattern '^## \[(\d+\.\d+\.\d+)\]' |
    Select-Object -First 1
  if (-not $line) { throw "No version in CHANGELOG.md; pass -Version" }
  $Version = $line.Matches[0].Groups[1].Value
}

$name = "Forza3-Native-$Version-windows-x64"
$dist = Join-Path $root "dist"
$stage = Join-Path $dist $name
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Force $stage | Out-Null

# Game and SDK binaries.
foreach ($file in "Forza3Launcher.exe", "forza3.exe", "forza3.toml", "game_files.sha256", "rexruntime.dll", "rexgpu-xenos.dll", "TracyClient.dll") {
  $path = Join-Path $bin $file
  if (Test-Path $path) { Copy-Item $path $stage }
  elseif ($file -ne "TracyClient.dll") { throw "Missing $path" }
}

# Visual C++ runtime, from the newest MSVC redist Visual Studio ships.
$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * -property installationPath
$crt = Get-ChildItem (Join-Path $vs "VC\Redist\MSVC\*\x64\Microsoft.VC*.CRT") -Directory -ErrorAction SilentlyContinue |
  Sort-Object FullName | Select-Object -Last 1
if (-not $crt) { throw "Visual C++ redistributable files not found under $vs" }
foreach ($dll in "msvcp140.dll", "msvcp140_1.dll", "vcruntime140.dll", "vcruntime140_1.dll") {
  Copy-Item (Join-Path $crt.FullName $dll) $stage
}

# Documentation.
Copy-Item (Join-Path $root "docs\PLAYERS.md") (Join-Path $stage "README.txt")
Copy-Item (Join-Path $root "docs\THIRD_PARTY_NOTICES.txt") $stage
Copy-Item (Join-Path $root "LICENSE") (Join-Path $stage "LICENSE.txt")
# Checksums of everything shipped.
$sums = Get-ChildItem $stage -Recurse -File | Sort-Object FullName | ForEach-Object {
  $rel = $_.FullName.Substring($stage.Length + 1).Replace('\', '/')
  "{0}  {1}" -f (Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLower(), $rel
}
Set-Content -Path (Join-Path $stage "SHA256SUMS.txt") -Value $sums -Encoding ASCII

$zip = Join-Path $dist "$name.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path (Join-Path $stage "*") -DestinationPath $zip
"Packaged $zip"
