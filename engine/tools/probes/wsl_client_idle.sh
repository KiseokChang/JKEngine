#!/usr/bin/env bash
# T3 WSL 클라 idle 실측 probe (#89 클라 idle 스핀 수리 — plan
# 2026-10-09-client-idle, wsl_dirty_present.sh(T3 선례)+clt-spin spike 선례 승계).
#
# 영수증 목표(스펙 성공 판정):
#   idle(앱 열어두고 무입력): 갤러리 클라 CPU < 5% + [cpustat] frames/s < 3
#   (변경 전 대조 원문: 클라 100.1~102.6%·19fps — spike-report §1b) +
#   terminal/taskbar 비교 축(대조군 앵커: terminal 7.8~8.9%·taskbar <4% — 폰
#   원문; WSL은 동형 실측 1독) + T2 관찰 몫(notify 토스트 유계·filedlg idle·
#   imguidemo FPS 정지·입력→렌더 유지·브라우저 폴백 래그). 캐논 게이트(WSL 565).
#
# 측정 설비 선례 승계(docs/85 §3 #12 트랩 원장 반영):
#   · /proc utime+stime 대차(ps %cpu는 수명 평균 — docs/78 §5.5 렛슨).
#   · [cpustat]는 클라 1행/s·서버("[cpustat] sdl=.. composites=N") 1행/s —
#     서버 스폰 클라(launch_app)는 서버 fd를 상속해 **같은 로그에 섞인다**.
#     [cpustat] 행 자체는 앱명을 달지 않는다 → 프레임 귀속은 **조성 창 귀속**:
#     legBase에서 태스크바(무타이머 — spike §2 대조군) idle frames=0을 먼저
#     증명하고, 이후 창의 frames>0 행을 "열어둔 활성 앱"으로 귀속한다(창마다
#     조성+창 길이를 인쇄해 오귀속 여지를 명시 노출 — 프레임 합계와 교차 판독).
#   · idle 무합성은 **성공 조건**이다: [compst] 0행을 hard FAIL로 만지지 않는다
#     (wsl_dirty_present 선례와 달리 — 그 probe는 블링크 커밋이 흐르는 합성 창을
#     상정했다. idle 창은 무렌더가 정답이고 서버 [cpustat] composites=0이 근거).
#     계측 실존은 서버/클라 [cpustat] 행 수로 단정한다(무음=진짜 hard FAIL).
#   · 브래킷 pkill+-9 에스컬레이션, WSL selftest는 WSL 내부 리다이렉트(Git Bash
#     파이프 조각 유실 렛슨), setsid 부팅, 런치→창 등장 리트라이 루프(폰 스폰
#     15-18s 원장 초과 대응).
#   · send_input은 기본 ask 게이트 — 자기 연결 승인 금지(:6326 selfApprove)라
#     agentctl 단발로는 승인 불가. 구독자(chat) 띄우고 read_events(브로커 게이트)
#     로 request id를 찾으면 approve → 주입 시도; 못 찾으면 SKIPPED 정직 원장
#     (입력→렌더 증명은 resize bounce 관찰로 대체 — 아래 legP 참조).
#   · 실행 표준(WSL 밖 Git Bash):
#     MSYS_NO_PATHCONV=1 wsl.exe -d Ubuntu-24.04 -- bash /mnt/i/progwork/JKENGINE/engine/tools/probes/wsl_client_idle.sh
set -u
cd /mnt/i/progwork/JKENGINE/engine || exit 1

FAIL() { echo "CLTIDLE-WSL-FAIL: $*"; exit 1; }
CLK_TCK=$(getconf CLK_TCK)
RES=$(mktemp /tmp/cltidle_res_XXXX.txt)
: > "$RES"
LOG=/tmp/cltidle_srv.log

echo "=== 1. bracketed pre-clean ==="
pkill -f 'buildwsl/[j]kdesktop' 2>/dev/null
sleep 1
pkill -9 -f 'buildwsl/[j]kdesktop' 2>/dev/null
sleep 1
LEFT=$(ps -eo args | grep -i 'buildwsl/[j]kdesktop' | wc -l)
[ "$LEFT" -eq 0 ] || FAIL "pre-clean left $LEFT jkdesktop alive"

echo "=== 2. ninja rebuild (buildwsl — 소스 무변경 probe, 증분) ==="
ninja -C buildwsl -j3 >/tmp/cltidle_build.log 2>&1
B_RC=$?
echo "WSL-BUILD-RC=$B_RC"
[ "$B_RC" -eq 0 ] || { tail -5 /tmp/cltidle_build.log; FAIL "ninja rc=$B_RC"; }
[ -x buildwsl/jkdesktop ] || FAIL "buildwsl/jkdesktop missing"

echo "=== 3. WSL selftest (캐논 게이트 — 기대 565 = T1+T2+fixr1 계보) ==="
timeout 420 ./buildwsl/jkdesktop test >/tmp/cltidle_st.log 2>&1
ST_RC=$?
ST_FAIL=$(grep -ac '^\[FAIL\]' /tmp/cltidle_st.log)
ST_PASS=$(grep -ac '^\[PASS\]' /tmp/cltidle_st.log)
P2I=$(grep -ac '^\[PASS\] 2i-' /tmp/cltidle_st.log)
echo "selftest rc=$ST_RC PASS=$ST_PASS FAIL=$ST_FAIL 2i-PASS=$P2I"
grep -aq 'AppSelfTest: 0 failure(s)' /tmp/cltidle_st.log || FAIL "selftest not 0 failure(s)"
[ "$ST_RC" -eq 0 ] || FAIL "selftest rc=$ST_RC"
[ "$ST_FAIL" -eq 0 ] || FAIL "selftest FAIL=$ST_FAIL"
awk -v p="$ST_PASS" -v i="$P2I" 'BEGIN{
    if (p == 565 && i == 19) print "CANON-INCLUSION=FULL-T1-T2-FIXR1 (WSL 565 — 2i 19건 실측)"
    else if (i == 19)        printf "CANON-INCLUSION=2I-FULL-CANON-OTHER(%d — 캐논 이탈, 원장)\n", p
    else                     printf "CANON-INCLUSION=STALE(2i PASS=%d — 19 미달, 원천 의심)\n", i
}'

# ------------------------------------------------------------- 측정 원료
ctl() { timeout 15 ./buildwsl/jkdesktop agentctl "$1" 2>/dev/null | grep -a '{' | head -1; }
ok()  { printf '%s' "${1:-}" | grep -aq '"ok":true'; }

# 클라 프로세스 스냅샷: "pid app utime stime" (--client/--filedlg 접두만;
# agentctl 단발 등 --client 없는 커맨드 라인은 뺀다).
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

# 창 등장 대기(비치명판: 빈 문자열 반환 — 레그마다 스킵 판정을 레그가 한다).
try_window() {
    T=$1
    LAST=""
    for i in 1 2 3 4 5 6 7 8 9 10; do
        sleep 2
        WIN=$(ctl '{"tool":"list_windows","args":{}}')
        LAST=$(printf '%s' "$WIN" |
            grep -aoE "\"id\":[^}]*\"title\":\"$T[^\"]*\"" | head -1)
        [ -n "$LAST" ] && break
    done
    [ -n "$LAST" ] && echo "WINDOW-FOUND: $LAST"
}

# 측정 창: 인자 LABEL DUR. 조성(열어둔 클라)을 먼저 인쇄하고 서버/클라 /proc
# 대차+[cpustat] 슬라이스(프레임)+[compst] 원문을 계량한다.
window() {
    LABEL=$1 DURW=$2
    echo "=== window $LABEL (${DURW}s — 조성: $(cclients | awk '{print $2}' | sort | uniq -c | tr '\n' ' ') )"
    O1=$(wc -c < "$LOG")
    SRD1=$(awk '{print $14+$15}' /proc/$SRVPID/stat 2>/dev/null)
    [ -n "$SRD1" ] || FAIL "window $LABEL: server stat unreadable"
    cclients > "/tmp/cltidle_c0_$LABEL.txt"
    sleep "$DURW"
    O2=$(wc -c < "$LOG")
    SRD2=$(awk '{print $14+$15}' /proc/$SRVPID/stat 2>/dev/null)
    [ -n "$SRD2" ] || FAIL "window $LABEL: server died mid-window"
    cclients > "/tmp/cltidle_c1_$LABEL.txt"
    tail -c +"$((O1+1))" "$LOG" | head -c "$((O2-O1))" >"/tmp/cltidle_win_$LABEL.txt"

    IS=$(awk -v d=$((SRD2-SRD1)) -v hz="$CLK_TCK" -v t="$DURW" 'BEGIN{printf "%.2f", d/hz/t*100}')
    echo "SRV-IDLE: ${IS}%"
    echo "METRIC-$LABEL IDLE_SRV=$IS"
    echo "$LABEL IDLE_SRV=$IS" >>"$RES"
    echo "--- window $LABEL 클라 /proc (app pid CPU%) — RES 병행 기록 ---"
    awk -v clk="$CLK_TCK" -v dur="$DURW" -v lab="$LABEL" -v res="$RES" '
        NR==FNR{c0[$1]=$3+$4; a0[$1]=$2; next}
        {c1[$1]=$3+$4; a1[$1]=$2}
        END{for(p in a1){if(p in c0){d=c1[p]-c0[p]; v=sprintf("%.2f", d/clk/dur*100)
                printf "METRIC-%s APPCPU_%s=%s\n",lab,a1[p],v
                printf "%s APP %s pid=%s CPU=%s%%\n",lab,a1[p],p,v
                print lab" APPCPU "a1[p]"="v >> res}}
            for(p in a0) if(!(p in a1)) printf "%s GONE %s pid=%s (창 소멸)\n",lab,a0[p],p}' \
        "/tmp/cltidle_c0_$LABEL.txt" "/tmp/cltidle_c1_$LABEL.txt" | sort
    SRVLINES=$(grep -ac '^\[cpustat\] sdl=' "/tmp/cltidle_win_$LABEL.txt")
    [ "$SRVLINES" -ge 3 ] || FAIL "window $LABEL: server cpustat silent ($SRVLINES lines) — 계측 파열"
    COMP=$(grep -a '^\[cpustat\] sdl=' "/tmp/cltidle_win_$LABEL.txt" |
        sed -n 's/.*composites=\([0-9]*\).*/\1/p' |
        awk -v t="$DURW" '{s+=$1;n++} END{printf "합성/s=%.2f (%d행)", s/t, n}')
    CLTLINES=$(grep -ac '^\[cpustat\] timer=' "/tmp/cltidle_win_$LABEL.txt")
    POS=$(grep -a '^\[cpustat\] timer=' "/tmp/cltidle_win_$LABEL.txt" |
        grep -avc 'frames=0$')
    FSUM=$(grep -a '^\[cpustat\] timer=' "/tmp/cltidle_win_$LABEL.txt" |
        sed -n 's/.*frames=\([0-9]*\).*/\1/p' |
        awk -v t="$DURW" '{s+=$1} END{printf "%.2f", s/t}')
    echo "CPUSTAT: 서버행=$SRVLINES 클라행=$CLTLINES (기대 클라행 ≈ $DURW × 클라수)"
    FSEQ=$(grep -a '^\[cpustat\] timer=' "/tmp/cltidle_win_$LABEL.txt" |
        sed -n 's/.*frames=\([0-9]*\).*/\1/p' | tr '\n' ' ')
    echo "CLIENT-FRAMES>0행=$POS sum/s=${FSUM}fps   서버합성=$COMP"
    echo "FRAMES-SEQ(클라 초당 frames 원문): ${FSEQ:-none}"
    echo "METRIC-$LABEL FRPS=$FSUM POS=$POS CLTLINES=$CLTLINES"
    [ "$CLTLINES" -gt 0 ] || echo "NOTE: 이 창에 클라 cpustat 0행 — 클라 미스폰 또는 JK_CPU_TRACE 비상속 (원장)"
    echo "--- window $LABEL [compst] 원문 전문 ---"
    grep -a compst "/tmp/cltidle_win_$LABEL.txt" || echo "(compst 0행 — idle 무합성)"
    echo "$LABEL FRPS=$FSUM POS=$POS" >>"$RES"
    echo "$LABEL CLTLINES=$CLTLINES" >>"$RES"
}

boot() {
    pkill -f 'buildwsl/[j]kdesktop' 2>/dev/null
    sleep 1
    pkill -9 -f 'buildwsl/[j]kdesktop' 2>/dev/null
    sleep 1
    rm -f /tmp/JKWindowServerPipe.sock
    env DISPLAY=:0 JK_CPU_TRACE=1 setsid nohup ./buildwsl/jkdesktop --server \
        >"$LOG" 2>&1 &
    sleep 8
    SRVPID=$(pgrep -f 'buildwsl/[j]kdesktop --server' | head -1)
    [ -n "$SRVPID" ] || { echo "--- boot log tail:"; tail -5 "$LOG"; FAIL "no server 8s after boot"; }
    PING=""
    for i in 1 2 3 4 5; do
        PING=$(ctl '{"tool":"ping","args":{}}')
        ok "$PING" && break
        sleep 2
    done
    ok "$PING" || FAIL "ping did not ok — $PING"
    echo "boot: server pid=$SRVPID (DISPLAY=:0 — WSL 생산 렌더러 GL 원문)"
}

# ------------------------------------------------------------------ legB
echo "=== legB: 서버+태스크바 기준선 (대조군 앵커 — 태스크바 idle frames=0 증명) ==="
boot
sleep 4
window base 12

# ------------------------------------------------------------------ legG
echo "=== legG: launch_app gallery — idle 영수증 본판 (스폰 + 5s 무입력 + 창) ==="
boot
L=$(ctl '{"tool":"launch_app","args":{"app":"gallery"}}')
ok "$L" || FAIL "launch_app gallery failed — $L"
try_window 'Gallery' || FAIL "no Gallery window"
sleep 5
window gallery 15

# ------------------------------------------------------------------ legT
echo "=== legT: terminal 비교 축 (대조군 — 블링크 ~1.9fps 이벤트/더티 구동 원문) ==="
boot
L=$(ctl '{"tool":"launch_app","args":{"app":"terminal"}}')
ok "$L" || FAIL "launch_app terminal failed — $L"
try_window 'Terminal' || FAIL "no Terminal window"
sleep 5
window terminal 12

# ------------------------------------------------------------------ legN
echo "=== legN: notify 토스트 (publish_event — 페이드 유계 실측: 5s 풀알파 정적+마지막 2s 페이드+만료 래치 1프레임) ==="
boot
L=$(ctl '{"tool":"open_notify","args":{}}')
ok "$L" || echo "NOTICE: open_notify reply — $L (계속)"
try_window 'Notifications' || FAIL "no Notifications window"
sleep 3
timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"publish_event","args":{"topic":"agent.notify","data":{"title":"T3 probe","body":"idle receipt toast"}}}' >/dev/null 2>&1
window notify 20

# ------------------------------------------------------------------ legF
echo "=== legF: filedlg (file_open — 열어둔 다이얼로그 idle: 승인 파킹 후 무조작 idle) ==="
boot
timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"file_open","args":{"wait":"event","title":"T3 probe"}}' >/dev/null 2>&1
try_window '파일' || FAIL "no filedlg window"
sleep 3
window filedlg 12

# ------------------------------------------------------------------ legP
echo "=== legP: palette — idle 창 + resize bounce (입력 이벤트가 렌더를 유발하는 지 — [cpustat] input/frames로) ==="
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

echo "=== legS: palette 타이핑 시도 (send_input — 기본 ask 게이트: chat 구독자+read_events로 request를 찾으면 승인 주입) ==="
L=$(ctl '{"tool":"launch_app","args":{"app":"chat"}}')
if ok "$L"; then try_window 'Chat'; else echo "NOTICE: chat launch 미성립($L) — 구독자 없이 진행"; fi
sleep 8
WIN=$(ctl '{"tool":"list_windows","args":{}}')
PAL_ID=$(printf '%s' "$WIN" |
    grep -aoE '\{"id":[^}]*"title":"Command Palette"[^}]*\}' | head -1 |
    sed -n 's/.*"id":\([0-9]*\),.*/\1/p')
REQ=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"read_events","args":{}}' 2>/dev/null |
    grep -ao '"request":[0-9]*' | head -1 | cut -d: -f2)
if [ -n "$PAL_ID" ] && [ -n "$REQ" ]; then
    timeout 15 ./buildwsl/jkdesktop agentctl \
        '{"tool":"send_input","args":{"op":"type","id":'"$PAL_ID"',"text":"jk"}}' >/dev/null 2>&1
    timeout 12 ./buildwsl/jkdesktop agentctl \
        '{"tool":"approve","args":{"request":'"$REQ"',"decision":"allow"}}' >/dev/null 2>&1
    sleep 2
    window paletteType 8
    echo "SEND-INPUT-TRY: request=$REQ 승인 경로 시도 — paletteType 창 클라 cpustat 원문 tail:"
    grep -a '^\[cpustat\] timer=' "/tmp/cltidle_win_paletteType.txt" | tail -8
else
    echo "SEND-INPUT-SKIPPED: request id 확보 불가(ask 게이트 파킹+브로커 계열 read_events 부재 조합 — 정직 원장)"
fi

# ------------------------------------------------------------------ legBr
echo "=== legBr: browser (주소줄 폴백 래그 관찰 — best-effort; CEF 모듈 부재 축은 정직 스킵) ==="
L=$(ctl '{"tool":"launch_app","args":{"app":"browser"}}')
if ok "$L"; then
    W=$(try_window 'Browser')
    echo "BROWSER-WINDOW: ${W:-NOREG (20s 대기 초과 — 원장)}"
    if [ -n "$W" ]; then
        sleep 3
        window browser 12
    fi
    echo "BROWSER-LAG-OBS: 주소줄/타이틀 폴백 래그 — 브라우저 유일 더티 원=on_paint 도착(g_painted), T2 정정 원문:"
    echo "  폴백은 fallback && dirtyWindow()(JKActivityGate.h:44)라 페인트 없는 전이(CAF 프리로드·에러 페이지)는"
    echo "  더티 없음 — 래그 상한='다음 on_paint/입력/테마 활동까지'(사실상 영구 래그, T2 리포트 §5 concern ① r1)."
    echo "  무타이핑 열기만이라 래그 실증 불가 — 본 leg는 idle 수치 원문만 남긴다(캡처 불요 계약)."
else
    echo "BROWSER-LAG-OBS: launch_app browser 미성립 — $L (buildwsl cef 모듈 미링크 축 — 정직 스킵)"
fi

# ---------------------------------------------------------------- 종결
echo "=== cleanup (bracketed pkill — WSL 축 전체 소각, WSLg 잔존 원상) ==="
pkill -f 'buildwsl/[j]kdesktop' 2>/dev/null
sleep 1
pkill -9 -f 'buildwsl/[j]kdesktop' 2>/dev/null
sleep 1
LEFT=$(ps -eo args | grep -i 'buildwsl/[j]kdesktop' | wc -l)
[ "$LEFT" -eq 0 ] || echo "WARN: $LEFT jkdesktop survived teardown"

echo "=== IDLE-판정 (스펙 하단 — 갤러리 클라 <5%·frames/s<3) ==="
GALCPU=$(grep -a '^gallery APPCPU gallery=' "$RES" | tail -1 | sed 's/.*=//' | tr -d '\r')
GALFPS=$(grep -a '^gallery FRPS=' "$RES" | head -1 | sed 's/.*FRPS=//;s/ .*//' | tr -d '\r')
echo "GALLERY: 클라=${GALCPU:-n/a}% frames/s=${GALFPS:-n/a} — 폰 대조 원문 100.1~102.6%·19fps (spike §1b)"
awk -v c="${GALCPU:-999}" -v f="${GALFPS:-999}" 'BEGIN{
    if ((c+0) < 5 && (f+0) < 3) printf "IDLE-VERDICT: WSL-IDLE-OK (클라 %.2f%% < 5%% · %.2ffps < 3 — WSL 동형 실측 1독)\n", c, f
    else printf "IDLE-VERDICT: WSL-IDLE-FAIL(클라 %.2f%%·%.2ffps — 하단 미달 — 원장)\n", c, f
}'
echo "NOTE: WSL 축은 GL 렌더러(값싼 프레임)라 %·fps 절대치가 폰과 동가는 아니다 — 스펙 본판정은 폰(phone_client_idle.sh), WSL은 동형 실측 1독."
echo "terminal/taskbar 비교 축 원문:"
grep -a -E '^(base|terminal|paletteIdle) APPCPU ' "$RES" | sort
rm -f "$RES" /tmp/cltidle_c*_*.txt /tmp/cltidle_win_*.txt
echo "CLTIDLE-WSL-END"
# honest-fail 원칙: IDLE-FAIL(수치 미달)은 원장 목적이라 rc=0.
# hard FAIL(빌드·selftest·부팅·ping·런치·창 대기 만료·클라 프로세스 부재)만 exit 1.
exit 0
