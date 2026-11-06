$ErrorActionPreference = "Continue"
$exe = "I:\progwork\JKENGINE\engine\build\jkdesktop.exe"
$trig = "I:\progwork\JKENGINE\engine\build\jktriggers.exe"
Get-Process jkdesktop,jktriggers -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Seconds 1
$root = Split-Path $exe
$state = Join-Path $root "state"
$trust = Join-Path $state "trust.json"
# backup pre-existing trust.json for restore - may not be test-created
$hadTrust = Test-Path $trust
if ($hadTrust) { Copy-Item $trust (Join-Path $env:TEMP "trust_pre_t3.json") -Force }
$fp = "sha256:" + ("ab" * 32)   # format-valid fake fingerprint
$fake = "{`"records`":[{`"fingerprint`":`"$fp`",`"name`":`"t3probe`",`"source`":`"dev`",`"ts`":1700000000000}]}"
# merge preserving existing records (simple: replace if none)
if (-not $hadTrust) { Set-Content -Path $trust -Value $fake -Encoding ASCII }
else {
    $cur = Get-Content $trust -Raw
    $cur = $cur -replace '\]\s*}$', (",{`"fingerprint`":`"$fp`",`"name`":`"t3probe`",`"source`":`"dev`",`"ts`":1700000000000}]}")
    [IO.File]::WriteAllText($trust, $cur)
}
Start-Process -FilePath $exe -ArgumentList "--server" -WorkingDirectory $root -WindowStyle Hidden
Start-Sleep -Seconds 3
Start-Process -FilePath $trig -WorkingDirectory (Split-Path $trig) -WindowStyle Hidden
Start-Sleep -Seconds 3
function Invoke-Agentctl([string]$json) {
    $escaped = $json -replace '"', '\"'
    return (& $exe agentctl $escaped) -join "`n"
}
$bad = Invoke-Agentctl ('{"tool":"trust_revoke","args":{"fingerprint":"sha256:xyz"}}')
if ($bad -match 'bad_fingerprint') { Write-Host "t3-badfp: PASS" } else { Write-Host "t3-badfp: FAIL $bad" }
$nf = Invoke-Agentctl ('{"tool":"trust_revoke","args":{"fingerprint":"sha256:' + ("cd" * 32) + '"}}')
if ($nf -match 'not_found') { Write-Host "t3-notfound: PASS" } else { Write-Host "t3-notfound: FAIL $nf" }
# full fp visible in trust_list
$tl = Invoke-Agentctl '{"tool":"trust_list","args":{}}'
if ($tl -match [regex]::Escape($fp)) { Write-Host "t3-fpfield: PASS" } else { Write-Host "t3-fpfield: FAIL $tl" }
# revoke E2E
$job = Start-Job -ScriptBlock {
    param($exe, $fp)
    $body = '{"tool":"trust_revoke","args":{"fingerprint":"' + $fp + '"}}' -replace '"', '\"'
    & $exe agentctl $body
} -ArgumentList $exe, $fp
Start-Sleep -Seconds 3
$ap = Invoke-Agentctl '{"tool":"approve","args":{"request":1,"decision":"allow"}}'
Start-Sleep -Seconds 2
$reply = Receive-Job $job -Wait
if ($reply -match '"restart_needed":true') { Write-Host "t3-reply: PASS" } else { Write-Host "t3-reply: FAIL $reply" }
$after = Get-Content $trust -Raw
if ($after -notmatch ($fp -replace ':', '\:')) { Write-Host "t3-removed: PASS" } else { Write-Host "t3-removed: FAIL" }
$bak = Join-Path $state "trust.json.bak"
if ((Test-Path $bak) -and ((Get-Content $bak -Raw) -match ($fp -replace ':', '\:'))) {
    Write-Host "t3-bak: PASS"
} else { Write-Host "t3-bak: FAIL" }
Get-Process jkdesktop,jktriggers -ErrorAction SilentlyContinue | Stop-Process -Force
# restore: remove t3-created record (remove fake fp row from server-written trust.json)
if ($hadTrust -and (Test-Path (Join-Path $env:TEMP "trust_pre_t3.json"))) {
    Copy-Item (Join-Path $env:TEMP "trust_pre_t3.json") $trust -Force
} elseif (-not $hadTrust) {
    Remove-Item $trust, (Join-Path $state "trust.json.bak") -ErrorAction SilentlyContinue
}