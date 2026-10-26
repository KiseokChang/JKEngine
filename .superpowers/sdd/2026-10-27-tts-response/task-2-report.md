# Task 2 리포트 — 폰 배포 런+폰 probe TTS 세그먼트+docs (#117 T2)

커밋 6건(push 전부 origin main 직행 — amend 0·`git add -A` 0):
- **a32a26d** `feat(probes): phone voice probe — #117 TTS 세그먼트 (T2)`(probe)
- **48c5474** fix r1(전용 런 페이즈 상속+ROOT 이중경로) · **d18c639** fix r2
  (focus_window 선행+FAIL 출구 permissions 원복) · **d0bec85** fix r3(-i 세션
  검증 재시도) · **ce0b760** fix r4(웨지 수속+판독 접두) · **0746680** fix r5
  (SESSION 판독 선행 숫자 절단) — fix 5건 = phone_voice.sh 1파일씩(probe 커밋
  +fix 커밋 분리 계약)
- **a54ecc4** `docs(apps): #117 구두 응답 TTS 원장 — docs/90 §7.14+§4.1-11 (T2)`
  (docs) — IP grep = 커밋 6건 전부 IPv4·내부 경로 0건.
- **fix r6(본 커밋 — docs 단독 — 런/probe 재실측 불요·src 무변경)**:
  `docs(apps): TTS 웨지 원장 정정 — hang 스폰 원진+수형 후보 재판정 (T2 fix
  r6)` — docs/90+task-2-report.md만 — 웨지 원장 4건 정정(아래 §5 concerns
  ①③④의 정정 반영): rc=137 = hang 스폰의 마감 kill 증상(R1-R5 무-fail
  receipt ~10건 — "15s 마감×장문 겹침" 요소 폐기)·수형 후보 순위(①즉시
  반환[백그라운드 spawn] 1순위·②마감 상향 near-no-op)·R5 = **완주**(CLI 구간
  stall ~25분 — 빈 CLIOUT receipt — 수기 회복)·opt-out 유효 결정판 = R7 단독
  (R5/R6 = 판독 결함 런 — raw 영수증만 유효)·수기 진단 원문 공백 원장행·
  §C 잔존 edge(t6pv_tts_perm_backup)+OTX 항등식 공전 원장행.

배포 드라이버 = engine/tmp/t117t2_deploy.sh(t115b_t2_deploy.sh 승계 — 커밋
금지 충족)·런 로그 = engine/tmp/t117t2_deploy.log+t117t2_run.log·probe 런 =
engine/tmp/phone_voice_run117_{1..7}.log — 전부 untracked(engine/tmp).

---

## 1. 폰 배포 런 1회 — T2-DEPLOY-OK

- **DEPLOY-BASE a32a26d**(T1 ae0dfb5+probe a32a26d — fix r1-r5는 probe 전용·
  src 무변경이라 폰 바이너리 무영향 = 배포 유효 원존 #115b 계보 승계).
- 전수 git archive(aarch64 — spawn leg 포함) → SWEEP-DIFF 6/472 → **POST-DIFF=0
  hard 게이트**·NINJA-RC=0 **472s**(whisper 신증 전량 재빌드)·WHISPER-ROOT-WIRED
  (tree 149)+LEG-STAGE-WIRED(SPATIAL — libopenal.a 유지배선)·MUSP-LEG-SYM
  alcOpenDevice strings=3(leg 링크 유지)·music.jkx 재팕(PACK-RC=0·ICON-ENTRIES=2)
  ·서버 재기동 **WHISPER_ROOT 보유 부팅**(부팅 env 승계 원존)·Boot 8s/8s·
  SCAN-COUNT=27+SORTED=OK·jkweb 생존(pid 동일)·settings 213B/permissions 33B
  무변경 등호·REMNANT-LS-RC=2·잔상 0(REMNANT left=0).
- **폰 selftest 캐논 재실측 = 기대 등호 성립**:
  `PHONE-SELFTEST-A rc=0 PASS=859 FAIL=0 2u=132 2r=10 2s=20 2v=14 2w=4 2x=15
  2y=17 2z=9` + 런B 동일 — **CANON-A=859 / CANON-B=859 등호(2런 — WSL 859 등호
  계보 — 850+2z 9)** → `T2-DEPLOY-VERDICT: T2-DEPLOY-OK`.
- 사용자 live 세션 존중: 진입 원문(Music focused)→종료 Music 창 복원
  (FINAL-WINDOWS Music focused).

## 2. 폰 probe TTS 세그먼트 (phone_voice.sh B3 — 별행 판정, 총 7런)

- **stamp 원문(진실원)**: `[TTS] speak: 인식하지 못했습니다. 채팅에서 쓸 수
  있는 명령:`(+계행 후속 행) — 전 런 도출(클라 stderr → 서버 boot log 도달 —
  posix 스폰 계약 부모 stdio 상속 ·JKProcess_posix.cpp fork leg 실측 원문)·
  `[TTS] fail (rc=137)` 원문 행(R6/R7 — 15s deadline TerminateJobTree 실측).
- **TTS 지연 원문**: 마침 탭→stamp 첫발견 = **18915(R5)·18520(R6)·20490ms(R7)**
  (STT+답신 구간 통괄 개발력 상한 — 리서치 문턱 1s = 첫음 구간 대조 **표기만**
  — 판정 어설션 아님)·CLI 개발력 표본(스폰→종료) = **R1-R4 12표본 2658-3548ms**
  +**R5 돌입 TTS 엔진 웨지 사건**(R5 CLI 구간 stall ~25분 — 빈 CLIOUT receipt
  — 수기 회복 뒤 런 완주·R6/R7 표본 전량 timeout — 아래 concerns ①).
- **opt-out 1판(VOICE_TTS=0 → stamp 부존)**: 유효 결정판 = **R7 단독** — 부팅
  env 영수증
  (/proc environ `VOICE_TTS=0`)+fresh log stamp 0건(180s 폴링)+세션 전이 양순
  (개시/마침 `-i` 진실원)+기록 도착 관측(+38220B) → `TTS-OPTOUT-OK`. R5·R6 =
  판독 결함 런(raw 영수증 행만 유효 — fix r5 원장)·R1/R2의 opt-out = **무효
  원장행**(30s 대기+done>rec 가양성 — fix r1 격하).
- **미접속 = BLOCKED 원장행**: 코드 본존(`TTS-PHONE-BLOCKED`)·실측 0(전 런
  접속 성립 — 정직 원장행).
- probe 런 원장: R1/R2(fix 전 — stamps 2+2·CLI 3표본·VOICE-PHONE-OK)·R3/R4
  (개시 실패 honest — 페이즈 결함 진원 확정)·R5(stamp 지연 첫 실측=18915·CLI
  웨지 시작 — **런 완주**: CLI 구간 stall ~25분(빈 CLIOUT receipt)뒤 수기
  회복 ·opt-out raw 영수증 유효[판정행 판독 결함])·R6(fix r4 — 웨지 유지
  ·판독 결함)·**R7 = 유효 결정 런: stamp=1 session=1 optout=1 lat=20490
  cli=0(웨지)** — RUN-VALS 요약 스트림에 tts 필드 가산 완료.
- probe 수리 5건(fix r1-r5) = 런 원문 기반 — 전부 재런으로 소각(7런 계보 행).

## 3. docs

- docs/90 **§7.14**(0 체인/1 배선 원리 D1-D6/2 캐논 계보(CANON=2Z-9 — 폰 850→
  859 — WSL 등호)+배포·probe 영수증/3 probe 원문/4 함정 원장 9행[fix r6 웨지
  정정•수기 진단•§C edge 포함]/5 비-목표+EYES/6 커밋 원장+IP 게이트) ·
  **§4.1-11** park 행 5건(웨지 수형 후보 순위·posix 313 재판정 채권·stamp
  다중행 1행 한계·BLOCKED 미발화·이중경로 산출물).

## 4. 캐논 1줄

**#117 T2 종료 — 판정 TTS-DEPLOY-OK·probe stamp/session/optout 축 전원(R7)·
폰 캐논 859 등호 2런(WSL 859 계보 — 2z 9)·TTS 지연 = 마침 탭→stamp 18.5-20.5s
(STT 포괄 상한)·CLI 개발력 2.7-3.5s(12표본) — 리서치 문턱 1s = 첫음 구간 대조
표기·EYES(폰 답듣기 체감) 대기(전제 = 폰 TTS 엔진 웨지 복구 — 재부팅 권장).**

## 5. concerns (fix r6 정정 반영)

1. **폰 TTS 엔진 웨지(런 밖 상태 — 사용자 현장 복구 필요)**: R5의 CLI 표본
   1건 무한 블록(CLI 구간 stall ~25분 — 빈 CLIOUT receipt)뒤 런 완주·libexec
   TextToSpeech helper 잔존·helper 소각 뒤에도 무응답(시스템 TTS 엔진 서비스
   측 — **원진 미확정 원장행**·Termux am = force-stop 미지원 원문) —
   스폰/stamp는 무결(spawn 성립 증거)·**발화는 웨지 상태에서 불가** —
   `[TTS] fail (rc=137)` = **hang 스폰의 마감 kill 증상**(R6/R7에만 출현 —
   R1-R5 무-fail receipt ~10건[120자 발화 15s 내 종료] — "15s 마감×장문 guide
   겹침" 계통 요소 = fix r6 폐기). EYES(답듣기 체감)의 전제 = 복구(폰 재부팅
   권장). **수형 후보 순위: ① 스폰 후 즉시 반환(백그라운드 spawn — hang
   rc=137 반복 구조 소거) = 1순위 ② 마감 비례 상향 = near-no-op 표기** —
   §4.1-11 park — 기각 시 즉시 수리.
2. **posix 313(+9) 미재판정** — T1 공통 TU(2z 순수 부품) 예상 — 채권(§4.1-11).
3. opt-out 1판: R1·R2 = 무효(30s 대기+done>rec 가양성)·R5·R6 = 판독 결함
   (SESSION 판독 접두/괄호 — **raw 영수증 행만 유효**)·**유효 결정판 = R7
   단독**(180s 폴링+세션 전이 통제+판정행 유효) — 정직 원장행(docs §7.14.3·4).
4. CLI 개발력 표본의 exit = 첫음+발화 포괄 **상한**이며 즉귀/블록형 판별 원문
   부족(웨지 방해 전 마지막 표본 2.66-3.55s — 첫 단어 1s 문턱과 직접 대조
   불가) — 실음 체감 = EYES 몫.
5. `engine/engine/tmp` 이중경로 산출물(#115b~fix r1 전 런 캡처·RLOG) = 원장
   보존(삭제 안 함 — §4.1-11) — fix r1(48c5474) 뒤 산출물은 engine/tmp 정착.
6. **런 밖 원장행(fix r6)**: ①"helper 6 잔존/60s+" 수치 = 수기 진단의 세션
   stdout 미수취(카운트 1행만 — 재현 시 stdout 수취 계약 원장행)·②§C 소각
   목록 밖 잔존 edge(`t6pv_tts_perm_backup` — 탭 스크립트 사망 시 생존 —
   후속 probe 커밋에서 소각 목록 가산 몫)·③opt-out 좌표 조립의 `[ "$OTX" =
   "$OTX" ]` 항등식 무해 공전(판정 무영향 — 원장 기록 소화).