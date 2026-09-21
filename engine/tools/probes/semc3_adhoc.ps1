# semc3_adhoc.ps1 - semantic cursor Task 3 ad-hoc probe (spec
# 2026-09-22-semantic-cursor sec 5, Task 3: cursor cell + approval cell layers).
# ASCII-only (PS5.1). Harness: semc2_adhoc.ps1 raw-pipe fake apps (window
# clients) + probe_app_tools_highlight.ps1 live-desktop pixel measurement
# (CopyFromScreen on the server window client rect + list_windows logical
# rect -> screen px). Checks:
#   t3-up            server up (VISIBLE - pixels must reach the screen)
#   t3-plain-clean   undeclared window client: no accent, no amber (baseline)
#   t3-predecl-clean declared-app window, cursor NOT yet declared: no accent
#   t3-cursor-init   cursor cell outline at the declared (0,0) cell (state
#                    exists = shown), none in a far cell of the same grid
#   t3-cursor-move   <app>.move moves the outline: blue at the new cell, gone
#                    from the old cell interior
#   t3-cursor-follow window_move of the layer: outline recomputed every frame,
#                    tracks the new layer origin (client->layer->physical chain)
#   t3-park-cell     parked act draws the amber 3-ring at the FIXED parked
#                    cell rect, while the blue cursor cell stays (2 layers)
#   t3-park-gone     resolution clears the amber cell ring, cursor cell persists
#   t3-plain-unaffected  the undeclared window still shows no overlays while
#                    overlays are live on the cursor app
#   t3-decl-gone     cursor block dropped (re-register without it): accent gone,
#                    layer still alive (exclusion/teardown guard)
#   t3-capture-excl  a "Region Capture" titled client with a cursor declaration
#                    draws nothing (capture-overlay exclusion guard intact)
# Run: powershell -File semc3_adhoc.ps1 "> log 2>&1" (file redirect - lesson 42).
# Requires a restartable desktop: kills jkdesktop/jkwinserver/jkbridge, starts
# jkdesktop --server (probe-owned server instance). permissions.json is
# probe-owned state: backup + NOTICE + byte-identical restore in finally.
$ErrorActionPreference = "Continue"
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class Wt3 {
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
[Wt3]::SetProcessDPIAware() | Out-Null
[Wt3]::ShowWindow([Wt3]::GetConsoleWindow(), 6) | Out-Null   # SW_MINIMIZE

$exe = "I:\progwork\JKENGINE\engine\build\jkdesktop.exe"
$root = Split-Path $exe
$shotDir = "I:\progwork\JKENGINE\.superpowers\sdd\2026-09-22-semantic-cursor\shots"
New-Item -ItemType Directory -Force -Path $shotDir | Out-Null
$script:fail = 0
function Check([string]$name, [bool]$ok, [string]$detail) {
    if ($ok) { Write-Host ("PASS: " + $name) }
    else { $script:fail++; Write-Host ("FAIL: " + $name + " -- " + $detail) }
    if ($detail) { Write-Host ("      " + $detail) }
}

# --- agentctl (raw command line - PS5.1 argv re-parsing trap) -----------------
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

# --- raw named pipe client (semc2 idiom) --------------------------------------
function SendMsg([System.IO.Pipes.NamedPipeClientStream]$s, [int]$type, [byte[]]$payload) {
    $hdr = New-Object byte[] 12
    [BitConverter]::GetBytes([uint32]0x4A4B0001).CopyTo($hdr, 0)
    [BitConverter]::GetBytes([uint32]$type).CopyTo($hdr, 4)
    [BitConverter]::GetBytes([uint32]$payload.Length).CopyTo($hdr, 8)
    $s.Write($hdr, 0, 12)
    if ($payload.Length -gt 0) { $s.Write($payload, 0, $payload.Length) }
    $s.Flush()
}
function New-Pipe([int]$subscriber, [bool]$window, [string]$title) {
    $p = New-Object System.IO.Pipes.NamedPipeClientStream(".", "JKWindowServerPipe",
        [System.IO.Pipes.PipeDirection]::InOut)
    $p.Connect(5000)
    $hello = New-Object byte[] 8
    [BitConverter]::GetBytes([uint32]2).CopyTo($hello, 0)
    [BitConverter]::GetBytes([uint32]$PID).CopyTo($hello, 4)
    SendMsg $p 1 $hello
    if ($window) {
        # Window client (Hello + CreateSurface). SurfaceCreatePayload:
        # {int32 w, int32 h, char title[128]}.
        $w = 128; $h = 96
        $pl = New-Object byte[] (8 + 128)
        [BitConverter]::GetBytes([int32]$w).CopyTo($pl, 0)
        [BitConverter]::GetBytes([int32]$h).CopyTo($pl, 4)
        $t = [Text.Encoding]::ASCII.GetBytes($title)
        [Array]::Copy($t, 0, $pl, 8, [Math]::Min($t.Length, 128))
        SendMsg $p 3 $pl
        $created = Read-Frame $p 5000
        if ($created -eq $null -or $created.type -ne 4) {
            Write-Host ("FAIL: newpipe-surfacecreated(" + $title + ")")
            $script:fail++
        }
    }
    if ($subscriber -ne 0) {
        $sub = New-Object byte[] 4
        [BitConverter]::GetBytes([uint32]$subscriber).CopyTo($sub, 0)
        SendMsg $p 19 $sub
    }
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
function Send-ToolRegister([System.IO.Pipes.NamedPipeClientStream]$s, [string]$json) {
    $body = [Text.Encoding]::UTF8.GetBytes($json)
    $payload = New-Object byte[] (4 + $body.Length)
    [BitConverter]::GetBytes([uint32]$body.Length).CopyTo($payload, 0)
    [Array]::Copy($body, 0, $payload, 4, $body.Length)
    SendMsg $s 22 $payload
}
function Send-ToolResult([System.IO.Pipes.NamedPipeClientStream]$s, [uint32]$reqId, [int]$ok, [string]$json) {
    $body = [Text.Encoding]::UTF8.GetBytes($json)
    $payload = New-Object byte[] (12 + $body.Length)
    [BitConverter]::GetBytes([uint32]$reqId).CopyTo($payload, 0)
    [BitConverter]::GetBytes([uint32]$ok).CopyTo($payload, 4)
    [BitConverter]::GetBytes([uint32]$body.Length).CopyTo($payload, 8)
    [Array]::Copy($body, 0, $payload, 12, $body.Length)
    SendMsg $s 24 $payload
}
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class PipePeekS3 {
    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool PeekNamedPipe(IntPtr hPipe, byte[] buf,
        int bufSize, out int read, out int avail, out int left);
}
"@
function Pipe-Avail([System.IO.Pipes.NamedPipeClientStream]$s) {
    $read = 0; $avail = 0; $left = 0
    try {
        $h = $s.SafePipeHandle.DangerousGetHandle()
        if (-not [PipePeekS3]::PeekNamedPipe($h, $null, 0, [ref]$read, [ref]$avail, [ref]$left)) { return -1 }
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
    $head0 = 0
    if ($type -eq 20 -or $type -eq 22) { $hs = 4 }
    elseif ($type -eq 17 -or $type -eq 23) { $hs = 8; $head0 = [BitConverter]::ToUInt32($pl, 0) }
    $text = ""
    if ($len -gt $hs) { $text = [Text.Encoding]::UTF8.GetString($pl, $hs, $len - $hs) }
    return @{ type = $type; len = $len; text = $text; head0 = [uint32]$head0 }
}
# Register ack: subscribed pipes queue broadcasts ahead of the ack, and a
# "Region Capture" titled client additionally receives the intake ResizeSurface
# (CommitChromeResize resizes it to the desktop) - accept ONLY AgentReply
# frames (type 18), skip events and resize payloads (lesson 30).
function Read-Ack([System.IO.Pipes.NamedPipeClientStream]$s, [int]$timeoutMs) {
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    while ($sw.ElapsedMilliseconds -lt $timeoutMs) {
        $f = Read-Frame $s 300
        if ($f -ne $null -and $f.type -eq 18) { return $f }
    }
    return $null
}
# Drain an agent pipe of any queued events (bounded).
function Drain-Agent([System.IO.Pipes.NamedPipeClientStream]$s, [int]$ms) {
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    while ($sw.ElapsedMilliseconds -lt $ms) {
        $f = Read-Frame $s 100
        if ($f -eq $null) { break }
    }
}
# Park an act and return the approval_request event frame.
function Park-Act([System.IO.Pipes.NamedPipeClientStream]$agent, [uint32]$qid,
                  [string]$argsJson) {
    SendQuery $agent $qid ('{"tool":"app_tool","args":{"app":"fakegrid3","tool":"act","args":' + $argsJson + '}}')
    $ev = $null
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    while ($sw.ElapsedMilliseconds -lt 10000) {
        $f = Read-Frame $agent 300
        if ($f -ne $null -and $f.type -eq 20 -and
            $f.text -match '"topic":"agent.approval_request"') {
            $ev = $f
            break
        }
    }
    return $ev
}
# Relay path helper: read the parked agent reply, answering the app pipe's
# AgentToolCall with a fixed payload (the fake app owns the transition).
function Read-Relay([System.IO.Pipes.NamedPipeClientStream]$agent,
                    [System.IO.Pipes.NamedPipeClientStream]$app,
                    [uint32]$qid, [int]$timeoutMs, [string]$appReplyJson) {
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    while ($sw.ElapsedMilliseconds -lt $timeoutMs) {
        $f = Read-Frame $agent 200
        if ($f -ne $null) {
            if ($f.type -eq 18) { return $f.text }
            if ($f.type -eq 20) { continue }
        }
        $g = Read-Frame $app 0
        if ($g -ne $null -and $g.type -eq 23) {
            Send-ToolResult $app $g.head0 1 $appReplyJson
        }
    }
    return $null
}
function Wait-Event([System.IO.Pipes.NamedPipeClientStream]$agent,
                    [string]$needle, [int]$ms) {
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    while ($sw.ElapsedMilliseconds -lt $ms) {
        $f = Read-Frame $agent 200
        if ($f -ne $null -and $f.type -eq 20 -and $f.text -match $needle) {
            return $f
        }
    }
    return $null
}

# --- screen capture (probe_app_tools_highlight idiom) --------------------------
$script:srvHwnd = [IntPtr]::Zero
$script:scale = 1.0; $script:ox = 0; $script:oy = 0
function Find-Server {
    $h = (Get-Process jkdesktop | Where-Object { $_.MainWindowHandle -ne 0 } |
        Select-Object -First 1).MainWindowHandle
    if (-not $h) { return $false }
    $script:srvHwnd = [IntPtr]$h
    [Wt3]::SetWindowPos($script:srvHwnd, [IntPtr](-1), 0, 0, 0, 0, 0x0003) | Out-Null  # TOPMOST
    $crect = New-Object Wt3+RECT
    [Wt3]::GetClientRect($script:srvHwnd, [ref]$crect) | Out-Null
    $co = New-Object Wt3+POINT; $co.X = 0; $co.Y = 0
    [Wt3]::ClientToScreen($script:srvHwnd, [ref]$co) | Out-Null
    $script:scale = ($crect.R - $crect.L) / 1280.0
    $script:ox = $co.X; $script:oy = $co.Y
    return ($script:scale -gt 0)
}
# Logical layer rect of a title from list_windows (id included - window_move).
function Get-Layer([string]$title) {
    $list = Invoke-Agentctl '{"tool":"list_windows","args":{}}'
    $m = [regex]::Match($list, ('"title":"' + [regex]::Escape($title) + '"'))
    if (-not $m.Success) { return $null }
    $idm = [regex]::Match($list.Substring([Math]::Max(0, $m.Index - 40)), '"id":(\d+)')
    $g = [regex]::Match($list.Substring($m.Index),
        '"x":(-?\d+),"y":(-?\d+),"w":(\d+),"h":(\d+)')
    if (-not $g.Success) { return $null }
    return @{
        x = [int]$g.Groups[1].Value; y = [int]$g.Groups[2].Value
        w = [int]$g.Groups[3].Value; h = [int]$g.Groups[4].Value
        id = $(if ($idm.Success) { [int]$idm.Groups[1].Value } else { 0 })
    }
}
# Sample a screen rect for accent pixels. Accent-blue = cursor cell (0,120,212),
# amber = approval (230,140,40); tolerance 40 per channel (probe idiom).
function Sample-Region([int]$sx, [int]$sy, [int]$sw, [int]$sh, [string]$tag) {
    if ($sw -lt 4) { $sw = 4 }
    if ($sh -lt 4) { $sh = 4 }
    $bmp = New-Object System.Drawing.Bitmap $sw, $sh
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($sx, $sy, 0, 0, (New-Object System.Drawing.Size $sw, $sh))
    $g.Dispose()
    $rect = New-Object System.Drawing.Rectangle 0, 0, $sw, $sh
    $bd = $bmp.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly,
        [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $bytes = New-Object byte[] ($bd.Stride * $bd.Height)
    [System.Runtime.InteropServices.Marshal]::Copy($bd.Scan0, $bytes, 0, $bytes.Length)
    $bmp.UnlockBits($bd)
    if ($tag) {
        $bmp.Save((Join-Path $shotDir ("semc3_" + $tag + ".png")),
            [System.Drawing.Imaging.ImageFormat]::Png) | Out-Null
    }
    $bmp.Dispose()
    $blue = 0; $amber = 0; $total = 0
    for ($y = 0; $y -lt $sh; $y++) {
        $row = $y * $bd.Stride
        for ($x = 0; $x -lt $sw; $x++) {
            $i = $row + $x * 4
            # 32bpp ARGB in memory = B,G,R,A little-endian - R is i+2.
            $r = [int]$bytes[$i + 2]; $gg = [int]$bytes[$i + 1]; $b = [int]$bytes[$i]
            if ([Math]::Abs($r - 0) -le 40 -and [Math]::Abs($gg - 120) -le 40 -and
                [Math]::Abs($b - 212) -le 40) { $blue++ }
            if ([Math]::Abs($r - 230) -le 40 -and [Math]::Abs($gg - 140) -le 40 -and
                [Math]::Abs($b - 40) -le 40) { $amber++ }
            $total++
        }
    }
    return @{ blue = $blue; amber = $amber; total = $total }
}
# Sample the CLIENT-pixel rect of a layer (layer rect from list_windows,
# client px converted with the same chain the server draws with: layer rect x
# outputScale, client px x Scale x outputScale - Scale is 1 for our small
# surfaces). inset shrinks the region before sampling.
function Sample-Cell([hashtable]$layer, [int]$cx, [int]$cy, [int]$cw, [int]$ch,
                     [int]$inset, [string]$tag) {
    $sx = [int][math]::Round($script:ox + ($layer.x + $cx) * $script:scale) + $inset
    $sy = [int][math]::Round($script:oy + ($layer.y + $cy) * $script:scale) + $inset
    $sw = [int][math]::Round($cw * $script:scale) - 2 * $inset
    $sh = [int][math]::Round($ch * $script:scale) - 2 * $inset
    if ($sw -lt 2) { $sw = 2 }
    if ($sh -lt 2) { $sh = 2 }
    return (Sample-Region $sx $sy $sw $sh $tag)
}

# --- permissions.json: probe-owned state (backup + NOTICE + finally-restore) ---
$permFile = Join-Path $root "permissions.json"
$stale = Get-ChildItem (Join-Path $env:TEMP "perm_pre_semc3_*.json") -ErrorAction SilentlyContinue
if ($stale) {
    Write-Host ("FAIL: setup-stale-residue -- " + (($stale | ForEach-Object { $_.Name }) -join ", "))
    Write-Host "      stale residue from a killed run - restore engine/build/permissions.json manually, delete the leftover(s) in TEMP, re-run"
    exit 1
}
$permBakFile = Join-Path $env:TEMP ("perm_pre_semc3_" + $PID + ".json")
$hadPerm = Test-Path $permFile
if ($hadPerm) { Copy-Item $permFile $permBakFile -Force }
Write-Host "NOTICE: backing up user runtime permissions.json -> $permBakFile (restored byte-identical in finally)"
function Set-Perms([string]$json) {
    [IO.File]::WriteAllText($permFile, $json, (New-Object System.Text.UTF8Encoding($false)))
}
function Restore-Perms {
    if ($hadPerm) { Copy-Item $permBakFile $permFile -Force }
    else { Remove-Item $permFile -ErrorAction SilentlyContinue }
}
Remove-Item $permFile -ErrorAction SilentlyContinue

# --- fake app declarations ------------------------------------------------------
# Grid: origin (10,20), cell 16x16, 4 rows x 5 cols on a 128x96 surface.
# Cell (r,c) client rect = (10 + 16c, 20 + 16r, 16, 16).
$cursorJson = '{"app":"fakegrid3","tools":[{"name":"snapshot","description":"board snapshot","inputSchema":{"type":"object","properties":{}}}],' +
    '"cursor":{"type":"cell-grid","coordSpace":"client","origin":{"x":10,"y":20},"cellW":16,"cellH":16,' +
    '"rows":4,"cols":5,"cursorOwner":"platform","act":{"kinds":["reveal","flag","question"],"gate":"ask"}}}'
# Same app, cursor block dropped - the cursor cell overlay must vanish while
# the layer stays alive.
$cursorDropped = '{"app":"fakegrid3","tools":[{"name":"snapshot","description":"board snapshot","inputSchema":{"type":"object","properties":{}}}]}'

# --- server lifecycle ------------------------------------------------------------
try {
Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force
Get-Process jkwinserver -ErrorAction SilentlyContinue | Stop-Process -Force
Get-Process jkbridge -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1
Start-Process -FilePath $exe -ArgumentList "--server" -WorkingDirectory $root
$up = $false
foreach ($i in 1..30) {
    Start-Sleep -Milliseconds 500
    if ((Invoke-Agentctl '{"tool":"ping","args":{}}') -match '"ok"\s*:\s*true') { $up = $true; break }
}
Check "t3-up" $up "ping"
if (-not (Find-Server)) { Check "t3-server-window" $false "no jkdesktop main window" }

# --- undeclared plain window client (no ToolRegister at all) --------------------
$plain = New-Pipe 1 $true "plainwin3"
Start-Sleep -Milliseconds 600
$plainLayer = Get-Layer "plainwin3"
$plainOk = ($plainLayer -ne $null -and $plainLayer.w -gt 40)
Check "t3-plain-layer" $plainOk $(if ($plainLayer) { "layer=$($plainLayer.x),$($plainLayer.y) $($plainLayer.w)x$($plainLayer.h)" } else { "not listed" })
if ($plainOk) {
    $s = Sample-Cell $plainLayer 4 4 32 32 0 "plain_baseline"
    Check "t3-plain-clean" ($s.blue -eq 0 -and $s.amber -eq 0) ("blue=$($s.blue) amber=$($s.amber) of $($s.total)")
}

# --- cursor app window, cursor NOT declared yet ----------------------------------
$app = New-Pipe 1 $true "fakegrid3"
Start-Sleep -Milliseconds 600
$lyr = Get-Layer "fakegrid3"
Check "t3-layer-geom" ($lyr -ne $null -and $lyr.w -ge 128 -and $lyr.h -ge 96) $(if ($lyr) { "layer=$($lyr.x),$($lyr.y) $($lyr.w)x$($lyr.h) scale=$script:scale" } else { "not listed" })
$pre = Sample-Cell $lyr 10 20 16 16 0 "predecl"
Check "t3-predecl-clean" ($pre.blue -eq 0) ("blue=$($pre.blue) of $($pre.total)")

# --- declare: cursor cell appears at (0,0) (state exists = shown) ---------------
Send-ToolRegister $app $cursorJson
$ack = Read-Ack $app 3000
Check "t3-register-ack" ($ack -ne $null -and $ack.text -match '"ok":true') $ack.text
Start-Sleep -Milliseconds 500
$cell00 = Sample-Cell $lyr 10 20 16 16 0 "cell00_init"
Check "t3-cursor-cell-init" ($cell00.blue -ge 12) ("blue=$($cell00.blue) of $($cell00.total)")
$far = Sample-Cell $lyr 58 20 16 16 0 "cell03_far"
Check "t3-cursor-single-cell" ($far.blue -eq 0) ("far cell (0,3) blue=$($far.blue) of $($far.total)")

# --- <app>.move moves the outline -------------------------------------------------
$agent = New-Pipe 1 $false ""
$script:qid = 300
$script:qid++
SendQuery $agent $script:qid '{"tool":"app_tool","args":{"app":"fakegrid3","tool":"move","args":{"to_row":3,"to_col":4}}}'
$mv = $null
$sw = [System.Diagnostics.Stopwatch]::StartNew()
while ($sw.ElapsedMilliseconds -lt 5000) {
    $f = Read-Frame $agent 300
    if ($f -ne $null -and $f.type -eq 18) { $mv = $f.text; break }
}
Check "t3-move-echo" ($mv -ne $null -and $mv -match '"ok":true' -and $mv -match '"row":3' -and $mv -match '"col":4') "$mv"
Start-Sleep -Milliseconds 500
$cell34 = Sample-Cell $lyr 74 68 16 16 0 "cell34_moved"
Check "t3-cursor-cell-moved" ($cell34.blue -ge 12) ("blue=$($cell34.blue) of $($cell34.total)")
$old = Sample-Cell $lyr 12 22 12 12 0 "cell00_cleared"
Check "t3-cursor-old-cleared" ($old.blue -eq 0) ("old cell interior blue=$($old.blue) of $($old.total)")

# --- window_move: the outline follows the layer (per-frame recompute) -----------
Invoke-Agentctl ('{"tool":"window_move","args":{"id":' + $lyr.id + ',"x":' + ($lyr.x + 60) + ',"y":' + ($lyr.y + 40) + '}}') | Out-Null
Start-Sleep -Milliseconds 500
$lyr2 = Get-Layer "fakegrid3"
$follow = Sample-Cell $lyr2 74 68 16 16 0 "cell34_follow"
Check "t3-cursor-follows-window-move" ($follow.blue -ge 12) ("blue=$($follow.blue) layer=$($lyr2.x),$($lyr2.y)")
$stale = Sample-Cell $lyr 74 68 16 16 0 "cell34_stale"
Check "t3-cursor-old-position-clear" ($stale.blue -eq 0) ("old layer pos blue=$($stale.blue)")

# --- parked act: amber cell ring at the FIXED parked rect + cursor persists ------
$script:qid++
$ev = Park-Act $agent $script:qid '{"kind":"flag","row":1,"col":2}'
$okPark = ($ev -ne $null -and $ev.text -match '"kind":"app_tool"' -and
           $ev.text -match '"name":"fakegrid3\.flag at \(1,2\)"')
Check "t3-park-banner-name" $okPark ($(if ($ev) { $ev.text } else { "no event" }))
$reqId = 0
if ($ev -ne $null -and $ev.text -match '"request":(\d+)') { $reqId = [int]$Matches[1] }
Start-Sleep -Milliseconds 500
# cell (1,2) client rect = (42,36,16,16) - below the banner band (y<=31)
$ring = Sample-Cell $lyr2 42 36 16 16 0 "cell12_ring"
Check "t3-park-cell-ring" ($ring.amber -ge 12) ("amber=$($ring.amber) of $($ring.total)")
$cur = Sample-Cell $lyr2 74 68 16 16 0 "cell34_during"
Check "t3-cursor-persists-during-park" ($cur.blue -ge 12) ("blue=$($cur.blue)")
# (c) the undeclared plain window stays clean while overlays are live
if ($plainOk) {
    $plainLayer2 = Get-Layer "plainwin3"
    if ($plainLayer2 -ne $null) {
        $ps = Sample-Cell $plainLayer2 0 0 64 64 0 "plain_during"
        Check "t3-plain-unaffected" ($ps.blue -eq 0 -and $ps.amber -eq 0) ("blue=$($ps.blue) amber=$($ps.amber)")
    }
}

# --- approve: amber cell ring gone, cursor cell persists --------------------------
$ap = Invoke-Agentctl ('{"tool":"approve","args":{"request":' + $reqId + ',"decision":"allow"}}')
Check "t3-approve-ack" ($ap -match '"approved":true') $ap
$rel = Read-Relay $agent $app $script:qid 8000 '{"ok":false,"error":"no_act_impl"}'
Check "t3-relay-reply" ($rel -ne $null -and $rel -match '"result"') "$rel"
$gone = $false
$after = $null
foreach ($i in 1..10) {
    Start-Sleep -Milliseconds 300
    $after = Sample-Cell $lyr2 42 36 16 16 0 "cell12_gone"
    if ($after.amber -eq 0) { $gone = $true; break }
}
Check "t3-park-ring-gone" ($gone -and $after.amber -eq 0) ("amber=$($after.amber)")
$cur2 = Sample-Cell $lyr2 74 68 16 16 0 "cell34_after"
Check "t3-cursor-persists-after-resolve" ($cur2.blue -ge 12) ("blue=$($cur2.blue)")

# --- declaration dropped: accent gone, layer still alive --------------------------
Send-ToolRegister $app $cursorDropped
$ackDrop = Read-Ack $app 3000
Check "t3-drop-ack" ($ackDrop -ne $null -and $ackDrop.text -match '"ok":true') $ackDrop.text
Start-Sleep -Milliseconds 500
$stillThere = Get-Layer "fakegrid3"
Check "t3-drop-layer-alive" ($stillThere -ne $null) "list_windows"
$dropCell = Sample-Cell $lyr2 74 68 16 16 0 "cell34_dropped"
Check "t3-decl-dropped-no-accent" ($dropCell.blue -eq 0) ("blue=$($dropCell.blue) of $($dropCell.total)")

# --- capture-overlay exclusion: "Region Capture" client draws nothing -------------
$cap = New-Pipe 1 $true "Region Capture"
Send-ToolRegister $cap $cursorJson
$ackCap = Read-Ack $cap 3000
Check "t3-cap-register-ack" ($ackCap -ne $null -and $ackCap.text -match '"ok":true') $ackCap.text
Start-Sleep -Milliseconds 700
$capLayer = Get-Layer "Region Capture"
$capOk = ($capLayer -ne $null -and $capLayer.w -ge 128)
Check "t3-cap-layer" $capOk $(if ($capLayer) { "layer=$($capLayer.x),$($capLayer.y) $($capLayer.w)x$($capLayer.h)" } else { "not listed" })
if ($capOk) {
    $capCell = Sample-Cell $capLayer 10 20 16 16 0 "cap_cell00"
    Check "t3-capture-overlay-excluded" ($capCell.blue -eq 0) ("blue=$($capCell.blue) of $($capCell.total) (would-be cursor cell)")
}
$cap.Dispose()
Start-Sleep -Milliseconds 500

# --- cleanup ----------------------------------------------------------------------
$app.Dispose()
$plain.Dispose()
$agent.Dispose()
Write-Host ("RESULT: " + ($(if ($script:fail -eq 0) { "ALL PASS" } else { "FAIL " + $script:fail })))
exit ($script:fail)
}
finally {
    Restore-Perms
    Remove-Item $permBakFile -ErrorAction SilentlyContinue
    Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force
    Get-Process jkwinserver -ErrorAction SilentlyContinue | Stop-Process -Force
    Get-Process jkbridge -ErrorAction SilentlyContinue | Stop-Process -Force
}