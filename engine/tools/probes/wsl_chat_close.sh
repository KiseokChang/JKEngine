#!/usr/bin/env bash
# WSL close/focus 해소 probe (채팅 F1 — plan 2026-10-08-chat-close-fix, docs/80
# §4 귀속 백로그 착지 실측. wsl_chat_boot.sh 원준 — agentctl 와이어+setsid 부팅
# 템플릿 승계).
# 영수증 목표: ① WSL ninja 리빌드 ② WSL selftest 편승(캐논 399 → 406 — 신설
#   1n-s 7행 증분, 무회귀) ③ close_window argless(포커스 창) 실측: launch
#   minesweeper → close argless → list_windows 소멸 단정 ④ close_window
#   args.app 지명 실측(제목 매칭 대소문자 무시) → 소멸 단정 ⑤ focus_window
#   args.app 지명 실측(list_windows focused:true 전이 단정) ⑥ 무매칭·창 부재는
#   window_not_found로 정직 회신 ⑦ args.id 직접호출 계약 보존.
#   실행법(반드시 파일로 — wsl.exe 인라인 인용 찢김 함정, wsl_chat_boot 선례):
#     MSYS_NO_PATHCONV=1 wsl.exe -d Ubuntu-24.04 -- bash /mnt/i/progwork/JKENGINE/engine/tools/probes/wsl_chat_close.sh
#   권한 함정 원장: close_window는 파일 부재 기본=Deny(M1 규칙 — AgentTool
#   Allowed defaultDecision). 폰/WSL build 트리에 permissions.json이 없어 무단
#   close 실측은 permission_denied로 사망한다. 본 probe는 buildwsl/
#   permissions.json이 **부재 시에만** {"close_window":"allow"}를 임시로 만들고
#   teardown에서 원복(부재 → 삭제)한다 — 기존 파일이 있으면 백업·복원(원문
#   불변, M1 승인 행위 규약 준수). 정리는 WSL 표준 bracketed pkill.
set -u
cd /mnt/i/progwork/JKENGINE/engine

FAIL() { echo "CHAT-CLOSE-FAIL: $*"; exit 1; }

ASSERT_FIELD() { printf '%s' "$1" | grep -aq "$2" || FAIL "$3 — reply: $1"; }

echo "=== 1. ninja rebuild (buildwsl) ==="
ninja -C buildwsl -j3 2>&1 | tail -4
BUILD_RC=${PIPESTATUS[0]}
echo "WSL-BUILD-RC=$BUILD_RC"
[ "$BUILD_RC" -eq 0 ] || FAIL "ninja rebuild rc=$BUILD_RC"
[ -x buildwsl/jkdesktop ] || FAIL "buildwsl/jkdesktop missing after rebuild"

echo "=== 2. WSL selftest 편승 (캐논 399 → 406 무회귀 — 1n-s 7행 신설 증분) ==="
timeout 300 ./buildwsl/jkdesktop test >/tmp/chatclose_st.log 2>&1
ST_RC=$?
echo "selftest rc=$ST_RC"
ST_PASS=$(grep -ac '^\[PASS\]' /tmp/chatclose_st.log)
ST_FAIL=$(grep -ac '^\[FAIL\]' /tmp/chatclose_st.log)
echo "selftest PASS=$ST_PASS FAIL=$ST_FAIL (하한 앵커 406 = WSL 캐논 399 + 1n-s 7)"
grep -a 'AppSelfTest' /tmp/chatclose_st.log | tail -2
grep -aq 'AppSelfTest: 0 failure(s)' /tmp/chatclose_st.log \
    || FAIL "selftest summary missing 'AppSelfTest: 0 failure(s)'"
[ "$ST_FAIL" -eq 0 ] || FAIL "selftest FAIL=$ST_FAIL — log: /tmp/chatclose_st.log"
[ "$ST_RC" -eq 0 ] || FAIL "selftest rc=$ST_RC"
[ "$ST_PASS" -ge 406 ] || FAIL "selftest PASS=$ST_PASS < 406 (WSL 캔캐논 399+7 회귀)"
grep -aq '1n-s1 argless' /tmp/chatclose_st.log || FAIL "selftest missing 1n-s cases (resolve 도표 미실행)"
for s in 1 2 3 4 5 6 7; do
    grep -aq "^\[PASS\] 1n-s$s" /tmp/chatclose_st.log || FAIL "selftest 1n-s$s not PASS"
done
echo "WSL-SELFTEST-PASS=$ST_PASS (캐논 갱신 후보)"

echo "=== 3. permissions.json 임시 스테이지 (부재 시에만 — M1 승인 행위, 원복 계약) ==="
PERM=buildwsl/permissions.json
PERM_BAK=/tmp/chatclose_perm_bak.json
PERM_STAGED=0
if [ -f "$PERM" ]; then
    cp "$PERM" "$PERM_BAK"
    echo "PERM-STAGE=RESTORED-FROM-BACKUP (기존 파일 보존 — teardown에서 복원)"
else
    PERM_STAGED=1
    echo "PERM-STAGE=CREATED-NO-PRIOR (기존 permissions.json 부재 실측 — teardown에서 삭제)"
fi
printf '{\n    "close_window":  "allow"\n}\n' > "$PERM"
# 봉인 후 단정: 서버는 호출마다 파일을 다시 읽는다(핫리드 계약) — 스테이지
# 직후 agentctl 접속 전 값이 "allow"인지 원문 실측.
grep -aq '"close_window"' "$PERM" || FAIL "staged permissions.json malformed"

echo "=== 4. boot server (setsid nohup detached, WSLg :0) ==="
SRV_BEFORE=$(pgrep -f 'buildwsl/jkdesktop' | tr '\n' ' ')
[ -n "$SRV_BEFORE" ] && echo "WARN-PRE-EXISTING-SERVER-PIDS=$SRV_BEFORE (선 존재 — 전회 런 잔존, 부팅 불가 회피 절사)"
pkill -f 'buildwsl/[j]kdesktop' 2>/dev/null
sleep 1
# WSL 서버 SIGTERM 흡수 함정(원장 — pre-clean 무력, -9 에스컬레이션 필수):
# SIGTERM한 서버가 가드를 붙잡아 재부팅한 서버가 사망했다(본 probe 1차 실측).
pkill -9 -f 'buildwsl/[j]kdesktop' 2>/dev/null
sleep 1
pgrep -f 'buildwsl/[j]kdesktop' >/dev/null 2>&1 \
    && FAIL "pre-clean could not clear stale server/clients (SIGKILL 후에도 잔존)"
rm -f /tmp/JKWindowServerPipe.sock
sleep 1
env DISPLAY=:0 setsid nohup ./buildwsl/jkdesktop --server >/tmp/chatclose_srv.log 2>&1 &
sleep 5
SRV_PIDS=$(pgrep -f 'buildwsl/[j]kdesktop' | tr '\n' ' ')
[ -n "$SRV_PIDS" ] || { echo "--- server log tail:"; tail -20 /tmp/chatclose_srv.log; FAIL "no jkdesktop process 5s after boot"; }
echo "server pids (probe-booted): $SRV_PIDS"
PING=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"ping","args":{}}' 2>/dev/null | grep -a '{' | head -1)
echo "ping reply: $PING"
ASSERT_FIELD "$PING" '"ok":true' "ping did not ok"

# 창 목록 절단 헬퍼 — 항목 오브젝트는 중괄호 없는 필드라 [^}]* 절단 안전
WIN_OF() { printf '%s' "$1" | grep -aoE "\{\"id\":[^}]*\"title\":\"$2\"[^}]*\}" | head -1; }
WAIT_GONE() { # $1=title — 최대 15초 폴링 소멸 단정(클라 quit 반영 지연 흡수)
    for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15; do
        W=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)
        [ -z "$(WIN_OF "$W" "$1")" ] && return 0
        sleep 1
    done
    return 1
}
WAIT_HAS() { # $1=title — 최대 15초 폴링 등장 단정(폰/WSL 스폰 지연 흡수)
    for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15; do
        W=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)
        O=$(WIN_OF "$W" "$1")
        [ -n "$O" ] && { echo "$O"; return 0; }
        sleep 1
    done
    return 1
}

echo "=== 5. launch minesweeper → 새 창 = 포커스(list_windows focused:true 근거) ==="
L=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"launch_app","args":{"app":"minesweeper"}}' 2>/dev/null | grep -a '{' | head -1)
echo "launch minesweeper: $L"
ASSERT_FIELD "$L" '"ok":true' "launch_app minesweeper did not ok"
MS=$(WAIT_HAS "Minesweeper") || FAIL "Minesweeper window never appeared in list_windows"
echo "minesweeper window: $MS"
printf '%s' "$MS" | grep -aq '"focused":true' \
    || FAIL "Minesweeper not focused after spawn (focusedClientId_ 근거 붕괴 — list_windows 원문: $MS)"
MS_ID=$(printf '%s' "$MS" | sed -n 's/^{"id":\([0-9]*\),.*$/\1/p')
echo "minesweeper id=$MS_ID focused=true"

echo "=== 6. close_window argless → 포커스 창 소멸 단정 (폰 \"닫아줘\"의 본 경로) ==="
C1=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"close_window","args":{}}' 2>/dev/null | grep -a '{' | head -1)
echo "close argless reply: $C1"
ASSERT_FIELD "$C1" '"ok":true' "close_window argless did not ok (해소 실패?)"
WAIT_GONE "Minesweeper" || FAIL "Minesweeper window survived argless close — list_windows: $(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)"
echo "GONE-AFTER-ARGLESS-CLOSE=OK"

echo "=== 7. close_window app 지명 (args.app=제목 매칭 — 대소문자 무시) ==="
L=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"launch_app","args":{"app":"minesweeper"}}' 2>/dev/null | grep -a '{' | head -1)
ASSERT_FIELD "$L" '"ok":true' "relaunch minesweeper did not ok"
MS=$(WAIT_HAS "Minesweeper") || FAIL "Minesweeper relaunch never appeared"
C2=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"close_window","args":{"app":"minesweeper"}}' 2>/dev/null | grep -a '{' | head -1)
echo "close app=minesweeper reply: $C2"
ASSERT_FIELD "$C2" '"ok":true' "close_window app 지명 did not ok (제목 매칭 실패?)"
WAIT_GONE "Minesweeper" || FAIL "Minesweeper survived app-named close"
echo "GONE-AFTER-APP-NAMED-CLOSE=OK"

echo "=== 8. focus_window app 지명 (list_windows focused:true 전이 단정) ==="
# close로 소멸시킨 지뢰찾기를 다시 띄워 두고(스폰 = 무조건 포커스 계약), 두 창
# 사이의 지명 포커스 전이를 관측한다.
L=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"launch_app","args":{"app":"minesweeper"}}' 2>/dev/null | grep -a '{' | head -1)
ASSERT_FIELD "$L" '"ok":true' "relaunch minesweeper (focus leg) did not ok"
MS=$(WAIT_HAS "Minesweeper") || FAIL "Minesweeper (focus leg) never appeared"
L=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"launch_app","args":{"app":"tetris"}}' 2>/dev/null | grep -a '{' | head -1)
ASSERT_FIELD "$L" '"ok":true' "launch_app tetris did not ok"
TT=$(WAIT_HAS "Tetris") || FAIL "Tetris window never appeared"
echo "tetris window: $TT"
# 스폰 = 무조건 포커스(JKWindowServer.cpp 749부근) → 마지막 스폰인 Tetris가
# 포커스 — 여기서 app 지명으로 Minesweeper에 포커스를 되돌린다(전이 단정).
F2=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"focus_window","args":{"app":"minesweeper"}}' 2>/dev/null | grep -a '{' | head -1)
echo "focus app=minesweeper reply: $F2"
ASSERT_FIELD "$F2" '"ok":true' "focus_window app 지명 did not ok"
sleep 1
W=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)
TT2=$(WIN_OF "$W" "Tetris")
MS2=$(WIN_OF "$W" "Minesweeper")
printf '%s' "$MS2" | grep -aq '"focused":true' || FAIL "Minesweeper not focused after app-named focus: $MS2"
printf '%s' "$TT2" | grep -aq '"focused":false' || FAIL "Tetris wrongly still focused: $TT2"
echo "FOCUS-TRANSFERED-BY-APP=OK (minesweeper=true tetris=false)"

echo "=== 9. 정직 회신 — 무매칭·창 부재·id 직접호출 보존 ==="
N1=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"close_window","args":{"app":"nosuchwindow"}}' 2>/dev/null | grep -a '{' | head -1)
echo "close app=nosuchwindow reply: $N1"
ASSERT_FIELD "$N1" '"error":"window_not_found"' "close 무매칭 did not answer window_not_found (거짓 성공 위험): $N1"
N2=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"focus_window","args":{"app":"nosuchwindow"}}' 2>/dev/null | grep -a '{' | head -1)
echo "focus app=nosuchwindow reply: $N2"
ASSERT_FIELD "$N2" '"error":"window_not_found"' "focus 무매칭 did not answer window_not_found: $N2"
# id 직접호출 계약 보존: launch → id로 닫기 → 소멸(diag_capgate 선례 경로)
MS=$(WIN_OF "$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)" "Minesweeper")
[ -n "$MS" ] || FAIL "Minesweeper missing for id-leg (state drift — reply: $MS)"
MS_ID=$(printf '%s' "$MS" | sed -n 's/^{"id":\([0-9]*\),.*$/\1/p')
C3=$(timeout 12 ./buildwsl/jkdesktop agentctl "{\"tool\":\"close_window\",\"args\":{\"id\":$MS_ID}}" 2>/dev/null | grep -a '{' | head -1)
echo "close id=$MS_ID reply: $C3"
ASSERT_FIELD "$C3" '"ok":true' "close_window id 직접호출 did not ok (기존 계약 회귀)"
WAIT_GONE "Minesweeper" || FAIL "Minesweeper survived id close (기존 계약 회귀)"
echo "ID-LEG-PRESERVED=OK"
# 마지막 정직 회신: 창이 남아 있으면 닫고, 전 창 소멸 후 argless=window_not_found
TT_ID=$(printf '%s' "$TT" | sed -n 's/^{"id":\([0-9]*\),.*$/\1/p')
timeout 12 ./buildwsl/jkdesktop agentctl "{\"tool\":\"close_window\",\"args\":{\"id\":$TT_ID}}" >/dev/null 2>&1
WAIT_GONE "Tetris" || FAIL "Tetris survived id close (teardown leg)"
N3=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"close_window","args":{}}' 2>/dev/null | grep -a '{' | head -1)
echo "close argless on empty desktop: $N3"
ASSERT_FIELD "$N3" '"error":"window_not_found"' "argless close on empty desktop did not answer window_not_found (정직 계약): $N3"
echo "HONEST-EMPTY-DESKTOP=OK"

echo "=== 10. teardown (permissions 원복 + bracketed pkill — WSL kill discipline) ==="
if [ "$PERM_STAGED" -eq 1 ]; then
    rm -f "$PERM"
    [ -e "$PERM" ] && FAIL "staged permissions.json removal failed"
    echo "PERM-RESTORE=REMOVED (부재 → 부재 원복)"
else
    mv "$PERM_BAK" "$PERM"
    echo "PERM-RESTORE=RESTORED-FROM-BACKUP"
fi
pkill -f 'buildwsl/[j]kdesktop' 2>/dev/null
sleep 1
pkill -9 -f 'buildwsl/[j]kdesktop' 2>/dev/null
sleep 1
LEFT=$(ps -eo args | grep -i 'buildwsl/[j]kdesktop' | wc -l)
[ "$LEFT" -eq 0 ] || echo "WARN: $LEFT jkdesktop process(es) survived teardown"
echo ""
echo "CHAT-CLOSE-OK (WSL axis — argless·app 지명 close/focus 해소 계약 실측)"
exit 0
# EOF — 끝 개행 유지