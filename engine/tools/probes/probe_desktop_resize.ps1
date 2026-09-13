# Desktop resize/maximize probe (docs/39 s8): the SERVER window itself is now
# SDL_WINDOW_RESIZABLE + OS-maximizable (ShowWindow SW_MAXIMIZE / SW_RESTORE).
# Verifies: (1) the server log's "desktop size changed to WxH" line fires on a
# real logical SIZE change, (2) maximized app layers are re-issued against the
# new work area WITHOUT a window.maximized event (desktop-size adjustment, not
# a toggle), (3) an app maximized while the desktop is maximized lands on the
# NEW work area exactly.
# PASS/FAIL via exit code. PowerShell 5.1 compatible. ASCII-only on purpose:
# a BOM-less .ps1 with non-ASCII bytes is read as cp949 and the parser breaks
# (docs/15 lesson).
#
# Assertion choice (documented per docs/39 task): the strongest robust signals
# are (a) the server stdout "desktop size changed to WxH (re-maximized N
# layer(s))" line captured via Start-Process -RedirectStandardOutput (the
# run_test-style stdout capture - the server printf's it with fflush, so the
# file reads live), and (b) list_windows geometry for the re-maximized layer.
# The taskbar re-dock is exercised implicitly (list_windows of the freshly
# spawned app must fit inside the new logical desktop); a screenshot add is
# NOT used as an assertion - PrintWindow lies on this rig (memory 2026-09-06).
#
# Conventions reused here (binding precedents, probe_agent_maximize.ps1):
# - probe_agent_e2e.ps1: minesweeper spawn via MCP pipe (the agent path IS the
#   tested path) + list_windows id/title read from the ESCAPED tool text.
# - probe_agent_trust.ps1: server launch/teardown + agent-events per-phase
#   jobs. jkdesktop is a GUI exe writing to a pipe, so job stdout is fully
#   buffered - Receive-Job returns nothing until the process exits, hence one
#   short job per phase, started BEFORE the triggering click (lesson 28).
# - probe_agent_shot.ps1 / smoke_chrome.ps1: synthetic click coordinate space.
#   ClientToScreen + GetClientRect on the server hwnd, scale = clientW / 1280
#   (the window/renderer ratio - the server desktop is 1280x720 logical and
#   its mouse pipeline is physical px / outputScale, docs/14 s14.3). Never
#   multiply by the monitor OutputScale on top (docs/35 lesson 8).
# - Chrome geometry (JKCompositor.h): close X = 20x20 at margin 2 from the
#   right edge, maximize/restore button = 20x20 a 2px gap to its left, title
#   bar 24px. Button center = (w - 34, 12) in the window's LOGICAL desktop
#   space (minesweeper is a fit-1:1 surface, so surface px == logical px).
# - probe_agent_maximize.ps1 check 2: maximize must land on the EXACT work
#   area (docs/39 s2): logical desktop minus ShellReserveHeight (the visible
#   shell layer's display height = 40, JKAppModule_taskbar.cpp meta 1280x40).
#   Unlike that probe, here the expected work area is DERIVED from the server
#   log's "desktop size changed to WxH" line (derive, don't guess) - the
#   OS-maximized logical size depends on the monitor's work area and DPI.
$ErrorActionPreference = "Continue"
$build = "I:\progwork\JKENGINE\engine\build"
$exe   = "$build\jkdesktop.exe"
$agnt  = "$build\jkagentd.exe"
$srvLog = "$env:TEMP\jk_desktop_resize_srv.log"
$srvErr = "$env:TEMP\jk_desktop_resize_srv.err"

function Stop-ProbeProcs {
    # Spawned client apps by PID only (lesson 42: never by image name - the
    # server shares the jkdesktop.exe image name); the server itself the
    # probes stop by name (shot/trust convention).
    Get-CimInstance Win32_Process -Filter "Name='jkdesktop.exe'" |
        Where-Object { $_.CommandLine -match '--client' } |
        ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
    Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force
}

function Get-DesktopSizeLines {
    # All "desktop size changed to WxH (re-maximized N layer(s))" lines so
    # far. Redirected logs can read STALE (memory: re-read before concluding
    # an event was lost) - the caller re-reads after extra sleeps if needed.
    return (Get-Content $srvLog -ErrorAction SilentlyContinue |
        Select-String -Pattern 'desktop size changed to (\d+)x(\d+) \(re-maximized (\d+) layer')
}
function Get-LastDesktopSize {
    # Last [w,h,maxN] from the log (or $null). Two-pass with a sleep: the
    # fflush'd line lands on the file live, but give the pipe a beat.
    for ($try = 0; $try -lt 5; ++$try) {
        $lines = Get-DesktopSizeLines
        if ($lines) {
            $last = $lines | Select-Object -Last 1
            if ($last.Line -match 'desktop size changed to (\d+)x(\d+) \(re-maximized (\d+) layer') {
                return @([int]$Matches[1], [int]$Matches[2], [int]$Matches[3])
            }
        }
        Start-Sleep -Milliseconds 500
    }
    return $null
}

Stop-ProbeProcs
Remove-Item $srvLog, $srvErr -ErrorAction SilentlyContinue
Start-Sleep -Seconds 1

# Server stdout IS the probe-relevant log here (the desktop-size line goes to
# stdout), so unlike the sibling probes this one redirects it to a file.
Start-Process -FilePath $exe -ArgumentList "--server" `
    -WorkingDirectory $build -WindowStyle Hidden `
    -RedirectStandardOutput $srvLog -RedirectStandardError $srvErr
Start-Sleep -Seconds 4

function Invoke-Mcp([string]$line) {
    # One-shot jsonrpc call piped to jkagentd (probe_agent_e2e convention:
    # no spaces in the JSON; tool results arrive with quotes escaped).
    return (($line | & $agnt) -join "`n")
}
function Get-AllWindows {
    # Window fields live ESCAPED inside the tool text (e2e convention).
    # Returns ALL listed windows (this probe has TWO minesweepers at times;
    # probe_agent_maximize's first-match helper is not enough here).
    $r = Invoke-Mcp '{"jsonrpc":"2.0","id":9,"method":"tools/call","params":{"name":"list_windows","arguments":{}}}'
    $script:lastList = $r
    $out = @()
    foreach ($m in [regex]::Matches($r, '\\"id\\":(\d+),\\"title\\":\\"([^"\\]*)\\",\\"pid\\":(\d+),' +
                                   '\\"x\\":(-?\d+),\\"y\\":(-?\d+),\\"w\\":(\d+),\\"h\\":(\d+)')) {
        $out += [pscustomobject]@{
            id = [int]$m.Groups[1].Value; title = $m.Groups[2].Value
            pid = [int]$m.Groups[3].Value; x = [int]$m.Groups[4].Value
            y = [int]$m.Groups[5].Value;   w = [int]$m.Groups[6].Value
            h = [int]$m.Groups[7].Value
        }
    }
    return $out
}
function Get-MineByExclude([int]$excludeId) {
    # The minesweeper window whose id is NOT excludeId (0 = any).
    return (Get-AllWindows | Where-Object { $_.title -match 'Mine' -and $_.id -ne $excludeId } |
        Select-Object -First 1)
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
    // One atomic SendInput batch: move + down + up, ABSOLUTE + VIRTUALDESK
    // (probe_agent_maximize lesson: without VIRTUALDESK the normalized coords
    // map over the PRIMARY monitor only and every clicked point drifted).
    public static bool SendClickAt(int px, int py) {
        int vx = GetSystemMetrics(76), vy = GetSystemMetrics(77);
        int vw = GetSystemMetrics(78), vh = GetSystemMetrics(79);
        if (vw < 2 || vh < 2) { return false; }
        int nx = (int)Math.Round((px - vx) * 65535.0 / (vw - 1));
        int ny = (int)Math.Round((py - vy) * 65535.0 / (vh - 1));
        INPUT[] seq = new INPUT[3];
        seq[0].type = 0; seq[0].mi.dwFlags = 0xC001; seq[0].mi.dx = nx; seq[0].mi.dy = ny;
        seq[1].type = 0; seq[1].mi.dwFlags = 2;
        seq[2].type = 0; seq[2].mi.dwFlags = 4;
        return SendInput(3, seq, Marshal.SizeOf(typeof(INPUT))) == 3;
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
function Update-ServerOrigin {
    # The server window MOVES when the OS maximizes/restores it, so the
    # ClientToScreen origin must be re-read before every logical->screen
    # transform (probe_agent_maximize could cache it once - its server window
    # never moved; this probe's does). The scale is the renderer ratio
    # clientW / LOGICAL width - and the logical width CHANGES when the
    # desktop is maximized (probe_agent_maximize's constant /1280 would be
    # wrong here: it would read the maximized desktop at 1.5x and land every
    # click low-right), so it is derived from the server log's latest
    # "desktop size changed" line (1280 before the first such line).
    $crect = New-Object Wm+RECT
    [void][Wm]::GetClientRect($script:hwnd, [ref]$crect)
    $co = New-Object Wm+POINT
    $co.X = 0; $co.Y = 0
    [void][Wm]::ClientToScreen($script:hwnd, [ref]$co)
    $script:coX = $co.X; $script:coY = $co.Y
    $dsz = Get-LastDesktopSize
    $logicalW = 1280
    if ($dsz) { $logicalW = $dsz[0] }
    $script:scale = ($crect.R - $crect.L) / $logicalW
}
function Pt([double]$lx, [double]$ly) {
    return @([int][math]::Round($script:coX + $lx * $script:scale),
             [int][math]::Round($script:coY + $ly * $script:scale))
}
function Click-At([int]$sx, [int]$sy) {
    [void][Wm]::SendClickAt($sx, $sy)
    Start-Sleep -Milliseconds 250
}
function Get-ButtonPointXY($win) {
    # Maximize/restore button center: x0 = w - 44, center x = w - 34; y = 12.
    return (Pt ($win.x + $win.w - 34) ($win.y + 12))
}
# One short-lived events job per phase (trust probe convention): job stdout
# is fully buffered, so the subscriber must already be up BEFORE the click
# (lesson 28) and is drained with Receive-Job -Wait afterwards.
function Watch-Events([int]$sec) {
    return (Start-Job -ScriptBlock { param($e, $s) (& $e agent-events $s) -join "`n" } `
        -ArgumentList $exe, $sec)
}

$hwnd = Get-ServerHwnd
if ($hwnd -ne [IntPtr]::Zero -and -not [Wm]::IsWindowVisible($hwnd)) {
    [void][Wm]::ShowWindow($hwnd, 8)   # SW_SHOWNOACTIVATE
    Start-Sleep -Milliseconds 500
}
# Pin the test window above everything - synthetic clicks must land on the
# server, not on whatever happens to be on top. Killed at cleanup.
if ($hwnd -ne [IntPtr]::Zero) {
    # HWND_TOPMOST = -1; SWP_NOSIZE|SWP_NOMOVE|SWP_NOACTIVATE = 0x13
    [void][Wm]::SetWindowPos($hwnd, [IntPtr](-1), 0, 0, 0, 0, 0x13)
}
Update-ServerOrigin

$ok = $true

# --- 1. spawn minesweeper via MCP, measure id + rect ------------------------
$launch = Invoke-Mcp '{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"launch_app","arguments":{"app":"minesweeper"}}}'
Start-Sleep -Seconds 3
$mineA = Get-MineByExclude 0

if ($hwnd -eq [IntPtr]::Zero -or -not $mineA) {
    Write-Host ("setup: FAIL (hwnd={0}, mineA={1})" -f $hwnd, ($null -ne $mineA))
    Write-Host ("launch reply: {0}" -f $launch)
    Write-Host ("last list reply: {0}" -f $script:lastList)
    Stop-ProbeProcs
    exit 1
}
if ($mineA.id -gt 0 -and $mineA.w -gt 0) {
    Write-Host ("spawn+list: PASS (id={0} '{1}' {2}x{3} @ {4},{5})" -f
        $mineA.id, $mineA.title, $mineA.w, $mineA.h, $mineA.x, $mineA.y)
} else {
    $ok = $false
    Write-Host "spawn+list: FAIL ($mineA)"
}

# --- 2. OS-maximize the server window ---------------------------------------
# ShowWindow(SW_MAXIMIZE=3) on the server hwnd. Expected: a SIZE_CHANGED
# funnels into UpdateOutputBounds -> one "desktop size changed to WxH" line
# with a size LARGER than the init 1280x720; the taskbar re-docks; the
# NON-maximized app layer keeps its rect (v1: no re-fit for plain windows on
# grow).
if ($ok) {
    [void][Wm]::ShowWindow($hwnd, 3)   # SW_MAXIMIZE
    Start-Sleep -Seconds 3
    $dsz = Get-LastDesktopSize
    Update-ServerOrigin   # the window moved; origin + scale must be re-read
    $mineA2 = Get-MineByExclude 0
    $maxLarger = ($dsz -ne $null -and $dsz[0] -gt 1280 -and $dsz[1] -gt 720)
    $aStable = ($mineA2 -ne $null -and $mineA2.x -eq $mineA.x -and $mineA2.y -eq $mineA.y -and
                $mineA2.w -eq $mineA.w -and $mineA2.h -eq $mineA.h)
    if ($maxLarger -and $aStable) {
        Write-Host ("os-maximize+log: PASS (desktop {0}x{1}, layer A stayed {2}x{3} @ {4},{5})" -f
            $dsz[0], $dsz[1], $mineA2.w, $mineA2.h, $mineA2.x, $mineA2.y)
    } else {
        $ok = $false
        Write-Host ("os-maximize+log: FAIL (dsz={0}, A-before={1}, A-after={2})" -f
            ($dsz -join 'x'), $mineA, $mineA2)
    }
}

# --- 3. spawn a SECOND app while maximized: placement inside the new area ---
# The freshly spawned client is placed by the CURRENT logical desktop, so its
# rect must fit inside the maximized desktop entirely (the taskbar/shell
# re-dock is what made that area available again).
if ($ok) {
    $dsz = Get-LastDesktopSize
    [void](Invoke-Mcp '{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"launch_app","arguments":{"app":"minesweeper"}}}')
    Start-Sleep -Seconds 3
    $mineB = Get-MineByExclude $mineA.id
    $fits = ($mineB -ne $null -and $dsz -ne $null -and
             $mineB.x -ge 0 -and $mineB.y -ge 0 -and
             ($mineB.x + $mineB.w) -le $dsz[0] -and ($mineB.y + $mineB.h) -le $dsz[1])
    if ($fits) {
        Write-Host ("spawn-while-maximized: PASS (id={0} {1}x{2} @ {3},{4} inside {5}x{6})" -f
            $mineB.id, $mineB.w, $mineB.h, $mineB.x, $mineB.y, $dsz[0], $dsz[1])
    } else {
        $ok = $false
        Write-Host ("spawn-while-maximized: FAIL (B={0}, dsz={1})" -f $mineB, ($dsz -join 'x'))
    }
}

# --- 4. maximize app B via its chrome button -> NEW work area exactly -------
# Expected rect = the log-derived maximized desktop minus the 40px shell
# reserve (ShellReserveHeight = visible shell display height, taskbar meta
# 1280x40). Larger than the 1280x680 of probe_agent_maximize by construction.
if ($ok) {
    $dsz = Get-LastDesktopSize
    $cur = Get-MineByExclude $mineA.id
    Update-ServerOrigin
    $job = Watch-Events 8
    Start-Sleep -Seconds 2
    $bp = Get-ButtonPointXY $cur
    Click-At $bp[0] $bp[1]
    $evA = (Receive-Job -Job $job -Wait 2>&1) | Out-String
    Start-Sleep -Seconds 1
    $bMax = Get-MineByExclude $mineA.id
    $nMax = ([regex]::Matches($evA, 'window\.maximized')).Count
    $evId = 0
    $line = [regex]::Match($evA, 'window\.maximized[^\r\n]*')
    if ($line.Success) {
        $im = [regex]::Match($line.Value, '"id\\?":\s*(\d+)')
        if ($im.Success) { $evId = [int]$im.Groups[1].Value }
    }
    $workW = $dsz[0]; $workH = $dsz[1] - 40
    $rectOk = ($bMax -ne $null -and $bMax.x -eq 0 -and $bMax.y -eq 0 -and
               $bMax.w -eq $workW -and $bMax.h -eq $workH)
    if ($nMax -eq 1 -and $evId -eq $mineB.id -and $rectOk -and $workW -gt 1280) {
        Write-Host ("app-maximize-on-big-desktop: PASS (event id={0}, work area {1}x{2} @ 0,0)" -f
            $evId, $workW, $workH)
    } else {
        $ok = $false
        Write-Host ("app-maximize-on-big-desktop: FAIL (maximized={0}, id={1} want {2}, rect={3} want {4}x{5}, events={6})" -f
            $nMax, $evId, $mineB.id, $bMax, $workW, $workH, $evA)
    }
}

# --- 5. OS-restore the server window -> re-maximize WITHOUT an event --------
# ShowWindow(SW_RESTORE=9): the desktop shrinks back to 1280x720, a second
# "desktop size changed" line appears, and app B (still maximized) is
# re-issued against the restored work area 1280x680. BY DESIGN no
# window.maximized fires here (desktop-size adjustment, not a toggle) - the
# assert is the log line + list_windows geometry.
if ($ok) {
    $job = Watch-Events 8
    Start-Sleep -Seconds 2
    [void][Wm]::ShowWindow($hwnd, 9)   # SW_RESTORE
    Start-Sleep -Seconds 3
    $evB = (Receive-Job -Job $job -Wait 2>&1) | Out-String
    Start-Sleep -Seconds 1
    Update-ServerOrigin
    $nMax = ([regex]::Matches($evB, 'window\.maximized')).Count
    $nRes = ([regex]::Matches($evB, 'window\.restored')).Count
    $bBack = Get-MineByExclude $mineA.id
    $rectOk = ($bBack -ne $null -and $bBack.x -eq 0 -and $bBack.y -eq 0 -and
               $bBack.w -eq 1280 -and $bBack.h -eq 680)
    $dsz = Get-LastDesktopSize
    $logOk = ($dsz -ne $null -and $dsz[0] -eq 1280 -and $dsz[1] -eq 720 -and $dsz[2] -ge 1)
    if ($nMax -eq 0 -and $nRes -eq 0 -and $rectOk -and $logOk) {
        Write-Host ("os-restore-remaximize: PASS (no event by design, log {0}x{1} n={2}, layer back to {3}x{4} @ 0,0)" -f
            $dsz[0], $dsz[1], $dsz[2], $bBack.w, $bBack.h)
    } else {
        $ok = $false
        Write-Host ("os-restore-remaximize: FAIL (maximized={0}, restored={1}, rect={2} want 1280x680@0,0, log={3}, events={4})" -f
            $nMax, $nRes, $bBack, ($dsz -join 'x'), $evB)
    }
}

# --- 6. cleanup --------------------------------------------------------------
Stop-ProbeProcs
Remove-Item $srvLog, $srvErr -ErrorAction SilentlyContinue

if ($ok) { Write-Host "PASS: desktop resize"; exit 0 }
else { Write-Host "FAIL: desktop resize"; exit 1 }