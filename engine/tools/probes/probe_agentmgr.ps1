$ErrorActionPreference = "Continue"
$exe = "I:\progwork\JKENGINE\engine\build\jkdesktop.exe"
$trig = "I:\progwork\JKENGINE\engine\build\jktriggers.exe"
$root = Split-Path $exe
$permFile = Join-Path $root "permissions.json"
$state = Join-Path $root "state"
$trust = Join-Path $state "trust.json"
$rcpt = Join-Path $state "receipts.jsonl"
$script:fail = 0

function Invoke-Agentctl([string]$json) {
    $escaped = $json -replace '"', '\"'
    return (& $exe agentctl $escaped) -join "`n"
}
function Check([string]$name, [bool]$ok) {
    if ($ok) { Write-Host "ok: $name" } else { Write-Host "FAIL: $name"; $script:fail++ }
}
# BOM-less UTF-8 write (PS5.1 Set-Content UTF8 emits BOM; quickjs rejects BOM)
function Write-NoBom([string]$path, [string]$text) {
    [IO.File]::WriteAllText($path, $text, (New-Object System.Text.UTF8Encoding($false)))
}

# --- guard: permissions.json must not pre-exist (test owns its lifecycle)
if (Test-Path $permFile) { Write-Host "ABORT - permissions.json exists"; exit 1 }
$hadTrust = Test-Path $trust
$hadRcpt = Test-Path $rcpt
if ($hadTrust) { Copy-Item $trust (Join-Path $env:TEMP "trust_pre_mgr.json") -Force }
if ($hadRcpt) { Copy-Item $rcpt (Join-Path $env:TEMP "rcpt_pre_mgr.json") -Force }

# --- seed trust fake record (revoked later in check 4)
$fp = "sha256:" + ("ab" * 32)
$fakeRec = ",{`"fingerprint`":`"$fp`",`"name`":`"mgrprobe`",`"source`":`"dev`",`"ts`":1700000000000}"
if (-not $hadTrust) {
    New-Item -ItemType Directory -Force -Path $state | Out-Null
    Write-NoBom $trust ("{`"records`":[{`"fingerprint`":`"$fp`",`"name`":`"mgrprobe`",`"source`":`"dev`",`"ts`":1700000000000}]}")
} else {
    $cur = Get-Content $trust -Raw
    $cur = $cur -replace '\]\s*}$', ($fakeRec + "]}")
    Write-NoBom $trust $cur
}

Get-Process jkdesktop,jktriggers -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1
Start-Process -FilePath $exe -ArgumentList "--server" -WorkingDirectory $root -WindowStyle Hidden
Start-Sleep -Seconds 3
Start-Process -FilePath $trig -WorkingDirectory (Split-Path $trig) -WindowStyle Hidden
Start-Sleep -Seconds 3

# --- 1. spawn
Invoke-Agentctl '{"tool":"launch_app","args":{"app":"agentmgr"}}' | Out-Null
Start-Sleep -Seconds 4
$wins = Invoke-Agentctl '{"tool":"list_windows","args":{}}'
Check "1-spawn" ($wins -match 'Agent Manager')

# --- 2. agent_permissions shape (permissions.json absent)
$p = Invoke-Agentctl '{"tool":"agent_permissions","args":{}}'
Check "2-shape" ($p -match '"tool":"close_window","gate":"server","file":"","effective":"deny","default":"deny"' -and $p -match '"tool":"permission_set","gate":"server\(fixed\)","file":"","effective":"ask"')

# --- 3. permission_set E2E (parked query in a job; approve request 1)
$job = Start-Job -ScriptBlock {
    param($e)
    $x = '{"tool":"permission_set","args":{"tool":"close_window","decision":"allow"}}' -replace '"', '\"'
    & $e agentctl $x
} -ArgumentList $exe
Start-Sleep -Seconds 3
Invoke-Agentctl '{"tool":"approve","args":{"request":1,"decision":"allow"}}' | Out-Null
Start-Sleep -Seconds 2
$reply = (Receive-Job $job -Wait) -join "`n"
Check "3a-reply" ($reply -match '"written":true')
$fraw = Get-Content $permFile -Raw -ErrorAction SilentlyContinue
Check "3b-file" ($fraw -match '"close_window":"allow"')
$p2 = Invoke-Agentctl '{"tool":"agent_permissions","args":{}}'
Check "3c-filefield" ($p2 -match '"tool":"close_window","gate":"server","file":"allow"')

# --- 4. trust_revoke E2E (approve request 2)
$job2 = Start-Job -ScriptBlock {
    param($e, $f)
    $x = '{"tool":"trust_revoke","args":{"fingerprint":"' + $f + '"}}' -replace '"', '\"'
    & $e agentctl $x
} -ArgumentList $exe, $fp
Start-Sleep -Seconds 3
Invoke-Agentctl '{"tool":"approve","args":{"request":2,"decision":"allow"}}' | Out-Null
Start-Sleep -Seconds 2
$reply2 = (Receive-Job $job2 -Wait) -join "`n"
Check "4a-reply" ($reply2 -match '"restart_needed":true')
$tafter = Get-Content $trust -Raw -ErrorAction SilentlyContinue
Check "4b-removed" ($tafter -notmatch $fp)
Check "4c-bak" ((Test-Path ($trust + ".bak")) -and ((Get-Content ($trust + ".bak") -Raw) -match $fp))

# --- 5. trust_revoke immediate errors
$nf = Invoke-Agentctl ('{"tool":"trust_revoke","args":{"fingerprint":"sha256:' + ("cd" * 32) + '"}}')
Check "5a-notfound" ($nf -match 'not_found')
$bf = Invoke-Agentctl '{"tool":"trust_revoke","args":{"fingerprint":"sha256:xyz"}}'
Check "5b-badfp" ($bf -match 'bad_fingerprint')

# --- 6. installed_list
$il = Invoke-Agentctl '{"tool":"installed_list","args":{}}'
Check "6-installed" ($il -match '"name":"sampletodo","kind":"console"' -and $il -match '"name":"minesweeper","kind":"jkx"')

# --- 7. read_receipts: absent -> []; seed 2 rows -> reverse tail + limit cap
if ($hadRcpt) { Remove-Item $rcpt -Force }
$r0 = Invoke-Agentctl '{"tool":"read_receipts","args":{}}'
Check "7a-empty" ($r0 -match '"rows":\[\]')
$twoRows = '{"ts":1700000001000,"tool":"first_tool","result":"{\"ok\":true}"}' + [char]10 + '{"ts":1700000002000,"tool":"second_tool","result":"{\"ok\":false,\"error\":\"x\"}"}'
Write-NoBom $rcpt $twoRows
$r1 = Invoke-Agentctl '{"tool":"read_receipts","args":{}}'
$r2 = Invoke-Agentctl '{"tool":"read_receipts","args":{"limit":1}}'
Check "7b-reverse" ($r1 -match '"tool":"second_tool"' -and ($r1.IndexOf("second_tool") -lt $r1.IndexOf("first_tool")))
Check "7c-cap" ($r2 -match '"tool":"second_tool"' -and $r2 -notmatch 'first_tool')

# --- 8. trigger toggle regression
# row shape is {"name":"X","topics":[...],"enabled":N} - skip topics (plan note c)
$tl = Invoke-Agentctl '{"tool":"trigger_list","args":{}}'
$tn = [regex]::Match($tl, '"name":"([^"]+)"').Groups[1].Value
$te = [int][regex]::Match($tl, '"name":"' + [regex]::Escape($tn) + '","topics":\[[^\]]*\],"enabled":(\d)').Groups[1].Value
if ($tn -ne "") {
    Invoke-Agentctl ('{"tool":"trigger_toggle","args":{"name":"' + $tn + '","on":' + (1 - $te) + '}}') | Out-Null
    $tl2 = Invoke-Agentctl '{"tool":"trigger_list","args":{}}'
    $te2 = [int][regex]::Match($tl2, '"name":"' + [regex]::Escape($tn) + '","topics":\[[^\]]*\],"enabled":(\d)').Groups[1].Value
    Check "8-toggle" ($te2 -eq (1 - $te))
    Invoke-Agentctl ('{"tool":"trigger_toggle","args":{"name":"' + $tn + '","on":' + $te + '}}') | Out-Null
} else { Check "8-toggle" $false }

# --- cleanup: kill, restore seeded files
Get-Process jkdesktop,jktriggers -ErrorAction SilentlyContinue | Stop-Process -Force
Remove-Item $permFile -Force -ErrorAction SilentlyContinue
if ($hadTrust -and (Test-Path (Join-Path $env:TEMP "trust_pre_mgr.json"))) {
    Copy-Item (Join-Path $env:TEMP "trust_pre_mgr.json") $trust -Force
} elseif (-not $hadTrust) {
    Remove-Item $trust, ($trust + ".bak") -Force -ErrorAction SilentlyContinue
}
if ($hadRcpt -and (Test-Path (Join-Path $env:TEMP "rcpt_pre_mgr.json"))) {
    Copy-Item (Join-Path $env:TEMP "rcpt_pre_mgr.json") $rcpt -Force
} else {
    Remove-Item $rcpt -Force -ErrorAction SilentlyContinue
}
if ($script:fail -gt 0) { Write-Host "probe_agentmgr: $script:fail FAILURES"; exit 1 }
Write-Host "probe_agentmgr: ALL PASS"