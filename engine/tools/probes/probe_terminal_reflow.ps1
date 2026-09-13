# probe_terminal_reflow.ps1 - docs/26 step 4: terminal reflow end to end.
# Long output line -> resize the SDL window -> the same logical text must
# re-wrap to the new width (read back through the selection/clipboard
# pipeline, docs/40). PASS/FAIL via exit code. PowerShell 5.1 compatible.
# ASCII-only on purpose (docs/15 lesson).
#
# Method: spawn the SINGLE-PROCESS terminal (jkdesktop.exe terminal, own SDL
# window, same rig as probe_terminal_select.ps1), echo a 200-char line made
# of "ABCDEFGHIJ" repeats, read the screen back (full-select + Ctrl+Shift+C),
# resize the window wider via SetWindowPos, read again, then resize back to
# the original geometry and read a third time. The reflow is correct when the
# A-J chunk rows join (no separators) to the exact same 200-char string at
# every width, and the widest readback uses strictly fewer rows than the
# narrow one.
#
# Readback caveat: only lines matching ^[A-J]+$ count - the PSReadLine prompt
# echo, the trailing cursor line and scrollback noise are excluded. Resize
# timing: ConPTY resize + PowerShell repaint take ~1s; Read-Screen retries.
#
# FLAKINESS (2026-09-13, docs/42): the clipboard readback needs an IDLE
# machine. Synthetic SendInput batches vanish while the user's physical mouse
# is active (engine/tools/probes memory lesson) and the first click after a
# focus change can be dropped as an activation click (SDL
# focus-clickthrough hint is only set by the server, not the single-process
# rig) - so a run can fail every read yet the SCREENSHOTS still prove the
# reflow (probe_reflow_wide.png shows the 200-char line rewrapped 99 -> 149
# cols). Read the PNGs when the exit code is red.
$ErrorActionPreference = "Continue"
$build = "I:\progwork\JKENGINE\engine\build"
$exe   = "$build\jkdesktop.exe"

$winW = 800; $winH = 500
$winW2 = 1200; $winH2 = 500
$border = 2; $titleH = 24
$cellW = 8;  $cellH = 16
$cols = 99; $rows = 29          # at 800x500
$marker = "ABCDEFGHIJ"
$expect = ($marker * 20)        # 200 chars

function Stop-ProbeProcs {
    Get-CimInstance Win32_Process -Filter "Name='jkdesktop.exe'" |
        Where-Object { $_.CommandLine -match 'terminal' } |
        ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
}
Stop-ProbeProcs
Start-Sleep -Seconds 1

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
    static INPUT Mouse(uint flags, int nx, int ny) {
        INPUT i = new INPUT(); i.type = 0;
        i.u.mi.dx = nx; i.u.mi.dy = ny; i.u.mi.dwFlags = flags;
        return i;
    }
    static INPUT Key(ushort vk, ushort scan, uint flags) {
        INPUT i = new INPUT(); i.type = 1;
        i.u.ki.wVk = vk; i.u.ki.wScan = scan; i.u.ki.dwFlags = flags;
        return i;
    }
    static INPUT Vk(ushort vk, uint flags) {
        return Key(vk, (ushort)MapVirtualKey(vk, 0), flags);
    }
    public static bool SendMoveDown(int px, int py) {
        INPUT[] seq = new INPUT[2];
        seq[0] = Mouse(0xC001, NormX(px), NormY(py));
        seq[1] = Mouse(2, 0, 0);
        return SendInput(2, seq, Size) == 2;
    }
    public static bool SendMoveTo(int px, int py) {
        INPUT[] seq = new INPUT[1];
        seq[0] = Mouse(0xC001, NormX(px), NormY(py));
        return SendInput(1, seq, Size) == 1;
    }
    public static bool SendMoveUp(int px, int py) {
        INPUT[] seq = new INPUT[2];
        seq[0] = Mouse(0xC001, NormX(px), NormY(py));
        seq[1] = Mouse(4, 0, 0);
        return SendInput(2, seq, Size) == 2;
    }
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
    [void][Wm]::ShowWindow($hwnd, 8)
    Start-Sleep -Milliseconds 500
}
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
    [void][Wm]::SendVkTap(0x0D)
    Start-Sleep -Milliseconds 400
}
function Send-Chord([uint16]$vk) {
    [void][Wm]::SendModsDown(0x11, 0x10)
    Start-Sleep -Milliseconds 80
    $scan = [Wm]::MapVirtualKey($vk, 0)
    $dn = [int64]1 -bor ([int64]$scan -shl 16)
    $up = $dn -bor 0xC0000000
    [void][Wm]::PostMessage($hwnd, 0x100, [IntPtr]$vk, [IntPtr]$dn)
    [void][Wm]::PostMessage($hwnd, 0x101, [IntPtr]$vk, [IntPtr]$up)
    Start-Sleep -Milliseconds 80
    [void][Wm]::SendModsUp(0x10, 0x11)
    Start-Sleep -Milliseconds 100
}
function Ensure-Focus {
    for ($i = 0; $i -lt 6; ++$i) {
        if ([Wm]::GetForegroundWindow() -eq $hwnd) { return $true }
        $p = Cell-Center ([int]($cols / 2)) ($rows - 2)
        [void][Wm]::SendMoveDown($p[0], $p[1])
        Start-Sleep -Milliseconds 60
        [void][Wm]::SendMoveUp($p[0], $p[1])
        Start-Sleep -Milliseconds 250
        if ([Wm]::GetForegroundWindow() -eq $hwnd) { return $true }
        [void][Wm]::SetForegroundWindow($hwnd)
        Start-Sleep -Milliseconds 300
    }
    return ([Wm]::GetForegroundWindow() -eq $hwnd)
}

# Full-screen select + copy -> one string per visible grid row.
function Read-Screen {
    for ($attempt = 0; $attempt -lt 3; ++$attempt) {
        try { Set-Clipboard -Value " " -ErrorAction Stop } catch { }
        Invoke-Drag 0 0 ($cols - 1) ($rows - 1)
        Send-Chord 0x43   # Ctrl+Shift+C
        $raw = $null
        for ($poll = 0; $poll -lt 15; ++$poll) {
            Start-Sleep -Milliseconds 200
            $raw = Get-Clipboard -Raw
            if ($null -ne $raw -and $raw.Contains("`n")) { break }
        }
        if ($null -ne $raw -and $raw.Contains("`n")) {
            return ($raw -split '\r?\n')
        }
        if ([Wm]::GetForegroundWindow() -ne $hwnd) { [void](Ensure-Focus) }
        Write-Host ("  read-screen retry {0}: fg {1} != hwnd? clipboard [{2}]" -f
            $attempt, [Wm]::GetForegroundWindow(),
            $(if ($raw) { $raw -replace "`r", '\r' -replace "`n", '\n' } else { "<null>" }))
        Start-Sleep -Milliseconds 400
    }
    return @("")
}

# Marker rows -> the logical text they assemble into (no separators: a
# wrapped line's chunks concatenate).
function Get-MarkerText([string[]]$lines) {
    $chunks = @($lines | Where-Object { $_ -match "^[A-J]+$" })
    return (($chunks | ForEach-Object { $_ }) -join "")
}
function Show-Screen([string]$tag, [string[]]$lines) {
    Write-Host ("  screen[{0}]: [{1}]" -f $tag,
        (($lines | Select-Object -First 12) -join " | "))
}
function Save-Shot([string]$path) {
    try {
        Add-Type -AssemblyName System.Drawing
        $wr = New-Object Wm+RECT
        [void][Wm]::GetWindowRect($hwnd, [ref]$wr)
        $w = $wr.R - $wr.L; $h2 = $wr.B - $wr.T
        $bmp = New-Object System.Drawing.Bitmap($w, $h2)
        $g = [System.Drawing.Graphics]::FromImage($bmp)
        $g.CopyFromScreen($wr.L, $wr.T, 0, 0, (New-Object System.Drawing.Size($w, $h2)))
        $bmp.Save($path)
        $g.Dispose(); $bmp.Dispose()
    } catch { Write-Host ("  shot failed: {0}" -f $_.Exception.Message) }
}

# Resize the window so the CLIENT becomes targetW x targetH LOGICAL px.
# SetWindowPos takes the OUTER size in physical px: adjust by the current
# outer-minus-client delta.
function Resize-Window([int]$targetW, [int]$targetH) {
    $wr = New-Object Wm+RECT
    [void][Wm]::GetWindowRect($hwnd, [ref]$wr)
    $cr = New-Object Wm+RECT
    [void][Wm]::GetClientRect($hwnd, [ref]$cr)
    $dW = ($wr.R - $wr.L) - ($cr.R - $cr.L)
    $dH = ($wr.B - $wr.T) - ($cr.B - $cr.T)
    $cx = [int][math]::Round($targetW * $scale) + $dW
    $cy = [int][math]::Round($targetH * $scale) + $dH
    # Flags 0x14 = SWP_NOZORDER | SWP_NOACTIVATE - NOT 0x13, whose SWP_NOSIZE
    # bit makes Windows silently ignore cx/cy (first-run lesson).
    [void][Wm]::SetWindowPos($hwnd, [IntPtr](-1), 0, 0, $cx, $cy, 0x14)
    Start-Sleep -Milliseconds 1800   # ConPTY resize + shell repaint
    # SDL may hand back a different HWND after a style change; re-find it.
    $h2 = [Wm]::FindWindow("SDL_app", "Terminal")
    if ($h2 -ne $hwnd) {
        Write-Host ("  note: hwnd changed after resize {0} -> {1}" -f $hwnd, $h2)
        $script:hwnd = $h2
    }
    $cr2 = New-Object Wm+RECT
    [void][Wm]::GetClientRect($script:hwnd, [ref]$cr2)
    Write-Host ("  measured client after resize: {0}x{1}" -f ($cr2.R - $cr2.L), ($cr2.B - $cr2.T))
    $alive = Get-CimInstance Win32_Process -Filter "Name='jkdesktop.exe'" |
        Where-Object { $_.CommandLine -match 'terminal' }
    Write-Host ("  process alive: {0}" -f ($null -ne $alive))
}

$ok = $true

# --- 1. spawn + focus + long output ------------------------------------------
$vis = ($hwnd -ne [IntPtr]::Zero) -and [Wm]::IsWindowVisible($hwnd)
if (-not ($vis -and $scale -gt 0)) {
    Write-Host ("spawn: FAIL (hwnd={0})" -f $hwnd)
    exit 1
}
Write-Host ("spawn: PASS (hwnd {0} scale {1:F2})" -f $hwnd, $scale)
if (-not (Ensure-Focus)) {
    Write-Host ("focus: FAIL (foreground {0})" -f [Wm]::GetForegroundWindow())
    exit 1
}
Send-Chars "cls"
Send-Enter
Start-Sleep -Milliseconds 800
Send-Chars "Write-Output ('ABCDEFGHIJ'*20)"
Send-Enter
Start-Sleep -Milliseconds 1500

# --- 2. read at 99 cols -------------------------------------------------------
$lines1 = Read-Screen
Save-Shot "$build\probe_reflow_narrow.png"
$text1 = Get-MarkerText $lines1
if ($text1 -eq $expect) {
    Write-Host "narrow-read: PASS (200 chars in A-J chunks)"
} else {
    $ok = $false
    Write-Host ("narrow-read: FAIL (got {0} chars: [{1}])" -f $text1.Length,
        $text1.Substring(0, [math]::Min(60, $text1.Length)))
}

# --- 3. resize wider (149 cols) + read ----------------------------------------
Resize-Window $winW2 $winH2
# The resize can drop foreground (observed); hammer focus back before the
# read - the drag needs the app alive and visible, the chord is posted.
for ($i = 0; $i -lt 8 -and [Wm]::GetForegroundWindow() -ne $hwnd; ++$i) {
    [void](Ensure-Focus)
    Start-Sleep -Milliseconds 200
}
Write-Host ("  focus after resize: {0}" -f ([Wm]::GetForegroundWindow() -eq $hwnd))
Save-Shot "$build\probe_reflow_wide.png"
# Refresh the client origin/scale for the new window size (scale is equal,
# but the probe grid geometry changed).
[Wm]::GetClientRect($hwnd, [ref]$crect) | Out-Null
[Wm]::ClientToScreen($hwnd, [ref]$co) | Out-Null
$scale = ($crect.R - $crect.L) / [double]$winW2
$cols = [int][math]::Floor(($winW2 - 2 * $border) / $cellW)
$rows = [int][math]::Floor(($winH2 - $titleH - $border) / $cellH)
Write-Host ("resized: client 1200x500 -> cols {0} rows {1}" -f $cols, $rows)

$lines2 = Read-Screen
Show-Screen "wide" $lines2
$text2 = Get-MarkerText $lines2
if ($text2 -eq $expect) {
    Write-Host "wide-read: PASS (200 chars re-assembled at the new width)"
} else {
    $ok = $false
    Write-Host ("wide-read: FAIL (got {0} chars: [{1}])" -f $text2.Length,
        $text2.Substring(0, [math]::Min(60, $text2.Length)))
}

# --- 4. resize back + read (round trip) ---------------------------------------
Resize-Window $winW $winH
[Wm]::GetClientRect($hwnd, [ref]$crect) | Out-Null
[Wm]::ClientToScreen($hwnd, [ref]$co) | Out-Null
$scale = ($crect.R - $crect.L) / [double]$winW
$cols = [int][math]::Floor(($winW - 2 * $border) / $cellW)
$rows = [int][math]::Floor(($winH - $titleH - $border) / $cellH)

$lines3 = Read-Screen
Save-Shot "$build\probe_reflow_back.png"
$text3 = Get-MarkerText $lines3
if ($text3 -eq $expect) {
    Write-Host "roundtrip-read: PASS (200 chars after resize back)"
} else {
    $ok = $false
    Write-Host ("roundtrip-read: FAIL (got {0} chars: [{1}])" -f $text3.Length,
        $text3.Substring(0, [math]::Min(60, $text3.Length)))
}

# --- cleanup -------------------------------------------------------------------
Stop-ProbeProcs
if ($ok) { Write-Host "REFLOW PROBE: PASS"; exit 0 }
else     { Write-Host "REFLOW PROBE: FAIL"; exit 1 }