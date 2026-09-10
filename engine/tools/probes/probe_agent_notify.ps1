# Desktop Agent notification center probe (docs/33): open_notify toggle,
# publish_event -> unread title badge (MsgType::WindowTitle), history file
# persistence, restart badge restore. PASS/FAIL via exit code.
# NOTE (lesson 34): ImGui apps are not WM_GETTEXT-readable — judge from
# list_windows titles and state files only.
$ErrorActionPreference = "Continue"
$exe = "I:\progwork\JKENGINE\engine\build\jkdesktop.exe"

Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1

# Clean baseline: no stale history from earlier manual runs.
$stateDir = Join-Path (Split-Path $exe) "state"
Remove-Item (Join-Path $stateDir "notify_history.json") -ErrorAction SilentlyContinue

Start-Process -FilePath $exe -ArgumentList "--server" `
    -WorkingDirectory (Split-Path $exe) -WindowStyle Hidden
Start-Sleep -Seconds 3

function Invoke-Agentctl([string]$json) {
    $escaped = $json -replace '"', '\"'
    return (& $exe agentctl $escaped) -join "`n"
}
# Raw JSON in — Invoke-Agentctl escapes (lesson 26/32: native argv).
$list = '{"tool":"list_windows","args":{}}' -replace '"', '\"'
$open = '{"tool":"open_notify","args":{}}' -replace '"', '\"'

function Get-NotifyWindow([string]$listJson) {
    # Returns the list_windows entry for the notify window, or $null.
    if ($listJson -match '\{[^{}]*"title\\?":\\?"Notifications[^{}]*\}') {
        return $Matches[0]
    }
    return $null
}
function Get-Title([string]$entry) {
    if ($entry -match '"title\\?":\\?"([^"]*)"') { return $Matches[1] }
    return ""
}

# --- 1. open_notify spawns the center ---
$r1 = (& $exe agentctl $open) -join "`n"
Start-Sleep -Seconds 3
$l1 = (& $exe agentctl $list) -join "`n"
$w1 = Get-NotifyWindow $l1
$count1 = 0
if ($w1) { $count1 = 1 }

# --- 2. two publishes -> badge Notifications (2) ---
$r2 = Invoke-Agentctl '{"tool":"publish_event","args":{"topic":"agent.notify","data":{"title":"ProbeOne","body":"first"}}}'
$r3 = Invoke-Agentctl '{"tool":"publish_event","args":{"topic":"agent.notify","data":{"title":"ProbeTwo"}}}'
Start-Sleep -Seconds 3
$l2 = (& $exe agentctl $list) -join "`n"
$w2 = Get-NotifyWindow $l2
$title2 = if ($w2) { Get-Title $w2 } else { "" }

# --- 3. history file persisted, 2 unread ---
Start-Sleep -Seconds 1
$hist = Get-Content (Join-Path $stateDir "notify_history.json") -Raw -ErrorAction SilentlyContinue
$histCount = 0
$histUnread = 0
if ($hist) {
    $histCount = ([regex]::Matches($hist, '"topic\\?":')).Count
    $histUnread = ([regex]::Matches($hist, '"read\\?":0')).Count
}

# --- 4. second open_notify toggles (focus), no second window ---
$r4 = (& $exe agentctl $open) -join "`n"
Start-Sleep -Seconds 3
$l3 = (& $exe agentctl $list) -join "`n"
$count3 = ([regex]::Matches($l3, 'Notifications')).Count

# --- 5. restart restore: kill the client, reopen -> badge from file ---
$notifyPid = $null
if ($w2 -match '"pid\\?":(\d+)') { $notifyPid = $Matches[1] }
if ($notifyPid) { Stop-Process -Id ([int]$notifyPid) -Force -ErrorAction SilentlyContinue }
Start-Sleep -Seconds 2
$r5 = (& $exe agentctl $open) -join "`n"
Start-Sleep -Seconds 3
$l4 = (& $exe agentctl $list) -join "`n"
$w4 = Get-NotifyWindow $l4
$title4 = if ($w4) { Get-Title $w4 } else { "" }

# --- cleanup (after all captures — dead handles read as empty, lesson 33) ---
Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1
Remove-Item (Join-Path $stateDir "notify_history.json") -ErrorAction SilentlyContinue

# --- judge ---
$ok = $true
if ($r1 -match 'ok\\?":true' -and $count1 -eq 1) { Write-Host "open-spawn: PASS" } else { $ok = $false; Write-Host "open-spawn: FAIL $r1 / $l1" }
if ($r2 -match 'ok\\?":true' -and $r3 -match 'ok\\?":true') { Write-Host "publish: PASS" } else { $ok = $false; Write-Host "publish: FAIL $r2 / $r3" }
if ($title2 -eq "Notifications (2)") { Write-Host "badge-2: PASS" } else { $ok = $false; Write-Host "badge-2: FAIL ($title2)" }
if ($histCount -ge 2 -and $histUnread -ge 2) { Write-Host "history-file: PASS ($histCount entries, $histUnread unread)" } else { $ok = $false; Write-Host "history-file: FAIL ($histCount / $histUnread)" }
if ($r4 -match 'ok\\?":true' -and $count3 -eq 1) { Write-Host "toggle-focus: PASS" } else { $ok = $false; Write-Host "toggle-focus: FAIL (count=$count3, $l3)" }
if ($title4 -eq "Notifications (2)") { Write-Host "restart-restore: PASS" } else { $ok = $false; Write-Host "restart-restore: FAIL ($title4)" }

if ($ok) { Write-Host "PASS: agent notify center"; exit 0 } else { Write-Host "FAIL: agent notify center"; exit 1 }