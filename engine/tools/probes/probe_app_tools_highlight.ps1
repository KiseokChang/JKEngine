# probe_app_tools_highlight.ps1 - approval target highlight (spec
# 2026-09-19-app-tool-hub sec 5 step 1, task 8). ASCII-only (PS5.1). Checks:
#   c1 server up (VISIBLE window - CopyFromScreen capture, vptdiag idiom)
#   c2 vplayer spawned + 6 tools registered + windowId>0
#   c3 clip opened (same vpt2_test.mp4 the other probes use)
#   c4 baseline shot: amber fraction of the layer top band is low (video only)
#   c5 park: permissions {"app_tool.vplayer.play_pause":"ask"} -> raw-pipe
#      subscriber query -> agent.approval_request (kind=app_tool, target_id>0)
#   c6 highlight present: amber band fraction > 0.5 AND the left ring column
#      is amber (3 stroked rects = 3px) - server draws ring+banner on the layer
#   c7 banner text rendered: dark glyph pixels present inside the amber band
#   c8 cross approve (agentctl, different conn) -> approved:true + parked reply
#   c9 highlight gone: fresh shot amber fraction back under 0.3
#   c10 cleanup: permissions.json restored verbatim + vplayer/server closed
# Conventions: probe_app_tools.ps1 (stale-process kill, permissions backup +
# stale-residue guard, ProcessStartInfo raw command line, Read-Frame pipe
# reader) + vpt12_fullscreen.ps1 (live desktop: minimize our console, pin the
# server TOPMOST, SetProcessDPIAware, list_windows logical rect -> screen px).
$ErrorActionPreference = "Continue"
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class Wtah {
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
    [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr a, int x, int y, int cx, int cy, uint f);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int cmd);
    [DllImport("kernel32.dll")] public static extern IntPtr GetConsoleWindow();
    public struct RECT { public int L, T, R, B; }
    public struct POINT { public int X, Y; }
}
"@
[Wtah]::SetProcessDPIAware() | Out-Null
# The user's desktop is LIVE while this runs (vpt12 lesson): our own console
# overlaps would pollute the pixel shots - minimize it and pin the server
# TOPMOST before every capture.
[Wtah]::ShowWindow([Wtah]::GetConsoleWindow(), 6) | Out-Null   # SW_MINIMIZE

$exe = "I:\progwork\JKENGINE\engine\build\jkdesktop.exe"
$root = Split-Path $exe
$clip = "I:/progwork/JKENGINE/tmp/vpt2_test.mp4"
$permFile = Join-Path $root "permissions.json"
$shotDir = "I:\progwork\JKENGINE\.superpowers\sdd\2026-09-19-app-tool-hub\shots"
New-Item -ItemType Directory -Force -Path $shotDir | Out-Null
$script:fail = 0

function Check([string]$name, [bool]$ok, [string]$detail) {
    if ($ok) { Write-Host ("PASS: " + $name) }
    else { $script:fail++; Write-Host ("FAIL: " + $name + " -- " + $detail) }
    if ($detail) { Write-Host ("      " + $detail) }
}

# --- permissions.json is probe-owned state (probe_app_tools idiom): per-process
# backup + stale-residue guard (a killed earlier run must never get adopted as
# user state) + verbatim restore in finally.
$stale = Get-ChildItem (Join-Path $env:TEMP "perm_pre_apptoolhl_*.json") -ErrorAction SilentlyContinue
if ($stale) {
    Write-Host ("FAIL: setup-stale-residue -- " + (($stale | ForEach-Object { $_.Name }) -join ", "))
    Write-Host "      stale residue from a killed run - restore engine/build/permissions.json manually, delete the leftover(s) in TEMP, re-run"
    exit 1
}
$permBakFile = Join-Path $env:TEMP ("perm_pre_apptoolhl_" + $PID + ".json")
$hadPerm = Test-Path $permFile
if ($hadPerm) { Copy-Item $permFile $permBakFile -Force }
function Set-Perms([string]$json) {
    [IO.File]::WriteAllText($permFile, $json, (New-Object System.Text.UTF8Encoding($false)))
}
function Clear-Perms { Remove-Item $permFile -ErrorAction SilentlyContinue }
Clear-Perms

# --- agentctl via raw command-line assembly (PS5.1 argv re-parsing trap) -----
function Invoke-Agentctl([string]$json) {
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $exe
    $psi.Arguments = 'agentctl "' + ($json -replace '"', '\"') + '"'
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.CreateNoWindow = $true
    $p = [System.Diagnostics.Process]::Start($psi)
    $out = $p.StandardOutput.ReadToEnd()
    $p.WaitForExit()
    return ($out -replace '(?m)^\[theme\][^\r\n]*[\r\n]*', '')
}

# --- raw named pipe client (probe_app_tools frame idiom) ----------------------
function SendMsg([System.IO.Pipes.NamedPipeClientStream]$s, [int]$type, [byte[]]$payload) {
    $hdr = New-Object byte[] 12
    [BitConverter]::GetBytes([uint32]0x4A4B0001).CopyTo($hdr, 0)
    [BitConverter]::GetBytes([uint32]$type).CopyTo($hdr, 4)
    [BitConverter]::GetBytes([uint32]$payload.Length).CopyTo($hdr, 8)
    $s.Write($hdr, 0, 12)
    if ($payload.Length -gt 0) { $s.Write($payload, 0, $payload.Length) }
    $s.Flush()
}
function New-Pipe([int]$subscriber) {
    $p = New-Object System.IO.Pipes.NamedPipeClientStream(".", "JKWindowServerPipe",
        [System.IO.Pipes.PipeDirection]::InOut)
    $p.Connect(5000)
    $hello = New-Object byte[] 8
    [BitConverter]::GetBytes([uint32]2).CopyTo($hello, 0)
    [BitConverter]::GetBytes([uint32]$PID).CopyTo($hello, 4)
    SendMsg $p 1 $hello
    $sub = New-Object byte[] 4
    [BitConverter]::GetBytes([uint32]$subscriber).CopyTo($sub, 0)
    SendMsg $p 19 $sub
    return $p
}
function SendQuery([System.IO.Pipes.NamedPipeClientStream]$s, [uint32]$qid, [string]$json) {
    $body = [Text.Encoding]::UTF8.GetBytes($json)
    $payload = New-Object byte[] (8 + $body.Length)
    [BitConverter]::GetBytes([uint32]$qid).CopyTo($payload, 0)
    [BitConverter]::GetBytes([uint32]$body.Length).CopyTo($payload, 4)
    [Array]::Copy($body, 0, $payload, 8, $body.Length)
    SendMsg $s 17 $payload
}
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class PipePeek2 {
    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool PeekNamedPipe(IntPtr hPipe, byte[] buf,
        int bufSize, out int read, out int avail, out int left);
}
"@
function Pipe-Avail([System.IO.Pipes.NamedPipeClientStream]$s) {
    $read = 0; $avail = 0; $left = 0
    try {
        $h = $s.SafePipeHandle.DangerousGetHandle()
        if (-not [PipePeek2]::PeekNamedPipe($h, $null, 0, [ref]$read, [ref]$avail, [ref]$left)) { return -1 }
    } catch { return -1 }
    return $avail
}
function Read-Frame([System.IO.Pipes.NamedPipeClientStream]$s, [int]$timeoutMs) {
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    while ((Pipe-Avail $s) -lt 12) {
        if ($sw.ElapsedMilliseconds -gt $timeoutMs) { return $null }
        Start-Sleep -Milliseconds 10
    }
    $hdr = New-Object byte[] 12
    if ($s.Read($hdr, 0, 12) -ne 12) { return $null }
    $type = [int][BitConverter]::ToUInt32($hdr, 4)
    $len = [int][BitConverter]::ToUInt32($hdr, 8)
    if ($len -lt 0 -or $len -gt (4 * 1024 * 1024)) { return $null }
    while ((Pipe-Avail $s) -lt $len) {
        if ($sw.ElapsedMilliseconds -gt $timeoutMs) { return $null }
        Start-Sleep -Milliseconds 10
    }
    $pl = New-Object byte[] $len
    $off = 0
    while ($off -lt $len) {
        $a = Pipe-Avail $s
        if ($a -le 0) { return $null }
        $n = [Math]::Min($a, $len - $off)
        $r = $s.Read($pl, $off, $n)
        if ($r -le 0) { return $null }
        $off += $r
    }
    $hs = 12
    if ($type -eq 20 -or $type -eq 22) { $hs = 4 }
    elseif ($type -eq 17 -or $type -eq 23) { $hs = 8 }
    $text = ""
    if ($len -gt $hs) { $text = [Text.Encoding]::UTF8.GetString($pl, $hs, $len - $hs) }
    return @{ type = $type; len = $len; text = $text }
}

# --- screen capture (vptdiag idiom): server window client rect + list_windows
# logical rect -> screen pixels ----------------------------------------------
$script:srvHwnd = [IntPtr]::Zero
$script:scale = 1.0; $script:ox = 0; $script:oy = 0
$script:layerX = 0; $script:layerY = 0; $script:layerW = 960; $script:layerH = 640

function Find-Server {
    $h = (Get-Process jkdesktop | Where-Object { $_.MainWindowHandle -ne 0 } |
        Select-Object -First 1).MainWindowHandle
    if (-not $h) { return $false }
    $script:srvHwnd = [IntPtr]$h
    [Wtah]::SetWindowPos($script:srvHwnd, [IntPtr](-1), 0, 0, 0, 0, 0x0003) | Out-Null  # TOPMOST
    $crect = New-Object Wtah+RECT
    [Wtah]::GetClientRect($script:srvHwnd, [ref]$crect) | Out-Null
    $co = New-Object Wtah+POINT; $co.X = 0; $co.Y = 0
    [Wtah]::ClientToScreen($script:srvHwnd, [ref]$co) | Out-Null
    $script:scale = ($crect.R - $crect.L) / 1280.0
    $script:ox = $co.X; $script:oy = $co.Y
    return ($script:scale -gt 0)
}
function Find-VPlayerLayer {
    $list = Invoke-Agentctl '{"tool":"list_windows","args":{}}'
    $mid = [regex]::Match($list, '"id":(\d+),"title":"Video Player"')
    if (-not $mid.Success) { return $false }
    $script:vplayerId = [int]$mid.Groups[1].Value
    $m = [regex]::Match($list, '"title":"Video Player".*?"x":(-?\d+),"y":(-?\d+),"w":(\d+),"h":(\d+)')
    if (-not $m.Success) { return $false }
    $script:layerX = [int]$m.Groups[1].Value
    $script:layerY = [int]$m.Groups[2].Value
    $script:layerW = [int]$m.Groups[3].Value
    $script:layerH = [int]$m.Groups[4].Value
    return ($script:layerW -gt 0 -and $script:layerH -gt 0)
}
function Refresh-Geom {
    if (-not (Find-Server)) { return $false }
    for ($i = 0; $i -lt 10; $i++) {
        if (Find-VPlayerLayer) { return $true }
        Start-Sleep -Milliseconds 300
    }
    return $false
}

# Capture the vplayer layer region off the live screen and return the pixel
# stats the checks judge: amber-ish fraction of the whole layer, of the top
# band (the banner strip, top 24 logical pts), of the left ring column, and
# dark (glyph) pixel count inside the band. Amber = spec color (230,140,40),
# tolerance 40 per channel; dark = r+g+b < 180 inside the amber band.
function Sample-Layer([string]$tag) {
    $sx = [int]([math]::Round($script:ox + $script:layerX * $script:scale))
    $sy = [int]([math]::Round($script:oy + $script:layerY * $script:scale))
    $w = [int][math]::Round($script:layerW * $script:scale)
    $h = [int][math]::Round($script:layerH * $script:scale)
    if ($w -lt 40) { $w = 40 }
    if ($h -lt 40) { $h = 40 }
    $bmp = New-Object System.Drawing.Bitmap $w, $h
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($sx, $sy, 0, 0, (New-Object System.Drawing.Size $w, $h))
    $g.Dispose()
    $rect = New-Object System.Drawing.Rectangle 0, 0, $w, $h
    $bd = $bmp.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly,
        [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $bytes = New-Object byte[] ($bd.Stride * $bd.Height)
    [System.Runtime.InteropServices.Marshal]::Copy($bd.Scan0, $bytes, 0, $bytes.Length)
    $bmp.UnlockBits($bd)
    $bmp.Save((Join-Path $shotDir ("highlight_" + $tag + ".png")), [System.Drawing.Imaging.ImageFormat]::Png) | Out-Null
    $bmp.Dispose()
    $bandH = [int][math]::Round(24 * $script:scale)
    if ($bandH -lt 6) { $bandH = 6 }
    if ($bandH -gt $h) { $bandH = $h }
    $amberAll = 0; $amberBand = 0; $bandTotal = 0; $darkBand = 0; $amberCol = 0; $colTotal = 0
    for ($y = 0; $y -lt $h; $y += 2) {
        $row = $y * $bd.Stride
        $rowHasLeftRing = $false
        # left-edge ring scan first (x 0..7, stride 1) - the ring is a 3px
        # stroke and the CopyFromScreen origin can sit ~1px off the layer edge,
        # so a fixed column grid straddles it.
        for ($x = 0; $x -lt 8; $x++) {
            $i = $row + $x * 4
            $r = [int]$bytes[$i + 2]; $gg = [int]$bytes[$i + 1]; $b = [int]$bytes[$i]
            if ([Math]::Abs($r - 230) -le 40 -and [Math]::Abs($gg - 140) -le 40 -and [Math]::Abs($b - 40) -le 40) {
                $rowHasLeftRing = $true
                break
            }
        }
        if ($rowHasLeftRing) { $amberCol++ }
        $colTotal++
        for ($x = 0; $x -lt $w; $x += 2) {
            $i = $row + $x * 4
            # 32bpp ARGB in memory = B,G,R,A little-endian - R is i+2.
            $r = [int]$bytes[$i + 2]; $gg = [int]$bytes[$i + 1]; $b = [int]$bytes[$i]
            $isAmber = ([Math]::Abs($r - 230) -le 40 -and [Math]::Abs($gg - 140) -le 40 -and [Math]::Abs($b - 40) -le 40)
            if ($isAmber) { $amberAll++ }
            if ($y -lt $bandH) {
                $bandTotal++
                if ($isAmber) { $amberBand++ }
                if (($r + $gg + $b) -lt 180) { $darkBand++ }
            }
        }
    }
    return @{
        amberAll = $amberAll
        amberBand = $amberBand
        bandFrac = $(if ($bandTotal -gt 0) { [double]$amberBand / [double]$bandTotal } else { [double]0 })
        darkBand = $darkBand
        colFrac = $(if ($colTotal -gt 0) { [double]$amberCol / [double]$colTotal } else { [double]0 })
        w = $w; h = $h
    }
}

# --- server lifecycle: stale kill, VISIBLE start (capture needs a real window)
Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force
Get-Process jkapp_vplayer -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1
Start-Process -FilePath $exe -ArgumentList "--server" -WorkingDirectory $root
$up = $false
foreach ($i in 1..30) {
    Start-Sleep -Milliseconds 500
    if ((Invoke-Agentctl '{"tool":"ping","args":{}}') -match '"ok"\s*:\s*true') { $up = $true; break }
}
Check "c1-server-up" $up ""
if (-not (Find-Server)) { Check "c1-server-window" $false "no jkdesktop main window" }

try {
    # ---- c2: vplayer registered ----------------------------------------------
    Invoke-Agentctl '{"tool":"launch_app","args":{"app":"vplayer"}}' | Out-Null
    $cat = ""
    foreach ($i in 1..30) {
        Start-Sleep -Milliseconds 500
        $cat = Invoke-Agentctl '{"tool":"list_app_tools","args":{}}'
        if (([regex]::Matches($cat, '"app":"vplayer"').Count) -eq 6) { break }
    }
    $rows = [regex]::Matches($cat, '"app":"vplayer"').Count
    Check "c2-catalog-6-rows" ($rows -eq 6) ("rows=$rows")
    $wid = 0
    $m = [regex]::Match($cat, '"windowId":(\d+)')
    if ($m.Success) { $wid = [int]$m.Groups[1].Value }
    Check "c2-windowId>0" ($wid -gt 0) ("windowId=$wid (approval targetId)")
    $geomOk = (Refresh-Geom) -and ($script:layerW -gt 60) -and ($script:layerH -gt 60)
    Check "c2-layer-geom" $geomOk ("layer=$($script:layerX),$($script:layerY) $($script:layerW)x$($script:layerH) scale=$script:scale")

    # ---- c3: clip opened -------------------------------------------------------
    $op = (Invoke-Agentctl ('{"tool":"app_tool","args":{"app":"vplayer","tool":"open","args":{"path":"' + $clip + '"}}}'))
    $opened = $false
    foreach ($i in 1..25) {
        Start-Sleep -Milliseconds 400
        $st = Invoke-Agentctl '{"tool":"app_tool","args":{"app":"vplayer","tool":"get_status","args":{}}}'
        if ($st -match '"opened":true') { $opened = $true; break }
    }
    Check "c3-clip-opened" $opened "$op / $st"

    # ---- c4: baseline (no parking) - amber fraction of the band is low ---------
    Start-Sleep -Milliseconds 500
    $pre = Sample-Layer "pre"
    Check "c4-baseline-no-amber" ($pre.bandFrac -lt 0.3 -and $pre.colFrac -lt 0.5) ("band=$([math]::Round($pre.bandFrac,3)) col=$([math]::Round($pre.colFrac,3))")

    # ---- c5: park app_tool play_pause (ask) ------------------------------------
    Set-Perms '{"app_tool.vplayer.play_pause":"ask"}'
    $connA = New-Pipe 1   # subscriber first (lesson 28) - the ask parks
    SendQuery $connA 901 '{"tool":"app_tool","args":{"app":"vplayer","tool":"play_pause","args":{}}}'
    $reqId = 0; $evt = ""
    foreach ($i in 1..50) {
        $f = Read-Frame $connA 400
        if ($f -and $f.type -eq 20 -and $f.text -match '"kind":"app_tool"' -and
            $f.text -match '"name":"vplayer\.play_pause"') {
            $mm = [regex]::Match($f.text, '"request":(\d+)')
            if ($mm.Success) { $reqId = [int]$mm.Groups[1].Value; $evt = $f.text }
            break
        }
    }
    $tid = 0
    $mm = [regex]::Match($evt, '"target_id":(\d+)')
    if ($mm.Success) { $tid = [int]$mm.Groups[1].Value }
    Check "c5-approval-request" ($reqId -gt 0 -and $tid -gt 0) ("req=$reqId target=$tid evt=$evt")

    # ---- c6: highlight present -------------------------------------------------
    Start-Sleep -Milliseconds 400   # the composite paints it the same frame
    $during = Sample-Layer "during"
    Check "c6-amber-band" ($during.bandFrac -gt 0.5) ("band=$([math]::Round($during.bandFrac,3)) (pre=$([math]::Round($pre.bandFrac,3)))")
    Check "c6-amber-ring-column" ($during.colFrac -gt 0.8) ("col=$([math]::Round($during.colFrac,3))")

    # ---- c7: banner text glyphs inside the band --------------------------------
    Check "c7-banner-text-pixels" ($during.darkBand -gt 20) ("dark=$($during.darkBand) bandPx=$($during.bandFrac)")

    # ---- c8: cross approve (agentctl = a different connection) -----------------
    $ap = (Invoke-Agentctl ('{"tool":"approve","args":{"request":' + $reqId + ',"decision":"allow"}}'))
    Check "c8-approve-ack" ($ap -match '"approved":true') $ap
    $parked = ""
    foreach ($i in 1..50) {
        $f = Read-Frame $connA 400
        if ($f -and $f.type -eq 18 -and $f.text -match '"result"') { $parked = $f.text; break }
    }
    Check "c8-parked-reply" ($parked -match '"ok":true') $parked
    $connA.Dispose()

    # ---- c9: highlight gone (resolve removes the entry - auto-clear) -----------
    $gone = $false
    $after = $null
    foreach ($i in 1..20) {
        Start-Sleep -Milliseconds 300
        Refresh-Geom | Out-Null
        $after = Sample-Layer "post"
        if ($after.bandFrac -lt 0.3) { $gone = $true; break }
    }
    Check "c9-highlight-gone" ($gone -and $after.bandFrac -lt 0.3) ("band=$([math]::Round($after.bandFrac,3))")
    Clear-Perms

} finally {
    Clear-Perms
    if ($hadPerm) { Copy-Item $permBakFile $permFile -Force; Remove-Item $permBakFile -ErrorAction SilentlyContinue }
    Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force
    Get-Process jkapp_vplayer -ErrorAction SilentlyContinue | Stop-Process -Force
}

Write-Host ("RESULT: " + $(if ($script:fail -eq 0) { "ALL PASS" } else { "$($script:fail) FAILURE(S)" }))
exit $(if ($script:fail -eq 0) { 0 } else { 1 })