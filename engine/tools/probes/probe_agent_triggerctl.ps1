# Desktop Agent trigger on/off probe (docs/34): trigger_toggle blocks and
# resumes a real trigger end-to-end, judged via the notify center's history
# file (agent.notify subscriber) — ImGui apps are not WM_GETTEXT-readable.
$ErrorActionPreference = "Continue"
$exe = "I:\progwork\JKENGINE\engine\build\jkdesktop.exe"
$trig = "I:\progwork\JKENGINE\engine\build\jktriggers.exe"

Get-Process jkdesktop,jktriggers -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1
$stateDir = Join-Path (Split-Path $exe) "state"
Remove-Item (Join-Path $stateDir "triggers.json") -ErrorAction SilentlyContinue
Remove-Item (Join-Path $stateDir "triggers_loaded.json") -ErrorAction SilentlyContinue
Remove-Item (Join-Path $stateDir "notify_history.json") -ErrorAction SilentlyContinue

Start-Process -FilePath $exe -ArgumentList "--server" `
    -WorkingDirectory (Split-Path $exe) -WindowStyle Hidden
Start-Sleep -Seconds 3
Start-Process -FilePath $trig -WorkingDirectory (Split-Path $trig) -WindowStyle Hidden
Start-Sleep -Seconds 3

function Invoke-Agentctl([string]$json) {
    $escaped = $json -replace '"', '\"'
    return (& $exe agentctl $escaped) -join "`n"
}
# Raw JSON in — Invoke-Agentctl escapes (native argv, no spaces allowed).

# --- 1. loaded manifest: jktriggers wrote its flat rows ---
$manifest = Get-Content (Join-Path $stateDir "triggers_loaded.json") -Raw -ErrorAction SilentlyContinue
$hasBuild = ($manifest -match 'trig_build')
$hasIdle = ($manifest -match 'trig_idle')
$hasCrash = ($manifest -match 'trig_crash')

# --- 2. trigger_list: ok + all three, enabled 1 ---
$list = Invoke-Agentctl '{"tool":"trigger_list","args":{}}'
$listOk = $list -match 'ok\\?":true' -and `
    ($list -match 'trig_build') -and ($list -match 'trig_idle') -and `
    ($list -match 'trig_crash') -and ($list -match 'enabled\\?":1')
$listCount = ([regex]::Matches($list, '"name\\?":')).Count

# --- 3. off-blocks: toggle trig_build off, publish, history unchanged ---
$r0 = Invoke-Agentctl '{"tool":"launch_app","args":{"app":"notify"}}'
Start-Sleep -Seconds 3
$rOff = Invoke-Agentctl '{"tool":"trigger_toggle","args":{"name":"trig_build","on":0}}'
Start-Sleep -Seconds 2   # reload lands via triggers.reload
$rPubOff = Invoke-Agentctl '{"tool":"publish_event","args":{"topic":"terminal.output","data":{"text":"x.cpp(12):error:C2084:body"}}}'
Start-Sleep -Seconds 4
$hist1 = Get-Content (Join-Path $stateDir "notify_history.json") -Raw -ErrorAction SilentlyContinue
$count1 = 0
if ($hist1) { $count1 = ([regex]::Matches($hist1, '"topic\\?":')).Count }

# --- 4. on-resumes: toggle back on, publish, history +1 ---
$rOn = Invoke-Agentctl '{"tool":"trigger_toggle","args":{"name":"trig_build","on":1}}'
Start-Sleep -Seconds 2
$rPubOn = Invoke-Agentctl '{"tool":"publish_event","args":{"topic":"terminal.output","data":{"text":"y.cpp(30):error:C2065:body2"}}}'
Start-Sleep -Seconds 4
$hist2 = Get-Content (Join-Path $stateDir "notify_history.json") -Raw -ErrorAction SilentlyContinue
$count2 = 0
if ($hist2) { $count2 = ([regex]::Matches($hist2, '"topic\\?":')).Count }

# --- 5. state file: trig_build enabled 1 after the toggle back ---
$flags = Get-Content (Join-Path $stateDir "triggers.json") -Raw -ErrorAction SilentlyContinue
$flagsOk = $flags -match 'trig_build' -and $flags -match '"enabled\\?":1'

# --- cleanup (after captures — lesson 33) ---
Get-Process jkdesktop,jktriggers -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1
Remove-Item (Join-Path $stateDir "triggers.json") -ErrorAction SilentlyContinue
Remove-Item (Join-Path $stateDir "triggers_loaded.json") -ErrorAction SilentlyContinue
Remove-Item (Join-Path $stateDir "notify_history.json") -ErrorAction SilentlyContinue

# --- judge ---
$ok = $true
if ($hasBuild -and $hasIdle -and $hasCrash) { Write-Host "loaded-manifest: PASS" } else { $ok = $false; Write-Host "loaded-manifest: FAIL $manifest" }
if ($listOk -and $listCount -ge 3) { Write-Host "list: PASS ($listCount)" } else { $ok = $false; Write-Host "list: FAIL $list" }
if ($rOff -match 'ok\\?":true' -and $count1 -eq 0) { Write-Host "off-blocks: PASS" } else { $ok = $false; Write-Host "off-blocks: FAIL (count1=$count1, $rOff)" }
if ($rOn -match 'ok\\?":true' -and $count2 -eq 1) { Write-Host "on-resumes: PASS (count2=$count2)" } else { $ok = $false; Write-Host "on-resumes: FAIL (count2=$count2, $rOn)" }
if ($flagsOk) { Write-Host "state-file: PASS" } else { $ok = $false; Write-Host "state-file: FAIL $flags" }

if ($ok) { Write-Host "PASS: agent triggerctl"; exit 0 } else { Write-Host "FAIL: agent triggerctl"; exit 1 }