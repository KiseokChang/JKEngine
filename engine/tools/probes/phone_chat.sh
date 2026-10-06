#!/usr/bin/env bash
# 폰 채팅 실측 probe (스펙 2026-10-07-desktop-chat-app Task 5 — §1.4,
# phone_library.sh 원준 재용, WSL 판 wsl_chat_boot.sh의 어설션 계약 승계).
# 영수증 목표: ① tar-over-ssh 재배포(기존 7종 + 신규 chat 5종=헤더 2·TU 3) ②
# 폰 aarch64 ninja 리빌드+jkapp_chat.so 성립 ③ AppSelfTest 0 failure(s)(케이스
# 1n 라우터 포함) ④ library-list에 name=chat source=builtin 단일 진실원 행 ⑤
# ~/tx4_boot.sh 서버 부팅 ⑥ agentctl ping ⑦ launch_app {"app":"chat"} ok:true
# ⑧ list_windows "title":"Chat" 720x540 focused ⑨ srvx.log "created (720x540)"
# → 서버·Chat 창을 **켜 둔 채 종료**(IME 육안 게이트 — 사용자 등장).
#   IME 게이트: Android 소프트 키보드 입력이 ImGui 입력 상자에 실도달하는지는
#   어느 쪽 프로브도 자동 단정 불가 — 반드시 사용자가 Chat 창에 탭·타자하여
#   확인한다(스펙 §1.4 게이트 조항). probe 산출물의 확인 조건은 최종 echo에 명시.
#   실행법(윈도 Git Bash, 저장소 루트 어디서든):
#     bash engine/tools/probes/phone_chat.sh
#   재실행 가능: 서버가 살아 있으면 선 절사(pkill -f 'buildterm/[j]kdesktop') 후
#   재부팅. 영수증 하나라도 빠지면 CHAT-PHONE-FAIL로 비정상 종료(rc=1).
#   함정 원장(phone_library.sh 헤더 그대로): tar는 저장소 루트에서 만들어
#   `-C ~/JKENGINE`으로 풀어야 한다(engine/engine 중첩 트랩). agentctl 와이어는
#   서브커맨드 agentctl·키 tool/args('agent'로 치면 demo 앱 CPU 루프). 폰 /tmp는
#   쓰기 불가 — 로그는 ~/tmp/로. toybox pgrep -x는 거짓음성 — -f 브래킷 계열.
#   tar 전송은 LAN 내부 ssh 한정 — 어떤 클라우드/외부로도 가지 않는다.
set -u
export MSYS_NO_PATHCONV=1  # MSYS가 ssh 인수의 posix 경로를 찢는 것 차단(wsl probe 선례)

ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
cd "$ROOT" || exit 1
SCRATCH="$ROOT/engine/tmp"
mkdir -p "$SCRATCH"
TARBALL="$SCRATCH/phone_chat.tar"
RLOG="$SCRATCH/phone_chat_run.log"
RSRC_TAR_NAME="phone_chat_remote.sh"
RSRC_PHONE="~/JKENGINE/$RSRC_TAR_NAME"   # 폰 측 삭제는 remote script H단계(임시 파일만)

SSH="ssh -p 8022 -o BatchMode=yes -o ConnectTimeout=15 -i $HOME/.ssh/termux_jkengine u0_a4@192.168.219.109"

FAIL() { echo "CHAT-PHONE-FAIL: $*"; exit 1; }

# ── 배포 원천: phone_library.sh 7종 + Task 1-2 신규 chat 서브셋(chat 5종) ──
# (라이브러리 라인 파일은 폰 트리에 이미 있지만 전량 재밀어 원준 수렴 —
#  폰 리빌드는 전체 대상 세트 ~9-10분이라 서브셋 스킵의 이득이 사실상 없다.)
FILES=(
  engine/include/JKLibraryCatalog.h
  engine/src/JKLibraryCatalog.cpp
  engine/include/apps/ClientLibraryApp.h
  engine/src/apps/ClientLibraryApp.cpp
  engine/src/apps/JKAppModule_library.cpp
  engine/src/main.cpp
  engine/CMakeLists.txt
  engine/include/apps/ChatRouter.h
  engine/include/apps/ClientChatApp.h
  engine/src/apps/ChatRouter.cpp
  engine/src/apps/ClientChatApp.cpp
  engine/src/apps/JKAppModule_chat.cpp
)

for f in "${FILES[@]}"; do
  [ -f "$ROOT/$f" ] || FAIL "deploy source missing: $f"
done

echo "=== 0. pre-flight + pre-clean (재실행 가능성) — ssh 생존 + wake-lock + 서버 절사 ==="
$SSH 'echo PHONE-REACHABLE; uname -m' || FAIL "ssh failed (phone unreachable) — 접속 증거: ssh -p 8022 -i ~/.ssh/termux_jkengine u0_a4@192.168.219.109"
$SSH 'command -v termux-wake-lock >/dev/null 2>&1 && termux-wake-lock || echo WARN-no-wake-lock' \
  || FAIL "ssh failed (phone unreachable?)"
# 폰 pgrep 함정(실측): toybox pgrep -x는 argv[0] 기준이라 전면 놓친다 — -f 브래킷 계열.
$SSH "pkill -f 'buildterm/[j]kdesktop' 2>/dev/null; sleep 2; pkill -9 -f 'buildterm/[j]kdesktop' 2>/dev/null; sleep 1; pgrep -f 'buildterm/[j]kdesktop' >/dev/null 2>&1 && { echo PRECLEAN-FAIL; exit 1; } || echo SRV-DOWN" \
  || FAIL "pre-clean could not bring server down"

echo "=== 1. tar-over-ssh 재배포 (12종 + 폰 측 영수증 스크립트) ==="
cat > "$SCRATCH/$RSRC_TAR_NAME" <<'PHONEEOF'
#!/bin/bash
# 폰 측 영수증 절차 (phone_chat.sh가 생성 — Task 5). 서버·Chat 창은 끄지 않는다.
set -u
cd ~/JKENGINE/engine
FAIL() { echo "CHAT-PHONE-FAIL: $*"; exit 1; }

echo "=== A. selftest (계약: AppSelfTest 0 failure(s) — 케이스 1n 라우터 포함) ==="
timeout 240 ./buildterm/jkdesktop test >~/tmp/ph_chat_selftest.log 2>&1
T_RC=$?
echo "selftest rc=$T_RC"
grep -a 'AppSelfTest' ~/tmp/ph_chat_selftest.log | tail -2
grep -aq 'AppSelfTest: 0 failure(s)' ~/tmp/ph_chat_selftest.log \
  || FAIL "AppSelfTest not 0 failure(s) (rc=$T_RC) — aarch64 계약 붕괴"

echo "=== B. library-list (chat builtin 단일 진실원 + 기존 2행 회귀) ==="
timeout 30 ./buildterm/jkdesktop library-list >~/tmp/ph_chat_lib_list.log 2>&1
L_RC=$?
echo "library-list rc=$L_RC"
cat ~/tmp/ph_chat_lib_list.log
[ "$L_RC" -eq 0 ] || FAIL "library-list rc=$L_RC"
grep -aE '^count=[0-9]+ base=' ~/tmp/ph_chat_lib_list.log | head -1 | grep -aq . \
  || FAIL "library-list no well-formed count= tail line"
CHATLINE=$(grep -a '^name=chat ' ~/tmp/ph_chat_lib_list.log | head -1)
[ -n "$CHATLINE" ] || FAIL "library-list missing chat entry (T2 builtin 등록이 aarch64에서 유실?)"
printf '%s\n' "$CHATLINE" | grep -aq 'name=chat title=Chat source=builtin' \
  || FAIL "chat entry not builtin 단일 진실원 — $CHATLINE"
echo "chat builtin line: $CHATLINE"
PP=$(grep -a '^name=phoneprobe ' ~/tmp/ph_chat_lib_list.log | head -1)
[ -n "$PP" ] || FAIL "library-list missing phoneprobe entry (.jkx scan — 회귀)"
printf '%s\n' "$PP" | grep -aq 'source=jkx' || FAIL "phoneprobe entry not source=jkx — $PP"
MS=$(grep -a '^name=minesweeper ' ~/tmp/ph_chat_lib_list.log | head -1)
[ -n "$MS" ] || FAIL "library-list missing minesweeper entry (builtin 회귀)"
printf '%s\n' "$MS" | grep -aq 'source=builtin' || FAIL "minesweeper entry not source=builtin — $MS"
TT=$(grep -a '^name=tetris ' ~/tmp/ph_chat_lib_list.log | head -1)
[ -n "$TT" ] || FAIL "library-list missing tetris entry (builtin 회귀)"
printf '%s\n' "$TT" | grep -aq 'source=builtin' || FAIL "tetris entry not source=builtin — $TT"

echo "=== C. boot server (bash ~/tx4_boot.sh — DISPLAY=:1 표준) ==="
bash ~/tx4_boot.sh
sleep 6
# pgrep -x 함정 회피(폰 실측) — 이 파일 안 실행이라 -f 자기매칭 무해.
P=$(pgrep -f 'buildterm/jkdesktop' | tr '\n' ' ')
if [ -z "$P" ]; then
  echo "--- srvx.log tail:"; tail -20 ~/srvx.log
  FAIL "no jkdesktop process 6s after tx4_boot (server died)"
fi
echo "server pids: $P"

echo "=== D. agentctl ping ==="
PING=$(timeout 12 ./buildterm/jkdesktop agentctl '{"tool":"ping","args":{}}' 2>/dev/null | grep -a '{' | head -1)
echo "ping reply: $PING"
[ -n "$PING" ] || FAIL "agentctl ping got no reply JSON (server unresponsive)"
printf '%s' "$PING" | grep -aq '"ok":true' || FAIL "ping did not ok — $PING"

echo "=== E. launch_app {app:chat} ==="
LAUNCH=$(timeout 15 ./buildterm/jkdesktop agentctl '{"tool":"launch_app","args":{"app":"chat"}}' 2>/dev/null | grep -a '{' | head -1)
echo "launch reply: $LAUNCH"
[ -n "$LAUNCH" ] || FAIL "launch_app got no reply (server gone?)"
printf '%s' "$LAUNCH" | grep -aq '"ok":true' || FAIL "launch_app did not ok — $LAUNCH"

echo "=== F. list_windows (영수증: title=Chat + 기하 720x540 + focused) ==="
sleep 15  # 폰은 클라 스폰(.so 로드+폰트 아틀라스)이 WSL보다 느리다
WIN=$(timeout 12 ./buildterm/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)
echo "list_windows: $WIN"
[ -n "$WIN" ] || FAIL "list_windows got no reply"
# Chat 창 오브젝트 절단 후 id·기하·focus까지 잠근다(항목 오브젝트엔 중괄호가
# 없어 [^}]* 절단 안전 — phone_library.sh final review Item 4 동형).
CHATW=$(printf '%s' "$WIN" | grep -aoE '\{"id":[^}]*"title":"Chat"[^}]*\}' | head -1)
[ -n "$CHATW" ] || FAIL "list_windows has no Chat window object (launch_app chat lost on aarch64?) — reply: $WIN"
printf '%s' "$CHATW" | grep -aqE '"id":[0-9]+' || FAIL "Chat window object missing id — $CHATW"
printf '%s' "$CHATW" | grep -aq '"w":720,"h":540' || FAIL "Chat window geometry not 720x540 — $CHATW"
printf '%s' "$CHATW" | grep -aq '"focused":true' || FAIL "Chat window not focused — $CHATW"

echo "=== G. 서버 로그 영수증 (srvx.log) ==="
grep -a 'created (720x540)' ~/srvx.log | tail -2
grep -aq 'created (720x540)' ~/srvx.log || FAIL "srvx.log missing 'created (720x540)'"

echo "=== H. 서버와 Chat 창을 켜 둔 채 종료 (IME 육안 게이트 — 사용자 등장) ==="
# teardown은 probe 임시 파일만 — 서버는 끄지 않는다. 소프트 키보드 입력이 Chat
# 입력 상자에 실도달하는지는 자동 어설션 불가 — 반드시 사람이 Taps a 타자로 확인:
#  1) Chat 창 입력 상자 탭 → Android 소프트 키보드 떠오름 확인
#  2) 한영 아무 글자나 타자 → ImGui 입력 상자에 문자 실도달 확인
#  3) 한글 IME 조합도 통과하면 완료(스펙 §1.4 게이트)
rm -f ~/JKENGINE/phone_chat_remote.sh 2>/dev/null
echo "CHAT-PHONE-OK"
exit 0
PHONEEOF

for r in 1 2 3; do
  if tar -cf "$TARBALL" -C "$ROOT" "${FILES[@]}" -C "$SCRATCH" "$RSRC_TAR_NAME"; then
    break
  elif [ "$r" -eq 3 ]; then
    FAIL "tar creation failed"
  fi
done
echo "deploy list:"; tar -tf "$TARBALL"
TAR_SZ=$(wc -c < "$TARBALL" | tr -d ' ')
echo "tar size: $TAR_SZ bytes"
[ "$TAR_SZ" -gt 100000 ] || FAIL "tar suspiciously small ($TAR_SZ bytes) — deploy list broken?"
$SSH "tar -xf - -C ~/JKENGINE" < "$TARBALL" || FAIL "remote tar extract failed"
rm -f "$TARBALL"
$SSH 'stat -c "%n %s" ~/JKENGINE/engine/src/apps/JKAppModule_chat.cpp \
                      ~/JKENGINE/engine/src/apps/ClientChatApp.cpp \
                      ~/JKENGINE/engine/src/apps/ChatRouter.cpp \
                      ~/JKENGINE/engine/include/apps/ClientChatApp.h \
                      ~/JKENGINE/engine/include/apps/ChatRouter.h \
                      ~/JKENGINE/engine/src/apps/JKAppModule_library.cpp \
                      ~/JKENGINE/engine/src/JKLibraryCatalog.cpp \
                      ~/JKENGINE/engine/src/apps/ClientLibraryApp.cpp \
                      ~/JKENGINE/engine/include/JKLibraryCatalog.h \
                      ~/JKENGINE/engine/include/apps/ClientLibraryApp.h \
                      ~/JKENGINE/engine/src/main.cpp \
                      ~/JKENGINE/engine/CMakeLists.txt \
                      ~/JKENGINE/phone_chat_remote.sh' \
  || FAIL "deployed files missing on phone"

echo "=== 2. 폰 리빌드 (ninja -C buildterm -j4, aarch64 — ~9-10분 정상) ==="
# rc 봉합(phone_library.sh 원준): 원격 복합문이 echo로 끝나면 종료코드가 항상
# 0이라 드라이버 ||FAIL이 죽은 코드가 된다 — PIPESTATUS를 rc로 삼아 exit로 전파.
# NOUT을 FAIL 앞에 인쇄 — ninja 마지막 행들이 실패 경로에도 도달한다.
N_RC=0
BUILD_BEGIN=$SECONDS
NOUT=$($SSH 'cd ~/JKENGINE/engine && ninja -C buildterm -j4 2>&1 | tail -12; rc=${PIPESTATUS[0]}; echo NINJA-RC=$rc; exit $rc') || N_RC=$?
BUILD_ELAPSED=$((SECONDS - BUILD_BEGIN))
echo "$NOUT"
echo "rebuild rc=$N_RC duration=${BUILD_ELAPSED}s"
[ "$N_RC" -eq 0 ] || FAIL "ninja rebuild rc=$N_RC (aarch64 compile failure — NINJA tail above)"
$SSH 'cd ~/JKENGINE/engine; test -x buildterm/jkdesktop && test -e buildterm/jkapp_chat.so \
      && ls -l buildterm/jkapp_chat.so buildterm/jkapp_library.so \
      || { ls buildterm/jkapp* 2>&1; exit 1; }' \
  || FAIL "buildterm/jkapp_chat.so did not come into existence (deploy did not register the module target?)"
# 존재 검증이 launch_app 게이트(JKWindowServer 존재 검증)와 같은 파일을 본다 — 위 통과면 E도 도달 가능.

echo "=== 3. 폰 영수증 절차 실행 (selftest → library-list → boot → launch → receipts) ==="
$SSH "bash $RSRC_PHONE" > "$RLOG" 2>&1
R_RC=$?
cat "$RLOG"
[ "$R_RC" -eq 0 ] || FAIL "phone-side receipt script rc=$R_RC (see above)"
grep -aq '^CHAT-PHONE-OK$' "$RLOG" || FAIL "receipt script did not emit CHAT-PHONE-OK"
if grep -aq 'CHAT-PHONE-FAIL' "$RLOG"; then FAIL "receipt script emitted CHAT-PHONE-FAIL"; fi

echo ""
echo "CHAT-PHONE-OK"
echo "서버는 살아 있고 Chat 창(720x540)이 떠 있다 — **IME 육안 게이트 대기**."
echo "  확인법: 폰에서 Chat 창의 입력 상자를 탭 → Android 소프트 키보드에서 타자 →"
echo "  문자가 ImGui 입력 상자에 실도달하는지 확인(한글 조합도 포함). 서버를 끄지 마세요."
exit 0
# EOF — 끝 개행 유지(스크립트 산술 무해 습관).
