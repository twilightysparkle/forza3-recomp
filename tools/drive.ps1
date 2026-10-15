# Test-driving helpers for forza3: launch, send keys to the window, capture it.
# Usage (PowerShell):  . .\tools\drive.ps1 ; Start-Forza3 run8 ; Send-Key Return ; Save-Shot a.png
# Keys go through PostMessage, so the window does not need focus.

Add-Type -AssemblyName System.Drawing
if (-not ('F3Win' -as [type])) {
  Add-Type @'
using System; using System.Runtime.InteropServices;
public static class F3Win {
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint f);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern uint MapVirtualKey(uint code, uint type);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
}
'@
}

# Physical pixels: without this, a DPI-scaled display (e.g. 125%) makes
# GetClientRect report logical size and Save-Shot crops the top-left corner.
if (-not ('F3Dpi' -as [type])) {
  Add-Type @'
using System; using System.Runtime.InteropServices;
public static class F3Dpi {
  [DllImport("user32.dll")] public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr c);
}
'@
}
[F3Dpi]::SetThreadDpiAwarenessContext([IntPtr](-4)) | Out-Null

$script:F3Root = Split-Path $PSScriptRoot -Parent
$script:F3Run = Join-Path $F3Root 'run'
$script:F3Keys = @{ Return = 0x0D; Space = 0x20; Escape = 0x1B; Backspace = 0x08; Tab = 0x09
  Left = 0x25; Up = 0x26; Right = 0x27; Down = 0x28; Shift = 0x10; F9 = 0x78 }

function Start-Forza3([string]$name = 'play', [string]$level = 'info', [string[]]$extra = @()) {
  $exe = Join-Path $F3Root 'out\build\win-amd64-relwithdebinfo\forza3.exe'
  # Game and user data use the exe's defaults (assets\ and userdata\ in the project).
  $args = @("--log_file=$F3Run\$name.log", '--log_level', $level, '--log_flush_interval=1') + $extra
  $script:F3Proc = Start-Process $exe -ArgumentList $args -WorkingDirectory $F3Run -PassThru
  for ($i = 0; $i -lt 60 -and $F3Proc.MainWindowHandle -eq 0; $i++) { Start-Sleep -Milliseconds 500; $F3Proc.Refresh() }
  $script:F3Hwnd = $F3Proc.MainWindowHandle
  "forza3 pid $($F3Proc.Id)"
}

# Re-attach to an already running instance (each new shell loses the handles).
# -name targets another process, e.g. xenia_canary for reference comparisons.
function Connect-Forza3([string]$name = 'forza3') {
  $script:F3Proc = Get-Process $name -ErrorAction Stop | Select-Object -First 1
  $script:F3Hwnd = $F3Proc.MainWindowHandle
}

function Get-VK([string]$key) {
  if ($F3Keys.ContainsKey($key)) { return $F3Keys[$key] }
  if ($key -match '^0x[0-9A-Fa-f]+$') { return [Convert]::ToInt32($key.Substring(2), 16) }
  if ($key -match '^F([1-9]|1[0-9]|2[0-4])$') { return 0x6F + [int]$Matches[1] }
  if ($key.Length -eq 1) { return [int][char]$key.ToUpper() }
  throw "unknown key $key"
}

function Send-KeyDown([string]$key) {
  $vk = Get-VK $key; $sc = [F3Win]::MapVirtualKey($vk, 0)
  $ext = if ($vk -in 0x21..0x28 -or $vk -in 0x2D, 0x2E) { 1 -shl 24 } else { 0 }
  [F3Win]::PostMessage($F3Hwnd, 0x100, [IntPtr]$vk, [IntPtr](1 -bor ($sc -shl 16) -bor $ext)) | Out-Null
}

function Send-KeyUp([string]$key) {
  $vk = Get-VK $key; $sc = [F3Win]::MapVirtualKey($vk, 0)
  $ext = if ($vk -in 0x21..0x28 -or $vk -in 0x2D, 0x2E) { 1 -shl 24 } else { 0 }
  [F3Win]::PostMessage($F3Hwnd, 0x101, [IntPtr]$vk, [IntPtr]([int64](0xC0000001 -bor ($sc -shl 16) -bor $ext))) | Out-Null
}

# Left click at client coordinates (same pixels as Save-Shot), e.g. for overlays.
function Send-Click([int]$x, [int]$y) {
  $pos = [IntPtr](($y -shl 16) -bor ($x -band 0xFFFF))
  [F3Win]::PostMessage($F3Hwnd, 0x200, [IntPtr]0, $pos) | Out-Null
  Start-Sleep -Milliseconds 80
  [F3Win]::PostMessage($F3Hwnd, 0x201, [IntPtr]1, $pos) | Out-Null
  Start-Sleep -Milliseconds 80
  [F3Win]::PostMessage($F3Hwnd, 0x202, [IntPtr]0, $pos) | Out-Null
}

# Tap (default 150 ms) or hold a key for -ms milliseconds.
function Send-Key([string]$key, [int]$ms = 150) { Send-KeyDown $key; Start-Sleep -Milliseconds $ms; Send-KeyUp $key }

function Save-Shot([string]$file) {
  [F3Dpi]::SetThreadDpiAwarenessContext([IntPtr](-4)) | Out-Null
  $r = New-Object F3Win+RECT
  if (-not [F3Win]::GetClientRect($F3Hwnd, [ref]$r) -or $r.R -le $r.L -or $r.B -le $r.T) { return "no window ($(Test-Forza3))" }
  $bmp = New-Object System.Drawing.Bitmap ($r.R - $r.L), ($r.B - $r.T)
  $g = [System.Drawing.Graphics]::FromImage($bmp); $dc = $g.GetHdc()
  [F3Win]::PrintWindow($F3Hwnd, $dc, 3) | Out-Null; $g.ReleaseHdc($dc); $g.Dispose()
  $path = Join-Path $F3Run $file; $bmp.Save($path); $bmp.Dispose(); $path
}

# Title -> profile -> Casual -> R8 showcase (A after the narration) -> opening race.
function Enter-FirstRace {
  Start-Sleep 30; Send-Key Return; Start-Sleep 5; Send-Key Return; Start-Sleep 4; Send-Key Return; Start-Sleep 6
  foreach ($i in 1..7) { Start-Sleep 8; Send-Key Return }
  Start-Sleep 20
}

# Crop a capture and scale it up (nearest neighbour) for close inspection.
function Save-Crop([string]$src, [string]$dst, [int]$x, [int]$y, [int]$w, [int]$h, [int]$scale = 2) {
  $b = [System.Drawing.Bitmap]::FromFile((Join-Path $F3Run $src))
  $o = New-Object System.Drawing.Bitmap ($w * $scale), ($h * $scale)
  $g = [System.Drawing.Graphics]::FromImage($o); $g.InterpolationMode = 'NearestNeighbor'
  $g.DrawImage($b, (New-Object System.Drawing.Rectangle 0, 0, ($w * $scale), ($h * $scale)), (New-Object System.Drawing.Rectangle $x, $y, $w, $h), 'Pixel')
  $o.Save((Join-Path $F3Run $dst)); $g.Dispose(); $o.Dispose(); $b.Dispose()
}

function Test-Forza3 { $F3Proc.Refresh(); if ($F3Proc.HasExited) { "EXITED 0x{0:X8}" -f $F3Proc.ExitCode } else { "running, cpu {0:N0}s" -f $F3Proc.TotalProcessorTime.TotalSeconds } }
