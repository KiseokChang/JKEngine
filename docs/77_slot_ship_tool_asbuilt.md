# docs/77 — 슬롯 출하 도구(slot-pack) as-built (2026-10-05)

docs/74 §5 "단 2 출하 라인 개통" 경로의 마지막 고리. 세션 원장: 16088f8(스펙)
→ 773ac9b(플랜) → ae11718(분석기) → ac302ef(시딩 전환) → 46c2e90(slot-pack
서브커맨드) → 36b4e42(템플릿 capabilities 주석) → 본 커밋(live probe +
as-built). 스펙: docs/superpowers/specs/2026-10-05-slot-ship-tool-design.md,
플랜: docs/superpowers/plans/2026-10-05-slot-ship-tool.md.
선대 as-built(게이트 어원): docs/76.

## 1. 결정 (스펙 §1 룰링 — 구속력 있는 봉인)

**출하 = 워크숍 모드 + 파묻힌 SCRI 시딩.** 갈림길에 있던 양안과 이유:

- **순수 SCRI 모드**(MANI `scriptfile=` 없음)를 출하에 쓰면 — 부팅이 DLL 옆
  파묻힌 `.app.js`만 본다(SideFilePath ".app.js"). 외부 진실원이 생기지
  않아 docs/74의 전체 취지("다른 환경에서 불리는 슬롯 앱"이야말로 게이트
  대상)가 빠진다: 능력 게이트(`EnableCapabilities` 미호출 = `gateActive_`
  거짓)와 배지가 작동하지 않는다. **기각.**
- **워크숍 모드를 기존 시딩(템플릿) 그대로 쓰면** — 수신 기기에 외부 파일이
  없을 때 출하본이 아니라 `kTemplateScript`가 시딩된다 — 슬롯 내용물
  (저자의 실앱)이 사라지고 저자 본인도 모른다. **기각.**
- **채택**: 출하 MANI는 `scriptfile=state/scripts/<slot>.js`를 싣는다(게이트·
  배지·진실원 문화 수신 기기에서도 산다) + `jk_app_run_client` 워크숍 분기의
  시딩을 확장한다(§3): 외부 부재 시 파묻힌 SCRI가 있으면 **그 원문으로
  시딩**, 없으면 템플릿(현행 회귀). 외부가 이미 있으면 파묻힌 SCRI는 무시
  (외부가 진실원 — 현행 불변).

파묻힌 SCRI는 배선상 항상 추출된다(main.cpp — SCRI 엔트리가 있으면 무조건
DLL 옆 `.app.js`로 떨어진다) — 그래서 "출하 팩이 scriptfile= 과 SCRI를
함께 싣는다"만 문이고, 시딩은 추출 원장 위의 순수 판정이 된다.

## 2. 도구 — `slot-pack` (스펙 §2)

```
./jkdesktop.exe slot-pack <slot>          # → <exeDir>/apps/<slot>.jkx
./jkdesktop.exe slot-pack <slot> <out>    # out 경로 지정 (선택)
```

- **매니페스트 조립 = 8행 캐노니컬 원문**(`jk::SlotShipManifestText`,
  JKJkxFile.cpp — `1k-a`가 전체 문자열 단언):

  ```
  name=<slot>
  title=<slot>
  width=360
  height=280
  module=jkapp_script.dll
  script=app.js
  scriptfile=state/scripts/<slot>.js
  capabilities=<used tokens>
  ```

  마지막 행 뒤 개행 1개 — 신작 원문이므로 workshop MANI의 "마지막 행
  개행 없음" 패턴 따르지 않는다(Parse는 last-wins 행 단위라 무충돌,
  1k-a 원문 단언).
- **3엔트리 팩** — `JkxFile::Write` 레이아웃(pack_workshop.ps1과 동일 구조,
  docs/60 §3): MANI `manifest.txt`(신작 원문) + MODL `jkapp_script.dll`
  (현재 빌드본) + SCRI `app.js`(슬롯 원문).
- **사용량 자동 분석 = 표 단일 출처**: `jk::JKScriptHost::
  CapabilityTokensForScript(source)` — 정적 표 `kCapabilityBindTokens`
  (JKScriptHost.cpp)만이 name→token을 안다. 도구가 표를 복제하지 않는다 —
  표가 흔들리면 출하 선언도 같이 흔들린다(설계 의도; `1h-f`가 표↔
  `BoundNames` 전수 일치를 핀다). 어휘 경계 매치 — 좌우가
  `[A-Za-z0-9_$]`가 아니면 경계(`mysetInterval` 미적중, `setInterval()`
  적중). 무조건 허용 3(log/assert/assertEq, token=nullptr)과 리저브
  network는 출하 선언에 들어가지 않는다.
- **인쇄 형식**: stdout `packed <outPath> (caps=<조인>)` 1행 + stderr
  빌드 규율 경고 1행(stale DLL 함정, docs/60:334 — 재빌드 없이 팩하면
  오래된 DLL이 파묻힌다). 슬롯 원천 부재 시 `slot-pack: no slot source`
  + 오류 1 종료 — 출하는 진실원이 있는 것만 팩한다(시딩-재시도 경로
  없음). 도구는 슬롯 파일을 읽기만 한다(쓰기·이력 접촉 금지).
- **이름 충돌 없음**: `jkctl pack`(콘솔앱 zip 배포용, tools/jkctl)과는
  다른 도구 — 건드리지 않는다.

## 3. 엔진 변경 — 시딩 출처 전환 (`jk::WorkshopSeedScript`, 스펙 §4)

`jk_app_run_client` 워크숍 분기의 `ReadTextFile(path)` 실패 자리가 이전에는
곧바로 템플릿 시딩이었다 — 그 자리를 정책 헬퍼 하나로 봉합했다
(include/apps/JKWorkshopSeed.h — 변경은 이 한 곳, 파스·게이트·배지·추출
배선 불변):

| 반환 | 의미 |
|---|---|
| `0` | 시딩 불요 — 외부 진실원이 이미 있다(외부가 이긴다, 현행 불변·SCRI 무시) |
| `1` | **shipped 시딩** — 외부 부재+DLL 옆 `.app.js` 원문으로 시딩 |
| `2` | template 시딩 — 외부·SCRI 둘 다 부재(현행 회귀) |
| `-1` | 오류(`error` 채움 — 쓰기 실패 등; 호출자가 stderr 1행+1 종료) |

- `SetAgentAppName(meta->name)` 그대로 — 수신 기기의 에이전트 도구 등록
  이름 = `<slot>`(app_tool 게이트 식별 유지).
- 워크숍 템플릿에 capabilities 선언 안내 주석 심기(36b4e42 — 스펙 §8,
  "출하 도구 작업 때 같이" 예약 소각): 첫 저자가 미선언 API를 쓰면
  `capability '<tok>' not declared in MANI`를 만나기 전에 무슨 일인지
  원문이 말해 준다.

## 4. 셀프테스트 맵 (RunAppSelfTest — `./build/jkdesktop.exe test`, 전부 녹색)

| 스펙 §5 | 구현 |
|---|---|
| 1 사용량 분석기 | 1h — (a) 조합 원문 선언 셋 정확 일치·표 순서 보존 (b) `mysetInterval` 미적중·`setInterval()` 적중 (c) 문자열·주석 폴스포짓 = 오버 방향만 (d) network·미지 이름 무시 (e) `-` 접미 경계 (f) 정적 표=30·표↔BoundNames 전수 일치 |
| 2 출하 MANI 원문 | 1k — 캐노니컬 원문 전체 비교 + Parse 통과·선언 원문 재검 + 빈 tokens=능력 없음(fail-closed) |
| 3 시딩 출처 전환 | 1j — (a) SCRI 존재+외부 부재 → 외부=SCRI 원문 (b) SCRI 부재 → TEMPLATE 회귀 (c) 외부 존재 → 외부 무변(SCRI 무시) (d) 쓰기 실패=-1+오류 |
| 4 회귀 | 기존 script demo·workshop·1g 게이트 셀프테스트 전부 녹색 유지(마지막 실측: `AppSelfTest: 0 failure(s)`) |

## 5. probe receipt (실측 원문 — engine/tools/probes/probe_slot_ship.ps1)

scratch 슬롯(shipscratch) 격리 — 사용자 슬롯(bang-gu/counter/fartcar/myapp)
및 그 `.history`는 읽지도 쓰지도 않았다(소각 후 ls 실측이 원장: 아래).
서버 불요 — slot-pack/jkx-list는 CLI 서브커맨드이므로 라이브 스택이 살아
있는 가운데 실행했다(접촉도 잠금도 없음).

```
SHIP-SCRATCH-SEED: OK
--- slot-pack stdout ---
[theme] preset 'dark' from I:\progwork\JKENGINE\engine\build\theme.json
packed I:\progwork\JKENGINE\engine\build\apps\shipscratch.jkx (caps=timer,input,canvas)

--- slot-pack stderr ---
[slot-pack] 빌드 규율: 팩 직전 jkapp_script.dll이 현재 소스인지 — stale DLL은 런타임 'is not defined'로만 발현 (docs/60:334)

--- slot-pack rc=0 ---
SHIP-PACK: OK
--- jkx-list ---
[theme] preset 'dark' from I:\progwork\JKENGINE\engine\build\theme.json
I:\progwork\JKENGINE\engine\build\apps\shipscratch.jkx: version=1 codec=0 entries=3
  manifest: name=shipscratch title=shipscratch module=jkapp_script.dll script=app.js scriptfile=state/scripts/shipscratch.js capabilities=timer,input,canvas 360x280
  MANI manifest.txt                            166 bytes @ 0x00000194
  MODL jkapp_script.dll                    8820169 bytes @ 0x0000023A
  SCRI app.js                                  272 bytes @ 0x00869803

SHIP-LIST: OK
SHIP-CAPS: OK
SHIP-CLEAN: OK
--- ls cleanup receipt: state\scripts ---
.history
fartcar.js
bang-gu.js
jkdesktop_launch.log
counter.js
myapp.js
.current_workshop
--- ls cleanup receipt: apps (shipscratch.*) ---
done
```

- **빌드 규율 경고 행은 캡처 파이프라인에서 깨져 온다**(CRT stdout이 콘솔
  코드페이지로 쓰는 한국어 행을 파이프에서 재해석) — receipt의 그 행은
  main.cpp 원문 상수를 옮겨 적은 것이고, 실측 인쇄 행의 내용물은 동일
  (행 개수·앞뒤 문맥으로 동일성 확인 — 캡처 깨김은 표시 결함, 내용물
  결함 아님). probe는 PS5.1 문서 관습대로 UTF-8 BOM 선두를 요구한다
  (BOM 없으면 PS가 ANSI로 읽어 한국어 주석이 파서 오류를 낸다 — 실측).
- **기댓값 순서 = 표 순서**: 자동 분석은 `kCapabilityBindTokens` 표를
  순회하며 토큰을 축적한다 — timer 행(setInterval/clearInterval)이 input
  행(injectMouse/injectKey/click)보다 앞서고 canvas 행
  (createCanvas/canvas*)이 둘 다보다 뒤서므로 `timer,input,canvas`.
  플랜 T5가 예고했던 `timer,canvas,input`은 **표 순서 미각오**였다
  (§8 교정 기록).
- caps grep은 **부분 문자열만** — `capabilities=timer,input,canvas` 뒤에
  ` 360x280`이 같은 행에 이어 인쇄된다(RunJkxList 인쇄 체인: 개행은 width
  뒤 한 번 — 기존 행 포맷 불변) — 행 끝 `$` 앵커를 쓰면 MISS가 난다
  (T3 스모크에서 예고된 그 함정, probe가 정평 회피).

## 6. 트레이드오크 — 수신 기기 동명 슬롯 (스펙 §1-3 원장)

수신 기기 상태에 이미 같은 슬롯 이름 파일(`state/scripts/<slot>.js`)이
있으면 **수신 기기의 그 파일이 이긴다** — 이름 충돌 = 수신자 진실원 존중
(docs/67 사훈 "파일=진실원"). `WorkshopSeedScript` 반환 0이 이 판정의
코드 정합이다: 출하 팩이 와도, 수신 기기에 저자 이외의 누군가(또는 저자의
이전 자신)의 진실원이 살아 있으면 그것이 계속 진실원이다. 개인 과제
범위에서 충분 — 충돌 정책(대화/덮어쓰기)은 나눔 확대 시 2단계(docs/74 §2
B·C 서명 도입과 같은 스케일에서 결정).

## 7. 잔여

- **갤러리 = 다음 문**(별도 스펙) — docs/67:144 "설치 앱 목록은 로컬
  라이브러리만" 대전제 + apps/*.jkx MODL 스캔 선례(engine/tools/jkctl/
  main.cpp:686). 출하 도구가 이 스펙으로 완료된 뒤 시작 — 이제 조건이
  만족됐다.
- **T1-T3 deferred minors 8건 원장**(최종 리뷰에서 전부 keep —
  .superpowers/sdd/2026-10-05-slot-ship-tool/progress.md):
  T1 ① `IsIdentCharW` 접미사 W가 UTF-16을 암시(실은 UTF-8 바이트 분류기)
  ② `isdigit‖isalpha` 로캘 종속(이론상 언더 — fail-closed가 흡수) ·
  T2 ③ JKWorkshopSeed.h/.cpp 끝 개행 누락 ④ 1j seedErr 미초기화(clear
  1줄 권고) ⑤ 1j 무의미 bytes.clear() · T3 ⑥ SlotShipManifestText 임의
  토큰 dedup 없음(단독 호출자는 분석기 산출 — 계약 주석 있음) ⑦ caps
  조인 5행 중복(조립기와 인쇄) ⑧ out override 부모 디렉터리 미생성
  (제재된 편차). T4=0.
- `jkctl install`·폰 게이트웨이·2KB 스키마 한도·MANI 충돌 정책 — 불변
  (스펙 §7).

## 8. 스펙 갱신기록

- **플랜 T5 기댓값 mis-order 교정**: 플랜(step 1-3)과 브리프가 선언
  예상을 `timer,canvas,input`으로 썼다 — 자동 분석은 정적 표 순서
  (§5 receipt)이므로 실측은 `timer,input,canvas`. 본 문서를 단일
  교정 원장으로 삼고 probe·인쇄 원문 전부 후자로 실측했다. 도구 출력
  (`caps=`)·jkx-list 인쇄·셀프테스트는 표 순서 계약 1개로 수렴 —
  스펙 본문(§3 "선언 순서 = 표 순서")은 옳은 채, 교정은 플랜 쪽
  예상문만 — 스펙 머리에 이 이행 기록 1행을 심어 두었다.

## 9. docs/74 §5 — 마지막 고리 완결

docs/74 §5의 경로: 결정 → 설계 스펙 → 플랜 → 구현(토큰 표+게이트+배지+
셀프테스트) → jkctl pack 뿌리 수리 → 슬롯→.jkx 출하 도구 → 갤러리. 이
라인에서 **출하 도구까지가 완결됐다**(위 원장 체인 — 갤러리만 다음 문으로
남는다, §7). 사용자 눈확인 대기 항으로 남는 것: 수신 기기에서 출하 .jkx를
`--jkx`로 띄웠을 때의 부팅 모양(워크숍 모드+shipped 시딩+능력 배지) —
폰 실기기 배포(docs/76 §9 마이그레이션) 때 확인하는 것으로 원장화한다;
개발 기기에서의 단독 눈확인이 필요하면 출하 팩 하나를 남겨 두어야 하므로
probe는 소각했다(원장 receipt가 유일 증거).