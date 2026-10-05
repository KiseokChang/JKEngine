# probe_slot_ship.ps1 — 슬롯 출하 도구 end-to-end (스펙 2026-10-05-slot-ship-tool §6,
# docs/77 as-built). scratch 슬롯(shipscratch) 격리 — 사용자 슬롯(bang-gu/counter/
# fartcar/myapp)과 그 .history는 읽지도 쓰지도 않는다. 서버 불요: slot-pack/jkx-list는
# CLI 서브커맨드이므로 라이브 스택 접촉 없음.
#
# PowerShell 전달 함정(docs/55 lesson 3, docs/76 §8, diag_capgate.ps1 선례):
# agentctl JSON은 `& $exe agentctl $json` 파이핑이 임베디드 쿼트를 삼킨다 — 본
# probe는 CLI 서브커맨드라 인자가 단순하지만, 같은 PSI raw-Arguments 패턴으로
# stdout/stderr를 정확히 잡는다(slot-pack의 빌드 규율 경고는 stderr에 인쇄).
$build = "I:\progwork\JKENGINE\engine\build"
$exe   = Join-Path $build "jkdesktop.exe"
$slotDir = Join-Path $build "state\scripts"
$appsDir = Join-Path $build "apps"
$slotJs  = Join-Path $slotDir "shipscratch.js"
$jxPath  = Join-Path $appsDir "shipscratch.jkx"

function Run-Cli([string]$argLine) {
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $exe
    $psi.Arguments = $argLine
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    # CRT stdout은 콘솔 코드페이지(CP949)로 쓴다 — 슬롯팩의 한국어 경고 행을
    # 제대로 디코드해 원문 receipt를 남긴다 (캡처 파이프라인 깨김 방지).
    $psi.StandardOutputEncoding = [System.Text.Encoding]::GetEncoding(949)
    $psi.StandardErrorEncoding = [System.Text.Encoding]::GetEncoding(949)
    $psi.CreateNoWindow = $true
    $p = [System.Diagnostics.Process]::Start($psi)
    $out = $p.StandardOutput.ReadToEnd()
    $err = $p.StandardError.ReadToEnd()
    $p.WaitForExit()
    return @{ rc = $p.ExitCode; out = $out; err = $err }
}

# ---- 1) scratch 슬롯 원천 생성 (setInterval + createCanvas + injectMouse) ------
# 기대 토큰 순서는 kCapabilityBindTokens 표 순서(JKScriptHost.cpp)에서 온다:
# timer 행(setInterval/clearInterval) < input 행(injectMouse/injectKey/click) <
# canvas 행(createCanvas/canvas*) — 그래서 timer,input,canvas (플랜 T5의
# timer,canvas,input은 표 순서 미각오 — docs/77 교정).
$src = @"
// slot-pack shipscratch probe scratch (probe_slot_ship.ps1 — 도구는 읽기만
// 하고 실행하지 않는다). 세 토큰 1회씩: timer / input / canvas.
setInterval(function(){}, 100);
var cv = createCanvas({x:0, y:0, w:100, h:40}, "");
injectMouse(10, 10);
"@
$src | Set-Content -Path $slotJs -Encoding UTF8
Write-Output ("SHIP-SCRATCH-SEED: " + (& { if (Test-Path $slotJs) { "OK" } else { "MISS" } }))

# ---- 2) slot-pack shipscratch — 기대: RC=0 + "packed" 1행 ---------------------
$pack = Run-Cli "slot-pack shipscratch"
Write-Output ("--- slot-pack stdout ---")
Write-Output $pack.out
Write-Output ("--- slot-pack stderr ---")
Write-Output $pack.err
Write-Output ("--- slot-pack rc=" + $pack.rc + " ---")
$shipPack = (($pack.rc -eq 0) -and ($pack.out -match 'packed') -and
             ($pack.out -match 'caps=timer,input,canvas') -and (Test-Path $jxPath))
Write-Output ("SHIP-PACK: " + (& { if ($shipPack) { "OK" } else { "FAIL" } }))

# ---- 3) jkx-list 조사 — 기대: entries=3 · name=shipscratch · caps · scriptfile ---
# grep은 SUBSTRING만 — capabilities= 뒤에 " 360x280"이 같은 행에 이어 인쇄된다
# ($ 줄끝 앵커 금지, RunJkxList 인쇄 체인 실측).
$lister = Run-Cli ('jkx-list "' + $jxPath + '"')
Write-Output ("--- jkx-list ---")
Write-Output $lister.out
$lout = $lister.out | Out-String
$shipList = (($lister.rc -eq 0) -and
             ($lout -match 'shipscratch\.jkx: version=\d+ codec=\d+ entries=3') -and
             ($lout -match 'name=shipscratch'))
Write-Output ("SHIP-LIST: " + (& {
    if ($shipList) { "OK" } else { "FAIL rc=" + $lister.rc }
}))
$shipCaps = (($lister.rc -eq 0) -and
             ($lout -match 'scriptfile=state/scripts/shipscratch\.js') -and
             ($lout -match 'capabilities=timer,input,canvas'))
Write-Output ("SHIP-CAPS: " + (& {
    if ($shipCaps) { "OK" } else { "FAIL — scriptfile/caps 미적중" }
}))

# ---- 4) 소각 — scratch 슬롯 원천 + 팩 파일 (사용자 슬롯·이력 무접촉 유지) ------
if (Test-Path $jxPath) { Remove-Item $jxPath -Force }
if (Test-Path $slotJs) { Remove-Item $slotJs -Force }
$shipClean = ((-not (Test-Path $jxPath)) -and (-not (Test-Path $slotJs)))
Write-Output ("SHIP-CLEAN: " + (& { if ($shipClean) { "OK" } else { "FAIL — 잔존" } }))
Write-Output "--- ls cleanup receipt: state\scripts ---"
Get-ChildItem -Path $slotDir | Select-Object -ExpandProperty Name
Write-Output "--- ls cleanup receipt: apps (shipscratch.*) ---"
Get-ChildItem -Path $appsDir -Filter "shipscratch*" -ErrorAction SilentlyContinue |
    Select-Object -ExpandProperty Name
Write-Output "done"