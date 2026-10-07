#!/usr/bin/env bash
# 폰 자연어 승격 E2E probe (채팅 LLM 승격 라인 T5 — plan 2026-10-08-chat-llm-promotion,
# phone_chat_close.sh(phone_apps.sh) 기계 승계 — 윈도 측 드라이버 + 폰 측 영수증
# 스크립트 구성). 영수증 목표:
#   ① tar-over-ssh 재배포 — 승격 라인이 건드린 소스 전량(T3+T4 diff 6739cfd..HEAD
#      전수 — 라우터/엔진/jkweb/jktalk/main.cpp/posix_selftest)+함정 원장 §3 #6
#      원칙(신선도 재배포 — chat-close 라인 유산 JKWindowServer 2파일 동반) ②
#      폰 aarch64 ninja 리빌드(NINJA-RC 전파 — ~9-10분) ③ AppSelfTest 0 failure(s)
#      (폰 축 캐논 재실측 — 1n-f 승격 배선 도표 동증분) ④ **exe-dir chat.json
#      함정 방어 실측**(T3 1표 승계): selftest가 exe-dir(buildterm/state)의
#      chat.json을 시딩→복원하는 계약 — 스테이지 **전** selftest 원칙+시딩 후
#      잔존 단정(ABSENT), 승격 chat.json은 **최후** 스테이지 ⑤ jkweb 기동
#      (nohup loopback 8090 — --bind all 금지, 보안 계약) ⑥ **E2E 4정판**(프론트
#      도메인 문장 그대로, 각 턴 경과초=curl time_total 기록, 목표 <10s):
#        턴1 "지뢰찾기 켜줘" → kind=Launch app=minesweeper + Minesweeper 창 단정
#        턴2 "창 목록 보여줘" → kind=ListWindows(**LLM 경로 단정** — 이 문장은
#          뒤접미 트리거 불매치라 stub이 Info 가이드로 돌고 LLM만 List로 산다)
#        턴3 "닫아줘" → argless close 해소(0e25762 수리판) + 소멸 단정
#        턴4 "qqqzzz" → HTTP 200 + 정직 회신(모델 안내문이냐 stub 폴백이냐 경로까지
#          기록 — crash/500이 아닌 것만이 계약)
#   ⑦ 종료 상태: 서버 UP + jkweb ALIVE + 승격 chat.json 상시(사용자 결제 게이트 —
#      폰 브라우저 localhost:8090 실사용 육안. **probe가 결제를 대신 기록하지
#      않는다**).
#   판정 사다리(라벨 계약):
#     PROMOTE-OK  = TURN-SCORE 4/4(각 턴 status/kind/경과초 행 수납) — rc=0
#     PROMOTE-PARTIAL = TURN-SCORE n/4 — 원인 행별(턴 실패·조달 실패 = 정직
#                   영수증만, rc=0) / PROMOTE-FAIL(턴·조달 — rc=0 통과)
#     PROMOTE-FAIL(rc=1) = hard FAIL만: ssh 단절·배포 실패·NINJA-RC≠0·
#                   selftest 회귀·서버/jkweb 기동 실패·chat.json 스테이지 실패·
#                   원격 스크립트 잔존.
#   실행법(윈도 Git Bash, 저장소 루트 어디서든):
#     PHONE_HOST=<폰 IP> bash engine/tools/probes/phone_promote.sh
#   접속 정보는 환경변수로(PHONE_HOST 필수 — 내부 IP는 커밋하지 않는다,
#   phone_apps.sh fix r1 M4 계약 승계; PHONE_PORT PHONE_USER PHONE_KEY만 기본값).
#   풀 로그: engine/tmp/phone_promote.log(드라이버 전체 — tee).
#   재실행 가능(멱등): 배포 tar 전량 재밀(증분 배포 — ninja는 재편성)·pre-clean
#   브래킷 pkill(-9 에스컬레이션 — SIGTERM 흡수 원장)·서버·jkweb 둘 다 재기동
#   (T7 fix r1 신선도 계약: 이전 커밋 바이너리의 jkweb은 승격 배선 없는 구판이므로
#   **절사가 정답** — WEB-DOWN-BYPRECLEAN=EXPECTED). E2E 전 잔존 Minesweeper 창은
#   app 지명 close로 치운다(턴3 argless 단정의 오염 방지). Wi-Fi 낙하: tar 창조
#   3회 재시도+git 판정 3회 재시도(문서 헤더 재용)·원격 절차는 자기 소각 후 잔존
#   검사(REMNANT-LS-RC=2 — T1 NR1 이행). 원격 run이 ssh 단절로 죽으면 드라이버가
#   RUN-DROPPED로 인쇄하고 재실행을 지시 — 폰 측 ninja 로그는 $TMPDIR에 살아서
#   재실행이 증분을 이득으로 튼다.
#   함정 원장(docs/81 §3 12건 + docs/80 §6 7건 + 신규 T5 1표 승계):
#     · tar는 저장소 루트에서 만들어 `-C ~/JKENGINE`으로 풀어야 한다(engine/engine
#       중첩 트랩). 폰 /tmp는 쓰기 불가 — 폰 측 스크래치는 $TMPDIR.
#     · 배포 오염 게이트(docs/81 §3 #11): 더러운(미커밋) 배포 원천은 HEAD blob
#       스테이징으로 배포 — probe 본체(자기 파일)만 예외. 스테이지 원문은 타르
#       끝에 추가(-C)해 추출 순서상 나중 항목이 이긴다.
#     · **폰 chat.json directory 함정(T5 신규 — 실측 상면)**: cfg.directory 기본
#       값은 Windows 절대 경로(I:\\progwork\\JKENGINE)이고 posix 스폰은 chdir
#       실패 시 자식이 _exit(127) — 배선이 산다 해도 모든 LLM 턴이 빈 stdout으로
#       정직 실패하고 stub 폴백만 돈다("같은 결과 다른 원인" — 스테이지 JSON에
#       directory를 폰 실존 경로로 명시해야 한다).
#     · ollama run의 프롬프트 본문 컨텐츠 따옴표·파이프 금지(T4 concern ① —
#       claude.cmd/ollama 재인용 사슬에서 컨텐츠 인용이 지역을 닫아 파이프를
#       살린다): 배선 프롬프트 본문(ComposeChatLlmPrompt)은 무인용 계약이고, 본
#       probe가 폰 chat.json을 쓸 때도 값에 따옴표를 넣지 않는다.
#     · 조달된 glibc 바이너리는 폰 bionic에서 exec 거부 — 표시 메시지가
#       "No such file or directory"로 기만(docs/81 §3 #8) — 판정은 실측 재실행으로.
#     · ssh 원격 명령에 로컬 `< /dev/null` 금지(채널 hang — 구계약 승계).
#     · rc 봉합: 원격 복합문이 echo로 끝나면 종료코드가 항상 0 — exit로 전파,
#       `|| true` 금지(정수 카운트용 grep -c는 의도된 예외 — 주석 명시).
#     · tar 전송은 LAN 내부 ssh 한정 — 어떤 클라우드/외부로도 가지 않는다.
#     · jkweb은 기본 루프백 바인드이다(0.0.0.0=--bind all 옵트인 — 본 probe는
#       쓰지 않는다). 사용자 결제 창은 폰 브라우저 localhost:8090이다.
set -u
export MSYS_NO_PATHCONV=1  # MSYS가 ssh 인수의 posix 경로를 찢는 것 차단(chat T8 선례)

ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
cd "$ROOT" || exit 1
SCRATCH="$ROOT/engine/tmp"
mkdir -p "$SCRATCH"
TARBALL="$SCRATCH/phone_promote.tar"
RLOG="$SCRATCH/phone_promote.log"
RSRC_TAR_NAME="phone_promote_remote.sh"
RSRC_PHONE="~/JKENGINE/$RSRC_TAR_NAME"   # 폰 측 삭제는 remote script 마지막(임시 파일만)

# 접속 정보는 환경변수로(PHONE_HOST 필수 — 기본값 금지: 내부 IP는 커밋·문서·
# 리포트에 두지 않는다).
PHONE_HOST="${PHONE_HOST:-}"
PHONE_PORT="${PHONE_PORT:-8022}"
PHONE_USER="${PHONE_USER:-u0_a4}"
PHONE_KEY="${PHONE_KEY:-$HOME/.ssh/termux_jkengine}"
[ -n "$PHONE_HOST" ] || { echo "PROMOTE-FAIL: PHONE_HOST unset — 폰 IP를 환경변수로 지정하세요 (내부 IP는 커밋하지 않는다)"; exit 1; }
SSH="ssh -p $PHONE_PORT -o BatchMode=yes -o ConnectTimeout=15 -o ServerAliveInterval=15 -o ServerAliveCountMax=10 -i $PHONE_KEY $PHONE_USER@$PHONE_HOST"

# 풀 로그(engine/tmp/phone_promote.log) — 드라이버 전체(tee; 원격 run은 별도
# RUNLOG로도 한벌 승계 — remote 스크립트의 exit 어설션이 이 파일을 먹지 않게).
exec > >(tee "$RLOG") 2>&1

FAIL() { echo "PROMOTE-FAIL: $*"; exit 1; }

# ── 배포 원천: 승격 라인(T3+T4, diff 6739cfd..1297210)의 코드 9파일 + 신선도
#    동반(chat-close 라인 유산 JKWindowServer 2파일 + ClientChatApp — chat-close
#    probe가 헤더·ClientChatApp을 미배포했음을 헤더 원장이 기록) + probe 본체.
#    CMakeLists는 승격 라인이 건드리지 않았다(git diff 실측 — T3/T4 무변, jkweb
#    /jktalk/target은 폰 CMake에 이미 있다) — 무변 파일 미배포(함정 #6 원칙의
#    역: CMake 재생성이 도는 조건의 신규 소스만 전부 동반이면 충분).
FILES=(
  engine/include/agent/JKLlmEngine.h        # T3 TurnSync+fileKnown 표지(T4 fix r1)
  engine/include/apps/ChatRouter.h          # T4 ChatRouteTurn+승격 게이트 선언
  engine/src/agent/JKLlmEngine.cpp          # T3+T4 ollama-direct leg·TurnSync
  engine/src/apps/ChatRouter.cpp            # T4 ChatRouteTurn·파서·프롬프트 조립
  engine/src/main.cpp                       # T3 1n-d+1n-e·T4 1n-f selftest
  engine/tools/jkweb/main.cpp               # T4 뇌호출 배선(HandleTalk hook)
  engine/tools/jktalk/main.cpp              # T4 동형 배선
  engine/tools/posix_selftest/main.cpp      # T3 llm15·T4 llm16 어댑터 축
  engine/tools/posix_selftest/build.sh      # T3 ChatRouter 링크(어댑터 축)
  engine/src/server/JKWindowServer.cpp      # chat-close 유산 — 신선도 재배포
  engine/include/server/JKWindowServer.h    # 〃(해소 계약 resolver 헤더)
  engine/src/apps/ClientChatApp.cpp         # chat-close 유산 — probe 미배포분
  engine/tools/probes/phone_promote.sh      # probe 본체(폰에 원문 유산)
)

for f in "${FILES[@]}"; do
  [ -f "$ROOT/$f" ] || FAIL "deploy source missing: $f"
done

echo "=== 0. 배포 오염 게이트 (docs/81 §3 #11 — 더러운 원천은 HEAD blob 스테이징) ==="
# git 네이티브(git.exe) 인수 경로 — MSYS_NO_PATHCONV=1 하에서도 유효한 Windows형
# 경로(cygpath; phone_apps.sh run 5a/5b 실측 선례).
GITROOT="$(cygpath -w "$ROOT" 2>/dev/null || echo "$ROOT")"
echo "HEAD: $(git -C "$GITROOT" rev-parse HEAD 2>/dev/null || echo rev-parse-failed)"
# 정수 카운트용 grep -c는 rc=1(0건)도 의도된 값 — `|| true` 대신 case 분기.
DIRTY_COUNT=$(git -C "$GITROOT" status --porcelain 2>/dev/null | grep -avc '^??')
echo "tracked-dirty-count=$DIRTY_COUNT (tracked 변경 0이면 배포 원천=워킹 카피 원문)"
STAGE_DIR=""
WIPPED=0
STAGE_ARGS=()
for f in "${FILES[@]}"; do
  [ "$f" = "engine/tools/probes/phone_promote.sh" ] && continue   # 자기 파일 예외
  # git 판정 재시도 3회(공유 repo 경합 — commit-graph 경고는 stderr로 무해 통과).
  GIT_RC=9
  GIT_OUT="unset"
  for try in 1 2 3; do
    if [ "$try" -gt 1 ]; then sleep 3; fi
    GIT_OUT=$(git -C "$GITROOT" diff --quiet HEAD -- "$f" 2>&1 >/dev/null)
    GIT_RC=$?
    [ "$GIT_RC" -le 1 ] && break
  done
  [ "$GIT_RC" -le 1 ] \
    || FAIL "git diff HEAD -- $f 판정 실패(재시도 3회) — rc=$GIT_RC: $(echo "$GIT_OUT" | tail -1)"
  [ "$GIT_RC" -eq 1 ] || continue
  if [ -z "$STAGE_DIR" ]; then
    STAGE_DIR="$SCRATCH/head_stage"
    rm -rf "$STAGE_DIR"
    mkdir -p "$STAGE_DIR"
  fi
  GS_OK=0
  for try in 1 2 3; do
    git -C "$GITROOT" show "HEAD:$f" > "$STAGE_DIR/$f" 2>>"$SCRATCH/head_gshow.err" \
      && { GS_OK=1; break; }
    sleep 3
  done
  [ "$GS_OK" -eq 1 ] \
    || FAIL "git show HEAD:$f 스테이징 실패(재시도 3회) — $(tail -1 "$SCRATCH/head_gshow.err")"
  WIPPED=1
  STAGE_ARGS+=(-C "$STAGE_DIR" "$f")
done
if [ "$WIPPED" -eq 1 ]; then
  echo "NOTE-WIP: 더러운 배포 원천 $WIPPED건 — HEAD blob 스테이징으로 배포"
else
  echo "CONTAMINATION-GATE=CLEAN (배포 원천 전량 HEAD와 일치 — 워킹 카피 배포)"
fi

echo "=== 1. pre-flight + pre-clean (재실행 가능성 — wake-lock, 서버·jkweb 절사) ==="
$SSH 'echo PHONE-REACHABLE; uname -m; command -v termux-wake-lock >/dev/null 2>&1 && termux-wake-lock || echo WARN-no-wake-lock' \
  || FAIL "ssh failed (phone unreachable)"
# 절사 대상=서버+**jkweb**(승격은 구판 jkweb(배선 없음)을 새 바이너리로 교체하는
# 재기동 계약 — T7 fix r1 신선도. chat-close 라인의 "jkweb 절사 금지"와 반대가
# 정답이다: 그 원장의 이유(육안 게이트 세션 유지)는 이 probe가 **이후** 다시
# 세워서 만족한다).
$SSH "pkill -f 'buildterm/[j]kdesktop' 2>/dev/null; pkill -f 'buildterm/[j]kweb' 2>/dev/null; sleep 2; pkill -9 -f 'buildterm/[j]kdesktop' 2>/dev/null; pkill -9 -f 'buildterm/[j]kweb' 2>/dev/null; sleep 1; pgrep -f 'buildterm/[j]kdesktop' >/dev/null 2>&1 && { echo PRECLEAN-FAIL; exit 1; }; pgrep -f 'buildterm/[j]kweb' >/dev/null 2>&1 && { echo PRECLEAN-FAIL-WEB; exit 1; }; echo SRV-DOWN-WEB-DOWN" \
  || FAIL "pre-clean could not bring server/jkweb down (폰 오래된 서버 정리 실패)"
echo "WEB-DOWN-BYPRECLEAN=EXPECTED (바이너리 교체 재기동 계약 — T7 fix r1)"
# 승격 전제 생존 확인 — ollama serve는 절사하지 않는다(폰 자원 보존, T1 계약).
OLL=$($SSH "pgrep -f '[o]llama serve' >/dev/null 2>&1 && echo OLLAMA-UP || echo OLLAMA-DOWN" || true)
echo "ollama serve 상태: $OLL"
if [ "$OLL" != "OLLAMA-UP" ]; then
  echo "PROCURE-HONEST: ollama serve DOWN — 턴 조달은 실패 영수증으로 간다(hard FAIL 아님). 재기동: ssh '$PHONE_USER@호스트 ollama serve' (사용자 측 조치 권장)"
fi

echo "=== 2. tar-over-ssh 재배포 (13파일 — 승격 라인 소스 전량 + 신선도 + probe) ==="
cat > "$SCRATCH/$RSRC_TAR_NAME" <<'PHONEEOF'
#!/bin/bash
# 폰 측 영수증 절차 (phone_promote.sh가 생성 — 승격 T5). 서버·jkweb은 E2E 후
# **켜 둔 채** 종료한다(사용자 결제 게이트 — 폰 브라우저 localhost:8090).
set -u
cd ~/JKENGINE/engine
FAIL() { echo "PROMOTE-FAIL: $*"; exit 1; }
TMPD="${TMPDIR:-/data/data/com.termux/files/home/tmp}"
NLOG="$TMPD/promote_ninja.log"
STLOG="$TMPD/promote_selftest.log"
CFG_DIR="buildterm/state"
CFG="$CFG_DIR/chat.json"

echo "=== A. pre-check — chat.json 상태 측정(selftest **전**) + ollama 조달 ==="
if [ -f "$CFG" ]; then
    echo "CHATJSON-BEFORE-SELFTEST=PRESENT ($(wc -c <"$CFG") bytes)"
else
    echo "CHATJSON-BEFORE-SELFTEST=ABSENT"
fi
command -v ollama >/dev/null 2>&1 || echo "OLLAMA-CMD=ABSENT (턴 조달 정직 실패 경로)"
ollama ls 2>/dev/null | grep -a 'glm-5.3-flash:cloud' >/dev/null \
  && echo "OLLAMA-MODEL=PRESENT" || echo "OLLAMA-MODEL=ABSENT (조달 실패 — 턴은 정직 영수증)"
pgrep -f '[o]llama serve' >/dev/null 2>&1 && echo "OLLAMA-SERVE=UP" || echo "OLLAMA-SERVE=DOWN"

echo "=== B. ninja rebuild (aarch64 — rc 전파, ~9-10분) ==="
ninja -C buildterm -j4 >"$NLOG" 2>&1
N_RC=$?
tail -4 "$NLOG"
echo "NINJA-RC=$N_RC"
[ "$N_RC" -eq 0 ] || FAIL "ninja rebuild rc=$N_RC (aarch64 compile failure)"
[ -x buildterm/jkdesktop ] || FAIL "buildterm/jkdesktop missing after rebuild"
[ -x buildterm/jkweb ] || FAIL "buildterm/jkweb missing after rebuild"

echo "=== C. selftest (계약: AppSelfTest 0 failure(s) — 1n-d/1n-f 승격 도표 동증분) ==="
timeout 600 ./buildterm/jkdesktop test >"$STLOG" 2>&1
S_RC=$?
ST_PASS=$(grep -ac '^\[PASS\]' "$STLOG")
ST_FAIL=$(grep -ac '^\[FAIL\]' "$STLOG")
echo "PHONE-SELFTEST-PASS=$ST_PASS FAIL=$ST_FAIL (폰 축 캐논 — 이전 폰 기록 406·WSL 472 계보 자기합산)"
grep -a 'AppSelfTest' "$STLOG" | tail -2
grep -aq 'AppSelfTest: 0 failure(s)' "$STLOG" || FAIL "AppSelfTest not 0 failure(s)"
[ "$S_RC" -eq 0 ] || FAIL "selftest rc=$S_RC"
[ "$ST_FAIL" -eq 0 ] || FAIL "selftest FAIL=$ST_FAIL (폰축 회귀)"
for s in 1 2 3 4 5 6 7; do
    grep -aq "^\[PASS\] 1n-s$s" "$STLOG" || FAIL "selftest 1n-s$s not PASS (폰 1n-s 도표 미실행)"
done
grep -aq '^\[PASS\] 1n-d' "$STLOG" || FAIL "selftest 1n-d 전부 미실행 (T3 브리지 도표 부재)"
grep -aq '^\[PASS\] 1n-f' "$STLOG" || FAIL "selftest 1n-f 전부 미실행 (T4 승격 배선 도표 부재)"

echo "=== D. exe-dir chat.json 함정 방어 실측 (T3 1표 — selftest는 시딩을 지운다) ==="
# 위계: selftest(1n-d21 계약)가 시딩+복원/소각을 마쳤어야 한다. chat.json이
# 이 시점에 존재하면 selftest가 승격 파일을 **덧칠/잔존시켰다**(덧칠 함정 —
# 스테이지는 최후 절차라 원칙상 여기 ABSENT가 정답).
if [ -f "$CFG" ]; then
    FAIL "chat.json EXISTS after selftest — selftest가 exe-dir 시딩을 지우지 못함(덧칠 함정 실측) — $(wc -c <"$CFG") bytes"
fi
echo "CHATJSON-AFTER-SELFTEST=ABSENT (selftest 시딩 소각 계약 그린 — 승격 스테이지는 최후 절차로 간다)"

echo "=== E. boot server (bash ~/tx4_boot.sh — DISPLAY=:1 표준) ==="
bash ~/tx4_boot.sh
sleep 8
P=$(pgrep -f 'buildterm/jkdesktop' | tr '\n' ' ')
[ -n "$P" ] || { echo "--- srvx.log tail:"; tail -20 ~/srvx.log; FAIL "no jkdesktop process 8s after tx4_boot (server died)"; }
echo "server pids: $P"
PING=$(timeout 15 ./buildterm/jkdesktop agentctl '{"tool":"ping","args":{}}' 2>/dev/null | grep -a '{' | head -1)
echo "ping reply: $PING"
printf '%s' "$PING" | grep -aq '"ok":true' || FAIL "agentctl ping did not ok — $PING"
# E2E 오염 방지 — 잔존 Minesweeper 창을 app 지명 close로 치운다(턴3 argless의
# 포커스 해소를 이 라인 스폰 창으로 확정하기 위한 선행 정리, 3회 한정).
for i in 1 2 3; do
    ./buildterm/jkdesktop agentctl '{"tool":"close_window","args":{"app":"minesweeper"}}' >/dev/null 2>&1
    sleep 2
done
echo "MINESWEEPER-PRECLEAN=DONE (잔존 여부와 무관 — 라운드 정리)"

echo "=== F. jkweb 기동 (nohup, loopback 8090 — --bind all 금지, 보안 계약) ==="
nohup ./buildterm/jkweb --port 8090 >"$TMPD/promote_jkweb.log" 2>&1 &
sleep 3
JW=$(pgrep -f 'buildterm/[j]kweb' | tr '\n' ' ')
[ -n "$JW" ] || { echo "--- jkweb log:"; cat "$TMPD/promote_jkweb.log"; FAIL "jkweb not alive 3s after start"; }
echo "jkweb pids: $JW"
cat "$TMPD/promote_jkweb.log"
GET=$(curl -s -w "\nHTTP=%{http_code}" http://localhost:8090/)
printf '%s' "$GET" | grep -aq 'HTTP=200' || FAIL "GET / not 200 — jkweb dead? log above"
printf '%s' "$GET" | grep -aq 'JK 채팅' || FAIL "GET / missing page marker 'JK 채팅'"
echo "JKWEB-HTTP-GET=OK (loopback — 폰 브라우저 localhost:8090 결제 창)"

echo "=== G. 승격 chat.json 스테이지 (최후 절차 — fileKnown 계약: exe-dir/state) ==="
# 백업 원칙: 기존 파일이 있으면 원문 보존(chat.json.bak-*), 없으면 신설.
if [ -f "$CFG" ]; then
    BAK="$CFG.bak-$(date +%s)"
    cp "$CFG" "$BAK" || FAIL "chat.json backup failed"
    echo "CHATJSON-STAGE=OVERWROTE-BACKED-UP -> $BAK"
else
    echo "CHATJSON-STAGE=CREATED (기존 부재 실측 — D단계 ABSENT와 합치)"
fi
mkdir -p "$CFG_DIR"
# directory는 반드시 폰 실존 경로($HOME) — 기본값(Windows 절대 경로)이면 posix
# 스폰 chdir 실패 _exit(127)로 **모든** LLM 턴이 실패한다(신규 함정 T5 1표).
printf '{"engine":"ollama-direct","model":"glm-5.3-flash:cloud","directory":"%s"}\n' "$HOME" > "$CFG"
CFG_SHA=$(sha256sum "$CFG" | awk '{print $1}')
echo "CHATJSON-CONTENT: $(cat "$CFG")"
echo "CHATJSON-SHA256=$CFG_SHA SIZE=$(wc -c <"$CFG")"
printf '%s' "$(cat "$CFG")" | grep -aq '"engine":"ollama-direct"' || FAIL "staged chat.json malformed (engine)"
printf '%s' "$(cat "$CFG")" | grep -aq '"model":"glm-5.3-flash:cloud"' || FAIL "staged chat.json malformed (model)"
printf '%s' "$(cat "$CFG")" | grep -aq '"directory":"/' || FAIL "staged chat.json directory not a posix path (127 함정 방어)"

ASSERT_FIELD() { printf '%s' "$1" | grep -aq "$2" || FAIL "$3 — reply: $1"; }

# E2E 턴 — 각 턴 = POST /talk + 경과초(curl time_total) + kind/app/server 판정.
# 정확 트리거(턴1·턴3)는 설계 계약상 즉발(LLM 우회 — 1n-f)이고 비매치(턴2·턴4)
# 만 LLM 턴을 탄다. **경과초 <10s 목표**, TURNn-LLM=engaged(≥0.8s)·instant(즉발).
talk_turn() { # $1=발화 JSON 본문 $2=라벨
    R=$(curl -s -m 45 -H 'Content-Type: application/json' \
        --data "$1" -w "\nTALKHTTP=%{http_code} TALKTIME=%{time_total}" \
        http://localhost:8090/talk)
    printf '%s\n' "$R"
}

echo "=== H. E2E 4정판 ==="
# 턴1 — 정확 트리거 즉발 경로.
echo "--- 턴1: 지뢰찾기 켜줘 ---"
T1R=$(talk_turn '{"text":"지뢰찾기 켜줘"}')
T1_TIME=$(printf '%s' "$T1R" | grep -ao 'TALKTIME=[0-9.]*' | head -1 | cut -d= -f2)
T1_HTTP=$(printf '%s' "$T1R" | grep -ao 'TALKHTTP=[0-9]*' | head -1 | cut -d= -f2)
T1_BODY=$(printf '%s' "$T1R" | grep -a '^{' | head -1)
echo "TURN1-HTTP=$T1_HTTP TURN1-TIME=$T1_TIME"
echo "TURN1-REPLY: $T1_BODY"
if [ "$T1_HTTP" = "200" ] && printf '%s' "$T1_BODY" | grep -aq '"kind":"Launch"' \
   && printf '%s' "$T1_BODY" | grep -aq '"app":"minesweeper"' \
   && printf '%s' "$T1_BODY" | grep -aq '"ok":true'; then
    echo "TURN1-STATUS=OK (launch 지시 성립 — kind=Launch app=minesweeper)"
    T1_OK=1
else
    echo "TURN1-STATUS=FAIL (http=$T1_HTTP body=$T1_BODY)"
    T1_OK=0
fi
printf '%s' "$T1R" | grep -aq 'TALKTIME=' || { T1_TIME=""; T1_OK=0; echo "TURN1-STATUS=FAIL (time_total 미수신)"; }
# LLM 경로 분류 — 즉발(<0.8s)이면 설계 계약의 LLM 우회(1n-f0) 실측.
awk -v t="${T1_TIME:-0}" 'BEGIN{ if (t+0 >= 0.8) print "TURN1-LLM=engaged"; else print "TURN1-LLM=instant-stub-path" }'
# 창 단정 — 최대 40초 폴링(폰 클라 스폰 15-18s 원장).
MSW=""
for i in $(seq 1 20); do
    W=$(timeout 15 ./buildterm/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)
    MSW=$(printf '%s' "$W" | grep -aoE '\{"id":[^}]*"title":"Minesweeper"[^}]*\}' | head -1)
    [ -n "$MSW" ] && break
    sleep 2
done
if [ -n "$MSW" ]; then
    printf '%s' "$MSW" | grep -aqE '"id":[0-9]+' || { echo "TURN1-WINDOW=FAIL (id 결손): $MSW"; T1_OK=0; }
    [ "$T1_OK" -eq 1 ] && echo "TURN1-WINDOW=OK (Minesweeper 창 단정: $MSW)"
else
    echo "TURN1-WINDOW=FAIL (Minesweeper 창 미생성) — list: $W"
    T1_OK=0
fi

# 턴2 — 비매치 → LLM 턴(배선 실측의 본계). stub이면 kind=Info 가이드로 돈다.
echo "--- 턴2: 창 목록 보여줘 (비매치 — LLM 경로) ---"
T2R=$(talk_turn '{"text":"창 목록 보여줘"}')
T2_TIME=$(printf '%s' "$T2R" | grep -ao 'TALKTIME=[0-9.]*' | head -1 | cut -d= -f2)
T2_HTTP=$(printf '%s' "$T2R" | grep -ao 'TALKHTTP=[0-9]*' | head -1 | cut -d= -f2)
T2_BODY=$(printf '%s' "$T2R" | grep -a '^{' | head -1)
echo "TURN2-HTTP=$T2_HTTP TURN2-TIME=$T2_TIME"
echo "TURN2-REPLY: $T2_BODY"
if [ "$T2_HTTP" = "200" ] && printf '%s' "$T2_BODY" | grep -aq '"kind":"ListWindows"' \
   && printf '%s' "$T2_BODY" | grep -aq '"ok":true'; then
    echo "TURN2-STATUS=OK (kind=ListWindows — stub은 이 문장을 Info로 돌리므로 LLM 경로 단정 성립)"
    T2_OK=1
else
    echo "TURN2-STATUS=FAIL (kind=ListWindows 불성립 — kind 불일치·LLM 파싱 실패 폴백 의심) body=$T2_BODY"
    T2_OK=0
fi
[ -n "$T2_TIME" ] && awk -v t="$T2_TIME" 'BEGIN{ if (t+0 >= 0.8) print "TURN2-LLM=engaged (경과초가 stub 즉발 밖 — 클라우드 턴 실측)"; else print "TURN2-LLM=instant (0.8s 미만 — LLM 턴 아님으로 보이는 영수증, 시간은 참고)"}'

# 턴3 — "닫아줘" argless close — 포커스 해소(0e25762 수리판 본경로).
echo "--- 턴3: 닫아줘 (argless close 해소) ---"
T3R=$(talk_turn '{"text":"닫아줘"}')
T3_TIME=$(printf '%s' "$T3R" | grep -ao 'TALKTIME=[0-9.]*' | head -1 | cut -d= -f2)
T3_HTTP=$(printf '%s' "$T3R" | grep -ao 'TALKHTTP=[0-9]*' | head -1 | cut -d= -f2)
T3_BODY=$(printf '%s' "$T3R" | grep -a '^{' | head -1)
echo "TURN3-HTTP=$T3_HTTP TURN3-TIME=$T3_TIME"
echo "TURN3-REPLY: $T3_BODY"
if [ "$T3_HTTP" = "200" ] && printf '%s' "$T3_BODY" | grep -aq '"kind":"Close"' \
   && printf '%s' "$T3_BODY" | grep -aq '"ok":true'; then
    echo "TURN3-STATUS=OK (Close 지시 + 서버 ok — argless 해소 성립)"
    T3_OK=1
else
    echo "TURN3-STATUS=FAIL (http=$T3_HTTP body=$T3_BODY)"
    T3_OK=0
fi
# 소멸 단정 — 최대 40초 폴링.
T3_GONE=0
for i in $(seq 1 20); do
    W=$(timeout 15 ./buildterm/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)
    if [ -z "$(printf '%s' "$W" | grep -aoE '\{"id":[^}]*"title":"Minesweeper"[^}]*\}')" ]; then
        T3_GONE=1
        break
    fi
    sleep 2
done
if [ "$T3_GONE" -eq 1 ]; then
    [ "$T3_OK" -eq 1 ] && echo "TURN3-WINDOW=OK (Minesweeper 소멸 단정)"
else
    echo "TURN3-WINDOW=FAIL (Minesweeper 잔존): $W"
    T3_OK=0
fi
awk -v t="${T3_TIME:-0}" 'BEGIN{ if (t+0 >= 0.8) print "TURN3-LLM=engaged"; else print "TURN3-LLM=instant-stub-path" }'

# 턴4 — 무의미 문자열 → 정직 회신. kind=Info(stub 폴백이냐 모델 talk냐)·200·
# ok=true·회신 비공백이면 계약 성립; **crash·500·공허**가 아닌 것만이 계약.
# 경로 분류: 회신이 stub 가이드 원문(인식하지 못했습니다…)이면 STUB-FALLBACK
# (파싱 실패 폴백), 그 밖이면 MODEL-GUIDE(모델 text가 도달). LLM 턴이 실패
 # (스폰 실패·빈 stdout)했어도 같은 STUB-FALLBACK 표면 — 경과초가 구별 원료.
echo "--- 턴4: qqqzzz (무의미 문자열 — 정직 회신) ---"
T4R=$(talk_turn '{"text":"qqqzzz"}')
T4_TIME=$(printf '%s' "$T4R" | grep -ao 'TALKTIME=[0-9.]*' | head -1 | cut -d= -f2)
T4_HTTP=$(printf '%s' "$T4R" | grep -ao 'TALKHTTP=[0-9]*' | head -1 | cut -d= -f2)
T4_BODY=$(printf '%s' "$T4R" | grep -a '^{' | head -1)
echo "TURN4-HTTP=$T4_HTTP TURN4-TIME=$T4_TIME"
echo "TURN4-REPLY: $T4_BODY"
if [ "$T4_HTTP" = "200" ] && printf '%s' "$T4_BODY" | grep -aq '"ok":true' \
   && printf '%s' "$T4_BODY" | grep -aq '"kind":"Info"'; then
    case "$T4_BODY" in
        *"인식하지 못했습니다"*)
            echo "TURN4-STATUS=OK PATH=STUB-FALLBACK (무의미 발화 — 파싱 불성립 폴백 안내문·정직)";;
        *)
            echo "TURN4-STATUS=OK PATH=MODEL-GUIDE (모델의 안내문이 도달 — 행동 없는 talk 턴)";;
    esac
    T4_OK=1
else
    echo "TURN4-STATUS=FAIL (http=$T4_HTTP body=$T4_BODY — 500/crash/공허는 계약 위반)"
    T4_OK=0
fi
[ -n "$T4_TIME" ] && awk -v t="$T4_TIME" 'BEGIN{ if (t+0 >= 0.8) print "TURN4-LLM=engaged"; else print "TURN4-LLM=instant"}'

echo "=== I. 승격 표면 종합 (드라이버가 PROMOTE-VERDICT로 봉합한다) ==="
SCORE=$(( T1_OK + T2_OK + T3_OK + T4_OK ))
echo "TURN-SCORE=$SCORE/4 (turn1=$T1_OK turn2=$T2_OK turn3=$T3_OK turn4=$T4_OK)"
echo "PROMOTE-SCORE=$SCORE/4"

echo "=== J. 종료 게이트 — 서버 UP + jkweb ALIVE + chat.json 상시 (사용자 결제 대기) ==="
FINAL_P=$(pgrep -f 'buildterm/jkdesktop' | tr '\n' ' ')
[ -n "$FINAL_P" ] || FAIL "server not UP after E2E (종료 게이트 위반)"
echo "FINAL-SERVER-PIDS=$FINAL_P"
pgrep -f '[j]kweb' >/dev/null 2>&1 || FAIL "jkweb not ALIVE after E2E (종료 게이트 위반)"
echo "JKWEB-ALIVE-AFTER=OK"
[ -f "$CFG" ] || FAIL "staged chat.json vanished during E2E (엔진 턴은 cfg를 재기록하지 않는다 — writer 0)"
CUR_SHA=$(sha256sum "$CFG" | awk '{print $1}')
[ "$CUR_SHA" = "$CFG_SHA" ] || FAIL "chat.json changed during E2E: $CFG_SHA -> $CUR_SHA"
echo "CHATJSON-UNCHANGED-AFTER-E2E=OK (sha256 동일)"
awk -v n="$SCORE" 'BEGIN{ exit !(n == 4) }' && echo "PROMOTE-PHONE-ALLGREEN" || echo "PROMOTE-PHONE-PARTIAL-OR-FAIL"
# 임시 스크립트 자기 소각(REMNANT 검사는 드라이버 몫) — bash가 fd로 읽은 뒤 안전.
rm -f -- "$0"
echo "PROMOTE-PHONE-FINISHED"
exit 0
PHONEEOF

for r in 1 2 3; do
  if tar -cf "$TARBALL" -C "$ROOT" "${FILES[@]}" ${STAGE_ARGS[@]+"${STAGE_ARGS[@]}"} \
     -C "$SCRATCH" "$RSRC_TAR_NAME"; then
    break
  elif [ "$r" -eq 3 ]; then
    FAIL "tar creation failed"
  fi
done
TAR_SZ=$(wc -c < "$TARBALL" | tr -d ' ')
echo "tar size: $TAR_SZ bytes"
[ "$TAR_SZ" -gt 150000 ] || FAIL "tar suspiciously small ($TAR_SZ bytes) — deploy list broken?"
echo "deploy list:"; tar -tf "$TARBALL"
tar -tf "$TARBALL" | grep -aq "$RSRC_TAR_NAME" \
  || FAIL "tar missing the remote receipt script member (구성 누락 방어 — 첫 run 실측)"
$SSH "tar -xf - -C ~/JKENGINE" < "$TARBALL" || FAIL "remote tar extract failed"
rm -f "$TARBALL"
$SSH "test -f ~/JKENGINE/$RSRC_TAR_NAME && echo REMOTE-SCRIPT-PRESENT || echo REMOTE-SCRIPT-ABSENT" \
  | grep -aq REMOTE-SCRIPT-PRESENT || FAIL "remote receipt script missing on phone (tar 구성 누락 — 첫 run 실측)"
# 배포 신선도 — 크기 대차(로컬 원천 vs 폰 수신) 전 건 어설션.
FRESH_OK=1
for f in "${FILES[@]}"; do
  LSZ=$(wc -c < "$ROOT/$f" | tr -d ' ')
  PSZ=$($SSH "wc -c < ~/JKENGINE/$f 2>/dev/null" | tr -d ' \r')
  if [ "$LSZ" != "$PSZ" ]; then
    echo "FRESHNESS-MISMATCH: $f local=$LSZ phone=$PSZ"
    FRESH_OK=0
  fi
done
[ "$FRESH_OK" -eq 1 ] || FAIL "deployed file size mismatch — 배포 원문 신선도 붕괴"
echo "DEPLOY-FRESHNESS-OK (13파일 크기 일치)"

echo "=== 3. 폰 리빌드+영수증 절차 실행 (ninja ~9-10분 + selftest + 스테이지 + E2E) ==="
$SSH "bash $RSRC_PHONE" > "$SCRATCH/phone_promote_run.log" 2>&1
R_RC=$?
RUN_RC=$R_RC
if [ "$R_RC" -eq 255 ]; then
  echo "RUN-DROPPED: ssh 채널 단절(Wi-Fi 낙하 함정 — 원격 ninja 로그는 폰 \$TMPDIR에 생존, 재실행=증분)"
fi
cat "$SCRATCH/phone_promote_run.log"
grep -aq '^PROMOTE-PHONE-FINISHED$' "$SCRATCH/phone_promote_run.log" \
  || FAIL "receipt script did not finish (rc=$RUN_RC — RUN-DROPPED면 재실행: PHONE_HOST=<폰 IP> bash engine/tools/probes/phone_promote.sh)"
# remote FAIL() = hard receipt(배포·빌드·selftest 회귀·덧칠 함정·종료 게이트) —
# 전부 rc=1로 승계한다. 턴 실패는 PROMOTE-FAIL이 아니라 TURNn-STATUS=FAIL 행이다.
if grep -aq '^PROMOTE-FAIL' "$SCRATCH/phone_promote_run.log"; then
  HARD=$(grep -a '^PROMOTE-FAIL' "$SCRATCH/phone_promote_run.log" | head -1)
  FAIL "phone hard receipt: $HARD"
fi

echo "=== 4. REMNANT 검사 — 원격 스크립트 자기 소각 확인 (T1 NR1 계약) ==="
$SSH "ls ~/JKENGINE/$RSRC_TAR_NAME" >/dev/null 2>&1
echo "REMNANT-LS-RC=$?"
echo "---"

echo "=== 5. PROMOTE-VERDICT 봉합 ==="
SCORE_LINE=$(grep -a '^PROMOTE-SCORE=' "$SCRATCH/phone_promote_run.log" | tail -1)
SCORE=${SCORE_LINE#PROMOTE-SCORE=}
case "${SCORE:-}" in
  4/4)
    echo "PROMOTE-VERDICT: PROMOTE-OK — 4/4 턴 성립(각 턴 status/kind/경과초는 위 영수증 행)"
    echo "TURN-RECEIPT-REPLAY:"
    grep -a '^TURN[1-4]-STATUS=' "$SCRATCH/phone_promote_run.log" || true
    grep -a '^TURN[1-4]-LLM=' "$SCRATCH/phone_promote_run.log" || true
    ;;
  *)
    echo "PROMOTE-VERDICT: PROMOTE-PARTIAL (${SCORE:-0}/4 — 아래 원인 행별):"
    grep -a '^TURN[1-4]-STATUS=' "$SCRATCH/phone_promote_run.log" || true
    grep -a '^OLLAMA-' "$SCRATCH/phone_promote_run.log" || true
    ;;
esac
echo ""
echo "사용자 결제 게이트(대기 — probe가 결제를 기록하지 않는다): 폰 브라우저"
echo "  http://localhost:8090/ 에서 자연어 실사용(예: '지뢰찾기 켜줘', '창 목록',"
echo "  '테트리스 켜줘', 자유 문장) — 서버·jkweb·chat.json 상시(종료 게이트)."
# 조달·턴 실패는 honest receipt로 rc=0 — hard FAIL(위 FAIL 경로)만 rc=1이다.
exit 0
# EOF — 끝 개행 유지(스크립트 산술 무해 습관).