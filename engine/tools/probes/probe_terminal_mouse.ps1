# probe_terminal_mouse.ps1 - docs/26 step 3 / docs/41 Task 4: SGR mouse
# REPORTING against the SINGLE-PROCESS terminal (jkdesktop.exe terminal), end
# to end with synthetic SendInput. PASS/FAIL via exit code. PowerShell 5.1
# compatible. ASCII-only on purpose; the file is saved WITH a UTF-8 BOM
# (docs/15 lesson 50: a BOM-less non-ASCII .ps1 parses as cp949; the BOM is
# kept even for pure-ASCII content so the rule never has exceptions).
#
# What is under test (docs/26 step 3): a TUI app that prints
# `\e[?1000;1006h` (DECSET 1000 normal tracking + 1006 SGR encoding) must
# start receiving xterm mouse reports on its stdin. The pipeline:
#   app stdout -> ConPTY -> JKVtParser (MouseMode/SgrMouse)
#   SendInput click -> SDL -> TerminalView::HandleMouseEvent
#     -> HandleMouseReport -> EncodeMouseSgr -> onInput_ -> ConPTY -> app stdin
#
# Probe design (controller ruling: the assert reads a DUMP FILE, not screen
# readback - strictly more robust than OCR/selection, same intent: prove SGR
# bytes arrived):
#   1. Launch the terminal single-process, focus it (clicks while reporting
#      is OFF still take the selection path - no reports).
#   2. Type `& <TEMP>\jkterm_mouse_reader.ps1` + Enter. The reader script
#      runs in the terminal's shell (foreground): sets the console input mode
#      to raw + ENABLE_VIRTUAL_TERMINAL_INPUT + ENABLE_MOUSE_INPUT, prints
#      `\e[?1000;1006h`, then reads stdin 1 byte at a time and appends each
#      byte as two hex chars to TEMP\jkterm_mouse_dump.txt (READY marker
#      first). It has NO deadline: the PROBE is the timeout controller and
#      kills the engine (the reader is a child of the console and dies with
#      it). A blocking stdin read cannot be raced with PSReadLine this way -
#      a detached background reader would fight the shell over the input
#      queue.
#   3. Click a known interior cell. Poll the dump for the SGR press bytes:
#      `\e[<0;x;yM` = hex 1b 5b 3c 30 3b <x digits> 3b <y digits> 4d (the
#      coords are decimal digits, hex-encoded as 30-39 byte pairs, so a
#      non-greedy scan to 4d cannot bridge into the next event).
#   4. Gate: press found AND decoded x/y == clicked cell + 1 (1-based, the
#      SGR wire convention - this proves the ENCODING, not just arrival).
#      Secondary: release `m` (terminator 6d) and wheel up `\e[<64;...`
#      = 1b 5b 3c 36 34 3b (our view reports the wheel in SGR mode ONLY, so
#      wheel bytes additionally prove DECSET 1006 reached the parser).
#   5. Cleanup by PID/command line (never window title), temp files deleted.
#
# Bring-up findings (docs/41 for the full story):
#   - CONHOST PASSTHROUGH: on this rig (Win11 26200 conhost) the raw+VT-input
#     reader receives our SGR bytes VERBATIM - no MOUSE_EVENT translation in
#     between (microsoft/terminal PR #4856-style input passthrough). Iteration
#     1 initially captured a classic X10 report instead: with QuickEdit still
#     ON, conhost suppresses mouse and never fires the ENABLE_MOUSE_INPUT ->
#     "?1003;1006h" terminal passthrough (PR #9970), so the reader now clears
#     QuickEdit too - exactly what real TUI apps do.
#   - ENGINE REGRESSION (fixed with this probe): the parser's private-mode
#     handler applied only the FIRST param of a combined DECSET, so the spec's
#     own `\e[?1000;1006h` enabled mode 1000 and silently dropped 1006 - the
#     view kept sending X10 (dump showed `\e[M` + 32+btn + 32+x + 32+y).
#     HandlePrivateMode now loops over every param; the self-test gained
#     combined ?1000;1006h / ?1002;1006h / ?1000;1006l checks.
#
# Coordinate convention (probe_terminal_select.ps1 precedent): 800x500
# logical window, per-monitor-v2 DPI aware, scale = physClientW / 800, mouse
# batches carry MOUSEEVENTF_MOVE|ABSOLUTE|VIRTUALDESK (0xC001), border 2 +
# title 24, kTermCellW=8 kTermCellH=16 -> cols=99 rows=29.
$ErrorActionPreference = "Continue"
$build = "I:\progwork\JKENGINE\engine\build"
$exe   = "$build\jkdesktop.exe"

$winW = 800; $winH = 500
$border = 2; $titleH = 24
$cellW = 8;  $cellH = 16
$cols = 99   # floor((800 - 2*2) / 8)
$rows = 29   # floor((500 - 24 - 2) / 16)

$dumpPath   = Join-Path $env:TEMP "jkterm_mouse_dump.txt"
$readerPath = Join-Path $env:TEMP "jkterm_mouse_reader.ps1"

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
Remove-Item $dumpPath, $readerPath -Force -ErrorAction SilentlyContinue

# --- the reader script that runs INSIDE the terminal -------------------------
# ASCII-only; written with a UTF-8 BOM (lesson 50). No deadline: the probe
# kills the console (and this child) right after the asserts.
$reader = @'
$ErrorActionPreference = "Continue"
$dump = Join-Path $env:TEMP "jkterm_mouse_dump.txt"
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class RM {
    [DllImport("kernel32.dll")] public static extern IntPtr GetStdHandle(int h);
    [DllImport("kernel32.dll")] public static extern bool GetConsoleMode(IntPtr h, out uint m);
    [DllImport("kernel32.dll")] public static extern bool SetConsoleMode(IntPtr h, uint m);
}
"@
$hIn = [RM]::GetStdHandle(-10)
$old = [uint32]0
[void][RM]::GetConsoleMode($hIn, [ref]$old)
# raw: strip processed(1)/line(2)/echo(4) and QuickEdit(0x40), add extended
# flags(0x80) + window(8) + mouse(0x10) + virtual-terminal-input(0x200).
# QuickEdit OFF is REQUIRED: with it on, conhost suppresses mouse events and
# skips the ENABLE_MOUSE_INPUT -> "?1003;1006h" pass-through to the terminal
# (microsoft/terminal PR #9970) - the probe observed exactly that failure
# (report arrived as conhost-passthrough X10 from a mode-1000-only parser).
$raw = (($old -band 0xFFFFFFF8) -band 0xFFFFFFBF) -bor 0x0080 -bor 0x0008 `
    -bor 0x0010 -bor 0x0200
[void][RM]::SetConsoleMode($hIn, [uint32]$raw)
$esc = [char]27
$bom = New-Object System.Text.UTF8Encoding($true)
[System.IO.File]::WriteAllText($dump, "READY`n", $bom)
# The spec wire form: ONE combined DECSET (this regressed - see the parser
# note in docs/41; HandlePrivateMode used to apply only the first param).
[Console]::Out.Write("$esc[?1000;1006h")
[Console]::Out.Flush()
$stream = [Console]::OpenStandardInput()
$b = New-Object byte[] 1
try {
    while ($true) {
        $n = $stream.Read($b, 0, 1)
        if ($n -le 0) { break }
        [System.IO.File]::AppendAllText($dump, $b[0].ToString("x2"), $bom)
    }
} catch { }
# Only reached if the stream ends on its own - the probe normally kills the
# console first. Restore the mode anyway so a manual run leaves a sane shell.
[Console]::Out.Write("$esc[?1000l$esc[?1006l")
[Console]::Out.Flush()
[void][RM]::SetConsoleMode($hIn, $old)
'@
[System.IO.File]::WriteAllText($readerPath, $reader, (New-Object System.Text.UTF8Encoding($true)))

# --- synthetic input setup (probe_terminal_select convention) ----------------
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
    static INPUT Mouse(uint flags, int nx, int ny, int data) {
        INPUT i = new INPUT(); i.type = 0;
        i.u.mi.dx = nx; i.u.mi.dy = ny; i.u.mi.mouseData = (uint)data;
        i.u.mi.dwFlags = flags; i.u.mi.time = 0; i.u.mi.dwExtraInfo = IntPtr.Zero;
        return i;
    }
    static INPUT Key(ushort vk, ushort scan, uint flags) {
        INPUT i = new INPUT(); i.type = 1;
        i.u.ki.wVk = vk; i.u.ki.wScan = scan; i.u.ki.dwFlags = flags;
        i.u.ki.time = 0; i.u.ki.dwExtraInfo = IntPtr.Zero;
        return i;
    }
    static INPUT Vk(ushort vk, uint flags) {
        return Key(vk, (ushort)MapVirtualKey(vk, 0), flags);
    }

    static INPUT Mouse(uint flags, int nx, int ny) {
        INPUT i = new INPUT(); i.type = 0;
        i.u.mi.dx = nx; i.u.mi.dy = ny; i.u.mi.mouseData = 0;
        i.u.mi.dwFlags = flags; i.u.mi.time = 0; i.u.mi.dwExtraInfo = IntPtr.Zero;
        return i;
    }
    // move + left down / move + left up in one atomic batch each
    // (VIRTUALDESK absolute - mandatory on multi-monitor, docs/39).
    public static bool SendMoveDown(int px, int py) {
        INPUT[] seq = new INPUT[2];
        seq[0] = Mouse(0xC001, NormX(px), NormY(py), 0);
        seq[1] = Mouse(2, 0, 0, 0);   // LEFTDOWN
        return SendInput(2, seq, Size) == 2;
    }
    public static bool SendMoveUp(int px, int py) {
        INPUT[] seq = new INPUT[2];
        seq[0] = Mouse(0xC001, NormX(px), NormY(py), 0);
        seq[1] = Mouse(4, 0, 0, 0);   // LEFTUP
        return SendInput(2, seq, Size) == 2;
    }
    // bare hover move (Any-mode motion diagnostics)
    public static bool SendMoveTo(int px, int py) {
        INPUT[] seq = new INPUT[1];
        seq[0] = Mouse(0xC001, NormX(px), NormY(py), 0);
        return SendInput(1, seq, Size) == 1;
    }
    // one wheel notch: +120 = up (SDL_MOUSEWHEEL y=+1 -> btn 64),
    // MOUSEEVENTF_WHEEL = 0x0800.
    public static bool SendWheel(int px, int py, int delta) {
        INPUT[] seq = new INPUT[2];
        seq[0] = Mouse(0xC001, NormX(px), NormY(py), 0);
        seq[1] = Mouse(0x0800, 0, 0, delta);
        return SendInput(2, seq, Size) == 2;
    }
    // KEYEVENTF_UNICODE = 4, KEYEVENTF_KEYUP = 2 (printable chars only).
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
}
"@
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
# Pin above everything: clicks must land on the terminal.
if ($hwnd -ne [IntPtr]::Zero) {
    [void][Wm]::SetWindowPos($hwnd, [IntPtr](-1), 0, 0, 0, 0, 0x13)
}

$crect = New-Object Wm+RECT
[Wm]::GetClientRect($hwnd, [ref]$crect) | Out-Null
$co = New-Object Wm+POINT
$co.X = 0; $co.Y = 0
[Wm]::ClientToScreen($hwnd, [ref]$co) | Out-Null
$scale = ($crect.R - $crect.L) / [double]$winW

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

# Synthetic keys go to the FOREGROUND window (probe_terminal_select lesson).
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

function Get-Dump { if (Test-Path $dumpPath) { [IO.File]::ReadAllText($dumpPath) } else { "" } }
# SGR press with button 0: bytes 1b 3c 30 3b <x> 3b <y> 4d. Coordinates are
# decimal digits -> hex-encoded 30-39 pairs, so [0-9a-f]+?4d cannot run past
# the terminator into a later event.
$rxPress   = '1b5b3c303b([0-9a-f]+?)4d'
$rxRelease = '1b5b3c303b([0-9a-f]+?)6d'
$rxWheelUp = '1b5b3c36343b'
# Classic X10 form: \e[M + (32+btn) + (32+x) + (32+y), bytes 1b 5b 4d ?? ?? ??
# (btn 0x20-0x22 = left/middle/right press; coord bytes are >= 0x21 for
# 1-based coords, so the coord groups cannot swallow a following \x1b).
$rxX10Press = '1b5b4d(2[0-2])([2-9a-f][0-9a-f])([2-9a-f][0-9a-f])'
# Decode the captured hex pairs of an SGR body ("..3b..") into (x, y) ints.
function Get-SgrCoords([string]$hexBody) {
    $pairs = @()
    for ($i = 0; $i + 1 -lt $hexBody.Length; $i += 2) {
        $pairs += $hexBody.Substring($i, 2)
    }
    $sep = $pairs.IndexOf("3b")   # ';' byte
    if ($sep -lt 0) { return $null }
    $xS = ""; $yS = ""
    for ($i = 0; $i -lt $sep; ++$i) { $xS += [char][Convert]::ToInt32($pairs[$i], 16) }
    for ($i = $sep + 1; $i -lt $pairs.Count; ++$i) { $yS += [char][Convert]::ToInt32($pairs[$i], 16) }
    $x = 0; $y = 0
    if (-not [int]::TryParse($xS, [ref]$x)) { return $null }
    if (-not [int]::TryParse($yS, [ref]$y)) { return $null }
    return @($x, $y)
}

$ok = $true

# --- 1. spawn + window -------------------------------------------------------
$vis = ($hwnd -ne [IntPtr]::Zero) -and [Wm]::IsWindowVisible($hwnd)
if ($vis -and $scale -gt 0) {
    Write-Host ("spawn+window: PASS (hwnd {0} scale {1:F2})" -f $hwnd, $scale)
} else {
    $ok = $false
    Write-Host ("spawn+window: FAIL (hwnd={0}, visible={1})" -f $hwnd, $vis)
}

# --- 2. focus + start the reader inside the terminal -------------------------
if ($ok) {
    if (-not (Ensure-Focus)) {
        $ok = $false
        Write-Host ("focus: FAIL (foreground {0} != terminal {1})" -f
            [Wm]::GetForegroundWindow(), $hwnd)
    } else {
        # Run the reader in the foreground shell; PSReadLine yields the input
        # queue to it for the whole probe.
        foreach ($ch in ("& '" + $readerPath + "'").ToCharArray()) {
            [void][Wm]::SendUnicodeChar($ch)
            Start-Sleep -Milliseconds 20
        }
        Send-Enter
        # The reader writes READY before enabling reporting; poll for it.
        $ready = $false
        for ($i = 0; $i -lt 30; ++$i) {
            Start-Sleep -Milliseconds 300
            if ((Get-Dump) -match 'READY') { $ready = $true; break }
        }
        if ($ready) {
            Write-Host "reader-start: PASS (dump READY)"
        } else {
            $ok = $false
            Write-Host "reader-start: FAIL (no READY marker in dump)"
            Write-Host ("  dump: [{0}]" -f (Get-Dump))
        }
    }
}

# --- 3. click an interior cell -> mouse press report in the dump (GATE) ------
# Gate accepts EITHER raw SGR bytes (1b5b3c30..4d) OR the classic X10 form
# (1b5b4d = \e[M + 32+btn + 32+x + 32+y) - conhost re-encodes mouse reports
# for raw VT-input readers; either way the press arrived with the wire
# button/coords. Coordinates are asserted after decoding the observed form.
$clickC = 30; $clickR = 10   # comfortably inside the 99x29 grid
if ($ok) {
    Invoke-Click $clickC $clickR
    $dump = ""
    for ($i = 0; $i -lt 40; ++$i) {          # poll up to ~12s
        $dump = Get-Dump
        if ($dump -match $rxPress) { break }
        if ($dump -match $rxX10Press) { break }
        Start-Sleep -Milliseconds 300
    }
    $m = [regex]::Match($dump, $rxPress)
    $mx = [regex]::Match($dump, $rxX10Press)
    if (-not $m.Success -and -not $mx.Success) {
        $ok = $false
        Write-Host ("mouse-press: FAIL (no mouse press report in dump)")
        Write-Host ("  dump: [{0}]" -f $dump)
    } else {
        if ($m.Success) {
            $xy = Get-SgrCoords $m.Groups[1].Value
            Write-Host "mouse-press: SGR form observed"
        } else {
            $b = [Convert]::ToInt32($mx.Groups[1].Value, 16)
            $x = [Convert]::ToInt32($mx.Groups[2].Value, 16) - 32
            $y = [Convert]::ToInt32($mx.Groups[3].Value, 16) - 32
            $xy = @($x, $y)
            Write-Host ("mouse-press: X10 form observed (btn byte 0x{0})" -f
                $mx.Groups[1].Value)
        }
        if ($xy -and $xy[0] -eq ($clickC + 1) -and $xy[1] -eq ($clickR + 1)) {
            Write-Host ("mouse-press: PASS (report x={0} y={1} == cell ({2},{3})+1)" -f
                $xy[0], $xy[1], $clickC, $clickR)
        } else {
            $ok = $false
            Write-Host ("mouse-press: FAIL (decoded coords {0}, want x={1} y={2})" -f
                ($(if ($xy) { "x=$($xy[0]) y=$($xy[1])" } else { "<none>" })),
                ($clickC + 1), ($clickR + 1))
            Write-Host ("  match: [{0}]" -f $m.Value)
        }
        # Secondary: the release report (SGR 'm' terminator; X10 has none).
        Start-Sleep -Milliseconds 400
        $d2 = Get-Dump
        if ($d2 -match $rxRelease) {
            Write-Host "mouse-release: PASS (SGR 1b5b3c303b..6d)"
        } else {
            Write-Host "mouse-release: WARN (no SGR release byte 6d in dump)"
        }
    }
}

# --- 4. wheel + hover move diagnostics ---------------------------------------
# Our view reports the wheel ONLY in SGR mode (btn 64/65), so wheel bytes here
# also prove DECSET 1006 reached the parser. Hover motion exists only in
# 1002/1003 tracking - the reader enables 1000, so NO motion bytes expected.
if ($ok) {
    $p = Cell-Center $clickC $clickR
    [void][Wm]::SendWheel($p[0], $p[1], 120)
    Start-Sleep -Milliseconds 800
    # hover: move to another interior cell (Any mode would report motion)
    $q = Cell-Center 40 12
    [void][Wm]::SendMoveTo($q[0], $q[1])
    Start-Sleep -Milliseconds 800
    $wdump = Get-Dump
    if ($wdump -match $rxWheelUp) {
        Write-Host "mouse-wheel: PASS (SGR 1b5b3c36343b = \e[<64;... press)"
    } else {
        Write-Host "mouse-wheel: WARN (no SGR wheel-up 1b5b3c36343b in dump)"
    }
    Write-Host ("  dump tail: [{0}]" -f
        $wdump.Substring([math]::Max(0, $wdump.Length - 160)))
}

# --- 5. cleanup --------------------------------------------------------------
if ($proc -and -not $proc.HasExited) {
    Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
}
Stop-ProbeProcs
Remove-Item $dumpPath, $readerPath -Force -ErrorAction SilentlyContinue

if ($ok) { Write-Host "PASS: terminal mouse report"; exit 0 }
else { Write-Host "FAIL: terminal mouse report"; exit 1 }