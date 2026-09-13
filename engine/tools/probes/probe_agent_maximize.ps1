# Window maximize/restore probe (docs/39 Task 2): the server chrome maximize
# button (left of the close X), the button's restore glyph, and the title-bar
# double-click toggle - all via synthetic SendInput on the server window.
# PASS/FAIL via exit code. PowerShell 5.1 compatible. ASCII-only on purpose:
# a BOM-less .ps1 with non-ASCII bytes is read as cp949 and the parser breaks
# (docs/15 lesson).
#
# Conventions reused here (binding precedents):
# - probe_agent_e2e.ps1: minesweeper spawn via MCP pipe (the agent path IS the
#   tested path) + list_windows id/title read from the ESCAPED tool text.
# - probe_agent_trust.ps1: server launch/teardown + agent-events per-phase
#   jobs. jkdesktop is a GUI exe writing to a pipe, so job stdout is fully
#   buffered - Receive-Job returns nothing until the process exits, hence one
#   short job per phase, started BEFORE the triggering click (lesson 28:
#   subscribe first, pushes are not replayed).
# - probe_agent_shot.ps1 / smoke_chrome.ps1: synthetic click coordinate space.
#   ClientToScreen + GetClientRect on the server hwnd, scale = clientW / 1280
#   (the window/renderer ratio - the server desktop is 1280x720 logical and
#   its mouse pipeline is physical px / outputScale, docs/14 s14.3). Never
#   multiply by the monitor OutputScale on top (docs/35 lesson 8: DPI
#   virtualization space is 1:1 with ClientToScreen).
#
# Chrome geometry (JKCompositor.h): close X = 20x20 at margin 2 from the
# right edge, maximize/restore button = 20x20 a 2px gap to its left, title
# bar 24px, top resize strip 6px. Button x0 = w - 2 - 20 - 2 - 20, so its
# center is (w - 34, 12). Minesweeper is a fit-1:1 surface (320x380 on the
# 1280x720 desktop), so surface px == logical display px and the button point
# maps 1:1 (chrome zones are surface-px and shrink with a fit scale).
$ErrorActionPreference = "Continue"
$build = "I:\progwork\JKENGINE\engine\build"
$exe   = "$build\jkdesktop.exe"
$agnt  = "$build\jkagentd.exe"

function Stop-ProbeProcs {
    # Spawned client apps by PID only (lesson 42: never by image name - the
    # server shares the jkdesktop.exe image name); the server itself the
    # probes stop by name (shot/trust convention).
    Get-CimInstance Win32_Process -Filter "Name='jkdesktop.exe'" |
        Where-Object { $_.CommandLine -match '--client' } |
        ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
    Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force
}

Stop-ProbeProcs
Start-Sleep -Seconds 1

Start-Process -FilePath $exe -ArgumentList "--server" `
    -WorkingDirectory $build -WindowStyle Hidden
Start-Sleep -Seconds 4

function Invoke-Mcp([string]$line) {
    # One-shot jsonrpc call piped to jkagentd (probe_agent_e2e convention:
    # no spaces in the JSON; tool results arrive with quotes escaped).
    $out = ($line | & $agnt)
    $script:lastMcp = ($out -join "`n")
    return $script:lastMcp
}
function Invoke-Agentctl([string]$json) {
    $escaped = $json -replace '"', '\"'
    return (& $exe agentctl $escaped) -join "`n"
}
function Get-MineWindow {
    # Window fields live ESCAPED inside the tool text (e2e convention). The
    # server is fresh, so the only listed window is the minesweeper client.
    $r = Invoke-Mcp '{"jsonrpc":"2.0","id":9,"method":"tools/call","params":{"name":"list_windows","arguments":{}}}'
    $script:lastList = $r
    $m = [regex]::Match($r, '\\"id\\":(\d+),\\"title\\":\\"([^"\\]*)\\",\\"pid\\":(\d+),' +
                         '\\"x\\":(-?\d+),\\"y\\":(-?\d+),\\"w\\":(\d+),\\"h\\":(\d+)')
    if (-not $m.Success) { return $null }
    return [pscustomobject]@{
        id = [int]$m.Groups[1].Value; title = $m.Groups[2].Value
        pid = [int]$m.Groups[3].Value; x = [int]$m.Groups[4].Value
        y = [int]$m.Groups[5].Value;   w = [int]$m.Groups[6].Value
        h = [int]$m.Groups[7].Value
    }
}

# --- synthetic input setup (smoke_chrome convention) -----------------------
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class Wm {
    [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr ctx);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int cmd);
    [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint flags);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern IntPtr FindWindow(string cls, string title);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
    [DllImport("user32.dll")] public static extern int GetSystemMetrics(int index);
    [DllImport("user32.dll", SetLastError = true)]
    public static extern uint SendInput(uint n, INPUT[] p, int size);
    public struct RECT { public int L, T, R, B; }
    public struct POINT { public int X, Y; }
    [StructLayout(LayoutKind.Sequential)]
    public struct MOUSEINPUT { public int dx; public int dy; public uint mouseData;
        public uint dwFlags; public uint time; public IntPtr dwExtraInfo; }
    [StructLayout(LayoutKind.Sequential)]
    public struct INPUT { public uint type; public MOUSEINPUT mi; }
    // 0 = INPUT_MOUSE; 1 = MOVE, 2 = LEFTDOWN, 4 = LEFTUP, 0x8000 = ABSOLUTE.
    // One atomic SendInput batch: move + down + up. The user's hand can move
    // the physical cursor between a SetCursorPos and a later click - packing
    // move + click into one input batch removes that race, and MOUSEEVENTF_
    // ABSOLUTE normalized coordinates sidestep per-monitor DPI spaces entirely
    // (docs/14 s11.2 multi-monitor lesson).
    public static bool SendClickAt(int px, int py) {
        int vx = GetSystemMetrics(76), vy = GetSystemMetrics(77);
        int vw = GetSystemMetrics(78), vh = GetSystemMetrics(79);
        if (vw < 2 || vh < 2) { return false; }
        int nx = (int)Math.Round((px - vx) * 65535.0 / (vw - 1));
        int ny = (int)Math.Round((py - vy) * 65535.0 / (vh - 1));
        INPUT[] seq = new INPUT[3];
        // MOVE|ABSOLUTE|VIRTUALDESK = 0xC001. WITHOUT VIRTUALDESK the
        // normalized coords map over the PRIMARY monitor only - on this rig
        // the virtual desktop starts at x=-1920, so every x-clicked point
        // drifted left by ~61 px and missed the button zone (hit the title
        // bar move grab instead).
        seq[0].type = 0; seq[0].mi.dwFlags = 0xC001; seq[0].mi.dx = nx; seq[0].mi.dy = ny;
        seq[1].type = 0; seq[1].mi.dwFlags = 2;
        seq[2].type = 0; seq[2].mi.dwFlags = 4;
        return SendInput(3, seq, Marshal.SizeOf(typeof(INPUT))) == 3;
    }
    public static bool SendButtonDownUp() {
        INPUT[] seq = new INPUT[2];
        seq[0].type = 0; seq[0].mi.dwFlags = 2;
        seq[1].type = 0; seq[1].mi.dwFlags = 4;
        return SendInput(2, seq, Marshal.SizeOf(typeof(INPUT))) == 2;
    }
}
"@
# Per-monitor-v2: GetClientRect/ClientToScreen report physical px; the
# normalized ABSOLUTE coords of SendInput cover the physical virtual desktop
# (GetSystemMetrics is DPI-aware here). The scale below is the renderer
# ratio, NOT the monitor DPI.
[Wm]::SetProcessDpiAwarenessContext([IntPtr](-4)) | Out-Null

function Get-ServerHwnd {
    # MainWindowHandle stays 0 here: the probe starts the server with
    # -WindowStyle Hidden (probe convention) and the STARTUPINFO SW_HIDE
    # sticks to the SDL window. Find it by class+title instead, then SHOW it
    # (no-activate) - a hidden window neither receives nor can be hit by
    # synthetic mouse input.
    for ($i = 0; $i -lt 50; ++$i) {
        $sp = Get-CimInstance Win32_Process -Filter "Name='jkdesktop.exe'" |
            Where-Object { $_.CommandLine -match '--server' } |
            Select-Object -First 1
        if ($sp) {
            $h = [Wm]::FindWindow("SDL_app", "JKENGINE Window Server")
            if ($h -ne [IntPtr]::Zero) { return $h }
        }
        Start-Sleep -Milliseconds 200
    }
    return [IntPtr]::Zero
}
$hwnd = Get-ServerHwnd
if ($hwnd -ne [IntPtr]::Zero -and -not [Wm]::IsWindowVisible($hwnd)) {
    [void][Wm]::ShowWindow($hwnd, 8)   # SW_SHOWNOACTIVATE
    Start-Sleep -Milliseconds 500
}
# Pin the test window above everything (the desktop session may have other
# windows over it) - synthetic clicks must land on the server, not on
# whatever happens to be on top. The server is killed at cleanup.
if ($hwnd -ne [IntPtr]::Zero) {
    # HWND_TOPMOST = -1; SWP_NOSIZE|SWP_NOMOVE|SWP_NOACTIVATE = 0x13
    [void][Wm]::SetWindowPos($hwnd, [IntPtr](-1), 0, 0, 0, 0, 0x13)
}

$crect = New-Object Wm+RECT
[Wm]::GetClientRect($hwnd, [ref]$crect) | Out-Null
$co = New-Object Wm+POINT
$co.X = 0; $co.Y = 0
[Wm]::ClientToScreen($hwnd, [ref]$co) | Out-Null
$scale = ($crect.R - $crect.L) / 1280.0

# Logical desktop point -> screen (physical) point via ClientToScreen.
# NOTE: Get-ButtonPointXY/Get-TitlePointXY already return SCREEN points, so
# Click-At/DblClick-At take screen points and must NOT run Pt again (a double
# transform lands the click at scale^2 - x*1.25^2 - off the button zone).
function Pt([double]$lx, [double]$ly) {
    return @([int][math]::Round($co.X + $lx * $scale),
             [int][math]::Round($co.Y + $ly * $scale))
}
function Click-At([int]$sx, [int]$sy) {
    [void][Wm]::SendClickAt($sx, $sy)
    Start-Sleep -Milliseconds 250
}
function DblClick-At([int]$sx, [int]$sy) {
    # Two rapid clicks at one position (cursor unmoved between them): SDL
    # reports ev.button.clicks == 2. First click of the pair also moves.
    [void][Wm]::SendClickAt($sx, $sy)
    Start-Sleep -Milliseconds 70
    [void][Wm]::SendButtonDownUp()
    Start-Sleep -Milliseconds 250
}
function Get-ButtonPointXY($win) {
    # Maximize/restore button center: x0 = w - 44, center x = w - 34; y = 12.
    return (Pt ($win.x + $win.w - 34) ($win.y + 12))
}
function Get-TitlePointXY($win) {
    # Title bar center: (w/2, 14) - inside [kResizeHotspot, kChromeTitleBar)
    # and clear of both buttons.
    return (Pt ($win.x + $win.w / 2) ($win.y + 14))
}
# One short-lived events job per phase (trust probe convention): job stdout
# is fully buffered, so the subscriber must already be up BEFORE the click
# (lesson 28) and is drained with Receive-Job -Wait afterwards.
function Watch-Events([int]$sec) {
    return (Start-Job -ScriptBlock { param($e, $s) (& $e agent-events $s) -join "`n" } `
        -ArgumentList $exe, $sec)
}
function Count-Topic([string]$stream, [string]$topic) {
    return ([regex]::Matches($stream, ('window\.' + $topic))).Count
}

$ok = $true

# --- 1. spawn minesweeper via MCP, measure id + title ----------------------
$launch = Invoke-Mcp '{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"launch_app","arguments":{"app":"minesweeper"}}}'
Start-Sleep -Seconds 3
$mine = Get-MineWindow

if ($hwnd -eq [IntPtr]::Zero -or -not $mine) {
    Write-Host ("setup: FAIL (hwnd={0}, mine={1})" -f $hwnd, ($null -ne $mine))
    Write-Host ("launch reply: {0}" -f $launch)
    Write-Host ("last list reply: {0}" -f $script:lastList)
    Stop-ProbeProcs
    exit 1
}

# check 1 - spawn + list_windows measured id/title/rect
if ($mine.id -gt 0 -and $mine.title -match 'Mine' -and $mine.w -gt 0) {
    Write-Host ("spawn+list: PASS (id={0} '{1}' {2}x{3} @ {4},{5})" -f
        $mine.id, $mine.title, $mine.w, $mine.h, $mine.x, $mine.y)
} else {
    $ok = $false
    Write-Host "spawn+list: FAIL ($mine)"
}

# --- 2. maximize button click -> window.maximized --------------------------
if ($ok) {
    $job = Watch-Events 8
    Start-Sleep -Seconds 2
    $bp = Get-ButtonPointXY $mine
    Click-At $bp[0] $bp[1]
    $evA = (Receive-Job -Job $job -Wait 2>&1) | Out-String
    Start-Sleep -Seconds 1
    $maxA = Get-MineWindow
    $evId = 0
    $lineA = [regex]::Match($evA, 'window\.maximized[^\r\n]*')
    if ($lineA.Success) {
        $im = [regex]::Match($lineA.Value, '"id\\?":\s*(\d+)')
        if ($im.Success) { $evId = [int]$im.Groups[1].Value }
    }
    $nMaxA = Count-Topic $evA 'maximized'
    # Maximize must land on the EXACT work area (docs/39 s2): server window is
    # 1280x720 logical (main.cpp Init) minus the shell reserve
    # (JKCompositor::ShellReserveHeight = the visible shell layer's display
    # height; the taskbar shell meta is 1280x40, JKAppModule_taskbar.cpp) ->
    # 1280x680 @ 0,0 on this rig. Growth-only would let a dropped
    # "- ShellReserveHeight()" (maximize covering the taskbar) pass.
    $workW = 1280
    $workH = 720 - 40
    $rectMaxOk = ($maxA -ne $null -and $maxA.x -eq 0 -and $maxA.y -eq 0 -and
                  $maxA.w -eq $workW -and $maxA.h -eq $workH)
    if ($nMaxA -eq 1 -and $evId -eq $mine.id -and $rectMaxOk) {
        Write-Host ("button-maximize: PASS (window.maximized id={0}, work-area rect {1}x{2} @ 0,0)" -f
            $evId, $maxA.w, $maxA.h)
        # BONUS (not pass/fail): shoot the maximized window for a visual check.
        $cap = Invoke-Agentctl ('{"tool":"capture_window","args":{"id":' + $mine.id + '}}')
        if ($cap -match '"path\\?":\\?"([^"]*)"') {
            Write-Host ("  bonus screenshot: {0}" -f ($Matches[1] -replace '\\\\', '\'))
        } else {
            Write-Host "  bonus screenshot: unavailable"
        }
    } else {
        $ok = $false
        Write-Host ("button-maximize: FAIL (maximized={0}, id={1} want {2}, rect={3}, events={4})" -f
            $nMaxA, $evId, $mine.id, $maxA, $evA)
    }
}

# --- 3. same button again (restore glyph) -> window.restored ---------------
if ($ok) {
    $maxWin = Get-MineWindow
    $job = Watch-Events 8
    Start-Sleep -Seconds 2
    $bp = Get-ButtonPointXY $maxWin
    Click-At $bp[0] $bp[1]
    $evB = Receive-Job -Job $job -Wait | Out-String
    Start-Sleep -Seconds 1
    $restB = Get-MineWindow
    $nResB = Count-Topic $evB 'restored'
    $nMaxB = Count-Topic $evB 'maximized'
    # Back to the exact pre-maximize rect, and no stray second maximized.
    $rectBack = ($restB -ne $null -and $restB.x -eq $mine.x -and $restB.y -eq $mine.y -and
                 $restB.w -eq $mine.w -and $restB.h -eq $mine.h)
    if ($nResB -eq 1 -and $nMaxB -eq 0 -and $rectBack) {
        Write-Host ("button-restore: PASS (window.restored, rect {0}x{1} @ {2},{3})" -f
            $restB.w, $restB.h, $restB.x, $restB.y)
    } else {
        $ok = $false
        Write-Host ("button-restore: FAIL (restored={0}, maximized={1}, rect={2}, events={3})" -f
            $nResB, $nMaxB, $restB, $evB)
    }
}

# --- 4. title double-click toggle ------------------------------------------
if ($ok) {
    $cur = Get-MineWindow
    $job = Watch-Events 8
    Start-Sleep -Seconds 2
    $tp = Get-TitlePointXY $cur
    DblClick-At $tp[0] $tp[1]
    $evC = Receive-Job -Job $job -Wait | Out-String
    Start-Sleep -Seconds 1
    $dblC = Count-Topic $evC 'maximized'
    $resC = Count-Topic $evC 'restored'
    $maxWin2 = Get-MineWindow
    $rectC = ($maxWin2 -ne $null -and $maxWin2.x -eq 0 -and $maxWin2.y -eq 0)
    # Exactly ONE maximized: with drag-restore deferred, the plain clicks of a
    # double-click must not restore and the 2nd click toggles exactly once.
    if ($dblC -eq 1 -and $resC -eq 0 -and $rectC) {
        Write-Host "dblclick-maximize: PASS (exactly 1 window.maximized, rect @ 0,0)"
    } else {
        $ok = $false
        Write-Host ("dblclick-maximize: FAIL (maximized={0}, restored={1}, rect={2}, events={3})" -f
            $dblC, $resC, $maxWin2, $evC)
    }
}

if ($ok) {
    $maxWin = Get-MineWindow
    $job = Watch-Events 8
    Start-Sleep -Seconds 2
    $tp = Get-TitlePointXY $maxWin
    DblClick-At $tp[0] $tp[1]
    $evD = Receive-Job -Job $job -Wait | Out-String
    Start-Sleep -Seconds 1
    $restD = Get-MineWindow
    $resD = Count-Topic $evD 'restored'
    $dblD = Count-Topic $evD 'maximized'
    $rectD = ($restD -ne $null -and $restD.x -eq $mine.x -and $restD.y -eq $mine.y -and
              $restD.w -eq $mine.w -and $restD.h -eq $mine.h)
    if ($resD -eq 1 -and $dblD -eq 0 -and $rectD) {
        Write-Host ("dblclick-restore: PASS (window.restored, rect back {0}x{1} @ {2},{3})" -f
            $restD.w, $restD.h, $restD.x, $restD.y)
    } else {
        $ok = $false
        Write-Host ("dblclick-restore: FAIL (restored={0}, maximized={1}, rect={2}, events={3})" -f
            $resD, $dblD, $restD, $evD)
    }
}

# --- 5. cleanup -------------------------------------------------------------
# Remove bonus-capture artifacts so the tree is left as found (probe_agent_shot
# re-creates and re-cleans the dir itself).
Remove-Item "$build\state\screenshots" -Recurse -ErrorAction SilentlyContinue
Stop-ProbeProcs
# No permission file was created (chrome input is server-local, no approval
# gate) - nothing to restore on that front.

if ($ok) { Write-Host "PASS: agent maximize"; exit 0 }
else { Write-Host "FAIL: agent maximize"; exit 1 }