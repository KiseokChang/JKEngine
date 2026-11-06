# vplayer-stability T2 e2e: seek I/O outside the state mutex + failed-seek
# clock restore. -STA required (Set-Clipboard, docs/15 cp949 convention).
#
# S1 normal mp4: repeated slider seeks — server ping responsive right after
#    each commit, clock display tracks, no crash.
# S2 rapid scrub: 6 seeks in ~2 s — coalescing/supersede path, no crash.
#    (Each synthetic click itself costs ~530 ms of deliberate sleeps, so the
#    burst budget is 8 s, not 2 s of wall clock.)
# S3 failed seek, LIVE triggers (return values measured with
# tmp/vpt2_seekret.c against these exact files):
#    - mpeg-ts over a named pipe: opens and streams, EVERY avformat_seek_file
#      fails (-1 "Operation not permitted", both directions).
#    - raw H.264 elementary stream (local): seek unsupported (-1) — but FFmpeg
#      find_stream_info consumes the whole file at this size, so playback has
#      already ended (quiet EOF) when the seek fires. Self-contained fallback
#      trigger: proves the classified one-shot notice on an ended pipeline.
#    (mp4/wav/mkv over the same pipe report seek SUCCESS — avio skip-reads —
#    so they are NOT failure triggers; measured.)
# S4 regression: back to the local mp4 — open + seek still work.
#
# Click geometry (measured): the app surface is 960x640 at layer origin
# (lx,ly)+(78,28) chrome offset in server-client pixels. Transport row y~77;
# the ##seek slider spans surface x~90..874 — clicks at surface
# x=150+690*f land on the track with true fraction ~0.08+0.88*f (distinct,
# monotonic targets; exact fractioning is irrelevant to the scenarios).
$ErrorActionPreference = 'Continue'
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class Wt2 {
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
$mp4     = "I:/progwork/JKENGINE/tmp/vpt2_test.mp4"
$raw     = "I:/progwork/JKENGINE/tmp/vpt2_raw.h264"
$pipeArg = "//./pipe/vpt2_serve"   # ffmpeg accepts this spelling for \\.\pipe\
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
    $crect = New-Object Wt2+RECT
    [Wt2]::GetClientRect($script:srvHwnd, [ref]$crect) | Out-Null
    $co = New-Object Wt2+POINT; $co.X = 0; $co.Y = 0
    [Wt2]::ClientToScreen($script:srvHwnd, [ref]$co) | Out-Null
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

function Send-Click([double]$lx, [double]$ly, [uint32]$down, [uint32]$up) {
    $px = [int]([math]::Round($script:ox + ($script:layerX + $lx) * $script:scale))
    $py = [int]([math]::Round($script:oy + ($script:layerY + $ly) * $script:scale))
    [Wt2]::SetForegroundWindow($script:srvHwnd) | Out-Null
    Start-Sleep -Milliseconds 200
    [Wt2]::SetCursorPos($px, $py) | Out-Null
    Start-Sleep -Milliseconds 250
    [Wt2]::mouse_event($down, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 80
    [Wt2]::mouse_event($up, 0, 0, 0, [UIntPtr]::Zero)
}
function Click-App([double]$lx, [double]$ly) { Send-Click $lx $ly 2 4 }

function Type-IntoPath([string]$path) {
    Click-App 400 42
    Start-Sleep -Milliseconds 300
    [Wt2]::SetForegroundWindow($script:srvHwnd) | Out-Null
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
function Click-Open { Click-App 808 42 }

function Save-ServerShot([string]$name) {
    # Crop = layer + 140 px to the right: the clock text ("MM:SS / MM:SS")
    # sits at surface x~880..950 but the surface renders offset inside the
    # layer rect, so the plain layer crop clipped it (measured).
    Find-Server
    $crect = New-Object Wt2+RECT
    [Wt2]::GetClientRect($script:srvHwnd, [ref]$crect) | Out-Null
    $co = New-Object Wt2+POINT; $co.X = 0; $co.Y = 0
    [Wt2]::ClientToScreen($script:srvHwnd, [ref]$co) | Out-Null
    $bmp = New-Object System.Drawing.Bitmap ($crect.R - $crect.L), ($crect.B - $crect.T)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($co.X, $co.Y, 0, 0, $bmp.Size)
    $g.Dispose()
    $cx = [int]([math]::Round($script:layerX * $script:scale))
    $cy = [int]([math]::Round($script:layerY * $script:scale))
    $cw = [int]([math]::Round(($script:layerW + 140) * $script:scale))
    $ch = [int]([math]::Round($script:layerH * $script:scale))
    if ($cx + $cw -gt $bmp.Width) { $cw = $bmp.Width - $cx }
    $rect = New-Object System.Drawing.Rectangle $cx, $cy, $cw, $ch
    $crop = $bmp.Clone($rect, $bmp.PixelFormat)
    $crop.Save((Join-Path $shotDir $name))
    $crop.Dispose(); $bmp.Dispose()
    Write-Host ("shot {0}" -f $name)
}

# Server+client responsiveness (agentctl round trip, ms) — the "UI never
# stalls" gate: must stay in the tens of ms while seeks run.
function Ping-Ms {
    $sw = [Diagnostics.Stopwatch]::StartNew()
    $r = Invoke-Agentctl '{"tool":"ping","args":{}}'
    $sw.Stop()
    if ($r -notmatch '"ok"\s*:\s*true') { return -1.0 }
    return $sw.Elapsed.TotalMilliseconds
}

# Slider seek: commit a click at parameter f of the ##seek slider track.
function Seek-Frac([double]$f) {
    Click-App (150.0 + 690.0 * $f) 77
}

# ---------- setup ----------
$errLog = "I:\progwork\JKENGINE\tmp\vpt2_client_stderr.log"
taskkill /F /IM jkdesktop.exe 2>$null | Out-Null
Start-Sleep -Seconds 1
Start-Process -FilePath $exe -ArgumentList "--server" -WorkingDirectory $build -RedirectStandardError $errLog
Start-Sleep -Seconds 4
Check "setup: server up" ((Invoke-Agentctl '{"tool":"ping","args":{}}') -match '"ok"\s*:\s*true') ""
Invoke-Agentctl '{"tool":"launch_app","args":{"app":"vplayer"}}' | Out-Null
Start-Sleep -Seconds 6
Find-Server
Check "setup: vplayer launched" (VPlayer-Alive) "no Video Player layer"

# ---------- S1: normal mp4, repeated seeks, UI stays responsive ----------
Type-IntoPath $mp4
Click-Open
Start-Sleep -Seconds 4
Save-ServerShot "vpt2-s1a-playing.png"
$marks = @(0.8, 0.2, 0.6, 0.35, 0.9, 0.5)
$maxPing = 0.0
foreach ($f in $marks) {
    Seek-Frac $f
    $p = Ping-Ms
    if ($p -gt $maxPing) { $maxPing = $p }
    Start-Sleep -Milliseconds 700
}
Save-ServerShot "vpt2-s1b-after-seeks.png"
Check "S1: 6 slider seeks, server ping responsive" ($maxPing -ge 0 -and $maxPing -lt 500) ("max ping {0:N0} ms" -f $maxPing)
Check "S1: no crash" (VPlayer-Alive) ""

# ---------- S2: rapid scrub (6 seeks in ~2 s of intent) ----------
$sw = [Diagnostics.Stopwatch]::StartNew()
foreach ($f in @(0.1, 0.7, 0.25, 0.85, 0.4, 0.95)) {
    Seek-Frac $f
    Start-Sleep -Milliseconds 300
}
$sw.Stop()
$p = Ping-Ms
Save-ServerShot "vpt2-s2-rapid-scrub.png"
Check "S2: rapid scrub burst completed" ($sw.Elapsed.TotalMilliseconds -lt 8000) ("{0:N0} ms (6 clicks x ~530 ms harness overhead + 6x300 ms)" -f $sw.Elapsed.TotalMilliseconds)
Check "S2: responsive after burst" ($p -ge 0 -and $p -lt 500) ("ping {0:N0} ms" -f $p)
Check "S2: no crash" (VPlayer-Alive) ""

# ---------- S3: failed seek on a live, unseekable source ----------
# mpeg-ts over a named pipe: opens and plays, but EVERY avformat_seek_file
# fails (tmp/vpt2_seekret.c on this exact stream: r=-1 "Operation not
# permitted", both directions). The worker's stderr ([vplayer] lines, incl.
# the failed-seek restore value) is captured via the server's stderr.
function Start-TsServer {
    return (Start-Process powershell -ArgumentList '-NoProfile','-ExecutionPolicy','Bypass',
        '-File','I:\progwork\JKENGINE\tmp\vpt2_serve.ps1','-Lifetime','180','-Name','vpt2_serve' -PassThru -WindowStyle Hidden)
}
$tsServer = Start-TsServer   # spawned here (not right before the paste): the
                             # spawn was observed to race the clipboard
Start-Sleep -Seconds 1
Type-IntoPath $pipeArg
Click-Open
Start-Sleep -Seconds 6
Save-ServerShot "vpt2-s3a-ts-playing.png"
Check "S3a: ts stream opens" ((VPlayer-Alive) -and ((Invoke-Agentctl '{"tool":"ping","args":{}}') -match '"ok"\s*:\s*true')) ""
Seek-Frac 0.05   # every seek on this source fails -> failed-seek path
Start-Sleep -Milliseconds 900
Save-ServerShot "vpt2-s3b-ts-seek-fail.png"
$p = Ping-Ms
Check "S3b: UI responsive right after failed seek" ($p -ge 0 -and $p -lt 500) ("ping {0:N0} ms" -f $p)
Start-Sleep -Seconds 3
Save-ServerShot "vpt2-s3c-ts-keeps-playing.png"
Check "S3c: still alive 3 s after failed seek" ((VPlayer-Alive) -and ((Invoke-Agentctl '{"tool":"ping","args":{}}') -match '"ok"\s*:\s*true')) ""
if ($tsServer) { Stop-Process -Id $tsServer.Id -Force -ErrorAction SilentlyContinue }
Start-Sleep -Milliseconds 800

# ---------- S3b: self-contained trigger — raw H.264 (seek unsupported) ----
Type-IntoPath $raw
Click-Open
Start-Sleep -Seconds 5
Seek-Frac 0.05
Start-Sleep -Milliseconds 900
Save-ServerShot "vpt2-s3d-raw-seek-fail.png"
Check "S3d: raw h264 seek failure, no crash" ((VPlayer-Alive) -and ((Invoke-Agentctl '{"tool":"ping","args":{}}') -match '"ok"\s*:\s*true')) ""

# Non-vacuous gate: the worker's [vplayer] seek-failed diagnostic (server
# stderr, inherited by the spawned clients) must have fired for BOTH failed
# seeks — the ts pipe seek and the raw h264 seek. Paste hiccups (clipboard
# races through the server input path) otherwise leave these silent.
Start-Sleep -Milliseconds 500
$diag = @(Select-String -Path $errLog -SimpleMatch '[vplayer] seek failed').Count
Check "S3: failed-seek path fired on both triggers" ($diag -ge 2) ("[vplayer] seek-failed diagnostics: $diag")

# ---------- S4: regression — local mp4 open + seek still fine ----------
Type-IntoPath $mp4
Click-Open
Start-Sleep -Seconds 4
Seek-Frac 0.5
Start-Sleep -Milliseconds 800
Save-ServerShot "vpt2-s4-regression.png"
Check "S4: mp4 open + seek still work" ((VPlayer-Alive) -and ((Invoke-Agentctl '{"tool":"ping","args":{}}') -match '"ok"\s*:\s*true')) ""

# ---------- teardown ----------
taskkill /F /IM jkdesktop.exe 2>$null | Out-Null
Write-Host ("RESULT: " + $(if ($script:fails -eq 0) { "ALL PASS" } else { "$($script:fails) FAILURE(S)" }))
exit $(if ($script:fails -eq 0) { 0 } else { 1 })