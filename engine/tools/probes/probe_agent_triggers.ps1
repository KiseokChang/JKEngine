# Desktop Agent M2b triggers probe (docs/32 완료 조건): the full event-bus
# conquest cycle — publish_event → jktriggers (QuickJS bundles) → agent.notify
# → jkchat [알림] transcript lines. PASS/FAIL via exit code.
$ErrorActionPreference = "Continue"
$exe  = "I:\progwork\JKENGINE\engine\build\jkdesktop.exe"
$trig = "I:\progwork\JKENGINE\engine\build\jktriggers.exe"
$chat = "I:\progwork\JKENGINE\engine\build\jkchat.exe"

Get-Process jkdesktop,jktriggers,jkchat -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1

# close_window allow (step 3 graceful close), restore default at the end.
$permFile = Join-Path (Split-Path $exe) "permissions.json"
'{"close_window":"allow"}' | Set-Content -Path $permFile -Encoding ASCII

Start-Process -FilePath $exe -ArgumentList "--server" `
    -WorkingDirectory (Split-Path $exe) -WindowStyle Hidden
Start-Sleep -Seconds 3

function Invoke-Agentctl([string]$json) {
    $escaped = $json -replace '"', '\"'
    return (& $exe agentctl $escaped) -join "`n"
}

# --- jkchat FIRST (lesson 28: subscribe before triggering) ---
$chatProc = Start-Process -FilePath $chat -WorkingDirectory (Split-Path $exe) -PassThru
Start-Sleep -Seconds 3

# --- jktriggers second ---
$trigLog = "$env:TEMP\trig_probe.log"
Start-Process -FilePath $trig -WorkingDirectory (Split-Path $trig) `
    -WindowStyle Hidden `
    -RedirectStandardOutput $trigLog -RedirectStandardError "$env:TEMP\trig_probe.err"
Start-Sleep -Seconds 2

# --- Win32 helpers for cross-process transcript reading ---
Add-Type -TypeDefinition @"
using System; using System.Runtime.InteropServices;
public class W8 {
    [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr p, EnumProc f, IntPtr l);
    public delegate bool EnumProc(IntPtr h, IntPtr l);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, System.Text.StringBuilder s, int n);
    [DllImport("user32.dll", EntryPoint = "SendMessageW")] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
    [DllImport("user32.dll", CharSet = CharSet.Unicode, EntryPoint = "SendMessageW")] public static extern IntPtr SendMessageBuf(IntPtr h, uint m, IntPtr w, System.Text.StringBuilder l);
    // WM_GETTEXTLENGTH under-reports vs WM_GETTEXT on multiline edits
    // (CR/LF accounting) — over-allocate and pass the cap as wParam.
    public static string EditWindowText(IntPtr h) {
        int len = (int)(long)SendMessage(h, 0x000E, IntPtr.Zero, IntPtr.Zero);
        int cap = len * 2 + 4096;
        System.Text.StringBuilder sb = new System.Text.StringBuilder(cap);
        SendMessageBuf(h, 0x000D, (IntPtr)cap, sb);
        return sb.ToString().TrimEnd('\0');
    }
}
"@

$transcriptBefore = ""
$p = Get-Process jkchat -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
$hLog = [IntPtr]::Zero
$global:edits = @()
$cb = {
    param($h, $l)
    $sb = New-Object System.Text.StringBuilder 64
    [W8]::GetClassNameW($h, $sb, 64) | Out-Null
    if ($sb.ToString() -eq "Edit") { $global:edits += $h }
    return $true
}
if ($p) { [W8]::EnumChildWindows($p.MainWindowHandle, $cb, [IntPtr]::Zero) | Out-Null }
foreach ($h in $edits) {
    $t = [W8]::EditWindowText($h)
    if ($t -match '연결') { $hLog = $h; break }
}
if ($hLog -eq [IntPtr]::Zero -and $edits.Count -gt 0) { $hLog = $edits[0] }
if ($hLog -ne [IntPtr]::Zero) { $transcriptBefore = [W8]::EditWindowText($hLog) }

# --- 1. build failure via synthetic terminal.output ---
# NOTE: raw JSON into Invoke-Agentctl (it escapes); no spaces in data —
# agentctl is native argv and spaces split the arg (lesson 26, docs/32 §제한).
$r1 = Invoke-Agentctl '{"tool":"publish_event","args":{"topic":"terminal.output","data":{"text":"x.cpp(12):error:C2084:body"}}}'
Start-Sleep -Seconds 2
Start-Sleep -Seconds 2

# --- 2. taskkill the minesweeper client (crash path) ---
# launch_app spawns "jkdesktop.exe --client minesweeper" — the pid comes from
# list_windows (SpawnerProcess keeps the child handle for exit-code probing).
$r2 = Invoke-Agentctl '{"tool":"launch_app","args":{"app":"minesweeper"}}'
Start-Sleep -Seconds 3
$list1 = Invoke-Agentctl '{"tool":"list_windows","args":{}}'
$mPid = $null
if ($list1 -match '"id\\?":\d+[^}]*"pid\\?":(\d+)') { $mPid = $Matches[1] }
if (-not $mPid) { Write-Host "FAIL: no minesweeper pid ($list1)" }
else { Stop-Process -Id ([int]$mPid) -Force -ErrorAction SilentlyContinue }
Start-Sleep -Seconds 3

# --- 3. graceful close (window.destroyed regression) ---
$transcriptMid = if ($hLog -ne [IntPtr]::Zero) { [W8]::EditWindowText($hLog) } else { "" }
$crashCountMid = ([regex]::Matches($transcriptMid, '앱 비정상 종료')).Count
$r3 = Invoke-Agentctl '{"tool":"launch_app","args":{"app":"minesweeper"}}'
Start-Sleep -Seconds 3
$list = Invoke-Agentctl '{"tool":"list_windows","args":{}}'
$mid = $null
if ($list -match '"id\\?":(\d+)') { $mid = $Matches[1] }
$r4 = Invoke-Agentctl ('{"tool":"close_window","args":{"id":' + $mid + '}}')
Start-Sleep -Seconds 3

# --- 4. idle auto-save with threshold 0 ---
echo 0 | Set-Content -Path (Join-Path (Split-Path $exe) "state\idle_minutes") -Encoding ASCII
Get-Process jktriggers -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1
Start-Process -FilePath $trig -WorkingDirectory (Split-Path $trig) `
    -WindowStyle Hidden `
    -RedirectStandardOutput $trigLog -RedirectStandardError "$env:TEMP\trig_probe2.err"
Start-Sleep -Seconds 12   # interval tick is 10s

# --- capture transcript BEFORE cleanup (dead handle reads as empty) ---
$transcriptAfter = if ($hLog -ne [IntPtr]::Zero) { [W8]::EditWindowText($hLog) } else { "" }

# --- cleanup ---
Get-Process jkdesktop,jktriggers,jkchat -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1
Remove-Item (Join-Path (Split-Path $exe) "state\idle_minutes") -ErrorAction SilentlyContinue
Remove-Item $permFile -ErrorAction SilentlyContinue   # back to default deny

# --- judge ---
$ok = $true
if ($r1 -match 'ok\\?":true') { Write-Host "publish: PASS" } else { $ok = $false; Write-Host "publish: FAIL $r1" }
if ($transcriptAfter -match '\[알림\] 빌드 실패 감지') { Write-Host "build-trigger: PASS" } else { $ok = $false; Write-Host "build-trigger: FAIL" }
if ($transcriptAfter -match '앱 비정상 종료') { Write-Host "crash-trigger: PASS" } else { $ok = $false; Write-Host "crash-trigger: FAIL" }
$crashCountAfter = ([regex]::Matches($transcriptAfter, '앱 비정상 종료')).Count
if ($crashCountAfter -eq $crashCountMid) { Write-Host "graceful-clean: PASS" } else { $ok = $false; Write-Host "graceful-clean: FAIL ($crashCountMid -> $crashCountAfter)" }
if ($r4 -match 'ok\\?":true') { Write-Host "close: PASS" } else { $ok = $false; Write-Host "close: FAIL $r4" }
if ($transcriptAfter -match '\[알림\] 자동 저장') { Write-Host "idle-trigger: PASS" } else { $ok = $false; Write-Host "idle-trigger: FAIL" }
if ((Test-Path $trigLog) -and ((Get-Content $trigLog -Raw) -match 'save_layout -> \{"ok":true')) {
    Write-Host "idle-save: PASS"
} else { $ok = $false; Write-Host "idle-save: FAIL" }

if ($ok) { Write-Host "PASS: agent triggers"; exit 0 } else { Write-Host "FAIL: agent triggers"; exit 1 }