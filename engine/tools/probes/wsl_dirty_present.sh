#!/usr/bin/env bash
# T3 WSL 더티프레젠트 A/B 실측 (docs/78 §5.7 cpustat+[compst] 선례 승계).
#
# 4레그 — 사다리 판 차이와 렌더러/역치 조건을 분리해 본다:
#   leg0(현행 GL 기준선): 무환경 부팅 — WSL 서버 렌더러는 ACCELERATED(GL)
#      이므로 T2 봉합 ④ SW 게이트가 부분 경로를 봉쇄 → 항상 present=full.
#      6.7% 기준선(docs/78 §5.7) 대조용.
#   leg1(SW·기본 기하): SDL_RENDER_DRIVER=software + 무환경 — 터미널 기본
#      표면 800x500 = 화면(1280x720)의 43.4% > 역치 40% → 블링크 커밋도
#      full로 접힌다(역치 접힘 실증용 — 클라는 CommitFull로 표면 전체 rect를
#      보낸다, JKClientApplication.cpp RenderAndCommit).
#   legA(신판 부분 — 축소 기하): SW + 터미널 420x300(13.7% < 40%) →
#      블링크 커밋이 역치 아래로 접히지 않는다 → present=dirty(N)=..ms.
#   legB(구판 강제 — 축소 기하 + JK_PRESENT_FORCE_FULL=1): legA와 동일 조건의
#      전체 판. 부분/전체 사다리 두 판의 공정 비교(렌더러·기하 동일).
#   * FORCE_FULL에서도 더티 0 프레임은 스킵된다(스킵 제거 아님 — T2 계약).
#     present=skip 라인은 전 레그에서 세어 혼입을 명시한다.
#   * rect는 클라 커밋 단위(표면 전체)다 — 셀 단위 rect는 v1 백로그(스펙
#     결정 5 근거). 그래서 부분 rect = 터미널 표면 크기.
#
# 판정(스크립트 규약): DIRTY-OK(...)/DIRTY-FAIL(원인) — 수치 미달은 정직
# 원장(행별 이유)으로 rc=0(honest-fail). 인프라 실패(빌드·부팅·ping·터미널
# 런치·리사이즈·compst 무음)만 hard FAIL rc=1.
#
# 표준: WSLg :0, setsid 부팅, selftest 캐논 495(WSL 내부 리다이렉트 — Git Bash
# 파이프는 조각 유실), 브래킷 pkill(빌드 앞 — 살아있는 ELF는 drvfs unlink 불가),
# pkill -9 에스컬레이션(서버 SIGTERM 흡수 유예), Δ=/proc utime+stime(ps %cpu는
# 수명 평균 — docs/78 §5.5 렛슨), 22s 창([compst]는 8합성당 1인쇄 — 10s 창엔
# 표본 ~3라인뿐이라 ms 표본 확보를 22s로).
#
# 실행 표준(WSL 밖에서):
#   MSYS_NO_PATHCONV=1 wsl.exe -d Ubuntu-24.04 -- bash /mnt/i/progwork/JKENGINE/engine/tools/probes/wsl_dirty_present.sh
set -u
cd /mnt/i/progwork/JKENGINE/engine

FAIL() { echo "TASKDP-FAIL: $*"; exit 1; }
CLK_TCK=$(getconf CLK_TCK)
RES=$(mktemp /tmp/tdp_ab_XXXX.txt)

echo "=== 1. bracketed pre-clean (살아있는 jkdesktop이 재링크를 막는다) ==="
pkill -f 'buildwsl/[j]kdesktop' 2>/dev/null
sleep 1
pkill -9 -f 'buildwsl/[j]kdesktop' 2>/dev/null
sleep 1
LEFT=$(ps -eo args | grep -i 'buildwsl/[j]kdesktop' | wc -l)
[ "$LEFT" -eq 0 ] || FAIL "pre-clean left $LEFT jkdesktop process(es) alive"

echo "=== 2. ninja rebuild (buildwsl) ==="
ninja -C buildwsl -j4 >/tmp/tdp_build.log 2>&1
BUILD_RC=${PIPESTATUS[0]}
echo "WSL-BUILD-RC=$BUILD_RC"
[ "$BUILD_RC" -eq 0 ] || FAIL "ninja rebuild rc=$BUILD_RC"
[ -x buildwsl/jkdesktop ] || FAIL "buildwsl/jkdesktop missing after rebuild"

echo "=== 3. WSL selftest (캐논 495 — 소스 무변경 빌드 안정화 용도) ==="
timeout 420 ./buildwsl/jkdesktop test >/tmp/tdp_st.log 2>&1
ST_RC=$?
ST_FAIL=$(grep -ac '^\[FAIL\]' /tmp/tdp_st.log)
ST_PASS=$(grep -ac '^\[PASS\]' /tmp/tdp_st.log)
echo "selftest rc=$ST_RC PASS=$ST_PASS FAIL=$ST_FAIL"
grep -aq 'AppSelfTest: 0 failure(s)' /tmp/tdp_st.log \
    || FAIL "selftest summary missing 'AppSelfTest: 0 failure(s)'"
[ "$ST_FAIL" -eq 0 ] || FAIL "selftest FAIL=$ST_FAIL — log: /tmp/tdp_st.log"
[ "$ST_RC" -eq 0 ] || FAIL "selftest rc=$ST_RC"
[ "$ST_PASS" -ge 495 ] || echo "WARN: selftest PASS=$ST_PASS < 캐논 495 (계보 이탈 — T5 원장 기록 필요)"

# ---------------------------------------------------------------- A/B 측정
# 인자: 레그 라벨, "resize" | "noresize", 이후 env 전달("env" 형태 그대로)
run_leg() {
    LEG=$1; RESIZE=$2; shift 2
    LOG=/tmp/tdp_srv_${LEG}.log
    echo "=== leg $LEG boot (resize=$RESIZE $*) ==="
    pkill -f 'buildwsl/[j]kdesktop' 2>/dev/null
    sleep 1
    pkill -9 -f 'buildwsl/[j]kdesktop' 2>/dev/null
    sleep 1
    rm -f /tmp/JKWindowServerPipe.sock
    env DISPLAY=:0 JK_CPU_TRACE=1 "$@" setsid nohup ./buildwsl/jkdesktop --server \
        >"$LOG" 2>&1 &
    sleep 6
    SRV_PID=$(pgrep -f 'buildwsl/[j]kdesktop --server' | head -1)
    [ -n "$SRV_PID" ] \
        || FAIL "leg $LEG: no server process 6s after setsid boot — log tail: $(tail -3 "$LOG" | tr '\n' ' ')"
    PING_OUT=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"ping","args":{}}' 2>/dev/null | grep -a '{' | head -1)
    printf '%s' "$PING_OUT" | grep -aq '"ok":true' \
        || FAIL "leg $LEG: ping did not ok — reply: $PING_OUT"
    L_OUT=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"launch_app","args":{"app":"terminal"}}' 2>/dev/null | grep -a '{' | head -1)
    printf '%s' "$L_OUT" | grep -aq '"ok":true' \
        || FAIL "leg $LEG: launch_app terminal failed — reply: $L_OUT"
    echo "leg $LEG: server pid=$SRV_PID (서버 창 1280x720 — main.cpp 고정, 역치 산치 원료)"
    if [ "$RESIZE" = "resize" ]; then
        # launch_app ok 응답이 클라 등록보다 빠를 수 있다(선례 wsl_taskmgr_stats.sh는
        # sleep 5 후 list_windows) — 재시도 루프로 등장 대기.
        TERM_ID=""
        WIN_OUT=""
        for _try in 1 2 3 4 5; do
            sleep 2
            WIN_OUT=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)
            TERM_ID=$(printf '%s' "$WIN_OUT" |
                grep -aoE '\{"id":[^}]*"title":"Terminal"[^}]*\}' | head -1 |
                sed -n 's/.*"id":\([0-9]*\),.*/\1/p')
            [ -n "$TERM_ID" ] && break
        done
        [ -n "$TERM_ID" ] || FAIL "leg $LEG: list_windows has no Terminal window (5회 재시도) — reply: $WIN_OUT"
        R_OUT=$(timeout 12 ./buildwsl/jkdesktop agentctl \
            '{"tool":"window_resize","args":{"id":'"$TERM_ID"',"w":420,"h":300}}' 2>/dev/null | grep -a '{' | head -1)
        printf '%s' "$R_OUT" | grep -aq '"ok":true' \
            || FAIL "leg $LEG: window_resize(Terminal 420x300) failed — reply: $R_OUT"
        echo "leg $LEG: terminal id=$TERM_ID resized 420x300 (역치 40% 통과용) — settle 3s"
        sleep 3
    else
        echo "leg $LEG: 터미널 기본 기하 800x500 유지(43.4% of 1280x720) — settle 5s"
        sleep 5
    fi

    OFF1=$(wc -c < "$LOG")
    DELTA1=$(awk '{print $14+$15}' /proc/$SRV_PID/stat 2>/dev/null)
    [ -n "$DELTA1" ] || FAIL "leg $LEG: /proc/$SRV_PID/stat unreadable"
    CLT1=$(for p in $(pgrep -f 'buildwsl/[j]kdesktop'); do
               [ "$p" != "$SRV_PID" ] && awk '{print $14+$15}' /proc/$p/stat 2>/dev/null
           done | awk '{s+=$1} END{print s+0}')
    sleep 22
    OFF2=$(wc -c < "$LOG")
    DELTA2=$(awk '{print $14+$15}' /proc/$SRV_PID/stat 2>/dev/null)
    [ -n "$DELTA2" ] || FAIL "leg $LEG: /proc stat unreadable at t2 (server died mid-window)"
    CLT2=$(for p in $(pgrep -f 'buildwsl/[j]kdesktop'); do
               [ "$p" != "$SRV_PID" ] && awk '{print $14+$15}' /proc/$p/stat 2>/dev/null
           done | awk '{s+=$1} END{print s+0}')
    IDLE_SRV=$(awk -v d=$((DELTA2-DELTA1)) -v hz="$CLK_TCK" -v t=22 'BEGIN{printf "%.1f", d/hz/t*100}')
    IDLE_CLT=$(awk -v d=$((CLT2-CLT1)) -v hz="$CLK_TCK" -v t=22 'BEGIN{printf "%.1f", d/hz/t*100}')

    tail -c +"$((OFF1+1))" "$LOG" | head -c "$((OFF2-OFF1))" >/tmp/tdp_win.txt
    echo "--- leg $LEG [compst] window (22s) ---"
    grep -a 'compst' /tmp/tdp_win.txt || echo "(no compst lines in window)"
    COMP_TOTAL=$(grep -ac 'compst' /tmp/tdp_win.txt || true)
    [ "$COMP_TOTAL" -gt 0 ] \
        || FAIL "leg $LEG: zero [compst] lines in 22s window (JK_CPU_TRACE 무음 — 부팅 죽음·계측 파열)"
    SKIP_CNT=$(grep -ac 'present=skip' /tmp/tdp_win.txt || true)
    FULL_CNT=$(grep -ac 'present=full' /tmp/tdp_win.txt || true)
    DIRTY_CNT=$(grep -ac 'present=dirty(' /tmp/tdp_win.txt || true)
    COMPSTAT=$(grep -a 'cpustat] sdl' /tmp/tdp_win.txt | tail -3 | tr '\n' '|')
    FULL_MS=$(grep -a 'present=full=' /tmp/tdp_win.txt \
        | sed -n 's/.*present=full=\([0-9.]*\).*/\1/p')
    DIRTY_MS=$(grep -a 'present=dirty(' /tmp/tdp_win.txt \
        | sed -n 's/.*present=dirty([0-9]*)=\([0-9.]*\).*/\1/p')
    RECOUNT=$(grep -ao 'present=dirty([0-9]*)' /tmp/tdp_win.txt \
        | sed -n 's/.*dirty(\([0-9]*\)).*/\1/p' | awk '{s+=$1} END{print s+0}')
    echo "LEG-$LEG: IDLE_SRV=$IDLE_SRV% IDLE_CLIENTS=$IDLE_CLT% SKIP=$SKIP_CNT FULL=$FULL_CNT DIRTY=$DIRTY_CNT RECTS=$RECOUNT"
    echo "LEG-$LEG: cpustat: $COMPSTAT"
    FSTAT=$(printf '%s' "$FULL_MS" | awk 'BEGIN{n=0;s=0;m=0} {n++;s+=$1;if($1>m)m=$1} END{if(n)printf "%.2f %.2f %d",s/n,m,n; else printf "0 0 0"}')
    DSTAT=$(printf '%s' "$DIRTY_MS" | awk 'BEGIN{n=0;s=0;m=0} {n++;s+=$1;if($1>m)m=$1} END{if(n)printf "%.2f %.2f %d",s/n,m,n; else printf "0 0 0"}')
    # 판정부 파싱 편의 — 메트릭별 분리 행(한 행에 여수치 심으면 접두 sed가
    # 공백 뒤를 못 자른다 — 1차 실측 트랩)
    echo "$LEG IDLE_SRV=$IDLE_SRV" >>"$RES"
    echo "$LEG IDLE_CLT=$IDLE_CLT" >>"$RES"
    echo "$LEG SKIP=$SKIP_CNT" >>"$RES"
    echo "$LEG FULL=$FULL_CNT" >>"$RES"
    echo "$LEG DIRTY=$DIRTY_CNT" >>"$RES"
    echo "$LEG RECTS=$RECOUNT" >>"$RES"
    echo "$LEG FSTAT=$FSTAT" >>"$RES"
    echo "$LEG DSTAT=$DSTAT" >>"$RES"
}

run_leg leg0 noresize
run_leg leg1 noresize SDL_RENDER_DRIVER=software
run_leg legA resize  SDL_RENDER_DRIVER=software
run_leg legB resize  SDL_RENDER_DRIVER=software JK_PRESENT_FORCE_FULL=1

echo "=== cleanup (bracketed pkill + SIGTERM 흡수 대비 -9 에스컬레이션) ==="
pkill -f 'buildwsl/[j]kdesktop' 2>/dev/null
sleep 1
pkill -9 -f 'buildwsl/[j]kdesktop' 2>/dev/null
sleep 1
LEFT=$(ps -eo args | grep -i 'buildwsl/[j]kdesktop' | wc -l)
[ "$LEFT" -eq 0 ] || echo "WARN: $LEFT jkdesktop process(es) survived teardown"

# ---------------------------------------------------------------- 판정
IDLE0=$(grep -a '^leg0 IDLE_SRV=' "$RES" | head -1 | sed 's/.*IDLE_SRV=//')
IDLE1=$(grep -a '^leg1 IDLE_SRV=' "$RES" | head -1 | sed 's/.*IDLE_SRV=//')
IDLEA=$(grep -a '^legA IDLE_SRV=' "$RES" | head -1 | sed 's/.*IDLE_SRV=//')
IDLEB=$(grep -a '^legB IDLE_SRV=' "$RES" | head -1 | sed 's/.*IDLE_SRV=//')
# FSTAT/DSTAT 행 = "legN FSTAT=mean max cnt DSTAT=mean max cnt"
DIRTY1=$(grep -a '^leg1 DIRTY=' "$RES" | head -1 | sed 's/.*DIRTY=//' | awk '{print $1}')
DIRTYA=$(grep -a '^legA DIRTY=' "$RES" | head -1 | sed 's/.*DIRTY=//' | awk '{print $1}')
SKIPA=$(grep -a '^legA SKIP=' "$RES" | head -1 | sed 's/.*SKIP=//' | awk '{print $1}')
SKIPB=$(grep -a '^legB SKIP=' "$RES" | head -1 | sed 's/.*SKIP=//' | awk '{print $1}')
F1LINE=$(grep -a '^leg1 DSTAT=' "$RES" | head -1 | sed 's/^leg1 DSTAT=//')
AMLINE=$(grep -a '^legA DSTAT=' "$RES" | head -1 | sed 's/^legA DSTAT=//')
ABL=$(grep -a '^legA FSTAT=' "$RES" | head -1 | sed 's/^legA FSTAT=//')
BBL=$(grep -a '^legB FSTAT=' "$RES" | head -1 | sed 's/^legB FSTAT=//')
DMEANA=$(printf '%s' "$AMLINE" | awk '{print $1}')
DMAXA=$(printf '%s' "$AMLINE" | awk '{print $2}')
FMEANA=$(printf '%s' "$ABL" | awk '{print $1}')
FMEANB=$(printf '%s' "$BBL" | awk '{print $1}')
FMAXB=$(printf '%s' "$BBL" | awk '{print $2}')

echo "=== A/B 판정 ==="
echo "leg0 GL현행:     idle=${IDLE0}% (present=full — 기준선 6.7% 대조)"
echo "leg1 SW+기본기하: idle=${IDLE1}% dirty_count=$DIRTY1 (역치 접힘 실증 — 0이면 800x500=43.4%가 40%를 넘어 full 접힘)"
echo "legA SW+축소:     idle=${IDLEA}% dirty=$DIRTYA skip=$SKIPA dirty_mean=${DMEANA}ms dirty_max=${DMAXA}ms (full_mean=${FMEANA}ms)"
echo "legB SW+FF:      idle=${IDLEB}% skip=$SKIPB full_mean=${FMEANB}ms full_max=${FMAXB}ms"

# 결정 규약(스크립트 선언): DIRTY-OK = legA의 dirty mean < legB의 full mean
# AND legA idle ≤ legB idle + 0.5pp(측정 소음). 그 밖은 행별 이유로 DIRTY-FAIL.
VERDICT=$(awk -v dm="$DMEANA" -v fm="$FMEANB" -v ia="$IDLEA" -v ib="$IDLEB" \
    -v i0="$IDLE0" -v i1="$IDLE1" -v d1="$DIRTY1" -v da="$DIRTYA" 'BEGIN{
    if (da + 0 == 0) {
        printf "DIRTY-VERDICT: DIRTY-FAIL(부분 경로 미발화 — legA에 present=dirty(N) 라인 0)"
    } else if (fm + 0 == 0) {
        printf "DIRTY-VERDICT: DIRTY-FAIL(전체 판 표본 0 — legB present=full 부족)"
    } else if (fm < dm + 0.001) {
        printf "DIRTY-VERDICT: DIRTY-FAIL(present 절감 없음 — dirty mean %.2fms ≥ full mean %.2fms, idle %.1f%% vs %.1f%%)", dm, fm, ia, ib
    } else if (ia > ib + 0.5) {
        save = (fm - dm) / fm * 100
        printf "DIRTY-VERDICT: DIRTY-FAIL(present 절감 %.2f→%.2fms(-%.0f%%)이나 idle 역행 %.1f%% vs %.1f%% — 표본 편차, leg1 idle %.1f%% 참조)", fm, dm, save, ia, ib, i1
    } else {
        save = (fm - dm) / fm * 100
        printf "DIRTY-VERDICT: DIRTY-OK(present mean %.2fms→%.2fms 절감 %.2fms(%.0f%%), idle %.1f%%(부분) vs %.1f%%(전체)·현행GL %.1f%%·기준선 6.7%%)", fm, dm, fm - dm, save, ia, ib, i0
    }
}')
echo "$VERDICT"

rm -f "$RES" /tmp/tdp_win.txt
echo "TASKDP-END"
# honest-fail 원칙: 수치 미달(DIRTY-FAIL)은 원장 목적이라 rc=0.
# hard FAIL(빌드·부팅·ping·터미널·리사이즈·계측 무음)만 FAIL()에서 exit 1.
exit 0