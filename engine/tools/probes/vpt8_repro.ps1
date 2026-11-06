# vpt8: reproduce the user's "basic playback broken" report on their real file:
#   I:\Addicted to the pain - Velvet Desires by Melvin and Gatica [aYoWDTfIA9U].mp4
# (3840x2160 @ 50 fps AV1 + opus). No seek, no wheel — plain open + play,
# exactly the reported scenario. Server stderr is redirected to a log; client
# (vplayer) stderr lands in the same pipe, so the av_log spam is captured.
# Harness recipe = vpt4_e2e.ps1 (DPI-aware, clipboard typing, measured geometry).
# -STA required (Set-Clipboard).
param()
$ErrorActionPreference = 'Continue'
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class Wt8 {
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, int d, UIntPtr e);
    public struct RECT { public int L, T, R, B; }
    public struct POINT { public int X, Y; }
}
"@
[Wt8]::SetProcessDPIAware() | Out-Null

$build   = "I:\progwork\JKENGINE\engine\build"
$exe     = "$build\jkdesktop.exe"
$shotDir = "I:\progwork\JKENGINE\.superpowers\sdd\2026-09-14-vplayer-stability"
$mp4     = "I:\Addicted to the pain - Velvet Desires by Melvin and Gatica [aYoWDTfIA9U].mp4"
$errLog  = Join-Path $env:TEMP "vpt8-server-stderr.log"
$script:fails = 0

function Check([string]$name, [bool]$ok, [string]$detail) {
    if ($ok) { Write-Host ("PASS: " + $name) }
    else     { $script:fails++; Write-Host ("FAIL: " + $name + " -- " + $detail) }
    if ($detail) { Write-Host ("      " + $detail) }
}

function Invoke-Agentctl([string]$json) {
    $escaped = $json.Replace('"', [string][char]92 + '"')
    return (& $exe agentctl $escaped 2>&1) -join "`n"
}

$script:srvHwnd = [IntPtr]::Zero
$script:scale = 1.0; $script:ox = 0; $script:oy = 0
$script:layerX = 0; $script:layerY = 0; $script:layerW = 960; $script:layerH = 640

function Find-Server {
    $h = (Get-Process jkdesktop | Where-Object { $_.MainWindowHandle -ne 0 } |
        Select-Object -First 1).MainWindowHandle
    if (-not $h) { Write-Host "FAIL: NO SERVER WINDOW"; exit 1 }
    $script:srvHwnd = [IntPtr]$h
    $crect = New-Object Wt8+RECT
    [Wt8]::GetClientRect($script:srvHwnd, [ref]$crect) | Out-Null
    $co = New-Object Wt8+POINT; $co.X = 0; $co.Y = 0
    [Wt8]::ClientToScreen($script:srvHwnd, [ref]$co) | Out-Null
    $script:scale = ($crect.R - $crect.L) / 1280.0
    $script:ox = $co.X; $script:oy = $co.Y
}

function Find-VPlayerLayer {
    $list = Invoke-Agentctl '{"tool":"list_windows","args":{}}'
    $m = [regex]::Match($list, '"title":"Video Player".*?"x":(-?\d+),"y":(-?\d+),"w":(\d+),"h":(\d+)')
    if (-not $m.Success) { return $false }
    $script:layerX = [int]$m.Groups[1].Value
    $script:layerY = [int]$m.Groups[2].Value
    $script:layerW = [int]$m.Groups[3].Value
    $script:layerH = [int]$m.Groups[4].Value
    return $true
}
function VPlayer-Alive { return (Find-VPlayerLayer) }
function Refresh-Geom {
    Find-Server
    for ($i = 0; $i -lt 10; $i++) { if (Find-VPlayerLayer) { return $true }; Start-Sleep -Milliseconds 300 }
    return $false
}

function Set-LayerCursor([double]$lx, [double]$ly) {
    $px = [int]([math]::Round($script:ox + ($script:layerX + $lx) * $script:scale))
    $py = [int]([math]::Round($script:oy + ($script:layerY + $ly) * $script:scale))
    [Wt8]::SetCursorPos($px, $py) | Out-Null
}

function Send-Click([double]$lx, [double]$ly) {
    Refresh-Geom | Out-Null
    [Wt8]::SetForegroundWindow($script:srvHwnd) | Out-Null
    Start-Sleep -Milliseconds 200
    Set-LayerCursor $lx $ly
    Start-Sleep -Milliseconds 250
    [Wt8]::mouse_event(2, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 80
    [Wt8]::mouse_event(4, 0, 0, 0, [UIntPtr]::Zero)
}

function Type-IntoPath([string]$path) {
    Send-Click 400 42
    Start-Sleep -Milliseconds 300
    [Wt8]::SetForegroundWindow($script:srvHwnd) | Out-Null
    Start-Sleep -Milliseconds 200
    for ($i = 0; $i -lt 5; $i++) {
        try { Set-Clipboard -Value $path; break } catch {
            try { [System.Windows.Forms.Clipboard]::SetText($path); break } catch {
                Start-Sleep -Milliseconds 700 }
        }
    }
    Start-Sleep -Milliseconds 150
    [System.Windows.Forms.SendKeys]::SendWait("{END}")
    Start-Sleep -Milliseconds 100
    [System.Windows.Forms.SendKeys]::SendWait("{BS 300}")
    Start-Sleep -Milliseconds 150
    [System.Windows.Forms.SendKeys]::SendWait("^v")
    Start-Sleep -Milliseconds 400
}

function Save-ServerShot([string]$name) {
    Refresh-Geom | Out-Null
    $crect = New-Object Wt8+RECT
    [Wt8]::GetClientRect($script:srvHwnd, [ref]$crect) | Out-Null
    $co = New-Object Wt8+POINT; $co.X = 0; $co.Y = 0
    [Wt8]::ClientToScreen($script:srvHwnd, [ref]$co) | Out-Null
    $bmp = New-Object System.Drawing.Bitmap ($crect.R - $crect.L), ($crect.B - $crect.T)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($co.X, $co.Y, 0, 0, $bmp.Size)
    $g.Dispose()
    $cx = [int]([math]::Round($script:layerX * $script:scale))
    $cy = [int]([math]::Round($script:layerY * $script:scale))
    $cw = [int]([math]::Round($script:layerW * $script:scale))
    $ch = [int]([math]::Round($script:layerH * $script:scale))
    if ($cx + $cw -gt $bmp.Width) { $cw = $bmp.Width - $cx }
    if ($cy + $ch -gt $bmp.Height) { $ch = $bmp.Height - $cy }
    $rect = New-Object System.Drawing.Rectangle $cx, $cy, $cw, $ch
    $crop = $bmp.Clone($rect, $bmp.PixelFormat)
    $crop.Save((Join-Path $shotDir $name))
    $crop.Dispose(); $bmp.Dispose()
    Write-Host ("shot {0}" -f $name)
}

# ---------- setup ----------
taskkill /F /IM jkdesktop.exe 2>$null | Out-Null
Start-Sleep -Seconds 1
Remove-Item $errLog -ErrorAction SilentlyContinue
Start-Process -FilePath $exe -ArgumentList "--server" -WorkingDirectory $build `
    -RedirectStandardError $errLog -WindowStyle Hidden
Start-Sleep -Seconds 4
Check "setup: server up" ((Invoke-Agentctl '{"tool":"ping","args":{}}') -match '"ok"\s*:\s*true') ""
Invoke-Agentctl '{"tool":"launch_app","args":{"app":"vplayer"}}' | Out-Null
Start-Sleep -Seconds 6
Check "setup: vplayer launched" (Refresh-Geom) "no Video Player layer"

# ---------- S1: plain open + play (the reported scenario) ----------
Type-IntoPath $mp4
Send-Click 808 42            # Open button
Start-Sleep -Seconds 5
Check "S1: vplayer alive 5 s after open" (Refresh-Geom) "vplayer layer gone"
Save-ServerShot "vpt8-s1-open-5s.png"

Start-Sleep -Seconds 15
Check "S1: vplayer alive 20 s after open" (VPlayer-Alive) "vplayer layer gone"
Save-ServerShot "vpt8-s1-open-20s.png"

# ---------- stderr tally ----------
$err = Get-Content $errLog -Raw -ErrorAction SilentlyContinue
if (-not $err) { $err = "" }
$fh  = ([regex]::Matches($err, 'Error parsing frame header')).Count
$obu = ([regex]::Matches($err, 'Error parsing OBU data')).Count
$poc = ([regex]::Matches($err, 'co located POCs')).Count
$opusW = ([regex]::Matches($err, 'Could not update timestamps')).Count
$openErr = ([regex]::Matches($err, '\[vplayer\]')).Count
Write-Host ("stderr: frame-header={0} obu={1} poc={2} opus-ts-warn={3} vplayer-lines={4}" -f $fh, $obu, $poc, $opusW, $openErr)
# Show the first 25 stderr lines around playback start for context.
$lines = $err -split "`n"
$firstErrIdx = -1
for ($i = 0; $i -lt $lines.Count; $i++) {
    if ($lines[$i] -match 'libdav1d|h264|opus|vplayer') { $firstErrIdx = $i; break }
}
if ($firstErrIdx -ge 0) {
    $from = [math]::Max(0, $firstErrIdx - 3)
    $to   = [math]::Min($lines.Count - 1, $firstErrIdx + 25)
    Write-Host "--- stderr context ---"
    $lines[$from..$to] | ForEach-Object { Write-Host $_ }
} else {
    Write-Host "--- no decoder stderr lines at all ---"
}
Check "S1: no dav1d parse errors during plain playback" ($fh -eq 0 -and $obu -eq 0) "dav1d error spam present"

# ---------- teardown ----------
taskkill /F /IM jkdesktop.exe 2>$null | Out-Null
Write-Host ("RESULT: " + $(if ($script:fails -eq 0) { "ALL PASS" } else { "$($script:fails) FAILURE(S)" }))
exit $(if ($script:fails -eq 0) { 0 } else { 1 })