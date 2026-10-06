# probe_library_list.ps1 — library-list CLI 서버리 검증 (스펙 2026-10-06-app-library
# §6, probe_slot_ship.ps1 템플릿 재용). 서버 불요: library-list는 순수 stdio
# 서브커맨드이므로 라이브 스택 접촉 없음 — 접미는 실제 기기 apps/의 진실원.
#
# 판정안(3정판 + CLEAN 소각 수령 — posix는 Task 4 몫, 아래 LIBRARY-POSIX 기록):
#   LIBRARY-BASE  — 기본 기점(exe dir)에서 count>=1 + name=minesweeper 존재.
#                   (주의: 이 build에는 minesweeper.jkx가 설치돼 있어 source=jkx가
#                   이긴다 — built-in 단정은 격리 가짜 트리(LIBRARY-ARG)에서 함.)
#   LIBRARY-CAPS  — slot-pack capgauge(스크래치 스크립트)로 능력 선언 jkx를 조립해
#                   설치 → MANI capabilities 원문이 콘솔에 그대로 나오는지 검증.
#                   (이 기기 build/apps의 실제 jkx는 능력 선언이 전부 비어 있어
#                   원문 전승 검증은 합성 jkx가 진실원 — probe_slot_ship 선례.)
#   LIBRARY-ARG   — 임시 가짜 트리(apps/galapp/manifest.json 콘솔 1 + capgauge.jkx)
#                   인수 기점 스캔 — source=console 파싱 + built-in 전폴백(minesweeper
#                   ·tetris, apps-bin 부재라 lf/hx 없음) + count==name 행수 정확.
#   LIBRARY-POSIX — (제거, 컨트롤러 판정 fix r1) Windows pwsh와 WSL pwsh는 별도
#                   툴체인이고 $build/$exe가 Windows 절대경로라, .ps1은 posix 런을
#                   정직하게 소유할 수 없다(WSL 런은 LIBRARY-BASE의 Run-Cli에서
#                   이미 죽는다 — 분기는 도달불능). WSL 검증 — Task 4
#                   wsl_library_boot.sh가 소유 (이 ps1은 Windows 전용).
#
# PowerShell 전달 함정(docs/55 lesson 3, probe_slot_ship 선례): stdout/stderr는
# PSI raw-Arguments + CP949 디코드로 정확히 잡는다. CRT 한국어(title)는 파이프
# 캡처에서 깨질 수 있는 화장품 함정 — **단정은 ASCII 필드(name=/source=/caps=/
# count=/size=)에만 건다**.
$build    = "I:\progwork\JKENGINE\engine\build"
$exe      = Join-Path $build "jkdesktop.exe"
$slotDir  = Join-Path $build "state\scripts"
$appsDir  = Join-Path $build "apps"
$gaugeJs  = Join-Path $slotDir "capgauge.js"
$gaugeJx  = Join-Path $appsDir "capgauge.jkx"
$failures = 0

function Run-Cli([string]$argLine) {
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $exe
    $psi.Arguments = $argLine
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.StandardOutputEncoding = [System.Text.Encoding]::GetEncoding(949)
    $psi.StandardErrorEncoding = [System.Text.Encoding]::GetEncoding(949)
    $psi.CreateNoWindow = $true
    $p = [System.Diagnostics.Process]::Start($psi)
    $out = $p.StandardOutput.ReadToEnd()
    $err = $p.StandardError.ReadToEnd()
    $p.WaitForExit()
    return @{ rc = $p.ExitCode; out = $out; err = $err }
}

# entry 행의 ASCII 원문만 보는 필터 — printf 포맷(name= ... source= ... caps= ...)
# 은 계약이므로 행렬에서 substring 단정한다. \r 제거(CRLF 캡처).
function Get-EntryLines([string]$o) {
    @($o -split "`r?`n" | Where-Object { $_ -like 'name=*' } | ForEach-Object { ($_ -replace "`r$") })
}
function Get-TrailerCount([string]$o) {
    if ($o -match 'count=(\d+) base=') { return [int]$Matches[1] }
    return -1
}

# ---- 0) 재런 격리 — 이전 실패 런의 capgauge 잔존을 미리 소각(재현성) -------------
if (Test-Path $gaugeJx) { Remove-Item $gaugeJx -Force }
if (Test-Path $gaugeJs) { Remove-Item $gaugeJs -Force }

# ---- 1) LIBRARY-BASE — 기본 기점(exe dir) 스캔 -----------------------------------
# 기대: rc=0 + count>=1 + name= 행수와 count 일치(꼬리 계약) + minesweeper 존재.
$base = Run-Cli "library-list"
Write-Output ("--- library-list (no arg) rc=" + $base.rc + " ---")
Write-Output $base.out
$baseLines = Get-EntryLines $base.out
$baseCount = Get-TrailerCount $base.out
$mineHit = @($baseLines | Where-Object { $_ -match 'name=minesweeper ' }).Count
$libBase = (($base.rc -eq 0) -and ($baseCount -gt 0) -and
            ($baseLines.Count -eq $baseCount) -and ($mineHit -eq 1) -and
            ($base.out -match ' base='))
Write-Output ("LIBRARY-BASE: " + (& {
    if ($libBase) { "OK (count=" + $baseCount + " lines=" + $baseLines.Count + " minesweeper=1)" }
    else { "FAIL rc=" + $base.rc + " count=" + $baseCount + " lines=" + $baseLines.Count + " mine=" + $mineHit }
}))
if (-not $libBase) { $failures++ }

# ---- 2) LIBRARY-CAPS — 능력 선언 jkx를 slot-pack으로 조립 → 원문 전승 검증 --------
# 기대 토큰 순서 kCapabilityBindTokens 표 순서(JKScriptHost.cpp): timer < canvas
# — setInterval 1회 + createCanvas 1회 → caps=timer,canvas.
# capgauge라는 이름은 내장·콘솔 어디에도 없는 유니크 키(그림자 없음 — count+1 확정).
$src = @"
// capgauge probe scratch (probe_library_list.ps1 — 도구는 읽기만 하고 실행하지
// 않는다). 능력 원문 전승 검증용 토큰 2개: timer / canvas.
setInterval(function(){}, 100);
var cv = createCanvas({x:0, y:0, w:80, h:30}, "");
"@
$src | Set-Content -Path $gaugeJs -Encoding UTF8
$pack = Run-Cli "slot-pack capgauge"
$capRcOk = (($pack.rc -eq 0) -and ($pack.out -match 'packed') -and (Test-Path $gaugeJx))
$cap = Run-Cli "library-list"
Write-Output ("--- library-list (capgauge installed) rc=" + $cap.rc + " ---")
$capLines = Get-EntryLines $cap.out
$capCount = Get-TrailerCount $cap.out
$gaugeLine = @($capLines | Where-Object { $_ -match 'name=capgauge .*source=jkx .*caps=timer,canvas' })
$libCaps = ($capRcOk -and ($cap.rc -eq 0) -and
            ($capCount -eq ($baseCount + 1)) -and ($capLines.Count -eq $capCount) -and
            ($gaugeLine.Count -eq 1) -and
            ($gaugeLine[0] -match 'size=\d+ .*\.jkx'))
Write-Output ("LIBRARY-CAPS: " + (& {
    if ($libCaps) { "OK (caps=timer,canvas verbatim, count=" + $capCount + ")" }
    else { "FAIL pack=" + $capRcOk + " gaugeHits=" + $gaugeLine.Count + " count=" + $capCount }
}))
if (-not $libCaps) { $failures++ }

# ---- 3) LIBRARY-ARG — 임시 가짜 트리를 인수 기점으로 스캔 -------------------------
# 콘솔 1(galapp: manifest.json name/cmd/desc) + capgauge.jkx 사본 → 콘솔 파싱·
# .jkx 파싱·built-in 폴백(minesweeper+tetris — temp 트리엔 apps-bin이 없으므로
# lf/hx 단말 내장은 절대 안 나온다) 3요소가 하나의 판정안에 담긴다. 기대 count = 4.
$fakeBase = Join-Path ([System.IO.Path]::GetTempPath()) ("jk_lib_probe_" + [guid]::NewGuid().ToString("N").Substring(0, 8))
New-Item -ItemType Directory -Force -Path (Join-Path $fakeBase "apps\galapp") | Out-Null
# manifest.json은 ASCII만 — desc도 ASCII(한국어는 CRT 캡처 깨김 함정).
$cliJson = '{"name":"galapp","cmd":"gal.cmd","desc":"Gallery probe scratch"}' + "`r`n"
Set-Content -Path (Join-Path $fakeBase "apps\galapp\manifest.json") -Value $cliJson -Encoding ASCII
$argExpectCount = 3    # 내장 2 + 콘솔 1
$argJkxName = ""
if (Test-Path $gaugeJx) {
    Copy-Item $gaugeJx (Join-Path $fakeBase "apps\capgauge.jkx")
    $argExpectCount = 4
    $argJkxName = "capgauge"
} elseif (Test-Path (Join-Path $appsDir "imguidemo.jkx")) {
    Copy-Item (Join-Path $appsDir "imguidemo.jkx") (Join-Path $fakeBase "apps\imguidemo.jkx")
    $argExpectCount = 4
    $argJkxName = "imguidemo"
}
$fakeArgs = Run-Cli ('library-list "' + $fakeBase + '"')
Write-Output ("--- library-list (arg fake tree) rc=" + $fakeArgs.rc + " ---")
Write-Output $fakeArgs.out
$fakeLines = Get-EntryLines $fakeArgs.out
$fakeCount = Get-TrailerCount $fakeArgs.out
$galHit = @($fakeLines | Where-Object { $_ -match 'name=galapp .*source=console' }).Count
$jkxHit = @($fakeLines | Where-Object { $_ -match ('name=' + $argJkxName + ' .*source=jkx') }).Count
$biMine = @($fakeLines | Where-Object { $_ -match 'name=minesweeper .*source=builtin' }).Count
$biTetris = @($fakeLines | Where-Object { $_ -match 'name=tetris .*source=builtin' }).Count
$libArg = (($fakeArgs.rc -eq 0) -and
           ($galHit -eq 1) -and ($jkxHit -eq 1) -and ($biMine -eq 1) -and ($biTetris -eq 1) -and
           ($fakeCount -eq $argExpectCount) -and ($fakeLines.Count -eq $argExpectCount) -and
           (-not ($fakeArgs.out -match 'terminal:')))
Write-Output ("LIBRARY-ARG: " + (& {
    if ($libArg) { "OK (expect=$argExpectCount got=$fakeCount galapp=console jkx=" + $argJkxName + " builtins=2)" }
    else { "FAIL rc=" + $fakeArgs.rc + " expect=$argExpectCount got=$fakeCount gal=" + $galHit + " jkx=" + $jkxHit + " mine=" + $biMine + " tetris=" + $biTetris }
}))
if (-not $libArg) { $failures++ }

# ---- WSL 검증 — Task 4 wsl_library_boot.sh가 소유 (이 ps1은 Windows 전용) --------
# 컨트롤러 판정(fix r1): 본 ps1의 posix 분기는 수리가 아니라 제거 — $build/$exe가
# Windows 절대경로여서 WSL 런은 LIBRARY-BASE의 Run-Cli에서 이미 죽고, 도달해도
# Test-Path가 false로 "posix binary missing" FAIL을 골라먹는 도달불능 분기였다
# (약속한 커버리지를 구조적으로 못 낸다). 이 스크립트의 판정안 = BASE/CAPS/ARG
# 3정판 + CLEAN 소각 수령.

# ---- 4) 소각 — scratch jkx + 슬롯 원천 + temp 트리 (사용자 apps/ 정합 유지) --------
if (Test-Path $gaugeJx) { Remove-Item $gaugeJx -Force }
if (Test-Path $gaugeJs) { Remove-Item $gaugeJs -Force }
Remove-Item $fakeBase -Recurse -Force -ErrorAction SilentlyContinue
$clean = ((-not (Test-Path $gaugeJx)) -and (-not (Test-Path $gaugeJs)) -and
          (-not (Test-Path $fakeBase)))
Write-Output ("LIBRARY-CLEAN: " + (& { if ($clean) { "OK" } else { "FAIL — 잔존" } }))
if (-not $clean) { $failures++ }

Write-Output ("--- ls cleanup receipt: apps (capgauge.*) ---")
Get-ChildItem -Path $appsDir -Filter "capgauge*" -ErrorAction SilentlyContinue |
    Select-Object -ExpandProperty Name
Write-Output ("--- ls cleanup receipt: state\scripts (capgauge.*) ---")
Get-ChildItem -Path $slotDir -Filter "capgauge*" -ErrorAction SilentlyContinue |
    Select-Object -ExpandProperty Name

if ($failures -gt 0) {
    Write-Output ("FAILED: " + $failures + " verdict(s) failed")
    exit 1
}
Write-Output "done"
exit 0
