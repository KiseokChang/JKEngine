# probe_lf_ops.ps1 — lf 설정 존재 확인 (docs/44, Phase A 흡수).
# 설정은 %APPDATA%\lf\lfrc에 산다. 파일이 없거나 필수 명령이 빠지면 FAIL.
$rc = Join-Path $env:APPDATA "lf\lfrc"
if (-not (Test-Path $rc)) { Write-Host "FAIL: $rc missing"; exit 1 }
$lines = Get-Content $rc
foreach ($want in @("cmd open", "cmd here", "map o open", "map T here", "cmd agent", "map A agent")) {
    if (-not ($lines | Where-Object { $_ -match [regex]::Escape($want) })) {
        Write-Host "FAIL: lfrc missing '$want'"; exit 1
    }
}
Write-Host "PASS: lfrc has open/here/agent commands"