# vpt14_preedit_client.ps1 - docs/61 section 22.3: CLIENT-path terminal
# (jkdesktop.exe --client terminal -> jkapp_terminal.dll -> ClientTerminalApp
# over the live window-server pipe) - the exact stack the user's terminal runs.
# Single-process repro (vpt13) showed the overlay PAINTS; if the client path
# fails, the diff between the two localizes the fault (frame gate / surface
# commit / event stream).
# Delivery: PostMessage WM_KEYDOWN to the SERVER window (SendInput is blocked
# on this rig - vpt13 measured False), server forwards to the focused client.
# Evidence: client_terminal.log ([preedit] stages) + before/after screenshots
# of the server window. PowerShell 5.1, ASCII-only.
$ErrorActionPreference = "Continue"
$build = "I:\progwork\JKENGINE\engine\build"
$exe   = "$build\jkdesktop.exe"
$clog  = "$build\client_terminal.log"
$shotA = "$build\vpt14_before.png"
$shotB = "$build\vpt14_after.png"

function Stop-ProbeProcs {
    Get-CimInstance Win32_Process -Filter "Name='jkdesktop.exe'" |
        Where-Object { $_.CommandLine -match '--client' } |
        ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
}
Stop-ProbeProcs
Start-Sleep -Seconds 1

Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class Wm {
    [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr ctx);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern IntPtr FindWindow(string cls, string title);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern uint MapVirtualKey(uint code, uint type);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint msg, IntPtr w, IntPtr l);
    public struct RECT { public int L, T, R, B; }
    public static void PostVk(IntPtr hwnd, ushort vk) {
        uint scan = MapVirtualKey(vk, 0);
        long dn = 1L | ((long)scan << 16);
        long up = dn | 0xC0000000L;
        PostMessage(hwnd, 0x100, (IntPtr)vk, (IntPtr)dn);
        PostMessage(hwnd, 0x101, (IntPtr)vk, (IntPtr)up);
    }
}
"@
[Wm]::SetProcessDpiAwarenessContext([IntPtr](-4)) | Out-Null

# --- 1. spawn the client terminal against the live server --------------------
Remove-Item $clog -ErrorAction SilentlyContinue
$env:JKTERM_PREEDIT_DBG = "1"
$env:JKTERM_FORCE_HANGUL = "1"
# -RedirectStandardError: with a valid stderr handle MirrorClientStderr skips
# the mirror file and the [preedit] stages land in the redirected pipe - the
# previous run lost the log entirely (Hidden console stderr went nowhere).
$proc = Start-Process -FilePath $exe -ArgumentList "--client","terminal" `
    -WorkingDirectory $build -WindowStyle Hidden -PassThru `
    -RedirectStandardError $clog
Start-Sleep -Seconds 6

$hwnd = [Wm]::FindWindow("SDL_app", "JKENGINE Window Server")
$ok = $true
if ($hwnd -eq [IntPtr]::Zero -or -not [Wm]::IsWindowVisible($hwnd)) {
    $ok = $false
    Write-Host "server window: FAIL (hwnd $hwnd)"
} else {
    [void][Wm]::SetForegroundWindow($hwnd)
    Start-Sleep -Milliseconds 300
    Write-Host ("server window: PASS (hwnd {0})" -f $hwnd)
    Write-Host ("client alive: {0}" -f (-not $proc.HasExited))
}

function Save-Shot([string]$path) {
    Add-Type -AssemblyName System.Drawing
    $wr = New-Object Wm+RECT
    [void][Wm]::GetWindowRect($hwnd, [ref]$wr)
    $w = $wr.R - $wr.L; $h = $wr.B - $wr.T
    $bmp = New-Object System.Drawing.Bitmap($w, $h)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($wr.L, $wr.T, 0, 0, (New-Object System.Drawing.Size($w, $h)))
    $bmp.Save($path)
    $g.Dispose(); $bmp.Dispose()
}

# --- 2. click inside the client terminal layer to focus it, then type rk -----
if ($ok) {
    # The server places new client windows cascaded from the top-left of its
    # 1280x720 client area; the terminal is 800x500 logical. Click near its
    # center to focus, then baseline shot.
    $wr = New-Object Wm+RECT
    [void][Wm]::GetWindowRect($hwnd, [ref]$wr)
    $cw = $wr.R - $wr.L; $ch = $wr.B - $wr.T
    $sx = [int]($cw * 0.25); $sy = [int]($ch * 0.3)
    $lp = [IntPtr]((([int64]$sy) -shl 16) -bor ([int64]$sx -band 0xFFFF))
    [void][Wm]::PostMessage($hwnd, 0x200, [IntPtr]1, $lp)   # WM_LBUTTONDOWN
    [void][Wm]::PostMessage($hwnd, 0x202, [IntPtr]1, $lp)   # WM_LBUTTONUP
    Start-Sleep -Milliseconds 500
    Save-Shot $shotA

    [void][Wm]::PostVk($hwnd, 0x52)   # r
    Start-Sleep -Milliseconds 400
    [void][Wm]::PostVk($hwnd, 0x4B)   # k -> composing GA (nothing to pty)
    Start-Sleep -Milliseconds 800
    Save-Shot $shotB
    [void][Wm]::PostVk($hwnd, 0x0D)   # Enter commits -> "GA\r" to pty
    Start-Sleep -Milliseconds 900
}

Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
Stop-ProbeProcs

# --- 3. evidence -------------------------------------------------------------
Write-Host "--- client_terminal.log ([preedit] stages) ---"
Get-Content $clog -ErrorAction SilentlyContinue | Select-Object -First 80
$diff = "$build\vpt14_pixel_diff.ps1"
@"
Add-Type -AssemblyName System.Drawing
`$a = New-Object System.Drawing.Bitmap('$shotA')
`$b = New-Object System.Drawing.Bitmap('$shotB')
`$w = [Math]::Min(`$a.Width, `$b.Width); `$h = [Math]::Min(`$a.Height, `$b.Height)
`$diffs = 0; `$greenish = 0; `$minX = -1; `$minY = -1; `$maxX = -1; `$maxY = -1
for (`$y = 0; `$y -lt `$h; `$y += 2) {
  for (`$x = 0; `$x -lt `$w; `$x += 2) {
    `$pa = `$a.GetPixel(`$x, `$y); `$pb = `$b.GetPixel(`$x, `$y)
    if (`$pa.R -ne `$pb.R -or `$pa.G -ne `$pb.G -or `$pa.B -ne `$pb.B) {
      `$diffs++
      if (`$minX -lt 0) { `$minX = `$x; `$minY = `$y }
      if (`$x -gt `$maxX) { `$maxX = `$x }
      if (`$y -gt `$maxY) { `$maxY = `$y }
      if (`$pb.G -gt 120 -and `$pb.G -gt (`$pb.R + 40)) { `$greenish++ }
    }
  }
}
Write-Host ('diff pixels: ' + `$diffs + ' greenish: ' + `$greenish + ' bbox: (' + `$minX + ',' + `$minY + ')-(' + `$maxX + ',' + `$maxY + ')')
"@ | Set-Content -Path $diff -Encoding ASCII
if (Test-Path $shotA) {
    & powershell -NoProfile -ExecutionPolicy Bypass -File $diff
}
if ($ok) { Write-Host "PASS: vpt14 client rig ran"; exit 0 }
Write-Host "FAIL: vpt14 client rig"; exit 1