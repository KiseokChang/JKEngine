# probe_agent_events.ps1 — events_list structured catalog (docs/32).
# Contract: agentctl events_list → ok:true, ≥9 catalog entries, every entry
# has topic/source/fields, stats keys (fired/last_ts) present, subscribers
# is a number, window.created present (server-internal topic, always
# cataloged) and its fired reflects at least one spawn in this probe run.
# 4b (task-1, docs/57 §12.6 ⑦): window.focused debounce — re-focus of the
# already-focused id must NOT re-push (old code: +2 per re-focus), a real
# focus change must still push (+1).
# Run from engine/: powershell -File tools/probes/probe_agent_events.ps1
# PowerShell 5.1 compatible.

$ErrorActionPreference = 'Stop'

# Existing probes use absolute paths (PS 5.1 + cwd variance).
$build = 'I:\progwork\JKENGINE\engine\build'
$ctl = "$build\jkdesktop.exe"
if (-not (Test-Path $ctl)) { Write-Output 'events: FAIL (jkdesktop.exe missing)'; exit 1 }

function Invoke-Ctl([string]$json) {
    # PS 5.1 native-arg quoting: bare quotes get stripped (probe lesson) -
    # escape them the way the other agentctl probes do.
    $escaped = $json -replace '"', '\"'
    # [theme] banner pollutes agentctl stdout (docs/52 lesson - JSON line
    # filter, probe_agent_shot/probe_agent_ratelimit precedent). Replies are
    # all JSON objects - keep '{'-leading lines only ([theme] also starts
    # with '[' so a bracket-keeping filter keeps the banner).
    return (($(& $ctl agentctl $escaped 2>$null) |
        Where-Object { $_ -match '^\s*\{' }) -join "`n")
}

function Get-ServerProc {
    Get-CimInstance Win32_Process -Filter "Name='jkdesktop.exe'" |
        Where-Object { $_.CommandLine -match '--server' }
}

# --- 0. server up (spawn if needed) --------------------------------------
$server = Get-ServerProc
if (-not $server) {
    Start-Process -FilePath (Join-Path $build 'jkdesktop.exe') `
        -ArgumentList '--server' -WorkingDirectory $build -WindowStyle Hidden
    Start-Sleep -Seconds 4
}

# --- 1. basic shape: ok / subscribers / events array ----------------------
$raw = Invoke-Ctl '{"tool":"events_list","args":{}}'
if (-not $raw) { Write-Output 'events: FAIL (no reply)'; exit 1 }
$r = $raw | ConvertFrom-Json
$shapeOk = ($r.ok -eq $true) -and ($null -ne $r.subscribers) -and ($null -ne $r.events) -and ($r.events.Count -ge 9)
Write-Output ("shape(ok/subscribers/>=9 entries): {0} (entries={1}, subscribers={2})" -f $(if ($shapeOk) {'PASS'} else {'FAIL'}), $r.events.Count, $r.subscribers)

# --- 2. schema: every entry has topic/source/fields + stats keys ----------
$schemaOk = $true
foreach ($e in $r.events) {
    if (-not $e.topic -or -not $e.source -or ($null -eq $e.fields) -or ($null -eq $e.fired) -or ($null -eq $e.last_ts)) {
        $schemaOk = $false
        Write-Output ("  bad entry: {0}" -f ($e | ConvertTo-Json -Compress))
    }
}
Write-Output ("schema(topic/source/fields/fired/last_ts): {0}" -f $(if ($schemaOk) {'PASS'} else {'FAIL'}))

# --- 3. window.created cataloged and stat-bearing -------------------------
$wc = $r.events | Where-Object { $_.topic -eq 'window.created' }
$wcOk = ($null -ne $wc) -and ($wc.source -eq 'server') -and ($wc.fired -ge 1) -and ($wc.last_ts -gt 0)
Write-Output ("window.created(cataloged/fired>=1/last_ts>0): {0} (fired={1})" -f $(if ($wcOk) {'PASS'} else {'FAIL'}), $wc.fired)

# --- 4. live stat update: spawn minesweeper → fired increments ------------
$before = ($r.events | Where-Object { $_.topic -eq 'window.created' }).fired
$launch = Invoke-Ctl '{"tool":"launch_app","args":{"app":"minesweeper"}}' | ConvertFrom-Json
Start-Sleep -Seconds 3
$raw2 = Invoke-Ctl '{"tool":"events_list","args":{}}' | ConvertFrom-Json
$after = ($raw2.events | Where-Object { $_.topic -eq 'window.created' }).fired
$statOk = ($launch.ok -eq $true) -and ($after -gt $before)
Write-Output ("live-stats(spawn: fired {0}→{1}): {2}" -f $before, $after, $(if ($statOk) {'PASS'} else {'FAIL'}))

# --- 4b. window.focused debounce (task-1) ---------------------------------
#     Discriminator: two focus_window calls on the ALREADY-focused id must
#     leave the fired counter flat (old code pushed on every FocusClient
#     call: +2). Counts are read immediately around the calls — the live
#     desktop is a real user desktop, so the window is kept tight.
function Get-FocusedFired {
    $r = Invoke-Ctl '{"tool":"events_list","args":{}}' | ConvertFrom-Json
    return [int]($r.events | Where-Object { $_.topic -eq 'window.focused' }).fired
}
$fl = Invoke-Ctl '{"tool":"list_windows","args":{}}' | ConvertFrom-Json
$cur = $fl.windows | Where-Object { $_.focused } | Select-Object -First 1
if (-not $cur) {
    # No focused window (edge): establish one first — that focus CHANGE is
    # allowed to push, so re-baseline the counter afterwards.
    $cur = $fl.windows | Select-Object -First 1
    [void](Invoke-Ctl ('{"tool":"focus_window","args":{"id":' + $cur.id + '}}'))
    Start-Sleep -Milliseconds 300
}
$f0 = Get-FocusedFired
[void](Invoke-Ctl ('{"tool":"focus_window","args":{"id":' + $cur.id + '}}'))
[void](Invoke-Ctl ('{"tool":"focus_window","args":{"id":' + $cur.id + '}}'))
$f1 = Get-FocusedFired
$debOk = ($null -ne $cur) -and ($f1 -eq $f0)
Write-Output ("refocus-debounce(same id x2: fired {0}->{1}): {2}" -f $f0, $f1, $(if ($debOk) {'PASS'} else {'FAIL'}))

#     Focus CHANGE must still push. Spawn a 2nd minesweeper: its intake focus
#     sets focusedClientId_ without a push (client not in the client table
#     yet — pre-existing), then re-focusing window 1 is a real id change →
#     exactly one window.focused push.
$spawn2 = Invoke-Ctl '{"tool":"launch_app","args":{"app":"minesweeper"}}' | ConvertFrom-Json
Start-Sleep -Seconds 3
$chgOk = ($spawn2.ok -eq $true)
if ($chgOk) {
    [void](Invoke-Ctl ('{"tool":"focus_window","args":{"id":' + $cur.id + '}}'))
    Start-Sleep -Milliseconds 500
    $f2 = Get-FocusedFired
    $chgOk = ($f2 -eq ($f1 + 1))
    Write-Output ("focus-change-push(2nd spawn + refocus: fired {0}->{1}): {2}" -f $f1, $f2, $(if ($chgOk) {'PASS'} else {'FAIL'}))
} else {
    Write-Output ("focus-change-push(2nd spawn + refocus): FAIL -- " + $spawn2)
}

# --- 5. subscriber counting: probe's control connection is not a ----------
#     subscriber (no AgentEventSubscribe) → subscribers stays at the
#     pre-existing count; a subscriber client (jktriggers) increments it.
$subBefore = $raw2.subscribers
Start-Process -FilePath (Join-Path $build 'jktriggers.exe') -WorkingDirectory $build -WindowStyle Hidden
Start-Sleep -Seconds 3
$raw3 = Invoke-Ctl '{"tool":"events_list","args":{}}' | ConvertFrom-Json
$subOk = ($raw3.subscribers -ge $subBefore) -and ($raw3.subscribers -gt 0)
Write-Output ("subscriber-count(jktriggers: {0}≥{1}>0): {2}" -f $raw3.subscribers, $subBefore, $(if ($subOk) {'PASS'} else {'FAIL'}))

# --- 6. cleanup: close spawned minesweeper window -------------------------
$list = Invoke-Ctl '{"tool":"list_windows","args":{}}' | ConvertFrom-Json
foreach ($w in $list.windows) {
    if ($w.title -match 'minesweeper' -or $w.app -eq 'minesweeper') {
        Invoke-Ctl ('{"tool":"close_window","args":{"id":' + $w.id + '}}') | Out-Null
    }
}

$total = @($shapeOk, $schemaOk, $wcOk, $statOk, $debOk, $chgOk, $subOk).Where({ $_ }).Count
Write-Output ("events: {0}/{1} {2}" -f $total, 7, $(if ($total -eq 7) {'PASS'} else {'FAIL'}))
if ($total -eq 7) { exit 0 } else { exit 1 }