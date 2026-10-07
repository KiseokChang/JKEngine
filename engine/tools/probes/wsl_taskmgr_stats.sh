#!/usr/bin/env bash
# WSL taskmgr posix leg probe (앱 커버리지 Task 5, 2026-10-07).
# 영수증 목표: ① WSL ninja 리빌드(rc=0) ② WSL selftest 기준선 무회귀(399 PASS,
# 0 FAIL) ③ setsid 부팅 → launch_app taskmgr → list_windows에 지정 기하 창
# (클라 900x620) + **pid 0 아님**(JKClientSurface Hello posix leg 트립와이어 —
# leg 이전 실측 pid:0) ④ 서버 capture_window 도구로 taskmgr 표를 PNG로 캡처해
# 비어있지 않음을 산출(사람 육안은 PNG Read — probe는 파일 크기 하한만 잠근다)
# ⑤ /proc Δ 산식 셸 검증(listed pid의 utime+stime 3초 대차) ⑥ 브래킷 pkill 철수
# ⑦ 교차수치(fix r1 review 권장) — 셸 Δ%와 앱 표 CPU%(PNG 육안 교차판독)를
#    영수증에 나란히 기록해 두 채널 독립 검증(누락이 fix r1 버그를 놓친 원인).
#
# 템플릿 관습(wsl_apps_count.sh) 그대로 — wsl.exe는 인라인 인용을 찢으니 파일로
# 실행한다(Git Bash/MSYS 경로 변환이 /mnt/ 경로를 찢으니 MSYS_NO_PATHCONV=1 접두
# 필수):
#   MSYS_NO_PATHCONV=1 wsl.exe -d Ubuntu-24.04 -- bash /mnt/i/progwork/JKENGINE/engine/tools/probes/wsl_taskmgr_stats.sh
# 정리는 WSL 표준 pkill(cdb q kill은 윈도 프로브 패턴 — 여기서 금지).
# 함정(Task 5 실측): 서버가 살아 있는 채로 재빌드하면 drvfs 링커가
# "cannot open output file jkdesktop: No such file or directory"로 죽는다 —
# 살아있는 ELF는 drvfs에서 unlink 불가. 브래킷 pkill이 빌드 앞에 온다.
set -u
cd /mnt/i/progwork/JKENGINE/engine

FAIL() { echo "TASKMGR-FAIL: $*"; exit 1; }

echo "=== 1. bracketed pre-clean (살아있는 jkdesktop이 재링크를 막는다 — 실측) ==="
pkill -f 'buildwsl/[j]kdesktop' 2>/dev/null
sleep 1
pkill -9 -f 'buildwsl/[j]kdesktop' 2>/dev/null
sleep 1
LEFT=$(ps -eo args | grep -i 'buildwsl/[j]kdesktop' | wc -l)
[ "$LEFT" -eq 0 ] || FAIL "pre-clean left $LEFT jkdesktop process(es) alive"

echo "=== 2. ninja rebuild (buildwsl) ==="
ninja -C buildwsl -j3 2>&1 | tail -4
BUILD_RC=${PIPESTATUS[0]}
echo "WSL-BUILD-RC=$BUILD_RC"
[ "$BUILD_RC" -eq 0 ] || FAIL "ninja rebuild rc=$BUILD_RC"
[ -x buildwsl/jkdesktop ] || FAIL "buildwsl/jkdesktop missing after rebuild"
[ -f buildwsl/jkapp_taskmgr.so ] || FAIL "buildwsl/jkapp_taskmgr.so missing"

echo "=== 3. WSL selftest (Task 3 기준선 399 하한) ==="
timeout 300 ./buildwsl/jkdesktop test >/tmp/t5_st.log 2>&1
ST_RC=$?
echo "selftest rc=$ST_RC"
ST_PASS=$(grep -ac '^\[PASS\]' /tmp/t5_st.log)
ST_FAIL=$(grep -ac '^\[FAIL\]' /tmp/t5_st.log)
echo "selftest PASS=$ST_PASS FAIL=$ST_FAIL"
grep -aq 'AppSelfTest: 0 failure(s)' /tmp/t5_st.log \
    || FAIL "selftest summary missing 'AppSelfTest: 0 failure(s)'"
[ "$ST_FAIL" -eq 0 ] || FAIL "selftest FAIL=$ST_FAIL — log: /tmp/t5_st.log"
[ "$ST_RC" -eq 0 ] || FAIL "selftest rc=$ST_RC"
[ "$ST_PASS" -ge 399 ] || FAIL "selftest PASS=$ST_PASS < 기준선 399"

# 어설션 헬퍼(wsl_apps_count.sh 템플릿).
ASSERT_FIELD() { printf '%s' "$1" | grep -aq "$2" || FAIL "$3 — reply: $1"; }

echo "=== 4. boot server (setsid nohup detached, WSLg :0) ==="
rm -f /tmp/JKWindowServerPipe.sock
env DISPLAY=:0 setsid nohup ./buildwsl/jkdesktop --server >/tmp/t5_srv.log 2>&1 &
sleep 5
SRV_PIDS=$(pgrep -f 'buildwsl/jkdesktop' | tr '\n' ' ')
if [ -z "$SRV_PIDS" ]; then
    echo "--- server log tail:"; tail -20 /tmp/t5_srv.log
    FAIL "no jkdesktop process 5s after setsid boot (server died)"
fi
echo "server pids: $SRV_PIDS"
PING_OUT=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"ping","args":{}}' 2>/dev/null | grep -a '{' | head -1)
ASSERT_FIELD "$PING_OUT" '"ok":true' "ping did not ok"

echo "=== 5. launch_app taskmgr → list_windows (기하 + pid 0 아님 트립와이어) ==="
L_OUT=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"launch_app","args":{"app":"taskmgr"}}' 2>/dev/null | grep -a '{' | head -1)
echo "launch taskmgr: $L_OUT"
[ -n "$L_OUT" ] || FAIL "launch_app taskmgr got no reply (server gone?)"
ASSERT_FIELD "$L_OUT" '"ok":true' "launch_app taskmgr did not ok"
sleep 5
WIN_OUT=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)
echo "list_windows: $WIN_OUT"
[ -n "$WIN_OUT" ] || FAIL "list_windows got no reply"
TM_WIN=$(printf '%s' "$WIN_OUT" | grep -aoE '\{"id":[^}]*"title":"Task Manager"[^}]*\}' | head -1)
[ -n "$TM_WIN" ] || FAIL "list_windows has no Task Manager window object — reply: $WIN_OUT"
echo "taskmgr window: $TM_WIN"
# 지정 기하: 클라 OnInit SetWindowRect(0,0,900,620) — 실측 그대로 900x620을
# 잠근다(이보다 작으면 무언가가 기하를 무시하는 것).
printf '%s' "$TM_WIN" | grep -aq '"w":900' || FAIL "taskmgr window w!=900 (지정 기하 이탈) — window: $TM_WIN"
printf '%s' "$TM_WIN" | grep -aq '"h":620' || FAIL "taskmgr window h!=620 (지정 기하 이탈) — window: $TM_WIN"
# pid 0 아님(Task 5 이전 실측=pid:0 — JKClientSurface Hello posix leg의 트립와이어).
# Hello leg가 깨지면 샘플러는 행을 건너뛰고 표가 영원히 "..."로 남는다.
TM_PID=$(printf '%s' "$TM_WIN" | sed -n 's/.*"pid":\([0-9]*\),.*/\1/p')
echo "taskmgr window pid: $TM_PID"
[ -n "$TM_PID" ] && [ "$TM_PID" -ne 0 ] \
    || FAIL "taskmgr window reports pid=0 (Hello posix leg 깨짐 — /proc 샘플러 표 채움 불가)"
[ -r /proc/"$TM_PID"/stat ] || FAIL "/proc/$TM_PID/stat not readable (표 채움 불가 환경 — hidepid?)"

echo "=== 6. /proc Δ 산식 셸 검증 listed pid의 utime+stime 3초 대차 ==="
# 같은 /proc ABI를 같은 환경에서 읽는 대차 — 앱 leg가 쓰는 산식의 원료.
T1=$(awk '{print $14+$15}' /proc/"$TM_PID"/stat 2>/dev/null) || FAIL "stat read 1 failed"
[ -n "$T1" ] || FAIL "stat read 1 empty"
sleep 3
T2=$(awk '{print $14+$15}' /proc/"$TM_PID"/stat 2>/dev/null) || FAIL "stat read 2 failed"
[ -n "$T2" ] || FAIL "stat read 2 empty"
DPROC=$((T2 - T1))
CLK_TCK=$(getconf CLK_TCK)
CORES=$(nproc)
echo "PROC-DELTA-TICKS=$DPROC (3s window, CLK_TCK=$CLK_TCK, cores=$CORES)"
[ "$DPROC" -gt 0 ] || FAIL "proc delta=0 over 3s (살아있는 프로세스가 유휴여도 렌더로 틱이 오른다 — Δ 경로 무음 의심)"
RSS=$(grep -a 'VmRSS:' /proc/"$TM_PID"/status 2>/dev/null | awk '{print $2}')
echo "RSS-KB=$RSS"
[ -n "$RSS" ] && [ "$RSS" -gt 0 ] || FAIL "VmRSS unreadable/0 — 메모리 열이 채워질 수 없다"

echo "=== 7. capture_window → taskmgr 표 PNG 산출 (내용 육안은 이 스크립트 밖) ==="
CAP_ID=$(printf '%s' "$TM_WIN" | sed -n 's/.*"id":\([0-9]*\),.*/\1/p')
CAP_OUT=$(timeout 20 ./buildwsl/jkdesktop agentctl '{"tool":"capture_window","args":{"id":'"$CAP_ID"'}}' 2>/dev/null | grep -a '{' | head -1)
echo "capture_window: $CAP_OUT"
ASSERT_FIELD "$CAP_OUT" '"ok":true' "capture_window did not ok (gate=ask/deny 또는 레이어 부재)"
CAP_PATH=$(printf '%s' "$CAP_OUT" | sed -n 's/.*"path":"\([^"]*\)".*/\1/p')
[ -f "$CAP_PATH" ] || FAIL "captured PNG missing: $CAP_PATH"
CAP_BYTES=$(wc -c < "$CAP_PATH")
echo "CAPTURE-BYTES=$CAP_BYTES"
[ "$CAP_BYTES" -ge 20000 ] || FAIL "captured PNG suspiciously small ($CAP_BYTES bytes) — 빈 표 렌더 의심"
CAP_WH=$(timeout 20 python3 - "$CAP_PATH" <<'PY' 2>/dev/null
import struct, sys
head = open(sys.argv[1], 'rb').read(24)
w, h = struct.unpack('>II', head[16:24])
print(w, h)
PY
)
echo "CAPTURE-WH=$CAP_WH (expected 900 620 — 클라 프레임버퍼 원형)"
printf '%s' "$CAP_WH" | grep -aq '900 620' || FAIL "capture geometry not 900 620: $CAP_WH"
echo "RECEIPT: $(ls -1 buildwsl/state/screenshots/shot_*.png | head -1)"

# 교차수치(fix r1 review 권장): 셸 Δ채널 %와 앱 표 CPU%를 독립 채널로 영수증에
# 나란히 둔다 — T5 fix r1(sscanf 억제 1개 누락: utime←cmajflt, stime←utime)
# 이 안 잡힌 원인이 두 채널 독립 검증 부재였다. 앱 표 CPU%는 PNG 육안
# (RECEIPT 경로) 교차판독으로 마감 — 자동 OCR은 현 단계 과다(대역 어설션은
# 샘플 창 미정렬로 유령 실패만 산한다).
CROSS_PCT=$(awk -v d="$DPROC" -v hz="$CLK_TCK" -v c="$CORES" \
    'BEGIN{printf "%.1f", d/hz/3.0/c*100}')
echo "CROSS-CHECK: shell Δ% of machine=$CROSS_PCT (Δticks=$DPROC/3s, CLK_TCK=$CLK_TCK) —"
echo "             app 표 CPU%(capture PNG 육안 교차판독)는 같은 오더여야 한다(두 채널 독립)."

echo "=== 8. cleanup (bracketed pkill — WSL kill discipline) ==="
pkill -f 'buildwsl/jkdesktop' 2>/dev/null
sleep 1
pkill -9 -f 'buildwsl/[j]kdesktop' 2>/dev/null
sleep 1
LEFT=$(ps -eo args | grep -i 'buildwsl/[j]kdesktop' | wc -l)
[ "$LEFT" -eq 0 ] || echo "WARN: $LEFT jkdesktop process(es) survived teardown"

echo "TASKMGR-OK"
exit 0
