#!/usr/bin/env bash
# 폰 라이브러리 실측 probe (스펙 2026-10-07-app-library Task 5 — §4 posix 개방의 실사입).
# 영수증 목표: ① tar-over-ssh 재배포(소스 7종) ② 폰 aarch64 ninja 리빌드+jkapp_library.so 성립
# ③ AppSelfTest 0 failure(s) ④ library-list에 phoneprobe(jkx)+minesweeper(builtin) ⑤
# ~/tx4_boot.sh 서버 부팅 ⑥ agentctl launch_app {"app":"library"} ok:true
# ⑦ list_windows "title":"Library" ⑧ srvx.log "created (920x640)"+"[library] apps=N"
# → 서버·Library 창을 **켜 둔 채 종료**(사용자 눈확인 등장 — teardown은 probe 임시 파일만).
#   실행법(윈도 Git Bash, 저장소 루트 어디서든):
#     bash engine/tools/probes/phone_library.sh
#   재실행 가능: 서버가 살아 있으면 선 절사(pkill -x) 후 재부팅. 영수증 하나라도
#   빠지면 LIBRARY-PHONE-FAIL로 비정상 종료(rc=1).
#   구조: 이 파일은 윈도 측 드라이버(배포본이 여기 있으므로). 폰 측 다행 절차는
#   인라인 인용이 2줄 넘기면 찢어지는 실측(측정 노트)에 따라 이 스크립트가
#   engine/tmp/phone_library_remote.sh(스크래치 — gitignore)로 생성해 같이 tar로
#   밀어 넣고 `bash ~/JKENGINE/phone_library_remote.sh`로 폰에서 실행한다.
#   함정 원장(docs/78 §4.5·§5.4): tar는 저장소 루트에서 만들어 `-C ~/JKENGINE`으로
#   풀어야 한다(engine/engine 중첩 트랩). agentctl 와이어는 서브커맨드 agentctl·
#   키 tool/args('agent'로 치면 demo 앱 CPU 루프). 폰 /tmp는 쓰기 불가 — 로그는
#   ~/tmp/로. tar 전송은 LAN 내부 ssh 한정 — 어떤 클라우드/외부로도 가지 않는다.
set -u
export MSYS_NO_PATHCONV=1  # MSYS가 ssh 인수의 posix 경로를 찢는 것 차단(wsl probe 선례)

ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
cd "$ROOT" || exit 1
SCRATCH="$ROOT/engine/tmp"
mkdir -p "$SCRATCH"
TARBALL="$SCRATCH/phone_library.tar"
RLOG="$SCRATCH/phone_library_run.log"
RSRC_TAR_NAME="phone_library_remote.sh"
RSRC_PHONE="~/JKENGINE/$RSRC_TAR_NAME"   # 폰 측 삭제는 remote script H단계(임시 파일만)

SSH="ssh -p 8022 -o BatchMode=yes -o ConnectTimeout=15 -i $HOME/.ssh/termux_jkengine u0_a4@192.168.219.109"

FAIL() { echo "LIBRARY-PHONE-FAIL: $*"; exit 1; }

# ── 배포 원천: Task 1-3에서 바뀐 소스 서브셋(요구 계약 그대로, 7종) ──
FILES=(
  engine/include/JKLibraryCatalog.h
  engine/src/JKLibraryCatalog.cpp
  engine/include/apps/ClientLibraryApp.h
  engine/src/apps/ClientLibraryApp.cpp
  engine/src/apps/JKAppModule_library.cpp
  engine/src/main.cpp
  engine/CMakeLists.txt
)

for f in "${FILES[@]}"; do
  [ -f "$ROOT/$f" ] || FAIL "deploy source missing: $f"
done

echo "=== 0. pre-clean (재실행 가능성) — wake-lock + 서버 절사 ==="
$SSH 'command -v termux-wake-lock >/dev/null 2>&1 && termux-wake-lock || echo WARN-no-wake-lock' \
  || FAIL "ssh failed (phone unreachable?)"
# 폰 pgrep 함정(실측): toybox pgrep -x는 argv[0] 기준이라 `./buildterm/jkdesktop
# --server`도 `/data/.../jkdesktop`도 전부 놓친다(서버가 살아 있어도 rc=1).
# 프로세스 절사·생존 검증은 -f 'buildterm/jkdesktop' 계열로 한다 — 인라인 ssh
# 자기매칭은 브래킷 트릭 [j]로, 스크립트 파일 안 실행은 그대로(자기 cmdline 무해).
$SSH "pkill -f 'buildterm/[j]kdesktop' 2>/dev/null; sleep 2; pkill -9 -f 'buildterm/[j]kdesktop' 2>/dev/null; sleep 1; pgrep -f 'buildterm/[j]kdesktop' >/dev/null 2>&1 && { echo PRECLEAN-FAIL; exit 1; } || echo SRV-DOWN" \
  || FAIL "pre-clean could not bring server down"

echo "=== 1. tar-over-ssh 재배포 (7종 + 폰 측 영수증 스크립트) ==="
cat > "$SCRATCH/$RSRC_TAR_NAME" <<'PHONEEOF'
#!/bin/bash
# 폰 측 영수증 절차 (phone_library.sh가 생성 — Task 5). 서버·Library 창은 끄지 않는다.
set -u
cd ~/JKENGINE/engine
FAIL() { echo "LIBRARY-PHONE-FAIL: $*"; exit 1; }

echo "=== A. selftest (계약: AppSelfTest 0 failure(s)) ==="
timeout 240 ./buildterm/jkdesktop test >~/tmp/ph_lib_selftest.log 2>&1
T_RC=$?
echo "selftest rc=$T_RC"
grep -a 'AppSelfTest' ~/tmp/ph_lib_selftest.log | tail -2
grep -aq 'AppSelfTest: 0 failure(s)' ~/tmp/ph_lib_selftest.log \
  || FAIL "AppSelfTest not 0 failure(s) (rc=$T_RC) — aarch64 계약 붕괴"

echo "=== B. library-list (phoneprobe jkx + minesweeper builtin) ==="
timeout 30 ./buildterm/jkdesktop library-list >~/tmp/ph_lib_list.log 2>&1
L_RC=$?
echo "library-list rc=$L_RC"
cat ~/tmp/ph_lib_list.log
[ "$L_RC" -eq 0 ] || FAIL "library-list rc=$L_RC"
grep -aE '^count=[0-9]+ base=' ~/tmp/ph_lib_list.log | head -1 | grep -aq . \
  || FAIL "library-list no well-formed count= tail line"
PP=$(grep -a '^name=phoneprobe ' ~/tmp/ph_lib_list.log | head -1)
[ -n "$PP" ] || FAIL "library-list missing phoneprobe entry (.jkx scan)"
printf '%s\n' "$PP" | grep -aq 'source=jkx' || FAIL "phoneprobe entry not source=jkx — $PP"
MS=$(grep -a '^name=minesweeper ' ~/tmp/ph_lib_list.log | head -1)
[ -n "$MS" ] || FAIL "library-list missing minesweeper entry (builtin 3원)"
printf '%s\n' "$MS" | grep -aq 'source=builtin' || FAIL "minesweeper entry not source=builtin — $MS"

echo "=== C. boot server (bash ~/tx4_boot.sh — DISPLAY=:1 표준) ==="
bash ~/tx4_boot.sh
sleep 6
# pgrep -x 함정 회피(폰 실측 — toybox는 argv[0] 기준이라 -x가 서버를 놓친다);
# 이 파일 안 실행이라 패턴이 자기 cmdline에 없어 -f 자기매칭 무해.
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

echo "=== E. launch_app {app:library} ==="
LAUNCH=$(timeout 15 ./buildterm/jkdesktop agentctl '{"tool":"launch_app","args":{"app":"library"}}' 2>/dev/null | grep -a '{' | head -1)
echo "launch reply: $LAUNCH"
[ -n "$LAUNCH" ] || FAIL "launch_app got no reply (server gone?)"
printf '%s' "$LAUNCH" | grep -aq '"ok":true' || FAIL "launch_app did not ok — $LAUNCH"

echo "=== F. list_windows (영수증: title=Library) ==="
sleep 15  # 폰은 클라 스폰(.so 16MB 로드+폰트 아틀라스)이 WSL보다 느리다
WIN=$(timeout 12 ./buildterm/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)
echo "list_windows: $WIN"
[ -n "$WIN" ] || FAIL "list_windows got no reply"
printf '%s' "$WIN" | grep -aq '"title":"Library"' \
  || FAIL "list_windows has no title=Library window (module fallback lost on aarch64?)"

echo "=== G. 서버 로그 영수증 (srvx.log) ==="
grep -a 'created (920x640)' ~/srvx.log | tail -2
grep -a '\[library\] apps=' ~/srvx.log | tail -2
grep -aq 'created (920x640)' ~/srvx.log || FAIL "srvx.log missing 'created (920x640)'"
grep -aq '\[library\] apps=' ~/srvx.log || FAIL "srvx.log missing '[library] apps='"

echo "=== H. 서버와 Library 창을 켜 둔 채 종료 (사용자 눈확인 대기) ==="
# teardown은 probe 임시 파일만 — 서버는 끄지 않는다. 자기 삭제는 bash가 fd로
# 읽은 뒤라 안전(스크래치 정책 — 재실행시 드라이버가 재생성).
rm -f ~/JKENGINE/phone_library_remote.sh 2>/dev/null
echo "LIBRARY-PHONE-OK"
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
[ "$TAR_SZ" -gt 100000 ] || FAIL "tar suspiciously small ($TAR_SZ bytes) — deploy list broken?"
$SSH "tar -xf - -C ~/JKENGINE" < "$TARBALL" || FAIL "remote tar extract failed"
rm -f "$TARBALL"
$SSH 'stat -c "%n %s" ~/JKENGINE/engine/src/apps/JKAppModule_library.cpp \
                      ~/JKENGINE/engine/src/JKLibraryCatalog.cpp \
                      ~/JKENGINE/engine/src/apps/ClientLibraryApp.cpp \
                      ~/JKENGINE/engine/include/JKLibraryCatalog.h \
                      ~/JKENGINE/engine/include/apps/ClientLibraryApp.h \
                      ~/JKENGINE/engine/src/main.cpp \
                      ~/JKENGINE/engine/CMakeLists.txt \
                      ~/JKENGINE/phone_library_remote.sh' \
  || FAIL "deployed files missing on phone"

echo "=== 2. 폰 리빌드 (ninja -C buildterm -j4, aarch64) ==="
$SSH 'cd ~/JKENGINE/engine && ninja -C buildterm -j4 2>&1 | tail -12; echo NINJA-RC=${PIPESTATUS[0]}' \
  || FAIL "ninja rebuild rc!=0 (aarch64 compile failure)"
$SSH 'cd ~/JKENGINE/engine; test -x buildterm/jkdesktop && test -e buildterm/jkapp_library.so \
      && ls -l buildterm/jkapp_library.so || { ls buildterm/jkapp* 2>&1; exit 1; }' \
  || FAIL "buildterm/jkapp_library.so did not come into existence (deploy did not register the module target?)"
# 존재 검증이 launch_app 게이트(JKWindowServer 존재 검증)와 같은 파일을 본다 — 위 통과면 E도 도달 가능.

echo "=== 3. 폰 영수증 절차 실행 (selftest → library-list → boot → launch → receipts) ==="
$SSH "bash $RSRC_PHONE" > "$RLOG" 2>&1
R_RC=$?
cat "$RLOG"
[ "$R_RC" -eq 0 ] || FAIL "phone-side receipt script rc=$R_RC (see above)"
grep -aq '^LIBRARY-PHONE-OK$' "$RLOG" || FAIL "receipt script did not emit LIBRARY-PHONE-OK"
grep -aq 'LIBRARY-PHONE-FAIL' "$RLOG" && FAIL "receipt script emitted LIBRARY-PHONE-FAIL" || true

echo ""
echo "LIBRARY-PHONE-OK"
echo "서버는 살아 있고 Library 창이 떠 있다 — 사용자 눈확인 대기. 서버를 끄지 마세요."
exit 0