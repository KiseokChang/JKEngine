# verify_dialog_keyboard.ps1 - Enter default / Escape cancel for modal dialogs
# Launches the main demo and uses SendKeys-style keybd_event to open the
# About message box (M), confirm with Enter, open the File dialog (F), and
# cancel with Escape. Validates dialog result messages and focus restoration.
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
$capturedStdout = New-Object System.Collections.Generic.List[string]

$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.FileName = $exe
$psi.UseShellExecute = $false
$psi.RedirectStandardError = $true
$psi.RedirectStandardOutput = $true
$proc = [System.Diagnostics.Process]::Start($psi)

$errEvent = Register-ObjectEvent -InputObject $proc -EventName ErrorDataReceived -Action {
    $line = $EventArgs.Data
    if ($line) { $global:capturedStderr.Add($line) }
}
$outEvent = Register-ObjectEvent -InputObject $proc -EventName OutputDataReceived -Action {
    $line = $EventArgs.Data
    if ($line) { $global:capturedStdout.Add($line) }
}
$proc.BeginErrorReadLine()
$proc.BeginOutputReadLine()

$h = [IntPtr]::Zero
for ($i = 0; $i -lt 40; $i++) {
    Start-Sleep -Milliseconds 250
    [Wci]::TargetPid = [uint32]($proc.Id)
    $cb = [Wci+EnumProc]{ param($hh, $l) [Wci]::Callback($hh, $l) }
    [Wci]::EnumWindows($cb, [IntPtr]::Zero) | Out-Null
    if ([Wci]::Found -ne [IntPtr]::Zero) { $h = [Wci]::Found; break }
}
if ($h -eq [IntPtr]::Zero) { throw "app window not found" }

# Give the SDL window keyboard focus by clicking in the client area center.
$cr = New-Object 'Wci+RECT'
[Wci]::GetClientRect($h, [ref]$cr) | Out-Null
$org = New-Object 'Wci+POINT'
[Wci]::ClientToScreen($h, [ref]$org) | Out-Null
$clickX = $org.x + [int](($cr.R - $cr.L) / 2)
$clickY = $org.y + [int](($cr.B - $cr.T) / 2)
[Wci]::SetForegroundWindow($h) | Out-Null
Start-Sleep -Milliseconds 1500
[Wci]::SetCursorPos($clickX, $clickY) | Out-Null
Start-Sleep -Milliseconds 300
[Wci]::mouse_event(0x0002, 0, 0, 0, [UIntPtr]::Zero) | Out-Null
Start-Sleep -Milliseconds 100
[Wci]::mouse_event(0x0004, 0, 0, 0, [UIntPtr]::Zero) | Out-Null
Start-Sleep -Milliseconds 600

function Send-Key {
    param([byte]$vk, [byte]$scan)
    [Wci]::keybd_event($vk, $scan, 0, [UIntPtr]::Zero) | Out-Null
    Start-Sleep -Milliseconds 100
    [Wci]::keybd_event($vk, $scan, 0x0002, [UIntPtr]::Zero) | Out-Null
    Start-Sleep -Milliseconds 400
}

# Open About message box with 'M', confirm default OK with Enter.
Send-Key 0x4D 0x32   # M
Send-Key 0x0D 0x1C   # Enter

# Open File dialog with 'F', cancel with Escape.
Send-Key 0x46 0x21   # F
Send-Key 0x1B 0x01   # Escape

Start-Sleep -Milliseconds 800

if (-not $proc.HasExited) {
    $proc.CloseMainWindow() | Out-Null
    Start-Sleep -Milliseconds 800
    if (-not $proc.HasExited) { Stop-Process -Id $proc.Id -Force }
}

Unregister-Event -SourceIdentifier $errEvent.Name -ErrorAction SilentlyContinue | Out-Null
Remove-Job $errEvent -ErrorAction SilentlyContinue | Out-Null
Unregister-Event -SourceIdentifier $outEvent.Name -ErrorAction SilentlyContinue | Out-Null
Remove-Job $outEvent -ErrorAction SilentlyContinue | Out-Null

Write-Host "--- captured stdout ---"
$capturedStdout | Write-Host
Write-Host "--- captured stderr ---"
$focusLines = $global:capturedStderr | Where-Object { $_ -match '\[FOCUS\]' }
$focusLines | Write-Host

$aboutClosed = $global:capturedStdout | Where-Object { $_ -match 'About box closed' }
$fileCancel = $global:capturedStdout | Where-Object { $_ -match 'JKFileDialog Cancel' }

$mainFocusIds = $focusLines | ForEach-Object {
    if ($_ -match 'controlId=(\d+)') { [int]$matches[1] }
}

$firstMainFocus = $null
$lastMainFocus = $null
for ($i = 0; $i -lt $mainFocusIds.Count; $i++) {
    if ($mainFocusIds[$i] -ne 0) {
        if ($null -eq $firstMainFocus) { $firstMainFocus = $mainFocusIds[$i] }
        $lastMainFocus = $mainFocusIds[$i]
    }
}

Write-Host ""
Write-Host "First main focus ID: $firstMainFocus"
Write-Host "Last main focus ID:  $lastMainFocus"
Write-Host "About closed message present: $(if ($aboutClosed) { 'yes' } else { 'no' })"
Write-Host "File cancel message present:  $(if ($fileCancel) { 'yes' } else { 'no' })"

$pass = $true
if (-not $aboutClosed) { Write-Host "FAIL: About box did not emit 'About box closed'" -ForegroundColor Red; $pass = $false }
if (-not $fileCancel)  { Write-Host "FAIL: File dialog did not emit 'JKFileDialog Cancel'" -ForegroundColor Red; $pass = $false }
if ($null -eq $firstMainFocus -or $null -eq $lastMainFocus) {
    Write-Host "FAIL: Could not determine focus IDs" -ForegroundColor Red
    $pass = $false
} elseif ($firstMainFocus -ne $lastMainFocus) {
    Write-Host "FAIL: Focus was not restored to original control ($firstMainFocus -> $lastMainFocus)" -ForegroundColor Red
    $pass = $false
}

if ($pass) {
    Write-Host "PASS: Dialog keyboard activation and focus restore work" -ForegroundColor Green
    exit 0
} else {
    exit 1
}
