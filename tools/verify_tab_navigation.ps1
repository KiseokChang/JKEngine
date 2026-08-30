# verify_tab_navigation.ps1 - Tab/Shift+Tab focus cycle verification
# Uses SendKeys to send real Tab/Shift+Tab keystrokes to the SDL window and
# checks stderr focus logs emitted by JKControl::SetFocus.
param(
    [string]$exe = "i:\progwork\JKENGINE\prototype\sdl2_jkwindow\build\jkproto_sdl2_jkwindow.exe"
)
$ErrorActionPreference = 'Stop'

Add-Type @"
using System;
using System.Text;
using System.Runtime.InteropServices;
public class Wci {
  public delegate bool EnumProc(IntPtr h, IntPtr l);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L; public int T; public int R; public int B; }
  [StructLayout(LayoutKind.Sequential)] public struct POINT { public int x; public int y; }
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr l);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
  [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder sb, int max);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr v);
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  [DllImport("user32.dll")] public static extern void keybd_event(byte bVk, byte bScan, uint dwFlags, UIntPtr dwExtraInfo);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, int dx, int dy, uint d, UIntPtr e);
  [DllImport("kernel32.dll")] public static extern IntPtr GetConsoleWindow();
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int nCmdShow);
  [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr h);
  public static uint TargetPid;
  public static IntPtr Found;
  public static bool Callback(IntPtr h, IntPtr l) {
    uint pid; GetWindowThreadProcessId(h, out pid);
    if (pid == TargetPid) {
      StringBuilder cls = new StringBuilder(256);
      GetClassNameW(h, cls, 256);
      if (cls.ToString().Equals("SDL_app", StringComparison.OrdinalIgnoreCase)) { Found = h; return false; }
    }
    return true;
  }
}
"@
if (-not [Wci]::SetProcessDpiAwarenessContext([IntPtr](-4))) { [Wci]::SetProcessDPIAware() | Out-Null }

if (-not (Test-Path $exe)) { throw "exe not found: $exe" }

Get-Process -Name "jkproto_sdl2_jkwindow" -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 400

$capturedStderr = New-Object System.Collections.Generic.List[string]

$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.FileName = $exe
$psi.UseShellExecute = $false
$psi.RedirectStandardError = $true
$proc = [System.Diagnostics.Process]::Start($psi)

$errEvent = Register-ObjectEvent -InputObject $proc -EventName ErrorDataReceived -Action {
    $line = $EventArgs.Data
    if ($line) { $global:capturedStderr.Add($line) }
}
$proc.BeginErrorReadLine()

$h = [IntPtr]::Zero
for ($i = 0; $i -lt 40; $i++) {
    Start-Sleep -Milliseconds 250
    [Wci]::TargetPid = [uint32]($proc.Id)
    $cb = [Wci+EnumProc]{ param($hh, $l) [Wci]::Callback($hh, $l) }
    [Wci]::EnumWindows($cb, [IntPtr]::Zero) | Out-Null
    if ([Wci]::Found -ne [IntPtr]::Zero) { $h = [Wci]::Found; break }
}
if ($h -eq [IntPtr]::Zero) { throw "app window not found" }
# Push the PowerShell console behind the SDL window so keybd_event reaches SDL.
$consoleHwnd = [Wci]::GetConsoleWindow()
if ($consoleHwnd -ne [IntPtr]::Zero) {
    [Wci]::SetForegroundWindow($h) | Out-Null
}
Start-Sleep -Milliseconds 1500

# Give SDL window keyboard focus by clicking its client area center.
$cr = New-Object 'Wci+RECT'
[Wci]::GetClientRect($h, [ref]$cr) | Out-Null
$org = New-Object 'Wci+POINT'
[Wci]::ClientToScreen($h, [ref]$org) | Out-Null
$clickX = $org.x + [int](($cr.R - $cr.L) / 2)
$clickY = $org.y + [int](($cr.B - $cr.T) / 2)
[Wci]::SetCursorPos($clickX, $clickY) | Out-Null
Start-Sleep -Milliseconds 300
[Wci]::mouse_event(0x0002, 0, 0, 0, [UIntPtr]::Zero) | Out-Null
Start-Sleep -Milliseconds 100
[Wci]::mouse_event(0x0004, 0, 0, 0, [UIntPtr]::Zero) | Out-Null
Start-Sleep -Milliseconds 600

# Re-verify foreground and activate SDL window.
$fg = [Wci]::GetForegroundWindow()
if ($fg -ne $h) {
    [Wci]::ShowWindow($h, 5) | Out-Null
    [Wci]::BringWindowToTop($h) | Out-Null
    [Wci]::SetForegroundWindow($h) | Out-Null
    Start-Sleep -Milliseconds 500
}

function Send-Tab {
    param([bool]$shift = $false)
    if ($shift) {
        [Wci]::keybd_event(0x10, 0x2A, 0, [UIntPtr]::Zero) | Out-Null  # VK_LSHIFT down
        Start-Sleep -Milliseconds 50
    }
    [Wci]::keybd_event(0x09, 0x0F, 0, [UIntPtr]::Zero) | Out-Null        # VK_TAB down
    Start-Sleep -Milliseconds 100
    [Wci]::keybd_event(0x09, 0x0F, 0x0002, [UIntPtr]::Zero) | Out-Null   # VK_TAB up
    if ($shift) {
        Start-Sleep -Milliseconds 50
        [Wci]::keybd_event(0x10, 0x2A, 0x0002, [UIntPtr]::Zero) | Out-Null  # VK_LSHIFT up
    }
    Start-Sleep -Milliseconds 300
}

# Send Tab 6 times to cycle through focusable controls.
$tabs = 6
for ($i = 0; $i -lt $tabs; $i++) {
    Send-Tab $false
}

# Send Shift+Tab 6 times to cycle backwards.
for ($i = 0; $i -lt $tabs; $i++) {
    Send-Tab $true
}

if (-not $proc.HasExited) {
    $proc.CloseMainWindow() | Out-Null
    Start-Sleep -Milliseconds 800
    if (-not $proc.HasExited) { Stop-Process -Id $proc.Id -Force }
}

Unregister-Event -SourceIdentifier $errEvent.Name -ErrorAction SilentlyContinue | Out-Null
Remove-Job $errEvent -ErrorAction SilentlyContinue | Out-Null

Write-Host "--- captured stderr ---"
$focusLines = $global:capturedStderr | Where-Object { $_ -match '\[FOCUS\]' }
$focusLines | Write-Host

$uniqueControls = $focusLines | ForEach-Object {
    if ($_ -match 'controlId=(\d+)') { [int]$matches[1] }
} | Sort-Object -Unique

Write-Host ""
Write-Host "Unique focus control IDs seen: $($uniqueControls -join ', ')"
if ($uniqueControls.Count -ge 4) {
    Write-Host "PASS: Tab navigation cycled through at least 4 controls" -ForegroundColor Green
    exit 0
} else {
    Write-Host "FAIL: Expected at least 4 distinct focusable controls, saw $($uniqueControls.Count)" -ForegroundColor Red
    exit 1
}
