#!/usr/bin/env bash
# 폰 더티프레젠트 재배포+실측 probe (더티프레젠트 라인 T4 — plan
# 2026-10-08-dirty-present, phone_promote.sh 전례 승계 — tar 오염 게이트·
# tar-over-ssh·REMNANT·RUN-DROPPED 재계약 + wsl_dirty_present.sh(T3)의
# run_leg 계측 승계 — /proc utime+stime 대차+[compst] 창 추출).
#
# 영수증 목표:
#   ① tar-over-ssh 재배포 — 더티프레젠트 라인(da69486..bf4e5ab)이 건드린
#      소스 전량(CMake+JKFrameDirty 신설 2+JKCompositor 2+JKWindowServer
#      2+JKDesktopShell 2+main.cpp 쌍둥이+posix_selftest 2)+probe 본체
#      (함정 원장 "신규 입자 전량 동반" — CMake 재생성 사망 방지 docs/81 §3 #6)
#   ② 폰 aarch64 ninja 리빌드(NINJA-RC 전파 — ~9-10분)
#   ③ AppSelfTest 0 failure(s) — **폰 캐논 계보 판정(원장)**: 472=pre-T1
#      (배포 원천 미반영)·489=T1만·495=T1+T2+fix r1(main.cpp 쌍둥이의 1p
#      케이스 23건 동반 — 캐논 상승 분기는 dispatch 원장이 "폰 빌드가 최신
#      원천을 포함하면 상승한다 — 원장이 판정"으로 예고). 1p [PASS] 라인
#      실측수를 함께 인쇄해 소스 반영을 이중 단정.
#   ④ cpustat idle+[compst] A/B 2레그(폰 출하 조건=SW 렌더러 기본 —
#      `#ifdef __ANDROID__` 힌트 docs/78 §5.7 ④):
#        legA(신판 부분 — 기본 조건, 무환경): 커서 블링크 커밋이 부분 경로로
#          내려가는지 = [compst] present=dirty(N) 실측. 화면/표면 면적 비가
#          40% 역치(JKFrameDirty.h kFullFrameThreshold)를 넘으면 full로 접힘
#          (T3 leg1 실증) — 그때는 420x300 축소 재실측(legA2 — T3 legA 선례).
#        legB(구판 강제 — JK_PRESENT_FORCE_FULL=1, legA와 동일 기하):
#          사다리 두 판의 공정 비교(렌더러·기하 동일).
#        legFS(fit-scale 관찰 — best-effort): vector.jkx(1920x1080 설계 창 —
#          폰 fit-scale 레이어) 기동 12s 창 — docs/78 §5.7 함정 원장(SW
#          렌더러 선형 필터 우려)의 [compst] 근료. launch 실패=정직 인쇄·계속.
#        legC(최종 출하 상태 — 부분 기본, 무 FORCE_FULL): 서버 UP+터미널 상시
#          — **사용자 육안 결제 게이트 대기 상태**(probe가 결제를 기록하지
#          않는다).
#   ⑤ idle 소등 영수증 — docs/78 §5.7 폰 기준선 34.4%(합성 3/s=present ~70ms
#      전체 업로드 지분) 대비 legA(부분, 출하 조건)의 idle %. 부분/전체
#      동조건 사다리(legA vs legB)가 공정 A/B, 기준선 대비는 조성 차(기준선은
#      probe 앱 시계 포함) 유의 행이 동반된다.
#   판정 사다리(라벨 계약 — T3 승계):
#     DIRTY-VERDICT: DIRTY-OK  = 유효 legA의 present mean < legB full mean AND
#                   legA idle ≤ legB idle + 0.5pp(측정 소음) — rc=0
#     DIRTY-VERDICT: DIRTY-FAIL(행별 이유) — 수치 미달=정직 원장 rc=0
#     DP-PHONE-FAIL(rc=1) = hard FAIL만: ssh 단절·배포 실패·NINJA-RC≠0·
#                   selftest 회귀·서버/ping/터미널 기동 실패·compst 무음·
#                   원격 스크립트 잔존.
#   실행법(윈도 Git Bash, 저장소 루트 어디서든):
#     PHONE_HOST=<폰 IP> bash engine/tools/probes/phone_dirty_present.sh
#   접속 정보는 환경변수로(PHONE_HOST 필수 — 내부 IP는 커밋·문서·리포트에 두지
#   않는다, docs/81 §6 원문; PHONE_PORT PHONE_USER PHONE_KEY PHONE_DISPLAY만
#   기본값). 풀 로그: engine/tmp/phone_dirty_present.log(드라이버 전체 — tee).
#
# 함정 원장(승계+신규):
#   · tar는 저장소 루트에서 만들어 `-C ~/JKENGINE`으로 풀어야 한다(engine/
#     engine 중첩 트랩 — phone_promote 승계). 폰 /tmp는 쓰기 불가 — 폰 측
#     스크래치는 $TMPDIR.
#   · 배포 오염 게이트(docs/81 §3 #11): 더러운(미커밋) 배포 원천은 HEAD blob
#     스테이징(git show HEAD:$f)으로 배포 — probe 본체(자기 파일)만 예외.
#     스테이지 원문은 타르 끝에 추가(-C)해 추출 순서상 나중 항목이 이긴다.
#   · 생존 바이너리 relink = ETXTBSY — 드라이버 pre-clean(브래킷 pkill+-9
#     에스컬레이션)이 tar 앞. 원격 스크립트는 부팅마다 재정리.
#   · 폰 클라 스폰 15-18s(docs/80 원장) — list_windows 3s 간격 최대 20회
#     재시도(WSL 선례의 5회보다 넉넉).
#   · 폰 서버 창(디바이스 화면) 크기 검출 도구가 폰에 없다(xrandr/xdpyinfo/
#     dumpsys/ wm 전부 부재 실측) — SCREEN-DETECT=UNKNOWN이면 역치 비율을
#     미리 산치 못한다: 행동 영수증(legA dirty 라인 유무)이 사다리 판정이고
#     접힘 시 legA2 축소 폴백이 대응한다. TERMINAL-GEOM(표면 dw/dh)은 전사
#     원장료로 인쇄 — 화면 크기 확인이 되는 세션에서 역치 비율 역계산 가능.
#   · ssh 원격 명령에 로컬 `< /dev/null` 금지(채널 hang — 구계약 승계).
#   · rc 봉합: 원격 복합문이 echo로 끝나면 종료코드가 항상 0 — exit로 전파,
#     `|| true` 금지(정수 카운트용 grep -c는 0건 rc=1도 의도된 값 — 주석).
#   · tar 전송은 LAN 내부 ssh 한정 — 어떤 클라우드/외부로도 가지 않는다.
#   · pkill 브래킷 '[j]kdesktop'(자기 grep 매칭 금지), 원격 임시 변수=$TMPDIR,
#     원격 스크립트 자기 소각(rm -f -- "$0")+드라이버 REMNANT 검사(REMNANT-LS-RC=2
#     기대 — T1 NR1 이행).
#   · FORCE_FULL env는 컴포지터 1회 판정(static) — 레그마다 프로세스 재부팅이
#     필수(T3 선례). skip 계약(T2)은 FORCE_FULL에서도 남는다(더티 0 프레임
#     스킵 제거 아님) — [compst] skip 라인은 전 레그에서 세어 혼입을 명시.
#   · 폰 기준선 34.4%는 probe 앱 시계(폰probe) 포함 조성이었다 — 본 probe는
#     터미널+태스크바만이라 절대비 직결 금지, legA vs legB(동조건)가 공정 판.
set -u
export MSYS_NO_PATHCONV=1  # MSYS가 ssh 인수의 posix 경로를 찢는 것 차단(chat T8 선례)

ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
cd "$ROOT" || exit 1
SCRATCH="$ROOT/engine/tmp"
mkdir -p "$SCRATCH"
TARBALL="$SCRATCH/phone_dirty_present.tar"
RLOG="$SCRATCH/phone_dirty_present.log"
RUNLOG="$SCRATCH/phone_dirty_present_run.log"
RSRC_TAR_NAME="phone_dirty_present_remote.sh"
RSRC_PHONE="~/JKENGINE/$RSRC_TAR_NAME"   # 폰 측 삭제는 remote script 자기 소각

# 접속 정보는 환경변수로(PHONE_HOST 필수 — 기본값 금지: 내부 IP는 커밋·문서·
# 리포트에 두지 않는다).
PHONE_HOST="${PHONE_HOST:-}"
PHONE_PORT="${PHONE_PORT:-8022}"
PHONE_USER="${PHONE_USER:-u0_a4}"
PHONE_KEY="${PHONE_KEY:-$HOME/.ssh/termux_jkengine}"
PHONE_DISPLAY="${PHONE_DISPLAY:-:1}"
[ -n "$PHONE_HOST" ] || { echo "DP-PHONE-FAIL: PHONE_HOST unset — 폰 IP를 환경변수로 지정하세요 (내부 IP는 커밋하지 않는다)"; exit 1; }
SSH="ssh -p $PHONE_PORT -o BatchMode=yes -o ConnectTimeout=15 -o ServerAliveInterval=15 -o ServerAliveCountMax=10 -i $PHONE_KEY $PHONE_USER@$PHONE_HOST"

# 풀 로그(engine/tmp/phone_dirty_present.log) — 드라이버 전체(tee; 원격 run은
# RUNLOG로 한벌 승계 — remote 스크립트의 exit 어설션이 이 파일을 먹지 않게).
exec > >(tee "$RLOG") 2>&1

FAIL() { echo "DP-PHONE-FAIL: $*"; exit 1; }

echo "HEAD: $(git -C "$(cygpath -w "$ROOT" 2>/dev/null || echo "$ROOT")" rev-parse HEAD 2>/dev/null || echo rev-parse-failed)"

# ── 배포 원천: 더티프레젠트 라인(da69486..bf4e5ab)이 건드린 소스 전량 —
#    신규 입자 전량 동반(CMake 재생성 사망 방지 — docs/81 §3 #6). T3 probe
#    (wsl_dirty_present.sh)는 폰 출하 원천 아님 — 미포함.
FILES=(
  engine/CMakeLists.txt                      # T1 — JKFrameDirty.cpp 빌드 입자(신규 소스 동반 필수)
  engine/include/server/JKFrameDirty.h       # T1 신설 — 순수 계산기 계약
  engine/src/server/JKFrameDirty.cpp         # T1 신설
  engine/include/server/JKCompositor.h       # T2+fix r1 — 스레드 규약+QueueCommitRects
  engine/src/server/JKCompositor.cpp         # T2+fix r1 — 제시 사다리+PresentPartialSurface
  engine/include/server/JKWindowServer.h     # T2 — overlayDrewThisFrame_ 멤버
  engine/src/server/JKWindowServer.cpp       # T2+fix r1 — CommitSurface rect 수집+UpdateOutputBounds
  engine/include/desktop/JKDesktopShell.h    # T2 fix r1 — onDynamicDraw 토크백(툴팁 사건화)
  engine/src/desktop/JKDesktopShell.cpp      # T2 fix r1 — 툴팁 전이 rect 사건
  engine/src/main.cpp                        # T1/T2 쌍둥이 — 1p 케이스 23건(폰 selftest 캐논 원료)
  engine/tools/posix_selftest/main.cpp       # T1/T2 posix 쌍둥이+chat T4 fix r2 원천(포함 여부 원장)
  engine/tools/posix_selftest/build.sh       # T1 어댑터 링크(신선도 동반)
  engine/tools/probes/phone_dirty_present.sh # probe 본체(폰에 원문 유산 — 게이트 예외)
)

for f in "${FILES[@]}"; do
  [ -f "$ROOT/$f" ] || FAIL "deploy source missing: $f"
done

echo "=== 0. 배포 오염 게이트 (docs/81 §3 #11 — 더러운 원천은 HEAD blob 스테이징) ==="
# git 네이티브(git.exe) 인수 경로 — MSYS_NO_PATHCONV=1 하에서도 유효한 Windows형
# 경로(cygpath; phone_apps.sh run 5a/5b 실측 선례).
GITROOT="$(cygpath -w "$ROOT" 2>/dev/null || echo "$ROOT")"
# 정수 카운트용 grep -c는 rc=1(0건)도 의도된 값 — `|| true` 대신 case 분기.
DIRTY_COUNT=$(git -C "$GITROOT" status --porcelain 2>/dev/null | grep -avc '^??')
echo "tracked-dirty-count=$DIRTY_COUNT (tracked 변경 0이면 배포 원천=워킹 카피 원문)"
STAGE_DIR=""
WIPPED=0
STAGE_ARGS=()
for f in "${FILES[@]}"; do
  [ "$f" = "engine/tools/probes/phone_dirty_present.sh" ] && continue   # 자기 파일 예외
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
  WIPPED=$((WIPPED+1))
  STAGE_ARGS+=(-C "$STAGE_DIR" "$f")
done
if [ "$WIPPED" -gt 0 ]; then
  echo "NOTE-WIP: 더러운 배포 원천 $WIPPED건 — HEAD blob 스테이징으로 배포"
else
  echo "CONTAMINATION-GATE=CLEAN (배포 원천 전량 HEAD와 일치 — 워킹 카피 배포)"
fi

echo "=== 1. pre-flight + pre-clean (재실행 가능성 — wake-lock, 서버 절사) ==="
$SSH 'echo PHONE-REACHABLE; uname -m; echo TMPDIR=$TMPDIR; command -v termux-wake-lock >/dev/null 2>&1 && termux-wake-lock || echo WARN-no-wake-lock' \
  || FAIL "ssh failed (phone unreachable — LAN 한정, sshd 기동: 폰 Termux에서 sshd)"
# 절사 대상=서버+클라 전부(구판 바이너리 — relink ETXTBSY 방지+빌드 램 여유).
# jkweb은 절사하지 않는다(승격 라인 사용자 결제 창 localhost:8090 상시 계약).
$SSH "pkill -f '[j]kdesktop' 2>/dev/null; sleep 2; pkill -9 -f '[j]kdesktop' 2>/dev/null; sleep 1; pgrep -f '[j]kdesktop' >/dev/null 2>&1 && { echo PRECLEAN-FAIL; exit 1; }; echo SRV-DOWN" \
  || FAIL "pre-clean could not bring the old phone server down"
echo "PRECLEAN-SRV-DOWN=OK (구판 서버+클라 — 새 바이너리 교체 재기동 계약)"

echo "=== 2. tar-over-ssh 재배포 (13파일 — 라인 소스 전량 + probe) ==="
cat > "$SCRATCH/$RSRC_TAR_NAME" <<'PHONEDPEOF'
#!/bin/bash
# 폰 측 영수증 절차 (phone_dirty_present.sh가 생성 — 더티프레젠트 T4).
# 종료 상태 = 서버 UP(부분 출하 조건)+터미널 상시 — 사용자 육안 결제 대기.
set -u
cd ~/JKENGINE/engine || exit 1
FAIL() { echo "DP-FAIL: $*"; exit 1; }
TMPD="${TMPDIR:-/data/data/com.termux/files/usr/tmp}"
NLOG="$TMPD/dp_ninja.log"
STLOG="$TMPD/dp_selftest.log"
CLK=$(getconf CLK_TCK 2>/dev/null)
[ -n "$CLK" ] || CLK=100
RES="$TMPD/dp_res.txt"
: > "$RES"

echo "=== A. screen detect (best-effort — 폰 X11 도구 다부재 실측) ==="
SCR=""
if command -v xrandr >/dev/null 2>&1; then
    SCR=$(DISPLAY=:1 xrandr 2>/dev/null | grep -aoE 'current [0-9]+ x [0-9]+' | head -1)
    [ -n "$SCR" ] && echo "SCREEN-XRANDR=$SCR"
fi
if [ -z "$SCR" ] && command -v xdpyinfo >/dev/null 2>&1; then
    SCR=$(DISPLAY=:1 xdpyinfo 2>/dev/null | grep -a '^dimensions' | head -1)
    [ -n "$SCR" ] && echo "SCREEN-XDPYINFO=$SCR"
fi
[ -n "$SCR" ] || command -v wm >/dev/null 2>&1 || echo "SCREEN-DETECT=UNKNOWN (역치 비율 사전 산치 불가 — 행동 영수증+legA2 축소 폴백이 판정한다)"
wm size 2>/dev/null | head -2

echo "=== B. ninja rebuild (aarch64 — rc 전파, ~9-10분) ==="
ninja -C buildterm -j4 >"$NLOG" 2>&1
N_RC=$?
tail -4 "$NLOG"
echo "NINJA-RC=$N_RC"
[ "$N_RC" -eq 0 ] || FAIL "ninja rebuild rc=$N_RC (aarch64 compile failure)"
[ -x buildterm/jkdesktop ] || FAIL "buildterm/jkdesktop missing after rebuild"

echo "=== C. selftest — 폰 캐논 계보 판정 (원장: 472=pre-T1·489=T1만·495=T1+T2+fix r1) ==="
timeout 900 ./buildterm/jkdesktop test >"$STLOG" 2>&1
S_RC=$?
ST_PASS=$(grep -ac '^\[PASS\]' "$STLOG")
ST_FAIL=$(grep -ac '^\[FAIL\]' "$STLOG")
P1P=$(grep -ac '^\[PASS\] 1p-' "$STLOG")
echo "PHONE-SELFTEST rc=$S_RC PASS=$ST_PASS FAIL=$ST_FAIL 1p-PASS=$P1P (배포된 1p 케이스=23건)"
grep -a 'AppSelfTest' "$STLOG" | tail -2
grep -aq 'AppSelfTest: 0 failure(s)' "$STLOG" || FAIL "AppSelfTest not 0 failure(s)"
[ "$S_RC" -eq 0 ] || FAIL "selftest rc=$S_RC"
[ "$ST_FAIL" -eq 0 ] || FAIL "selftest FAIL=$ST_FAIL (폰축 회귀)"
[ "$P1P" -eq 23 ] || echo "WARN: 1p PASS=$P1P != 배포 케이스 23 — 소스 반영 계보 요원장"
awk -v p="$ST_PASS" 'BEGIN{
    if (p == 495) print "CANON-INCLUSION=FULL-T1-T2-FIXR1 (캐논 472→495 상승 — main.cpp 쌍둥이 1p 23건 동반 실측)"
    else if (p == 489) print "CANON-INCLUSION=T1-ONLY (T2 원천 계보 결손 — 원장)"
    else if (p == 472) print "CANON-INCLUSION=STALE-PRE-T1 (배포 원천이 셀프테스트 쌍둥이에 미반영 — 신선도 의심 — 원장)"
    else printf "CANON-INCLUSION=OTHER-N(%d — 계보 미부합 원장)\n", p
}'

# ---------------------------------------------------------------- 계측 레그
# ml 라벨 env_extra(빈도 가능) GEOM("default"|"resized") — T3 run_leg 승계:
# /proc utime+stime 대차(idle %, Δ=/proc — ps %cpu는 수명 평균 docs/78 §5.5
# 렛슨)+[compst] 22s 창(8합성당 1인쇄 — 폰 3합성/s ≈ 라인 6-8개).
ml() {
    LEG=$1; ENVX=$2; GEOM=$3
    LOG="$TMPD/dp_srv_${LEG}.log"
    echo "=== leg $LEG boot (geom=$GEOM $ENVX) ==="
    pkill -f '[j]kdesktop' 2>/dev/null
    sleep 1
    pkill -9 -f '[j]kdesktop' 2>/dev/null
    sleep 1
    env DISPLAY=:1 JK_CPU_TRACE=1 $ENVX setsid nohup ./buildterm/jkdesktop --server >"$LOG" 2>&1 &
    sleep 8
    SRVPID=$(pgrep -f 'buildterm/[j]kdesktop --server' | head -1)
    [ -n "$SRVPID" ] \
        || { echo "--- leg $LEG log tail:"; tail -8 "$LOG"; FAIL "leg $LEG: no server 8s after boot"; }
    echo "leg $LEG: server pid=$SRVPID"
    PING=""
    for i in 1 2 3 4 5; do
        PING=$(timeout 15 ./buildterm/jkdesktop agentctl '{"tool":"ping","args":{}}' 2>/dev/null | grep -a '{' | head -1)
        printf '%s' "$PING" | grep -aq '"ok":true' && break
        sleep 2
    done
    printf '%s' "$PING" | grep -aq '"ok":true' || FAIL "leg $LEG: ping did not ok — $PING"
    L=$(timeout 15 ./buildterm/jkdesktop agentctl '{"tool":"launch_app","args":{"app":"terminal"}}' 2>/dev/null | grep -a '{' | head -1)
    printf '%s' "$L" | grep -aq '"ok":true' || FAIL "leg $LEG: launch_app terminal failed — $L"
    TERM_ID=""
    TW=""
    for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20; do
        WIN=$(timeout 15 ./buildterm/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)
        TW=$(printf '%s' "$WIN" | grep -aoE '\{"id":[^}]*"title":"Terminal"[^}]*\}' | head -1)
        [ -n "$TW" ] && break
        sleep 3
    done
    [ -n "$TW" ] || FAIL "leg $LEG: no Terminal window (60s 대기 — 폰 스폰 15-18s 원장 초과) — last: ${WIN:-none}"
    TERM_ID=$(printf '%s' "$TW" | sed -n 's/.*"id":\([0-9]*\),.*/\1/p')
    echo "TERMINAL-GEOM-$LEG: $TW"
    if [ "$GEOM" = "resized" ]; then
        R=$(timeout 15 ./buildterm/jkdesktop agentctl \
            '{"tool":"window_resize","args":{"id":'"$TERM_ID"',"w":420,"h":300}}' 2>/dev/null | grep -a '{' | head -1)
        printf '%s' "$R" | grep -aq '"ok":true' || FAIL "leg $LEG: window_resize(420x300) failed — $R"
        echo "leg $LEG: terminal resized 420x300 — settle 3s"
        sleep 3
    else
        echo "leg $LEG: 기본 기하 유지 — settle 5s"
        sleep 5
    fi

    O1=$(wc -c < "$LOG")
    D1=$(awk '{print $14+$15}' /proc/$SRVPID/stat 2>/dev/null)
    [ -n "$D1" ] || FAIL "leg $LEG: /proc/$SRVPID/stat unreadable"
    C1=$(for p in $(pgrep -f '[j]kdesktop'); do
             [ "$p" != "$SRVPID" ] && awk '{print $14+$15}' /proc/$p/stat 2>/dev/null
         done | awk '{s+=$1} END{print s+0}')
    sleep 22
    O2=$(wc -c < "$LOG")
    D2=$(awk '{print $14+$15}' /proc/$SRVPID/stat 2>/dev/null)
    [ -n "$D2" ] || FAIL "leg $LEG: server died mid-window (stat t2)"
    C2=$(for p in $(pgrep -f '[j]kdesktop'); do
             [ "$p" != "$SRVPID" ] && awk '{print $14+$15}' /proc/$p/stat 2>/dev/null
         done | awk '{s+=$1} END{print s+0}')
    IS=$(awk -v d=$((D2-D1)) -v hz="$CLK" 'BEGIN{printf "%.1f", d/hz/22*100}')
    IC=$(awk -v d=$((C2-C1)) -v hz="$CLK" 'BEGIN{printf "%.1f", d/hz/22*100}')

    tail -c +"$((O1+1))" "$LOG" | head -c "$((O2-O1))" >"$TMPD/dp_win.txt"
    echo "--- leg $LEG [compst] window (22s, 원문 전문) ---"
    cat "$TMPD/dp_win.txt"
    COMP_TOTAL=$(grep -ac 'compst' "$TMPD/dp_win.txt")
    [ "$COMP_TOTAL" -gt 0 ] \
        || FAIL "leg $LEG: zero [compst] lines in 22s window (JK_CPU_TRACE 무음 — 계측 파열)"
    SKIP_CNT=$(grep -ac 'present=skip' "$TMPD/dp_win.txt")
    FULL_CNT=$(grep -ac 'present=full' "$TMPD/dp_win.txt")
    DIRTY_CNT=$(grep -ac 'present=dirty(' "$TMPD/dp_win.txt")
    COMPSTAT=$(grep -a 'cpustat] sdl' "$TMPD/dp_win.txt" | tail -3 | tr '\n' '|')
    FULL_MS=$(grep -a 'present=full=' "$TMPD/dp_win.txt" | sed -n 's/.*present=full=\([0-9.]*\).*/\1/p')
    DIRTY_MS=$(grep -a 'present=dirty(' "$TMPD/dp_win.txt" | sed -n 's/.*present=dirty([0-9]*)=\([0-9.]*\).*/\1/p')
    RECOUNT=$(grep -ao 'present=dirty([0-9]*)' "$TMPD/dp_win.txt" | sed -n 's/.*dirty(\([0-9]*\)).*/\1/p' | awk '{s+=$1} END{print s+0}')
    echo "LEG-$LEG: IDLE_SRV=${IS}% IDLE_CLT=${IC}% SKIP=$SKIP_CNT FULL=$FULL_CNT DIRTY=$DIRTY_CNT RECTS=$RECOUNT"
    echo "LEG-$LEG: cpustat: $COMPSTAT"
    # 메트릭별 분리 행(판정부 파싱 — T3 렛슨: 한 행에 여수치+접두 sed는
    # 공백 뒤를 못 자른다 — 드라이버가 RUNLOG에서 정규 파싱한다).
    FSTAT=$(printf '%s' "$FULL_MS" | awk 'BEGIN{n=0;s=0;m=0} {n++;s+=$1;if($1>m)m=$1} END{if(n)printf "%.2f %.2f %d",s/n,m,n; else printf "0 0 0"}')
    DSTAT=$(printf '%s' "$DIRTY_MS" | awk 'BEGIN{n=0;s=0;m=0} {n++;s+=$1;if($1>m)m=$1} END{if(n)printf "%.2f %.2f %d",s/n,m,n; else printf "0 0 0"}')
    echo "METRIC-$LEG IDLE_SRV=$IS"
    echo "METRIC-$LEG IDLE_CLT=$IC"
    echo "METRIC-$LEG SKIP=$SKIP_CNT"
    echo "METRIC-$LEG FULL=$FULL_CNT"
    echo "METRIC-$LEG DIRTY=$DIRTY_CNT"
    echo "METRIC-$LEG RECTS=$RECOUNT"
    echo "METRIC-$LEG FSTAT=$FSTAT"
    echo "METRIC-$LEG DSTAT=$DSTAT"
    echo "$LEG IDLE_SRV=$IS" >>"$RES"
    echo "$LEG IDLE_CLT=$IC" >>"$RES"
    echo "$LEG SKIP=$SKIP_CNT" >>"$RES"
    echo "$LEG FULL=$FULL_CNT" >>"$RES"
    echo "$LEG DIRTY=$DIRTY_CNT" >>"$RES"
    echo "$LEG RECTS=$RECOUNT" >>"$RES"
    echo "$LEG FSTAT=$FSTAT" >>"$RES"
    echo "$LEG DSTAT=$DSTAT" >>"$RES"
}

# ---------------------------------------------------------------- 레그 실행
echo "=== D. legA — 부분 기본(폰 출하 조건, 무 FORCE_FULL) ==="
ml legA "" default
DIRTY_A=$(grep -a '^legA DIRTY=' "$RES" | head -1 | sed 's/.*DIRTY=//' | awk '{print $1}')
EFFECTIVE="legA"
GEOM="default"
if [ "${DIRTY_A:-0}" -eq 0 ]; then
    echo "NOTE-THRESHOLD: legA dirty 라인 0 — 표면/화면 면적 비가 40% 역치를 넘어 full 접힘(T3 leg1 실증 동형)"
    echo "                 → 축소 재실측 legA2 (420x300 — T3 legA 선례)"
    ml legA2 "" resized
    DIRTY_A2=$(grep -a '^legA2 DIRTY=' "$RES" | head -1 | sed 's/.*DIRTY=//' | awk '{print $1}')
    if [ "${DIRTY_A2:-0}" -gt 0 ]; then
        EFFECTIVE="legA2"
        GEOM="resized"
    fi
fi

echo "=== E. legB — 구판 강제(JK_PRESENT_FORCE_FULL=1, 동일 기하 $GEOM) ==="
if [ "$GEOM" = "resized" ]; then
    ml legB "JK_PRESENT_FORCE_FULL=1" resized
else
    ml legB "JK_PRESENT_FORCE_FULL=1" default
fi

echo "=== F. legFS — fit-scale 관찰 (vector.jkx 1920x1080 설계 창 — best-effort) ==="
V=$(timeout 20 ./buildterm/jkdesktop agentctl '{"tool":"launch_app","args":{"jkx":"vector"}}' 2>/dev/null | grep -a '{' | head -1)
printf '%s' "$V" | grep -aq '"ok":true' || echo "legFS: vector.jkx launch 미성립(정직 인쇄 — 계속) — $V"
if printf '%s' "$V" | grep -aq '"ok":true'; then
    VW=""
    for i in 1 2 3 4 5 6 7 8; do
        sleep 3
        WIN=$(timeout 15 ./buildterm/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)
        VW=$(printf '%s' "$WIN" | grep -aoE '\{"id":[^}]*"title":"Vector[^}]*\}' | head -1)
        [ -n "$VW" ] && break
    done
    echo "VECTOR-WINDOW=$VW"
    if [ -n "$VW" ]; then
        echo "--- legFS [compst] window (12s, 기존 legB 서버 위 — FORCE_FULL 주의, 원문 전문) ---"
        OFF1=$(wc -c < "$TMPD/dp_srv_legB.log")
        sleep 12
        OFF2=$(wc -c < "$TMPD/dp_srv_legB.log")
        tail -c +"$((OFF1+1))" "$TMPD/dp_srv_legB.log" | head -c "$((OFF2-OFF1))" | grep -a 'compst' || echo "(no compst lines in 12s window)"
    fi
fi

echo "=== G. legC(final) — 최종 출하 상태(부분 기본, 무 FORCE_FULL) — 서버 UP 상시(사용자 육안 게이트) ==="
# legFS에서 띄운 vector 창도 이 부팅의 pkill로 함께 소각 — 최종 창 상태는 태스크바+터미널.
ml final "" default

echo "=== H. 종료 게이트 — 서버 UP(사용자 결제 대기, probe가 결제를 기록하지 않는다) ==="
FINAL_P=$(pgrep -f 'buildterm/[j]kdesktop --server' | tr '\n' ' ')
[ -n "$FINAL_P" ] || FAIL "server not UP at end (종료 게이트 위반)"
echo "FINAL-SERVER-PIDS=$FINAL_P"
echo "END-STATE: 서버 UP+터미널 상시 — 육안 항목: 런처/태스크바/터미널 정상 표시,"
echo "  커서 블링크 정상, 부분 업로드로 인한 깜빡임·끊김·찌꺼기 없음."
awk -v n="$EFFECTIVE" 'BEGIN{ print "EFFECTIVE-LEG=" n }'
rm -f -- "$0"
echo "PHONE-DP-FINISHED"
exit 0
PHONEDPEOF

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
[ "$TAR_SZ" -gt 400000 ] || FAIL "tar suspiciously small ($TAR_SZ bytes) — deploy list broken?"
echo "deploy list:"; tar -tf "$TARBALL"
tar -tf "$TARBALL" | grep -aq "$RSRC_TAR_NAME" \
  || FAIL "tar missing the remote receipt script member (구성 누락 방어)"
$SSH "tar -xf - -C ~/JKENGINE" < "$TARBALL" || FAIL "remote tar extract failed"
rm -f "$TARBALL"
$SSH "test -f ~/JKENGINE/$RSRC_TAR_NAME && echo REMOTE-SCRIPT-PRESENT || echo REMOTE-SCRIPT-ABSENT" \
  | grep -aq REMOTE-SCRIPT-PRESENT || FAIL "remote receipt script missing on phone (tar 구성 누락)"

# 배포 신선도 — 크기 대차(로컬 원천 vs 폰 수신) 전 건 어설션 + 컴포지터 마커.
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
MARK=$($SSH "grep -ac 'PresentPartialSurface' ~/JKENGINE/engine/src/server/JKCompositor.cpp 2>/dev/null")
echo "COMPOSITOR-MARKER=PresentPartialSurface phone-grep-count=$MARK (배포 원천에 더티프레젠트 코드 실존 단정)"
[ "${MARK:-0}" != "0" ] || FAIL "phone JKCompositor.cpp lacks PresentPartialSurface — 배포 원천 결손"

echo "=== 3. 폰 리빌드+영수증 절차 실행 (ninja ~9-10분 + selftest + 레그 4~5) ==="
$SSH "bash $RSRC_PHONE" > "$RUNLOG" 2>&1
R_RC=$?
RUN_RC=$R_RC
if [ "$R_RC" -eq 255 ]; then
  echo "RUN-DROPPED: ssh 채널 단절(Wi-Fi 낙하 함정 — 폰 ninja 로그는 \$TMPDIR에 생존, 재실행=증분: PHONE_HOST=<폰 IP> bash engine/tools/probes/phone_dirty_present.sh)"
fi
cat "$RUNLOG"
grep -aq '^PHONE-DP-FINISHED$' "$RUNLOG" \
  || FAIL "receipt script did not finish (rc=$RUN_RC — RUN-DROPPED면 재실행)"
if grep -aq '^DP-FAIL' "$RUNLOG"; then
  HARD=$(grep -a '^DP-FAIL' "$RUNLOG" | head -1)
  FAIL "phone hard receipt: $HARD"
fi

echo "=== 4. REMNANT 검사 — 원격 스크립트 자기 소각 확인 (T1 NR1 계약) ==="
$SSH "ls ~/JKENGINE/$RSRC_TAR_NAME" >/dev/null 2>&1
echo "REMNANT-LS-RC=$?"

echo "=== 5. DIRTY-VERDICT 봉합 ==="
# 유효 legA = 부분이 실발화한 레그(legA 기본 조건, 접힘이면 legA2 축소).
EFF=$(grep -a '^EFFECTIVE-LEG=' "$RUNLOG" | tail -1 | cut -d= -f2 | tr -d ' \r')
[ -n "$EFF" ] || EFF=none
CANON=$(grep -a '^CANON-INCLUSION=' "$RUNLOG" | tail -1 | sed 's/^CANON-INCLUSION=//' | tr -d '\r')
# PASS 숫자는 "PASS=N FAIL=" 경계로 자른다(.*PASS= 탐욕은 "1p-PASS=23"을 잡는다
# — 판정 파싱 시험 실측 트랩).
STPASS=$(grep -a '^PHONE-SELFTEST' "$RUNLOG" | head -1 | sed -n 's/^PHONE-SELFTEST rc=.* PASS=\([0-9]*\) FAIL.*/\1/p')
if [ "$EFF" = "none" ]; then
  echo "DIRTY-VERDICT: DIRTY-FAIL(부분 경로 미발화 — legA·legA2 어느 쪽에도 present=dirty(N) 라인 0 — [compst] 원문 위)"
  echo "TASKDP-PHONE-END"
  exit 0
fi
# tr -d ' \r' 금지(다중값 행의 공백 구분을 지운다 — 시험 실측 트랩): 개행만
# 제거(CR)는 별값 파서가 못 읽게 하지 않는 한도에서 유지하지 않는다.
IDLEA=$(grep -a "METRIC-$EFF IDLE_SRV" "$RUNLOG" | head -1 | sed 's/.*IDLE_SRV=//;s/%//' | tr -d '\r')
IDLEB=$(grep -a '^LEG-legB:' "$RUNLOG" | head -1 | sed 's/.*IDLE_SRV=//;s/%.*//' | tr -d '\r')
IDLECLTA=$(grep -a "METRIC-$EFF IDLE_CLT" "$RUNLOG" | head -1 | sed 's/.*IDLE_CLT=//;s/%//;s/ .*//' | tr -d '\r')
IDLECLTB=$(grep -a '^LEG-legB:' "$RUNLOG" | head -1 | sed 's/.*IDLE_CLT=//;s/%//;s/ .*//' | tr -d '\r')
FULLCNT=$(grep -a '^LEG-legB:' "$RUNLOG" | head -1 | sed 's/.*FULL=//;s/ .*//' | tr -d ' \r')
# 메트릭별 분리 행(T3 렛슨 — 한 행에 여수치+접두 sed는 공백 뒤를 못 자른다)
DSTATLINE=$(grep -a "METRIC-$EFF DSTAT=" "$RUNLOG" | head -1 | sed "s/^METRIC-$EFF DSTAT=//" | tr -d '\r')
FSTATA=$(grep -a "METRIC-$EFF FSTAT=" "$RUNLOG" | head -1 | sed "s/^METRIC-$EFF FSTAT=//" | tr -d '\r')
FSTATB=$(grep -a '^METRIC-legB FSTAT=' "$RUNLOG" | head -1 | sed 's/^METRIC-legB FSTAT=//' | tr -d '\r')
DMEAN=$(printf '%s' "$DSTATLINE" | awk '{print $1}')
DMAX=$(printf '%s' "$DSTATLINE" | awk '{print $2}')
DN=$(printf '%s' "$DSTATLINE" | awk '{print $3}')
FMEANA=$(printf '%s' "$FSTATA" | awk '{print $1}')
FMEANB=$(printf '%s' "$FSTATB" | awk '{print $1}')
FMAXB=$(printf '%s' "$FSTATB" | awk '{print $2}')
FN=$(printf '%s' "$FSTATB" | awk '{print $3}')
SKIPB=$(grep -a '^LEG-legB:' "$RUNLOG" | head -1 | sed 's/.*SKIP=//;s/ .*//' | tr -d ' \r')

echo "=== A/B 판정 ==="
echo "legA기본(부분 출하): idle=${IDLEA}%(srv) ${IDLECLTA}%(clt) — 기준선 34.4%(docs/78 §5.7, probe 앱 시계 포함 조성)"
echo "legB(FORCE_FULL):    idle=${IDLEB}%(srv) ${IDLECLTB}%(clt) full 표본=$FULLCNT skip=$SKIPB"
if [ "$FMEANB" = "0" ] || [ -z "$FMEANB" ]; then
    echo "DIRTY-VERDICT: DIRTY-FAIL(전체 판 표본 0 — legB present=full 라인 부족)"
elif [ "$DMEAN" = "0" ] || [ -z "$DMEAN" ]; then
    echo "DIRTY-VERDICT: DIRTY-FAIL(부분 ms 표본 0 — 유효 레그 $EFF의 dirty(ms) 파열)"
else
    awk -v dm="$DMEAN" -v fm="$FMEANB" -v ia="${IDLEA:-0}" -v ib="${IDLEB:-0}" \
        -v da="${DN:-0}" -v db="${FN:-0}" -v eff="$EFF" -v stpass="${STPASS:-0}" 'BEGIN{
        if (fm < dm + 0.001) {
            printf "DIRTY-VERDICT: DIRTY-FAIL(present 절감 없음 — dirty mean %.2fms ≥ full mean %.2fms, idle %.1f%% vs %.1f%%)", dm, fm, ia, ib
        } else if (ia > ib + 0.5) {
            save = (fm - dm) / fm * 100
            printf "DIRTY-VERDICT: DIRTY-FAIL(present 절감 %.2f→%.2fms(-%.0f%%)이나 idle 역행 %.1f%% vs %.1f%% — 정직 원장)", fm, dm, save, ia, ib
        } else {
            save = (fm - dm) / fm * 100
            printf "DIRTY-VERDICT: DIRTY-OK(폰 present mean %.2fms→%.2fms 절감 %.2fms(%.0f%%), 표본 dirty %d/full %d, idle %.1f%%(부분) vs %.1f%%(전체), 폰 캐논 PASS=%s — CANON 원장은 상단 CANON-INCLUSION 행)", fm, dm, fm - dm, save, da, db, ia, ib, stpass
        }
    }'
fi
if [ -n "$IDLEA" ]; then
    awk -v ia="$IDLEA" -v base="34.4" 'BEGIN{
        d = base - ia
        printf "IDLE-REDUCTION(vs 34.4%% 기준선): %.1f→%.1f%% — 절감 %.1fpp (기준선은 probe 앱 시계 포함 조성 — 조성 차 행 유의: legA vs legB가 공정 A/B)", base, ia, d
    }'
fi
echo ""
echo "사용자 결제 게이트(대기 — probe가 결제를 기록하지 않는다): 폰 화면(서버 UP+터미널"
echo "  상시)에서 런처·태스크바·터미널 정상 표시+커서 블링크+부분 업로드 인한 깜빡임·"
echo "  끊김·찌꺼기 없음 — \"정상\" 선언만 결제. fit-scale 앱(vector 등)은 별도 눈확인."
echo "TASKDP-PHONE-END"
# honest-fail 원칙: 수치 미달(DIRTY-FAIL)은 원장 목적이라 rc=0.
# hard FAIL(배포·빌드·selftest·기동·계측 무음·REMNANT)만 FAIL()에서 exit 1.
exit 0
# EOF — 끝 개행 유지(스크립트 산술 무해 습관).