# probe_terminal_select.ps1 - docs/26 step 2 / docs/40 Task 3: terminal text
# selection + clipboard (Ctrl+Shift+C/V, bracketed paste) against the SINGLE-
# PROCESS terminal (jkdesktop.exe terminal), end to end with synthetic
# SendInput. PASS/FAIL via exit code. PowerShell 5.1 compatible. ASCII-only on
# purpose: a BOM-less .ps1 with non-ASCII bytes is read as cp949 and the
# parser breaks (docs/15 lesson).
#
# This probe does NOT go through the window server: `jkdesktop.exe terminal`
# creates its OWN SDL window (JKApplication/JKRenderThread legacy pipeline,
# TerminalApp::Init("Terminal", 800, 500)) - the view IS the main window and
# paints its own chrome (border 2 + title 24, JKWindow.h kBorder/kTitle).
# Window discovery is FindWindow("SDL_app", "Terminal") - same SDL class the
# server probe uses, title from the SDL_CreateWindow call (the view SetTitle
# coincides). -WindowStyle Hidden hides the SDL window (STARTUPINFO SW_HIDE
# sticks, MainWindowHandle stays 0), so show + pin + focus it exactly like
# probe_agent_maximize.ps1 does for the server - with one addition: this probe
# sends KEYBOARD input too, so it verifies GetForegroundWindow == terminal
# before typing (synthetic keys go to whatever has focus).
#
# Coordinate convention (probe_agent_maximize.ps1 / docs/35 lesson 8):
# per-monitor-v2 DPI aware, scale = physClientW / 800 (the renderer ratio -
# SDL logical window space, the space ev.x/y and kTermCellW/H live in), mouse
# batches carry MOUSEEVENTF_ABSOLUTE|MOUSEEVENTF_VIRTUALDESK (multi-monitor),
# one atomic SendInput batch per move/click stage.
#
# Screen READBACK trick: the feature under test doubles as an output probe.
# A full-screen drag + Ctrl+Shift+C puts the visible grid rows into the
# clipboard (ExtractSelectedText: one line per row, trailing empty cells
# trimmed, rows joined with \n - empty rows are never skipped, so split-line
# index == grid row). The probe never assumes a fixed row for the "hello"
# output: it reads the screen first, finds the row, then drag-selects it.
#
# Selection visual diagnostic (not pass/fail): a selected cell renders with
# fg/bg swapped, so its background is themeFg 0xCCCCCC (light) vs the normal
# themeBg 0x0C0C0C (dark). A CopyFromScreen pixel sample at a text cell after
# the drag tells drag-OK vs chord-broken apart when the clipboard check fails.
# CAVEAT: cells the SHELL colored (PSReadLine error lines render red bg) do
# not visibly swap - sample a default-colored cell only.
#
# Keyboard delivery lessons (SDL 2.32 + this rig):
# - synthetic VK input MUST carry a scan code (MapVirtualKey), else SDL drops
#   the keydown as SDL_SCANCODE_UNKNOWN (see the Wm::Vk comment below);
# - Enter must be VK_RETURN (SDL_SendKeyboardText drops '\r' as unprintable);
# - ctrl+shift+C is swallowed system-wide by a RegisterHotKey owner on this
#   rig (mods pass, the C keydown is removed before the focused window), so
#   the chord letter is PostMessage'd straight to the terminal window.
#
# Geometry: window 800x500 logical, client (2,24)-(798,498) = 796x474 ->
# cols = 796/8 = 99, rows = 474/16 = 29 (kTermCellW=8, kTermCellH=16,
# TerminalView.h). Cell (c,r) center = (2 + c*8 + 4, 24 + r*16 + 8) logical.
$ErrorActionPreference = "Continue"
$build = "I:\progwork\JKENGINE\engine\build"
$exe   = "$build\jkdesktop.exe"

$winW = 800; $winH = 500
$border = 2; $titleH = 24
$cellW = 8;  $cellH = 16
$cols = 99   # floor((800 - 2*2) / 8)
$rows = 29   # floor((500 - 24 - 2) / 16)

function Stop-ProbeProcs {
    # Terminal single-process runs as its own jkdesktop.exe with the bare
    # "terminal" argument - never by image name alone (lesson 42: the server
    # shares the jkdesktop.exe image name).
    Get-CimInstance Win32_Process -Filter "Name='jkdesktop.exe'" |
        Where-Object { $_.CommandLine -match 'terminal' } |
        ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
}

Stop-ProbeProcs
Start-Sleep -Seconds 1

# --- synthetic input setup (probe_agent_maximize convention) ----------------
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
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern int GetSystemMetrics(int index);
    [DllImport("user32.dll")] public static extern uint MapVirtualKey(uint code, uint type);
    [DllImport("user32.dll", SetLastError = true)]
    public static extern uint SendInput(uint n, INPUT[] p, int size);
    public struct RECT { public int L, T, R, B; }
    public struct POINT { public int X, Y; }
    [StructLayout(LayoutKind.Sequential)]
    public struct MOUSEINPUT { public int dx; public int dy; public uint mouseData;
        public uint dwFlags; public uint time; public IntPtr dwExtraInfo; }
    [StructLayout(LayoutKind.Sequential)]
    public struct KBDINPUT { public ushort wVk; public ushort wScan;
        public uint dwFlags; public uint time; public IntPtr dwExtraInfo; }
    [StructLayout(LayoutKind.Explicit)]
    public struct INPUTU { [FieldOffset(0)] public MOUSEINPUT mi;
        [FieldOffset(0)] public KBDINPUT ki; }
    [StructLayout(LayoutKind.Sequential)]
    public struct INPUT { public uint type; public INPUTU u; }
    public static readonly int Size = Marshal.SizeOf(typeof(INPUT));

    static int NormX(int px) {
        int vx = GetSystemMetrics(76), vw = GetSystemMetrics(78);
        return (int)Math.Round((px - vx) * 65535.0 / (vw - 1));
    }
    static int NormY(int py) {
        int vy = GetSystemMetrics(77), vh = GetSystemMetrics(79);
        return (int)Math.Round((py - vy) * 65535.0 / (vh - 1));
    }
    // MOUSEEVENTF_MOVE|ABSOLUTE|VIRTUALDESK = 0xC001 for every move (VIRTUALDESK
    // is MANDATORY on multi-monitor: ABSOLUTE alone normalizes over the primary
    // monitor only and drifts every point by the virtual-desktop origin,
    // docs/39). Button flags 2/4 are positionless and follow the cursor.
    static INPUT Mouse(uint flags, int nx, int ny) {
        INPUT i = new INPUT(); i.type = 0;
        i.u.mi.dx = nx; i.u.mi.dy = ny; i.u.mi.mouseData = 0;
        i.u.mi.dwFlags = flags; i.u.mi.time = 0; i.u.mi.dwExtraInfo = IntPtr.Zero;
        return i;
    }
    static INPUT Key(ushort vk, ushort scan, uint flags) {
        INPUT i = new INPUT(); i.type = 1;
        i.u.ki.wVk = vk; i.u.ki.wScan = scan; i.u.ki.dwFlags = flags;
        i.u.ki.time = 0; i.u.ki.dwExtraInfo = IntPtr.Zero;
        return i;
    }
    // SCAN CODE IS MANDATORY on synthetic VK input: SDL 2.32 maps the message
    // lParam scan code through WindowsScanCodeToSDLScanCode and DROPS the
    // keydown when it resolves to SDL_SCANCODE_UNKNOWN (only a tiny fallback
    // table maps VKs by wParam: arrows + VK_CONTROL + VK_V - notably NOT
    // VK_SHIFT or letters). wScan=0 events silently vanish: the chord letter
    // never arrived and Ctrl+Shift+C never fired. MapVirtualKey is what a real
    // keyboard report carries. (Unicode events keep scan = the char.)
    static INPUT Vk(ushort vk, uint flags) {
        return Key(vk, (ushort)MapVirtualKey(vk, 0), flags);
    }

    // move + left down, one atomic batch (the user's hand must not move the
    // cursor between the move and the press - probe_agent_maximize lesson).
    public static bool SendMoveDown(int px, int py) {
        INPUT[] seq = new INPUT[2];
        seq[0] = Mouse(0xC001, NormX(px), NormY(py));
        seq[1] = Mouse(2, 0, 0);   // LEFTDOWN
        return SendInput(2, seq, Size) == 2;
    }
    public static bool SendMoveTo(int px, int py) {
        INPUT[] seq = new INPUT[1];
        seq[0] = Mouse(0xC001, NormX(px), NormY(py));
        return SendInput(1, seq, Size) == 1;
    }
    // move + left up, one atomic batch (release at the exact end cell).
    public static bool SendMoveUp(int px, int py) {
        INPUT[] seq = new INPUT[2];
        seq[0] = Mouse(0xC001, NormX(px), NormY(py));
        seq[1] = Mouse(4, 0, 0);   // LEFTUP
        return SendInput(2, seq, Size) == 2;
    }
    // KEYEVENTF_UNICODE = 4, KEYEVENTF_KEYUP = 2. A unicode down/up pair makes
    // Windows synthesize WM_CHAR -> SDL_TEXTINPUT -> JKEventType::Char, which
    // TerminalView forwards to the pty verbatim. PRINTABLE chars only: SDL's
    // SDL_SendKeyboardText drops anything < ' ' or DEL, so '\r' typed this way
    // vanishes - Enter MUST go through the VK_RETURN KeyDown path
    // (HandleKeyDown -> "\r"), which also means a chord's control WM_CHAR
    // (0x03 for ctrl+C, 0x16 for ctrl+V) can never leak into the pty.
    public static bool SendUnicodeChar(char ch) {
        INPUT[] seq = new INPUT[2];
        seq[0] = Key(0, ch, 4);
        seq[1] = Key(0, ch, 4 | 2);
        return SendInput(2, seq, Size) == 2;
    }
    public static bool SendVkTap(ushort vk) {
        INPUT[] seq = new INPUT[2];
        seq[0] = Vk(vk, 0);
        seq[1] = Vk(vk, 2);
        return SendInput(2, seq, Size) == 2;
    }
    // ctrl down + shift down in ONE batch, then the letter down+up in ONE
    // batch (modifiers stay logically down across the batches - the view
    // reads the chord on the letter KeyDown; bare modifier KeyDowns never
    // clear the selection, TerminalView::HandleKeyDown).
    public static bool SendModsDown(ushort vk1, ushort vk2) {
        INPUT[] seq = new INPUT[2];
        seq[0] = Vk(vk1, 0);
        seq[1] = Vk(vk2, 0);
        return SendInput(2, seq, Size) == 2;
    }
    public static bool SendModsUp(ushort vk1, ushort vk2) {
        INPUT[] seq = new INPUT[2];
        seq[0] = Vk(vk1, 2);
        seq[1] = Vk(vk2, 2);
        return SendInput(2, seq, Size) == 2;
    }
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint msg, IntPtr w, IntPtr l);
    [DllImport("user32.dll")] public static extern IntPtr GetOpenClipboardWindow();
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
}
"@
# Per-monitor-v2 BEFORE any geometry call: GetClientRect/ClientToScreen then
# report physical px (probe_agent_maximize convention).
[Wm]::SetProcessDpiAwarenessContext([IntPtr](-4)) | Out-Null

function Get-TermHwnd {
    for ($i = 0; $i -lt 50; ++$i) {
        $p = Get-CimInstance Win32_Process -Filter "Name='jkdesktop.exe'" |
            Where-Object { $_.CommandLine -match 'terminal' } | Select-Object -First 1
        if ($p) {
            $h = [Wm]::FindWindow("SDL_app", "Terminal")
            if ($h -ne [IntPtr]::Zero) { return $h }
        }
        Start-Sleep -Milliseconds 200
    }
    return [IntPtr]::Zero
}

$proc = Start-Process -FilePath $exe -ArgumentList "terminal" `
    -WorkingDirectory $build -WindowStyle Hidden -PassThru
Start-Sleep -Seconds 4

$hwnd = Get-TermHwnd
if ($hwnd -ne [IntPtr]::Zero -and -not [Wm]::IsWindowVisible($hwnd)) {
    [void][Wm]::ShowWindow($hwnd, 8)   # SW_SHOWNOACTIVATE
    Start-Sleep -Milliseconds 500
}
# Pin above everything (probe convention): synthetic clicks must land on the
# terminal, not on whatever the desktop session has on top. Killed at cleanup.
if ($hwnd -ne [IntPtr]::Zero) {
    [void][Wm]::SetWindowPos($hwnd, [IntPtr](-1), 0, 0, 0, 0, 0x13)
}

$crect = New-Object Wm+RECT
[Wm]::GetClientRect($hwnd, [ref]$crect) | Out-Null
$co = New-Object Wm+POINT
$co.X = 0; $co.Y = 0
[Wm]::ClientToScreen($hwnd, [ref]$co) | Out-Null
$scale = ($crect.R - $crect.L) / [double]$winW

# Logical window point -> screen (physical) point. Same trap as the maximize
# probe: Pt already returns SCREEN points; never Pt a second time.
function Pt([double]$lx, [double]$ly) {
    return @([int][math]::Round($co.X + $lx * $scale),
             [int][math]::Round($co.Y + $ly * $scale))
}
function Cell-Center([int]$c, [int]$r) {
    return (Pt ($border + $c * $cellW + 4) ($titleH + $r * $cellH + 8))
}
function Invoke-Click([int]$c, [int]$r) {
    $p = Cell-Center $c $r
    [void][Wm]::SendMoveDown($p[0], $p[1])
    Start-Sleep -Milliseconds 60
    [void][Wm]::SendMoveUp($p[0], $p[1])
    Start-Sleep -Milliseconds 250
}
function Invoke-Drag([int]$c0, [int]$r0, [int]$c1, [int]$r1) {
    $a = Cell-Center $c0 $r0
    $b = Cell-Center $c1 $r1
    [void][Wm]::SendMoveDown($a[0], $a[1])
    Start-Sleep -Milliseconds 80
    for ($s = 1; $s -lt 5; ++$s) {
        $mx = $a[0] + [int](($b[0] - $a[0]) * $s / 5)
        $my = $a[1] + [int](($b[1] - $a[1]) * $s / 5)
        [void][Wm]::SendMoveTo($mx, $my)
        Start-Sleep -Milliseconds 30
    }
    [void][Wm]::SendMoveUp($b[0], $b[1])
    Start-Sleep -Milliseconds 200
}
function Send-Chars([string]$text) {
    foreach ($ch in $text.ToCharArray()) {
        [void][Wm]::SendUnicodeChar($ch)
        Start-Sleep -Milliseconds 20
    }
}
function Send-Enter {
    [void][Wm]::SendVkTap(0x0D)   # VK_RETURN -> SDL_KEYDOWN -> "\r"
    Start-Sleep -Milliseconds 400
}
# VK_CONTROL = 0x11, VK_SHIFT = 0x10. Mods go through real SendInput; the
# chord letter is posted as WM_KEYDOWN/WM_KEYUP DIRECTLY to the terminal
# window. Why: a RegisterHotKey owner on the test rig swallows ctrl+shift+C
# system-wide (instrumented proof: the ctrl/shift keydowns arrive at the app,
# the C keydown is removed from the input stream before the focused window
# sees it - ctrl+shift+V passes, C vanishes with no keydown logged at the
# app). A posted message bypasses the hotkey filter; the mods are still real
# input, so SDL's ev.option mod state is complete when the posted keydown is
# processed. (Posted MODIFIERS are unreliable - the letter must be the only
# posted key; SDL re-syncs mods against the real keyboard state.)
function Send-Chord([uint16]$vk) {
    [void][Wm]::SendModsDown(0x11, 0x10)
    Start-Sleep -Milliseconds 80
    $scan = [Wm]::MapVirtualKey($vk, 0)
    $dn = [int64]1 -bor ([int64]$scan -shl 16)            # repeat=1 | scancode
    $up = $dn -bor 0xC0000000                             # + prev/transition
    [void][Wm]::PostMessage($hwnd, 0x100, [IntPtr]$vk, [IntPtr]$dn)   # WM_KEYDOWN
    [void][Wm]::PostMessage($hwnd, 0x101, [IntPtr]$vk, [IntPtr]$up)   # WM_KEYUP
    Start-Sleep -Milliseconds 80
    [void][Wm]::SendModsUp(0x10, 0x11)
    Start-Sleep -Milliseconds 100
}
function Copy-Sel { Send-Chord 0x43 }          # Ctrl+Shift+C
function Paste-Clipboard { Send-Chord 0x56 }   # Ctrl+Shift+V

# Synthetic keys go to the FOREGROUND window - typing blind risks writing
# "cls" into the user's editor. Click + verify, retry a few times.
function Ensure-Focus {
    for ($i = 0; $i -lt 6; ++$i) {
        if ([Wm]::GetForegroundWindow() -eq $hwnd) { return $true }
        Invoke-Click ([int]($cols / 2)) ($rows - 2)
        if ([Wm]::GetForegroundWindow() -eq $hwnd) { return $true }
        [void][Wm]::SetForegroundWindow($hwnd)
        Start-Sleep -Milliseconds 300
    }
    return ([Wm]::GetForegroundWindow() -eq $hwnd)
}

# Pixel sample of the window (diagnostic only): brightness at a cell center.
# Selected cell bg = themeFg 0xCCCCCC (bright) vs normal bg 0x0C0C0C (dark).
function Get-CellBrightness([int]$c, [int]$r) {
    try {
        Add-Type -AssemblyName System.Drawing
        $wr = New-Object Wm+RECT
        [void][Wm]::GetWindowRect($hwnd, [ref]$wr)
        $w = $wr.R - $wr.L; $h = $wr.B - $wr.T
        $bmp = New-Object System.Drawing.Bitmap($w, $h)
        $g = [System.Drawing.Graphics]::FromImage($bmp)
        $g.CopyFromScreen($wr.L, $wr.T, 0, 0, (New-Object System.Drawing.Size($w, $h)))
        $p = Cell-Center $c $r
        $px = $bmp.GetPixel($p[0] - $wr.L, $p[1] - $wr.T)
        $g.Dispose(); $bmp.Dispose()
        return (($px.R + $px.G + $px.B) / 3)
    } catch { return -1 }
}
function Save-Shot([string]$path) {
    try {
        Add-Type -AssemblyName System.Drawing
        $wr = New-Object Wm+RECT
        [void][Wm]::GetWindowRect($hwnd, [ref]$wr)
        $w = $wr.R - $wr.L; $h = $wr.B - $wr.T
        $bmp = New-Object System.Drawing.Bitmap($w, $h)
        $g = [System.Drawing.Graphics]::FromImage($bmp)
        $g.CopyFromScreen($wr.L, $wr.T, 0, 0, (New-Object System.Drawing.Size($w, $h)))
        $bmp.Save($path)
        $g.Dispose(); $bmp.Dispose()
        return $true
    } catch { return $false }
}

# Full-screen select + copy: returns one string per visible grid row (split
# line index == grid row - every row contributes exactly one line, empty rows
# included). The readback mechanism IS the feature under test. The clipboard is
# preset to a newline-less sentinel so a copy racing SDL_SetClipboardText (or a
# chord that landed on a window which lost focus) reads as failure and is
# retried - a successful full-screen copy ALWAYS carries >= 28 newlines.
function Read-Screen {
    for ($attempt = 0; $attempt -lt 3; ++$attempt) {
        try { Set-Clipboard -Value " " -ErrorAction Stop } catch { }
        Invoke-Drag 0 0 ($cols - 1) ($rows - 1)
        Copy-Sel
        # SDL_SetClipboardText can lose an OpenClipboard race against other
        # clipboard watchers on the desktop; poll instead of a fixed sleep.
        $raw = $null
        for ($poll = 0; $poll -lt 15; ++$poll) {
            Start-Sleep -Milliseconds 200
            $raw = Get-Clipboard -Raw
            if ($null -ne $raw -and $raw.Contains("`n")) { break }
        }
        if ($null -ne $raw -and $raw.Contains("`n")) {
            return ($raw -split '\r?\n')
        }
        Write-Host ("  read-screen retry {0}: no copy (len {1}, fg {2})" -f
            $attempt, $(if ($raw) { $raw.Length } else { 0 }),
            [Wm]::GetForegroundWindow())
        Write-Host ("  clipboard: [{0}]" -f
            ($(if ($raw) { $raw -replace "`r", '\r' -replace "`n", '\n' } else { "<null>" })))
        $locker = [Wm]::GetOpenClipboardWindow()
        if ($locker -ne [IntPtr]::Zero) {
            [uint32]$pid2 = 0
            [void][Wm]::GetWindowThreadProcessId($locker, [ref]$pid2)
            $pname = try { (Get-Process -Id $pid2 -ErrorAction Stop).ProcessName } catch { "?" }
            Write-Host ("  clipboard LOCKED by hwnd {0} pid {1} ({2})" -f $locker, $pid2, $pname)
        } else {
            Write-Host "  clipboard not locked at failure time"
        }
        $b1 = Get-CellBrightness 50 1
        $b5 = Get-CellBrightness 50 5
        Write-Host ("  diagnostic: cell(50,1) brightness {0:F0} / cell(50,5) {1:F0} (dark ~14 = no highlight, bright ~204 = selected)" -f $b1, $b5)
        [void](Save-Shot "$build\probe_terminal_select_dbg.png")
        if ([Wm]::GetForegroundWindow() -ne $hwnd) {
            [void](Ensure-Focus)
        }
        Start-Sleep -Milliseconds 400
    }
    return @("")
}

$ok = $true

# --- 1. spawn + window -------------------------------------------------------
$vis = ($hwnd -ne [IntPtr]::Zero) -and [Wm]::IsWindowVisible($hwnd)
$cw = $crect.R - $crect.L; $chh = $crect.B - $crect.T
if ($vis -and $cw -gt 0 -and $chh -gt 0 -and $scale -gt 0) {
    Write-Host ("spawn+window: PASS (hwnd {0} client {1}x{2} scale {3:F2})" -f
        $hwnd, $cw, $chh, $scale)
} else {
    $ok = $false
    Write-Host ("spawn+window: FAIL (hwnd={0}, client={1}x{2})" -f $hwnd, $cw, $chh)
}

# --- 2. shell up + typed `echo hello` + Enter is on screen -------------------
if ($ok) {
    if (-not (Ensure-Focus)) {
        $ok = $false
        Write-Host ("focus: FAIL (foreground {0} != terminal {1})" -f
            [Wm]::GetForegroundWindow(), $hwnd)
    } else {
        # `cls` first: the prompt lands on row 0 regardless of startup scroll-
        # back, then `echo hello` + Enter. Chars typed as unicode events
        # (KEYEVENTF_UNICODE - exactly one Char event per char into the pty),
        # Enter as a scan-coded VK_RETURN tap.
        Send-Chars "cls"
        Send-Enter
        Start-Sleep -Milliseconds 800
        Send-Chars "echo hello"
        Send-Enter
        Start-Sleep -Milliseconds 1100

        $lines = Read-Screen
        $helloRows = @()
        for ($i = 0; $i -lt $lines.Count; ++$i) {
            if ($lines[$i] -eq "hello") { $helloRows += $i }
        }
        $echoLine = ($lines | Where-Object { $_ -match '^PS .*echo hello$' } |
            Select-Object -First 1)
        if ($helloRows.Count -ge 1 -and $echoLine) {
            Write-Host ("typed-echo: PASS ('{0}' -> output row {1})" -f
                $echoLine.Trim(), $helloRows[0])
        } else {
            $ok = $false
            Write-Host "typed-echo: FAIL (no standalone 'hello' line on screen)"
            Write-Host ("screen readback: [{0}]" -f ($lines -join " | "))
        }
        $script:helloRow = if ($helloRows.Count -ge 1) { $helloRows[0] } else { -1 }
    }
}

# --- 3. drag-select the output row + Ctrl+Shift+C -> clipboard ---------------
if ($ok) {
    $got = ""
    $bright = -1
    # The chord + clipboard set are proven correct; a clipboard-watcher losing
    # the OpenClipboard race on the desktop is environmental - re-drag and
    # re-copy instead of failing on the first attempt.
    for ($try = 0; $try -lt 3; ++$try) {
        try { Set-Clipboard -Value " " -ErrorAction Stop } catch { }
        Invoke-Drag 0 $script:helloRow ($cols - 1) $script:helloRow
        $bright = Get-CellBrightness ([int]($cols / 2)) $script:helloRow
        Copy-Sel
        for ($poll = 0; $poll -lt 15; ++$poll) {
            Start-Sleep -Milliseconds 200
            $clip = Get-Clipboard -Raw
            if ($null -ne $clip -and $clip.TrimEnd("`r", "`n") -eq "hello") { break }
        }
        if ($null -ne $clip) { $got = $clip.TrimEnd("`r", "`n") }
        if ($got -eq "hello") { break }
        Write-Host ("  drag-copy retry {0}: clipboard = '{1}'" -f
            $try, ($got -replace "`r", '\r' -replace "`n", '\n'))
    }
    if ($got -eq "hello") {
        Write-Host ("drag-copy: PASS (clipboard == 'hello', row {0})" -f $script:helloRow)
        # BONUS (not pass/fail): shoot the selected window for the report.
        if (Save-Shot "$build\probe_terminal_select_sel.png") {
            Write-Host "  bonus screenshot: $build\probe_terminal_select_sel.png"
        }
    } else {
        $ok = $false
        Write-Host ("drag-copy: FAIL (clipboard = '{0}')" -f
            ($got -replace "`r", '\r' -replace "`n", '\n'))
        Write-Host ("  diagnostic: selection pixel brightness at cell = {0:F0} (dark ~14 = no highlight, bright ~204 = selected)" -f $bright)
    }
}

# --- 4. Ctrl+Shift+V -> the pty receives the clipboard -----------------------
if ($ok) {
    # Type `echo ` at the fresh prompt (each Char also drops the selection,
    # spec s2 - already copied), then paste: the command line must read
    # `echo hello` (PSReadLine echoes the pasted bytes).
    Send-Chars "echo "
    Start-Sleep -Milliseconds 300
    Paste-Clipboard
    Start-Sleep -Milliseconds 800
    $linesP = Read-Screen
    $nEcho = ($linesP | Where-Object { $_ -match '^PS .*echo hello$' }).Count
    if ($nEcho -ge 2) {
        Write-Host ("paste-line: PASS (command line 'echo hello' x{0})" -f $nEcho)
    } else {
        $ok = $false
        Write-Host ("paste-line: FAIL (echo-hello lines = {0}, want >= 2)" -f $nEcho)
        Write-Host ("screen readback: [{0}]" -f ($linesP -join " | "))
    }
}
# 4b. Enter runs the pasted argument: a SECOND standalone hello line appears.
if ($ok) {
    Send-Enter
    Start-Sleep -Milliseconds 1000
    $linesR = Read-Screen
    $nHello = ($linesR | Where-Object { $_ -eq "hello" }).Count
    if ($nHello -ge 2) {
        Write-Host ("paste-exec: PASS (standalone 'hello' output x{0})" -f $nHello)
        [void](Save-Shot "$build\probe_terminal_select_paste.png")
        Write-Host "  bonus screenshot: $build\probe_terminal_select_paste.png"
    } else {
        $ok = $false
        Write-Host ("paste-exec: FAIL (standalone 'hello' lines = {0}, want >= 2)" -f $nHello)
        Write-Host ("screen readback: [{0}]" -f ($linesR -join " | "))
    }
}

# --- 5. cleanup --------------------------------------------------------------
if ($proc -and -not $proc.HasExited) {
    Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
}
Stop-ProbeProcs

if ($ok) { Write-Host "PASS: terminal select"; exit 0 }
else { Write-Host "FAIL: terminal select"; exit 1 }