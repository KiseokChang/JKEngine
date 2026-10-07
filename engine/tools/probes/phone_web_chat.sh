#!/usr/bin/env bash
# 폰 웹 채팅 실측 probe (스펙 2026-10-07-desktop-chat-app §5.1 Task 7 —
# phone_chat.sh 원준 재용, 그 fix r1의 tetris 회귀 어설션 계약 승계).
# 영수증 목표: ① tar-over-ssh 재배포(jkweb 신규 1종 + CMakeLists + 라우터 2종
# 원준 수렴) ② 폰 aarch64 ninja 리빌드+buildterm/jkweb 성립 ③ AppSelfTest 0
# failure(s) ④ ~/tx4_boot.sh 서버 부팅 ⑤ buildterm/jkweb 기동(nohup — ssh
# 단절에도 생존, 종료 상태까지 유지) ⑥ 폰 측 curl GET / = 채팅 페이지 200
# ⑦ 폰 측 curl POST /talk {"text":"지뢰찾기 켜줘"} = 200 kind=Launch
# app=minesweeper(jkweb→창 서버 위임 실측 — jkdesktop이 실제로 반응) ⑧
# list_windows에 title=Minesweeper 창 생성 단정 ⑨ tetris 회귀(테트리스 켜줘 →
# app=tetris + Tetris 창) ⑩ srvx.log created 증가 ⑪ 종료 상태=서버 UP+jkweb
# 유지(**사용자 육안 게이트: 폰 브라우저로 실제 타작 원단 확인**).
#   육안 게이트: 브라우저 입력 박스에 소프트 키보드 타자·fetch 회신 표시·DeX
#   화면 반응은 어느 자동 어설션도 대신 몫 못한다 — 반드시 사용자가
#   http://localhost:8090/ (또는 http://<폰 IP>:8090/)를 열어 확인한다(스펙
#   §5.1 게이트 조항). 최종 echo에 확인 조건 명시.
#   실행법(윈도 Git Bash, 저장소 루트 어디서든):
#     bash engine/tools/probes/phone_web_chat.sh
#   재실행 가능: 서버·jkweb이 살아 있으면 선 절사(pkill -f 브래킷 — 폰 toybox
#   pgrep -x 거짓음성 레슨) 후 재기동. 영수증 하나라도 빠지면
#   WEBCHAT-PHONE-FAIL로 비정상 종료(rc=1).
#   함정 원장(phone_chat.sh 헤더 그대로): tar는 저장소 루트에서 만들어
#   `-C ~/JKENGINE`으로 풀어야 한다(engine/engine 중첩 트랩). agentctl 와이어는
#   서브커맨드 agentctl·키 tool/args. 폰 /tmp는 쓰기 불가 — 로그는 ~/tmp/로.
#   rc 봉합: 원격 복합문이 echo로 끝나면 종료코드가 항상 0 — PIPESTATUS를 rc로
#   삼아 exit로 전파, `|| true` 금지. tar 전송은 LAN 내부 ssh 한정 — 어떤
#   클라우드/외부로도 가지 않는다.
set -u
export MSYS_NO_PATHCONV=1  # MSYS가 ssh 인수의 posix 경로를 찢는 것 차단(wsl probe 선례)

ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
cd "$ROOT" || exit 1
SCRATCH="$ROOT/engine/tmp"
mkdir -p "$SCRATCH"
TARBALL="$SCRATCH/phone_web_chat.tar"
RLOG="$SCRATCH/phone_web_chat_run.log"
RSRC_TAR_NAME="phone_web_chat_remote.sh"
RSRC_PHONE="~/JKENGINE/$RSRC_TAR_NAME"   # 폰 측 삭제는 remote script I단계(임시 파일만)

SSH="ssh -p 8022 -o BatchMode=yes -o ConnectTimeout=15 -i $HOME/.ssh/termux_jkengine u0_a4@192.168.219.109"

FAIL() { echo "WEBCHAT-PHONE-FAIL: $*"; exit 1; }

# ── 배포 원천: Task 7 신규 jkweb 1종 + CMakeLists + 소비 라우터 2종(원준 수렴)
#    + jktalk 1종 — CMakeLists에는 jktalk 타깃(T6)이 이미 있어 폰 트리에
#    tools/jktalk/main.cpp가 없으면 "No SOURCES given to target: jktalk"로
#    cmake 재생성이 죽는다(실측 2026-10-07 — 폰에 T6 소스 미배포 상태였다).
FILES=(
  engine/tools/jkweb/main.cpp
  engine/tools/jktalk/main.cpp
  engine/CMakeLists.txt
  engine/include/apps/ChatRouter.h
  engine/src/apps/ChatRouter.cpp
)

for f in "${FILES[@]}"; do
  [ -f "$ROOT/$f" ] || FAIL "deploy source missing: $f"
done

echo "=== 0. pre-flight + pre-clean (재실행 가능성) — ssh 생존 + wake-lock + 서버·jkweb 절사 ==="
$SSH 'echo PHONE-REACHABLE; uname -m' || FAIL "ssh failed (phone unreachable) — 접속 증거: ssh -p 8022 -i ~/.ssh/termux_jkengine u0_a4@192.168.219.109"
$SSH 'command -v termux-wake-lock >/dev/null 2>&1 && termux-wake-lock || echo WARN-no-wake-lock' \
  || FAIL "ssh failed (phone unreachable?)"
# 폰 pgrep 함정(실측) — -f 브래킷 계열. 서버와 이전 probe 잔존 jkweb 모두 절사.
$SSH "pkill -f 'buildterm/[j]kdesktop' 2>/dev/null; pkill -f 'buildterm/[j]kweb' 2>/dev/null; sleep 2; pkill -9 -f 'buildterm/[j]kdesktop' 2>/dev/null; pkill -9 -f 'buildterm/[j]kweb' 2>/dev/null; sleep 1; pgrep -f 'buildterm/[j]kdesktop' >/dev/null 2>&1 && { echo PRECLEAN-FAIL; exit 1; }; pgrep -f 'buildterm/[j]kweb' >/dev/null 2>&1 && { echo PRECLEAN-FAIL-WEB; exit 1; }; echo SRV-DOWN-WEB-DOWN" \
  || FAIL "pre-clean could not bring server/jkweb down"

echo "=== 1. tar-over-ssh 재배포 (4종 + 폰 측 영수증 스크립트) ==="
cat > "$SCRATCH/$RSRC_TAR_NAME" <<'PHONEEOF'
#!/bin/bash
# 폰 측 영수증 절차 (phone_web_chat.sh가 생성 — Task 7). 서버·jkweb은 끄지 않는다.
set -u
cd ~/JKENGINE/engine
FAIL() { echo "WEBCHAT-PHONE-FAIL: $*"; exit 1; }
command -v curl >/dev/null 2>&1 || FAIL "폰에 curl 없음 — POST /talk 단정 불가"

echo "=== A. selftest (계약: AppSelfTest 0 failure(s)) ==="
timeout 240 ./buildterm/jkdesktop test >~/tmp/ph_web_selftest.log 2>&1
T_RC=$?
echo "selftest rc=$T_RC"
grep -a 'AppSelfTest' ~/tmp/ph_web_selftest.log | tail -2
grep -aq 'AppSelfTest: 0 failure(s)' ~/tmp/ph_web_selftest.log \
  || FAIL "AppSelfTest not 0 failure(s) (rc=$T_RC) — aarch64 계약 붕괴"

echo "=== B. boot server (bash ~/tx4_boot.sh — DISPLAY=:1 표준) ==="
bash ~/tx4_boot.sh
sleep 6
# pgrep -x 함정 회피(폰 실측) — 이 파일 안 실행이라 -f 자기매칭 무해.
P=$(pgrep -f 'buildterm/jkdesktop' | tr '\n' ' ')
if [ -z "$P" ]; then
  echo "--- srvx.log tail:"; tail -20 ~/srvx.log
  FAIL "no jkdesktop process 6s after tx4_boot (server died)"
fi
echo "server pids: $P"
PING=$(timeout 12 ./buildterm/jkdesktop agentctl '{"tool":"ping","args":{}}' 2>/dev/null | grep -a '{' | head -1)
echo "ping reply: $PING"
printf '%s' "$PING" | grep -aq '"ok":true' || FAIL "agentctl ping did not ok — $PING"
# created 기준선은 boot **후**에 잡는다 — 실측 2026-10-07: tx4_boot가 srvx.log를
# 돌리는(재작성) 관계로 boot 전 기준선은 부팅 후 카운트와 우연히 같아짐(3→3 유령).
CR=$(grep -ac 'created (' ~/srvx.log 2>/dev/null) || CR=0
echo "srvx.log created baseline count (after boot): $CR"

echo "=== C. start jkweb (localhost 8090 — nohup: ssh 단절에도 생존, 종료 상태) ==="
nohup ./buildterm/jkweb --port 8090 >~/tmp/ph_web_jkweb.log 2>&1 &
sleep 3
JW=$(pgrep -f 'buildterm/[j]kweb' | tr '\n' ' ')
[ -n "$JW" ] || { echo "--- jkweb log:"; cat ~/tmp/ph_web_jkweb.log; FAIL "jkweb not alive 3s after start"; }
echo "jkweb pids: $JW"
cat ~/tmp/ph_web_jkweb.log

echo "=== D. 폰 측 GET / (채팅 페이지 200 + 한국어 마커) ==="
GET=$(curl -s -w "\nHTTP=%{http_code}" http://localhost:8090/)
echo "$GET" | head -5
printf '%s' "$GET" | grep -aq 'HTTP=200' || FAIL "GET / not 200 — jkweb dead? log above"
printf '%s' "$GET" | grep -aq 'JK 채팅' || FAIL "GET / missing Korean page marker 'JK 채팅' — page broken"
printf '%s' "$GET" | grep -aq 'POST' || FAIL "GET / page has no /talk POST plumbing — page broken"

echo "=== E. 폰 측 POST /talk {텍스트:지뢰찾기 켜줘} (jkweb→창 서버 위임 실측) ==="
TALK=$(curl -s -H 'Content-Type: application/json' \
  --data '{"text":"지뢰찾기 켜줘"}' -w "\nHTTP=%{http_code}" http://localhost:8090/talk)
echo "$TALK"
printf '%s' "$TALK" | grep -aq 'HTTP=200' || FAIL "POST /talk not 200 (정직 500 = 서버 위임 실패) — $TALK"
printf '%s' "$TALK" | grep -aq '"kind":"Launch"' || FAIL "reply kind != Launch — $TALK"
printf '%s' "$TALK" | grep -aq '"app":"minesweeper"' || FAIL "reply app != minesweeper — $TALK"

echo "=== F. list_windows — Minesweeper 창 생성 단정 ==="
sleep 15  # 폰은 클라 스폰(.so 로드+폰트 아틀라스)이 WSL보다 느리다
WIN=$(timeout 12 ./buildterm/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)
echo "list_windows: $WIN"
[ -n "$WIN" ] || FAIL "list_windows got no reply"
# 창 오브젝트 절단 후 id까지 잠근다(phone_chat final review Item 4 동형).
MSW=$(printf '%s' "$WIN" | grep -aoE '\{"id":[^}]*"title":"Minesweeper"[^}]*\}' | head -1)
[ -n "$MSW" ] || FAIL "list_windows has no Minesweeper window (jkweb launch_app lost on aarch64?) — reply: $WIN"
printf '%s' "$MSW" | grep -aqE '"id":[0-9]+' || FAIL "Minesweeper window object missing id — $MSW"
echo "Minesweeper window object: $MSW"

echo "=== G. tetris 회귀 어설션 (phone_chat fix r1 동형 — 동일 라인 두 번째 앱어) ==="
TALK2=$(curl -s -H 'Content-Type: application/json' \
  --data '{"text":"테트리스 켜줘"}' -w "\nHTTP=%{http_code}" http://localhost:8090/talk)
echo "$TALK2"
printf '%s' "$TALK2" | grep -aq 'HTTP=200' || FAIL "tetris POST /talk not 200 — $TALK2"
printf '%s' "$TALK2" | grep -aq '"kind":"Launch"' || FAIL "tetris reply kind != Launch — $TALK2"
printf '%s' "$TALK2" | grep -aq '"app":"tetris"' || FAIL "tetris reply app != tetris — $TALK2"
sleep 15
WIN2=$(timeout 12 ./buildterm/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)
echo "list_windows: $WIN2"
TTW=$(printf '%s' "$WIN2" | grep -aoE '\{"id":[^}]*"title":"Tetris"[^}]*\}' | head -1)
[ -n "$TTW" ] || FAIL "list_windows has no Tetris window (tetris regression) — reply: $WIN2"
printf '%s' "$TTW" | grep -aqE '"id":[0-9]+' || FAIL "Tetris window object missing id — $TTW"
echo "Tetris window object: $TTW"

echo "=== H. srvx.log created 영수증 (기준선 대비 증가) ==="
grep -a 'created (' ~/srvx.log | tail -3
CR2=$(grep -ac 'created (' ~/srvx.log)
[ "$CR2" -gt "$CR" ] || FAIL "srvx.log created count did not rise ($CR → $CR2)"
echo "created count: $CR → $CR2"

echo "=== I. 종료 상태 — 서버 UP + jkweb 유지 (육안 게이트 — 사용자 등장) ==="
# teardown은 probe 임시 파일만 — 서버·jkweb은 끄지 않는다. 브라우저 타작은
# 자동 어설션 불가 — 반드시 사람이 확인:
#  1) 폰(또는 PC) 브라우저에서 http://localhost:8090/ (폰 외 기기는
#     http://<폰 IP>:8090/) 열기 → JK 채팅 페이지 표시 확인
#  2) 입력 박스에 "지뢰찾기 켜줘" 타자 → 회신 "'지뢰찾기' 앱을 실행합니다." 표시
#  3) DeX/X11 화면의 jkdesktop이 실제로 Minesweeper 창을 띄우는지 확인
#     (스펙 §5.1 게이트 — 서버를 끄지 마세요)
P2=$(pgrep -f 'buildterm/jkdesktop' | tr '\n' ' ')
[ -n "$P2" ] || FAIL "end-state: server not alive"
JW2=$(pgrep -f 'buildterm/[j]kweb' | tr '\n' ' ')
[ -n "$JW2" ] || FAIL "end-state: jkweb not alive"
rm -f ~/JKENGINE/phone_web_chat_remote.sh 2>/dev/null
echo "WEBCHAT-PHONE-OK"
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
[ "$TAR_SZ" -gt 50000 ] || FAIL "tar suspiciously small ($TAR_SZ bytes) — deploy list broken?"
$SSH "tar -xf - -C ~/JKENGINE" < "$TARBALL" || FAIL "remote tar extract failed"
rm -f "$TARBALL"
$SSH 'stat -c "%n %s" ~/JKENGINE/engine/tools/jkweb/main.cpp \
                      ~/JKENGINE/engine/tools/jktalk/main.cpp \
                      ~/JKENGINE/engine/CMakeLists.txt \
                      ~/JKENGINE/engine/include/apps/ChatRouter.h \
                      ~/JKENGINE/engine/src/apps/ChatRouter.cpp \
                      ~/JKENGINE/phone_web_chat_remote.sh' \
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
$SSH 'cd ~/JKENGINE/engine; test -x buildterm/jkdesktop && test -x buildterm/jkweb \
      && ls -l buildterm/jkweb \
      || { ls buildterm/jkweb* 2>&1; exit 1; }' \
  || FAIL "buildterm/jkweb did not come into existence (deploy did not register the target?)"

echo "=== 3. 폰 영수증 절차 실행 (selftest → boot → jkweb → POST 단정 → tetris 회귀) ==="
$SSH "bash $RSRC_PHONE" > "$RLOG" 2>&1
R_RC=$?
cat "$RLOG"
[ "$R_RC" -eq 0 ] || FAIL "phone-side receipt script rc=$R_RC (see above)"
grep -aq '^WEBCHAT-PHONE-OK$' "$RLOG" || FAIL "receipt script did not emit WEBCHAT-PHONE-OK"
if grep -aq 'WEBCHAT-PHONE-FAIL' "$RLOG"; then FAIL "receipt script emitted WEBCHAT-PHONE-FAIL"; fi

echo ""
echo "WEBCHAT-PHONE-OK"
echo "서버는 살아 있고 jkweb(8090)도 유지 중 — Minesweeper·Tetris 창이 DeX 화면에 떠 있다."
echo "**육안 게이트 대기**: 폰 브라우저에서 http://localhost:8090/ 를 열고 발화를 타자해"
echo "회신 표시와 DeX 화면 반응 원단을 확인하세요(스펙 §5.1). 서버·jkweb을 끄지 마세요."
exit 0
# EOF — 끝 개행 유지(스크립트 산술 무해 습관).