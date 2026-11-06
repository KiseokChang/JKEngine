# vplayer filedlg start-dir probe (2026-09-14 user request: "파일열기하면 현재
# 파일의 폴더를 기준으로 열어야 하는거 아닌가요?").
#
# Verifies: OpenPath records the CURRENT file's folder as lastDir_ (CLI arg,
# drag-drop and picker opens alike), and the 열기... picker spawn carries it
# as "start". Decisive evidence = the server's own spawn log line
# ("JKWindowServer: spawned jkdesktop.exe --filedlg {...\"start\":\"...\"}"),
# read from the redirected server stderr — no dialog-internal state needed.
# Harness recipe = vpt4_e2e.ps1 (-STA, Set-Clipboard) + probe_filedlg_fix.ps1
# (code-point Korean title, capture_window shot). 열기... button measured on
# vpt4-s0-playing.png: layer-relative (856, 42).
param()
$ErrorActionPreference = 'Continue'
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
public class Wt6 {
    [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, int d, UIntPtr e);
    public struct RECT { public int L, T, R, B; }
    public struct POINT { public int X; public int Y; }
}
"@
[Wt6]::SetProcessDPIAware() | Out-Null

$build   = "I:\progwork\JKENGINE\engine\build"
$exe     = "$build\jkdesktop.exe"
$shotDir = "I:\progwork\JKENGINE\.superpowers\sdd\2026-09-14-vplayer-stability"
$mp4     = "I:/progwork/JKENGINE/tmp/vpt2_test.mp4"
$wantStart = 'I:\\progwork\\JKENGINE\\engine\\tmp'   # JSON-escaped in the spawn line
$pildo   = -join [char[]](0xD30C,0xC77C,0x20,0xC5F4,0xAE30)   # 파일 열기 (dialog title)
$errLog  = Join-Path $env:TEMP "vpt6-server-stderr.log"
$script:fails = 0

function Check([string]$name, [bool]$ok, [string]$detail) {
    if ($ok) { Write-Host ("PASS: " + $name) }
    else     { $script:fails++; Write-Host ("FAIL: " + $name + " -- " + $detail) }
    if ($detail) { Write-Host ("      " + $detail) }
}
function Invoke-Agentctl([string]$json) {
    $escaped = $json.Replace('"', [string][char]92 + '"')
    return (& $exe agentctl $escaped 2>&1) -join "`n"
}
$script:srvHwnd = [IntPtr]::Zero
$script:scale = 1.0; $script:ox = 0; $script:oy = 0
$script:layerX = 0; $script:layerY = 0; $script:layerW = 960; $script:layerH = 640

function Find-Server {
    $h = (Get-Process jkdesktop | Where-Object { $_.MainWindowHandle -ne 0 } |
        Select-Object -First 1).MainWindowHandle
    if (-not $h) { Write-Host "FAIL: NO SERVER WINDOW"; exit 1 }
    $script:srvHwnd = [IntPtr]$h
    $crect = New-Object Wt6+RECT
    [Wt6]::GetClientRect($script:srvHwnd, [ref]$crect) | Out-Null
    $co = New-Object Wt6+POINT; $co.X = 0; $co.Y = 0
    [Wt6]::ClientToScreen($script:srvHwnd, [ref]$co) | Out-Null
    $script:scale = ($crect.R - $crect.L) / 1280.0
    $script:ox = $co.X; $script:oy = $co.Y
}
function Find-VPlayerLayer {
    $list = Invoke-Agentctl '{"tool":"list_windows","args":{}}'
    $m = [regex]::Match($list, '"title":"Video Player".*?"x":(-?\d+),"y":(-?\d+),"w":(\d+),"h":(\d+)')
    if (-not $m.Success) { return $false }
    $script:layerX = [int]$m.Groups[1].Value
    $script:layerY = [int]$m.Groups[2].Value
    $script:layerW = [int]$m.Groups[3].Value
    $script:layerH = [int]$m.Groups[4].Value
    return $true
}
function Refresh-Geom {
    Find-Server
    for ($i = 0; $i -lt 10; $i++) { if (Find-VPlayerLayer) { return $true }; Start-Sleep -Milliseconds 300 }
    return $false
}
function Set-LayerCursor([double]$lx, [double]$ly) {
    $px = [int]([math]::Round($script:ox + ($script:layerX + $lx) * $script:scale))
    $py = [int]([math]::Round($script:oy + ($script:layerY + $ly) * $script:scale))
    [Wt6]::SetCursorPos($px, $py) | Out-Null
}
function Send-Click([double]$lx, [double]$ly) {
    Refresh-Geom | Out-Null
    [Wt6]::SetForegroundWindow($script:srvHwnd) | Out-Null
    Start-Sleep -Milliseconds 200
    Set-LayerCursor $lx $ly
    Start-Sleep -Milliseconds 250
    [Wt6]::mouse_event(2, 0, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 80
    [Wt6]::mouse_event(4, 0, 0, 0, [UIntPtr]::Zero)
}
function Type-IntoPath([string]$path) {
    Send-Click 400 42
    Start-Sleep -Milliseconds 300
    [Wt6]::SetForegroundWindow($script:srvHwnd) | Out-Null
    Start-Sleep -Milliseconds 200
    for ($i = 0; $i -lt 5; $i++) {
        try { Set-Clipboard -Value $path; break } catch {
            try { [System.Windows.Forms.Clipboard]::SetText($path); break } catch {
                Start-Sleep -Milliseconds 700 }
        }
    }
    Start-Sleep -Milliseconds 150
    [System.Windows.Forms.SendKeys]::SendWait("{END}")
    Start-Sleep -Milliseconds 100
    [System.Windows.Forms.SendKeys]::SendWait("{BS 300}")
    Start-Sleep -Milliseconds 150
    [System.Windows.Forms.SendKeys]::SendWait("^v")
    Start-Sleep -Milliseconds 400
}
function Get-DialogWin {
    $list = Invoke-Agentctl '{"tool":"list_windows","args":{}}'
    try {
        $wins = ($list | ConvertFrom-Json).windows
        return ($wins | Where-Object { $_.title -eq $pildo } | Select-Object -First 1)
    } catch { return $null }
}

# ---------- setup ----------
taskkill /F /IM jkdesktop.exe 2>$null | Out-Null
Start-Sleep -Seconds 1
Remove-Item $errLog -ErrorAction SilentlyContinue
Start-Process -FilePath $exe -ArgumentList "--server" -WorkingDirectory $build `
    -RedirectStandardError $errLog -WindowStyle Hidden
Start-Sleep -Seconds 4
Check "setup: server up" ((Invoke-Agentctl '{"tool":"ping","args":{}}') -match '"ok"\s*:\s*true') ""
Invoke-Agentctl '{"tool":"launch_app","args":{"app":"vplayer"}}' | Out-Null
Start-Sleep -Seconds 6
Check "setup: vplayer launched" (Refresh-Geom) "no Video Player layer"

# ---------- S1: open a file via the path row (OpenPath records lastDir_) ----------
Type-IntoPath $mp4
Send-Click 808 42            # Open button (vpt2 measured geometry)
Start-Sleep -Seconds 4
Check "S1: playback started" (Refresh-Geom) "vplayer layer gone"

# ---------- S2: 열기... must spawn filedlg with start = the file's folder ----------
Send-Click 856 42            # 열기... button (measured on vpt4-s0-playing.png)
$dlg = $null
for ($i = 0; $i -lt 12; $i++) { $dlg = Get-DialogWin; if ($dlg) { break }; Start-Sleep -Milliseconds 300 }
Check "S2: filedlg window opened" ($null -ne $dlg) "no 파일 열기 layer in list_windows"
if ($dlg) {
    $cap = Invoke-Agentctl ('{"tool":"capture_window","args":{"id":' + $dlg.id + '}}')
    if ($cap -match '"path\\?":\\?"([^"]*)"') {
        $capPath = $Matches[1] -replace '\\\\', '\'
        if ($capPath -and (Test-Path $capPath)) {
            Copy-Item $capPath (Join-Path $shotDir "vpt6-filedlg-start.png") -Force
            Write-Host "shot vpt6-filedlg-start.png"
        }
    }
}
$err = Get-Content $errLog -Raw -ErrorAction SilentlyContinue
$spawn = [regex]::Match($err, '--filedlg[^\r\n]*').Value
if (-not $spawn) { $spawn = "(no --filedlg spawn line found)" }
Write-Host ("      spawn: " + $spawn)
$okStart = $spawn -match ('"start\\?":\\?"' + $wantStart)
Check "S2: spawn start = current file's folder" $okStart ("wanted " + $wantStart)
if ($dlg) {
    # close the dialog so the parked query resolves (cancel path, no side effect)
    Invoke-Agentctl ('{"tool":"close_window","args":{"id":' + $dlg.id + '}}') | Out-Null
    Start-Sleep -Seconds 2
}

Write-Host ("RESULT: " + $(if ($script:fails -eq 0) { "ALL PASS" } else { "$($script:fails) FAILURE(S)" }))
taskkill /F /IM jkdesktop.exe 2>$null | Out-Null