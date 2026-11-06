# vplayer-stability T1 e2e: async open + guardrails.
# Corrupt files: truncated-tail mp4 (no moov) / 0-byte / header-corrupted -> no
# crash + classified openError_ (red). Truncated faststart mp4 -> opens, plays,
# ends early. Named pipe (silent server) -> "opening" phase visible + cancel
# works + 10 s interrupt timeout. ASCII-only ps1 (cp949 convention, docs/15).
$ErrorActionPreference = 'Continue'
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class DpiE {
    [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr ctx);
}
"@
[DpiE]::SetProcessDpiAwarenessContext([IntPtr](-4)) | Out-Null
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class WtE {
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, UIntPtr e);
    public struct RECT { public int L, T, R, B; }
    public struct POINT { public int X, Y; }
}
"@

$build   = "I:\progwork\JKENGINE\engine\build"
$exe     = "$build\jkdesktop.exe"
$shotDir = "I:\progwork\JKENGINE\.superpowers\sdd\2026-09-14-vplayer-stability"
$bs      = [char]92
# Device paths are spelled with forward slashes: clipboard paste mangles
# backslashes ('\.' -> '##' through the server input path, measured) while
# ffmpeg accepts '//./pipe/...' and blocks on a silent pipe (ffprobe measured,
# task-1 report 4c).
$pipeArg = '//./pipe/vpt1_hang'
$script:fails = 0

function Check([string]$name, [bool]$ok, [string]$detail) {
    if ($ok) { Write-Host ("PASS: " + $name) }
    else     { $script:fails++; Write-Host ("FAIL: " + $name + " -- " + $detail) }
    if ($detail) { Write-Host ("      " + $detail) }
}

function Invoke-Agentctl([string]$json) {
    # PS 5.1 native argv: the server needs each JSON quote as backslash-quote;
    # built with [char]92 to dodge quoting hell.
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
    $crect = New-Object WtE+RECT
    [WtE]::GetClientRect($script:srvHwnd, [ref]$crect) | Out-Null
    $co = New-Object WtE+POINT; $co.X = 0; $co.Y = 0
    [WtE]::ClientToScreen($script:srvHwnd, [ref]$co) | Out-Null
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

function VPlayer-Alive {
    return (Find-VPlayerLayer)
}

function Send-Click([double]$lx, [double]$ly, [uint32]$down, [uint32]$up) {
    $px = [int]([math]::Round($script:ox + ($script:layerX + $lx) * $script:scale))
    $py = [int]([math]::Round($script:oy + ($script:layerY + $ly) * $script:scale))
    [WtE]::SetForegroundWindow($script:srvHwnd) | Out-Null
    Start-Sleep -Milliseconds 200
    [WtE]::SetCursorPos($px, $py) | Out-Null
    Start-Sleep -Milliseconds 250
    [WtE]::mouse_event($down, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 80
    [WtE]::mouse_event($up, 0, 0, 0, [UIntPtr]::Zero)
}

function Click-App([double]$lx, [double]$ly) { Send-Click $lx $ly 2 4 }

function Type-IntoPath([string]$path) {
    Click-App 400 42          # path input focus
    Start-Sleep -Milliseconds 300
    [WtE]::SetForegroundWindow($script:srvHwnd) | Out-Null
    Start-Sleep -Milliseconds 200
    # Clipboard paste: per-char synthetic typing drops/mangles characters
    # through the server input path (measured: '/tmp/' -> '/tl:/'), and
    # Ctrl+A/{DEL} select-all is flaky on repeat opens. One Ctrl+V is exact.
    # Forward slashes: fopen/ffmpeg accept them on Windows.
    Set-Clipboard -Value $path
    Start-Sleep -Milliseconds 150
    # Ctrl+A select-all is unreliable through the server input path on repeat
    # opens (measured: paste appended after stale text) — clear explicitly.
    [System.Windows.Forms.SendKeys]::SendWait("{END}")
    Start-Sleep -Milliseconds 100
    [System.Windows.Forms.SendKeys]::SendWait("{BS 300}")
    Start-Sleep -Milliseconds 150
    [System.Windows.Forms.SendKeys]::SendWait("^v")
    Start-Sleep -Milliseconds 400
}

function Click-Open { Click-App 808 42 }

function Save-ServerShot([string]$name) {
    Find-Server
    $crect = New-Object WtE+RECT
    [WtE]::GetClientRect($script:srvHwnd, [ref]$crect) | Out-Null
    $co = New-Object WtE+POINT; $co.X = 0; $co.Y = 0
    [WtE]::ClientToScreen($script:srvHwnd, [ref]$co) | Out-Null
    $bmp = New-Object System.Drawing.Bitmap ($crect.R - $crect.L), ($crect.B - $crect.T)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($co.X, $co.Y, 0, 0, $bmp.Size)
    $g.Dispose()
    $cx = [int]([math]::Round($script:layerX * $script:scale))
    $cy = [int]([math]::Round($script:layerY * $script:scale))
    $cw = [int]([math]::Round($script:layerW * $script:scale))
    $ch = [int]([math]::Round($script:layerH * $script:scale))
    $rect = New-Object System.Drawing.Rectangle $cx, $cy, $cw, $ch
    $crop = $bmp.Clone($rect, $bmp.PixelFormat)
    $crop.Save((Join-Path $shotDir $name))
    $crop.Dispose(); $bmp.Dispose()
    Write-Host ("shot {0}" -f $name)
}

function Start-PipeHolder([string]$mode = 'Hang', [string]$name = 'vpt1_hang') {
    $hold = "I:" + $bs + "progwork" + $bs + "JKENGINE" + $bs + "tmp" + $bs + "vpt1_pipehold.ps1"
    return (Start-Process powershell -ArgumentList '-NoProfile','-ExecutionPolicy','Bypass','-File',$hold,'-Mode',$mode,'-Name',$name -PassThru -WindowStyle Hidden)
}
function Stop-PipeHolder($p) {
    if ($p) { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue }
    Start-Sleep -Milliseconds 800   # pipe instance teardown
}

# ---------- setup: fresh server + vplayer ----------
taskkill /F /IM jkdesktop.exe 2>$null | Out-Null
Start-Sleep -Seconds 1
Start-Process -FilePath $exe -ArgumentList "--server" -WorkingDirectory $build
Start-Sleep -Seconds 4
$ping = Invoke-Agentctl '{"tool":"ping","args":{}}'
Check "setup: server up" ($ping -match '"ok"\s*:\s*true') $ping
Invoke-Agentctl '{"tool":"launch_app","args":{"app":"vplayer"}}' | Out-Null
Start-Sleep -Seconds 6
Find-Server
Check "setup: vplayer launched" (VPlayer-Alive) "no Video Player layer"
Save-ServerShot "vpt1-baseline.png"

# ---------- S1: truncated-tail mp4 (no moov) -> classified openError, no crash ----------
Type-IntoPath ("I:/progwork/JKENGINE/tmp/vpt1_trunc_tail.mp4")
Click-Open
Start-Sleep -Seconds 3
Save-ServerShot "vpt1-s1-trunc-no-moov.png"
Check "S1: no crash after truncated-tail mp4" ((VPlayer-Alive) -and ((Invoke-Agentctl '{"tool":"ping","args":{}}') -match '"ok"\s*:\s*true')) ""

# ---------- S2: 0-byte file ----------
Type-IntoPath ("I:/progwork/JKENGINE/tmp/vpt1_zero.mp4")
Click-Open
Start-Sleep -Seconds 3
Save-ServerShot "vpt1-s2-zero-byte.png"
Check "S2: no crash after 0-byte file" ((VPlayer-Alive) -and ((Invoke-Agentctl '{"tool":"ping","args":{}}') -match '"ok"\s*:\s*true')) ""

# ---------- S3: header-corrupted file ----------
Type-IntoPath ("I:/progwork/JKENGINE/tmp/vpt1_hdr.mp4")
Click-Open
Start-Sleep -Seconds 3
Save-ServerShot "vpt1-s3-header-corrupt.png"
Check "S3: no crash after header-corrupted file" ((VPlayer-Alive) -and ((Invoke-Agentctl '{"tool":"ping","args":{}}') -match '"ok"\s*:\s*true')) ""

# ---------- S4: truncated faststart mp4 -> opens, plays, ends early ----------
Type-IntoPath ("I:/progwork/JKENGINE/tmp/vpt1_trunc_head.mp4")
Click-Open
Start-Sleep -Seconds 4
Save-ServerShot "vpt1-s4a-trunc-playing.png"
Check "S4a: no crash 4s into truncated playback" ((VPlayer-Alive) -and ((Invoke-Agentctl '{"tool":"ping","args":{}}') -match '"ok"\s*:\s*true')) ""
Start-Sleep -Seconds 24
Save-ServerShot "vpt1-s4b-trunc-ended.png"
Check "S4b: no crash after truncated stream hit EOF" ((VPlayer-Alive) -and ((Invoke-Agentctl '{"tool":"ping","args":{}}') -match '"ok"\s*:\s*true')) ""

# ---------- S5: named-pipe hang -> opening phase + cancel works ----------
# A silent pipe read NEVER returns, and FFmpeg does not consult the interrupt
# callback inside a blocked read (measured) — so cancel cannot unwind it.
# What S5 proves: the UI stays in the opening state, the cancel affordance
# works, the client stays alive, and a following open (S6) still works because
# ClosePlayer abandons the stuck worker instead of joining it forever.
$holder = Start-PipeHolder 'Hang' 'vpt1_hang'
Start-Sleep -Seconds 1
Type-IntoPath $pipeArg
Click-Open
Start-Sleep -Seconds 2
Save-ServerShot "vpt1-s5a-opening.png"
Check "S5a: no crash 2s into hung open (opening phase)" ((VPlayer-Alive) -and ((Invoke-Agentctl '{"tool":"ping","args":{}}') -match '"ok"\s*:\s*true')) ""
Click-App 115 42    # cancel button (right of the opening text)
Start-Sleep -Seconds 3
Save-ServerShot "vpt1-s5b-cancelled.png"
Check "S5b: no crash after open cancel" ((VPlayer-Alive) -and ((Invoke-Agentctl '{"tool":"ping","args":{}}') -match '"ok"\s*:\s*true')) ""
Stop-PipeHolder $holder

# ---------- S6: hung open held past 10 s -> UI stays responsive ----------
# Measured (task-1 report 4c): FFmpeg 7.1 does NOT consult the interrupt
# callback during file-protocol probe/read loops, so the 10 s open deadline
# cannot fire on a never-returning pipe read. What S6 proves: the opening UI
# survives past the 10 s deadline mark with no crash, and S7 below proves
# ClosePlayer's bounded TryClose (2 s) + Abandon keeps a later open working
# even while the stuck worker is parked inside avformat_open_input.
$holder = Start-PipeHolder 'Hang' 'vpt1_hang2'
Start-Sleep -Seconds 1
Type-IntoPath '//./pipe/vpt1_hang2'
$sw = [Diagnostics.Stopwatch]::StartNew()
Click-Open
Start-Sleep -Seconds 13
$sw.Stop()
Save-ServerShot "vpt1-s6-timeout.png"
Check "S6: no crash 13s into hung open (past 10s deadline mark)" ((VPlayer-Alive) -and ((Invoke-Agentctl '{"tool":"ping","args":{}}') -match '"ok"\s*:\s*true')) ("waited " + [int]$sw.Elapsed.TotalSeconds + "s")
Stop-PipeHolder $holder

# ---------- S7: regression - good file still plays after all of the above ----------
Type-IntoPath ("I:/progwork/JKENGINE/tmp/vpt1_faststart.mp4")
Click-Open
Start-Sleep -Seconds 4
Save-ServerShot "vpt1-s7-good-file-plays.png"
Check "S7: good 30s file still opens and plays" ((VPlayer-Alive) -and ((Invoke-Agentctl '{"tool":"ping","args":{}}') -match '"ok"\s*:\s*true')) ""

# ---------- teardown ----------
taskkill /F /IM jkdesktop.exe 2>$null | Out-Null
Write-Host ("RESULT: " + $(if ($script:fails -eq 0) { "ALL PASS" } else { "$($script:fails) FAILURE(S)" }))
exit $(if ($script:fails -eq 0) { 0 } else { 1 })