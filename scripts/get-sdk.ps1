# Build the ReXGlue SDK this port needs into _sdk\win-amd64 (called by build.bat).
#
# The official release zip can't be used: the port needs the SDK fixes in
# patches\rexglue-sdk\ (and the rebuilt resolve shaders in
# patches\rexglue-sdk\shaders\). So this clones the SDK source at the release
# tag into _sdk\src, applies them, and builds and installs the SDK. It runs once,
# and again only when the patches change. Needs git, and the MSVC + clang
# environment build.bat sets up.
#
# usage: scripts\get-sdk.ps1 [-Version 0.10.0]
param([string]$Version = "0.10.0")
$ErrorActionPreference = "Stop"

$root = Split-Path $PSScriptRoot
$sdk = Join-Path $root "_sdk"
$src = Join-Path $sdk "src"
$install = Join-Path $sdk "win-amd64"
$stamp = Join-Path $install "forza3-sdk.txt"
$patchDir = Join-Path $root "patches\rexglue-sdk"

function Invoke-Checked([string]$what) {
  if ($LASTEXITCODE -ne 0) { throw "$what failed (exit code $LASTEXITCODE)" }
}

# Identify the patch set, so changed patches trigger a rebuild.
$hashes = Get-ChildItem $patchDir -Recurse -File | Sort-Object FullName |
  ForEach-Object { (Get-FileHash $_.FullName -Algorithm SHA256).Hash }
$sha = [System.Security.Cryptography.SHA256]::Create()
$id = -join ($sha.ComputeHash([Text.Encoding]::ASCII.GetBytes($hashes -join "")) |
  ForEach-Object { $_.ToString("x2") })
$want = "rexglue-sdk $Version patches $id"

if ((Test-Path (Join-Path $install "bin\rexglue.exe")) -and (Test-Path $stamp) -and
    ((Get-Content $stamp -Raw).Trim() -eq $want)) {
  "SDK up to date in $install"
  exit 0
}

if (-not (Test-Path (Join-Path $src ".git"))) {
  "Cloning rexglue-sdk v$Version (with submodules) into $src"
  git clone --depth 1 --branch "v$Version" --recurse-submodules --shallow-submodules `
    https://github.com/rexglue/rexglue-sdk.git $src
  Invoke-Checked "git clone"
} else {
  "Resetting $src to v$Version"
  git -C $src reset --hard --quiet
  Invoke-Checked "git reset"
}

"Applying patches"
foreach ($patch in Get-ChildItem $patchDir -Filter *.patch | Sort-Object Name) {
  "  $($patch.Name)"
  git -C $src apply --ignore-whitespace $patch.FullName
  Invoke-Checked "git apply $($patch.Name)"
}
Copy-Item (Join-Path $patchDir "shaders\d3d12_5_1\*.h") `
  (Join-Path $src "src\graphics\shaders\bytecode\d3d12_5_1") -Force

"Building the SDK (this takes a while the first time)"
Push-Location $src
try {
  cmake --preset win-amd64 "-DCMAKE_INSTALL_PREFIX=$install"
  Invoke-Checked "SDK configure"
  cmake --build out/build/win-amd64 --config Release --target install
  Invoke-Checked "SDK build"
} finally {
  Pop-Location
}

Set-Content -Path $stamp -Value $want -Encoding ASCII
"SDK ready in $install"
