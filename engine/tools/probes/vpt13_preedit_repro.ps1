# vpt13_preedit_repro.ps1 - docs/61 section 22.3: single-process terminal
# (jkdesktop.exe terminal) live repro of the Hangul composition overlay.
# Drives the FULL pipeline with evidence at every stage:
#   1. spawn with JKTERM_PREEDIT_DBG=1 (stage logs) + JKTERM_FORCE_HANGUL=1
#      (no LL hook in single-process mode - hangul mode forced at init)
#   2. detach the OS IME (ImmAssociateContext 0) - same as server DetachIme
#   3. baseline screenshot BEFORE typing
#   4. SendVkTap r, k (scan-coded VK input - mandatory, docs/26 lesson)
#   5. pause 800ms (composition must be ON SCREEN, persistent)
#   6. after screenshot + pixel diff analysis vs baseline
#   7. Enter to commit, verify pty bytes via the stage log
# PASS/FAIL via exit code. PowerShell 5.1, ASCII-only.
$ErrorActionPreference = "Continue"
$build = "I:\progwork\JKENGINE\engine\build"
$exe   = "$build\jkdesktop.exe"
$log   = "$build\vpt13_preedit_dbg.log"
$shotA = "$build\vpt13_before.png"
$shotB = "$build\vpt13_after.png"

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
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern uint MapVirtualKey(uint code, uint type);
    [DllImport("user32.dll", SetLastError = true)]
    public static extern uint SendInput(uint n, INPUT[] p, int size);
    [DllImport("imm32.dll")] public static extern IntPtr ImmAssociateContext(IntPtr h, IntPtr imc);
    public struct RECT { public int L, T, R, B; }
    [StructLayout(LayoutKind.Sequential)]
    public struct KBDINPUT { public ushort wVk; public ushort wScan;
        public uint dwFlags; public uint time; public IntPtr dwExtraInfo; }
    [StructLayout(LayoutKind.Explicit)]
    public struct INPUTU { [FieldOffset(0)] public KBDINPUT ki; }
    [StructLayout(LayoutKind.Sequential)]
    public struct INPUT { public uint type; public INPUTU u; }
    public static readonly int Size = Marshal.SizeOf(typeof(INPUT));
    static INPUT Key(ushort vk, ushort scan, uint flags) {
        INPUT i = new INPUT(); i.type = 1;
        i.u.ki.wVk = vk; i.u.ki.wScan = scan; i.u.ki.dwFlags = flags;
        i.u.ki.time = 0; i.u.ki.dwExtraInfo = IntPtr.Zero;
        return i;
    }
    // Scan code MANDATORY on synthetic VK input (SDL drops wScan=0 keydowns).
    public static bool SendVkTap(ushort vk) {
        INPUT[] seq = new INPUT[2];
        seq[0] = Key(vk, (ushort)MapVirtualKey(vk, 0), 0);
        seq[1] = Key(vk, (ushort)MapVirtualKey(vk, 0), 2);
        return SendInput(2, seq, Size) == 2;
    }
    // Direct WM_KEYDOWN/WM_KEYUP to the window - bypasses the input stream
    // (probe_terminal_select Send-Chord precedent: posted letters are legal,
    // only posted MODIFIERS are unreliable). lParam: repeat=1 | scancode<<16.
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint msg, IntPtr w, IntPtr l);
    public static void PostVk(IntPtr hwnd, ushort vk) {
        uint scan = MapVirtualKey(vk, 0);
        long dn = 1L | ((long)scan << 16);
        long up = dn | 0xC0000000L;
        PostMessage(hwnd, 0x100, (IntPtr)vk, (IntPtr)dn);
        PostMessage(hwnd, 0x101, (IntPtr)vk, (IntPtr)up);
    }
}
"@
[Wm]::SetProcessDpiAwarenessContext([IntPtr](-4)) | Out-Null

# --- 1. spawn with evidence env ---------------------------------------------
Remove-Item $log -ErrorAction SilentlyContinue
$env:JKTERM_PREEDIT_DBG = "1"
$env:JKTERM_FORCE_HANGUL = "1"
$proc = Start-Process -FilePath $exe -ArgumentList "terminal" -WorkingDirectory $build `
    -WindowStyle Hidden -PassThru -RedirectStandardError $log
Start-Sleep -Seconds 5

$hwnd = [IntPtr]::Zero
for ($i = 0; $i -lt 40; ++$i) {
    $hwnd = [Wm]::FindWindow("SDL_app", "Terminal")
    if ($hwnd -ne [IntPtr]::Zero) { break }
    Start-Sleep -Milliseconds 250
}
$ok = $true
if ($hwnd -ne [IntPtr]::Zero -and -not [Wm]::IsWindowVisible($hwnd)) {
    [void][Wm]::ShowWindow($hwnd, 8)   # SW_SHOWNOACTIVATE - STARTUPINFO SW_HIDE sticks
    Start-Sleep -Milliseconds 500
}
if ($hwnd -eq [IntPtr]::Zero -or -not [Wm]::IsWindowVisible($hwnd)) {
    $ok = $false
    Write-Host "spawn+window: FAIL (hwnd $hwnd)"
} else {
    [void][Wm]::ShowWindow($hwnd, 8)
    [void][Wm]::SetWindowPos($hwnd, [IntPtr](-1), 0, 0, 0, 0, 0x13)
    Start-Sleep -Milliseconds 400
    # Detach OS IME on the probe window: without it a hangul-mode OS IME turns
    # letters into VK_PROCESSKEY and the repro types nothing (docs/61 s20).
    [void][Wm]::ImmAssociateContext($hwnd, [IntPtr]::Zero)
    if ([Wm]::GetForegroundWindow() -ne $hwnd) {
        [void][Wm]::SetForegroundWindow($hwnd)
        Start-Sleep -Milliseconds 300
    }
    if ([Wm]::GetForegroundWindow() -eq $hwnd) {
        Write-Host ("spawn+window: PASS (hwnd {0})" -f $hwnd)
    } else {
        $ok = $false
        Write-Host "focus: FAIL"
    }
}

function Save-Shot([string]$path) {
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

# --- 2. baseline, type rk (2-set: r=giyok, k=a -> GA), diff ------------------
if ($ok) {
    Start-Sleep -Milliseconds 800
    Save-Shot $shotA
    Write-Host ("sendinput r: {0}" -f [Wm]::SendVkTap(0x52))   # r
    Start-Sleep -Milliseconds 400
    Write-Host ("fg after r: {0} (want {1})" -f [Wm]::GetForegroundWindow(), $hwnd)
    Write-Host ("sendinput k: {0}" -f [Wm]::SendVkTap(0x4B))   # k
    Start-Sleep -Milliseconds 400
    # If the events still never arrived (log empty of KeyDown), fall back to
    # posted WM_KEYDOWN - the delivery, not the automata, is what we tune.
    $logNow = Get-Content $log -ErrorAction SilentlyContinue
    $haveKey = ($logNow | Where-Object { $_ -match 'type=6' }).Count
    if ($haveKey -lt 2) {
        Write-Host ("sendinput produced {0} keydowns - posting WM_KEYDOWN" -f $haveKey)
        [void][Wm]::PostVk($hwnd, 0x52)
        Start-Sleep -Milliseconds 300
        [void][Wm]::PostVk($hwnd, 0x4B)
        Start-Sleep -Milliseconds 500
    }
    Save-Shot $shotB
    Write-Host "typed rk, screenshots taken"
}

# --- 3. Enter commits GA -> pty (verifies the automata ran live) -------------
if ($ok) {
    [void][Wm]::SendVkTap(0x0D)
    Start-Sleep -Milliseconds 900
}

Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
Stop-ProbeProcs

# --- 4. evidence: stage log + pixel diff ------------------------------------
Write-Host "--- stage log ---"
Get-Content $log -ErrorAction SilentlyContinue | Select-Object -First 60
Write-Host "--- pixel diff (before vs after) ---"
$diff = "$build\vpt13_pixel_diff.ps1"
@"
Add-Type -AssemblyName System.Drawing
`$a = New-Object System.Drawing.Bitmap('$shotA')
`$b = New-Object System.Drawing.Bitmap('$shotB')
`$w = [Math]::Min(`$a.Width, `$b.Width); `$h = [Math]::Min(`$a.Height, `$b.Height)
`$diffs = 0; `$greenish = 0; `$minX = -1; `$minY = -1; `$maxX = -1; `$maxY = -1
for (`$y = 0; `$y -lt `$h; `$y += 2) {
  for (`$x = 0; `$x -lt `$w; `$x += 2) {
    `$pa = `$a.GetPixel(`$x, `$y); `$pb = `$b.GetPixel(`$x, `$y)
    if (`$pa.R -ne `$pb.R -or `$pa.G -ne `$pb.G -or `$pa.B -ne `$pb.B) {
      `$diffs++
      if (`$minX -lt 0) { `$minX = `$x; `$minY = `$y }
      if (`$x -gt `$maxX) { `$maxX = `$x }
      if (`$y -gt `$maxY) { `$maxY = `$y }
      if (`$pb.G -gt 120 -and `$pb.G -gt (`$pb.R + 40)) { `$greenish++ }
    }
  }
}
Write-Host ('diff pixels: ' + `$diffs + ' greenish: ' + `$greenish + ' bbox: (' + `$minX + ',' + `$minY + ')-(' + `$maxX + ',' + `$maxY + ')')
"@ | Set-Content -Path $diff -Encoding ASCII
if (Test-Path $shotA) {
    & powershell -NoProfile -ExecutionPolicy Bypass -File $diff
}
if ($ok) { Write-Host "PASS: vpt13 repro rig ran"; exit 0 }
Write-Host "FAIL: vpt13 repro rig"; exit 1