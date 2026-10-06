#!/usr/bin/env bash
# WSL chat boot probe (스펙 2026-10-07-desktop-chat-app Task 4 — §1.1,
# wsl_library_boot.sh 원준 승계).
# 영수증 목표: ① WSL ninja 리빌드 ② jkdesktop library-list rc=0 + chat 내장
# 단일 진실원 행 ③ setsid 서버 부팅 생존 ④ agentctl launch_app {"app":"chat"}
# ok:true ⑤ list_windows에 "title":"Chat" 720x540 focused ⑥ 정리 후 CHAT-BOOT-OK.
#   템플릿: wsl_library_boot.sh 그대로 — wsl.exe는 인라인 인용을 찢으니
#   이 스크립트는 항상 파일로 실행한다(Git Bash/MSYS 경로 변환이 /mnt/ 경로를
#   C:/Program Files/Git/... 로 찢으니 MSYS_NO_PATHCONV=1 접두 필수 — 실측:
#   접두 없으면 "No such file or directory"):
#     MSYS_NO_PATHCONV=1 wsl.exe -d Ubuntu-24.04 -- bash /mnt/i/progwork/JKENGINE/engine/tools/probes/wsl_chat_boot.sh
#   agentctl 와이어(docs/78 §5.4 함정 ①): 서브커맨드는 'agentctl' —
#   'agent'로 치면 demo 앱으로 넘어가 CPU 루프를 돈다. 키= tool/args.
#   스캔 행 계약 결정: ClientChatApp::OnInit은 stderr 스캔 행을 인쇄하지
#   않는다(2026-10-07 실측 — [library] apps=%d ClientLibraryApp.cpp:104 선례와
#   달리 스캔 개수 로그 없음). brief 규약에 따라 스캔 행 어설션은 생략하고
#   launch ok + 창 기하만 단정한다.
#   정리는 WSL 표준 pkill(cdb q kill은 윈도 프로브 패턴 — 여기서 금지).
set -u
cd /mnt/i/progwork/JKENGINE/engine

FAIL() { echo "CHAT-BOOT-FAIL: $*"; exit 1; }

echo "=== 1. ninja rebuild (buildwsl) ==="
ninja -C buildwsl -j3 2>&1 | tail -6
BUILD_RC=${PIPESTATUS[0]}
echo "WSL-BUILD-RC=$BUILD_RC"
[ "$BUILD_RC" -eq 0 ] || FAIL "ninja rebuild rc=$BUILD_RC"
[ -x buildwsl/jkdesktop ] || FAIL "buildwsl/jkdesktop missing after rebuild"
[ -e buildwsl/jkapp_chat.so ] || FAIL "buildwsl/jkapp_chat.so missing after rebuild (launch presence check would reject app=chat)"

echo "=== 2. library-list (serverless catalog CLI) ==="
timeout 30 ./buildwsl/jkdesktop library-list >/tmp/chat_lib_list.out 2>/tmp/chat_lib_list.err
LIST_RC=$?
echo "library-list rc=$LIST_RC"
if [ "$LIST_RC" -ne 0 ]; then
    echo "--- stderr:"; cat /tmp/chat_lib_list.err
    FAIL "library-list rc=$LIST_RC (expected 0)"
fi
cat /tmp/chat_lib_list.out
NAMED=$(grep -c '^name=' /tmp/chat_lib_list.out)
COUNTLINE=$(grep -aE '^count=[0-9]+ base=' /tmp/chat_lib_list.out | head -1)
[ -n "$COUNTLINE" ] || FAIL "library-list has no 'count=<n> base=' tail (malformed output)"
COUNT=$(printf '%s' "$COUNTLINE" | sed -n 's/^count=\([0-9]*\) .*$/\1/p')
[ "$NAMED" -eq "$COUNT" ] || FAIL "library-list line/count mismatch: name= lines=$NAMED count=$COUNT"
awk '/^name=/ { if ($0 !~ /title=/ || $0 !~ /source=/ || $0 !~ /caps=/ || $0 !~ /size=/ || $0 !~ /path=/) { print "malformed: " $0; exit 1 } }' /tmp/chat_lib_list.out || FAIL "library-list name= line missing required fields (title/source/caps/size/path)"
CHATLINE=$(grep -aE '^name=chat title=Chat source=builtin ' /tmp/chat_lib_list.out | head -1)
[ -n "$CHATLINE" ] || FAIL "library-list has no 'name=chat title=Chat source=builtin' line (T2 builtin registration lost on posix) — output: $(cat /tmp/chat_lib_list.out | tr '\n' ' ')"
echo "chat builtin line: $CHATLINE"

# 어설션 헬퍼(wsl_library_boot.sh final review Item 4 — `|| true` 관용 금지,
# 실패=FAIL 경로). 서버는 compact JSON으로 답한다(JKWindowServer
# "ping"=`"ok":true,"pong":true`, list_windows 항목=
# `{"id":..,"w":720,"h":540,..,"focused":true|false}) — 실측 형태 그대로 잠근다.
ASSERT_FIELD() { printf '%s' "$1" | grep -aq "$2" || FAIL "$3 — reply: $1"; }

echo "=== 3. boot server (setsid nohup detached, WSLg :0) ==="
pkill -f 'buildwsl/jkdesktop' 2>/dev/null
rm -f /tmp/JKWindowServerPipe.sock
sleep 1
env DISPLAY=:0 setsid nohup ./buildwsl/jkdesktop --server >/tmp/chat_srv.log 2>&1 &
sleep 5
SRV_PIDS=$(pgrep -x jkdesktop | tr '\n' ' ')
if [ -z "$SRV_PIDS" ]; then
    echo "--- server log tail:"; tail -20 /tmp/chat_srv.log
    FAIL "no jkdesktop process 5s after setsid boot (server died)"
fi
echo "server pids: $SRV_PIDS"
PING_OUT=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"ping","args":{}}' 2>/dev/null | grep -a '{' | head -1)
echo "ping reply: $PING_OUT"
[ -n "$PING_OUT" ] || FAIL "agentctl ping got no reply JSON (server unresponsive)"
ASSERT_FIELD "$PING_OUT" '"ok":true' "ping did not ok"

echo "=== 4. launch_app {app:chat} ==="
LAUNCH_OUT=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"launch_app","args":{"app":"chat"}}' 2>/dev/null | grep -a '{' | head -1)
echo "launch reply: $LAUNCH_OUT"
[ -n "$LAUNCH_OUT" ] || FAIL "launch_app got no reply (server gone?)"
grep -aq '"ok":true' <<< "$LAUNCH_OUT" || FAIL "launch_app did not ok — reply: $LAUNCH_OUT"

echo "=== 5. list_windows — receipt: title=Chat + geometry 720x540 + focused ==="
sleep 8
WIN_OUT=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)
echo "list_windows: $WIN_OUT"
[ -n "$WIN_OUT" ] || FAIL "list_windows got no reply"
# Chat 창 오브젝트만 잘라 기하·포커스까지 잠근다(항목 오브젝트엔 중괄호가
# 없어 [^}]* 절단이 안전하다). 실패 어설션은 그대로 FAIL — 관용 없음.
CHAT_WIN=$(printf '%s' "$WIN_OUT" | grep -aoE '\{"id":[^}]*"title":"Chat"[^}]*\}' | head -1)
[ -n "$CHAT_WIN" ] || FAIL "list_windows has no Chat window object (launch_app chat lost) — reply: $WIN_OUT"
ASSERT_FIELD "$CHAT_WIN" '"title":"Chat"' "Chat window title lost"
ASSERT_FIELD "$CHAT_WIN" '"w":720' "Chat window w != 720"
ASSERT_FIELD "$CHAT_WIN" '"h":540' "Chat window h != 540"
ASSERT_FIELD "$CHAT_WIN" '"focused":true' "Chat window not focused"

echo "=== 6. cleanup (pkill — WSL kill discipline) ==="
pkill -f 'buildwsl/jkdesktop' 2>/dev/null
sleep 1
pkill -9 -f 'buildwsl/[j]kdesktop' 2>/dev/null
sleep 1
LEFT=$(ps -eo args | grep -i 'buildwsl/[j]kdesktop' | wc -l)
[ "$LEFT" -eq 0 ] || echo "WARN: $LEFT jkdesktop process(es) survived teardown"

echo "CHAT-BOOT-OK"
exit 0
