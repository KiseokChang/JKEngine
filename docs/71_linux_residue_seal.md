# docs/71 — 리눅스 잔여 봉합 플랜 F (as-built)

> 플랜: docs/superpowers/plans/2026-10-05-linux-residue-seal.md · 커밋 원장
> 147e980 → e9433a2 · 2026-10-05 · 상위: docs/70 §6 잔여 9건 중 5건 소각
> (#1·#2·#3·#4-fmt·#8) — SDD(subagent-driven): 구현 5 태스크 + 라이더 웨이브 +
> 게이트 · 최종리뷰 오판 대기 포함 본 문서가 as-built.

## 0. 성과 요약

**리눅스 서버가 이제 명령을 실행한다.** WSL2 서버에서 agent 도구로
`launch_app "terminal:mktemp"` → jk::process::Spawn posix leg가 실제 자식을
fork/exec해 pty 터미널이 mktemp를 실행했고, `terminal_exec "echo
pty-smoke-v2"` → JKConPtyBridge 실물 pty 왕복이 `ok:true`+출력 캡처로 돌아왔다
(스모크 v2, §4). docs/70 §6의 "미봉쇄 3종"(terminal_exec 스텁·spawn 스텁·셸 접두
cmd.exe 리터럴)을 전부 플랫폼 중립으로 전환. 부수로 win32 잠재 결함 1건
(FmtStamp 역전 boolean — 스탬프 복원, 유일 승인 관측 편차 #1)과 수기 백슬래치
경로 합성 ~64곳을 소각했다.

```text
윈도우 공식 9항목 게이트  GREEN (1회차 contiguous, @e9433a2)
WSL 빌드+posix_selftest   GREEN (ninja RC=0, 0 failure(s), 케이스 10·11 포함)
WSLg 스모크 v2            GREEN (4 observables, §4)
diff 크기                 12 파일, +307/-137 (win32 동작 변화 = 승인 편차 2건 — F4 스탬프 복원 + F1 '/' 관측 표면)
```

## 1. docs/70 §6 잔여 → 소각 매핑

| docs/70 §6 | 플랜 F 태스크 | 커밋 | 소각 실측 | 남은 것 |
|---|---|---|---|---|
| #1 terminal_exec posix 배선 | F3 | 33cb501 | jkagentd RunTerminalExec 몸통 플랫폼 통일 — win32 ConPTY leg 유지, posix leg가 JKConPtyBridge 실물 pty(openpty+setsid+TIOCSCTTY) 구동. 서버리스 WSL 스모크: 터미널_exec을 서버 연결 없이 로컬 pty로 `ok:true` 왕복 | 없음(이 항목 완결) |
| #2 posix LLM 셸 접두 | F2 | 292b5d0 | stub 분기 플랫폼 분할 — win32는 `cmd.exe /c echo {...}` 원문 유지, posix는 `echo '{...}'`(sh). spawn 사이트도 분할(win32 `cmd.exe /c `+BuildEngineCmd 불변 / posix bare BuildEngineCmd). posix_selftest 케이스 10이 stub JSON 왕복 실측 | 케이스 10 stub 리터럴 복제(공용화 후보 — 리뷰 MEDIUM) |
| #3 수기 백슬래치 합성 | F1+라이더 | 325de07+e9433a2 | JKWindowServer ~30곳·jktriggers 14곳·WorkshopStore 7곳·JKDesktopShell(EnsureTrustRecord 게이트 해제+filesystem)·jkctl 16곳(+std::replace 방향 전환 2곳)·jkbridge 3곳·JKLlmEngine:36 — 리터럴 `"\\"`→`'/'` 전수·esc/validator/find_last_of 무손실·ec 오버로드 전용 | **남음**: ScanJkxApps/ScanConsoleApps posix 갭(작은 데스크톱 아이콘 소실 — fail-quiet 설계 선례), JKDesktopShell:495 `apps\\"` 아이콘 스캔 1쌍(같은 갭 소속), win32 전용 스택 냉동(ClientBrowserApp·CrashHandler) |
| #4 fmt localtime_s 역전 | F4 | 4e18b08 | ClientFilesApp/ClientNotesApp `!LocaltimeS(...)`→`LocaltimeS(...) != 0` — errno_t 0=성공 계약(win32 ::localtime_s 위임, posix localtime_r 랩+zero-errno→1 폴백 정규화). posix_selftest 케이스 11: rc==0·tm_year≥126·stamp 11자·구분자 b[2]=='-' b[5]==' ' 락 | win32 잠재 결함이었으므로 **윈도우 관측 변화 = 스탬프 복원**(승인 편차 #1 — 사용자 눈확인: notes/files 허브 스탬프 "mm-dd hh:mm" 렌더). 별도 라인 "BuildEngineCmd win32 백슬래시 누락"은 결함 후보로 유지(미취급) |
| #8 셸리스 리눅스 서버 스폰 | F5+라이더 | f4fe20a+e9433a2 | throttle 블록 #ifdef 밖 verbatim 공용화 + posix SpawnProcess leg(단일 쿼트 exe·workingDir·spawnedClients_ 저장) + ComposeClientSpawnArgs 추상(terminal:/filedlg:/plain 합성 플랫폼 중립화 — win32 합성 텍스트 원문 불변, win32 실측 diff = +appName 1토큰만) + clientHostExe_ 플랫폼 기본값("jkdesktop"/"jkdesktop.exe") + posix SpawnClient leg(fromJkx 프리게이트). 실서버 스모크: launch_app "terminal:mktemp" → 스폰+throttle 26ms+jkx 프리게이트("jkx spawn unsupported on posix (v1)") | 서버 구동 이후의 셸 진입 경로 3종 = **F7 신규 잔차로 이관**(§5, docs/70 §6 #8의 후속) |
| #5 job/stdin/좀비/phantom | — | — | **범위 밖**(docs/69 §4 소비자 접봉) | 유효 그대로 — 후속 판정 |
| #6 vplayer 실측·Termux | — | — | **범위 밖** | 유효 — 폰 실기기 세션 소관 |
| #7 selftest 강화 | 부분 | 292b5d0+4e18b08 | 케이스 10·11 스핀오프로 일부 충족(12케이스) | RecvAll 패턴·Accept 무한블록 방어 잔존 |
| #9 관측 편차 기록 | — | — | **범위 밖**(이미 코드 주석+문서 완료) | 유지 |

## 2. 태스크 실측 기록 (F1→F5+라이더, 레저 요약)

**F1 (325de07, opus APPROVE).** 원칙: 리터럴 `"\\"`→`'/'`는 win32 관측 동일
(Win32 파일 API가 '/' 수용). 소각 판정 기준 — (a) 파일 API로 흐르는 합성만,
(b) JsonEsc/escape 합성 금지, (c) ValidFilePath 검증기(:2953)·find_last_of
("\\/")·명령줄 인용 종료(:8061)·`Local\` 커널 개체명 변환 금지. 스윕이 플랜의
검증 사이트 목록을 초과해 ~47+ 훙크로 확장(플랜 목록이 `\` 그립으로
구조적으로 놓치는 snprintf 포맷 합성 2곳 포함) — 리뷰 전수 대조 실증. posix
서버의 clientHostExe_ 경로도 플랫폼 기본값화. **남은 MEDIUM 관측 편차**(승인
룰링): F6 게이트가 reply/HostLog 표면의 구분자를 관측하면 win32와 `\`≠`/`
차이가 보인다 — 문자열 비교 게이트는 `'/'` 기준 유지. 실측 구체면(최종리뷰
확정): capture_window/capture_region JSON reply 경로(:5042·:5140,
`StateDir() + "/screenshots"/…`)와 jkctl init/promote stderr(:419·457·529·696)
가 win32에서 혼합 구분자로 인쇄 — **승인 편차 #2**(F1 룰링 ① 범주).

**F2 (292b5d0, opus APPROVE).** 셸 추상의 v1: "셸 접두"는 셸 실행기 계약이
아니라 **stub 실행 경로의 접두 리터럴**만 처리 — claude/ollama 분기는
애초 접두 없이(sh에서 직접 실행). win32 분기는 바이트 동일 유지
(`cmd.exe /c ` 그대로). posix_selftest 케이스 10: stub 명령을 실제
posix 어댑터(Spawn→pipe drain)로 구동해 JSON 왕복 — EOL 편차(LF)가
AgentJson/JS_ParseJSON 후행 공백 수용으로 무해인 것을 실증. 리뷰 MEDIUM:
케이스 10 stub 리터럴이 복제(JKLlmEngine win32 분기 2곳과 동일 문자열 ×3)
— 공용 상수로 후속 공용화 권고.

**F3 (33cb501, sonnet APPROVE, 무결점).** win32 ConPTY leg와 posix pty leg가
같은 몸통을 공유 — Sleep(30)/(50) → std::this_thread::sleep_for 단 두 콜,
`#else unsupported_platform` 스텁 소멸. `.receipts.jsonl`의 역사적
unsupported_platform 기록은 빌드 아티팩트(코드 0건). 서버리스 WSL
stdio `tools/call terminal_exec` → `ok:true, ended:exited,
output:"pty-smoke-v2<CR><LF>"` 실측.

**F4 (4e18b08, sonnet APPROVE).** `errno_t` 0=성공 계약이며 JKCrtShim posix
래퍼는 `saved != 0 ? saved : 1`로 성공을 정규화하므로 역전 boolean은
양플랫폼 결함 — 취급은 win32 관측 편차(스탬프가 "빈 칸"→"렌더")로 실현될
뿐. 역전 전수 재확인: ClientNotifyApp/JOClock은 무시-설계(역전 아님),
FopenS 4곳은 `!=0` 이미 옳음. 지각/경계 플레이크 없음.

**F5 (f4fe20a → 라이더 e9433a2, opus APPROVED WITH RIDER).** 리뷰가 win32
바이트-모션 실증을 요구해 실측으로 승인 — win32 `--client` 인수 diff는
`--client <app>`(1토큰 보존) 하나뿐. posix leg 실서버 스모크 GREEN(§4).
라이더 웨이브: (1) F5 헬퍼 주석 3건 무결성 — "Returns false"와 void 시그니처
모순 제거, plain route `argsOut = std::string("--client ") + appName;` 원복
(:8200), 헤더/플랫폼 코멘트 정직화. (2) F1 판정의 posix-빌드 잔여 소각:
*jkctl*(13곳 + `std::replace '\\','/'` 방향 전환 :651·:800 — 팩 서브커맨드에서
언팩 서브디렉터리가 리터럴 백슬래치 평면 파일이 되는 posix 실물 파손, win32
CRT/filesystem은 '/' 수용하므로 win32 무변동)·*jkbridge*(3곳 — 토큰/리포트
fopen 흐름)·*JKLlmEngine:36*(백슬래치 1건). 리뷰 opus APPROVE.

**선계약 기록(R-D2):** posix SpawnClient의 commandLine은 `sh -c` 해석 대상
(JKProcess_posix.cpp:156 설계) — terminal: 계열과 **filedlg:<json>**에 셸
메타문자(`'`/`$`/백틱)가 섞이면 해석된다(:8194 posix leg 적용 후).
win32는 CreateProcess 리터럴. 관측 차이이자 보안 등가(win32 인용 계약도
정책 게이트 선행) — docs/71 §5에 ledger로 영속.

## 3. 게이트 (2026-10-05, @e9433a2)

**(1) Windows 공식 9항목 ×1 연속 GREEN:** ①ninja -C build -j3 RC=0(up-to-date)
②AppSelfTest 0 failure(s) ③hangul 44/44 ④app_tools ALL PASS ⑤jkbridge PASS
⑥events 8/8 ⑦semantic ALL PASS ⑧agentd list_windows ok:true(빈 — 윈도우
스택 무상태) ⑨HTTP root 200+health ok. 이후 F6 리뷰 이슈 없음 — 플랜 F는
게이트 1회차 요건 충족.

**(2) WSL 전체 빌드+셀프테스트:** CMakeLists diff since a16d4fc = 빈(재구성
불요) → ninja RC=0 → posix_selftest **0 failure(s)** — 케이스 10(stub sh
왕복)+11(LocaltimeS errno_t 계약: stamp="10-05 15:25", 구분자 b[2]/b[5]
관측) 포함 11 케이스.

**(3) WSLg 스모크 v2** — 다음 절. 종료는 pkill(셸리스 서버이므로).

**(4) 최종리뷰 (whole-branch, opus, 147e980..e9433a2 패키지 + docs-only
이행 5a8aef5):** VERDICT **APPROVED WITH RIDER**(문서 정직성 교정 — 코드 변경
없음). 핵심 무결성 전수 실증: 보호 사이트(`Local\`/JsonEsc/ValidFilePath/
find_last_of/:8061 인용 종료) 전부 무손대·ComposeClientSpawnArgs win32 원문
바이트 동일·posix leg 부가 전용·ec 오버로드 전용·트레일러 6커밋. 라이더
반영: §0·§2 승인 편차 2건 정직화·케이스 수 11·§1 #8 트리킬 표현 축소·§5
 잔차 2건 추가(launch_chat 127 거짓 성공·jkx 인수 폴드)·R-D2 filedlg 포함.

## 4. WSLg 스모크 v2 (engine/tmp/f6_smoke.sh 실측 전사)

서버 기동 (`env DISPLAY=:0 ./jkdesktop --server`, WSL Ubuntu-24.04), 그 후
jkagentd stdio로 3 도구 회선을 하나의 세션에 주입:

```text
O1  소켓      srwxr-xr-x /tmp/JKWindowServerPipe.sock — 존재
O2  스폰      launch_app {"app":"terminal:mktemp"} → {"ok":true}
              서버 로그: "JKWindowServer: spawned jkdesktop terminal --shell mktemp"
              자식 실측: /tmp/tmp.agRSw6a9Zx 생성 (pty 셀의 자기 명령 실행)
O3  pty 왕복  terminal_exec {"command":"echo pty-smoke-v2"}
              → {"ok":true,"ended":"exited","output":"pty-smoke-v2\r\n"}
              (\r = pty 개행 그대로 — win32 ConPTY와 동일 관측)
O4  state     bare --server로 trust.json 재생성 안 됨 — 이 유일 F1 실물
              증명은 라이더 웨이브 jktriggers 팩 런(posix 빌드, 15:19)이
              state/trust.json에 기록한 source:pack 레코드 3건
              (trig_build/idle/crash). EnsureTrustRecord의 진입자는 데스크톱
              셸 콘솔 스캔(:500)이지 서버가 아님 — plan F6 step3 항목4의
              "notes/files 스폰 후 trust 관측"은 (a) writer가 서버가 아니고
              (b) posix 앱 스폰은 F7 .dll 프로브로 막혀 있어 **불요·불능 —
              취소 룰링**(레저 기록).
O5  정리      pkill 정리 완료 — list_windows {"ok":true,"windows":[]} (빈이
              정상 — taskbar 자동 스폰은 F7)
```

## 5. F7 신규 잔차 (쉘 진입 경로 — 다음 봉합 후보)

F5에서 posix spawn leg는 개통됐지만, 그 leg에 도달하는 **셸 경로 3종**이 아직
win32 전용:

1. **`launch_app` plain route의 존재 프로브** — agent 핸들러가
   `exeDir + "/jkapp_" + app + ".dll"`만 확인(posix는 .so) → plain 앱(예:
   "minesweeper")은 posix에서 거부. **`.so` 접미 분기 1줄이 우선 봉합 후보**
2. **posix `--client` 직접 기동 route** — main.cpp:3339 "Windows-only in
   this prototype" → 수동 클라이언트 기동·taskbar 자동 스폰(:516, StartAcceptor
   `#ifdef _WIN32`+`jkapp_taskbar.dll` 프로브) 모두 이 관문 아래
3. **jkx 핸들러가 SpawnClient false를 무시**(:4295) — posix jkx 거부가
   agent에 `ok:true`로 보임(정직화 후보)
4. **launch_chat 하드코드**(:5915) — `SpawnProcess("jkchat.exe", "")`
   (최종리뷰 신규 발견): posix에서 `dir/jkchat.exe`(ELF 없음) → sh exit
   127인데 도구는 여전히 `{"ok":true}` — posix 거짓 성공 경로.
   `.exe` 접미 플랫폼화 필요
5. **jkx 인수 정규기의 `'/'→'\'` 폴드**(:4262-4277) — 인수 텍스트 그대로가
   옳아 무손대, 단 posix에서 슬래시 포함 jkx 인수가 백슬래치 이름 프로브
   미스 — ScanJkxApps 갭과 같은 계열(최종리뷰 신규 발견)

이 5종이 봉합되면 list_windows windows=[] 관측이 유효 앱 스폰으로 전환된다.
그때까지 WSLg 스모크의 windows 비었음은 예상 상태로 유지.

## 6. 커밋 원장 (이 플랜, BASE 147e980→)

| 커밋 | 내용 |
|---|---|
| 147e980 | 플랜 F 문서 (docs/superpowers/plans/2026-10-05-linux-residue-seal.md) |
| 325de07 | F1 수기 백슬래치 전수 소각 (JKWindowServer 30·jktriggers 14·WorkshopStore 7·JKDesktopShell) |
| 292b5d0 | F2 셸 접두 추상+posix_selftest 케이스 10 |
| 33cb501 | F3 terminal_exec posix pty 배선 |
| 4e18b08 | F4 FmtStamp 역전 boolean 픽스+케이스 11 |
| f4fe20a | F5 posix 서버 스폰 개통 (SpawnProcess/SpawnClient+throttle 공용화) |
| e9433a2 | 라이더 웨이브 (F5 주석 무결성+plain 원복; F1 판정 잔여 jkctl·jkbridge·JKLlmEngine:36) |
| (이 문서) | F6 as-built |

## 7. 환경 교훈 (레저 → 기억)

- WSL에서 `pkill -f jkdesktop`은 **자기 명령줄을 매치해 셸을 자살시킨다**
  (exit 15, 스모크 스크립트 사망) — `[j]kdesktop` 패턴 필수.
- WSL 데몬은 발사된 sh 세션과 함께 소멸 — 서버는 `(env DISPLAY=:0
  ./jkdesktop --server >log 2>&1 &)` 서브셸 방출 + 6s 대기 후 관측.
- posix_selftest 케이스 번호 연속성: 케이스 9(socket fold) 다음이
  10(stub 왕복)·11(FmtStamp 계약) — 케이스 12부터 신규 자리.
- trust.json 소비처 간 '/' 일치는 F1·라이더로 전 소각 — 왕복 성립 유지
  원칙 자체는 docs/70 §6 #3에서 유지.