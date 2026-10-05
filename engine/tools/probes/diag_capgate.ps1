# diag_capgate.ps1 — 능력 게이트 end-to-end: set_script에 미선언 API를 심어
# 토크 응답의 고정 에러 문구를 확인한다 (probe-ws 격리 — scratch 슬롯 사용,
# 사용자 슬롯 파일 불변).
$build = "I:\progwork\JKENGINE\engine\build"
$bs = [string][char]92
# agentctl argv는 유효 JSON 그대로여야 한다. PS5.1 native quoting trap
# (probe_workshop_livepatch 컨벤션, docs/55 lesson 3): `& $exe agentctl $json`
# 파이핑은 임베디드 쿼트를 깨먹어 bad_request — 원문 명령줄을 직접 만든다.
# 소스값의 이중 이스케이프(\"gate-ok\")는 백슬래시 선이스케이프가 필요:
# \ → \\ 후 " → \" (아니면 CRT가 2bs+쿼트를 인용 토글로 삼켜 쿼트 소실 —
# argv 덤프 실측). .Replace(원문,비정규식)로 검색식 백슬래시 혼동 회피.
function Invoke-Agentctl([string]$json) {
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = "$build\jkdesktop.exe"
    $psi.Arguments = ('agentctl "' +
        ($json.Replace($bs, $bs + $bs).Replace('"', $bs + '"')) + '"')
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.CreateNoWindow = $true
    $p = [System.Diagnostics.Process]::Start($psi)
    $out = $p.StandardOutput.ReadToEnd()
    $p.WaitForExit()
    return ($out -replace '(?m)^\[theme\][^\r\n]*[\r\n]*', '')
}
# 선대 청소 (idempotent): 전회 런 잔존 Workshop 창이 있으면 set_script가
# "ambiguous"로 답한다 — 닫고 새로 띄운다.
$prev = (Invoke-Agentctl '{"tool":"list_windows","args":{}}')
foreach ($m in [regex]::Matches($prev, '"id":(\d+),"title":"Workshop"')) {
    $null = (Invoke-Agentctl ('{"tool":"close_window","args":{"id":' + $m.Groups[1].Value + '}}'))
}
Start-Sleep -Milliseconds 800
$mk = '{"tool":"launch_app","args":{"app":"workshop"}}'
$null = (Invoke-Agentctl $mk)
Start-Sleep -Seconds 3
# 1) 미선언 토큰(input) — 기대: capability 'input' not declared in MANI
$src = '{"tool":"app_tool","args":{"app":"workshop","tool":"set_script","args":' +
       '{"slot":"gatescratch","source":"var probe = injectMouse(10, 10);"}}}'
$out = (Invoke-Agentctl $src) | Out-String
Write-Output ("GATE-BLOCK: " + (& {
    if ($out -match "capability 'input' not declared in MANI") { "OK" } else { "MISS: " + $out }
}))
# 2) 선언 토큰(widget) — 기대: ok:true (게이트 통과 + 슬래시 정상 재평가)
$src2 = '{"tool":"app_tool","args":{"app":"workshop","tool":"set_script","args":' +
        '{"slot":"gatescratch","source":"var l = createLabel({x:10,y:10,w:100,h:24}, \"gate-ok\");"}}}'
$out2 = (Invoke-Agentctl $src2) | Out-String
Write-Output ("GATE-PASS: " + (& {
    if ($out2 -match '"ok"\s*:\s*true') { "OK" } else { "MISS: " + $out2 }
}))
Write-Output "done"