# docs/76 — 워크숍 능력 게이트 as-built (2026-10-05)

docs/74 결정 이행의 구현 기록. 세션 원장: 0728a25(스펙) → 893585b(플랜) →
f4d05d7(호스트 게이트) → 616b8d1(MANI 필드) → 7361067(배선+배지) →
e78c0f5(MANI 선언+라이브 실측 probe). 스펙:
docs/superpowers/specs/2026-10-05-workshop-capability-gate-design.md,
플랜: docs/superpowers/plans/2026-10-05-workshop-capability-gate.md

## 1. 결정 (docs/74, 사용자 확정 2026-10-05 — 구속력 있는 원장)

1. **모델 = A 능력 선언형** — MANI가 선언한 능력만 스크립트가 쓴다.
2. **fail-closed = 미선언 호출 차단** — 선언 없이 게이트 대상 API를 부르면
   차단 + 정직 에러.
3. **적용 범위 = 워크숍 스크립트 앱만** — `WorkshopScriptApp`
   (MANI `scriptfile=` 판정 참인 앱, JKAppModule_script.cpp). 순수 SCRI
   (`ClientScriptApp`)·내장 네이티브 앱은 게이트 밖 — `EnableCapabilities`
   미호출 = `gateActive_` 거짓 = 게이트 완전 비활성(기존 셀프테스트 무영향).

사용자 근거: "아직은 개인 과제입니다" — 나눔 범위가 좁은 지금은 진위 보장(서명)
보다 능력 가시성이 먼저다. 서명은 나눔 확대 시 2단계(docs/74 §2 B, 접미
확장: 선언을 서명 본문에 넣는 결합 — 선언형 데이터 구조를 그대로 살린다).

## 2. 능력 토큰 표 (as-built 최종 — GateCap 삽입 전수 실측, JKScriptHost.cpp)

| 토큰 | 바인딩 | GateCap 삽입 |
|---|---|---|
| `widget` | messageBox, createButton, createLabel, createEdit, setText, getText, createDialog, dialogAddLabel, dialogAddEdit, dialogAddButton, dialogShow, dialogClose | 12 |
| `timer` | setInterval, clearInterval | 2 |
| `canvas` | createCanvas, canvasClear, canvasRect, canvasPixel, canvasLine, canvasCircle, canvasText | 7 |
| `agent` | declareCursor | 1 |
| `fs` | readConfig | 1 |
| `input` | injectMouse, injectKey, click | 3 |
| `uiauto` | findControl | 1 |
| `network` | (바인딩 없음 — 리저브) | 0 (삽입 대상 없음 — 미래 확장 자리) |
| **합계** | | **27** |

- **무조건 허용(게이트 밖)**: `log`, `assert`, `assertEq` — 진단·테스트
  전용, 환경 부수효과 없음. 선언 문화를 조작으로 만들지 않기 위해 "선언=능력"
  대상에서 제외. jk.d.ts 기준 전체 바인딩 30개 = 게이트 27 + 무조건 허용 3,
  정확히 1:1 대응(전수 삽입 누락 없음).
- d.ts 갱신은 **주석만**(v8 게이트 계약 블록, 스펙 §4) — 신규 바인딩 없음.
- `agent` 게이트는 이층이 아니다: declareCursor의 서버 유입 경로
  (move/read/act)는 서버 app_tool 3단 게이트가 이미 검사한다 — 두 층의
  직책 분리(docs/74 §5, 스펙 §1-3).
- 분리 근거 보존: `widget` vs `input` — 위젯 생성은 **내 자식 UI**, input
  주입은 **내 창 밖으로 손을 뻗는** 최대 특권. 위험 등급이 정반대라 토큰을
  나눴다(스펙 §2).

## 3. 배선 경로 (원문이 두 번 바뀌지 않는다 — 보존과 해석의 분리)

```
MANI capabilities= (원문, 파서 key=trim 이후 — 내부 공백 유지)
  → JkxManifest::Parse   else if (key == "capabilities")  → 멤버 capabilities
     (원문 그대로 보존 — 정규화하지 않는다, 스펙 §3.2)
  → JKAppModule_script::jk_app_meta   g_capabilities = mani.capabilities
     (모듈 static — meta 캡처 시점에 1회)
  → jk_app_run_client WorkshopScriptApp 분기  app.SetEnabledCapabilities(g_capabilities)
     (ClientScriptApp 분기(순수 SCRI)는 주입하지 않음 → 게이트 비활성)
  → WorkshopScriptApp::OnInit  host_->EnableCapabilities(capabilitiesRaw_)
     (StartScript 전 1회 — 리로드·라이브 패치도 같은 호스트 인스턴스라 선언 상속)
  → 각 호스트 thunk 선두(HostOf 직후)  GateCap(host, ctx, "<tok>") fail-closed
```

- **정규화(trim+소문자)는 `EnableCapabilities` 몫** — 파스는 원문 보존
  ("원문 보존은 파스, 정규화는 호스트" 접합점 계약). 이 분리가 배지에
  선언 원문(미지 토큰 포함)을 보여 준다는 수신자 가시성을 가능케 한다.
- 게이트 활성 조건: `gateActive_` = `EnableCapabilities`가 **불렸는지**.
  빈 선언("")도 활성이다(능력 없음 = 모두 차단이며 배지가 그대로 표시) —
  "선언 안 함"과 "빈 선언"의 차이는 수신자 UI 계층의 정보일 뿐, 게이트는
  둘 다 차단.
- 미지 토큰은 파스에서 거부하지 않는다 — 게이트에서 바인딩이 없어 무의미할
  뿐, 선언됐다는 사실 자체가 배지에 그대로 표시된다(표 §2 network 동일
  철학: 선언해도 오늘은 아무 API도 안 열림).
- `EnableCapabilities`는 런타임 변경 API가 없다 — 선언은 MANI가 유일 원천.

## 4. 게이트 문구 계약 (고정 — 셀프테스트·d.ts 주석이 이 문자열을 단언)

```
capability '<tok>' not declared in MANI
```

- `JS_ThrowTypeError`로 던져지고, 워크숍 도구 경로(set_script)에서는 응답
  `error`에 실려 돌아간다. set_script의 **hint 행** 계약:
  `"hint":"call the api tool for the function list"` — 정직 에러가 정답
  자리를 가리키게 하는 기존 폐곡선 관습의 연장(게이트 차단은 슬롯 문제가
  아니라 MANI 선언 문제라, MANI의 어느 토큰을 고칠지는 MANI 주석과 배지
  원문이 말한다).
- 셀프테스트가 `find(... != npos)`로 부분 일치 단언하므로 문구를 고치면
  main.cpp 1g(d)·jk.d.ts v8 주석·이 문서를 함께 고친다.

## 5. 수신자 UI — 능력 배지

- 위치: 슬롯 스트립(타이틀바 내장) 콤보 끝 — `kBadgeX = 216`, 폭
  `cr.w - 216 - 30`(X 버튼 침범 금지: 오른쪽 여백 30), 최소 폭 60 미만이면
  생성 생략(어두운 창). rect는 ANCHOR_NONE이라 리사이즈 재배치에도 고정
  (기존 스트립 설계 동일 — 폭은 생성 시 고정, 잔여 ⑧).
- 문구 계약: 선언 있음 = `능력: <원문 그대로>`(미지 토큰 포함), 선언 없음 =
  `능력 없음`. `jk::CapabilityBadgeText` 1곳의 진실원.
- 갱신 시점: `BuildStrip` 1회 + `OnScriptStarted` 리레이즈에서
  `MoveChildToTop`로 살아남기(부트·리로드·슬롯 전환 전부 이 문을 통과 —
  docs/67 "슬롯 선택 먹통" 봉합의 확장).

## 6. 셀프테스트 (RunAppSelfTest, main.cpp — 전 태스크 `0 failure(s)` RC=0)

| 스펙 §6 | 구현 |
|---|---|
| 1 fail-closed + 문구 | 1g (a) — 미선언 setInterval Start 실패 + 문구 단언 + `GateActive()` |
| 2 선언 통과 | 1g (b) — 정규화(`"Timer, input,network,weird "`→trim/소문자·미지 보존) + 통과 + timer winId |
| 3 무조건 허용 | 1g (c) — 게이트 활성 상태에서 log/assert 통과 |
| 5 워크숍만 게이트 | 1g (d) — EnableCapabilities 미호출 경로, 기존 케이스 1-2가 회귀 겸임 |
| 4 MANI 파스 | I2 블록 직후 — 원문 보존(`"Timer, input,network,weird"` 그대로)/빈 선언=능력 없음/키 부재=기존 동일 3검 |
| 6 배지 문구 | 1c2 — `"agent,timer"`→`"능력: agent,timer"`, `""`→`"능력 없음"` |
| — | JkxManifestMerge `capabilities=agent,timer` 잔존 검증(b7a76a3에서 이미) — 파스 테스트의 이중 보장(merge는 행 보존, Parse는 의미 접근) |

## 7. 실측 receipt (라이브 — engine/tools/probes/diag_capgate.ps1, scratch 슬롯 격리)

- `GATE-BLOCK: OK` — set_script에 미선언 API 심음(`injectMouse`) → 토크
  응답에 고정 문구 `capability 'input' not declared in MANI` +
  JS 스택트레이스 `at injectMouse (native)`. 차단이 런타임 발화점(정확한
  함수)을 가리킨다 — 스크립트 저자가 어느 줄을 고칠지 한 눈.
- `GATE-PASS: OK` — widget 선언(bench MANI has widget) 슬롯 set_script →
  `"ok":true` (게이트 통과 + 라이브 재평가 정상).
- probe 격리: scratch 슬롯(gatescratch)이라 사용자 슬롯 파일·라이브 패치
  이력 불변. 선대 Workshop 창 청소는 idempotent(잔존 창이 있으면
  set_script가 ambiguous로 답하는 것 회피 — 단 사용자 Workshop 창도 닫는다,
  잔여 ①). **단 최슬롯 포인터(`.current_workshop`)만은 probe가 `gatescratch`
 로 바꿔 놓았다** — 포인터 대상 파일이 없으면 부팅이 MANI 기본
  (`myapp.js`)으로 자가 치유(ClientScriptApp::OnInit 판정), 런타임 위험
 없음. probe가 포인터를 원복하도록 개선할 것(잔여 ①과 같이 다음 probe 터치
 때).
- **jkagentd.exe ping은 MCP 대응 도구 없음** — 실측 검증은
  `agentctl ping`(+read_events)로 한다(이번 실측 기록한 전제).

## 8. 렛슨 (실측에서 온 것만)

1. **수기 pack은 반영이 아니다**: manifest.txt 교체만으로는 build/apps/
   workshop.jkx가 갱신되지 않는다 — `tools/pack_workshop.ps1` repack을
   해야 한다(docs/60 §3 이미 원장). "MANI 고쳤는데 버전이 여전히 실패"는
   반영 누락을 의심한다.
2. **PowerShell native 전달은 PSI raw-Arguments 패턴**(docs/55 lesson 3
   재확인): agentctl JSON은 `& $exe agentctl $json` 파이핑이 임베디드
   쿼트를 삼킨다. probe diag_capgate는 ProcessStartInfo.Arguments에
   이스케이프한 원문 명령줄을 직접 조립 + 소스값 내부의 이중 이스케이프는
   백슬래시 선이스케이프(`\`→`\\` 후 `"`→`\"`) — 아니면 CRT가
   `2bs+쿼트`를 인용 토글로 삼켜 쿼트 소실(argv 덤프 실측).

## 9. 마이그레이션

- **기존 워크숍 슬롯·폰 실기기**: MANI에 `capabilities=` 없던 앱은 지금부터
  능력 없음으로 게이트된다 — 미선언 API를 쓰던 슬롯은 **MANI 토큰 추가+
  repack** 필요. 정직 에러가 어디를 고칠지 말해 준다("not declared in
  MANI" + 배지 원문).
- **명명 사례 (최종 리뷰 I-1, 2026-10-05)**: 슬롯 `bang-gu.js`가
  `injectMouse`/`injectKey`(토큰 `input`)를 쓰는데 벤치 MANI가 `input`을
  의도적으로 미선언 — 다음 부팅부터 첫 타이머 틱(2.5s)마다 정직 차단
  (canvas 그리기·마우스 이벤트 수신은 계속 동작). **결정은 사용자 몫:
  ① 벤치 MANI에 `input` 추가+repack(주입 계속) — 단 probe diag_capgate의
  GATE-BLOCK 케이스가 PASS로 뒤집혀 실측 원장과 어긋나므로 probe 케이스를
  바꿔야 한다(예: 미선언 토큰을 `uiauto`로 전환) ② 미선언 유지(차단 수용) —
  fail-closed 자세와 실측 원장 유지.** 컨트롤러 판정: ②가 현 기본 —
  fail-closed는 docs/74 사용자 확정 결정이고 probe·셀프테스트·원장이 이에
  정렬돼 있다. ①은 사용자가 말하면 즉시 수리(토큰 1개+repack+probe 정정).
- **슬롯 출하(.jkx) 도구가 개별 MANI를 좁게 선언하는 것** = 단 2 출하
  라인의 다음 문 — 워크숍 벤치 원본은 넓게 선언했지만, 출하 시 각자
  (벤치 MANI가 총망라 성격의 원문임을 파일 주석에 명시).
- 워크숍 템플릿에 capabilities 선언 예시 심기(첫 선언 경험 낮추기) —
  출하 도구 작업 때 같이.

## 10. 잔여 (parked minors — 사용자 수요 없으면 소각 없음; 트라이아지는 최종 리뷰)

**최종 리뷰 트라이아지 (2026-10-05, opus — 8건 전부 keep; ①만 must-track):**
① probe 선대 청소(사용자 창도 닫음)+최슬롯 포인터 원복은 다음 probe 터치 때
함께 수리(수리법 알려짐: 선존재 창 id 캡처→증분만 닫음). ③은 §7 정정으로
봉합(끝). 나머지는 비행동적이며 원장화된 채 유지.

1. probe 선대 청소가 사용자 Workshop 창도 닫는다 — probe에 주석 권고함.
2. GATE-PASS 정규식(`"ok"\s*:\s*true`)이 느슨하다.
3. 스펙 §4.2의 `RequireCap` 명칭과 구현 `GateCap`이 다르다(파일 스폰 헬퍼)
   — 각주로 봉인.
4. 토큰 중복 미제거(동일 토큰 재선언 시 벡터 중복 — HasCapability 선형
   탐색이라 동작 무영향).
5. 셀프테스트 1g gateWinIds (a)/(b) 시나리오 공유.
6. Parse 분기 후속 주석 4칸 들여쓰기 혼돈 가닝.
7. `!o.scriptfile.empty() == false` 이중 부정.
8. 배지 폭 생성 시 고정(리사이즈 미반영) — 기존 스트립 설계 동일.

## 11. 다음 (사용자 몫)

- **눈확인 대기**: 배지 문구 GUI 표시 (`능력: widget,timer,canvas,agent,fs`).
- 단 2 출하 라인 개통: jkctl pack 뿌리 수리 → 슬롯→.jkx 출하 도구(개별
  MANI 좁은 선언+템플릿 예시) → 갤러리.