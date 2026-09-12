# Script trust model probe (docs/37 spec 완료 조건): untrusted dev script ->
# trust_request -> chat approval pipeline (allow) -> script runs + user
# record; restart -> no re-prompt; second script denied -> never runs;
# packer records are trusted from boot.
#
# Deviations from the brief's draft (observed reality, brief's tuning note):
# 1. agent-events buffering: jkdesktop is a GUI-subsystem exe writing to a
#    pipe, so its stdout is fully buffered — Receive-Job returns NOTHING
#    until the process exits (verified: a 12s job produced no partial output
#    at 8s). Polling loops therefore never see mid-run events; instead each
#    phase runs its own events job and drains it with Receive-Job -Wait
#    (probe_agent_chat.ps1 convention of duration ≈ polling window). Pushes
#    are not replayed, so a separate job is started BEFORE the approve lands
#    to catch the script's marker publish.
# 2. Pack records: the packer attests at jkx-pack time (main.cpp PackMode),
#    NOT at load — the brief's "the packer will re-add pack records" after a
#    wholesale trust.json delete does not hold. The probe keeps the packer's
#    source:"pack" records (spec §6.5 "패커 기록 3종은 부팅부터 무프롬프트")
#    and drops only user records; cleanup restores that same post-pack state
#    (a bare delete would strand the 3 pack containers, prompt-parking the
#    next jktriggers boot — breaking probe_agent_triggers in the sweep).
$ErrorActionPreference = "Continue"
$exe   = "I:\progwork\JKENGINE\engine\build\jkdesktop.exe"
$trig  = "I:\progwork\JKENGINE\engine\build\jktriggers.exe"
$state = "I:\progwork\JKENGINE\engine\build\state"
$trustFile = "$state\trust.json"
$devDir = "$state\triggers"

Get-Process jkdesktop,jktriggers -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1

# Rewrite the trust store keeping only source:"pack" records (user records
# dropped — a leftover record for the deterministic probe script would skip
# the prompt).
function Write-PackOnlyStore {
    $packs = @()
    if (Test-Path $trustFile) {
        try {
            $store = Get-Content $trustFile -Raw | ConvertFrom-Json
            $packs = @($store.records | Where-Object { $_.source -eq 'pack' })
        } catch { }
    }
    $parts = foreach ($p in $packs) {
        '{"fingerprint":"' + $p.fingerprint + '","name":"' + $p.name +
        '","source":"pack","ts":' + [int64]$p.ts + '}'
    }
    ('{"records":[' + ($parts -join ',') + ']}') |
        Set-Content -Path $trustFile -Encoding ASCII
    return $packs.Count
}

$packCount0 = Write-PackOnlyStore
if ($packCount0 -lt 3) {
    # Self-healing: the packer attests at pack time, so a wiped/missing store
    # has no pack records. Re-run the offline packer (never contacts the
    # server, CMake's own build step) to regenerate the containers and
    # re-attest — container bytes are deterministic, fingerprints match.
    Write-Host "pack records missing — re-running the offline packer to re-attest"
    & $trig --pack "I:\progwork\JKENGINE\engine\tools\triggers" `
        "I:\progwork\JKENGINE\engine\build\apps\triggers" | Out-Null
    $packCount0 = Write-PackOnlyStore
}
if ($packCount0 -lt 3) {
    Write-Host "WARNING: only $packCount0 pack record(s) after re-pack"
}
New-Item -ItemType Directory -Force -Path $devDir | Out-Null
Remove-Item "$devDir\trust_probe.js","$devDir\trust_probe2.js" -ErrorAction SilentlyContinue

Start-Process -FilePath $exe -ArgumentList "--server" `
    -WorkingDirectory (Split-Path $exe) -WindowStyle Hidden
Start-Sleep -Seconds 3

function Invoke-Agentctl([string]$json) {
    $escaped = $json -replace '"', '\"'
    return (& $exe agentctl $escaped) -join "`n"
}

function Start-TriggerHost {
    Start-Process -FilePath $trig -WorkingDirectory (Split-Path $trig) `
        -WindowStyle Hidden `
        -RedirectStandardOutput "$env:TEMP\trust_probe.log" `
        -RedirectStandardError "$env:TEMP\trust_probe.err"
}

# --- 1. untrusted dev trigger -> trust_request on the event stream --------
# events subscription FIRST (pushes are not replayed — docs/31 lesson)
$evJob = Start-Job -ScriptBlock {
    param($e)
    (& $e agent-events 10) -join "`n"
} -ArgumentList $exe
Start-Sleep -Seconds 2

# untrusted dev trigger: publishes a marker at eval time
'desktop.publish("trust.probe", {"n":1});' | Set-Content -Path "$devDir\trust_probe.js" -Encoding ASCII

Start-TriggerHost
$events = Receive-Job -Job $evJob -Wait | Out-String

# the envelope has "request" before "kind" (docs/31 shape) — take the
# last request id on the stream, probe_agent_chat.ps1 convention
$reqId = $null
if ($events -match '"kind\\?":"trust_request"' -and $events -match '"request\\?":(\d+)') {
    $ids = [regex]::Matches($events, '"request\\?":(\d+)')
    $reqId = $ids[$ids.Count - 1].Groups[1].Value
}
if ($reqId) { Write-Host "approval-request: PASS (request $reqId)" }
else {
    Write-Host "approval-request: FAIL"
    Write-Host "--- events ---"; Write-Host $events
    Get-Content "$env:TEMP\trust_probe.log" -ErrorAction SilentlyContinue
    Get-Process jkdesktop,jktriggers -ErrorAction SilentlyContinue | Stop-Process -Force
    exit 1
}

# --- 2. approve allow -> script runs (marker event) + user record ---------
# subscriber must exist BEFORE the approve lands (pushes are not replayed)
$evJob2 = Start-Job -ScriptBlock {
    param($e)
    (& $e agent-events 10) -join "`n"
} -ArgumentList $exe
Start-Sleep -Seconds 2

$approve = Invoke-Agentctl ('{"tool":"approve","args":{"request":' + $reqId + ',"decision":"allow"}}')
if ($approve -match 'approved\\?":true') { Write-Host "approve: PASS" }
else {
    Write-Host "approve: FAIL $approve"
    Get-Process jkdesktop,jktriggers -ErrorAction SilentlyContinue | Stop-Process -Force
    exit 1
}

$events2 = Receive-Job -Job $evJob2 -Wait | Out-String
if ($events2 -match 'trust\.probe') { Write-Host "script-ran: PASS" }
else {
    Write-Host "script-ran: FAIL"
    Write-Host "--- events ---"; Write-Host $events2
    Get-Content "$env:TEMP\trust_probe.log" -ErrorAction SilentlyContinue
    Get-Process jkdesktop,jktriggers -ErrorAction SilentlyContinue | Stop-Process -Force
    exit 1
}

$trust = Get-Content $trustFile -Raw -ErrorAction SilentlyContinue
if ($trust -match '"source":"user"' -and $trust -match 'trust_probe') {
    Write-Host "user-record: PASS"
} else {
    Write-Host "user-record: FAIL ($trust)"
    Get-Process jkdesktop,jktriggers -ErrorAction SilentlyContinue | Stop-Process -Force
    exit 1
}

# --- 3. restart: trusted now — no new approval, script runs again ---------
Get-Process jktriggers -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1
$evJob3 = Start-Job -ScriptBlock {
    param($e)
    (& $e agent-events 10) -join "`n"
} -ArgumentList $exe
Start-Sleep -Seconds 2
Start-TriggerHost
$events3 = Receive-Job -Job $evJob3 -Wait | Out-String
$ran2 = $events3 -match 'trust\.probe'
$noPrompt = $events3 -notmatch '"kind\\?":"trust_request"'
if ($ran2 -and $noPrompt) { Write-Host "restart-trusted: PASS" }
else {
    Write-Host "restart-trusted: FAIL (ran=$ran2 noPrompt=$noPrompt)"
    Write-Host "--- events ---"; Write-Host $events3
    Get-Content "$env:TEMP\trust_probe.log" -ErrorAction SilentlyContinue
    Get-Process jkdesktop,jktriggers -ErrorAction SilentlyContinue | Stop-Process -Force
    exit 1
}

# --- 4. deny path: second script -> deny -> never runs --------------------
'desktop.publish("trust.probe2", {"n":1});' |
    Set-Content -Path "$devDir\trust_probe2.js" -Encoding ASCII
Get-Process jktriggers -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1
$evJob4 = Start-Job -ScriptBlock {
    param($e)
    (& $e agent-events 12) -join "`n"
} -ArgumentList $exe
Start-Sleep -Seconds 2
Start-TriggerHost
$events4 = Receive-Job -Job $evJob4 -Wait | Out-String
$req2 = $null
$ids = [regex]::Matches($events4, '"request\\?":(\d+)')
if ($ids.Count -gt 0) { $req2 = $ids[$ids.Count - 1].Groups[1].Value }
if ($req2) {
    # subscriber up BEFORE the deny lands so a (wrong) post-deny run is seen
    $evJob5 = Start-Job -ScriptBlock {
        param($e)
        (& $e agent-events 10) -join "`n"
    } -ArgumentList $exe
    Start-Sleep -Seconds 2
    $deny = Invoke-Agentctl ('{"tool":"approve","args":{"request":' + $req2 + ',"decision":"deny"}}')
    if ($deny -match 'approved\\?":false') { Write-Host "deny-decision: PASS" }
    else {
        Write-Host "deny-decision: FAIL $deny"
        Get-Process jkdesktop,jktriggers -ErrorAction SilentlyContinue | Stop-Process -Force
        exit 1
    }
    $events5 = Receive-Job -Job $evJob5 -Wait | Out-String
    if ($events5 -notmatch 'trust\.probe2') { Write-Host "deny-skipped: PASS" }
    else {
        Write-Host "deny-skipped: FAIL"
        Write-Host "--- events ---"; Write-Host $events5
        Get-Content "$env:TEMP\trust_probe.log" -ErrorAction SilentlyContinue
        Get-Process jkdesktop,jktriggers -ErrorAction SilentlyContinue | Stop-Process -Force
        exit 1
    }
} else {
    Write-Host "deny-decision: FAIL (no request)"
    Write-Host "--- events ---"; Write-Host $events4
    Get-Content "$env:TEMP\trust_probe.log" -ErrorAction SilentlyContinue
    Get-Process jkdesktop,jktriggers -ErrorAction SilentlyContinue | Stop-Process -Force
    exit 1
}

# --- 5. pack records trusted from boot: trust_list shows the packs --------
$tl = Invoke-Agentctl '{"tool":"trust_list","args":{}}'
$packCount = ([regex]::Matches($tl, '"source\\?":"pack"')).Count
if ($packCount -ge 3) { Write-Host "pack-records: PASS ($packCount)" }
else {
    Write-Host "pack-records: FAIL ($tl)"
    Get-Process jkdesktop,jktriggers -ErrorAction SilentlyContinue | Stop-Process -Force
    exit 1
}

# --- cleanup ---------------------------------------------------------------
Get-Process jkdesktop,jktriggers -ErrorAction SilentlyContinue | Stop-Process -Force
Remove-Item "$devDir\trust_probe.js","$devDir\trust_probe2.js" -ErrorAction SilentlyContinue
# restore the post-pack steady state: pack records only (drops the user
# record for the now-deleted probe script). Deleting trust.json outright
# would strand the pack containers — see header note.
Write-PackOnlyStore | Out-Null

Write-Host "PASS: agent trust"
exit 0