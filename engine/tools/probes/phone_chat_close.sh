#!/usr/bin/env bash
# 폰 close/focus 해소 실측 probe (채팅 F1 — plan 2026-10-08-chat-close-fix,
# phone_apps.sh 기계 승계 — 윈도 측 드라이버 + 폰 측 영수증 스크립트 구성).
# 영수증 목표: ① tar-over-ssh **신변 2파일** 재배포(JKWindowServer.cpp — 인자
#   무관 close/focus 해소 확장 — + include/JKWindowServer.h 순수 resolver) —
#   잔여 소스는 argless 폼이 해소 계약 1행이면 산다는 이유로 미배포(정직 노트) ②
#   폰 aarch64 ninja 리빌드(rc 전파 — ~9-10분) ③ AppSelfTest 0 failure(s) —
#   WSL·Windows 캐논과 자기합산(WSL 406·Windows 427 — 1n-s 7행 동증분) ④
#   permissions.json 스테이지: **부재 시에만** {"close_window":"allow"} 생성 후
#   **유지**(폰은 사용자 살아 있는 기기 — Windows build/permissions.json이
#   사용자 승인으로 close_window allow인 것과 같은 포즈로 "닫아줘" 실사용을
#   여는 것. 철수 명령은 영수증에 명문) ⑤ launch minesweeper → close argless →
#   list_windows 소멸 단정(폰 "닫아줘" 본 경로) ⑥ close app 지명 → 소멸 ⑦
#   focus_window app 지명 전이 단정 ⑧ 무매칭·창 부재 = window_not_found 정직 ⑨
#   id 직접호출 보존 ⑩ 종료: 서버 UP + jkweb ALIVE 유지(wake-lock 유지).
#   실행법(윈도 Git Bash, 저장소 루트 어디서든):
#     PHONE_HOST=<폰 IP> bash engine/tools/probes/phone_chat_close.sh
#   접속 정보는 환경변수로(PHONE_HOST 필수 — 내부 IP는 커밋하지 않는다,
#   phone_apps.sh fix r1 M4 정화 승계): PHONE_PORT PHONE_USER PHONE_KEY도 오버라이드 가능.
#   재실행 가능: 재부팅 절사(브래킷 pkill + -9 에스컬레이션 — WSL SIGTERM 흡수
#   원장 승계)로 새 바이너리 서버로 교체, jkweb은 절사 안 한다(선/후 생존 어설션).
set -u
export MSYS_NO_PATHCONV=1  # MSYS가 ssh 인수의 posix 경로를 찢는 것 차단

ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
cd "$ROOT" || exit 1
SCRATCH="$ROOT/engine/tmp"
mkdir -p "$SCRATCH"
TARBALL="$SCRATCH/phone_chat_close.tar"
RLOG="$SCRATCH/phone_chat_close_run.log"
RSRC_TAR_NAME="phone_chat_close_remote.sh"
RSRC_PHONE="~/JKENGINE/$RSRC_TAR_NAME"

PHONE_HOST="${PHONE_HOST:-}"
PHONE_PORT="${PHONE_PORT:-8022}"
PHONE_USER="${PHONE_USER:-u0_a4}"
PHONE_KEY="${PHONE_KEY:-$HOME/.ssh/termux_jkengine}"
# IP 정화(M4 — 내부 IP는 커밋하지 않는다): HOST 기본값 공백+미설정 FAIL.
[ -n "$PHONE_HOST" ] || { echo "CHAT-CLOSE-PHONE-FAIL: PHONE_HOST unset — 폰 IP를 환경변수로 지정하세요 (내부 IP는 커밋하지 않는다: M4)"; exit 1; }
SSH="ssh -p $PHONE_PORT -o BatchMode=yes -o ConnectTimeout=15 -o ServerAliveInterval=15 -o ServerAliveCountMax=10 -i $PHONE_KEY $PHONE_USER@$PHONE_HOST"

FAIL() { echo "CHAT-CLOSE-PHONE-FAIL: $*"; exit 1; }

# ── 배포 원천: 신변 2파일(brief 계약) + probe 본체. 서버 계약 1파일+순수
#    resolver 헤더 1파일이면 폰 축 동작 전부(잔여 소식체 jkweb/jktalk/jkapp_chat는
#    argless 폼이 서버 포커스 해소 계약상 그대로 산다 — 원장에 정직 기록).
#    폰 기존 소스 신선도(phone_apps.sh 배포분)는 그대로 — 이 라인이 건드린
#    파일만 tar에 실는다(git status 신변 원칙).
FILES=(
  engine/src/server/JKWindowServer.cpp    # close_window/focus_window 해소 확장
  engine/include/server/JKWindowServer.h  # AgentWindowRef+ResolveAgentWindowTarget
  engine/tools/probes/phone_chat_close.sh # probe 본체(폰에 원문 유산)
)
for f in "${FILES[@]}"; do
  [ -f "$ROOT/$f" ] || FAIL "deploy source missing: $f"
done

echo "=== 0. pre-clean (재실행 가능성) — wake-lock + 서버 절사(브래킷 + -9, jkweb 절사 금지) ==="
$SSH 'command -v termux-wake-lock >/dev/null 2>&1 && termux-wake-lock || echo WARN-no-wake-lock' \
  || FAIL "ssh failed (phone unreachable?)"
$SSH "pgrep -f '[j]kweb' >/dev/null 2>&1 && echo JKWEB-ALIVE-BEFORE || echo JKWEB-ABSENT-BEFORE" \
  || FAIL "jkweb liveness probe failed"
# 절사 대상=jkdesktop(서버+클라)만 — jkweb은 사용자 살아 있는 세션(원장 계약).
# -9 에스컬레이션 필수: 폰 서버도 SIGTERM 흡수로 pre-clean을 무력화했다
# (wsl 축 동일 함정 — 1차 실측). 조건별 별도 발화(브래킷 트릭 — 자살 방지).
$SSH 'pkill -f "buildterm/[j]kdesktop" 2>/dev/null; sleep 2; pkill -9 -f "buildterm/[j]kdesktop" 2>/dev/null; sleep 1; pgrep -f "buildterm/[j]kdesktop" >/dev/null 2>&1 && { echo PRECLEAN-FAIL; exit 1; } || echo SRV-DOWN' \
  || FAIL "pre-clean could not bring server down"
$SSH "pgrep -f '[j]kweb' >/dev/null 2>&1 && echo JKWEB-ALIVE-AFTER || { echo JKWEB-KILLED-BYPRECLEAN; exit 1; }" \
  || FAIL "pre-clean killed jkweb (사용자 육안 게이트 침해)"

echo "=== 1. tar-over-ssh 재배포 (신변 2파일 + probe 본체) ==="
cat > "$SCRATCH/$RSRC_TAR_NAME" <<'PHONEEOF'
#!/bin/bash
# 폰 측 영수증 절차 (phone_chat_close.sh가 생성 — 채팅 F1). 서버는 **켜 둔 채** 종료.
set -u
cd ~/JKENGINE/engine
FAIL() { echo "CHAT-CLOSE-PHONE-FAIL: $*"; exit 1; }
TMPD="${TMPDIR:-/data/data/com.termux/files/usr/tmp}"

echo "=== A. ninja rebuild (aarch64 — rc 전파, ~9-10분) ==="
ninja -C buildterm -j4 >"$TMPD/chatclose_ninja.log" 2>&1
N_RC=$?
tail -4 "$TMPD/chatclose_ninja.log"
echo "PHONE-BUILD-RC=$N_RC"
[ "$N_RC" -eq 0 ] || FAIL "ninja rebuild rc=$N_RC (aarch64 compile failure)"
[ -x buildterm/jkdesktop ] || FAIL "buildterm/jkdesktop missing after rebuild"

echo "=== B. selftest (계약: AppSelfTest 0 failure(s) — 1n-s 7행 동증분) ==="
timeout 300 ./buildterm/jkdesktop test >"$TMPD/chatclose_st.log" 2>&1
S_RC=$?
ST_PASS=$(grep -ac '^\[PASS\]' "$TMPD/chatclose_st.log")
ST_FAIL=$(grep -ac '^\[FAIL\]' "$TMPD/chatclose_st.log")
echo "PHONE-SELFTEST-PASS=$ST_PASS FAIL=$ST_FAIL (WSL 406·Windows 427 — 1n-s 7행 동증분 자기합산)"
grep -a 'AppSelfTest' "$TMPD/chatclose_st.log" | tail -2
grep -aq 'AppSelfTest: 0 failure(s)' "$TMPD/chatclose_st.log" || FAIL "AppSelfTest not 0 failure(s)"
[ "$S_RC" -eq 0 ] || FAIL "selftest rc=$S_RC"
[ "$ST_FAIL" -eq 0 ] || FAIL "selftest FAIL=$ST_FAIL (폰축 회귀)"
for s in 1 2 3 4 5 6 7; do
    grep -aq "^\[PASS\] 1n-s$s" "$TMPD/chatclose_st.log" || FAIL "selftest 1n-s$s not PASS (폰 1n-s 도표 미실행)"
done

echo "=== C. permissions.json 스테이지 — 부재 시에만 생성 후 **유지** ==="
PERM=buildterm/permissions.json
if [ -f "$PERM" ]; then
    echo "PERM-STAGE=PRIOR-EXISTS (기존 파일 불변 — 철수/덮어쓰기 금지 계약)"
    cat "$PERM"
else
    printf '{\n    "close_window":  "allow"\n}\n' > "$PERM"
    echo "PERM-STAGE=CREATED-AND-KEPT (기존 부재 실측 — 사용자 \"닫아줘\" 실사용 포즈. 철수 원함 시: rm $PERM)"
fi
grep -aq '"close_window"' "$PERM" || FAIL "staged permissions.json malformed"

echo "=== D. boot server (bash ~/tx4_boot.sh — DISPLAY=:1 표준) ==="
bash ~/tx4_boot.sh
sleep 8
P=$(pgrep -f 'buildterm/jkdesktop' | tr '\n' ' ')
[ -n "$P" ] || { echo "--- srvx.log tail:"; tail -20 ~/srvx.log; FAIL "no jkdesktop process 8s after tx4_boot (server died)"; }
echo "server pids: $P"

ASSERT_FIELD() { printf '%s' "$1" | grep -aq "$2" || FAIL "$3 — reply: $1"; }
PING=$(timeout 15 ./buildterm/jkdesktop agentctl '{"tool":"ping","args":{}}' 2>/dev/null | grep -a '{' | head -1)
echo "ping reply: $PING"
[ -n "$PING" ] || FAIL "agentctl ping got no reply (server unresponsive)"
ASSERT_FIELD "$PING" '"ok":true' "ping did not ok"

WAIT_HAS() { # $1=title — 최대 40초 폴링 등장 단정(폰 스폰 18s 원장 흡수)
    for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20; do
        W=$(timeout 15 ./buildterm/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)
        O=$(printf '%s' "$W" | grep -aoE "\{\"id\":[^}]*\"title\":\"$1\"[^}]*\}" | head -1)
        [ -n "$O" ] && { echo "$O"; return 0; }
        sleep 2
    done
    return 1
}
WAIT_GONE() { # $1=title — 최대 40초 폴링 소멸 단정
    for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20; do
        W=$(timeout 15 ./buildterm/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)
        [ -z "$(printf '%s' "$W" | grep -aoE "\{\"id\":[^}]*\"title\":\"$1\"[^}]*\}")" ] && return 0
        sleep 2
    done
    return 1
}

echo "=== E. launch minesweeper → close argless → 소멸 단정 (폰 \"닫아줘\" 본 경로) ==="
L=$(timeout 15 ./buildterm/jkdesktop agentctl '{"tool":"launch_app","args":{"app":"minesweeper"}}' 2>/dev/null | grep -a '{' | head -1)
echo "launch minesweeper: $L"
ASSERT_FIELD "$L" '"ok":true' "launch_app minesweeper did not ok"
MS=$(WAIT_HAS "Minesweeper") || FAIL "Minesweeper window never appeared on phone"
echo "minesweeper window: $MS"
printf '%s' "$MS" | grep -aq '"focused":true' || FAIL "Minesweeper not focused after spawn: $MS"
C1=$(timeout 15 ./buildterm/jkdesktop agentctl '{"tool":"close_window","args":{}}' 2>/dev/null | grep -a '{' | head -1)
echo "close argless reply: $C1"
ASSERT_FIELD "$C1" '"ok":true' "close_window argless did not ok (해소 실패 또는 권한)"
WAIT_GONE "Minesweeper" || FAIL "Minesweeper survived argless close — list_windows: $(timeout 15 ./buildterm/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)"
echo "PHONE-GONE-AFTER-ARGLESS-CLOSE=OK"

echo "=== F. close app 지명 (args.app — 제목 매칭 대소문자 무시) ==="
L=$(timeout 15 ./buildterm/jkdesktop agentctl '{"tool":"launch_app","args":{"app":"minesweeper"}}' 2>/dev/null | grep -a '{' | head -1)
ASSERT_FIELD "$L" '"ok":true' "relaunch minesweeper did not ok"
MS=$(WAIT_HAS "Minesweeper") || FAIL "Minesweeper relaunch never appeared"
C2=$(timeout 15 ./buildterm/jkdesktop agentctl '{"tool":"close_window","args":{"app":"minesweeper"}}' 2>/dev/null | grep -a '{' | head -1)
echo "close app=minesweeper reply: $C2"
ASSERT_FIELD "$C2" '"ok":true' "close_window app 지명 did not ok"
WAIT_GONE "Minesweeper" || FAIL "Minesweeper survived app-named close"
echo "PHONE-GONE-AFTER-APP-NAMED-CLOSE=OK"

echo "=== G. focus_window app 지명 전이 (list_windows focused 단정) ==="
L=$(timeout 15 ./buildterm/jkdesktop agentctl '{"tool":"launch_app","args":{"app":"notes"}}' 2>/dev/null | grep -a '{' | head -1)
ASSERT_FIELD "$L" '"ok":true' "launch_app notes (focus leg) did not ok"
NO=$(WAIT_HAS "Notes") || FAIL "Notes window never appeared (focus leg)"
L=$(timeout 15 ./buildterm/jkdesktop agentctl '{"tool":"launch_app","args":{"app":"minesweeper"}}' 2>/dev/null | grep -a '{' | head -1)
ASSERT_FIELD "$L" '"ok":true' "relaunch minesweeper (focus leg) did not ok"
MS=$(WAIT_HAS "Minesweeper") || FAIL "Minesweeper (focus leg) never appeared"
G1=$(timeout 15 ./buildterm/jkdesktop agentctl '{"tool":"focus_window","args":{"app":"notes"}}' 2>/dev/null | grep -a '{' | head -1)
echo "focus app=notes reply: $G1"
ASSERT_FIELD "$G1" '"ok":true' "focus_window app 지명 did not ok"
sleep 2
W=$(timeout 15 ./buildterm/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)
NO2=$(printf '%s' "$W" | grep -aoE '\{"id":[^}]*"title":"Notes"[^}]*\}' | head -1)
MS2=$(printf '%s' "$W" | grep -aoE '\{"id":[^}]*"title":"Minesweeper"[^}]*\}' | head -1)
printf '%s' "$NO2" | grep -aq '"focused":true' || FAIL "Notes not focused after app-named focus: $NO2"
printf '%s' "$MS2" | grep -aq '"focused":false' || FAIL "Minesweeper wrongly still focused: $MS2"
echo "PHONE-FOCUS-TRANSFERED-BY-APP=OK (notes=true minesweeper=false)"

echo "=== H. 정직 회신 + id 직접호출 보존 ==="
N1=$(timeout 15 ./buildterm/jkdesktop agentctl '{"tool":"close_window","args":{"app":"nosuchwindow"}}' 2>/dev/null | grep -a '{' | head -1)
echo "close app=nosuchwindow reply: $N1"
ASSERT_FIELD "$N1" '"error":"window_not_found"' "close 무매칭 did not answer window_not_found: $N1"
MS_ID=$(printf '%s' "$MS" | sed -n 's/^{"id":\([0-9]*\),.*$/\1/p')
NO_ID=$(printf '%s' "$NO" | sed -n 's/^{"id":\([0-9]*\),.*$/\1/p')
C3=$(timeout 15 ./buildterm/jkdesktop agentctl "{\"tool\":\"close_window\",\"args\":{\"id\":$MS_ID}}" 2>/dev/null | grep -a '{' | head -1)
echo "close id=$MS_ID reply: $C3"
ASSERT_FIELD "$C3" '"ok":true' "close_window id 직접호출 did not ok (기존 계약 회귀)"
WAIT_GONE "Minesweeper" || FAIL "Minesweeper survived id close"
C4=$(timeout 15 ./buildterm/jkdesktop agentctl "{\"tool\":\"focus_window\",\"args\":{\"id\":$NO_ID}}" 2>/dev/null | grep -a '{' | head -1)
echo "focus id=$NO_ID reply: $C4"
ASSERT_FIELD "$C4" '"ok":true' "focus_window id 직접호출 did not ok"
echo "PHONE-ID-LEGS-PRESERVED=OK"
# 남은 창 정리 — 라인 착지 시 폰 데스크톱은 라인 이전과 같은 정리 상태(id leg 재실측 겸용)
C5=$(timeout 15 ./buildterm/jkdesktop agentctl "{\"tool\":\"close_window\",\"args\":{\"id\":$NO_ID}}" 2>/dev/null | grep -a '{' | head -1)
echo "teardown close notes id=$NO_ID reply: $C5"
WAIT_GONE "Notes" || FAIL "Notes survived id close (teardown leg)"

echo "=== I. 종료 게이트 — 서버 UP + jkweb ALIVE (사용자 육안 세션 유지) ==="
# 서버는 살려 둔다(끄지 않는다) — 폰 사용자의 "닫아줘" 재검증 게이트가 이 서버를 쓴다.
FINAL_P=$(pgrep -f 'buildterm/jkdesktop' | tr '\n' ' ')
[ -n "$FINAL_P" ] || FAIL "server not UP after probe (종료 게이트 위반)"
echo "FINAL-SERVER-PIDS=$FINAL_P"
pgrep -f '[j]kweb' >/dev/null 2>&1 || FAIL "jkweb not ALIVE after probe (종료 게이트 위반)"
echo "JKWEB-ALIVE-AFTER=OK"
rm -f ~/JKENGINE/phone_chat_close_remote.sh 2>/dev/null
echo "CHAT-CLOSE-PHONE-OK"
exit 0
PHONEEOF

for r in 1 2 3; do
  if tar -cf "$TARBALL" -C "$ROOT" "${FILES[@]}" -C "$SCRATCH" "$RSRC_TAR_NAME"; then
    break
  elif [ "$r" -eq 3 ]; then
    FAIL "tar creation failed"
  fi
done
TAR_SZ=$(wc -c < "$TARBALL" | tr -d ' ')
echo "tar size: $TAR_SZ bytes"
[ "$TAR_SZ" -gt 5000 ] || FAIL "tar suspiciously small ($TAR_SZ bytes) — deploy list broken?"
$SSH "tar -xf - -C ~/JKENGINE" < "$TARBALL" || FAIL "remote tar extract failed"
rm -f "$TARBALL"
$SSH 'test -f ~/JKENGINE/engine/src/server/JKWindowServer.cpp \
      && test -f ~/JKENGINE/engine/include/server/JKWindowServer.h \
      && test -f ~/JKENGINE/phone_chat_close_remote.sh \
      && echo DEPLOY-FILES-OK' \
  || FAIL "deployed files missing on phone"

echo "=== 2. 폰 리빌드+영수증 절차 실행 (ninja ~9-10분 + close 실측 전체) ==="
$SSH "bash $RSRC_PHONE" > "$RLOG" 2>&1
R_RC=$?
cat "$RLOG"
[ "$R_RC" -eq 0 ] || FAIL "phone-side receipt script rc=$R_RC (see above)"
grep -aq '^CHAT-CLOSE-PHONE-OK$' "$RLOG" || FAIL "receipt script did not emit CHAT-CLOSE-PHONE-OK"
if grep -aq 'CHAT-CLOSE-PHONE-FAIL' "$RLOG"; then FAIL "receipt script emitted CHAT-CLOSE-PHONE-FAIL"; fi

echo ""
echo "CHAT-CLOSE-PHONE-OK (driver axis)"
echo "서버 UP+jkweb ALIVE — 폰 \"닫아줘\" 재검증 게이트: 브라우저 채팅에서 닫아줘 → 지뢰찾기 창 소멸."
exit 0
# EOF — 끝 개행 유지(스크립트 산술 무해 습관).