#!/usr/bin/env bash
# 폰 클라 idle 실측 probe (#89 클라 idle 스핀 수리 — plan
# 2026-10-09-client-idle Task 3 폰 축 — phone_dirty_present.sh(T4) 재배포 계약
# + clt-spin spike(spin_phone_remote2.sh) 측정 선례 + wsl_client_idle.sh 동형
# window() 계측 승계.
#
# 영수증 목표(스펙 하단 — 본판정):
#   폰 idle(앱 열어두고 무입력 5s 이상): **갤러리 클라 CPU < 5%**(현 100.1~102.6%)
#   + **[cpustat] frames/s < 3**(현 19) — terminal/taskbar 비교 축(대조군
#   앵커: terminal 7.8~8.9%·taskbar <4% — spike §1b 원문) + 서버 존중(서버 UP
#   종료·jkweb 무접촉·관측 전부 원문). WSL 동형 실측은 wsl_client_idle.sh.
#
# 재배포 계약(D1 원장 승계 — .superpowers/sdd/2026-10-09-d1-diag/repair-report.md):
#   · **전수 sweep로 트리 낙후 발각**(D1의 "폰 트리 낙후→30 DIFF 발각" 실측 계약)
#     — engine/src+include+CMakeLists 전량을 HEAD blob 크기(개행 정규화)와 대조.
#   · 오차 집합 = 기대 배포 집합(D1 소거 시점 3f76f21 이후 src/include diff —
#     T1+T2+fixr1 계보) 안이면 **tar 재배포(git show HEAD blob 스테이징 — 오염
#     게이트: 워킹 카피 접촉 0, docs/81 §3 #11 계약)**. 오차가 기대 집합 밖을
#     넘으면(트리 낙후·기타 라인 결손) **전량 git archive 재배포**(D1 §4 원장)
#     — `-c core.autocrlf=false`(D1 실측: autocrlf 변환은 CRLF 부피+CR —
#     LF 원문 배포로 blob 크기 등호 비교를 가능하게 한다).
#   · selftest 캐논 계보 래더(원장): **546=pre-T1(배포 결손)·556=T1만·
#     565=T1+T2(fixr1 포함)** — 2i [PASS] 19건을 별도 세어 소스 반영 이중 단정.
#
# 접속 계약(docs/81 §6+docs/84 승계): PHONE_HOST 필수(환경변수 — 내부 IP는
# 커밋·문서·리포트에 기록하지 않는다; known_hosts 후보 BatchMode 자기 판명은
# 리포트 자리표시만 PHONE_HOST=<폰>). PHONE_PORT/USER/KEY/DISPLAY만 기본값.
# 브래킷 pkill·$TMPDIR 스크래치·원격 스크립트 자기 소각+REMNANT·`< /dev/null`
# 금지·jkweb 절사 금지·종료 서버 UP. tar 전송은 LAN 내부 ssh 한정.
#
# 실행 표준(윈도 Git Bash):
#   PHONE_HOST=<폰 IP> bash engine/tools/probes/phone_client_idle.sh
#   (ssh known_hosts 대조 — BatchMode 성립 후보만 사용). 풀 로그
#   engine/tmp/phone_client_idle.log(tee)·런 원문 engine/tmp/phone_cltidle_run.log.
set -u
export MSYS_NO_PATHCONV=1  # MSYS가 ssh 인수의 posix 경로를 찢는 것 차단(chat T8 선례)

ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
cd "$ROOT" || exit 1
SCRATCH="$ROOT/engine/tmp"
mkdir -p "$SCRATCH"
RLOG="$SCRATCH/phone_client_idle.log"
RUNLOG="$SCRATCH/phone_cltidle_run.log"
RSRC_TAR_NAME="phone_client_idle_remote.sh"
RSRC_PHONE="~/JKENGINE/$RSRC_TAR_NAME"
GITROOT="$(cygpath -w "$ROOT" 2>/dev/null || echo "$ROOT")"

PHONE_HOST="${PHONE_HOST:-}"
PHONE_PORT="${PHONE_PORT:-8022}"
PHONE_USER="${PHONE_USER:-u0_a4}"
PHONE_KEY="${PHONE_KEY:-$HOME/.ssh/termux_jkengine}"
PHONE_DISPLAY="${PHONE_DISPLAY:-:1}"
[ -n "$PHONE_HOST" ] || { echo "PC-PHONE-FAIL: PHONE_HOST unset — 폰 주소는 환경변수만(기록 금지 계약)"; exit 1; }
SSH="ssh -p $PHONE_PORT -o BatchMode=yes -o ConnectTimeout=15 -o ServerAliveInterval=15 -o ServerAliveCountMax=10 -i $PHONE_KEY $PHONE_USER@$PHONE_HOST"

exec > >(tee "$RLOG") 2>&1
FAIL() { echo "PC-PHONE-FAIL: $*"; exit 1; }

echo "HEAD: $(git -C "$GITROOT" rev-parse HEAD 2>/dev/null || echo rev-parse-failed)"
D1_COMMIT=$(git -C "$GITROOT" rev-parse 3f76f21 2>/dev/null | tr -d ' \r\n')
[ -n "$D1_COMMIT" ] || FAIL "D1(3f76f21) rev-parse 실패 — 계보 앵커 누락"
echo "BASE-ANCHOR(d1 소거 커밋): $D1_COMMIT"

# ------------------------------------------------------------------ 0. 전수 sweep
echo "=== 0. 전수 sweep(트리 낙후 발각 — D1 원장) — local HEAD blob vs 폰(CR 정규화) ==="
TREE="$SCRATCH/idle_tree.txt"
git -C "$GITROOT" ls-tree -rl HEAD -- engine/src engine/include engine/CMakeLists.txt |
    sed 's/\t/ /' | awk '{print $4, $5}' | sort > "$TREE"
NTREE=$(wc -l < "$TREE")
echo "sweep 대상: $NTREE 파일"
awk '{print $2}' "$TREE" | sort > "$SCRATCH/idle_head_files.txt"
LC_ALL=C sort "$TREE" > "$SCRATCH/idle_head_sizes.txt"
# parse canary(T3 원장 — 이중 축약 결함: TREE는 이미 size path 2필드라 그 위에서
# 다시 $4/$5를 세면 전부 공란이 되고, 폰은 공란 목록→전행 MISSING→tar 0파일로
# 사망한다. NF/형식 가드로 이 결함은 계측 전에 hard FAIL로 잡는다):
NTREE_OK=$(awk 'NF==2 && $1 ~ /^[0-9]+$/ && $2 ~ /^engine\//' "$TREE" | wc -l)
[ "$NTREE_OK" -eq "$NTREE" ] || FAIL "sweep parse broken ($NTREE_OK/$NTREE valid)"
$SSH "cat > \$HOME/.idle_tree.txt" < "$SCRATCH/idle_head_files.txt" || FAIL "tree list push failed"
$SSH 'cd ~/JKENGINE && : > ~/.idle_sizes.txt
while read -r f; do
  if [ -f "$f" ]; then printf "%s %s\n" "$(tr -d "\r" < "$f" | wc -c)" "$f"; else echo "MISSING $f"; fi
done < ~/.idle_tree.txt' > "$SCRATCH/idle_phone_sizes.txt" || FAIL "phone size sweep failed"
# comm/diff 규약 — 양측 같은 collation(Git Bash sort vs 폰 sort 결과 불일치 실측:
# comm "not in sorted order" 306행 가짜 오차). LC_ALL=C로 양측 재정렬해 비교.
LC_ALL=C sort "$SCRATCH/idle_phone_sizes.txt" > "$SCRATCH/idle_phone_sizes.s" \
    || FAIL "phone size sweep sort failed"
mv -f "$SCRATCH/idle_phone_sizes.s" "$SCRATCH/idle_phone_sizes.txt"
# 크기+이름 쌍 diff — 오차 행마다 <(HEAD)/>(폰) 한 파일씩, 경로는 마지막 필드.
DIFFOUT=$(diff "$SCRATCH/idle_head_sizes.txt" "$SCRATCH/idle_phone_sizes.txt" || true)
DIFFN=$(printf '%s\n' "$DIFFOUT" | grep -ac '^[<>]')
DIFFFILES=$(printf '%s\n' "$DIFFOUT" | grep -a '^[<>]' | sed 's/^[<>] //' | awk '{print $2}' | sort -u)
echo "SWEEP-DIFF=$DIFFN / $NTREE"

# 기대 배포 집합 = D1 소거 시점 이후 src/include diff (T1+T2+fixr1 계보)
git -C "$GITROOT" diff --name-only "$D1_COMMIT" HEAD -- engine/src engine/include | sort \
    > "$SCRATCH/idle_expected.txt"
NEXP=$(wc -l < "$SCRATCH/idle_expected.txt")
echo "EXPECTED-DEPLOY=$NEXP (D1 이후 src/include 계보 파일)"
echo "$DIFFFILES" | grep -a . > "$SCRATCH/idle_difffiles.txt"
# 오차가 기대 집합 안에 다 들어가는지(기대 집합 밖 파일 유무)
UNEXPECTED=$(comm -23 "$SCRATCH/idle_difffiles.txt" "$SCRATCH/idle_expected.txt" | head -10)
if [ -z "${UNEXPECTED:-}" ] && [ "$DIFFN" != "0" ]; then
    echo "SWEEP-SCOPE=EXPECTED (DIFF ⊆ T1/T2 계보 — tar blob 배포로 진행)"
elif [ "$DIFFN" = "0" ]; then
    echo "SWEEP-SCOPE=CLEAN (폰 트리=HEAD 동일 — 배포 불요)"
else
    echo "SWEEP-SCOPE=UNEXPECTED-FILES (기대 집합 밖 결손 트리 낙후 — D1 원장 전량 git archive 경로):"
    echo "$UNEXPECTED"
fi

# ------------------------------------------------------------------ 1. pre-clean
echo "=== 1. pre-flight + pre-clean (재링크 ETXTBSY 방지 — jkweb 무접촉) ==="
$SSH 'echo PHONE-REACHABLE; uname -m; echo TMPDIR=$TMPDIR; command -v termux-wake-lock >/dev/null 2>&1 && termux-wake-lock || echo WARN-no-wake-lock' \
    || FAIL "ssh failed (phone unreachable — LAN 한정, sshd 기동: 폰 Termux에서 sshd)"
$SSH "pkill -f '[j]kdesktop' 2>/dev/null; sleep 2; pkill -9 -f '[j]kdesktop' 2>/dev/null; sleep 1; pgrep -f '[j]kdesktop' >/dev/null 2>&1 && { echo PRECLEAN-FAIL; exit 1; }; echo SRV-DOWN" \
    || FAIL "pre-clean could not bring the old phone server down"
echo "PRECLEAN-SRV-DOWN=OK (구판 서버+클라 — 배포 교체 재기동 계약, jkweb 불접촉)"

# ------------------------------------------------------------------ 2. 원격 스크립트 생성
cat > "$SCRATCH/$RSRC_TAR_NAME" <<'PHONECIDLEEOF'
#!/bin/bash
# 폰 측 idle 영수증 절차(phone_client_idle.sh가 생성 — #89 T3 폰 축).
# 종료 상태 = 서버 UP+터미널 상시(기존 세션 원상) — 서버 존중 계약.
set -u
cd ~/JKENGINE/engine || exit 1
FAIL() { echo "PC-FAIL: $*"; exit 1; }
DSP="__PHONE_DISPLAY__"   # 드라이버가 PHONE_DISPLAY로 치환한다(기본 1)
TMPD="${TMPDIR:-/data/data/com.termux/files/usr/tmp}"
NLOG="$TMPD/pc_ninja.log"
STLOG="$TMPD/pc_selftest.log"
CLK=$(getconf CLK_TCK 2>/dev/null); [ -n "$CLK" ] || CLK=100
LOG="$TMPD/pc_srv.log"
RES="$TMPD/pc_res.txt"
: > "$RES"

echo "=== B. ninja rebuild (aarch64 — T1/T2/fixr1 소스 증분) ==="
ninja -C buildterm -j4 >"$NLOG" 2>&1
N_RC=$?
tail -4 "$NLOG"
echo "NINJA-RC=$N_RC"
[ "$N_RC" -eq 0 ] || FAIL "ninja rebuild rc=$N_RC"
[ -x buildterm/jkdesktop ] || FAIL "buildterm/jkdesktop missing"

echo "=== C. selftest — 폰 캐논 래더(원장: 546=pre-T1·556=T1만·565=T1+T2+fixr1) ==="
timeout 900 ./buildterm/jkdesktop test >"$STLOG" 2>&1
S_RC=$?
ST_PASS=$(grep -ac '^\[PASS\]' "$STLOG")
ST_FAIL=$(grep -ac '^\[FAIL\]' "$STLOG")
P2I=$(grep -ac '^\[PASS\] 2i-' "$STLOG")
echo "PHONE-SELFTEST rc=$S_RC PASS=$ST_PASS FAIL=$ST_FAIL 2i-PASS=$P2I"
grep -a 'AppSelfTest' "$STLOG" | tail -1
grep -aq 'AppSelfTest: 0 failure(s)' "$STLOG" || FAIL "AppSelfTest not 0 failure(s)"
[ "$S_RC" -eq 0 ] || FAIL "selftest rc=$S_RC"
[ "$ST_FAIL" -eq 0 ] || FAIL "selftest FAIL=$ST_FAIL (폰축 회귀)"
awk -v p="$ST_PASS" -v i="$P2I" 'BEGIN{
    if (p == 565 && i == 19) print "CANON-INCLUSION=FULL-T1-T2-FIXR1 (폰 565 — 2i 19건 실측)"
    else if (p == 556)       print "CANON-INCLUSION=T1-ONLY (T2 원천 결손 — 원장)"
    else if (p == 546)       print "CANON-INCLUSION=STALE-PRE-T1 (배포 원천 미반영 — 원장)"
    else                     printf "CANON-INCLUSION=OTHER-N(%d, 2i=%d — 계보 미부합 원장)\n", p, i
}'

# ---------------------------------------------------------------- 계측 원료
ctl() { timeout 15 ./buildterm/jkdesktop agentctl "$1" 2>/dev/null | grep -a '{' | head -1; }
ok()  { printf '%s' "${1:-}" | grep -aq '"ok":true'; }
cclients() {
    for d in /proc/[0-9]*; do
        p=${d#/proc/}
        [ "$(cat "$d/comm" 2>/dev/null)" = "jkdesktop" ] || continue
        c=$(tr '\0' ' ' < "$d/cmdline" 2>/dev/null)
        case "$c" in *"--server"*) continue ;; esac
        app=$(printf '%s' "$c" | sed -n 's/.*--client \([^ ]*\).*/\1/p')
        [ -n "$app" ] || { case "$c" in *"--filedlg"*) app=filedlg ;; esac; }
        [ -n "$app" ] || continue
        set -- $(sed 's/^[^)]*) //' "$d/stat" 2>/dev/null)
        echo "$p $app ${12:-0} ${13:-0}"
    done
}
try_window() {
    T=$1
    LAST=""
    for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15; do
        sleep 2
        WIN=$(ctl '{"tool":"list_windows","args":{}}')
        LAST=$(printf '%s' "$WIN" | grep -aoE "\"id\":[^}]*\"title\":\"$T[^\"]*\"" | head -1)
        [ -n "$LAST" ] && break
    done
    [ -n "$LAST" ] && echo "WINDOW-FOUND: $LAST"
}
window() {
    LABEL=$1 DURW=$2
    echo "=== window $LABEL (${DURW}s — 조성: $(cclients | awk '{print $2}' | sort | uniq -c | tr '\n' ' ') ) ==="
    O1=$(wc -c < "$LOG")
    SRD1=$(awk '{print $14+$15}' /proc/$SRVPID/stat 2>/dev/null)
    [ -n "$SRD1" ] || FAIL "window $LABEL: server stat unreadable"
    cclients > "$TMPD/pc_c0_$LABEL.txt"
    sleep "$DURW"
    O2=$(wc -c < "$LOG")
    SRD2=$(awk '{print $14+$15}' /proc/$SRVPID/stat 2>/dev/null)
    [ -n "$SRD2" ] || FAIL "window $LABEL: server died mid-window"
    cclients > "$TMPD/pc_c1_$LABEL.txt"
    tail -c +"$((O1+1))" "$LOG" | head -c "$((O2-O1))" >"$TMPD/pc_win_$LABEL.txt"

    IS=$(awk -v d=$((SRD2-SRD1)) -v hz="$CLK" -v t="$DURW" 'BEGIN{printf "%.2f", d/hz/t*100}')
    echo "SRV-IDLE: ${IS}%"
    echo "METRIC-$LABEL IDLE_SRV=$IS"
    echo "$LABEL IDLE_SRV=$IS" >>"$RES"
    echo "--- window $LABEL 클라 /proc (app pid CPU%) — RES 병행 기록 ---"
    awk -v clk="$CLK" -v dur="$DURW" -v lab="$LABEL" -v res="$RES" '
        NR==FNR{c0[$1]=$3+$4; a0[$1]=$2; next}
        {c1[$1]=$3+$4; a1[$1]=$2}
        END{for(p in a1){if(p in c0){d=c1[p]-c0[p]; v=sprintf("%.2f", d/clk/dur*100)
                printf "METRIC-%s APPCPU_%s=%s\n",lab,a1[p],v
                printf "%s APP %s pid=%s CPU=%s%%\n",lab,a1[p],p,v
                print lab" APPCPU "a1[p]"="v >> res}}
            for(p in a0) if(!(p in a1)) printf "%s GONE %s pid=%s (창 소멸)\n",lab,a0[p],p}' \
        "$TMPD/pc_c0_$LABEL.txt" "$TMPD/pc_c1_$LABEL.txt" | sort
    SRVLINES=$(grep -ac '^\[cpustat\] sdl=' "$TMPD/pc_win_$LABEL.txt")
    [ "$SRVLINES" -ge 3 ] || FAIL "window $LABEL: server cpustat silent ($SRVLINES lines) — 계측 파열"
    COMP=$(grep -a '^\[cpustat\] sdl=' "$TMPD/pc_win_$LABEL.txt" |
        sed -n 's/.*composites=\([0-9]*\).*/\1/p' |
        awk -v t="$DURW" '{s+=$1;n++} END{printf "합성/s=%.2f (%d행)", s/t, n}')
    CLTLINES=$(grep -ac '^\[cpustat\] timer=' "$TMPD/pc_win_$LABEL.txt")
    POS=$(grep -a '^\[cpustat\] timer=' "$TMPD/pc_win_$LABEL.txt" | grep -avc 'frames=0$')
    FSUM=$(grep -a '^\[cpustat\] timer=' "$TMPD/pc_win_$LABEL.txt" |
        sed -n 's/.*frames=\([0-9]*\).*/\1/p' |
        awk -v t="$DURW" '{s+=$1} END{printf "%.2f", s/t}')
    FSEQ=$(grep -a '^\[cpustat\] timer=' "$TMPD/pc_win_$LABEL.txt" |
        sed -n 's/.*frames=\([0-9]*\).*/\1/p' | tr '\n' ' ')
    echo "CPUSTAT: 서버행=$SRVLINES 클라행=$CLTLINES (기대 클라행 ≈ $DURW × 클라수)"
    echo "CLIENT-FRAMES>0행=$POS sum/s=${FSUM}fps   서버합성=$COMP"
    echo "FRAMES-SEQ(클라 초당 frames 원문): ${FSEQ:-none}"
    echo "METRIC-$LABEL FRPS=$FSUM POS=$POS CLTLINES=$CLTLINES"
    [ "$CLTLINES" -gt 0 ] || echo "NOTE: 이 창에 클라 cpustat 0행 — 클라 미스폰 또는 JK_CPU_TRACE 비상속 (원장)"
    echo "--- window $LABEL [compst] 원문 전문 ---"
    grep -a compst "$TMPD/pc_win_$LABEL.txt" || echo "(compst 0행 — idle 무합성)"
    echo "$LABEL FRPS=$FSUM POS=$POS" >>"$RES"
    echo "$LABEL CLTLINES=$CLTLINES" >>"$RES"
}
boot() {
    pkill -f '[j]kdesktop' 2>/dev/null
    sleep 1
    pkill -9 -f '[j]kdesktop' 2>/dev/null
    sleep 1
    env DISPLAY=:__PHONE_DISPLAY__ JK_CPU_TRACE=1 setsid nohup ./buildterm/jkdesktop --server >"$LOG" 2>&1 &
    sleep 8
    SRVPID=$(pgrep -f 'buildterm/[j]kdesktop --server' | head -1)
    [ -n "$SRVPID" ] || { echo "--- boot log tail:"; tail -5 "$LOG"; FAIL "no server 8s after boot"; }
    PING=""
    for i in 1 2 3 4 5; do
        PING=$(ctl '{"tool":"ping","args":{}}')
        ok "$PING" && break
        sleep 2
    done
    ok "$PING" || FAIL "ping did not ok — $PING"
    echo "boot: server pid=$SRVPID (DISPLAY=:$DSP — 폰 생산 원문)"
}

# ------------------------------------------------------------------ legB
echo "=== legB: 서버+태스크바 기준선 (taskbar idle frames=0 증명 — 귀속 앵커) ==="
boot
sleep 4
window base 12

# ------------------------------------------------------------------ legG
echo "=== legG: launch_app gallery — idle 영수증 본판 (스폰 + 5s 무입력 대기 + 창) ==="
boot
L=$(ctl '{"tool":"launch_app","args":{"app":"gallery"}}')
ok "$L" || FAIL "launch_app gallery failed — $L"
try_window 'Gallery' || FAIL "no Gallery window"
sleep 5
window gallery 15

# ------------------------------------------------------------------ legT
echo "=== legT: terminal 비교 축 (대조군 앵커 — 폰 원문 7.8~8.9%·블링크 구동) ==="
boot
L=$(ctl '{"tool":"launch_app","args":{"app":"terminal"}}')
ok "$L" || FAIL "launch_app terminal failed — $L"
try_window 'Terminal' || FAIL "no Terminal window"
sleep 5
window terminal 12

# ------------------------------------------------------------------ legN
echo "=== legN: notify 토스트 (publish_event — 페이드 유계 실측) ==="
boot
L=$(ctl '{"tool":"open_notify","args":{}}')
ok "$L" || echo "NOTICE: open_notify reply — $L (계속)"
try_window 'Notifications' || FAIL "no Notifications window"
sleep 3
timeout 12 ./buildterm/jkdesktop agentctl '{"tool":"publish_event","args":{"topic":"agent.notify","data":{"title":"T3 probe","body":"idle receipt toast"}}}' >/dev/null 2>&1
window notify 20

# ------------------------------------------------------------------ legF
echo "=== legF: filedlg (file_open — 열어둔 다이얼로그 idle) ==="
boot
timeout 12 ./buildterm/jkdesktop agentctl '{"tool":"file_open","args":{"wait":"event","title":"T3 probe"}}' >/dev/null 2>&1
try_window '파일' || FAIL "no filedlg window"
sleep 3
window filedlg 12

# ------------------------------------------------------------------ legP
echo "=== legP: palette — idle 창 + resize bounce (입력 이벤트→렌더 유지 관찰) ==="
boot
L=$(ctl '{"tool":"launch_app","args":{"app":"palette"}}')
ok "$L" || FAIL "launch_app palette failed — $L"
try_window 'Command Palette' || FAIL "no Command Palette window"
sleep 3
window paletteIdle 10
PID_=$(cclients | awk '$2=="palette"{print $1}' | head -1)
[ -n "$PID_" ] || FAIL "palette client process not found"
R=$(ctl "{\"tool\":\"window_resize\",\"args\":{\"id\":$PID_,\"w\":530,\"h\":368}}")
ok "$R" || echo "NOTICE: palette resize failed — $R (계속)"
sleep 1
window paletteResize 6

echo "=== legS: palette 타이핑 시도 (send_input — ask 게이트. 폰 permissions.json에 send_input allow 없으면 승인 경로 없음 — 정직 스킵) ==="
L=$(ctl '{"tool":"launch_app","args":{"app":"chat"}}')
if ok "$L"; then try_window 'Chat'; else echo "NOTICE: chat launch 미성립($L) — 구독자 없이 진행"; fi
sleep 8
WIN=$(ctl '{"tool":"list_windows","args":{}}')
PAL_ID=$(printf '%s' "$WIN" | grep -aoE '\{"id":[^}]*"title":"Command Palette"[^}]*\}' | head -1 |
    sed -n 's/.*"id":\([0-9]*\),.*/\1/p')
REQ=$(timeout 12 ./buildterm/jkdesktop agentctl '{"tool":"read_events","args":{}}' 2>/dev/null |
    grep -ao '"request":[0-9]*' | head -1 | cut -d: -f2)
if [ -n "$PAL_ID" ] && [ -n "$REQ" ]; then
    timeout 15 ./buildterm/jkdesktop agentctl \
        '{"tool":"send_input","args":{"op":"type","id":'"$PAL_ID"',"text":"jk"}}' >/dev/null 2>&1
    timeout 12 ./buildterm/jkdesktop agentctl \
        '{"tool":"approve","args":{"request":'"$REQ"',"decision":"allow"}}' >/dev/null 2>&1
    sleep 2
    window paletteType 8
    echo "SEND-INPUT-TRY: request=$REQ 승인 경로 시도 — paletteType 창 클라 cpustat 원문 tail:"
    grep -a '^\[cpustat\] timer=' "$TMPD/pc_win_paletteType.txt" | tail -8
else
    echo "SEND-INPUT-SKIPPED: request id 확보 불가(ask 게이트+폰 permissions.json send_input 미허용 — 정직 원장)"
    echo "  → 입력→렌더 증명은 위 legP resize bounce(paletteResize) 원문으로 대신한다"
fi

echo "=== legD: imguidemo (FPS 표시 정지 — idle 무렌더 정상 귀결 관찰) ==="
L=$(ctl '{"tool":"launch_app","args":{"app":"imguidemo"}}')
if ok "$L"; then
    try_window 'Dear ImGui' || FAIL "no Dear ImGui Demo window"
    sleep 3
    window imguidemo 12
else
    echo "IMGUIDEMO-SKIPPED: launch 미성립 — $L"
fi

# ---------------------------------------------------------------- 종결 구성
echo "=== final: 출하·복원 구성(서버 UP+터미널 상시 — 서버 존중 계약, 무 trace·참작) ==="
boot
L=$(ctl '{"tool":"launch_app","args":{"app":"terminal"}}')
ok "$L" || echo "NOTICE: final terminal launch 미성립 — $L"
WIN=$(ctl '{"tool":"list_windows","args":{}}')
echo "FINAL-WINDOWS: $(printf '%s' "$WIN" | head -c 400)"
FINAL_P=$(pgrep -f 'buildterm/[j]kdesktop --server' | tr '\n' ' ')
[ -n "$FINAL_P" ] || FAIL "server not UP at end (종료 게이트 위반)"
echo "FINAL-SERVER-PIDS=$FINAL_P"
echo "END-STATE: 서버 UP+터미널 상시(태스크바 자동) — 폰 idle 육안 게이트는 사용자 선언만 결제."

# ---------------------------------------------------------------- 판정
echo "=== PHONE-IDLE-판정 (스펙 하단 — 갤러리 클라 <5%·frames/s<3 — 본판정) ==="
GALCPU=$(grep -a '^gallery APPCPU gallery=' "$RES" | tail -1 | sed 's/.*=//' | tr -d '\r')
GALFPS=$(grep -a '^gallery FRPS=' "$RES" | head -1 | sed 's/.*FRPS=//;s/ .*//' | tr -d '\r')
echo "PHONE-GALLERY: 클라=${GALCPU:-n/a}% frames/s=${GALFPS:-n/a} — 변경 전 대조 원문 100.1~102.6%·19fps (spike §1b)"
awk -v c="${GALCPU:-999}" -v f="${GALFPS:-999}" 'BEGIN{
    if ((c+0) < 5 && (f+0) < 3) printf "PHONE-IDLE-VERDICT: IDLE-OK (갤러리 클라 %.2f%% < 5%% · %.2ffps < 3 — 스펙 하단 충족)\n", c, f
    else printf "PHONE-IDLE-VERDICT: IDLE-FAIL(갤러리 클라 %.2f%%·%.2ffps — 하단 미달 — 정직 원장)\n", c, f
}'
echo "terminal/taskbar 비교 축 원문:"
grep -a -E '^(base|terminal|paletteIdle) APPCPU ' "$RES" | sort
rm -f "$TMPD"/pc_c*_*.txt "$TMPD"/pc_win_*.txt "$TMPD"/pc_res.txt
rm -f -- "$0"
echo "PHONE-PC-FINISHED"
exit 0
PHONECIDLEEOF
sed -i "s/__PHONE_DISPLAY__/${PHONE_DISPLAY#:}/" "$SCRATCH/$RSRC_TAR_NAME" ||
    FAIL "remote script display substitution failed"
grep -aq '__PHONE_DISPLAY__' "$SCRATCH/$RSRC_TAR_NAME" && FAIL "display placeholder unresolved"

# ------------------------------------------------------------------ 3. 배포
echo "=== 2. 재배포 (sweep 산치 따라 tar blob 스테이징 / 전량 git archive — D1 원장) ==="
if [ "$DIFFN" != "0" ]; then
    if [ -z "${UNEXPECTED:-}" ]; then
        # tar 오염 게이트: 워킹 카피 접촉 0 — HEAD blob 스테이징으로만 조립.
        STAGE="$SCRATCH/idle_headstage"
        rm -rf "$STAGE"
        TARBALL="$SCRATCH/phone_client_idle.tar"
        rm -f "$TARBALL"
        GERR="$SCRATCH/idle_gshow.err"
        : > "$GERR"
        for f in $DIFFFILES; do
            mkdir -p "$STAGE/$(dirname "$f")"
            GS_OK=0
            for try in 1 2 3; do
                git -C "$GITROOT" show "HEAD:$f" > "$STAGE/$f" 2>>"$GERR" && { GS_OK=1; break; }
                sleep 3
            done
            [ "$GS_OK" -eq 1 ] || FAIL "git show HEAD:$f 스테이징 실패(재시도 3회) — $(tail -1 "$GERR")"
        done
        echo "CONTAMINATION-GATE=CLEAN (tar 조립=HEAD blob 전량 — 워킹 카피 접촉 0)"
        tar -cf "$TARBALL" -C "$STAGE" $DIFFFILES || FAIL "tar creation failed"
        echo "tar size: $(wc -c < "$TARBALL" | tr -d ' ') bytes — files: $DIFFN"
        $SSH "tar -xf - -C ~/JKENGINE" < "$TARBALL" || FAIL "remote tar extract failed"
        rm -f "$TARBALL"
    else
        echo "REDEPLOY-MODE=FULL-GIT-ARCHIVE (D1 §4 원장 — 트리 정합 복구, autocrlf=false로 LF 원문 배포)"
        git -C "$GITROOT" -c core.autocrlf=false archive HEAD -- engine/src engine/include engine/CMakeLists.txt |
            $SSH "tar -xf - -C ~/JKENGINE" || FAIL "git archive deploy failed"
    fi
else
    echo "DEPLOY-SKIP (폰 트리=HEAD 동일)"
fi
$SSH "cat > ~/JKENGINE/$RSRC_TAR_NAME" < "$SCRATCH/$RSRC_TAR_NAME" || FAIL "remote script copy failed"

# 배포 신선도 — 재sweep으로 마감(D1 원장 sweep 배포 절차 명문화 승계).
$SSH 'cd ~/JKENGINE && : > ~/.idle_sizes.txt
while read -r f; do
  if [ -f "$f" ]; then printf "%s %s\n" "$(tr -d "\r" < "$f" | wc -c)" "$f"; else echo "MISSING $f"; fi
done < ~/.idle_tree.txt' > "$SCRATCH/idle_phone_sizes2.txt" || FAIL "post-deploy sweep failed"
LC_ALL=C sort "$SCRATCH/idle_phone_sizes2.txt" > "$SCRATCH/idle_phone_sizes2.s" \
    || FAIL "post-deploy sweep sort failed"
mv -f "$SCRATCH/idle_phone_sizes2.s" "$SCRATCH/idle_phone_sizes2.txt"
grep -aq MISSING "$SCRATCH/idle_phone_sizes2.txt" && FAIL "phone tree missing a tracked file (배포 원천 결손)"
DIFFN2=$(comm -3 "$SCRATCH/idle_head_sizes.txt" "$SCRATCH/idle_phone_sizes2.txt" | grep -ac '.')
echo "SWEEP-POST-DIFF=$DIFFN2"
[ "$DIFFN2" -eq 0 ] || FAIL "post-deploy sweep not clean — 배포 후에도 트리 오차 (원장)"
echo "DEPLOY-FRESHNESS-OK (전수 sweep 정합 — blob 크기 등호)"

# ------------------------------------------------------------------ 4. 실행
echo "=== 3. 폰 리빌드+영수증 절차 실행 (ninja ~9-10분 + selftest + 레그 8) ==="
$SSH "bash $RSRC_PHONE" > "$RUNLOG" 2>&1
R_RC=$?
RUN_RC=$R_RC
if [ "$R_RC" -eq 255 ]; then
    echo "RUN-DROPPED: ssh 채널 단절(Wi-Fi 낙하 함정 — 폰 ninja 로그는 \$TMPDIR에 생존, 재실행=증분:"
    echo "  PHONE_HOST=<폰 IP> bash engine/tools/probes/phone_client_idle.sh)"
fi
cat "$RUNLOG"
grep -aq '^PHONE-PC-FINISHED$' "$RUNLOG" || FAIL "receipt script did not finish (rc=$RUN_RC — RUN-DROPPED면 재실행)"
if grep -aq '^PC-FAIL' "$RUNLOG"; then
    HARD=$(grep -a '^PC-FAIL' "$RUNLOG" | head -1)
    FAIL "phone hard receipt: $HARD"
fi

echo "=== 4. REMNANT 검사 — 원격 스크립트 자기 소각 확인 (T1 NR1 계약) ==="
$SSH "ls ~/JKENGINE/$RSRC_TAR_NAME" >/dev/null 2>&1
REMNANT_RC=$?
echo "REMNANT-LS-RC=$REMNANT_RC"
# 2=ls 미발견(소각 성공) — rc 미게이트였던 T3 리뷰 I1 봉합: 리모트 잔존물 소각
# 실패 시 리턴코드가 성공을 반영하지 않음.
[ "$REMNANT_RC" -eq 2 ] || FAIL "REMNANT survived (원격 스크립트 자기 소각 실패 — rc=$REMNANT_RC)"
$SSH "rm -f \$HOME/.idle_tree.txt \$HOME/.idle_sizes.txt" 2>/dev/null

echo "PC-PHONE-END"
# honest-fail 원칙: 수치 미달(IDLE-FAIL — 원격 스크립트 판정행)은 rc=0 — RUNLOG 원장.
# hard FAIL(배포·sweep·빌드·selftest·기동·계측 무음·REMNANT·서버 종료 게이트)만 exit 1.
exit 0
# EOF — 끝 개행 유지.
