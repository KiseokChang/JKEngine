# docs/84 — 채팅 자연어 승격 라인 as-built (T1-T5, 2026-10-08)

승격 라인(2026-10-08-chat-llm-promotion) T1-T5 원장. 계획 문서=`.superpowers/sdd/2026-10-08-chat-llm-promotion/`(progress·task-1..5 brief/report — repo 밖 워크숍 원장),
코드 계보는 git log가 진실원. 본 문서는 실측 영수증·함정·deferred를 봉합한다.

## §1. 계보 (T1-T5, 커밋 전체 SHA — rev-parse 실측)

| 태스크 | 커밋(SHA) | 내용 |
| --- | --- | --- |
| T1 | `cb7e72922bc46847960ad92f6bb06bbf3a2283b3` | probes(chat): 폰 cloud-model 턴 조달 실측 probe — /api/chat 직접 왕복 TURN-OK 1.3-2.0s, signin 불요 |
| T1 fix r1 | `bfcf7908e904c622b26cc32043c584905d03c0e6` | phone_cloud_turn 원격 스크립트 자기 소각 + report 정정 |
| T2 | `6739cfdb2bf8b64b581848ea346b09c6d8885a07` | probes(chat): 폰 claude CLI 조달 실측 — launch-claude 경로 판정(조달 불성립로 close) |
| T3 | `95f1ca758d0d36b87ac170bea3263556a6d12bff` | feat(chat): JKLlmEngine 동기 턴 브리지(TurnSync) + ollama-direct leg |
| T3 fix r1 | `507c0352c83aee4d949c7a844ef5742f98049675` | fix(chat): win32 cmd 토글 주입 수리 — 3leg 단일 출처 이스케이프 |
| T4 | `5d31300ef572b1d687e45379b0c173e820417c96` | feat(chat): jkweb/jktalk 자연어 승격 배선 — cfg 선택형 + stub 폴백 |
| T4 fix r1 | `c5e79d6ce37e6f969d455074f1a23cea7bc385d5` | fix(chat): chat.json 부재=미구성 — 승격 opt-in 계약 수리(fileKnown 게이트) |
| T4 fix r2 | `12972107992300d696304f390d8f73c1ac7f0144` | fix(chat): posix llm16 파스-실패 수형 fileKnown 보정 |
| T5 | 본 문서를 포함하는 커밋 | probes(chat): 폰 자연어 E2E probe( phone_promote.sh ) + 원장 착지 |

파일 축: `engine/src/agent/JKLlmEngine.cpp`(+h)·`engine/src/apps/ChatRouter.cpp`(+h)·
`engine/tools/jkweb/main.cpp`·`engine/tools/jktalk/main.cpp`·`engine/src/main.cpp`(desktop selftest
1n-d/e/f)·`engine/tools/posix_selftest/main.cpp`(llm15/llm16+1n-s 도표)·신규 probe
`engine/tools/probes/phone_promote.sh`.

## §2. 캐논 + E2E 영수증

### §2.1 selftest 축 캐논 (T5 재측정 — 무회귀 실측)

| 축 | PASS/FAIL | 비고 |
| --- | --- | --- |
| Windows(jkdesktop) | **495 / 0** | ninja rc=0(2 staging 스텝), T5 착지 축 |
| WSL(posix_selftest) | 472 / 0 | T4 기준, T5 전수 독해 축 |
| posix 어댑터 계열 | 203 | T4 fix r2 기록(llm15/llm16 계열 합산 규약) |
| **폰(termux aarch64)** | **472 / 0** | **T5 신설 축 — probe run 2 실측(PHONE-SELFTEST-PASS=472 FAIL=0)** |

### §2.2 폰 E2E 4정판 영수증 (probe run 2, 원문 인용 — engine/tmp/phone_promote.log)

헤드실측: `HEAD: 12972107992300d696304f390d8f73c1ac7f0144`,
`CONTAMINATION-GATE=CLEAN`(tracked-dirty-count=0), tar 1034240 bytes, 13파일 크기 신선도 일치,
`NINJA-RC=0`, `OLLAMA-MODEL=PRESENT`, `OLLAMA-SERVE=UP`.

```
--- 턴1: 지뢰찾기 켜줘 ---
TURN1-HTTP=200 TURN1-TIME=0.024758
TURN1-REPLY: {"ok":true,"reply":"'지뢰찾기' 앱을 실행합니다.","kind":"Launch","app":"minesweeper","server":"{\"ok\":true}"}
TURN1-STATUS=OK (launch 지시 성립 — kind=Launch app=minesweeper)
TURN1-WINDOW=OK (Minesweeper 창 단정: {"id":8,"title":"Minesweeper","pid":21213,"x":520,"y":190,"w":320,"h":380,...,"focused":true,"minimized":false})
--- 턴2: 창 목록 보여줘 (비매치 — LLM 경로) ---
TURN2-HTTP=200 TURN2-TIME=1.643424
TURN2-REPLY: {"ok":true,"reply":"인식하지 못했습니다. 채팅에서 쓸 수 있는 명령:...예: '지뢰찾기 켜줘', '창 목록'","kind":"Info","app":""}
TURN2-STATUS=FAIL (kind=ListWindows 불성립 — kind 불일치·LLM 파싱 실패 폴백 의심)
TURN2-LLM=engaged (경과초가 stub 즉발 밖 — 클라우드 턴 실측)
--- 턴3: 닫아줘 (argless close 해소) ---
TURN3-HTTP=200 TURN3-TIME=0.031894
TURN3-REPLY: {"ok":true,"reply":"포커스 창을 닫습니다.","kind":"Close","app":"","server":"{\"ok\":true}"}
TURN3-WINDOW=OK (Minesweeper 소멸 단정)
--- 턴4: qqqzzz (무의미 문자열 — 정직 회신) ---
TURN4-HTTP=200 TURN4-TIME=3.381325
TURN4-REPLY: {"ok":true,"reply":"인식하지 못했습니다. 채팅에서 쓸 수 있는 명령:...","kind":"Info","app":""}
TURN4-STATUS=OK PATH=STUB-FALLBACK (무의미 발화 — 파싱 불성립 폴백 안내문·정직)
TURN4-LLM=engaged
=== I. 승격 표면 종합 ===
TURN-SCORE=3/4 (turn1=1 turn2=0 turn3=1 turn4=1)
=== J. 종료 게이트 ===
FINAL-SERVER-PIDS=21107 21148 (서버 UP)·jkweb pid 21177 (ALIVE-AFTER=OK)
CHATJSON-UNCHANGED-AFTER-E2E=OK (sha256 동일)
REMNANT-LS-RC=2 (원격 스크립트 자기 소각 그린)
```

경과초: 턴1 0.025s·턴3 0.032s(즉발 stub) / 턴2 1.643s·턴4 3.381s(LLM 왕복+폴백)— 전부 목표 <10s 성립.

### §2.3 승격 판정 봉합

**PROMOTE-VERDICT: PROMOTE-PARTIAL (3/4/4)** — hard 실패 0(배포·리빌드·selftest·서버·
jkweb·스테이지·소각 전부 그린). 불성립 1턴(턴2) 원인은 **엔진 배선 결함이 아닌
모델 스키마 불준수**(§3 T5-#1 진단 원문). 승격 도표 자체(비매치→LLM→파싱 실패 시
stub 폴백 fail-closed)는 설계대로 작동 — 폴백 회신이 정직 안내문이고 채팅은 무너지지
않는다. stub 라우터가 1차·opt-in 가법 유지.

### §2.4 턴2/턴4 진단 원문 (폰 ollama 직통 — 조립식 프롬프트 파이프, 2026-10-08 실측)

jkweb이 스폰하는 것과 같은 명령(`ollama run "glm-5.3-flash:cloud" "<프리앰블+프롬프트>"`)을
폰에서 stdout 리다이렉트로 통과시켜 raw 모델 출력 관측(611 bytes 실측):

- ollama CLI가 파이프에서도 **thinking 블록을 stdout에 혼입**시킨다 — "Thinking...\n(영어 추론
  텍스트+CRLF·ANSI 커서 시퀀스 ^[[K·^[[1D 혼입)\n...done thinking.\n<최종 답>" 꼴.
  즉 엔진이 보는 stdout은 추론 텍스트+ANSI+최종 답의 혼합물.
- 턴2 최종 답 원문: `{"action": "list_windows"}` — 스키마 밖 값(계약 enum은
  launch/close/focus/**list**/talk).
- 턴4 최종 답 원문: `{"action": "no_op", "reason": "입력 'qqqzzz'이 데스크톱 자동화 명령으로
  인식되지 않습니다. 사용자에게 그렇게 알리는 것이 필요합니다."}` — 스키마 밖 action +
  말바꿈 필드(계약은 talk+text).
- 2중 불성립: ① thinking 블록에 `{`가 들어 있으면 ChatLlmActionParse의 first-`{`..last-`}`
  절단이 두 개의 JSON 사이를 가로질러 무효 슬라이스가 된다 ② 절단이 성립해도(정답만
  있으면) action 값이 enum 밖이라 AgentJson 검증 실패. **②가 결정 원인** — 완전 클린
  stdout이어도 위 원문은 둘 다 파싱 실패로 간다. 모델 컴플라이언스 사안이며 파서/배선
  수형 사안이 아님.

## §3. 함정 원장 (docs/81 §3 승계 + T5 신규)

1. **T5-#1 model schema non-compliance + ollama CLI thinking 혼입**(§2.4 진단 원문) —
   승격 라인의 유일한 미성립 원인. 수리 후보는 deferred(§4 D1).
2. **T5-#2 ChatConfig.directory Windows 기본값 → posix chdir _exit(127)** —
   `ChatConfig.directory` 기본값이 `"I:\\progwork\\JKENGINE"`(Windows 경로)이고 posix Spawn은
   `chdir(workingDir)` 실패 시 `_exit(127)`(JKProcess_posix.cpp:185-205). chat.json에 directory를
   빼면 모든 LLM 턴이 빈 stdout으로 실패해 항시 stub 폴백 — 폰 스테이지 JSON은 directory를
   반드시 폰 실존 경로(`$HOME`)로 명시: `{"engine":"ollama-direct","model":"glm-5.3-flash:cloud","directory":"$HOME"}`
   (실측 sha256=59be04fadae37b2fa36563ee4fb108842552277ca50fd1c529b7c8b1fadbc5bd SIZE=104).
3. **T5-#3 exe-dir chat.json selftest 덧칠 방어**(T3 1표 승계·폰 실측) — selftest가 캐논 축에서
   exe-dir(buildterm/state) chat.json 시딩→소각을 하므로 승격 chat.json 스테이지는 **selftest
   종료 최후**에 간다. probe run 2 실측: `CHATJSON-BEFORE-SELFTEST=ABSENT`,
   `CHATJSON-AFTER-SELFTEST=ABSENT`(덧칠 방어 그린), 스테이지 후 sha256 전량 무변.
4. **signin 게이트 불요** — 폰 ollama 0.23.2에서 cloud 모델 턴이 signin 없이 성립(T1 실측
   TURN-OK, docs/80 §3 근거). NEEDS-SIGNIN 대응 절차는 게이트 밖으로 봉합.
5. **npm/조달 용량** — T2 claude CLI 조달: npm 래퍼 ~223KB·pack ~110MB·musl 번들 ~244MB.
   폰 용량 계약상 wrapper 직행이 아니라 **조달 자체 불성립로 판정**(launch-claude 경로 close) —
   조달 바이너리 커밋도 기각(기계만 커밋 원칙).
6. **bionic 기만** — glibc 프리빌트 바이너리가 bionic(termux)에서 "돌아가는 척"하는 함정;
   동적 로더/바이너리 관련 조달 판정은 직접 실행 실측으로만 확정한다(T2 절차 상속).
7. **drvfs/tar 오염 게이트**(docs/81 §3 #11 승계) — 더러운 원천은 tar에 흘러들지만, tracked
   워킹 카피가 HEAD와 일치하면 오염 0. probe는 tracked-dirty-count 기록+HEAD blob 스테이징
   (probe 본체 자기 파일 제외)+스테이지 원문 tar 끝 append. T5 run 2 실측 CLEAN.
8. **무따옴표 계약 사슬** — 원격 스크립트는 브래킷 pkill(`pkill -f '[j]kweb'`)·`< /dev/null` 금지·
   $TMPDIR·`rm -f -- "$0"` 자기 소각+REMNANT-LS-RC 검사. T5 run 1은 tar 멤버 누락(rc=127)을
   터뜨렸고 → tar -tf 멤버 검증+REMOTE-SCRIPT-PRESENT 단정으로 봉합(재실행 시 스테일 스크립트
   오판 방어).

## §4. Deferred (수리 후보 — 유예 라인)

| ID | 사항 | 근거 | 비고 |
| --- | --- | --- | --- |
| D1 | 승격 파스 견고화 | §2.4 — 모델이 enum 밖 값(`list_windows`/`no_op`)을 내놓음 | 후보: 프롬프트에 enum 정답 예시 강화(`list` 단독), 파서 표기 사상(`list_windows`→list·`no_op`→talk 격상), `ollama run` thinking 억제 조사. 엔진 계약 락(BuildOllamaDirectCmd·TurnSync) 유지 하에 가능한 수형만 — 별도 태스크로 결정 |
| D2 | jkweb usedLlm 표기 | HTTP reply에 LLM 사용 축 표기 미전달(usedLlm 기본 nullptr) | stub/LLM 구별은 현재 timing으로만 — 도표 축 수리 후보 |
| D3 | 폰 브라우저 결제 사용자 실사용 | §5 게이트 | 사용자 선언만 진실원 |

## §5. 사용자 결제 게이트 (대기 — 기록은 사용자 선언만)

폰 서버·jkweb·chat.json 상시(종료 게이트 그린 — FINAL-SERVER-PIDS·JKWEB-ALIVE-AFTER=OK·
CHATJSON-UNCHANGED-AFTER-E2E=OK). 결제 절차:

```
폰 브라우저 → http://localhost:8090/
  예시: '지뢰찾기 켜줘', '창 목록 보여줘', '테트리스 켜줘', 'qqqzzz', 자유 문장
```

probe/본 원장이 결제를 대신 기록하지 않는다 — 사용자 육안 선언으로만 채운다.

## §6. 커밋 원장 (rev-parse 전체 SHA)

T1-T4 전체 SHA는 §1 표(rev-parse 실측). T5 커밋은 본 문서·docs/80 정정·docs/81 유예 갱신·
`engine/tools/probes/phone_promote.sh`를 포함하는 커밋 — SHA는 git log가 진실원.
**T5 커밋 실측: `37a9bf6e695fce04fa5b705042ac35b0cfa07fe3`** (pushed 1297210..37a9bf6).

**IP 스윕 범위 명시(T5 fix r1 — NR5-1)**: 커밋 전 IP 노출 재검사는 **본 커밋 신설/수정 행 +
docs 한정**이었지 트리 전수 스윕이 아니었다 — 4-옥텟 패턴 grep 결과 신설/수정 행과
`phone_promote.sh`·3개 docs는 **0건**(실측 grep rc=1). 추적 트리에는 이전 라인 착지품으로
폰 IP(`$PHONE_HOST` 리터럴)가 **8회 5파일 잔존**(tracked `git grep` 실측 —
phone_chat.sh 2·phone_web_chat.sh 2·phone_ollama_try.sh 2·phone_library.sh 1·
docs/superpowers/plans/2026-10-07-desktop-chat-app.md 1). 본 커밋에서 docs/80·81의
잔존 2건만 `$PHONE_HOST` 화법으로 레닥션했다; 나머지 후속 레닥션은 본 라인 밖 사용자
가동 순서(#83 deferred 소각 라인)에 배정되어 별도 처분한다 — 본 문서의 스윕 계약은
"커밋 스코프 행 전수"로 한정 확정한다.
