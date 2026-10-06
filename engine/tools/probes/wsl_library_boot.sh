#!/usr/bin/env bash
# WSL library boot probe (스펙 2026-10-06-app-library Task 4 — §6 posix 실측).
# 영수증 목표: ① WSL ninja 리빌드 ② jkdesktop library-list rc=0+파서 가능 행
# ③ setsid 서버 부팅 생존 ④ agentctl launch_app {"app":"library"} ok:true
# ⑤ list_windows에 "title":"Library" ⑥ 정리 후 LIBRARY-BOOT-OK.
#   템플릿: wsl_cpu_pacing.sh 훅 그대로 — wsl.exe는 인라인 인용을 찢으니
#   이 스크립트는 항상 파일로 실행한다(Git Bash/MSYS 경로 변환이 /mnt/ 경로를
#   C:/Program Files/Git/... 로 찢으니 MSYS_NO_PATHCONV=1 접두 필수 — 실측:
#   접두 없으면 "No such file or directory"):
#     MSYS_NO_PATHCONV=1 wsl.exe -d Ubuntu-24.04 -- bash /mnt/i/progwork/JKENGINE/engine/tools/probes/wsl_library_boot.sh
#   agentctl 와이어(docs/78 §5.4 함정 ①): 서브커맨드는 'agentctl' —
#   'agent'로 치면 demo 앱으로 넘어가 CPU 루프를 돈다. 키= tool/args.
#   정리는 WSL 표준 pkill(cdb q kill은 윈도 프로브 패턴 — 여기서 금지).
set -u
cd /mnt/i/progwork/JKENGINE/engine

FAIL() { echo "LIBRARY-BOOT-FAIL: $*"; exit 1; }

echo "=== 1. ninja rebuild (buildwsl) ==="
ninja -C buildwsl -j3 2>&1 | tail -6
BUILD_RC=${PIPESTATUS[0]}
echo "WSL-BUILD-RC=$BUILD_RC"
[ "$BUILD_RC" -eq 0 ] || FAIL "ninja rebuild rc=$BUILD_RC"
[ -x buildwsl/jkdesktop ] || FAIL "buildwsl/jkdesktop missing after rebuild"
[ -e buildwsl/jkapp_library.so ] || FAIL "buildwsl/jkapp_library.so missing after rebuild (launch presence check would reject app=library)"

echo "=== 2. library-list (serverless catalog CLI) ==="
timeout 30 ./buildwsl/jkdesktop library-list >/tmp/lib_list.out 2>/tmp/lib_list.err
LIST_RC=$?
echo "library-list rc=$LIST_RC"
if [ "$LIST_RC" -ne 0 ]; then
    echo "--- stderr:"; cat /tmp/lib_list.err
    FAIL "library-list rc=$LIST_RC (expected 0)"
fi
cat /tmp/lib_list.out
NAMED=$(grep -c '^name=' /tmp/lib_list.out)
COUNTLINE=$(grep -aE '^count=[0-9]+ base=' /tmp/lib_list.out | head -1)
[ -n "$COUNTLINE" ] || FAIL "library-list has no 'count=<n> base=' tail (malformed output)"
COUNT=$(printf '%s' "$COUNTLINE" | sed -n 's/^count=\([0-9]*\) .*$/\1/p')
[ "$NAMED" -eq "$COUNT" ] || FAIL "library-list line/count mismatch: name= lines=$NAMED count=$COUNT"
awk '/^name=/ { if ($0 !~ /title=/ || $0 !~ /source=/ || $0 !~ /caps=/ || $0 !~ /size=/ || $0 !~ /path=/) { print "malformed: " $0; exit 1 } }' /tmp/lib_list.out || FAIL "library-list name= line missing required fields (title/source/caps/size/path)"
grep -aq 'source=builtin' /tmp/lib_list.out || echo "WARN: no source=builtin line (minesweeper/tetris) in catalog — non-fatal per brief's no-over-assert rule"

# 어설션 헬퍼(final review Item 4 — `|| true` 관용 금지, 실패=FAIL 경로).
# 서버는 compact JSON으로 답한다(JKWindowServer "ping"=`"ok":true,"pong":true`,
# list_windows 항목=`{"id":..,"w":920,"h":640,..,"focused":true|false}) —
# 실측 형태 그대로 잠근다.
ASSERT_FIELD() { printf '%s' "$1" | grep -aq "$2" || FAIL "$3 — reply: $1"; }

echo "=== 3. boot server (setsid nohup detached, WSLg :0) ==="
pkill -f 'buildwsl/jkdesktop' 2>/dev/null
rm -f /tmp/JKWindowServerPipe.sock
sleep 1
env DISPLAY=:0 setsid nohup ./buildwsl/jkdesktop --server >/tmp/lib_srv.log 2>&1 &
sleep 5
SRV_PIDS=$(pgrep -x jkdesktop | tr '\n' ' ')
if [ -z "$SRV_PIDS" ]; then
    echo "--- server log tail:"; tail -20 /tmp/lib_srv.log
    FAIL "no jkdesktop process 5s after setsid boot (server died)"
fi
echo "server pids: $SRV_PIDS"
PING_OUT=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"ping","args":{}}' 2>/dev/null | grep -a '{' | head -1)
echo "ping reply: $PING_OUT"
[ -n "$PING_OUT" ] || FAIL "agentctl ping got no reply JSON (server unresponsive)"
ASSERT_FIELD "$PING_OUT" '"ok":true' "ping did not ok"

echo "=== 4. launch_app {app:library} ==="
LAUNCH_OUT=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"launch_app","args":{"app":"library"}}' 2>/dev/null | grep -a '{' | head -1)
echo "launch reply: $LAUNCH_OUT"
[ -n "$LAUNCH_OUT" ] || FAIL "launch_app got no reply (server gone?)"
grep -aq '"ok":true' <<< "$LAUNCH_OUT" || FAIL "launch_app did not ok — reply: $LAUNCH_OUT"

echo "=== 5. list_windows — receipt: title=Library + geometry 920x640 + focused ==="
sleep 8
WIN_OUT=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)
echo "list_windows: $WIN_OUT"
[ -n "$WIN_OUT" ] || FAIL "list_windows got no reply"
# Library 창 오브젝트만 잘라 기하·포커스까지 잠근다(항목 오브젝트엔 중괄호가
# 없어 [^}]* 절단이 안전하다). 실패 어설션은 그대로 FAIL — 관용 없음.
LIB_WIN=$(printf '%s' "$WIN_OUT" | grep -aoE '\{"id":[^}]*"title":"Library"[^}]*\}' | head -1)
[ -n "$LIB_WIN" ] || FAIL "list_windows has no Library window object (spawn or module fallback lost) — reply: $WIN_OUT"
ASSERT_FIELD "$LIB_WIN" '"title":"Library"' "Library window title lost"
ASSERT_FIELD "$LIB_WIN" '"w":920' "Library window w != 920"
ASSERT_FIELD "$LIB_WIN" '"h":640' "Library window h != 640"
ASSERT_FIELD "$LIB_WIN" '"focused":true' "Library window not focused"

echo "=== 6. cleanup (pkill — WSL kill discipline) ==="
pkill -f 'buildwsl/jkdesktop' 2>/dev/null
sleep 1
pkill -9 -f 'buildwsl/jkdesktop' 2>/dev/null
sleep 1
LEFT=$(ps -eo args | grep -i 'buildwsl/jkdesktop' | grep -v grep | wc -l)
[ "$LEFT" -eq 0 ] || echo "WARN: $LEFT jkdesktop process(es) survived teardown"

echo "LIBRARY-BOOT-OK"
exit 0
