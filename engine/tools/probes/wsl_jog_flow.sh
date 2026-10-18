#!/usr/bin/env bash
# T2 WSL vplayer 조그 휠 흐름 실측 probe (#100 — plan
# docs/superpowers/plans/2026-10-12-vplayer-jog-flow.md task-2; wsl_music.sh
# 위생 수형 + wsl_vp_cycle.sh 접착제 직행 수형 + wsl_ttc_check.sh 직행 클라
# 수형 + T1 Windows smoke(engine/tmp/t1_wheel_smoke.ps1 untracked)의 WSL 동형).
#
# r2(T2 fix — 사용자 실측 기각 "아직 프레임 단위가 아닌 시간 단위"): 틱 척도
# **비선형**(틱 간격 기준) — 느린/단발 틱(간격 ≥120ms 或 세션 첫 틱) = 틱당
# 정확히 2프레임(2/fps s — 프레임 수준 세밀도), 빠른 연속 틱 = 간격비 제곱
# 가속(최대 0.25s/틱 상한 — 스캔 속도 유지)·sPerRev 균일 척도는 휠 leg에서
# 소멸. 관성 glide는 **빠름으로 끝난 제스처만**(마지막 틱 간격 <120ms) — 느린
# 조정 틱 뒤 coast는 프레임 정밀을 훼손하므로 금지(400ms legacy release 원존).
# 이 probe는 r2 legs를 추가한다: SLOW leg(2틱 이격 200ms — 프레임 수준 미세
# 이동 원문) + FAST 가속 원문(FWD leg 내 avg/tick ≥1.3×슬로우 양자)+ post-burst
# 무입력 glide 원문(FWD/REV 회귀 0 계약).
#
# r3(#101 T2 — 백워드 조그 프리필, plan docs/superpowers/plans/
# 2026-10-12-vplayer-backward-prefill.md task-2, BASE 18429c3): **BWD SLOW
# leg 신설** — ①링/프리필 재료(짧은 재생 세그먼트) ②느린 뒤로 틱 시퀀스
# (2틱·이격 200ms — SLOW 수형·coast 금지) → 착지 err 원문(FWD SLOW 동형
# 게이트 0.06 — 기대 ≤0.02 실측 원문 기록) + **프리필 히트 원문**([vptB]
# pass/span/MB 행 — 첫 뒤 틱이 10s 윈도로 패스를 즉시 견인 → [vpt13] lo가
# 시작점 밑으로 내려오는 것이 프리필 span의 진실원) ③**BWD DEEP leg** —
# 프리필 밖(링 시작 이상 뒤) 크리프 graceful 원문: 파일 시작 클램프까지
# 계속 걷기(T1 smoke leg B 유계 원문 계보) — 폴백 wedge(ringMiss 스톨
# SeekScrub → 링 재건, [vpt13] lo=-1.000 행이 재건 서명) 카운트는 원문으로,
# 합성 클립(-g 30·패스 채움 ~310s/s)에서는 프리필이 전 구간을 제공해
# wedge가 발화하지 않을 수 있다(docs/50 §12.5 #1 "게이트 부재=자극 부족"
# 직감 계보 — 0이면 hard fail 아님·WARN 원문). FWD/REV 기존 세그먼트 회귀
# 원존(순서 미변경 — BWD legs는 9b와 10 사이 삽입). ⑦캐논 등호 원문(코드
# 무변경 — Win 691/WSL 668/posix 304): **WSL "671" 라벨 정정 원장행** —
# T1 리포트가 671로 표기했지만 T1 자체 캡처 2파일(t1_selftest_wsl3.txt·
# t1_r2_selftest_wsl.txt)+본 T2 런이 전부 668 등호·PASS 세트 diff=0(아래
# PASSSET-DIFF 블록이 자가 실측) — 실질 소동 0, 계수만 리포트 오표기.
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
# 2런: JOG_RUN 라벨 두 번 실행 — 리포트가 JOG-RUN-VALS 2행으로 재현 등호
# 원장을 남긴다(#101 T2 계보 = b2/b3). **WSLENV 함정 원장**: JOG_RUN env는
# wsl.exe가 기본 전달하지 않는다 — `WSLENV=JOG_RUN/u JOG_RUN=<label>` 조합만
# 넘는다(/u = Win→WSL 방향 전용 — 누락 시 JOGRUN 항상 r1 폴백=라벨 겹침).
# 시차/세척 원문: ENTRY rm(해당 라벨 재런 잔상 소각)+브래킷 pkill+permissions
# 스테이지/END 소각+서버·클라 teardown+RECEIVED ls 원문 = 캡처 회수 단열
# (혼입 금지 — 수취 dir는 라인 고유).
set -u
cd /mnt/i/progwork/JKENGINE/engine || exit 1

FAIL() { echo "JOG-WSL-FAIL: $*"; cleanup_burst "$*"; exit 1; }
LOGDIR=/tmp/jog
mkdir -p "$LOGDIR"
RECEIVE=/mnt/i/progwork/JKENGINE/engine/tmp/t2jog101   # #101 고유 수취 단열dir
                        # — (r0 결함 원장행) 구 RECEIVE t2wheel은 #100 런 수취
                        # dir와 동명이라 본 r0 런들이 #100-era r1/r2 사본을
                        # 겹쳐썼다(mtime 2026-10-17 원문 — 진정 원문=docs/50
                        # 원문 인용+wsl_jog_run*.log 생존) — 본 커밋부터
                        # 분리(혼입 금지 계약).
CANON_WSL_SELFTEST=668   # 캐논 WSL 축 — #100(8d83027)+T1(18429c3) 캡처
                         # (engine/tmp/t1_selftest_wsl3.txt·t1_r2_selftest_
                         # wsl.txt)+본 T2 런 = **3계보 668 등호·PASS 세트 0
                         # diff** — T1 리포트의 "671" 표기 = 라벨 정정 대상
                         # 원장행(실측 계수 668 — 본 커밋 docs/90 부기). 신설
                         # 어설션 0 계약.
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

# r2 원문 상수(vpt13 계보 — 960x640 레이어): 느린 틱 = 2프레임 = 2/fps s
# (fps 클립 실측 30 — JOG-FPS)·가속 상한 = 0.25 s/틱(zero gap·코드
# kWFastTick 원문 상수 동기)·노브 중심 = 창 원점 + (895,584)(지름 64 —
# ±32px 여유). env 오버라이드 창구(정찰-재보정 수형 — wsl_music 선례).
JOG_FPS=${JOG_FPS:-30};  JOG_SLOWFRAMES=${JOG_SLOWFRAMES:-2}
JOG_FASTTICK=${JOG_FASTTICK:-0.25}
JOG_KNOB_X=${JOG_KNOB_X:-895}; JOG_KNOB_Y=${JOG_KNOB_Y:-584}
# 흐름 연속성 상한 = 감쇠 최대 계단(8틱/100ms 샘플) + 1프레임(30fps 클립)
# + 관성 진입 여유 — T1 smoke bound 수형.
BOUND=$(awk "BEGIN{printf \"%.3f\", 8.0*0.1 + 1.0/30.0 + 0.3}")   # 1.133
SLOWQ=$(awk "BEGIN{printf \"%.6f\", $JOG_SLOWFRAMES/$JOG_FPS}")   # 0.066667 (2 frames)

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
      "$LOGDIR"/${JOGRUN}_mute_stream.txt "$LOGDIR"/${JOGRUN}_bwd_stream.txt \
      "$LOGDIR"/${JOGRUN}_bwd_vptB.txt "$LOGDIR"/${JOGRUN}_bwdclamp_stream.txt \
      "$LOGDIR"/${JOGRUN}_bwdclamp_vptB.txt
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
# 라벨 소동(수식 아님) 자가 실측(#101 T2): 참조 캡처가 있으면 PASS **세트**
# 등호를 그자리에서 실측한다(T1 캡처+본 런 = 3계보 668 등호 — set이 같으면
# "계수만 소동/오표기" 원문으로 등호 확정, 다르면 진소동 원장 행).
canon_set=tmp/t1_r2_selftest_wsl.txt   # cwd=engine — 엔진 소유 참조 캡처(untracked)
if [ -f "$canon_set" ]; then
    if diff <(grep -a '^\[PASS\]' "$LOGDIR/jog_st_$JOGRUN.log") \
            <(grep -a '^\[PASS\]' "$canon_set") >/dev/null 2>&1; then
        echo "PASSSET-DIFF: 0 (vs T1 참조 캡처 — CANON-WSL-SET 등호 원문 — 라벨 소동만)"
    else
        echo "PASSSET-DIFF: !=0 (vs T1 참조 캡처 — **진소동** — 원장 필수)"
    fi
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
FWDERR=0; REVERR=0; MUTEERR=0; SLOWERR=0; BWDERR=0; DSWERR=0; SEEDERR=0
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
[ "$AWK_DUR" -eq 0 ] || FAIL "clip duration $DUR != 30 — r2 틱 상수(JOG_FPS=30 프레임 양자) 계보 불성립"

echo "=== 8. r2 원료 상수 원문"
echo "JOG-TICKS: slowTick=$SLOWQ ($JOG_SLOWFRAMES frames @ ${JOG_FPS}fps) fastCeil=$JOG_FASTTICK (빠른 가속 상한 — r2 비선형 척도)"
# 클립 fps 30 → T1 bound 1.133 수형 그대로.
FASTMIN=$(awk "BEGIN{printf \"%.3f\", 10*1.3*$SLOWQ}")   # 0.867 — 가속 판정 하한(avg/tick ≥ 1.3× 슬로우 양자)

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
OFF_END=$(wc -l < "$CLOG")   # burst 종료 시점 스냅샷 — 이후 행 = 무입력 glide 원문(r2)
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
awk "BEGIN{exit ($AN_TSPAN+0 >= 0.90) ? 0 : 1}" || { FWDERR=$((FWDERR+1)); echo "JOG-FAIL: FWD Tspan=$AN_TSPAN < 0.90 (r2 가속 스캔 원문 부족)"; }
awk "BEGIN{exit (($AN_TSPAN+0)/10.0 >= 1.3*$SLOWQ) ? 0 : 1}" || { FWDERR=$((FWDERR+1)); echo "JOG-FAIL: FWD accel avg=$(awk "BEGIN{printf \"%.3f\", $AN_TSPAN/10.0}") < 1.3x slow quantum (가속 원문 부족 — r2)"; }
awk "BEGIN{exit ((\"$ERRF\"+0) <= 0.3) ? 0 : 1}" || { FWDERR=$((FWDERR+1)); echo "JOG-FAIL: FWD landing err=$ERRF > 0.3 (스냅 단일 출구 원문 부족)"; }
# r2 관성 원문 = burst 종료 "이후" 행의 무입력 T 상승(0틱 스팬 ≥0.15 s·상승 ≥2)
tail -n +"$((OFF_END+1))" "$CLOG" | grep -a '^\[vpt13\]' > "$LOGDIR/${JOGRUN}_fwd_glide.txt" || true
ANF_G=$(vpt13_analyze "$LOGDIR/${JOGRUN}_fwd_glide.txt" 1)
read -r GF_N GF_MAXDD GF_TSPAN GF_BADWAY <<EOF
$ANF_G
EOF
GRISE=$(awk '{if (match($0,/T=(-?[0-9.]+)/)) { t=substr($0,RSTART+2,RLENGTH-2); if (p!="" && t-p>=0.03) r++; p=t } } END{print r+0}' "$LOGDIR/${JOGRUN}_fwd_glide.txt")
echo "FWD-GLIDE-VALS: samples=$GF_N span=$GF_TSPAN rises=$GRISE"
awk "BEGIN{exit (($GF_TSPAN+0 >= 0.15) && ($GRISE+0 >= 2) && ($GF_N+0 >= 3)) ? 0 : 1}" || { FWDERR=$((FWDERR+1)); echo "JOG-FAIL: FWD zero-input glide=$GF_TSPAN rises=$GRISE (관성 연장 원문 부족 — r2)"; }
[ "$PAUSED_SNAP" = "true" ] || { FWDERR=$((FWDERR+1)); echo "JOG-FAIL: FWD parked after snap — paused=$PAUSED_SNAP"; }
P1=$(p_of "$(status)"); sleep 0.5; P2=$(p_of "$(status)")
DP=$(awk "BEGIN{d=$P2-$P1; if(d<0)d=-d; printf \"%.3f\", d}")
echo "FWD-STATIC: $P1 -> $P2 (d=$DP)"
awk "BEGIN{exit ($DP <= 0.02) ? 0 : 1}" || { FWDERR=$((FWDERR+1)); echo "JOG-FAIL: FWD pos static — d=$DP > 0.02"; }

echo "=== 9b. SLOW leg — 2틱 이격 200ms = 프레임 수준 미세 이동 (r2 원문 계약)"
ctl '{"tool":"app_tool","args":{"app":"vplayer","tool":"seek","args":{"seconds":6}}}' >/dev/null
sleep 1
SEEDS=$(p_of "$(status)")
echo "SLOW-SEED: $SEEDS"
awk "BEGIN{e=$SEEDS-6.0; if(e<0)e=-e; exit (e<=0.5) ? 0 : 1}"     || { SLOWERR=$((SLOWERR+1)); echo "JOG-FAIL: SLOW seed=$SEEDS != 6±0.5"; }
knobclick || FAIL "knob click failed (SLOW leg)"
sleep 1
OFFS=$(wc -l < "$CLOG")
for i in 1 2; do
    wheel 1 || { WHEEL_FAILS=$((WHEEL_FAILS+1)); continue; }
    sleep 0.2   # 이격 200ms(+호출 오버헤드 ~240ms) — 전부 느린 게이트(≥120ms)
done
echo "SLOW-WHEEL: 2 sends, sends-failed=$WHEEL_FAILS"
sleep 0.9   # 400ms legacy release — coast 없음(r2 계약)
vpt13_dump "$OFFS" "$LOGDIR/${JOGRUN}_slow_stream.txt"
ST=$(status)
LANDEDS=$(p_of "$ST")
echo "SLOW-SNAP: landed=$LANDEDS"
ANS=$(vpt13_analyze "$LOGDIR/${JOGRUN}_slow_stream.txt" 1)
read -r ANS_N ANS_MAXDD ANS_TSPAN ANS_BADWAY <<EOF
$ANS
EOF
RAWS=$(awk "BEGIN{printf \"%.3f\", 2*$SLOWQ}")
DELTS=$(awk "BEGIN{d=$LANDEDS-$SEEDS; printf \"%.3f\", d}")
MAXS=$(awk "BEGIN{printf \"%.3f\", $SLOWQ+0.04}")
echo "SLOW-VALS: seed=$SEEDS landed=$LANDEDS delta=$DELTS raw=$RAWS maxstep=$ANS_MAXDD (bound $MAXS)"
[ "${ANS_N:-0}" -ge 2 ] || { SLOWERR=$((SLOWERR+1)); echo "JOG-FAIL: SLOW samples=$ANS_N < 2"; }
awk "BEGIN{exit ($ANS_MAXDD+0 <= $MAXS) ? 0 : 1}" || { SLOWERR=$((SLOWERR+1)); echo "JOG-FAIL: SLOW step=$ANS_MAXDD > 2-frame quantum+slack (가속이 느린 영역에 유출 — r2 위반)"; }
awk "BEGIN{exit ($DELTS+0 >= 0.10 && $DELTS+0 <= 0.22) ? 0 : 1}" || { SLOWERR=$((SLOWERR+1)); echo "JOG-FAIL: SLOW delta=$DELTS ∉ raw 2틱=0.133± (프레임 수준 미세 이동 원문 부족 — coast 유출 의심)"; }
ERRS=$(awk "BEGIN{e=$SEEDS+2*$SLOWQ-$LANDEDS; if(e<0)e=-e; printf \"%.3f\", e}")
awk "BEGIN{exit ((\"$ERRS\"+0) <= 0.06) ? 0 : 1}" || { SLOWERR=$((SLOWERR+1)); echo "JOG-FAIL: SLOW landing err=$ERRS > 0.06 (틱당 정확 2프레임 착지 실패)"; }
echo "SLOW-ERR: |landed - (seed+2q)| = $ERRS"

echo "=== 9c. BWD SLOW leg — 역방향 프리필 히트 (#101 T2 — FWD SLOW 동형 판정)"
# 재료 = 짧은 재생 세그먼트: seek 20 → 재생 2s(전진 디코드가 링 [20..~22]를 채움
# — ②링/프리필 재료 계약) → park. 링 span < 10s = 프리필 윈도
# (kPrefillAheadSecs) 안이라 **첫 뒤 틱이 패스를 즉시 견인**(트리거 원문
# — 역방향 세션만 알아서 사전 확장). 시작 상태 정규화: 9b 종료 = paused·
# 부정이면 우선 park(3회 재시도원문).
ST=$(status)
if [ "$(pause_of "$ST")" != "true" ]; then
    for i in 1 2 3; do
        PR=$(ctl '{"tool":"app_tool","args":{"app":"vplayer","tool":"play_pause","args":{}}}')
        sleep 1
        ST=$(status)
        [ "$(pause_of "$ST")" = "true" ] && break
    done
fi
[ "$(pause_of "$ST")" = "true" ] || FAIL "BWD pre-park failed — paused=$(pause_of "$ST")"
UNPAUSED=0
for i in 1 2 3; do
    PR=$(ctl '{"tool":"app_tool","args":{"app":"vplayer","tool":"play_pause","args":{}}}')
    echo "BWD-UNPAUSE-TRY-$i reply: $PR"
    sleep 1
    ST=$(status)
    echo "BWD-UNPAUSE-TRY-$i: paused=$(pause_of "$ST") pos=$(p_of "$ST")"
    [ "$(pause_of "$ST")" = "false" ] && { UNPAUSED=1; break; }
done
[ "$UNPAUSED" -eq 1 ] || FAIL "BWD unpause failed (3 toggles — paused=$(pause_of "$ST") pos=$(p_of "$ST"))"
P0B=$(p_of "$ST")
ctl '{"tool":"app_tool","args":{"app":"vplayer","tool":"seek","args":{"seconds":20}}}' >/dev/null
sleep 1
P0B=$(p_of "$(status)")   # seek 20 착지 재판독 — 재생 세그먼트 시작점
sleep 2                   # 재생 2s — 링 재료 [~20..~22] (②계약 원문)
PARKB=0
for i in 1 2 3; do
    PR=$(ctl '{"tool":"app_tool","args":{"app":"vplayer","tool":"play_pause","args":{}}}')
    echo "BWD-PARK-TRY-$i reply: $PR"
    sleep 1
    ST=$(status)
    echo "BWD-PARK-TRY-$i: paused=$(pause_of "$ST") pos=$(p_of "$ST")"
    [ "$(pause_of "$ST")" = "true" ] && { PARKB=1; break; }
done
[ "$PARKB" -eq 1 ] || FAIL "BWD post-play park failed (3 toggles — paused=$(pause_of "$ST") pos=$(p_of "$ST"))"
ST=$(status)
SEEDB=$(p_of "$ST")
echo "BWD-SEED: $SEEDB (재생 세그먼트 $P0B→$SEEDB — 링 재료 원문)"
awk "BEGIN{e=$SEEDB-$P0B; exit (e>=1.0 && e<=3.5) ? 0 : 1}" \
    || { BWDERR=$((BWDERR+1)); echo "JOG-FAIL: BWD 재생 세그먼트=$SEEDB-$P0B ∉ [1.0,3.5] (링 재료 원문 부족)"; }
knobclick || FAIL "knob click failed (BWD leg)"
sleep 1
OFFB=$(wc -l < "$CLOG")
BWDSENDF=0
for i in 1 2; do
    wheel -1 || { WHEEL_FAILS=$((WHEEL_FAILS+1)); BWDSENDF=$((BWDSENDF+1)); continue; }
    sleep 0.2   # 이격 200ms(+호출 오버헤드) — 전부 느린 게이트(≥120ms — coast 금지 r2)
done
echo "BWD-WHEEL: 2 sends, sends-failed=$BWDSENDF"
[ "$BWDSENDF" -eq 0 ] || FAIL "BWD wheel send gate failed ($BWDSENDF/2) — permissions.json send_input allow 확인"
sleep 1.1   # 프리필 패스(즉시 견인)+400ms legacy release(coast 없음)+스냅 정산
            # — FWD SLOW 0.9 + 패스 완결 여유
vpt13_dump "$OFFB" "$LOGDIR/${JOGRUN}_bwd_stream.txt"
tail -n +"$((OFFB+1))" "$CLOG" | grep -a '^\[vptB\]' > "$LOGDIR/${JOGRUN}_bwd_vptB.txt" || true
NRB=$(wc -l < "$LOGDIR/${JOGRUN}_bwd_vptB.txt")
SPANB=$(awk '{ if (match($0,/span=([0-9.]+)/)) { s=substr($0,RSTART+5,RLENGTH-5); if (n==0||s>maxs) maxs=s; n++ } } END{print maxs+0}' "$LOGDIR/${JOGRUN}_bwd_vptB.txt")
LOMIN=$(awk '{ if (match($0,/lo=(-?[0-9.]+)/)) { l=substr($0,RSTART+3,RLENGTH-3)+0; if (n==0||l<minl) minl=l; n++ } } END{print minl+0}' "$LOGDIR/${JOGRUN}_bwd_stream.txt")
ST=$(status)
LANDEDB=$(p_of "$ST")
PAUSEDB=$(pause_of "$ST")
echo "BWD-SNAP: landed=$LANDEDB paused=$PAUSEDB"
vpt13_dump "$OFFB" "$LOGDIR/${JOGRUN}_bwd_stream.txt"   # 스냅 포함 확정판
ANB=$(vpt13_analyze "$LOGDIR/${JOGRUN}_bwd_stream.txt" -1)
read -r ANB_N ANB_MAXDD ANB_TSPAN ANB_BADWAY <<EOF
$ANB
EOF
RAWB=$(awk "BEGIN{printf \"%.3f\", 2*$SLOWQ}")
DELTB=$(awk "BEGIN{d=$SEEDB-$LANDEDB; printf \"%.3f\", d}")
MAXB=$(awk "BEGIN{printf \"%.3f\", $SLOWQ+0.04}")
ERRB=$(awk "BEGIN{e=$SEEDB-2*$SLOWQ-$LANDEDB; if(e<0)e=-e; printf \"%.3f\", e}")
echo "BWD-VALS: seed=$SEEDB landed=$LANDEDB delta=$DELTB raw=$RAWB maxstep=$ANB_MAXDD (bound $MAXB) lo_min=$LOMIN vptB=$NRB span=$SPANB"
[ "${ANB_N:-0}" -ge 2 ] || { BWDERR=$((BWDERR+1)); echo "JOG-FAIL: BWD samples=$ANB_N < 2 (세션 원문 부족)"; }
[ "${ANB_BADWAY:-1}" -eq 0 ] || { BWDERR=$((BWDERR+1)); echo "JOG-FAIL: BWD direction — D가 증가한 샘플($ANB_BADWAY) (역방향 워크 원문 부족)"; }
awk "BEGIN{exit ($ANB_MAXDD+0 <= $MAXB) ? 0 : 1}" || { BWDERR=$((BWDERR+1)); echo "JOG-FAIL: BWD step=$ANB_MAXDD > 2-frame quantum+slack (가속이 느린 영역에 유출 — r2 계약 위반)"; }
awk "BEGIN{exit ($DELTB+0 >= 0.10 && $DELTB+0 <= 0.22) ? 0 : 1}" || { BWDERR=$((BWDERR+1)); echo "JOG-FAIL: BWD delta=$DELTB ∉ raw 2틱=0.133± (역방향 프레임 수준 미세 이동 원문 부족 — coast 유출 의심)"; }
awk "BEGIN{exit (\"$ERRB\"+0 <= 0.06) ? 0 : 1}" || { BWDERR=$((BWDERR+1)); echo "JOG-FAIL: BWD landing err=$ERRB > 0.06 (틱당 정확 2프레임 착지 실패 — FWD SLOW 동형 게이트)"; }
[ "${NRB:-0}" -ge 1 ] || { BWDERR=$((BWDERR+1)); echo "JOG-FAIL: BWD [vptB] rows=0 (프리필 히트 원문 부족 — 첫 뒤 틱이 10s 윈도로 패스를 견인해야 한다)"; }
awk "BEGIN{exit (($LOMIN+0 > 0.001) && ($LOMIN+0 <= $SEEDB-5.0)) ? 0 : 1}" \
    || { BWDERR=$((BWDERR+1)); echo "JOG-FAIL: BWD lo_min=$LOMIN ≤ seed-5 아님 (프레임 front 사전 확장 원문 부족)"; }
[ "$PAUSEDB" = "true" ] || { BWDERR=$((BWDERR+1)); echo "JOG-FAIL: BWD parked after snap — paused=$PAUSEDB"; }
P1B=$(p_of "$(status)"); sleep 0.5; P2B=$(p_of "$(status)")
DPB=$(awk "BEGIN{d=$P2B-$P1B; if(d<0)d=-d; printf \"%.3f\", d}")
echo "BWD-STATIC: $P1B -> $P2B (d=$DPB)"
awk "BEGIN{exit ($DPB <= 0.02) ? 0 : 1}" || { BWDERR=$((BWDERR+1)); echo "JOG-FAIL: BWD pos static — d=$DPB > 0.02"; }

echo "=== 9d. BWD DEEP leg — 프리필 밖(링 시작 이상 뒤) 크리프 graceful + 시작 클램프"
# 링 재료 = 짧은 세그먼트(9c 수형): seek 1 → 재생 1.2s(링 [~1..~2.2]) → park →
# 빠른 뒤로 버스트(60틱 — r2 스캔 척도·하단 fix 원장) → 타깃이 파일 시작
# 클램프(0.0)로. 링
# 시작(≈1.0) 이하 = 체인/프리필이 아직 못 채운 **프리필 밖 크리프 존** — 패스가
# t<0 을 0으로 클램프해(docs/50 원문 "the front is already near the file
# start") D가 0.000까지 graceful로 내려간다(T1 smoke leg B 유계 원문 계보).
# 폴백 wedge(ringMiss 스톨 SeekScrub → 링 재건) 서명 = [vpt13] lo=-1.000 행
# — 0이면 hard fail 아님(프리필이 전 구간을 제공해 폴백 불요 — docs/50
# §12.5 #1 계보 WARN 원문).
ctl '{"tool":"app_tool","args":{"app":"vplayer","tool":"seek","args":{"seconds":1}}}' >/dev/null
sleep 1
UNPAUSED=0
for i in 1 2 3; do
    PR=$(ctl '{"tool":"app_tool","args":{"app":"vplayer","tool":"play_pause","args":{}}}')
    echo "CLAMP-UNPAUSE-TRY-$i reply: $PR"
    sleep 1
    ST=$(status)
    echo "CLAMP-UNPAUSE-TRY-$i: paused=$(pause_of "$ST") pos=$(p_of "$ST")"
    [ "$(pause_of "$ST")" = "false" ] && { UNPAUSED=1; break; }
done
[ "$UNPAUSED" -eq 1 ] || FAIL "clamp unpause failed (3 toggles — paused=$(pause_of "$ST") pos=$(p_of "$ST"))"
P0C=$(p_of "$ST")
sleep 1.2                  # 재생 1.2s — 링 재료 [~1..~2.2]
PARKC=0
for i in 1 2 3; do
    PR=$(ctl '{"tool":"app_tool","args":{"app":"vplayer","tool":"play_pause","args":{}}}')
    echo "CLAMP-PARK-TRY-$i reply: $PR"
    sleep 1
    ST=$(status)
    echo "CLAMP-PARK-TRY-$i: paused=$(pause_of "$ST") pos=$(p_of "$ST")"
    [ "$(pause_of "$ST")" = "true" ] && { PARKC=1; break; }
done
[ "$PARKC" -eq 1 ] || FAIL "clamp post-play park failed (3 toggles — paused=$(pause_of "$ST") pos=$(p_of "$ST"))"
SEEDD=$(p_of "$(status)")
echo "CLAMP-SEED: $SEEDD (링 시작 ≈1.0 — 걸어 내려갈 크리프 존 원문)"
knobclick || FAIL "knob click failed (clamp leg)"
sleep 1
OFFD=$(wc -l < "$CLOG")
# fix-r0(r0-1 런 원문 engine/tmp/t2bwd_run1.log)+fix-r1(r0-2 런 원문
# engine/tmp/t2bwd_run2.log): 16틱/30틱으로는 못 건넌다 — ctl 호출 오버헤드
# (~85ms)가 실격차를 만들어 r2 비선형 척도상 틱 ~0.08-0.09s(간격비
# f²≈0.04-0.09) — 원문: 16틱 착지 1.500·30틱 착지 0.600(링 시작 1.0 미달)
# → **60틱**(순수 느린 틱 하한 0.0667×60 = 4.0s ≥ 시드 3.3 — 어떤 ctl 지연
# 에도 클램프 도달 보장 — 느린 틱은 상계 밑창이 2프레임이라 작아지지 않는다).
for i in $(seq 1 60); do
    wheel -1 || { WHEEL_FAILS=$((WHEEL_FAILS+1)); continue; }
    sleep 0.025   # 빠른 페이스 — 스캔 척수(r2·REV leg 수형)
done
echo "CLAMP-WHEEL: 60 sends (fast)"
sleep 2.6   # 체인 패스+클램프 체이스(8프레임/UI프레임)+스냅 정산
vpt13_dump "$OFFD" "$LOGDIR/${JOGRUN}_bwdclamp_stream.txt"
tail -n +"$((OFFD+1))" "$CLOG" | grep -a '^\[vptB\]' > "$LOGDIR/${JOGRUN}_bwdclamp_vptB.txt" || true
NRD=$(wc -l < "$LOGDIR/${JOGRUN}_bwdclamp_vptB.txt")
ST=$(status)
LANDEDD=$(p_of "$ST")
PAUSEDC=$(pause_of "$ST")
echo "CLAMP-SNAP: landed=$LANDEDD paused=$PAUSEDC"
vpt13_dump "$OFFD" "$LOGDIR/${JOGRUN}_bwdclamp_stream.txt"
AND=$(vpt13_analyze "$LOGDIR/${JOGRUN}_bwdclamp_stream.txt" -1)
read -r AND_N AND_MAXDD AND_TSPAN AND_BADWAY <<EOF
$AND
EOF
SIG0=$(awk '{ if (match($0,/lo=(-?[0-9.]+)/)) { l=substr($0,RSTART+3,RLENGTH-3)+0; if (match($0,/D=(-?[0-9.]+)/)) { d=substr($0,RSTART+2,RLENGTH-2)+0; if (l==0.0 && d>0.5) n++ } } } END{print n+0}' "$LOGDIR/${JOGRUN}_bwdclamp_stream.txt")
SIGN1=$(grep -ac 'lo=-1.000' "$LOGDIR/${JOGRUN}_bwdclamp_stream.txt")
# 표시 방향 정책 서명(#103 fix 1 — 원존 계보): 폴백/프리필 크리프 구간의
# 그림 hold 행들([vptHOLD] — 마지막 표시 유지·앞 크리프 소거). 합성 클립
# (480p·패스 ms급)은 hold 자극이 부족할 수 있다 — 0이면 hard fail 아님
# (WARN 원문 — wedge WARN과 동일 계보·4K/롱GOP 런이 본계).
HOLDN=$(tail -n +"$((OFFD+1))" "$CLOG" | grep -ac '\[vptHOLD\]' || true)
tail -n +"$((OFFD+1))" "$CLOG" | grep -a '\[vptHOLD\]' > "$LOGDIR/${JOGRUN}_hold_rows.txt" || true
HOLDGAP=$(awk '{ if (match($0,/gap=([0-9.]+)/)) { g=substr($0,RSTART+4,RLENGTH-4)+0; if (n==0||g>maxg) maxg=g; n++ } } END{print maxg+0}' "$LOGDIR/${JOGRUN}_hold_rows.txt")
DMINB=$(awk '{ if (match($0,/D=(-?[0-9.]+)/)) { d=substr($0,RSTART+2,RLENGTH-2)+0; if (n==0||d<dmin) dmin=d; n++ } } END{print dmin+0}' "$LOGDIR/${JOGRUN}_bwdclamp_stream.txt")
ERRD=$(awk "BEGIN{a=$LANDEDD; if(a<0)a=-a; printf \"%.3f\", a}")
echo "CLAMP-VALS: seed=$SEEDD landed=$LANDEDD samples=$AND_N maxjump=$AND_MAXDD tspan=$AND_TSPAN lo0rows=$SIG0 lom1=$SIGN1 dmin=$DMINB vptB=$NRD hold_rows=$HOLDN hold_maxgap=$HOLDGAP"
[ "${AND_N:-0}" -ge 4 ] || { BWDERR=$((BWDERR+1)); echo "JOG-FAIL: CLAMP samples=$AND_N < 4 (크리프 세션 원문 부족)"; }
[ "${AND_BADWAY:-1}" -eq 0 ] || { BWDERR=$((BWDERR+1)); echo "JOG-FAIL: CLAMP direction — D가 증가한 샘플($AND_BADWAY) (graceful 크리프 위반)"; }
awk "BEGIN{exit ($AND_MAXDD+0 <= $BOUND) ? 0 : 1}" || { BWDERR=$((BWDERR+1)); echo "JOG-FAIL: CLAMP continuity max|dD|=$AND_MAXDD > bound $BOUND (크리프 중 급점프 — 유계 위반)"; }
awk "BEGIN{exit ($AND_TSPAN+0 >= $SEEDD-0.3) ? 0 : 1}" || { BWDERR=$((BWDERR+1)); echo "JOG-FAIL: CLAMP target walk=$AND_TSPAN < seed-0.3 ($SEEDD) — 클램프까지 못 걸음"; }
awk "BEGIN{exit (\"$LANDEDD\"+0 <= 0.05) ? 0 : 1}" || { BWDERR=$((BWDERR+1)); echo "JOG-FAIL: CLAMP landed=$LANDEDD > 0.05 (시작 클램프 graceful 종착 실패)"; }
awk "BEGIN{exit ($DMINB+0 >= -0.05) ? 0 : 1}" || { BWDERR=$((BWDERR+1)); echo "JOG-FAIL: CLAMP D_min=$DMINB < -0.05 (클램프 밖 역행 — 유계 위반)"; }
[ "$SIGN1" -ge 1 ] && echo "WEDGE-CONFIRMED: lo=-1.000 재건 행 $SIGN1개 (폴백 wedge 서명 — 1행 원문)" \
    || echo "WARN: wedge 서명 0 — 프리필/체인 패스가 전 구간을 제공해 폴백 불요였다(자극 부족 — docs/50 §12.5 #1 직감 계보·원장행; hard fail 아님)"
echo "CLAMP-HOLD: rows=$HOLDN maxgap=$HOLDGAP"
[ "${HOLDN:-0}" -ge 1 ] && echo "HOLD-CONFIRMED: 그림 hold 행 $HOLDN개 (maxgap=$HOLDGAP — #103 표시 정책 원문)" \
    || echo "WARN: hold 행 0 — 크리프 자극 부족(합성 클립 패스 ms급 — 표시 크리프가 hold 없이 관측 안 될 수 있음; hard fail 아님)"
[ "$PAUSEDC" = "true" ] || { BWDERR=$((BWDERR+1)); echo "JOG-FAIL: CLAMP parked after snap — paused=$PAUSEDC"; }
P1C=$(p_of "$(status)"); sleep 0.5; P2C=$(p_of "$(status)")
DPC=$(awk "BEGIN{d=$P2C-$P1C; if(d<0)d=-d; printf \"%.3f\", d}")
echo "CLAMP-STATIC: $P1C -> $P2C (d=$DPC)"
awk "BEGIN{exit ($DPC <= 0.02) ? 0 : 1}" || { BWDERR=$((BWDERR+1)); echo "JOG-FAIL: CLAMP pos static — d=$DPC > 0.02"; }

echo "=== 9e. DIRSWITCH leg — 전환 틱 즉시 양자+관성 소멸 (#102 fix 1, spike b1/b2/c 계보)"
# spike(jog-dirswitch-spike-report, HEAD 093170f)의 (c) 후보 = 등급 A 진원:
# 방향 전환 틱이 구 방향의 휠 흐름 상태(EMA 창·wheelVel_·관성)에 잠식 —
# b1: 전환 틱 뒤 glide 재올림으로 net +0.043(그림 변화 0·+941ms)·b2: 300ms
# gap 내 +0.124 표류·c1/c2: 첫 전환 틱 반양자. 수형(#102 fix 1) = 전환 틱의
# 도착 순간 구 방향 휠 흐름 전량 폐기(WheelFlowReset 확장 — 접붙임 사이트 =
# 방향 이벤트 다음) → 전환 틱 = 새 방향의 첫 양자 그대로 착탈.
# 본 leg: 3 fast FWD 틱 → 1 전환(−)틱 → 무입력 정지 — 판정: ①전환 틱의
# T 하강 = 깨끗한 양자(≤3프레임) ②전환 뒤 T 상승 행 0(구 방향 glide 재올림
# 소멸 = b1 서명 폐곡) ③스냅 착지·parked·정지.
ctl '{"tool":"app_tool","args":{"app":"vplayer","tool":"seek","args":{"seconds":15}}}' >/dev/null
sleep 1
SEEDW=$(p_of "$(status)")
echo "DIRSW-SEED: $SEEDW"
knobclick || FAIL "knob click failed (dirswitch leg)"
sleep 1
OFFW=$(wc -l < "$CLOG")
DSFWD=0; DSREV=0
for i in 1 2 3; do
    wheel 1 || { DSFWD=$((DSFWD+1)); continue; }
    sleep 0.025   # fast run — 구 방향 세션 살아있음(b1의 전제)
done
sleep 0.11   # 전환 틱 — b1의 gap(~110ms) 수형(스위치 = 빠른 run 끝에서 역방향 1틱)
wheel -1 || { DSREV=$((DSREV+1)); }
echo "DIRSW-WHEEL: 3 fwd + 1 rev, fwd-failed=$DSFWD rev-failed=$DSREV"
[ "$DSFWD" -eq 0 ] && [ "$DSREV" -eq 0 ] || FAIL "DIRSW wheel send gate failed — permissions.json send_input allow 확인"
# 전환 후 무입력 — r2 release 게이트(전환 틱 = deliberate → 400ms quiet release,
# glide 무기 — WheelFlowReset이 wheelLastTickFast_/vel 소거) → 스냅 정산.
sleep 2.2
vpt13_dump "$OFFW" "$LOGDIR/${JOGRUN}_dirsw_stream.txt"
ST=$(status)
LANDEDW=$(p_of "$ST")
PAUSEDW=$(pause_of "$ST")
echo "DIRSW-SNAP: landed=$LANDEDW paused=$PAUSEDW"
# 전환 틱 행의 자동 판독: T의 **첫 감소 행** = 전환 틱의 착탈; 그 앞 마지막 상승 행
# 과의 델타 = 전환 양자; 감소 행 이후의 T 상승(>0.02) = 구 방향 glide 잔유(위반).
ANW=$(vpt13_analyze "$LOGDIR/${JOGRUN}_dirsw_stream.txt" 1)
read -r ANW_N ANW_MAXDD ANW_TSPAN ANW_BADWAY <<EOF
$ANW
EOF
DSDELTA=$(awk '
    { if (match($0,/T=(-?[0-9.]+)/)) { t=substr($0,RSTART+2,RLENGTH-2)+0
        if (seen && t < p - 0.01 && !firstdrop) { firstdrop=1; dd=p - t }
        if (seen && firstdrop && t > p + 0.02 && !risen) { risen=1; rrow=NR }
        p=t; seen=1 } }
    END { printf "%.3f %d %d\n", dd+0, (risen+0), rrow+0 }' "$LOGDIR/${JOGRUN}_dirsw_stream.txt")
read -r DS_DD DS_RISE DS_RROW <<EOF
$DSDELTA
EOF
QLIM=$(awk "BEGIN{printf \"%.3f\", 3.2*$SLOWQ}")   # ≤3.2프레임(전환 틱 상한 — 슬랙 포함)
echo "DIRSW-VALS: seed=$SEEDW samples=$ANW_N switch_quantum=$DS_DD (bound qlim=$QLIM) post_rise_rows=$DS_RISE landing=$LANDEDW"
[ "${ANW_N:-0}" -ge 4 ] || { DSWERR=$((DSWERR+1)); echo "JOG-FAIL: DIRSW samples=$ANW_N < 4"; }
[ "$DS_DD" != "" ] && [ "$(awk "BEGIN{print ($DS_DD+0 > 0.005 && $DS_DD+0 <= $QLIM) ? 1 : 0}")" = "1" ] \
    || { DSWERR=$((DSWERR+1)); echo "JOG-FAIL: DIRSW switch quantum=$DS_DD ∈ (0, $QLIM] 대결 실패 (전환 틱 응답 원문 부족)"; }
[ "${DS_RISE:-99}" -eq 0 ] \
    || { DSWERR=$((DSWERR+1)); echo "JOG-FAIL: DIRSW post-switch T rise row $DS_RROW (구 방향 관성 잔유 — b1 서명 재발)"; }
DS_TLAST=$(sed -n 's/.*T=\(-\?[0-9.]*\).*/\1/p' "$LOGDIR/${JOGRUN}_dirsw_stream.txt" | tail -1)
ERRW=$(awk "BEGIN{e=$LANDEDW-$DS_TLAST; if(e<0)e=-e; printf \"%.3f\", e}")
ERRW=$(awk "BEGIN{e=$LANDEDW-$DS_TLAST; if(e<0)e=-e; printf \"%.3f\", e}")
echo "DIRSW-ERR: |landed $LANDEDW - T_end $DS_TLAST| = $ERRW"
[ "$(awk "BEGIN{print ((\"$ERRW\"+0) <= 0.3) ? 1 : 0}")" = "1" ] \
    || { DSWERR=$((DSWERR+1)); echo "JOG-FAIL: DIRSW landing err=$ERRW > 0.3"; }
[ "$PAUSEDW" = "true" ] || { DSWERR=$((DSWERR+1)); echo "JOG-FAIL: DIRSW parked after snap — paused=$PAUSEDW"; }

echo "=== 9f. SEED leg — 세션 시드 = 표시 프레임 (진실원) (#102 fix 2, spike a1/a2)"
# spike 후보 (D) 등급 A: 진입 시드 st.pos = 오디오 클럭 = 그림보다 6-8프레임
# 앞(재생 중) → 역방향 진입 첫 틱의 그림이 전진 +8프레임 출렁(a2 원문).
# 수형 = 시드 = lastShownPts_(실제 올라간 마지막 표시 프레임 pts)·클럭 폴백.
# 진실원 증거 행 [vptSEED](pos/shown/seed)의 판정:
#  - 재생 중 진입: shown < pos − 0.04 (클럭이 표시를 리드 — 시드가 그림에 뿌리
#    내림 = **수형 서명·수형 전에는 pos−seed ≈ 0**)
#  - 일시정지 진입: |pos − seed| ≤ 0.1 (핀 착지 표시의 양자화 — 동일 기저)
# 오디오 클럭의 다른 표기(get_status pos·시간줄) = 원존(변경 없음 — 원장행).
ctl '{"tool":"app_tool","args":{"app":"vplayer","tool":"seek","args":{"seconds":24}}}' >/dev/null
sleep 1
UNPAUSEA=0
for i in 1 2 3; do
    PR=$(ctl '{"tool":"app_tool","args":{"app":"vplayer","tool":"play_pause","args":{}}}')
    sleep 1
    ST=$(status)
    [ "$(pause_of "$ST")" = "false" ] && { UNPAUSEA=1; break; }
done
[ "$UNPAUSEA" -eq 1 ] || FAIL "SEED leg unpause failed (paused=$(pause_of "$ST"))"
sleep 0.5                   # 재생 — 그림/클럭 리드(gap) 확보
SEEDPLAY=$(p_of "$(status)")
knobclick || FAIL "knob click failed (seed leg, playing entry)"
sleep 1.1                   # degenerate 드래그 release(무해 스냅) — 시드 시크 정산
# release는 jogWasPlaying_ 복원계약으로 **재생 재개** — 그 뒤 get_status의 pos =
# 재개된 오디오 클럭이 아니라 클럭이 흐르는 값(착지 판독 불가) → 즉시 park 토글로
# 클럭을 핀에 두고 착지를 판독한다(스냅 착지만큼만).
ST=$(status)
resumed=$(pause_of "$ST")
SLT0=$(date +%s%N)
ctl '{"tool":"app_tool","args":{"app":"vplayer","tool":"play_pause","args":{}}}' >/dev/null
SLT0=$(date +%s%N)
sleep 1
ST=$(status)
SLT1=$(date +%s%N)
SEEDLANDED=$(p_of "$ST")
RELMS=$(( (SLT1 - SLT0 - 0) / 1000000 ))
echo "SEED-PLAY-RESUME: paused_after_release=$resumed (release 후 재개) → park pinned landed=$SEEDLANDED (park 시점 ≈ release+${RELMS}ms)"
LASTSEEDP=$(tail -n +1 "$CLOG" | grep -a 'entry=drag' | tail -1)
echo "SEED-PLAY-ROW: $LASTSEEDP (재생 중 진입 — 진실원 서명)"
SPPOSS=$(printf '%s' "$LASTSEEDP" | sed -n 's/.*pos=\([0-9.]*\).*/\1/p')
SPSHOWN=$(printf '%s' "$LASTSEEDP" | sed -n 's/.*shown=\([0-9.]*\).*/\1/p')
SPSEED=$(printf '%s' "$LASTSEEDP" | sed -n 's/.*seed=\([0-9.]*\).*/\1/p')
GAPPLAY=$(awk "BEGIN{g=$SPPOSS-$SPSHOWN; printf \"%.3f\", g}")
echo "SEED-PLAY-VALS: pos=$SPPOSS shown=$SPSHOWN seed=$SPSEED gap(pos-shown)=$GAPPLAY pre_entry_pos=$SEEDPLAY"
[ "$(awk "BEGIN{print ($GAPPLAY+0 >= 0.04) ? 1 : 0}")" = "1" ] \
    || { SEEDERR=$((SEEDERR+1)); echo "JOG-FAIL: SEED play-entry gap(pos-shown)=$GAPPLAY < 0.04 (재생 그림 리드 미실측 — 시드 진실원 계보 부족)"; }
awk "BEGIN{exit (($SPSEED+0 <= $SPPOSS+0) && ($SPSEED+0 >= $SPPOSS+0 - $GAPPLAY - 0.01)) ? 0 : 1}" \
    || { SEEDERR=$((SEEDERR+1)); echo "JOG-FAIL: SEED play-entry seed=$SPSEED ∉ [shown,pos] (시드가 표시 프레임에 뿌리내리지 않음)"; }
# 착지 판독 = **재개 클럭의 성장**이다: release의 시크 핀 = seed(무모션 진입의
# 표시 시드) → 재개 후 park의 핀 = seed + (release→park 사이의 실시간) —
# 판정 = 성장량이 실시간 창(0.6-1.6s — 재개가 시드에서 실시간으로 이어짐을
# 검증; 스톨/점프 0) 안에 드는 것.
SLANDED=$(awk "BEGIN{d=$SEEDLANDED-$SPSEED; if(d<0)d=-d; printf \"%.3f\", d}")
echo "SEED-PLAY-SNAP: growth(landed-seed)=$SLANDED (resume-from-seed-dial — 실시간 창 0.6-1.6s)"
awk "BEGIN{exit ($SLANDED+0 >= 0.60 && $SLANDED+0 <= 1.60) ? 0 : 1}" \
    || { SEEDERR=$((SEEDERR+1)); echo "JOG-FAIL: SEED play-entry resume growth=$SLANDED ∉ [0.6,1.6] (재개가 시드 다이얼에서 표류/습니다)"; }
# 일시정지 진입 대조: park → knobclick → |pos-seed| ≤ 0.1
PARKS=0
for i in 1 2 3; do
    PR=$(ctl '{"tool":"app_tool","args":{"app":"vplayer","tool":"play_pause","args":{}}}')
    sleep 1
    ST=$(status)
    [ "$(pause_of "$ST")" = "true" ] && { PARKS=1; break; }
done
[ "$PARKS" -eq 1 ] || FAIL "SEED leg pre-park failed (paused=$(pause_of "$ST"))"
knobclick || FAIL "knob click failed (seed leg, paused entry)"
sleep 1.1
LASTSEEDQ=$(tail -n +1 "$CLOG" | grep -a 'entry=drag' | tail -1)
echo "SEED-PARK-ROW: $LASTSEEDQ (일시정지 진입 대조)"
QPPOSS=$(printf '%s' "$LASTSEEDQ" | sed -n 's/.*pos=\([0-9.]*\).*/\1/p')
QPSEED=$(printf '%s' "$LASTSEEDQ" | sed -n 's/.*seed=\([0-9.]*\).*/\1/p')
QGAP=$(awk "BEGIN{d=$QPPOSS-$QPSEED; if(d<0)d=-d; printf \"%.3f\", d}")
echo "SEED-PARK-VALS: pos=$QPPOSS seed=$QPSEED |pos-seed|=$QGAP"
[ "$(awk "BEGIN{print ($QGAP+0 <= 0.1) ? 1 : 0}")" = "1" ] \
    || { SEEDERR=$((SEEDERR+1)); echo "JOG-FAIL: SEED paused-entry |pos-seed|=$QGAP > 0.1 (일시정지 기저 분절)"; }

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
OFF_END2=$(wc -l < "$CLOG")   # burst 종료 스냅샷 — 이후 행 = 무입력 glide 원문(r2)
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
awk "BEGIN{exit ($AN2_TSPAN+0 >= 0.90) ? 0 : 1}" || { REVERR=$((REVERR+1)); echo "JOG-FAIL: REV Tspan=$AN2_TSPAN < 0.90 (r2 가속 스캔 원문 부족)"; }
awk "BEGIN{exit (($AN2_TSPAN+0)/10.0 >= 1.3*$SLOWQ) ? 0 : 1}" || { REVERR=$((REVERR+1)); echo "JOG-FAIL: REV accel avg < 1.3x slow quantum (가속 원문 부족 — r2)"; }
awk "BEGIN{exit ((\"$ERRR\"+0) <= 0.3) ? 0 : 1}" || { REVERR=$((REVERR+1)); echo "JOG-FAIL: REV landing err=$ERRR > 0.3"; }
# r2 관성 원문 = burst 종료 이후 행의 무입력 T 감소(≥0.15 s·감소 ≥2)
tail -n +"$((OFF_END2+1))" "$CLOG" | grep -a '^\[vpt13\]' > "$LOGDIR/${JOGRUN}_rev_glide.txt" || true
GRISE2=$(awk '{if (match($0,/T=(-?[0-9.]+)/)) { t=substr($0,RSTART+2,RLENGTH-2); if (p!="" && p-t>=0.03) r++; p=t } } END{print r+0}' "$LOGDIR/${JOGRUN}_rev_glide.txt")
GT2=$(awk '{if (match($0,/T=(-?[0-9.]+)/)) { t=substr($0,RSTART+2,RLENGTH-2); if (n==0||t<mint) mint=t; if (n==0||t>maxt) maxt=t; n++ } } END{print maxt-mint+0}' "$LOGDIR/${JOGRUN}_rev_glide.txt")
echo "REV-GLIDE-VALS: span=$GT2 recedes=$GRISE2"
awk "BEGIN{exit (($GT2+0 >= 0.15) && ($GRISE2+0 >= 2)) ? 0 : 1}" || { REVERR=$((REVERR+1)); echo "JOG-FAIL: REV zero-input glide=$GT2 recedes=$GRISE2 (역방향 관성 동형 부족 — r2)"; }

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
tail -n +1 "$CLOG" | grep -a '\[vptSEED\]' > "$LOGDIR/${JOGRUN}_seed_rows.txt" || true
cp "$LOGDIR/${JOGRUN}_fwd_stream.txt" "$LOGDIR/${JOGRUN}_rev_stream.txt" "$LOGDIR/${JOGRUN}_mute_stream.txt" "$LOGDIR/${JOGRUN}_bwd_stream.txt" "$LOGDIR/${JOGRUN}_bwd_vptB.txt" "$LOGDIR/${JOGRUN}_bwdclamp_stream.txt" "$LOGDIR/${JOGRUN}_bwdclamp_vptB.txt" "$LOGDIR/${JOGRUN}_dirsw_stream.txt" "$LOGDIR/${JOGRUN}_seed_rows.txt" "$LOGDIR/${JOGRUN}_hold_rows.txt" "$RECEIVE/" 2>/dev/null
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
echo "JOG-RUN-VALS-SLOW: run=$JOGRUN slow_seed=$SEEDS slow_landed=$LANDEDS slow_delta=$DELTS slow_raw=$RAWS slow_maxstep=$ANS_MAXDD slow_err=$ERRS slow_samples=$ANS_N glide_fwd_span=$GF_TSPAN rise=$GRISE glide_rev_span=$GT2 recede=$GRISE2"
echo "JOG-RUN-VALS-BWD: run=$JOGRUN bwd_seed=$SEEDB bwd_landed=$LANDEDB bwd_delta=$DELTB bwd_raw=$RAWB bwd_err=$ERRB bwd_step=$ANB_MAXDD bwd_lo_min=$LOMIN bwd_vptB=$NRB bwd_span=$SPANB bwd_samples=$ANB_N clamp_seed=$SEEDD clamp_landed=$LANDEDD clamp_tspan=$AND_TSPAN clamp_lo0=$SIG0 clamp_lom1=$SIGN1 clamp_dmin=$DMINB clamp_samples=$AND_N"
echo "JOG-RUN-VALS: run=$JOGRUN fwd_seed=$SEED fwd_Tspan=$AN_TSPAN fwd_maxdD=$AN_MAXDD fwd_T_end=$LATEST_T fwd_landed=$LANDED fwd_err=$ERRF fwd_glide=$GLIDEF fwd_samples=$AN_N rev_seed=$SEED2 rev_T_end=$T_END2 rev_landed=$LANDED2 rev_err=$ERRR rev_glide=$GLIDER rev_samples=$AN2_N mute_samples=$MIDSAMP mute_mid=$(pause_of "$STMID") mute_after=$(pause_of "$STAFTER") ping_ms=$PINGMS"
TOTERR=$((FWDERR+REVERR+MUTEERR+SLOWERR+BWDERR+DSWERR+SEEDERR))
echo "JOG-RUN-VALS-DIRSW: run=$JOGRUN dirsw_seed=$SEEDW dirsw_samples=$ANW_N switch_quantum=$DS_DD post_rise_rows=$DS_RISE dirsw_landed=$LANDEDW dirsw_Tend=$DS_TLAST dirsw_err=$ERRW seed_play_gap=$GAPPLAY seed_play=$SPSEED seed_park_gap=$QGAP seed_park=$QPSEED"
if [ "$TOTERR" -eq 0 ]; then
    echo "JOGRUN-VERDICT: JOG-OK(T=$JOGRUN) — 휠 연속 워크+관성 감쇠 FWD/REV 동형+BWD 프리필 히트/크리프 graceful+DIRSW 전환 즉시 양자/관성 소멸+SEED 표시 진실원+무음 축 원문 성립 — 캐논 WSL $ST_PASS 등호 원문"
    echo "RESULT: ALL PASS"
else
    echo "JOGRUN-VERDICT: JOG-FAIL(T=$JOGRUN) — 흐름 수치 결함 $TOTERR건 — 리포트 원장(honest-fail — rc=0)"
    echo "RESULT: $TOTERR FAILURE(S)"
fi
echo "JOG-FLOW-END run=$JOGRUN $(date +%T)"
# honest-fail 원칙: JOG-FAIL(흐름 수치)은 원장 목적이라 rc=0. hard FAIL(빌드·
# selftest·부팅·창·클립·기하·시드·소각)만 exit 1.
exit 0