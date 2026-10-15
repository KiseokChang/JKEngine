#!/usr/bin/env bash
# T2 WSL vplayer 조그 휠 흐름 실측 probe (#100 — plan
# docs/superpowers/plans/2026-10-12-vplayer-jog-flow.md task-2; wsl_music.sh
# 위생 수형 + wsl_vp_cycle.sh 접착제 직행 수형 + wsl_ttc_check.sh 직행 클라
# 수형 + T1 Windows smoke(engine/tmp/t1_wheel_smoke.ps1 untracked)의 WSL 동형).
#
# 목적(#100 T1 — 8d83027 휠=연속 워크+관성 감쇠): 합성 클립 열기 → 휠 시퀀스
# (send_input wheel — 기존 원문 수형) → [vpt13] 세션/glide 중 pos 원문 + 관성
# 정지 후 스냅 pos 원문 → REV 동형 → 무음 검증 축(세션 중 get_status
# paused=true + 스냅 후 복원) → verdict. 2런(재현 등호)·WSL 캐논 등호 원문
# (코드 무변경 — Win 691/WSL 668/posix 304).
#
# T1 리뷰 concern #2 이관 원문(합성 경로 계약): 런 1의 실기기 함정 — Knox
# 토스트가 노브 위 커서(실 입력 경로)를 덮어 wheel 세션 0 샘플.본 probe는
# **전량 합성 경로**다: send_input은 표면 직접 주입(z-순서/진짜 커서 무관 —
# run4 원문 계보·t1_evidence run5 성립)·클릭 1발로 io.MousePos를 노브 위에
# 파킹(1프레임 degenerate drag — 무해 스냅) 후 wheel dy만 보낸다. 진짜
# 커서/토스트 의존 0.
#
# [vpt13] 표시 축: **클라 직행 stderr 캡처**(docs/60 §13.2 원장 — 서버
# 리다이렉트는 죽은 길, 클라 프로세스를 직접 스폰해 stderr를 파일로) —
# wsl_ttc_check.sh 수형 + T1 smoke의 RedirectStandardError 동형.
#
# 측정 원료(원문):
#   * 클라 stderr [vpt13] D/T/lo 스트림(ClientVPlayerApp.cpp 펌프 100ms
#     스로틀 — docs/50 §11 probe gate) = 세션 진행+관성 glide의 진실원
#     (get_status pos는 ClockNow 오디오 클럭 — 세션 중 진입 전 위치를
#     보인다[도구 관측 한계 — T1 Concern #4, 결함 아님]).
#   * get_status pos = 스냅(단일 출구 finishScrub) 착지 원문 — 스냅 후 정확.
#   * get_status paused = 무음 계약 원문(세션 중 true = 장치 정지+SetJog(true)
#     디코드 스킵, 스냅 후 false = jogWasPlaying_ 복원 — PlayerCore 무변조
#     무음 경로, T1 리뷰 3축 PASS 계보).
#
# 판정 구조(스크립트 규약 — wsl_music.sh 선형 수형): 인프라 실패(빌드·
# selftest·부팅·ping·launch·창 부재·클립 개봉·기하·시드·소각)만 hard FAIL
# rc=1. 흐름 수치(JOG-FAIL)는 honest-fail rc=0 — 최종 판정은 리포트 원장.
#
# 위생 계약 전부(wsl_family 승계 — 계약 원문):
#   * #98 runner guard: 라이브 jkdesktop 서버 있으면 probe-외부라 [SKIP]+rc=0
#     (아무 것도 pkill/rm하지 않는다 — ENTRY 기록도 하지 않는다).
#   * 브래킷 pkill+-9 에스컬레이션+pre-clean rm 가드(rm on a LIVE socket entombs
#     the parked listener — #98 원장).
#   * permissions.json = 프로브 소유 스테이지(buildwsl/에 부재 확인 후
#     send_input 1키 allow — 전면 allow 런타임 파일은 프로브 소유 아님 —
#     wsl_music.sh STEP 5 수형)·END에서 소각(ENTRY 원복).
#   * selftest 출력 WSL 내부 리다이렉트·setsid 부팅·pings 리트라이(부팅 지연
#     ~33s 원장 수용 — wsl_music T3 원장).
#   * 캡처 회수 단열: /tmp 결과 → engine/tmp/t2wheel 단일 cp(ls·혼입 금지).
#
# 실행 표준(WSL 밖 Git Bash):
#   MSYS_NO_PATHCONV=1 wsl.exe -d Ubuntu-24.04 -- bash /mnt/i/progwork/JKENGINE/engine/tools/probes/wsl_jog_flow.sh
# 2런: JOG_RUN=r1 / JOG_RUN=r2 두 번 실행 — 리포트가 JOG-RUN-VALS 2행으로
# 재현 등호 원장을 남긴다.
set -u
cd /mnt/i/progwork/JKENGINE/engine || exit 1

FAIL() { echo "JOG-WSL-FAIL: $*"; cleanup_burst "$*"; exit 1; }
LOGDIR=/tmp/jog
mkdir -p "$LOGDIR"
RECEIVE=/mnt/i/progwork/JKENGINE/engine/tmp/t2wheel
CANON_WSL_SELFTEST=668   # 캐논 WSL 축 — T1(8d83027) 승계·신설 어설션 0 계약
CLIP=/mnt/i/progwork/JKENGINE/tmp/vpt2_test.mp4   # 엔진 소유 합성 클립(30.0s/30fps)
JOGRUN=${JOG_RUN:-r1}
CLOG=$LOGDIR/jog_client_$JOGRUN.err   # 클라 직행 stderr — [vpt13] 진실원
SLOG=$LOGDIR/jog_srv_$JOGRUN.log

# hard-FAIL 경로도 소각 계약 원복(permissions.json+probe가 스폰한 서버/클라
# 만 — ENTRY 가드로 probe-외부 서버는 앞서 [SKIP]됐다).
cleanup_burst() {
    rm -f buildwsl/permissions.json; echo "PERM-BURIED: permissions.json removed (hard-fail)"
    [ -n "${SRV_PID:-}" ] && { pkill -f 'buildwsl/[j]kdesktop --server' 2>/dev/null; echo "SRV-BURIED: probe server stopped"; }
    [ -n "${CLPID:-}" ]  && { pkill -f 'buildwsl/[j]kdesktop --client vplayer' 2>/dev/null; echo "CLI-BURIED: probe client stopped"; }
    sleep 1
    pkill -9 -f 'buildwsl/[j]kdesktop' 2>/dev/null
    return 0
}

# T1 smoke 원문 상수(vpt13 계보 — 960x640 레이어): tick = sPerRev/16 =
# 3.75/16 = 0.234375 s(dur/8 클램프)·노브 중심 = 창 원점 + (895,584)(지름 64 —
# ±32px 여유). env 오버라이드 창구(정찰-재보정 수형 — wsl_music 선례).
JOG_TICK=${JOG_TICK:-3.75};  JOG_TICKS16=16
JOG_KNOB_X=${JOG_KNOB_X:-895}; JOG_KNOB_Y=${JOG_KNOB_Y:-584}
# 흐름 연속성 상한 = 감쇠 최대 계단(8틱/100ms 샘플) + 1프레임(30fps 클립)
# + 관성 진입 여유 — T1 smoke bound 수형.
BOUND=$(awk "BEGIN{printf \"%.3f\", 8.0*0.1 + 1.0/30.0 + 0.3}")   # 1.133

echo "=== JOG-FLOW RUN=$JOGRUN start $(date +%T)"

# ---------------------------------------------------------------- fail-closed 가드
if [ -z "${WSL_DISTRO_NAME:-}" ]; then
  echo "JOG-WFAIL: not inside WSL — 실행 축 계약 위반 (wsl.exe -d Ubuntu-24.04 -- bash .../wsl_jog_flow.sh)"
  exit 1
fi
ls -la "$CLIP" >/dev/null 2>&1 || FAIL "clip missing: $CLIP (엔진 소유 합성 클립만 계약)"

# #98 runner guard — this runner's bracketed pkill/rm 사슬 전부를 커버한다.
if pgrep -f '[j]kdesktop --server' >/dev/null 2>&1; then
    echo "[SKIP] jog-flow: a live jkdesktop server is running — #98 runner guard (nothing pkilled, nothing rm'd)"
    exit 0
fi

echo "=== 0. ENTRY 상태 기록 (END 원복 판정용)"
# 동일 라벨의 이전 런 잔상(중단/수정 런)은 probe 소유 /tmp 파일 — 소각 후 재런.
rm -f "$LOGDIR"/jog_srv_$JOGRUN.log "$LOGDIR"/jog_client_$JOGRUN.err \
      "$LOGDIR"/jog_client_$JOGRUN.out "$LOGDIR"/jog_client_$JOGRUN.log \
      "$LOGDIR"/jog_build_$JOGRUN.log "$LOGDIR"/jog_st_$JOGRUN.log \
      "$LOGDIR"/${JOGRUN}_fwd_stream.txt "$LOGDIR"/${JOGRUN}_rev_stream.txt \
      "$LOGDIR"/${JOGRUN}_mute_stream.txt
ENTRY_PERM=0
[ -f buildwsl/permissions.json ] && ENTRY_PERM=1
ENTRY_SLOG=0
[ -f "$SLOG" ] && ENTRY_SLOG=1
echo "ENTRY-STATE: permissions.json=$ENTRY_PERM srv-log=$ENTRY_SLOG run=$JOGRUN"
[ "$ENTRY_PERM" -eq 0 ] \
    || FAIL "buildwsl/permissions.json already exists — probe는 런타임 permissions 파일을 건드릴 수 없다(전면 allow 파일은 프로브 소유 아님)"

restore_and_exit() {
    echo "=== 복원 (early exit)"
    rm -f buildwsl/permissions.json
    echo "PERM-BURIED: permissions.json removed"
    pkill -f 'buildwsl/[j]kdesktop' 2>/dev/null
    sleep 1
    pkill -9 -f 'buildwsl/[j]kdesktop' 2>/dev/null
    sleep 1
    echo "JOG-RESTORED: probe-소유 잔상 0"
    exit ${1:-1}
}

echo "=== 1. bracketed pre-clean"
pkill -f 'buildwsl/[j]kdesktop' 2>/dev/null
sleep 1
pkill -9 -f 'buildwsl/[j]kdesktop' 2>/dev/null
sleep 1
LEFT=$(ps -eo args | grep -ic 'buildwsl/[j]kdesktop' || true)
[ "$LEFT" -eq 0 ] || FAIL "pre-clean left $LEFT jkdesktop alive"

echo "=== 2. ninja 증분 (소스 무변경 probe — 빌드 신선도만 확인)"
ninja -C buildwsl -j3 >"$LOGDIR/jog_build_$JOGRUN.log" 2>&1
B_RC=$?
echo "JOG-BUILD-RC=$B_RC"
[ "$B_RC" -eq 0 ] || { tail -5 "$LOGDIR/jog_build_$JOGRUN.log"; FAIL "ninja rc=$B_RC"; }
[ -x buildwsl/jkdesktop ] || FAIL "buildwsl/jkdesktop missing"
[ -f buildwsl/jkapp_vplayer.so ] || FAIL "buildwsl/jkapp_vplayer.so missing (조그 재설계 배포 확인 실패)"

echo "=== 3. WSL selftest (캐논 $CANON_WSL_SELFTEST 등호 원문 — 코드 무변경 계약)"
timeout 420 ./buildwsl/jkdesktop test >"$LOGDIR/jog_st_$JOGRUN.log" 2>&1
ST_RC=$?
ST_FAIL=$(grep -ac '^\[FAIL\]' "$LOGDIR/jog_st_$JOGRUN.log")
ST_PASS=$(grep -ac '^\[PASS\]' "$LOGDIR/jog_st_$JOGRUN.log")
echo "selftest rc=$ST_RC PASS=$ST_PASS FAIL=$ST_FAIL"
grep -aq 'AppSelfTest: 0 failure(s)' "$LOGDIR/jog_st_$JOGRUN.log" \
    || FAIL "selftest summary missing 'AppSelfTest: 0 failure(s)'"
[ "$ST_FAIL" -eq 0 ] || FAIL "selftest FAIL=$ST_FAIL"
[ "$ST_RC" -eq 0 ] || FAIL "selftest rc=$ST_RC"
if [ "$ST_PASS" -eq "$CANON_WSL_SELFTEST" ]; then
    echo "CANON-WSL-INCLUSION=FULL (WSL $ST_PASS 등호 — 코드 무변경 원문)"
else
    echo "CANON-WSL-INCLUSION=DEVIANT(PASS=$ST_PASS vs 캐논 $CANON_WSL_SELFTEST — 원장)" \
        && echo "WARN: selftest PASS=$ST_PASS != 캐논 $CANON_WSL_SELFTEST — 리포트 원장 필수"
fi

echo "=== 4. permissions.json 프로브 소유 스테이지 (send_input allow — END 소각)"
printf '{\n    "send_input": "allow"\n}\n' > buildwsl/permissions.json
[ -f buildwsl/permissions.json ] || FAIL "permissions.json write failed"
grep -aq '"send_input"' buildwsl/permissions.json || FAIL "permissions.json staging malformed"
echo "PERM-WIRE-OK: buildwsl/permissions.json (send_input allow — END에서 소각)"

echo "=== 5. 서버 부팅 (setsid nohup — wsl_family 수형)"
if pgrep -f '[j]kdesktop --server' >/dev/null 2>&1; then
    echo "NOTE #98: live server holds /tmp/JKWindowServerPipe.sock — pre-clean rm skipped"
else
    rm -f /tmp/JKWindowServerPipe.sock
fi
env DISPLAY=:0 setsid nohup ./buildwsl/jkdesktop --server >"$SLOG" 2>&1 &
sleep 6
SRV_PID=$(pgrep -f 'buildwsl/[j]kdesktop --server' | head -1)
[ -n "$SRV_PID" ] || { echo "--- boot log tail:"; tail -5 "$SLOG"; FAIL "no server 6s after boot"; }
echo "server pid=$SRV_PID"
PING=""
for i in $(seq 1 30); do
    PING=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"ping","args":{}}' 2>/dev/null | grep -a '{' | head -1)
    printf '%s' "$PING" | grep -aq '"ok":true' && break
    sleep 2
done
printf '%s' "$PING" | grep -aq '"ok":true' || FAIL "ping failed — $PING"
echo "PING-OK"

echo "=== 6. 클라 직행 스폰 (vplayer — stderr [vpt13] 캡처 파이프)"
rm -f "$CLOG" "$LOGDIR/jog_client_$JOGRUN.out"
: > "$CLOG"
# 클라 connect는 단발이라 서버 리스너 리바인드 틈새(#98 원장 계열 — 런 2
# 부팅 직후 원문 "ConnectClient failed: Connection refused")에 빠지면 클라가
# 즉사한다 — 재시도 루프(스폰 5회 — 승인 불필요 none 등급 도구만 씀).
CL_CONNECTED=0
for attempt in 1 2 3 4 5; do
    env DISPLAY=:0 setsid nohup ./buildwsl/jkdesktop --client vplayer \
        >"$LOGDIR/jog_client_$JOGRUN.out" 2>"$CLOG" &
    for i in 1 2 3 4 5 6 7 8 9 10; do
        sleep 0.5
        grep -aq 'connected surfaceId' "$CLOG" && { CL_CONNECTED=1; break; }
        grep -aq 'failed to connect' "$CLOG" && break   # 조기 사망 — 재스폰
    done
    [ "$CL_CONNECTED" -eq 1 ] && break
    echo "JOG-CONNECT-RETRY: attempt $attempt — stderr: $(head -2 "$CLOG" | tr '\n' ' ')"
    pkill -9 -f 'buildwsl/[j]kdesktop --client vplayer' 2>/dev/null
    sleep 1
done
CLPID=$(pgrep -f 'buildwsl/[j]kdesktop --client vplayer' | head -1)
[ "$CL_CONNECTED" -eq 1 ] && [ -n "$CLPID" ] \
    || { echo "--- client stderr head:"; head -5 "$CLOG"; FAIL "vplayer client connect failed (5 attempts)"; }
echo "client pid=$CLPID (connected on attempt $attempt)"
VPLAYWIN=""
for i in $(seq 1 12); do
    sleep 2
    VPLAYWIN=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)
    VPLAYWIN=$(printf '%s' "$VPLAYWIN" | grep -aoE '\{"id":[^}]*"title":"Video Player"[^}]*\}' | head -1)
    [ -n "$VPLAYWIN" ] && break
done
[ -n "$VPLAYWIN" ] || FAIL "no Video Player window — list_windows: ${VPLAYWIN:-none}"
echo "VPWIN: $VPLAYWIN"
WID=$(printf '%s' "$VPLAYWIN" | sed -n 's/.*"id":\([0-9]*\),.*/\1/p')
WX=$(printf '%s' "$VPLAYWIN" | sed -n 's/.*"x":\(-\?[0-9]*\),.*/\1/p')
WY=$(printf '%s' "$VPLAYWIN" | sed -n 's/.*"y":\(-\?[0-9]*\),.*/\1/p')
WW=$(printf '%s' "$VPLAYWIN" | sed -n 's/.*"w":\([0-9]*\),.*/\1/p')
WH=$(printf '%s' "$VPLAYWIN" | sed -n 's/.*"h":\([0-9]*\),.*/\1/p')
echo "JOG-GEO: id=$WID x=$WX y=$WY w=$WW h=$WH (기본 960x640 — SetWindowRect 계보)"
if [ "${WW:-0}" -lt 958 ] || [ "${WW:-0}" -gt 962 ] || [ "${WH:-0}" -lt 638 ] || [ "${WH:-0}" -gt 642 ]; then
    echo "NOTE: 기본 기하 이탈 — window_resize 960x640 시도(상수 보정 재정착 원장)"
    R=$(timeout 12 ./buildwsl/jkdesktop agentctl "{\"tool\":\"window_resize\",\"args\":{\"id\":$WID,\"w\":960,\"h\":640}}" 2>/dev/null | grep -a '{' | head -1)
    printf '%s' "$R" | grep -aq '"ok":true' || FAIL "window_resize failed — $R"
    sleep 1
    VPLAYWIN=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)
    WW=$(printf '%s' "$VPLAYWIN" | sed -n 's/.*"w":\([0-9]*\),.*/\1/p')
    WH=$(printf '%s' "$VPLAYWIN" | sed -n 's/.*"h":\([0-9]*\),.*/\1/p')
    WX=$(printf '%s' "$VPLAYWIN" | sed -n 's/.*"x":\(-\?[0-9]*\),.*/\1/p')
    WY=$(printf '%s' "$VPLAYWIN" | sed -n 's/.*"y":\(-\?[0-9]*\),.*/\1/p')
    echo "JOG-GEO-2: x=$WX y=$WY w=$WW h=$WH"
fi
[ "${WW:-0}" -ge 958 ] && [ "${WW:-0}" -le 962 ] && [ "${WH:-0}" -ge 638 ] && [ "${WH:-0}" -le 642 ] \
    || FAIL "geometry mismatch w=$WW h=$WH — 노브 상수(960x640 계보) 상실: 정찰·재보정 런 필요"
KNOB_DX=$((WX + JOG_KNOB_X)); KNOB_DY=$((WY + JOG_KNOB_Y))
echo "JOG-KNOB: desktop point = ($KNOB_DX,$KNOB_DY) = 창 원점 + ($JOG_KNOB_X,$JOG_KNOB_Y)"
sleep 3   # 클라 초기 렌더 + 상태 안정화

# ---------------------------------------------------------------- 접착제 원문 헬퍼
ctl() { timeout 15 ./buildwsl/jkdesktop agentctl "$1" 2>/dev/null | grep -a '{' | head -1; }
ok()  { printf '%s' "${1:-}" | grep -aq '"ok":true'; }
FWDERR=0; REVERR=0; MUTEERR=0
CHK() { # $1=tag $2=결과(0/1) — 흐름 수치 판정(honest-fail — 카운터만)
    if [ "$2" -eq 0 ]; then echo "PASS: $1"; else echo "FAIL: $1 -- ${3:-}"; fi
}
GETSTAT=$(mktemp /tmp/jog_stat_XXXX)
status() { # get_status 원문 → $GETSTAT
    ctl '{"tool":"app_tool","args":{"app":"vplayer","tool":"get_status","args":{}}}' >"$GETSTAT"
    printf '%s' "$(cat "$GETSTAT")"
}
p_of()     { printf '%s' "$1" | sed -n 's/.*"pos":\(-\?[0-9.]*\).*/\1/p'; }
pause_of() { if printf '%s' "$1" | grep -aq '"paused":true'; then echo true; else echo false; fi; }
wheel() { # $1=dy — 합성 휠 1틱(send_input wheel 수형 — 표면 직접 주입)
    W=$(ctl "{\"tool\":\"send_input\",\"args\":{\"id\":$WID,\"op\":\"wheel\",\"dy\":$1}}")
    printf '%s' "$W" | grep -aq '"sent":true' || return 1
    return 0
}
knobclick() { # 노브 1클릭 = MouseMove 파킹(1프레임 degenerate drag — 무해 스냅 원문)
    C=$(ctl "{\"tool\":\"send_input\",\"args\":{\"id\":$WID,\"op\":\"click\",\"x\":$KNOB_DX,\"y\":$KNOB_DY}}")
    printf '%s' "$C" | grep -aq '"sent":true'
}
vpt13_of() { # $1=stderr 행 오프셋 → 그 뒤의 [vpt13] 행 수
    tail -n +"$((($1)+1))" "$CLOG" | grep -ac '^\[vpt13\]' || true
}
vpt13_dump() { # $1=오프셋 $2=outfile — 원문 단열 덤프
    tail -n +"$((($1)+1))" "$CLOG" | grep -a '^\[vpt13\]' > "$2" || true
}
vpt13_analyze() { # $1=stream file $2=dir(+1/-1) → "n maxdd tspan" 한 줄
    awk -v s="$2" '
        { if (match($0,/D=(-?[0-9.]+)/)) d=substr($0,RSTART+2,RLENGTH-2)
          if (match($0,/T=(-?[0-9.]+)/)) t=substr($0,RSTART+2,RLENGTH-2)
          if (n>0) { dd=d-pd; if (dd<0) dd=-dd
                     if (n>1 && dd>maxdd) maxdd=dd
                     if (n>1 && (s>0 ? (d < pd-0.04) : (d > pd+0.04))) badway=n }
          pd=d; if (n==0||t<mint) mint=t; if (n==0||t>maxt) maxt=t; n++ }
        END { ts=maxt-mint; if (ts<0) ts=-ts
              printf "%d %.3f %.3f %d\n", n, maxdd, ts, badway }
    ' "$1"
}

echo "=== 7. 클립 개봉 (app_tool open 직행 — wsl_vp_cycle 수형)"
OPEN=$(ctl "{\"tool\":\"app_tool\",\"args\":{\"app\":\"vplayer\",\"tool\":\"open\",\"args\":{\"path\":\"$CLIP\"}}}")
ok "$OPEN" || { echo "OPEN-REPLY: $OPEN"; FAIL "app_tool open failed"; }
echo "OPEN-REPLY: $OPEN"
ST=""; OPENED=""
for i in $(seq 1 16); do
    sleep 1
    ST=$(status)
    printf '%s' "$ST" | grep -aq '"opened":true' || continue
    [ "$(p_of "$ST")" != "" ] && { OPENED=1; break; }
done
[ "$OPENED" = "1" ] || FAIL "clip did not open — last status: ${ST:-none}"
ST=$(status)
DUR=$(printf '%s' "$ST" | sed -n 's/.*"dur":\([0-9.]*\).*/\1/p')
echo "JOG-DUR: $DUR (기대 30.0)"
AWK_DUR=$(awk "BEGIN{print ($DUR > 29.5 && $DUR < 30.5) ? 0 : 1}")
[ "$AWK_DUR" -eq 0 ] || FAIL "clip duration $DUR != 30 — tick 상수(sPerRev/16) 계보 불성립"

echo "=== 8. 팍 원료 상수 원문"
TICK=$(awk "BEGIN{printf \"%.6f\", $JOG_TICK/$JOG_TICKS16}")
RAW=$(awk "BEGIN{printf \"%.3f\", 10*$TICK}")
echo "JOG-TICKS: tick=$TICK raw10=$RAW (FWD/REV 각 = 틱률 버스터 원동량)"
# 클립 fps 30 → T1 bound 1.133 수형 그대로.

echo "=== 9. FWD leg — burst +10틱 → [vpt13] glide 원문 → 스냅 착지"
# park = 전제 상태(시드 계약) — toggle 1발로 못 미치면(레이아웃/릴레이 레이스
# 원장 — 런 1 원문) 3회까지 재시도하고 각 reply를 원문으로 남긴다.
PARK_OK=0
for i in 1 2 3; do
    PR=$(ctl '{"tool":"app_tool","args":{"app":"vplayer","tool":"play_pause","args":{}}}')
    echo "PARK-TRY-$i reply: $PR"
    ok "$PR" || { echo "NOTE: play_pause envelope — $PR"; }
    sleep 1
    ST=$(status)
    echo "PARK-TRY-$i: paused=$(pause_of "$ST") pos=$(p_of "$ST")"
    if [ "$(pause_of "$ST")" = "true" ]; then PARK_OK=1; break; fi
done
[ "$PARK_OK" -eq 1 ] || FAIL "pre-FWD park failed (3 toggles — paused=$(pause_of "$ST") pos=$(p_of "$ST"))"
ctl '{"tool":"app_tool","args":{"app":"vplayer","tool":"seek","args":{"seconds":10}}}' >/dev/null
sleep 1
SEED=$(p_of "$(status)")
echo "FWD-SEED: $SEED"
awk "BEGIN{e=$SEED-10.0; if(e<0)e=-e; exit (e<=0.5) ? 0 : 1}" \
    || { FWDERR=$((FWDERR+1)); echo "JOG-FAIL: FWD seed=$SEED != 10±0.5 (seek 시드 원문 부족)"; }
knobclick || FAIL "knob click failed"
sleep 1
OFF=$(wc -l < "$CLOG")
WHEEL_FAILS=0
for i in $(seq 1 10); do
    wheel 1 || { WHEEL_FAILS=$((WHEEL_FAILS+1)); continue; }
    sleep 0.025   # 합성 페이스 — T1 smoke 25ms 수형
done
echo "FWD-WHEEL: 10 sends, sends-failed=$WHEEL_FAILS"
[ "$WHEEL_FAILS" -eq 0 ] || FAIL "wheel send gate failed ($WHEEL_FAILS/10) — permissions.json send_input allow 확인"
sleep 0.25
vpt13_dump "$OFF" "$LOGDIR/${JOGRUN}_fwd_stream.txt"
NSAMP=$(vpt13_of "$OFF")
echo "FWD-STREAM-1: $NSAMP samples (engine/tmp 수취 대기 — END 단열 cp)"
sleep 1.9   # glide(~1.1s)+스냅 정산
ST=$(status)
LANDED=$(p_of "$ST")
PAUSED_SNAP=$(pause_of "$ST")
echo "FWD-SNAP: landed=$LANDED paused=$PAUSED_SNAP"
vpt13_dump "$OFF" "$LOGDIR/${JOGRUN}_fwd_stream.txt"   # 스냅 포함 원본 확정판
AN=$(vpt13_analyze "$LOGDIR/${JOGRUN}_fwd_stream.txt" 1)
read -r AN_N AN_MAXDD AN_TSPAN AN_BADWAY <<EOF
$AN
EOF
echo "FWD-ANALYZE: samples=$AN_N max|dD|=$AN_MAXDD Tspan=$AN_TSPAN badway=$AN_BADWAY bound=$BOUND"
ERRF=""
LATEST_T=$(tail -1 "$LOGDIR/${JOGRUN}_fwd_stream.txt" 2>/dev/null | sed -n 's/.*T=\(-\?[0-9.]*\).*/\1/p')
ERRF=$(awk "BEGIN{e=$LANDED-$LATEST_T; if(e<0)e=-e; printf \"%.3f\", e}" 2>/dev/null)
GLIDEF=$(awk "BEGIN{printf \"%.3f\", $LANDED-$SEED}")
echo "FWD-VALS: seed=$SEED T_last=$LATEST_T landed=$LANDED err=$ERRF glide=$GLIDEF"
[ "${AN_N:-0}" -ge 4 ] || { FWDERR=$((FWDERR+1)); echo "JOG-FAIL: FWD samples=$AN_N < 4 (세션 미개시 — 노브/게이트 원장)"; }
awk "BEGIN{exit ($AN_MAXDD+0 <= $BOUND) ? 0 : 1}" || { FWDERR=$((FWDERR+1)); echo "JOG-FAIL: FWD continuity max|dD|=$AN_MAXDD > bound $BOUND"; }
[ "${AN_BADWAY:-1}" -eq 0 ] || { FWDERR=$((FWDERR+1)); echo "JOG-FAIL: FWD direction — D가 감소한 샘플($AN_BADWAY)"; }
awk "BEGIN{exit ($AN_TSPAN+0 >= 2.30) ? 0 : 1}" || { FWDERR=$((FWDERR+1)); echo "JOG-FAIL: FWD Tspan=$AN_TSPAN < raw 2.344 (세션 진행 원문 부족)"; }
awk "BEGIN{exit ((\"$ERRF\"+0) <= 0.3) ? 0 : 1}" || { FWDERR=$((FWDERR+1)); echo "JOG-FAIL: FWD landing err=$ERRF > 0.3 (스냅 단일 출구 원문 부족)"; }
awk "BEGIN{exit ($GLIDEF+0 >= 2.45) ? 0 : 1}" || { FWDERR=$((FWDERR+1)); echo "JOG-FAIL: FWD glide=$GLIDEF < raw+0.1 (관성 연장 원문 부족)"; }
[ "$PAUSED_SNAP" = "true" ] || { FWDERR=$((FWDERR+1)); echo "JOG-FAIL: FWD parked after snap — paused=$PAUSED_SNAP"; }
P1=$(p_of "$(status)"); sleep 0.5; P2=$(p_of "$(status)")
DP=$(awk "BEGIN{d=$P2-$P1; if(d<0)d=-d; printf \"%.3f\", d}")
echo "FWD-STATIC: $P1 -> $P2 (d=$DP)"
awk "BEGIN{exit ($DP <= 0.02) ? 0 : 1}" || { FWDERR=$((FWDERR+1)); echo "JOG-FAIL: FWD pos static — d=$DP > 0.02"; }

echo "=== 10. REV leg — seek 15 → burst -10틱 → 동형 판정 (dir만)"
ctl '{"tool":"app_tool","args":{"app":"vplayer","tool":"seek","args":{"seconds":15}}}' >/dev/null
sleep 1
SEED2=$(p_of "$(status)")
echo "REV-SEED: $SEED2"
awk "BEGIN{e=$SEED2-15.0; if(e<0)e=-e; exit (e<=0.5) ? 0 : 1}" \
    || { REVERR=$((REVERR+1)); echo "JOG-FAIL: REV seed=$SEED2 != 15±0.5"; }
knobclick || FAIL "knob click failed"
sleep 1
OFF2=$(wc -l < "$CLOG")
for i in $(seq 1 10); do
    wheel -1 || { WHEEL_FAILS=$((WHEEL_FAILS+1)); continue; }
    sleep 0.025
done
echo "REV-WHEEL: 10 sends, sends-failed=$WHEEL_FAILS"
sleep 2.1
vpt13_dump "$OFF2" "$LOGDIR/${JOGRUN}_rev_stream.txt"
NSAMP2=$(vpt13_of "$OFF2")
echo "REV-STREAM-1: $NSAMP2 samples"
ST=$(status)
LANDED2=$(p_of "$ST")
echo "REV-SNAP: landed=$LANDED2"
# REV = T가 감소한다 — released T = 스트림의 **마지막** 샘플(FWD=T 증가 → 마지막
# 샘플이 max. 런 r1 원장: 첫 샘플을 넣은 probe 분석 버그 — REV err 2.666 유출).
T_END2=$(tail -1 "$LOGDIR/${JOGRUN}_rev_stream.txt" 2>/dev/null | sed -n 's/.*T=\(-\?[0-9.]*\).*/\1/p')
ERRR=$(awk "BEGIN{e=$LANDED2-$T_END2; if(e<0)e=-e; printf \"%.3f\", e}" 2>/dev/null)
GLIDER=$(awk "BEGIN{printf \"%.3f\", $SEED2-$LANDED2}")
echo "REV-VALS: seed=$SEED2 T_end=$T_END2 landed=$LANDED2 err=$ERRR glide=$GLIDER"
[ "${NSAMP2:-0}" -ge 4 ] || { REVERR=$((REVERR+1)); echo "JOG-FAIL: REV samples=$NSAMP2 < 4"; }
AN2=$(vpt13_analyze "$LOGDIR/${JOGRUN}_rev_stream.txt" -1)
read -r AN2_N AN2_MAXDD AN2_TSPAN AN2_BADWAY <<EOF
$AN2
EOF
echo "REV-ANALYZE: samples=$AN2_N max|dD|=$AN2_MAXDD Tspan=$AN2_TSPAN badway=$AN2_BADWAY"
awk "BEGIN{exit ($AN2_MAXDD+0 <= $BOUND) ? 0 : 1}" || { REVERR=$((REVERR+1)); echo "JOG-FAIL: REV continuity max|dD|=$AN2_MAXDD > bound $BOUND"; }
[ "${AN2_BADWAY:-1}" -eq 0 ] || { REVERR=$((REVERR+1)); echo "JOG-FAIL: REV direction — D가 증가한 샘플($AN2_BADWAY)"; }
awk "BEGIN{exit ($AN2_TSPAN+0 >= 2.30) ? 0 : 1}" || { REVERR=$((REVERR+1)); echo "JOG-FAIL: REV Tspan=$AN2_TSPAN < raw 2.344"; }
awk "BEGIN{exit ((\"$ERRR\"+0) <= 0.3) ? 0 : 1}" || { REVERR=$((REVERR+1)); echo "JOG-FAIL: REV landing err=$ERRR > 0.3"; }
awk "BEGIN{exit ($GLIDER+0 >= 2.45) ? 0 : 1}" || { REVERR=$((REVERR+1)); echo "JOG-FAIL: REV glide=$GLIDER < raw+0.1 (역방향 관성 동형 부족)"; }

echo "=== 11. MUTE leg — 재생 중 휠 세션 = 무음 스팬 (paused=true 원문+복원 원문)"
for i in 1 2; do
    PR=$(ctl '{"tool":"app_tool","args":{"app":"vplayer","tool":"play_pause","args":{}}}')
    echo "MUTE-TOGGLE-$i reply: $PR"
    sleep 1
    ST=$(status)
    printf '%s' "$ST" | grep -aq '"paused":false' && break
done
PRE_PAUSE=$(pause_of "$ST")
echo "MUTE-PRE: paused=$PRE_PAUSE pos=$(p_of "$ST")"
[ "$PRE_PAUSE" = "false" ] || { MUTEERR=$((MUTEERR+1)); echo "JOG-FAIL: MUTE pre-session not playing — paused=$PRE_PAUSE"; }
OFF3=$(wc -l < "$CLOG")
for i in $(seq 1 8); do
    wheel 1 || { WHEEL_FAILS=$((WHEEL_FAILS+1)); continue; }
    sleep 0.06   # 느린 페이스 — T1 MUTE leg 수형
done
echo "MUTE-WHEEL: 8 sends"
STMID=$(status)   # 세션/관성 스팬 중 get_status
vpt13_dump "$OFF3" "$LOGDIR/${JOGRUN}_mute_stream.txt"
MIDSAMP=$(vpt13_of "$OFF3")
sleep 2.2   # glide+스냅+복원
STAFTER=$(status)
echo "MUTE-MID: paused=$(pause_of "$STMID") pos=$(p_of "$STMID") (samples=$MIDSAMP)"
echo "MUTE-AFTER: paused=$(pause_of "$STAFTER") pos=$(p_of "$STAFTER") ended=$(printf '%s' "$STAFTER" | grep -aq '"ended":true' && echo true || echo false)"
[ "$MIDSAMP" -ge 2 ] || { MUTEERR=$((MUTEERR+1)); echo "JOG-FAIL: MUTE session samples=$MIDSAMP < 2 (세션 중 원문 부족)"; }
[ "$(pause_of "$STMID")" = "true" ] || { MUTEERR=$((MUTEERR+1)); echo "JOG-FAIL: MUTE inside session paused!=$(pause_of "$STMID") (무음 계약 원문 부족)"; }
[ "$(pause_of "$STAFTER")" = "false" ] || { MUTEERR=$((MUTEERR+1)); echo "JOG-FAIL: MUTE after snap paused=$(pause_of "$STAFTER") (복원 원문 부족)"; }
printf '%s' "$STMID" | grep -aq '"failed"' && { MUTEERR=$((MUTEERR+1)); echo "JOG-FAIL: MUTE mid status error — $STMID"; }

echo "=== 12. 반응성 (ping — 리플레이 여유 원문)"
T0=$(date +%s%N)
P=$(ctl '{"tool":"ping","args":{}}')
T1_=$(date +%s%N)
PINGMS=$(( (T1_ - T0) / 1000000 ))
echo "PING-MS: $PINGMS"
ok "$P" || { MUTEERR=$((MUTEERR+1)); echo "JOG-FAIL: ping ok 미성립 — $P"; }
[ "$PINGMS" -lt 500 ] || { MUTEERR=$((MUTEERR+1)); echo "JOG-FAIL: responsive — ping=${PINGMS}ms >= 500"; }

echo "=== 13. teardown (클라+서버 회수 — [vpt13] 스트림 원문 단열 cp)"
mkdir -p "$RECEIVE"
cp "$LOGDIR/${JOGRUN}_fwd_stream.txt" "$LOGDIR/${JOGRUN}_rev_stream.txt" "$LOGDIR/${JOGRUN}_mute_stream.txt" "$RECEIVE/" 2>/dev/null
cp "$CLOG" "$RECEIVE/${JOGRUN}_client_stderr_full.txt" 2>/dev/null
cp "$LOGDIR/jog_st_$JOGRUN.log" "$RECEIVE/${JOGRUN}_selftest.log" 2>/dev/null
echo "RECEIVED: $(ls "$RECEIVE" | tr '\n' ' ')"
rm -f buildwsl/permissions.json
[ -f buildwsl/permissions.json ] && FAIL "permissions.json 소각 실패"
echo "PERM-BURIED: permissions.json removed"
pkill -f 'buildwsl/[j]kdesktop' 2>/dev/null
sleep 1
pkill -9 -f 'buildwsl/[j]kdesktop' 2>/dev/null
sleep 1
LEFT=$(ps -eo args | grep -ic 'buildwsl/[j]kdesktop' || true)
[ "$LEFT" -eq 0 ] || echo "WARN: $LEFT jkdesktop survived teardown (원장)"

echo "=== verdict"
echo "JOG-RUN-VALS: run=$JOGRUN fwd_seed=$SEED fwd_Tspan=$AN_TSPAN fwd_maxdD=$AN_MAXDD fwd_T_end=$LATEST_T fwd_landed=$LANDED fwd_err=$ERRF fwd_glide=$GLIDEF fwd_samples=$AN_N rev_seed=$SEED2 rev_T_end=$T_END2 rev_landed=$LANDED2 rev_err=$ERRR rev_glide=$GLIDER rev_samples=$AN2_N mute_samples=$MIDSAMP mute_mid=$(pause_of "$STMID") mute_after=$(pause_of "$STAFTER") ping_ms=$PINGMS"
TOTERR=$((FWDERR+REVERR+MUTEERR))
if [ "$TOTERR" -eq 0 ]; then
    echo "JOGRUN-VERDICT: JOG-OK(T=$JOGRUN) — 휠 연속 워크+관성 감쇠 FWD/REV 동형+무음 축 원문 성립 — 캐논 WSL $ST_PASS 등호 원문"
    echo "RESULT: ALL PASS"
else
    echo "JOGRUN-VERDICT: JOG-FAIL(T=$JOGRUN) — 흐름 수치 결함 $TOTERR건 — 리포트 원장(honest-fail — rc=0)"
    echo "RESULT: $TOTERR FAILURE(S)"
fi
echo "JOG-FLOW-END run=$JOGRUN $(date +%T)"
# honest-fail 원칙: JOG-FAIL(흐름 수치)은 원장 목적이라 rc=0. hard FAIL(빌드·
# selftest·부팅·창·클립·기하·시드·소각)만 exit 1.
exit 0