#!/usr/bin/env bash
# 폰 ollama 설치 실험 + jktalk stub 턴 영수증 probe (스펙 2026-10-07-desktop-chat-app
# §1.2 Task 8 — honest-fail 허용: 실패 영수증이 곧 결과물).
# 영수증 목표:
#   ① A(레시피 필수): 파티클 해시 비교 → 변경분만 tar 재배포(전부 동일이면 스킵) +
#      ninja 인크리멘털(rc 봉합, 생략 가능) ② `echo "지뢰찾기 켜줘" | ./jktalk`
#      파이프 모드 stub 턴 — 확인문(launch_app {"app":"minesweeper"})+ok:true 회신 +
#      **list_windows 기준선 diff로 새 Minesweeper 창 id 생성 단정**(srvx.log created
#      카운트 증가는 보조 증거로 기록) ③ pkg/dpkg로 폰 ollama 원천·버전·아키텍처
#      기록(honest: 패키지 부재/아키텍처 미지원이어도 그대로 영수증) ④ `ollama serve`
#      백그라운드 기동+health ⑤ 소형 모델 pull(qwen2.5:0.5b, timeout 900) ⑥
#      **통합 갭 정직 기록**: JKLlmEngine::StartTurn은 비동기 콜백이고 jktalk의 ①
#      슬롯 주석이 상정한 llm.Route(text,action) 메서드는 존재하지 않아 jktalk의
#      동기 ProcessTurn에 "cfg만" 배선할 수 없다 — 코드 무수정 실턴은 불가, 대신
#      curl 대체 영수증(/api/generate 발화→응답 초 실측, **대체 영수증으로만
#      정직 라벨**) ⑦ 종료 상태=창 서버 UP+jkweb 유지(**절서 끊지 않는다**;
#      ollama serve는 모델 성립 시에만 유지).
# 판정 컨트롤러에서의 의미: OLLAMA-VERDICT: PROMOTE-CANDIDATE면 승격 검토로,
# KEEP-STUB이면 유예(원인 목록이 판정 근거). hard receipt(ssh/서버/jkweb/영수증 A/
# 종료 상태) 실패만 probe rc=1 — ollama 단계 실패는 rc=0으로 통과하며
# OLLAMA-VERDICT: KEEP-STUB이 곧 결과물이다(honest-fail 계약).
#   실행법(윈도 Git Bash, 저장소 루트 어디서든):
#     bash engine/tools/probes/phone_ollama_try.sh
#   풀 로그: engine/tmp/phone_ollama_try_run.log
#   재실행 가능: 창 서버·jkweb은 **끊지 않는다**(T7 사용자 육안 게이트 잔존 —
#   죽어 있으면 tx4_boot.sh로 부팅해 되살린다 — 이 probe 전체에서 유일한 서버
#   기동 지점). ollama serve만 선 절사 후 재기동.
#   함정 원장(phone_chat.sh/phone_web_chat.sh 헤더 그대로): tar는 저장소 루트에서
#   만들어 `-C ~/JKENGINE`으로 풀어야 한다(engine/engine 중첩 트랩). agentctl
#   와이어는 서브커맨드 agentctl·키 tool/args. 폰 /tmp는 쓰기 불가 — 로그는
#   ~/tmp/로. 폰 toybox pgrep -x는 거짓음성 — -f 브래킷 계열('[]' 자기매칭 방지).
#   rc 봉합: 원격 복합문이 echo로 끝나면 종료코드가 항상 0 — PIPESTATUS/exit로
#   전파, `|| true` 금지. tar 전송은 LAN 내부 ssh 한정 — 클라우드/외부 불가.
set -u
export MSYS_NO_PATHCONV=1  # MSYS가 ssh 인수의 posix 경로를 찢는 것 차단(wsl probe 선례)

ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
cd "$ROOT" || exit 1
SCRATCH="$ROOT/engine/tmp"
mkdir -p "$SCRATCH"
TARBALL="$SCRATCH/phone_ollama_try.tar"
RLOG="$SCRATCH/phone_ollama_try_run.log"
RSRC_TAR_NAME="phone_ollama_try_remote.sh"
RSRC_PHONE="~/JKENGINE/$RSRC_TAR_NAME"   # 폰 측 삭제는 remote script 마지막(임시 파일만)

SSH="ssh -p 8022 -o BatchMode=yes -o ConnectTimeout=15 -i $HOME/.ssh/termux_jkengine u0_a4@${PHONE_HOST:?PHONE_HOST unset}"

FAIL() { echo "OLLAMA-TRY-FAIL: $*"; exit 1; }

# ── 배포 후보 파티클: 채팅 라인 5종(T6 jktalk + T7 jkweb + 라우터 2종 +
#    CMakeLists). 전부 해시가 같으면 배포도 ninja도 스킵(인크리멘털 최단 경로).
PARTICLES=(
  engine/tools/jktalk/main.cpp
  engine/tools/jkweb/main.cpp
  engine/CMakeLists.txt
  engine/include/apps/ChatRouter.h
  engine/src/apps/ChatRouter.cpp
)

for f in "${PARTICLES[@]}"; do
  [ -f "$ROOT/$f" ] || FAIL "deploy source missing: $f"
done

echo "=== 0. pre-flight — ssh 생존 + wake-lock + 창 서버·jkweb 상태(끊지 않는다) ==="
$SSH 'echo PHONE-REACHABLE; uname -m' || FAIL "ssh failed (phone unreachable) — 접속 증거: ssh -p 8022 -i ~/.ssh/termux_jkengine u0_a4@${PHONE_HOST:?PHONE_HOST unset}"
$SSH 'command -v termux-wake-lock >/dev/null 2>&1 && termux-wake-lock || echo WARN-no-wake-lock' \
  || FAIL "ssh failed (phone unreachable?)"
SRV_STATE=$($SSH "pgrep -f 'buildterm/[j]kdesktop' >/dev/null 2>&1 && echo SRV-UP || echo SRV-DOWN") || FAIL "server state check failed"
echo "server state at entry: $SRV_STATE"
WEB_STATE=$($SSH "pgrep -f 'buildterm/[j]kweb' >/dev/null 2>&1 && echo WEB-UP || echo WEB-DOWN") || FAIL "jkweb state check failed"
echo "jkweb state at entry: $WEB_STATE"
if [ "$SRV_STATE" = "SRV-DOWN" ]; then
  echo "server down at entry — tx4_boot.sh로 부팅 후 재진행(이 probe의 유일한 서버 기동 지점)"
  $SSH 'bash ~/tx4_boot.sh; sleep 6; pgrep -f "buildterm/[j]kdesktop" >/dev/null && echo SRV-UP-BOOTED || { echo "--- srvx.log:"; tail -20 ~/srvx.log; exit 1; }' \
    || FAIL "server could not be booted (tx4_boot failed)"
fi
[ "$WEB_STATE" = "WEB-UP" ] || FAIL "jkweb not alive at entry — Task 7 육안 게이트 잔존 상태가 무너져 있다(서버는 살리지만 jkweb은 probe가 기동하지 않는다 — 수동 복구 후 재실행)"

echo "=== 1. 파티클 신선도 해시 비교 — 변경분만 배포(전부 동일이면 tar+ninja 스킵) ==="
printf '%s\n' "${PARTICLES[@]}" > "$SCRATCH/phone_ollama_try_local_manifest"
LOCAL_HASHES=$(sha256sum "${PARTICLES[@]}" | sed 's/\*$//')
REMOTE_HASHES=$($SSH 'cd ~/JKENGINE && sha256sum engine/tools/jktalk/main.cpp engine/tools/jkweb/main.cpp engine/CMakeLists.txt engine/include/apps/ChatRouter.h engine/src/apps/ChatRouter.cpp' 2>/dev/null) \
  || FAIL "remote hash check failed"
echo "local hashes:"; printf '%s\n' "$LOCAL_HASHES"
echo "remote hashes:"; printf '%s\n' "$REMOTE_HASHES"
CHANGED=()
while IFS= read -r line; do
  h=${line%% *}
  f=${line#* }
  rline=$(printf '%s\n' "$REMOTE_HASHES" | grep " $f\$" | head -1)
  rh=${rline%% *}
  [ "$h" != "$rh" ] && CHANGED+=("$f")
done <<< "$LOCAL_HASHES"
if [ "${#CHANGED[@]}" -gt 0 ]; then
  echo "changed particles (${#CHANGED[@]}):"; printf '  %s\n' "${CHANGED[@]}"
  for f in "${CHANGED[@]}"; do
    [ -f "$ROOT/$f" ] || FAIL "changed particle missing locally: $f"
  done
  echo "=== 1b. tar-over-ssh 재배포 (변경분 ${#CHANGED[@]}종) + ninja 인크리멘털 ==="
  for r in 1 2 3; do
    if tar -cf "$TARBALL" -C "$ROOT" "${CHANGED[@]}"; then break
    elif [ "$r" -eq 3 ]; then FAIL "tar creation failed"; fi
  done
  echo "deploy list:"; tar -tf "$TARBALL"
  TAR_SZ=$(wc -c < "$TARBALL" | tr -d ' ')
  echo "tar size: $TAR_SZ bytes"
  [ "$TAR_SZ" -gt 1000 ] || FAIL "tar suspiciously small ($TAR_SZ bytes) — changed set broken?"
  $SSH "tar -xf - -C ~/JKENGINE" < "$TARBALL" || FAIL "remote tar extract failed"
  rm -f "$TARBALL"
  # 재배포 후 해시 일치 단정(전송 무결 — phone_chat.sh 선준).
  REMOTE_HASHES2=$($SSH 'cd ~/JKENGINE && sha256sum engine/tools/jktalk/main.cpp engine/tools/jkweb/main.cpp engine/CMakeLists.txt engine/include/apps/ChatRouter.h engine/src/apps/ChatRouter.cpp' 2>/dev/null) \
    || FAIL "remote hash recheck failed"
  while IFS= read -r line; do
    h=${line%% *}; f=${line#* }
    rh=$(printf '%s\n' "$REMOTE_HASHES2" | grep " $f\$" | head -1 | sed 's/^[0-9a-f]* //')
    [ "$h" = "$rh" ] || FAIL "post-deploy hash mismatch: $f (local=$h remote=$rh)"
  done <<< "$LOCAL_HASHES"
  echo "post-deploy hashes match"
  N_RC=0
  BUILD_BEGIN=$SECONDS
  NOUT=$($SSH 'cd ~/JKENGINE/engine && ninja -C buildterm -j4 2>&1 | tail -12; rc=${PIPESTATUS[0]}; echo NINJA-RC=$rc; exit $rc') || N_RC=$?
  BUILD_ELAPSED=$((SECONDS - BUILD_BEGIN))
  echo "$NOUT"
  echo "rebuild rc=$N_RC duration=${BUILD_ELAPSED}s"
  [ "$N_RC" -eq 0 ] || FAIL "ninja incremental rebuild rc=$N_RC (NINJA tail above)"
else
  echo "deploy skipped — 모든 채팅 파티클 5종이 폰 트리와 해시 동일(T7 fix-r1 tar가 이미 원준 전달)"
fi
$SSH 'cd ~/JKENGINE/engine; test -x buildterm/jktalk && test -x buildterm/jkweb && test -x buildterm/jkdesktop \
      && ls -l buildterm/jktalk buildterm/jkweb buildterm/jkdesktop | awk "{print \$NF, \$5}" \
      || { ls buildterm/jktalk buildterm/jkweb buildterm/jkdesktop 2>&1; exit 1; }' \
  || FAIL "phone buildterm is missing a chat-line binary (jktalk/jkweb/jkdesktop)"
echo "binaries present: jktalk/jkweb/jkdesktop"

echo "=== 2. 폰 영수증 절차 실행 (stub 턴 → ollama 실험 → 종료 상태) ==="
# 영수증 스크립트는 항상 이 시점에 미니 tar로 전송(1b 배포 스킵 경로에서도 도달 —
# 실측 2026-10-07: 스킵 경로에서 스크립트 미전송이면 rc=127 사망).
tar -cf "$TARBALL" -C "$SCRATCH" "$RSRC_TAR_NAME" || FAIL "tar (remote script) failed"
$SSH "tar -xf - -C ~/JKENGINE" < "$TARBALL" || FAIL "remote tar extract (remote script) failed"
rm -f "$TARBALL"
$SSH "stat -c '%n %s' ~/JKENGINE/$RSRC_TAR_NAME" || FAIL "remote receipt script missing on phone"
cat > "$SCRATCH/$RSRC_TAR_NAME" <<'PHONEEOF'
#!/bin/bash
# 폰 측 영수증 절차 (phone_ollama_try.sh가 생성 — Task 8). 서버·jkweb은 끊지 않는다.
set -u
cd ~/JKENGINE/engine
FAIL() { echo "OLLAMA-TRY-FAIL: $*"; exit 1; }      # hard receipt — probe rc=1
SOFT() { echo "OLLAMA-PHASE-NOTE: $*"; }            # honest 실패 기록 — rc 유지
command -v curl >/dev/null 2>&1 || FAIL "폰에 curl 없음 — health·턴 단정 불가"

echo "=== A. stub 턴 영수증 (echo | jktalk → launch_app → 새 Minesweeper 창) ==="
WINB=$(timeout 12 ./buildterm/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)
echo "baseline list_windows: $WINB"
[ -n "$WINB" ] || FAIL "baseline list_windows got no reply (server unresponsive at entry)"
MSB=$(printf '%s' "$WINB" | grep -aoE '\{"id":[^}]*"title":"Minesweeper"[^}]*\}' | grep -aoE '"id":[0-9]+' | sort)
CRB=$(grep -ac 'created (' ~/srvx.log 2>/dev/null) || CRB=0
echo "baseline minesweeper ids: ${MSB:-none} / srvx.log created baseline: $CRB"

printf '%s\n' "지뢰찾기 켜줘" | timeout 30 ./buildterm/jktalk >~/tmp/ph_ollama_jktalk.log 2>&1
A_RC=$?
echo "--- jktalk pipe-mode verbatim (rc=$A_RC):"
cat ~/tmp/ph_ollama_jktalk.log
[ "$A_RC" -eq 0 ] || FAIL "jktalk pipe-mode rc=$A_RC (fail-loud — stderr above)"
grep -aq '실행 요청됨 (launch_app {"app":"minesweeper"})' ~/tmp/ph_ollama_jktalk.log \
  || FAIL "jktalk receipt missing launch_app minesweeper confirmation line"
grep -aq '"ok":true' ~/tmp/ph_ollama_jktalk.log \
  || FAIL "jktalk receipt missing ok:true server reply (지뢰찾기 켜줘)"

sleep 15   # 폰은 클라 스폰(.so 로드+폰트 아틀라스)이 느다 — phone_chat/phone_web 원준
WINA=$(timeout 12 ./buildterm/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)
echo "after list_windows: $WINA"
[ -n "$WINA" ] || FAIL "after list_windows got no reply (server died during stub turn?)"
MSA=$(printf '%s' "$WINA" | grep -aoE '\{"id":[^}]*"title":"Minesweeper"[^}]*\}' | grep -aoE '"id":[0-9]+' | sort)
NEW=$(comm -13 <(printf '%s\n' "$MSB") <(printf '%s\n' "$MSA"))
echo "new minesweeper window ids (baseline diff): ${NEW:-NONE}"
[ -n "$NEW" ] || FAIL "stub turn produced no new Minesweeper window (baseline ids: ${MSB:-none} → after: ${MSA:-none})"
CRA=$(grep -ac 'created (' ~/srvx.log 2>/dev/null) || CRA=0
echo "srvx.log created count: $CRB → $CRA (보조 증거)"
grep -a 'created (' ~/srvx.log | tail -2 || echo "note: srvx.log had no created lines"

echo "=== B. ollama 설치 실험 (honest-fail — 각 단계 rc·원인 그대로 기록) ==="
echo "--- B0. 원천 조사: 설치 여부·pkg 원천·아키텍처·리소스 헤드룸 ---"
command -v ollama || echo "OLLAMA-ABSENT: command -v ollama 실패 — termux repos에 미설치"
PKG=$(command -v ollama) || PKG=""
if [ -n "$PKG" ]; then
  stat -c 'binary: %n %s bytes mtime=%y' "$PKG"
  dpkg -s ollama 2>/dev/null | grep -aE 'Package|Version|Maintainer|Architecture|Depends' || SOFT "dpkg -s ollama failed — 원천 기록 불가"
fi
dpkg --print-architecture
uname -m
echo "--- df home:"; df -h ~ 2>/dev/null | tail -2
echo "--- meminfo:"
head -3 /proc/meminfo
MEM_AVAIL_KB=$(awk '/MemAvailable/{print $2}' /proc/meminfo)
echo "MemAvailable: $MEM_AVAIL_KB kB"

echo "--- B1. ollama serve 기동 (선 절사 후 nohup — ssh 단절에도 생존) ---"
pkill -9 -f '[o]llama serve' 2>/dev/null
sleep 1
nohup ollama serve >~/tmp/ph_ollama_serve.log 2>&1 &
sleep 4
SERVE_UP=0
V=""
for i in 1 2 3 4 5 6 7 8; do
  V=$(curl -s -m 5 http://127.0.0.1:11434/api/version)
  [ -n "$V" ] && break
  sleep 2
done
if [ -n "$V" ]; then
  SERVE_UP=1
  echo "OLLAMA-SERVE-UP: health reply = $V"
else
  echo "--- ollama serve log:"
  tail -40 ~/tmp/ph_ollama_serve.log
  SOFT "ollama serve did not become healthy in ~20s (log above) — 모델 수령·턴 단계 스킵"
fi

PULLED=0
if [ "$SERVE_UP" -eq 1 ]; then
  echo "--- B2. ollama list (프리폴 상태 — rc 그대로 기록) ---"
  timeout 20 ollama list >~/tmp/ph_ollama_list_pre.log 2>&1
  L_RC=$?
  echo "ollama list rc=$L_RC"
  cat ~/tmp/ph_ollama_list_pre.log

  echo "--- B3. ollama pull qwen2.5:0.5b (소형 0.5B — timeout 900) ---"
  BEGIN=$(date +%s)
  timeout 900 ollama pull qwen2.5:0.5b >~/tmp/ph_ollama_pull.log 2>&1
  PULL_RC=$?
  ELAPSED=$(( $(date +%s) - BEGIN ))
  echo "pull rc=$PULL_RC elapsed=${ELAPSED}s (진행 바 로그: ~/tmp/ph_ollama_pull.log)"
  tail -c 2000 ~/tmp/ph_ollama_pull.log | tr '\r' '\n' | grep -a . | tail -6
  if [ "$PULL_RC" -eq 0 ]; then
    echo "--- B4. ollama list (post-pull — 모델 성립 단정) ---"
    timeout 20 ollama list 2>&1
    ollama list 2>/dev/null | grep -aq 'qwen2.5:0.5b' \
      && PULLED=1 || SOFT "pull rc=0인데 ollama list에 qwen2.5:0.5b 부재 — manifest 원인 기록:"
    timeout 20 ollama show qwen2.5:0.5b 2>&1 | head -12 || SOFT "ollama show failed"
  else
    echo "OLLAMA-PULL-FAIL: rc=$PULL_RC after ${ELAPSED}s — pull log tail:"
    tail -c 2000 ~/tmp/ph_ollama_pull.log | tr '\r' '\n' | tail -8
    SOFT "pull 실패(rc=$PULL_RC) — 원인은 로그 기록으로 유예 판정"
  fi

  echo "--- B5. 엔진 명령 경로 유효성 (JKLlmEngine ollama 경로 = ollama launch claude) ---"
  command -v claude || echo "GAP-EVIDENCE: 폰에 claude CLI 없음 (NO-CLAUDE-CLI)"
  timeout 10 ollama --help 2>&1 | grep -aE '^\s+[a-z]+' | head -12
  timeout 10 ollama --help 2>&1 | grep -aq 'launch' \
    && echo "GAP-EVIDENCE: ollama --help에 launch 서브커맨드 있음" \
    || echo "GAP-EVIDENCE: ollama --help에 launch 서브커맨드 없음 — BuildEngineCmd의 'ollama launch claude' 경로는 폰에서 부정(통합 갭)"
else
  echo "--- B2-B5 스킵 (serve 미성립) — OLLAMA-PHASE-NOTE로 유예 판정 기록됨"
fi

echo "=== C. ollama 턴 대체 영수증 (curl 직결 — jktalk 실턴은 통합 갭으로 불가, 정직 라벨) ==="
# 정직 라벨: 이 영수증은 **jktalk 턴이 아니라** ollama HTTP API 단독 실측이다 —
# jktalk의 ProcessTurn① 슬롯 주석이 상정한 llm.Route(text,action)는 JKLlmEngine에
# 존재하지 않고(실제 API는 비동기 StartTurn 콜백), ollama 경로 명령은 폰에 없는
# claude CLI 프로토콜('ollama launch claude ... --output-format stream-json')을
# 안다. cfg(engine=ollama)만으로 배선 불가 — 승격엔 실장 과제가 남는다.
TURN_OK=0
if [ "$SERVE_UP" -eq 1 ] && [ "$PULLED" -eq 1 ]; then
  echo "--- C1. /api/generate 비스트림 1턴 (발화: 지뢰찾기 켜줘) — time_total = 발화→응답 초"
  T1_BEGIN=$(date +%s)
  TURN=$(curl -s -m 300 -w '\nTIME_TOTAL=%{time_total}' \
    http://127.0.0.1:11434/api/generate \
    -H 'Content-Type: application/json' \
    -d '{"model":"qwen2.5:0.5b","prompt":"지뢰찾기 켜줘","stream":false}')
  C1_RC=$?
  T1_ELAPSED=$(( $(date +%s) - T1_BEGIN ))
  echo "curl rc=$C1_RC wrapper-elapsed=${T1_ELAPSED}s"
  printf '%s\n' "$TURN" | head -c 2500
  echo ""
  printf '%s' "$TURN" | grep -aq '"response"' \
    && TURN_OK=1 || SOFT "generate reply missing response field (rc=$C1_RC) — 대체 영수증 부실"
  echo "--- C2. /api/generate 스트림 — 첫 토큰까지 %{time_starttransfer}"
  TTFB=$(curl -s -N -m 300 -o ~/tmp/ph_ollama_stream.log \
    -w '%{time_starttransfer}' \
    http://127.0.0.1:11434/api/generate \
    -H 'Content-Type: application/json' \
    -d '{"model":"qwen2.5:0.5b","prompt":"지뢰찾기 켜줘","stream":true}')
  C2_RC=$?
  echo "stream rc=$C2_RC time_starttransfer=${TTFB}s — first chunks:"
  head -c 800 ~/tmp/ph_ollama_stream.log
  echo ""
else
  echo "OLLAMA-TURN-SUBSTITUTE-SKIPPED: serve/pull 미성립(SERVE_UP=$SERVE_UP PULLED=$PULLED) — 원인은 B단계 NOTE"
fi

echo "=== D. 승격 판정 (honest-fail 계약 — 유예 원인 전부 나열) ==="
REASONS=""
[ "$SERVE_UP" -eq 1 ]  || REASONS="$REASONS serve 미성립;"
[ "$PULLED" -eq 1 ]    || REASONS="$REASONS 모델 pull 미성립;"
[ "$TURN_OK" -eq 1 ]   || REASONS="$REASONS 턴(curl 대체 영수증) 미성립;"
REASONS="$REASONS 통합 갭: jktalk↔JKLlmEngine 배선 부재(비동기 StartTurn·ollama launch claude 경로)"
if [ "$TURN_OK" -eq 1 ]; then
  echo "OLLAMA-VERDICT: PROMOTE-CANDIDATE (모델 서빙·턴 실측 성립 — 단 curl 대체 영수증, jktalk 실장은 별도 과제)"
else
  echo "OLLAMA-VERDICT: KEEP-STUB (원인:$REASONS)"
fi

echo "=== E. 종료 상태 — 서버 UP + jkweb 유지 + Minesweeper 창 잔존 ==="
# teardown은 probe 임시 파일만. 창 서버·jkweb은 끊지 않는다(T7 사용자 육안 게이트
# 잔존 — 스펙 §5.1). ollama serve는 모델이 성립한 경우에만 유지(폰 자원 보존).
P2=$(pgrep -f 'buildterm/jkdesktop' | tr '\n' ' ')
[ -n "$P2" ] || FAIL "end-state: server not alive"
echo "server pids: $P2"
JW2=$(pgrep -f 'buildterm/[j]kweb' | tr '\n' ' ')
[ -n "$JW2" ] || FAIL "end-state: jkweb not alive"
echo "jkweb pids: $JW2"
WINF=$(timeout 12 ./buildterm/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)
printf '%s' "$WINF" | grep -aq '"title":"Minesweeper"' \
  || FAIL "end-state: Minesweeper window gone (list reply: $WINF)"
echo "Minesweeper window survives (stub 턴 잔존 상태 유지)"
if [ "$PULLED" -eq 1 ] && [ "$SERVE_UP" -eq 1 ]; then
  OL=$(pgrep -f '[o]llama serve' | tr '\n' ' ')
  echo "OLLAMA-SERVE-LEFT-UP: pids=$OL (모델 성립 — 후속 실측 보존; 정지는: pkill -f '[o]llama serve')"
else
  pkill -9 -f '[o]llama serve' 2>/dev/null
  echo "OLLAMA-SERVE-STOPPED: 모델 미성립 — 폰 자원 보존으로 serve 정지(원인은 B·C단계 NOTE)"
fi
rm -f ~/JKENGINE/phone_ollama_try_remote.sh 2>/dev/null
echo "OLLAMA-TRY-OK"
exit 0
PHONEEOF

echo "=== 3. 폰 영수증 절차 실행 (전 로그: $RLOG) ==="
SECONDS=0
$SSH "bash $RSRC_PHONE" > "$RLOG" 2>&1
R_RC=$?
cat "$RLOG"
PROBE_ELAPSED=$SECONDS
echo "phone-side receipt script rc=$R_RC duration=${PROBE_ELAPSED}s"
[ "$R_RC" -eq 0 ] || FAIL "phone-side receipt script rc=$R_RC (see above)"
grep -aq '^OLLAMA-TRY-OK$' "$RLOG" || FAIL "receipt script did not emit OLLAMA-TRY-OK"
if grep -aq 'OLLAMA-TRY-FAIL' "$RLOG"; then FAIL "receipt script emitted OLLAMA-TRY-FAIL"; fi

echo ""
echo "OLLAMA-TRY-OK"
grep -a 'OLLAMA-VERDICT' "$RLOG" || FAIL "receipt script emitted no OLLAMA-VERDICT line"
grep -aq 'OLLAMA-VERDICT: PROMOTE-CANDIDATE' "$RLOG" \
  && echo "승격 판정: PROMOTE-CANDIDATE — curl 대체 영수증 성립(스폰 실장은 별도 과제)" \
  || echo "승격 판정: KEEP-STUB — 원인은 OLLAMA-PHASE-NOTE 줄들(honest-fail 영수증)"
echo "종료 상태: 창 서버 UP + jkweb 유지(끊지 않음 — T7 육안 게이트 잔존)"
exit 0
# EOF — 끝 개행 유지(스크립트 산술 무해 습관).