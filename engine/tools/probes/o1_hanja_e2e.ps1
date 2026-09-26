# o1_hanja_e2e.ps1 - docs/66 Phase C live e2e gate (synthetic input, x2 runs).
#
# Question: does the full VK_HANJA channel work live? LL hook (vkCode 0x19)
#   -> server `[ime] hanja -> client` -> wire ImeHanja -> TerminalView
#   popup during composition -> digit commit sends the first hanja to pty.
#
# Flow per run:
#   1. stack up: jkwinserver + jkdesktop --client terminal with
#      JKTERM_FORCE_HANGUL=1 (internal 2-beolsik) + JKTERM_PREEDIT_DBG=1.
#   2. foreground server window, click into client.
#   3. REAL-STREAM letters g k s (SendInput, MOUSEINPUT-sized INPUT - the
#      docs/65 lesson 4) -> "한" composing (preEdit overlay).
#   4. SendVkTap VK_HANJA (0x19). Popup opens (candidate[0..] from the
#      system IME dictionary via HanjaDic COM).
#   5. Evidence: server log `[ime] hanja -> client`, popup screenshot diff,
#      then digit '1' commits -> client log result send=3 (UTF-8 韓).
#   6. restore live stack + ping.
#
# PowerShell 5.1, ASCII-only. Exit 0 decisive, 2 inconclusive, 1 rig failure.
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
    [DllImport("user32.dll")] public static extern uint MapVirtualKey(uint code, uint type);
    [DllImport("user32.dll", SetLastError = true)]
    public static extern uint SendInput(uint n, INPUT[] p, int size);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint msg, IntPtr w, IntPtr l);
    public struct RECT { public int L, T, R, B; }
    // MOUSEINPUT-sized union or SendInput rejects cbSize (docs/65 lesson 4).
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
}
"@
[Wm]::SetProcessDpiAwarenessContext([IntPtr](-4)) | Out-Null
$latin1 = [System.Text.Encoding]::GetEncoding(28591)

$VK_HANJA = 0x19
# 한자 키 스캔: MapVirtualKey 우선, 0이면 한국 표준 한자 키 0x71 폴백.
$hanjaScan = [Wm]::MapVirtualKey([uint32]$VK_HANJA, 0)
if ($hanjaScan -eq 0) { $hanjaScan = 0x71 }
Write-Host ("[setup] VK_HANJA scan = 0x{0:X2}" -f $hanjaScan)

function Stop-Stack {
    Get-Process jkdesktop, jkwinserver, jkagentd -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 2
}

function Find-ServerHwnd {
    # STARTUPINFO SW_HIDE hides the SDL window: ShowWindow(SW_SHOWNOACTIVATE) after finding.
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
    for ($i = 0; $i -lt 5; ++$i) {
        if ([Wm]::GetForegroundWindow() -eq $hwnd) { return $true }
        [void][Wm]::ShowWindow($hwnd, 8)
        if ($i -eq 3) {
            [void][Wm]::SendVkTap([uint16]0x12, [uint16]0x38)   # ALT poke
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

function Send-Letters([int[]]$vks) {
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

# ---------------------------------------------------------------------------
$verdicts = @()
for ($n = 1; $n -le $runs; ++$n) {
    $tag = "r$n"
    Write-Host ""
    Write-Host "================ RUN $n ================"
    $serverLog = "$tmp\o1h_${tag}_server.log"
    $clientLog = "$tmp\o1h_${tag}_client.log"
    $shot0     = "$tmp\o1h_${tag}_0_baseline.png"
    $shot1     = "$tmp\o1h_${tag}_1_typed.png"
    $shot2     = "$tmp\o1h_${tag}_2_popup.png"
    $shot3     = "$tmp\o1h_${tag}_3_committed.png"

    # --- 1. stack up -------------------------------------------------------
    Stop-Stack
    $env:JKTERM_PREEDIT_DBG = "1"
    $env:JKTERM_FORCE_HANGUL = "1"
    # [ime] lines go to server STDOUT (printf); stderr is quiet. Force-kill
    # loses buffered stdio only if unredirected - capture both.
    $srv = Start-Process -FilePath "$build\jkwinserver.exe" -WorkingDirectory $build `
        -WindowStyle Hidden -PassThru -RedirectStandardError "$serverLog.err" `
        -RedirectStandardOutput $serverLog
    Start-Sleep -Seconds 3
    $cli = Start-Process -FilePath "$build\jkdesktop.exe" -ArgumentList "--client","terminal" `
        -WorkingDirectory $build -WindowStyle Hidden -PassThru -RedirectStandardError $clientLog
    Start-Sleep -Seconds 6
    if (-not (Check ("{0} server alive" -f $tag) (-not $srv.HasExited))) { $verdicts += "RIG"; break }
    if (-not (Check ("{0} client alive" -f $tag) (-not $cli.HasExited))) { $verdicts += "RIG"; break }

    $hwnd = Find-ServerHwnd
    if (-not (Check ("{0} server window" -f $tag) ($hwnd -ne [IntPtr]::Zero))) { $verdicts += "RIG"; break }
    if (-not (Check ("{0} foreground" -f $tag) (Set-Foreground $hwnd))) { $verdicts += "RIG"; break }
    Click-Into-Client $hwnd

    # --- 2. type gks -> 한 composing ---------------------------------------
    Save-Shot $hwnd $shot0
    $delivered = Send-Letters @(0x47, 0x4B, 0x53)   # g k s = 한
    Check ("{0} letters delivered" -f $tag) $delivered
    Start-Sleep -Milliseconds 800
    Save-Shot $hwnd $shot1

    # --- 3. VK_HANJA tap -> popup -------------------------------------------
    $tapOk = [Wm]::SendVkTap([uint16]$VK_HANJA, [uint16]$hanjaScan)
    Start-Sleep -Seconds 2
    Save-Shot $hwnd $shot2
    Write-Host ("[step] VK_HANJA tap sent: {0}" -f $tapOk)

    $popupDiff = Get-DiffPix $shot1 $shot2
    Write-Host ("[evidence] popup diff vs typed: {0}" -f $popupDiff)

    # --- 4. digit '1' commit -------------------------------------------------
    $digOk = [Wm]::SendVkTap([uint16]0x31, [uint16]0x02)
    Start-Sleep -Seconds 2
    Save-Shot $hwnd $shot3
    $commitDiff = Get-DiffPix $shot2 $shot3
    Write-Host ("[evidence] commit diff vs popup: {0} (digit sent {1})" -f $commitDiff, $digOk)

    # --- 5. evidence dump ----------------------------------------------------
    # Kill the client FIRST: a live redirect holder locks the log file.
    $cli | Stop-Process -Force -ErrorAction SilentlyContinue
    $srv | Stop-Process -Force -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 2
    [void](Wait-LogStable $clientLog 9000)

    $cbytes = Read-RawBytes $clientLog
    $ctext = if ($null -ne $cbytes) { $latin1.GetString($cbytes) } else { "" }
    $sbytes = Read-RawBytes $serverLog
    $stext = if ($null -ne $sbytes) { $latin1.GetString($sbytes) } else { "" }

    # Letter keydowns delivered (type=6 key=0x67/0x6B/0x73 raw).
    $letterKeys = [regex]::Matches($ctext, '\[preedit\] ev type=6 key=0x(67|6B|73) ')
    Write-Host ("  [ev] letter keydowns: {0}" -f $letterKeys.Count)

    # Server-side hanja push (the B2 wiring evidence).
    $hanjaLines = [regex]::Matches($stext, '\[ime\] hanja -> client')
    Write-Host ("  [ime] server hanja lines: {0}" -f $hanjaLines.Count)

    # Composing overlay painted 한 (UTF-8 ED 95 9C).
    $hanPaint = [regex]::Matches($ctext, '\[preedit\] paint preEdit="[^\x22]*\u00ED\u0095\u009C')
    Write-Host ("  [paint] 한 overlay lines: {0}" -f $hanPaint.Count)

    # Hanja commit result: send=3 bytes (UTF-8 韓 = E9 9F 93). The hex bracket
    # distinguishes the hanja commit from the plain composition flush (한 =
    # ED 95 9C, same 3-byte length) - the false-positive lesson.
    $result3 = [regex]::Matches($ctext, '\[preedit\] result [^\r\n]*send=3 bytes \[E9 9F 93')
    Write-Host ("  [result] hanja bytes (EC 97 93) result lines: {0}" -f $result3.Count)

    # The ImeHanja wire event reaching the view (type=12, JKEvent enum).
    $type12 = [regex]::Matches($ctext, '\[preedit\] ev type=12 ')
    Write-Host ("  [ev] ImeHanja (type=12) at view: {0}" -f $type12.Count)
    foreach ($m in [regex]::Matches($ctext, '\[preedit\] result [^\r\n]*')) {
        Write-Host ("    result line: {0}" -f ($m.Value -replace '[\u0080-\u00FF]', '?'))
    }

    # --- 6. verdict ----------------------------------------------------------
    $hanByteIdx = $ctext.IndexOf([char]0xED)
    Write-Host ("[verdict {0}] delivered {1} hanjaLines {2} hanPaint {3} popupDiff {4} result3 {5} commitDiff {6}" -f `
        $tag, $delivered, $hanjaLines.Count, $hanPaint.Count, $popupDiff, $result3.Count, $commitDiff)

    $ok = ($delivered -and $hanjaLines.Count -ge 1 -and $type12.Count -ge 1 -and `
           $hanPaint.Count -ge 1 -and $popupDiff -gt 20 -and $result3.Count -ge 1)
    if ($ok) { $verdicts += "HANJA-E2E PASS" }
    elseif ($delivered -and $hanPaint.Count -ge 1) { $verdicts += "INCONCLUSIVE" }
    else { $verdicts += "RIG" }
    Start-Sleep -Seconds 2
}

# --- restore live stack ------------------------------------------------------
Write-Host ""
Write-Host "=== restore live stack ==="
Stop-Stack
Start-Process -FilePath "$build\jkwinserver.exe" -WorkingDirectory $build -WindowStyle Hidden
Start-Sleep -Seconds 5
Start-Process -FilePath "$build\jkdesktop.exe" -WorkingDirectory $build
$pingOk = $false
$pingArg = '{\"tool\":\"ping\",\"args\":{}}'
for ($i = 0; $i -lt 40; ++$i) {
    Start-Sleep -Milliseconds 500
    $ping = (& "$build\jkdesktop.exe" agentctl $pingArg 2>&1 | Out-String)
    if ($ping -match '"ok"\s*:\s*true') { $pingOk = $true; break }
}
Check "restore ping" $pingOk

Write-Host ""
Write-Host ("RESULT: verdicts = {0} | checks pass {1} fail {2}" -f ($verdicts -join ' / '), $script:passN, $script:failN)
if ($script:failN -eq 0 -and ($verdicts | Where-Object { $_ -eq "HANJA-E2E PASS" }).Count -eq $runs) { exit 0 }
if (($verdicts | Where-Object { $_ -eq "INCONCLUSIVE" }).Count -gt 0) { exit 2 }
exit 1