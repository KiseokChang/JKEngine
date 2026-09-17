# probe_settings.ps1 - settings hub end-to-end (specs/2026-09-18-settings-hub).
# ASCII-only (PS5.1). Checks 16: settings_read shape, theme_set round-trip,
# trigger_toggle round-trip, idle_minutes file, receipt prune + .bak,
# audio.master event capture, capture_allow key-ask parking, capture flip gate
# (capture_ask), bad_key/bad_value.
$ErrorActionPreference = "Continue"
$exe = "I:\progwork\JKENGINE\engine\build\jkdesktop.exe"
$root = Split-Path $exe
$state = Join-Path $root "state"
$rcpt = Join-Path $state "receipts.jsonl"
$trigFile = Join-Path $state "triggers.json"
$idleFile = Join-Path $state "idle_minutes"
$kvFile = Join-Path $state "settings.json"
$script:fail = 0

function Invoke-Agentctl([string]$json) {
    $escaped = $json -replace '"', '\"'
    return (& $exe agentctl $escaped) -join "`n"
}
function Check([string]$name, [bool]$ok) {
    if ($ok) { Write-Host "ok: $name" } else { Write-Host "FAIL: $name"; $script:fail++ }
}
function Write-NoBom([string]$path, [string]$text) {
    [IO.File]::WriteAllText($path, $text, (New-Object System.Text.UTF8Encoding($false)))
}

# --- state lifecycle: back up what we touch, restore at the end
$hadRcpt = Test-Path $rcpt
if ($hadRcpt) { Copy-Item $rcpt (Join-Path $env:TEMP "rcpt_pre_set.json") -Force }
$hadTrig = Test-Path $trigFile
if ($hadTrig) { Copy-Item $trigFile (Join-Path $env:TEMP "trig_pre_set.json") -Force }
$hadIdle = Test-Path $idleFile
if ($hadIdle) { Copy-Item $idleFile (Join-Path $env:TEMP "idle_pre_set.txt") -Force }
# check 14b writes capture_* overrides into permissions.json (build root) —
# probe_agentmgr owns that file's lifecycle, so restore/remove it too.
$permFile = Join-Path $root "permissions.json"
$hadPerm = Test-Path $permFile
if ($hadPerm) { Copy-Item $permFile (Join-Path $env:TEMP "perm_pre_set.json") -Force }
# opus 리뷰 MINOR-6: settings.json KV와 theme.json도 사용자 실제 상태 —
# 백업 없이 지우거나 마지막 preset을 고정 복원하면 소각된다. (kvFile은
# 위 state lifecycle 선언을 재사용)
$hadKv = Test-Path $kvFile
if ($hadKv) { Copy-Item $kvFile (Join-Path $env:TEMP "kv_pre_set.json") -Force }
$themeFile = Join-Path $root "theme.json"
$hadTheme = Test-Path $themeFile
if ($hadTheme) { Copy-Item $themeFile (Join-Path $env:TEMP "theme_pre_set.json") -Force }

Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1
Start-Process -FilePath $exe -ArgumentList "--server" -WorkingDirectory $root -WindowStyle Hidden
Start-Sleep -Seconds 4

# --- 1. server up: settings_read ok
$read = Invoke-Agentctl '{"tool":"settings_read","args":{}}'
Check "1-server-up" ($read -match '"ok":true' -and $read -match '"settings"')

# --- 2. spawn the settings app
Invoke-Agentctl '{"tool":"launch_app","args":{"app":"settings"}}' | Out-Null
Start-Sleep -Seconds 4
$wins = Invoke-Agentctl '{"tool":"list_windows","args":{}}'
Check "2-settings-spawn" ($wins -match '"Settings"')

# --- 3. settings_read keys exist (theme/idle/audio)
Check "3-read-keys" ($read -match '"key":"theme.current"' -and
                     $read -match '"key":"idle_minutes"' -and
                     $read -match '"key":"audio_master_volume"')

# --- 4. receipts stats present
Check "4-receipts-field" ($read -match '"receipts"' -and $read -match '"rows"')

# --- 5. theme dark -> read reflects
Invoke-Agentctl '{"tool":"theme_set","args":{"preset":"dark"}}' | Out-Null
Start-Sleep -Seconds 1
$read2 = Invoke-Agentctl '{"tool":"settings_read","args":{}}'
Check "5-theme-dark" ($read2 -match '"theme.current"[^}]*"value":"dark"')

# --- 6. theme light -> read reflects -> dark restore
Invoke-Agentctl '{"tool":"theme_set","args":{"preset":"light"}}' | Out-Null
Start-Sleep -Seconds 1
$read3 = Invoke-Agentctl '{"tool":"settings_read","args":{}}'
$themeLight = $read3 -match '"theme.current"[^}]*"value":"light"'
Invoke-Agentctl '{"tool":"theme_set","args":{"preset":"dark"}}' | Out-Null
Check "6-theme-light-restore" $themeLight

# --- 7. trigger_toggle off -> settings_read 0 (seed triggers.json)
Write-NoBom $trigFile "{`"triggers`":[{`"name`":`"setprobe`",`"enabled`":1}]}"
Start-Sleep -Seconds 1
Invoke-Agentctl '{"tool":"trigger_toggle","args":{"name":"setprobe","on":0}}' | Out-Null
Start-Sleep -Seconds 1
$read4 = Invoke-Agentctl '{"tool":"settings_read","args":{}}'
Check "7-trigger-off" ($read4 -match '"key":"trigger\.setprobe"[^}]*"value":0')

# --- 8. trigger on restore
Invoke-Agentctl '{"tool":"trigger_toggle","args":{"name":"setprobe","on":1}}' | Out-Null
Start-Sleep -Seconds 1
$read5 = Invoke-Agentctl '{"tool":"settings_read","args":{}}'
Check "8-trigger-on" ($read5 -match '"key":"trigger\.setprobe"[^}]*"value":1')

# --- 9. idle_minutes 5 -> file "5"
Invoke-Agentctl '{"tool":"settings_set","args":{"key":"idle_minutes","value":5}}' | Out-Null
Start-Sleep -Seconds 1
$idle = (Get-Content $idleFile -Raw).Trim()
Check "9-idle-file" ($idle -eq "5")

# --- 10. idle 30 restore
Invoke-Agentctl '{"tool":"settings_set","args":{"key":"idle_minutes","value":30}}' | Out-Null
Start-Sleep -Seconds 1
$idle2 = (Get-Content $idleFile -Raw).Trim()
Check "10-idle-restore" ($idle2 -eq "30")

# --- 11. receipt prune: old row removed + .bak created
# receipts ts is epoch ms (read_receipts contract). Seed old (30d) + fresh.
$nowMs = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
$oldMs = $nowMs - [int64](30 * 86400 * 1000)
Write-NoBom $rcpt ("{`"ts`":" + $oldMs + ",`"tool`":`"probeold`",`"result`":{`"ok`":true}}`n{`"ts`":" + $nowMs + ",`"tool`":`"probefresh`",`"result`":{`"ok`":true}}`n")
Start-Sleep -Seconds 1
Invoke-Agentctl '{"tool":"settings_set","args":{"key":"receipt_retention_days","value":7}}' | Out-Null
Start-Sleep -Seconds 1
$rcptBody = (Get-Content $rcpt -Raw)
$oldGone = ($rcptBody -notmatch 'probeold') -and ($rcptBody -match 'probefresh')
$bakThere = Test-Path ($rcpt + ".bak")
Check "11-prune-bak" ($oldGone -and $bakThere)
if (-not $oldGone) { Write-Host "  (old row present)" }

# --- 12. retention 0 / 3 -> bad_value (하한 7 — opus M4)
$r0 = Invoke-Agentctl '{"tool":"settings_set","args":{"key":"receipt_retention_days","value":0}}'
$r3 = Invoke-Agentctl '{"tool":"settings_set","args":{"key":"receipt_retention_days","value":3}}'
Check "12-retention-zero-bad" ($r0 -match 'bad_value' -and $r3 -match 'bad_value')

# --- 13. audio_master_mute 1 -> audio.master event + read reflects
function Wait-EventLine([string]$file, [string]$needle, [int]$maxSec) {
    $deadline = (Get-Date).AddSeconds($maxSec)
    while ((Get-Date) -lt $deadline) {
        $t = (Get-Content $file -Raw -ErrorAction SilentlyContinue)
        if ($t -and ($t -match $needle)) { return $t }
        Start-Sleep -Milliseconds 500
    }
    return ""
}
$evPath = Join-Path $env:TEMP "settings_events.txt"
Remove-Item $evPath -Force -ErrorAction SilentlyContinue
$evProc = Start-Process -FilePath $exe -ArgumentList "agent-events","30" `
    -WorkingDirectory $root -WindowStyle Hidden `
    -RedirectStandardOutput $evPath -PassThru
Start-Sleep -Seconds 2
Invoke-Agentctl '{"tool":"settings_set","args":{"key":"audio_master_mute","value":1}}' | Out-Null
Start-Sleep -Seconds 2
$read6 = Invoke-Agentctl '{"tool":"settings_read","args":{}}'
$evText = Wait-EventLine $evPath '"topic":"audio\.master".*?"mute":1' 20
$muteSeen = $read6 -match '"key":"audio_master_mute"[^}]*"value":1'
Check "13-audio-master-event" ($evText -ne "" -and $muteSeen)

# --- 14. capture_allow by agentctl -> parked approval -> approve -> written
# opus M3: 파킹 가용 검사는 control-only(jkchat류) 연결을 센다 — jkchat을
# 먼저 띄워 승인 표면을 확보한다.
Invoke-Agentctl '{"tool":"launch_chat","args":{}}' | Out-Null
Start-Sleep -Seconds 4
$approveJob = Start-Job -ScriptBlock {
    param($e)
    $esc = '{"tool":"settings_set","args":{"key":"capture_allow","value":1}}' -replace '"', '\"'
    return (& $e agentctl $esc) -join "`n"
} -ArgumentList $exe
Start-Sleep -Seconds 2
$reqId = 0
$evText2 = Wait-EventLine $evPath '"topic":"agent\.approval_request".*?"kind":"capture_allow"' 20
if ($evText2) {
    $allReq0 = [regex]::Matches($evText2, '"request":(\d+)')
    if ($allReq0.Count -gt 0) { $reqId = [int]$allReq0[$allReq0.Count - 1].Groups[1].Value }
}
Check "14a-capture-parked" ($reqId -gt 0)
$approve = Invoke-Agentctl ('{"tool":"approve","args":{"request":' + $reqId + ',"decision":"allow"}}')
Wait-Job $approveJob -Timeout 40 | Out-Null
$parked = Receive-Job $approveJob | Out-String
Remove-Job $approveJob -Force
Check "14b-capture-approved" ($parked -match '"ok":true' -and $parked -match '"written":true')

# --- 16. flip gate: capture_allow=0 (park->approve) -> capture_window refuses
# approval_request는 파일에 누적된다 — "카운트 증가" 폴링으로 새 파킹만 잡는다.
function Wait-NewApproval([string]$file, [int]$prevCount, [int]$maxSec) {
    $deadline = (Get-Date).AddSeconds($maxSec)
    while ((Get-Date) -lt $deadline) {
        $t = (Get-Content $file -Raw -ErrorAction SilentlyContinue)
        if ($t) {
            $c = [regex]::Matches($t, '"kind":"capture_allow"').Count
            if ($c -gt $prevCount) { return $t }
        }
        Start-Sleep -Milliseconds 500
    }
    return ""
}
function Last-RequestId([string]$text) {
    if (-not $text) { return 0 }
    $all = [regex]::Matches($text, '"request":(\d+)')
    if ($all.Count -eq 0) { return 0 }
    return [int]$all[$all.Count - 1].Groups[1].Value
}
$baseText = (Get-Content $evPath -Raw -ErrorAction SilentlyContinue)
$baseCap = 0
if ($baseText) { $baseCap = [regex]::Matches($baseText, '"kind":"capture_allow"').Count }
$capOffJob = Start-Job -ScriptBlock {
    param($e)
    $esc = '{"tool":"settings_set","args":{"key":"capture_allow","value":0}}' -replace '"', '\"'
    return (& $e agentctl $esc) -join "`n"
} -ArgumentList $exe
Start-Sleep -Seconds 2
$reqId3 = Last-RequestId (Wait-NewApproval $evPath $baseCap 20)
Invoke-Agentctl ('{"tool":"approve","args":{"request":' + $reqId3 + ',"decision":"allow"}}') | Out-Null
Wait-Job $capOffJob -Timeout 40 | Out-Null
Remove-Job $capOffJob -Force -ErrorAction SilentlyContinue
Start-Sleep -Seconds 1
$capRefused = Invoke-Agentctl '{"tool":"capture_window","args":{"id":999999}}'
Check "16-capture-ask-gate" ($capRefused -match 'capture_ask')
# capture allow 복원(다른 프로브 영향 방지 — permissions.json은 cleanup이 복원)
$midText = (Get-Content $evPath -Raw -ErrorAction SilentlyContinue)
$midCap = 0
if ($midText) { $midCap = [regex]::Matches($midText, '"kind":"capture_allow"').Count }
$capJob2 = Start-Job -ScriptBlock {
    param($e)
    $esc = '{"tool":"settings_set","args":{"key":"capture_allow","value":1}}' -replace '"', '\"'
    return (& $e agentctl $esc) -join "`n"
} -ArgumentList $exe
Start-Sleep -Seconds 2
$reqId4 = Last-RequestId (Wait-NewApproval $evPath $midCap 20)
Invoke-Agentctl ('{"tool":"approve","args":{"request":' + $reqId4 + ',"decision":"allow"}}') | Out-Null
Wait-Job $capJob2 -Timeout 40 | Out-Null
Remove-Job $capJob2 -Force -ErrorAction SilentlyContinue

# --- 15. bad_key + bad_value
$bk = Invoke-Agentctl '{"tool":"settings_set","args":{"key":"nope","value":1}}'
$bv = Invoke-Agentctl '{"tool":"settings_set","args":{"key":"audio_master_volume","value":101}}'
Check "15-bad-key-value" ($bk -match 'bad_key' -and $bv -match 'bad_value')

# --- cleanup: restore state files, kill server
$evProc | Stop-Process -Force -ErrorAction SilentlyContinue
Get-Process jkdesktop -ErrorAction SilentlyContinue | Stop-Process -Force
if ($hadRcpt) { Copy-Item (Join-Path $env:TEMP "rcpt_pre_set.json") $rcpt -Force }
else {
    Remove-Item $rcpt -Force -ErrorAction SilentlyContinue
    Remove-Item ($rcpt + ".bak") -Force -ErrorAction SilentlyContinue
}
if ($hadTrig) { Copy-Item (Join-Path $env:TEMP "trig_pre_set.json") $trigFile -Force }
else { Remove-Item $trigFile -Force -ErrorAction SilentlyContinue }
if ($hadIdle) { Copy-Item (Join-Path $env:TEMP "idle_pre_set.txt") $idleFile -Force }
else { Remove-Item $idleFile -Force -ErrorAction SilentlyContinue }
if ($hadPerm) { Copy-Item (Join-Path $env:TEMP "perm_pre_set.json") $permFile -Force }
else { Remove-Item $permFile -Force -ErrorAction SilentlyContinue }
if ($hadKv) { Copy-Item (Join-Path $env:TEMP "kv_pre_set.json") $kvFile -Force }
else { Remove-Item $kvFile -Force -ErrorAction SilentlyContinue }
if ($hadTheme) { Copy-Item (Join-Path $env:TEMP "theme_pre_set.json") $themeFile -Force }
Get-Process jkchat -ErrorAction SilentlyContinue | Stop-Process -Force
Remove-Item ($rcpt + ".bak") -Force -ErrorAction SilentlyContinue

Write-Host ("RESULT: " + ($(if ($script:fail -eq 0) { "ALL PASS" } else { "$($script:fail) FAIL" })))
if ($script:fail -eq 0) { exit 0 } else { exit 1 }