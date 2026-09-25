# o2_double_compose.ps1 - docs/65 O2 step 1 decision gate (probe-only spike).
#
# Question: in CLIENT mode (jkwinserver.exe + jkdesktop.exe --client terminal),
# when the OS IME is in Korean conversion mode, does OS IME composition co-fire
# with the engine's internal hangul automata ("double composition")?
# docs/61 s16/s18 assumed the risk is real; the server window is DetachIme'd
# (JKWindowServer.cpp 329-335: ImmAssociateContext(hwnd, nullptr)), which may
# make it structurally impossible.
#
# Instruments (all measured live 2026-09-25):
#   1. REAL-STREAM typing (SendInput, scan-coded) against the foreground
#      server window. The INPUT struct MUST be MOUSEINPUT-union sized (40
#      bytes on x64): the vpt13 lesson "SendInput is blocked on this rig" was
#      a struct-size bug (cbSize 16 -> ERROR_INVALID_PARAMETER 87).
#   2. OS IME forcing levers, all measured:
#      - WM_IME_CONTROL to ImmGetDefaultIMEWnd(serverHwnd): every subcommand
#        read returns 0 before AND after real taps -> route dead (no context
#        on the server window to command).
#      - Synthetic VK_HANGUL (SendInput, correct scan 0x72): the server LL
#        hook FIRES (client ImeToggle), but the OS IME does NOT move - the
#        observer window's own context stays put even when IT is foreground
#        => the OS IME ignores injected toggle keys. Synthetic forcing of the
#        server thread's OS IME mode is NOT possible on this machine.
#      - Probe-owned observer window with a live IMM context: readable and
#        settable (ImmSetOpenStatus + ImmSetConversionStatus) -> used as the
#        POSITIVE CONTROL instead.
#   3. POSITIVE CONTROL: foreground the probe's own window, force the OS IME
#      Korean on ITS context (open + NATIVE), type "gk" via the real stream,
#      read ImmGetCompositionStringW(GCS_COMPSTR). Non-empty composition
#      ("ha" = UTF-16 D5 58, measured) proves the OS IME composition channel
#      works on this machine WHEN a context exists - the contrast case for
#      the detached server window.
#   4. Composition evidence at the client: the terminal [preedit] log prints
#      EVERY KeyDown/Char/TextEditing/ImeToggle with raw UTF-8 text bytes.
#      OS IME composition at the engine = type=9 (TextEditing) events or
#      type=8 (Char) events with non-ASCII bytes. With an attached IME,
#      letters arrive as VK_PROCESSKEY and NO letter keydowns are seen
#      (docs/61 s16) - so letter keydowns arriving raw IS the evidence the
#      IME is out of the loop. NOTE: the redirected stderr file LAGS several
#      seconds (console-child aliasing) - verdict reads wait for stability.
#
# Cells, x2 runs:
#   A: internal automata OFF (no toggles -> ASCII), type "gks" real stream.
#   B: internal hangul ON (JKTERM_FORCE_HANGUL=1), type "gks" (2-beolsik =
#      HAN). IME commit chars / TextEditing here is the literal
#      double-composition signature.
#   Both run with the OS IME in the machine's current state; synthetic forcing
#   is measured separately (see 2). The control (3) carries the Korean-mode
#   premise: with a context present the OS IME composes, and the server window
#   has NO context (code + behavior), so no mode can compose there.
#
# Verdict: DOUBLE-COMPOSITION REAL | NO DOUBLE-COMPOSITION (defense holds) |
# INCONCLUSIVE. PowerShell 5.1, ASCII-only. Exit 0 decisive, 2 inconclusive,
# 1 rig failure.
$ErrorActionPreference = "Continue"
$build = "I:\progwork\JKENGINE\engine\build"
$tmp   = "I:\progwork\JKENGINE\tmp"
$runs  = 2
if (-not (Test-Path $tmp)) { New-Item -ItemType Directory -Force -Path $tmp | Out-Null }

$script:passN = 0
$script:failN = 0
function Check([string]$name, [bool]$cond) {
    if ($cond) { $script:passN++; Write-Host ("CHECK {0}: PASS" -f $name) }
    else       { $script:failN++; Write-Host ("CHECK {0}: FAIL" -f $name) }
    return $cond
}

Add-Type -AssemblyName System.Windows.Forms
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class Wm {
    [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr ctx);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern IntPtr FindWindow(string cls, string title);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int cmd);
    [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint flags);
    [DllImport("user32.dll")] public static extern uint MapVirtualKey(uint code, uint type);
    [DllImport("user32.dll", SetLastError = true)]
    public static extern uint SendInput(uint n, INPUT[] p, int size);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint msg, IntPtr w, IntPtr l);
    [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint msg, IntPtr w, IntPtr l);
    [DllImport("user32.dll")] public static extern IntPtr GetKeyboardLayout(uint idThread);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    public static extern IntPtr CreateWindowExW(uint ex, string cls, string title, uint style, int x, int y, int w, int h, IntPtr parent, IntPtr menu, IntPtr inst, IntPtr param);
    [DllImport("user32.dll")] public static extern bool DestroyWindow(IntPtr h);
    [DllImport("imm32.dll")] public static extern IntPtr ImmGetDefaultIMEWnd(IntPtr h);
    [DllImport("imm32.dll")] public static extern IntPtr ImmGetContext(IntPtr h);
    [DllImport("imm32.dll")] public static extern bool ImmReleaseContext(IntPtr h, IntPtr c);
    [DllImport("imm32.dll")] public static extern bool ImmGetOpenStatus(IntPtr c);
    [DllImport("imm32.dll")] public static extern bool ImmGetConversionStatus(IntPtr c, out uint conv, out uint sent);
    [DllImport("imm32.dll")] public static extern bool ImmSetConversionStatus(IntPtr c, uint conv, uint sent);
    [DllImport("imm32.dll")] public static extern bool ImmSetOpenStatus(IntPtr c, bool open);
    [DllImport("imm32.dll", CharSet = CharSet.Unicode)]
    public static extern int ImmGetCompositionStringW(IntPtr c, int idx, byte[] buf, int len);
    public struct RECT { public int L, T, R, B; }
    // Real INPUT layout: the union must be MOUSEINPUT-sized (32 bytes) or
    // SendInput rejects cbSize with ERROR_INVALID_PARAMETER (87).
    [StructLayout(LayoutKind.Sequential)]
    public struct MOUSEINPUT { public int dx; public int dy; public uint mouseData;
        public uint dwFlags; public uint time; public IntPtr dwExtraInfo; }
    [StructLayout(LayoutKind.Sequential)]
    public struct KBDINPUT { public ushort wVk; public ushort wScan;
        public uint dwFlags; public uint time; public IntPtr dwExtraInfo; }
    [StructLayout(LayoutKind.Explicit)]
    public struct INPUTU { [FieldOffset(0)] public MOUSEINPUT mi; [FieldOffset(0)] public KBDINPUT ki; }
    [StructLayout(LayoutKind.Sequential)]
    public struct INPUT { public uint type; public INPUTU u; }
    public static readonly int Size = Marshal.SizeOf(typeof(INPUT));
    static INPUT Key(ushort vk, ushort scan, uint flags) {
        INPUT i = new INPUT(); i.type = 1;
        i.u.ki.wVk = vk; i.u.ki.wScan = scan; i.u.ki.dwFlags = flags;
        return i;
    }
    // Scan code MANDATORY on synthetic VK input (SDL drops wScan=0 keydowns).
    public static bool SendVkTap(ushort vk, ushort scan) {
        INPUT[] seq = new INPUT[2];
        seq[0] = Key(vk, scan, 0);
        seq[1] = Key(vk, scan, 2);
        return SendInput(2, seq, Size) == 2;
    }
    public static void PostVk(IntPtr hwnd, ushort vk, ushort scan) {
        long dn = 1L | ((long)scan << 16);
        long up = dn | 0xC0000000L;
        PostMessage(hwnd, 0x100, (IntPtr)vk, (IntPtr)dn);
        PostMessage(hwnd, 0x101, (IntPtr)vk, (IntPtr)up);
    }
    // GCS_COMPSTR = 0x0008: the in-progress composition as UTF-16 bytes.
    public static string CompString(IntPtr ctx) {
        int len = ImmGetCompositionStringW(ctx, 0x0008, null, 0);
        if (len <= 0) return "";
        byte[] buf = new byte[len + 4];
        int got = ImmGetCompositionStringW(ctx, 0x0008, buf, buf.Length);
        if (got <= 0) return "";
        return BitConverter.ToString(buf, 0, Math.Min(got, len)).Replace("-", " ");
    }
}
"@
[Wm]::SetProcessDpiAwarenessContext([IntPtr](-4)) | Out-Null
$latin1 = [System.Text.Encoding]::GetEncoding(28591)

$WM_IME_CONTROL = 0x283
$IME_CMODE_NATIVE = 0x0001
$VK_HANGUL = 0x15
$HANGUL_SCAN = 0x72

function Stop-Stack {
    Get-Process jkdesktop, jkwinserver -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 2
}

function Find-ServerHwnd {
    # STARTUPINFO SW_HIDE (probe spawn) hides the SDL window: visibility is
    # NOT a find condition - ShowWindow(SW_SHOWNOACTIVATE) after finding.
    for ($i = 0; $i -lt 40; ++$i) {
        $h = [Wm]::FindWindow("SDL_app", "JKENGINE Window Server")
        if ($h -ne [IntPtr]::Zero) {
            [void][Wm]::ShowWindow($h, 8)
            Start-Sleep -Milliseconds 200
            return $h
        }
        Start-Sleep -Milliseconds 250
    }
    return [IntPtr]::Zero
}

function Set-Foreground([IntPtr]$hwnd) {
    # Foreground rights: retry, then the classic ALT-poke workaround.
    for ($i = 0; $i -lt 5; ++$i) {
        if ([Wm]::GetForegroundWindow() -eq $hwnd) { return $true }
        [void][Wm]::ShowWindow($hwnd, 8)
        if ($i -eq 3) {
            [void][Wm]::SendVkTap([uint16]0x12, [uint16]0x38)   # ALT down+up
            Start-Sleep -Milliseconds 120
        }
        [void][Wm]::SetForegroundWindow($hwnd)
        Start-Sleep -Milliseconds 350
    }
    return ([Wm]::GetForegroundWindow() -eq $hwnd)
}

function Save-Shot([IntPtr]$hwnd, [string]$path) {
    Add-Type -AssemblyName System.Drawing
    $wr = New-Object Wm+RECT
    [void][Wm]::GetWindowRect($hwnd, [ref]$wr)
    $w = $wr.R - $wr.L; $h = $wr.B - $wr.T
    $bmp = New-Object System.Drawing.Bitmap($w, $h)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($wr.L, $wr.T, 0, 0, (New-Object System.Drawing.Size($w, $h)))
    $bmp.Save($path)
    $g.Dispose(); $bmp.Dispose()
}

function Get-DiffPix([string]$a, [string]$b) {
    if (-not (Test-Path $a) -or -not (Test-Path $b)) { return -1 }
    Add-Type -AssemblyName System.Drawing
    $ba = New-Object System.Drawing.Bitmap($a)
    $bb = New-Object System.Drawing.Bitmap($b)
    $w = [Math]::Min($ba.Width, $bb.Width); $h = [Math]::Min($ba.Height, $bb.Height)
    $diffs = 0
    for ($y = 0; $y -lt $h; $y += 2) {
        for ($x = 0; $x -lt $w; $x += 2) {
            $pa = $ba.GetPixel($x, $y); $pb = $bb.GetPixel($x, $y)
            if ($pa.R -ne $pb.R -or $pa.G -ne $pb.G -or $pa.B -ne $pb.B) { $diffs++ }
        }
    }
    $ba.Dispose(); $bb.Dispose()
    return $diffs
}

# Parse the client [preedit] log: ev lines carry the raw UTF-8 text bytes.
# The file is read as Latin-1 so byte values survive unchanged (hex dump).
# The Start-Process redirect handle may hold the file open while the client
# lives: open with FileShare.ReadWrite and retry, else give up gracefully.
function Read-RawBytes([string]$path) {
    if (-not (Test-Path $path)) { return $null }
    for ($i = 0; $i -lt 8; ++$i) {
        try {
            $fs = [System.IO.File]::Open($path, [System.IO.FileMode]::Open,
                [System.IO.FileAccess]::Read, [System.IO.FileShare]::ReadWrite)
            $bytes = New-Object byte[] $fs.Length
            [void]$fs.Read($bytes, 0, $fs.Length)
            $fs.Close()
            return $bytes
        } catch {
            Start-Sleep -Milliseconds 400
        }
    }
    return $null
}

function Read-ClientLog([string]$path) {
    $r = @{ keydowns = 0; chars = 0; charTexts = @(); edits = 0; toggles = 0;
            lang1 = 0; keys = @(); results = @(); resultPre = 0; resultSend = 0 }
    $bytes = Read-RawBytes $path
    if ($null -eq $bytes) { Write-Host ("[warn] client log unreadable (locked): " + $path); return $r }
    $txt = $latin1.GetString($bytes)
    foreach ($m in [regex]::Matches($txt, '\[preedit\] ev type=(\d+) key=0x([0-9A-Fa-f]+) text="([^"]*)"')) {
        $t = [int]$m.Groups[1].Value
        $k = [Convert]::ToUInt32($m.Groups[2].Value, 16)
        $tx = $m.Groups[3].Value
        if ($t -eq 6) { $r.keydowns++; $r.keys += ("0x{0:X}" -f $k) }
        elseif ($t -eq 8) {
            $r.chars++
            $hex = ($tx.ToCharArray() | ForEach-Object { "{0:X2}" -f [int]$_ }) -join ' '
            $r.charTexts += ,@($hex, ($tx.Length))
        }
        elseif ($t -eq 9) { $r.edits++ }
        elseif ($t -eq 11) { $r.toggles++ }
        if ($t -eq 6 -and ($k -band 0x40000000) -ne 0) { $r.lang1++ }
    }
    foreach ($m in [regex]::Matches($txt, '\[preedit\] result preEdit="([^"]*)" send=(\d+)')) {
        $pre = $m.Groups[1].Value; $send = [int]$m.Groups[2].Value
        $r.results += ,@((($pre.ToCharArray() | ForEach-Object { "{0:X2}" -f [int]$_ }) -join ' '), $send)
        if ($pre.Length -gt 0) { $r.resultPre++ }
        if ($send -gt 0) { $r.resultSend++ }
    }
    return $r
}

# The redirected stderr file lags seconds: poll until the log stops growing.
# A live holder can lock the file (share-none) - tolerate and keep polling.
function Wait-LogStable([string]$path, [int]$timeoutMs) {
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    $last = -1
    while ($sw.ElapsedMilliseconds -lt $timeoutMs) {
        Start-Sleep -Milliseconds 800
        $now = -1
        try { if (Test-Path $path) { $now = (Get-Item $path).Length } } catch { $now = -1 }
        if ($now -gt 0 -and $now -eq $last) { return $now }
        $last = $now
    }
    return $last
}

function Send-Letters([IntPtr]$hwnd, [int[]]$vks) {
    $all = $true
    foreach ($vk in $vks) {
        $scan = [Wm]::MapVirtualKey([uint32]$vk, 0)
        $ok = [Wm]::SendVkTap([uint16]$vk, [uint16]$scan)
        Write-Host ("sendinput vk=0x{0:X2} scan=0x{1:X2} -> {2}" -f $vk, $scan, $ok)
        $all = ($all -and $ok)
        Start-Sleep -Milliseconds 180
    }
    return $all
}

function Read-ImeSubs([IntPtr]$defIme) {
    $vals = @{}
    if ($defIme -eq [IntPtr]::Zero) { return $vals }
    foreach ($sub in 1..6) {
        $vals[$sub] = [int][Wm]::SendMessage($defIme, $WM_IME_CONTROL, [IntPtr]$sub, [IntPtr]::Zero)
    }
    return $vals
}

function Print-ImeSubs([string]$tag, $vals) {
    $line = ($vals.Keys | Sort-Object | ForEach-Object { "sub{0}={1}" -f $_, $vals[$_] }) -join ' '
    Write-Host ("[ime] {0}: {1}" -f $tag, $line)
}

# The probe's own observer window: a live IMM context we can read and set.
$script:obsHwnd = [IntPtr]::Zero
function Open-Observer {
    $h = [Wm]::CreateWindowExW(0, "STATIC", "o2ime", 2415919104, 2, 2, 30, 14, [IntPtr]::Zero, [IntPtr]::Zero, [IntPtr]::Zero, [IntPtr]::Zero)
    if ($h -eq [IntPtr]::Zero) { return $null }
    [void][Wm]::ShowWindow($h, 5)
    Start-Sleep -Milliseconds 150
    $script:obsHwnd = $h
    return (Read-Observer)
}

function Read-Observer {
    if ($script:obsHwnd -eq [IntPtr]::Zero) { return $null }
    $c = [Wm]::ImmGetContext($script:obsHwnd)
    if ($c -eq [IntPtr]::Zero) { return $null }
    $cv = 0; $sn = 0
    $open = [Wm]::ImmGetOpenStatus($c)
    $okc = [Wm]::ImmGetConversionStatus($c, [ref]$cv, [ref]$sn)
    $comp = [Wm]::CompString($c)
    [void][Wm]::ImmReleaseContext($script:obsHwnd, $c)
    return @{ open = $open; conv = [int]$cv; comp = $comp }
}

function Force-ObserverKorean {
    if ($script:obsHwnd -eq [IntPtr]::Zero) { return $false }
    $c = [Wm]::ImmGetContext($script:obsHwnd)
    if ($c -eq [IntPtr]::Zero) { return $false }
    [void][Wm]::ImmSetOpenStatus($c, $true)
    [void][Wm]::ImmSetConversionStatus($c, [uint32]$IME_CMODE_NATIVE, [uint32]0)
    Start-Sleep -Milliseconds 200
    $n = Read-Observer
    [void][Wm]::ImmReleaseContext($script:obsHwnd, $c)
    return ($n.open -and (($n.conv -band $IME_CMODE_NATIVE) -ne 0))
}

function Click-Into-Client([IntPtr]$hwnd) {
    $wr = New-Object Wm+RECT
    [void][Wm]::GetWindowRect($hwnd, [ref]$wr)
    $cw = $wr.R - $wr.L; $ch = $wr.B - $wr.T
    $sx = [int]($cw * 0.25); $sy = [int]($ch * 0.3)
    $lp = [IntPtr]((([int64]$sy) -shl 16) -bor ([int64]$sx -band 0xFFFF))
    [void][Wm]::PostMessage($hwnd, 0x200, [IntPtr]1, $lp)
    [void][Wm]::PostMessage($hwnd, 0x202, [IntPtr]1, $lp)
    Start-Sleep -Milliseconds 500
}

function NonAsciiChars($cl) {
    $out = @()
    foreach ($c in $cl.charTexts) {
        $hex = $c[0]; $nonAscii = $false
        foreach ($tok in ($hex -split ' ')) { if ([Convert]::ToInt32($tok, 16) -gt 0x7F) { $nonAscii = $true } }
        if ($nonAscii) { $out += $hex }
    }
    return $out
}

# ---------------------------------------------------------------------------
$verdicts = @()
$obsBaseConv = 0
for ($n = 1; $n -le $runs; ++$n) {
    $tag = "r$n"
    Write-Host ""
    Write-Host "================ RUN $n ================"
    $serverLog = "$tmp\o2_${tag}_server.log"
    $clientA   = "$tmp\o2_${tag}_clientA.log"
    $clientB   = "$tmp\o2_${tag}_clientB.log"
    $shotA0    = "$tmp\o2_${tag}_a0_baseline.png"
    $shotA1    = "$tmp\o2_${tag}_a1_typed.png"
    $shotB0    = "$tmp\o2_${tag}_b0_baseline.png"
    $shotB1    = "$tmp\o2_${tag}_b1_typed.png"

    # --- 1. stack up -------------------------------------------------------
    Get-Process jkdesktop, jkwinserver -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 2
    $env:JKTERM_PREEDIT_DBG = "1"
    [Environment]::SetEnvironmentVariable("JKTERM_FORCE_HANGUL", $null)
    $srv = Start-Process -FilePath "$build\jkwinserver.exe" -WorkingDirectory $build `
        -WindowStyle Hidden -PassThru -RedirectStandardError $serverLog
    Start-Sleep -Seconds 3
    $cliA = Start-Process -FilePath "$build\jkdesktop.exe" -ArgumentList "--client","terminal" `
        -WorkingDirectory $build -WindowStyle Hidden -PassThru -RedirectStandardError $clientA
    Start-Sleep -Seconds 6
    Check ("{0} server alive" -f $tag) (-not $srv.HasExited)
    Check ("{0} clientA alive" -f $tag) (-not $cliA.HasExited)

    $hwnd = Find-ServerHwnd
    if (-not (Check ("{0} server window" -f $tag) ($hwnd -ne [IntPtr]::Zero))) { break }
    if (-not (Check ("{0} foreground" -f $tag) (Set-Foreground $hwnd))) { break }
    Click-Into-Client $hwnd

    # --- 2. IME instruments -------------------------------------------------
    $hklSrv = [Wm]::GetKeyboardLayout(0)
    Write-Host ("[ime] probe thread hkl 0x{0:X8}" -f [int64]$hklSrv)
    $defIme = [Wm]::ImmGetDefaultIMEWnd($hwnd)
    Check ("{0} default IME window" -f $tag) ($defIme -ne [IntPtr]::Zero)
    $baseSubs = Read-ImeSubs $defIme
    Print-ImeSubs ("{0} baseline (server default IME wnd)" -f $tag) $baseSubs

    $obs = Open-Observer
    Check ("{0} observer IME context" -f $tag) ($obs -ne $null -and $obs.open -ne $null)
    if ($obs) {
        Write-Host ("[ime] observer baseline: open {0} conv 0x{1:X}" -f $obs.open, $obs.conv)
        $obsBaseConv = $obs.conv
    }

    # POSITIVE CONTROL: observer foreground, OS IME forced Korean on its own
    # context, type "gk" real-stream, read the in-progress composition.
    $ctlComp = ""
    if ($obs -and $script:obsHwnd -ne [IntPtr]::Zero) {
        if (Set-Foreground $script:obsHwnd) {
            $setOk = Force-ObserverKorean
            $o2 = Read-Observer
            Write-Host ("[control] observer forced korean: {0} (open {1} conv 0x{2:X})" -f $setOk, $o2.open, $o2.conv)
            if ($setOk) {
                foreach ($vk in @(0x47, 0x4B)) {
                    [void][Wm]::SendVkTap([uint16]$vk, [uint16]([Wm]::MapVirtualKey([uint32]$vk, 0)))
                    Start-Sleep -Milliseconds 200
                    [System.Windows.Forms.Application]::DoEvents()
                }
                Start-Sleep -Milliseconds 500
                [System.Windows.Forms.Application]::DoEvents()
                $o3 = Read-Observer
                $ctlComp = $o3.comp
                Write-Host ("[control] composing string after gk: '{0}' (UTF-16 bytes; open {1} conv 0x{2:X})" -f $ctlComp, $o3.open, $o3.conv)
                Check ("{0} positive control: OS IME composes on a context window" -f $tag) ($ctlComp.Length -gt 0)
                [void][Wm]::SendVkTap([uint16]0x1B, [uint16]([Wm]::MapVirtualKey([uint32]0x1B, 0)))
                Start-Sleep -Milliseconds 300
            }
        } else {
            Write-Host "[control] could not foreground the observer window"
        }
    }
    # Back to the server window for the measurement cells.
    [void](Set-Foreground $hwnd)

    # --- 3. CELL A: internal ASCII -> type gks (real stream) ----------------
    Save-Shot $hwnd $shotA0
    $deliveredA = Send-Letters $hwnd @(0x47, 0x4B, 0x53)   # g k s
    Start-Sleep -Milliseconds 800
    Save-Shot $hwnd $shotA1
    Write-Host ("[cell A] shot diff pixels vs baseline: {0}" -f (Get-DiffPix $shotA0 $shotA1))
    [Wm]::SendVkTap([uint16]0x0D, [uint16]([Wm]::MapVirtualKey([uint32]0x0D, 0)))
    Start-Sleep -Milliseconds 500

    # --- 4. CELL B: internal hangul (FORCE_HANGUL) -> type gks --------------
    $cliA | Stop-Process -Force -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 2
    $env:JKTERM_FORCE_HANGUL = "1"
    $cliB = Start-Process -FilePath "$build\jkdesktop.exe" -ArgumentList "--client","terminal" `
        -WorkingDirectory $build -WindowStyle Hidden -PassThru -RedirectStandardError $clientB
    Start-Sleep -Seconds 6
    Check ("{0} clientB alive" -f $tag) (-not $cliB.HasExited)
    [void](Set-Foreground $hwnd)
    Click-Into-Client $hwnd
    Save-Shot $hwnd $shotB0
    $deliveredB = Send-Letters $hwnd @(0x47, 0x4B, 0x53)
    Start-Sleep -Milliseconds 800
    Save-Shot $hwnd $shotB1
    Write-Host ("[cell B] shot diff pixels vs baseline: {0}" -f (Get-DiffPix $shotB0 $shotB1))
    [Wm]::SendVkTap([uint16]0x0D, [uint16]([Wm]::MapVirtualKey([uint32]0x0D, 0)))
    Start-Sleep -Milliseconds 500

    # --- 5. VK_HANGUL instrument measurement (after the cells) --------------
    # The hook must fire AND the raw VK_HANGUL keydown must reach SDL (under
    # an attached IME this key never reaches the app - docs/61 s16). Also
    # shows the OS IME ignoring the injected toggle key (observer unmoved).
    $obsBeforeTap = Read-Observer
    $tapOk = [Wm]::SendVkTap([uint16]$VK_HANGUL, [uint16]$HANGUL_SCAN)
    Start-Sleep -Seconds 5
    $obsAfterTap = Read-Observer
    Write-Host ("[instrument] VK_HANGUL tap: sent {0}; observer conv before 0x{1:X} after 0x{2:X} (OS IME ignores injected toggle key)" -f $tapOk, [int64]$obsBeforeTap.conv, [int64]$obsAfterTap.conv)
    Check ("{0} observer immune to synthetic VK_HANGUL (no OS forcing lever)" -f $tag) ($obsBeforeTap.conv -eq $obsAfterTap.conv)

    # --- 6. evidence dump + verdict ----------------------------------------
    # Kill the clients FIRST: a live redirect holder can lock its log file
    # (measured: the r1/r2 clientB reads failed with IOException while the
    # process lived; after death the full [preedit] stream is on disk).
    $cliB | Stop-Process -Force -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 2
    [void](Wait-LogStable $clientA 9000)
    [void](Wait-LogStable $clientB 9000)
    $clA = Read-ClientLog $clientA
    $clB = Read-ClientLog $clientB
    Write-Host ("--- cellA events: keydowns {0} chars {1} edits {2} toggles {3} lang1 {4}" -f $clA.keydowns, $clA.chars, $clA.edits, $clA.toggles, $clA.lang1)
    Write-Host ("  [cell A] keydown keys: {0}" -f ($clA.keys -join ' '))
    foreach ($c in $clA.charTexts) { Write-Host ("  [cell A] char text bytes: {0}" -f $c[0]) }
    foreach ($r in $clA.results)  { Write-Host ("  [cell A] result preEdit bytes: {0} send {1}" -f $r[0], $r[1]) }
    Write-Host ("--- cellB events: keydowns {0} chars {1} edits {2} toggles {3} lang1 {4}" -f $clB.keydowns, $clB.chars, $clB.edits, $clB.toggles, $clB.lang1)
    Write-Host ("  [cell B] keydown keys: {0}" -f ($clB.keys -join ' '))
    foreach ($c in $clB.charTexts) { Write-Host ("  [cell B] char text bytes: {0}" -f $c[0]) }
    foreach ($r in $clB.results)  { Write-Host ("  [cell B] result preEdit bytes: {0} send {1}" -f $r[0], $r[1]) }
    Write-Host "--- server log [ime] lines ---"
    Get-Content $serverLog -ErrorAction SilentlyContinue | Where-Object { $_ -match '\[ime\]' } | Select-Object -First 20
    $endObs = Read-Observer
    if ($endObs) { Write-Host ("[ime] observer end state: open {0} conv 0x{1:X}" -f $endObs.open, $endObs.conv) }

    $nonAsciiA = NonAsciiChars $clA
    $nonAsciiB = NonAsciiChars $clB
    $letterKeysA = @($clA.keys | Where-Object { $_ -in @('0x67','0x6B','0x73') })
    $letterKeysB = @($clB.keys | Where-Object { $_ -in @('0x67','0x6B','0x73') })
    $delivered = ($letterKeysA.Count -ge 3) -and ($letterKeysB.Count -ge 3)
    $internalRan = ($clB.resultPre -gt 0)
    $composed = (($nonAsciiA.Count + $nonAsciiB.Count) -gt 0) -or (($clA.edits + $clB.edits) -gt 0)
    $ctlOk = ($ctlComp.Length -gt 0)

    Write-Host ("[verdict {0}] control {1} delivered {2} internalRan {3} composed {4}" -f $tag, $ctlOk, $delivered, $internalRan, $composed)
    Write-Host ("[verdict {0}] nonAsciiA: {1} | nonAsciiB: {2} | ctlComp: {3}" -f $tag, ($nonAsciiA -join ';'), ($nonAsciiB -join ';'), $ctlComp)

    if ($composed) { $verdicts += "DOUBLE-COMPOSITION REAL" }
    elseif ($ctlOk -and $delivered -and $internalRan) { $verdicts += "NO DOUBLE-COMPOSITION" }
    else { $verdicts += "INCONCLUSIVE" }

    # Per-run OS IME restore: the control set the observer Korean - hand the
    # user's baseline back before the next run measures its own baseline.
    $obsEnd = Read-Observer
    if ($obsEnd -and ($obsEnd.open -or (($obsEnd.conv -band $IME_CMODE_NATIVE) -ne 0))) {
        $c = [Wm]::ImmGetContext($script:obsHwnd)
        if ($c -ne [IntPtr]::Zero) {
            [void][Wm]::ImmSetOpenStatus($c, $false)
            [void][Wm]::ImmSetConversionStatus($c, [uint32]0, [uint32]0)
            [void][Wm]::ImmReleaseContext($script:obsHwnd, $c)
            Write-Host "[ime] observer OS IME restored per-run (open=False conv=0)"
        }
    }
    [void][Wm]::DestroyWindow($script:obsHwnd)
    $script:obsHwnd = [IntPtr]::Zero

    $cliB | Stop-Process -Force -ErrorAction SilentlyContinue
    $srv | Stop-Process -Force -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 2
}

# Restore the OS IME state the control may have changed (user's machine).
if ($script:obsHwnd -ne [IntPtr]::Zero) {
    $o = Read-Observer
    if ($o -and ($o.open -or (($o.conv -band $IME_CMODE_NATIVE) -ne 0))) {
        $c = [Wm]::ImmGetContext($script:obsHwnd)
        if ($c -ne [IntPtr]::Zero) {
            [void][Wm]::ImmSetOpenStatus($c, $false)
            [void][Wm]::ImmSetConversionStatus($c, [uint32]0, [uint32]0)
            [void][Wm]::ImmReleaseContext($script:obsHwnd, $c)
            Write-Host "[ime] observer OS IME state restored to open=False conv=0 (baseline)"
        }
    }
    [void][Wm]::DestroyWindow($script:obsHwnd)
}

# --- 7. restore the live stack ----------------------------------------------
Write-Host ""
Write-Host "=== restore live stack ==="
Get-Process jkdesktop, jkwinserver -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Seconds 2
Start-Process -FilePath "$build\jkwinserver.exe" -WorkingDirectory $build -WindowStyle Hidden
Start-Sleep -Seconds 5
Start-Process -FilePath "$build\jkdesktop.exe" -WorkingDirectory $build
$pingOk = $false
# PS 5.1 native arg passing strips bare embedded quotes ("tool" -> tool), so
# the server parsed a broken JSON and answered bad_request. Backslash-escaped
# quotes survive CommandLineToArgvW as literal quotes - the fix.
$pingArg = '{\"tool\":\"ping\",\"args\":{}}'
for ($i = 0; $i -lt 40; ++$i) {
    Start-Sleep -Milliseconds 500
    $ping = (& "$build\jkdesktop.exe" agentctl $pingArg 2>&1 | Out-String)
    if ($ping -match '"ok"\s*:\s*true') { $pingOk = $true; break }
}
Check "restore ping" $pingOk

Write-Host ""
Write-Host ("RESULT: verdicts = {0} | checks pass {1} fail {2}" -f ($verdicts -join ' / '), $script:passN, $script:failN)
if ($script:failN -eq 0 -and $verdicts.Count -eq $runs -and ($verdicts | Where-Object { $_ -eq "INCONCLUSIVE" }).Count -eq 0) { exit 0 }
if ($verdicts.Count -eq $runs -and ($verdicts | Where-Object { $_ -eq "INCONCLUSIVE" }).Count -gt 0) { exit 2 }
exit 1