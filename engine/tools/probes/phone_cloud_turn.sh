#!/usr/bin/env bash
# 폰 ollama 조달 + cloud 모델 턴 첫 실측 probe (채팅 LLM 승격 라인 T1 —
# 2026-10-08-chat-llm-promotion task-1 brief). honest-fail 허용: 실패 영수증이 곧
# 결과물 — hard receipt(ssh·창 서버·jkweb) 실패만 rc=1이고, 조달/턴 실패는
# CLOUD-VERDICT 라벨로 정직 기록하며 rc=0으로 통과한다.
# 영수증 목표:
#   ① 폰 ollama 버전·서브커맨드·cloud 지원 여부 실측 ② 구판이면 조달
#      (pkg install ollama → 실패 시 pinned GitHub release aarch64 fetch,
#      sha256 기록) ③ ollama list에서 glm-5.3-flash:cloud 성립(없으면 pull —
#      cloud 모델은 blob 없이 레퍼런스만 등록되므로 수초) ④ curl /api/chat 1 턴
#      (모델 glm-5.3-flash:cloud, timeout 60) — 응답+경과초 실측 ⑤ 동일 세션 PC
#      기준선 턴(best-effort — PC ollama 서버가 떠 있으면 실측, 아니면 헤더 상수)
#      대차 인쇄 ⑥ CLOUD-VERDICT: TURN-OK(초) / NEEDS-SIGNIN / PROCURE-FAIL(원인).
#   판정 규칙: 턴 응답에 message.content 비공백이면 TURN-OK(초); 조달·pull·턴 에러
#   이면 PROCURE-FAIL(원인); 인가·서명 계열 에러면 NEEDS-SIGNIN(signin 실행은
#   본 태스크 범위 밖 — 라벨로만).
#   실행법(윈도 Git Bash, 저장소 루트 어디서든):
#     PHONE_HOST=<폰 IP> bash engine/tools/probes/phone_cloud_turn.sh
#   접속 정보는 환경변수로(PHONE_HOST 필수 — 내부 IP는 커밋하지 않는다,
#   phone_apps.sh/fix r1 M4 계약 승계; PHONE_PORT PHONE_USER PHONE_KEY만 기본값).
#   풀 로그: engine/tmp/phone_cloud_turn.log
#   재실행 가능: 창 서버·jkweb은 **끊지 않는다**(진행 중 사용자 육안 게이트 잔존
#   — 죽어 있으면 hard FAIL; 이 probe는 서버 기동 지점이 아니다). ollama serve만
#   선 절사 후 재기동, 턴 성립 시에만 UP 유지(폰 자원 보존 — 원준 계약).
#   함정 원장(phone_ollama_try.sh/phone_apps.sh 헤더 승계 + 신규 실측):
#     · tar는 저장소 루트에서 만들어 `-C ~/JKENGINE`으로 풀어야 한다(engine/engine
#       중첩 트랩). 폰 /tmp는 쓰기 불가 — 폰 측 로그는 $TMPDIR.
#     · pkill -f '[o]llama serve' 브래킷은 **원격 스크립트 파일 안에서만 안전**.
#       ssh 인라인 복합문 같은 문자열에 "nohup ollama serve" 부분열이 있으면
#       pkill -f가 원격 셸 자신의 cmdline을 매칭해 자기 제거 → ssh rc=255로
#       채널 사망(2026-10-08 실측 trap). 이 probe는 전부 원격 스크립트 파일 경유.
#     · ssh 원격 명령에 로컬 `< /dev/null` 금지(채널 hang — 구계약 승계).
#     · rc 봉합: 원격 복합문이 echo로 끝나면 종료코드가 항상 0 — exit/PIPESTATUS로
#       전파, `|| true` 금지(정수 카운트용 grep -c는 의도된 예외 — 주석 명시).
#     · 조달된 glibc 바이너리는 폰 bionic에서 exec 거부 — 표시 메시지가
#       "No such file or directory"로 기만한다(파일 존재와 무관). 조달 성공
#       판정은 파일 존재가 아니라 `ollama version` 재실측으로만 한다.
#     · cloud 모델 응답 JSON에 duration_api_ms가 없을 수 있다(2026-10-08 PC
#       실측: chat 응답은 eval_count만) — 경과초는 curl %{time_total}이 1차 진실.
#   PC 기준선 헤더 상수(controller 실측 2026-10-08): PC cloud 턴 duration_api_ms
#   3221ms / 출력 206 tok ≈ 3.2s. 동일 세션 재실측 성립 시 그 값으로 대차한다.
set -u
export MSYS_NO_PATHCONV=1  # MSYS가 ssh 인수의 posix 경로를 찢는 것 차단(chat T8 선례)

ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
cd "$ROOT" || exit 1
SCRATCH="$ROOT/engine/tmp"
mkdir -p "$SCRATCH"
RLOG="$SCRATCH/phone_cloud_turn.log"
RSRC_NAME="phone_cloud_turn_remote.sh"   # 폰 측 삭제는 remote script 마지막(임시 파일만)

PHONE_HOST="${PHONE_HOST:-}"
PHONE_PORT="${PHONE_PORT:-8022}"
PHONE_USER="${PHONE_USER:-u0_a4}"
PHONE_KEY="${PHONE_KEY:-$HOME/.ssh/termux_jkengine}"
[ -n "$PHONE_HOST" ] || { echo "CLOUD-TURN-FAIL: PHONE_HOST unset — 폰 IP를 환경변수로 지정하세요 (내부 IP는 커밋하지 않는다)"; exit 1; }

SSH="ssh -p $PHONE_PORT -o BatchMode=yes -o ConnectTimeout=15 -o ServerAliveInterval=15 -o ServerAliveCountMax=10 -i $PHONE_KEY $PHONE_USER@$PHONE_HOST"

PC_BASELINE_S="3.2"   # PC cloud 턴 헤더 상수(동일 세션 재실측으로 대체)
PC_TURN=""            # 동일 세션 실측값(비어 있으면 상수 사용)
fail() { echo "CLOUD-TURN-FAIL: $*"; exit 1; }

echo "=== 0. pre-flight — ssh 생존 + 창 서버·jkweb 상태(끊지 않는다) ==="
$SSH 'echo PHONE-REACHABLE; uname -m' || fail "ssh failed (phone unreachable)"
SRV_STATE=$($SSH "pgrep -f 'buildterm/[j]kdesktop' >/dev/null 2>&1 && echo SRV-UP || echo SRV-DOWN") || fail "server state check failed"
WEB_STATE=$($SSH "pgrep -f 'buildterm/[j]kweb' >/dev/null 2>&1 && echo WEB-UP || echo WEB-DOWN") || fail "jkweb state check failed"
echo "server state at entry: $SRV_STATE"
echo "jkweb state at entry: $WEB_STATE"
[ "$SRV_STATE" = "SRV-UP" ] || fail "창 서버가 죽어 있다 — 이 probe는 서버 기동 지점이 아니다(진행 중 육안 게이트 상태 보존 계약)"
[ "$WEB_STATE" = "WEB-UP" ] || fail "jkweb이 죽어 있다 — 수동 복구 후 재실행(서버 보존 계약)"

echo "=== 1. PC 기준선 동일 세션 재실측 (best-effort — 떠 있으면 실측값으로 대차) ==="
PC_JSON="$SCRATCH/pc_cloud_turn.json"
# MSYS_NO_PATHCONV=1 하에서는 MSYS가 인수 경로를 바꿔주지 않는다 — PC 측 curl.exe는
# Windows형 경로를 요구한다(git.exe 경로 선례와 같은 계열; posix형이면 rc=23
# 쓰기 오류). cygpath 부재 시 조용한 폴백(스크래치 경로 그대로).
PC_JSON_WIN="$PC_JSON"
command -v cygpath >/dev/null 2>&1 && PC_JSON_WIN=$(cygpath -w "$PC_JSON")
PC_TURN=$(curl -s -m 60 http://127.0.0.1:11434/api/chat \
  -H 'Content-Type: application/json' \
  -d '{"model":"glm-5.3-flash:cloud","stream":false,"messages":[{"role":"user","content":"지뢰찾기 켜줘 — 가능한 행동 한 개를 JSON {\"action\":\"launch\",\"app\":\"minesweeper\"} 형태로만 답해줘"}]}' \
  -o "$PC_JSON_WIN" -w '%{time_total}' 2>/dev/null)
PC_RC=$?
if [ "$PC_RC" -eq 0 ] && [ -n "$PC_TURN" ] && grep -aq '"message"' "$PC_JSON"; then
  echo "PC baseline live: time_total=${PC_TURN}s (헤더 상수 ${PC_BASELINE_S}s 대체)"
  echo "PC reply head:"; head -c 300 "$PC_JSON"; echo ""
  PC_EVAL=$(grep -ao '"eval_count":[0-9]*' "$PC_JSON" | head -1 | grep -ao '[0-9]*$')
  echo "PC eval_count: ${PC_EVAL:-n/a}"
  PC_BASELINE_S="$PC_TURN"
else
  PC_TURN=""   # 실패한 실측은 대차 근거로 쓰지 않는다(전 실측 rc=23 오염 원인)
  echo "PC-BASELINE-BEST-EFFORT-SKIP: PC 기준선 실측 실패(rc=$PC_RC) — 헤더 상수 ${PC_BASELINE_S}s로 대차 (hard receipt가 아니다)"
fi

echo "=== 2. 폰 영수증 절차 생성·전송·실행 (전 로그: $RLOG) ==="
cat > "$SCRATCH/$RSRC_NAME" <<'PHONEEOF'
#!/bin/bash
# 폰 측 영수증 절차 (phone_cloud_turn.sh가 생성 — T1). 서버·jkweb은 끊지 않는다.
set -u
PLOG="${TMPDIR:-$HOME/tmp}"     # 폰 /tmp 쓰기 불가 — $TMPDIR(Termux 기본 존재)
mkdir -p "$PLOG" "$HOME/JKENGINE" 2>/dev/null
FAIL()   { echo "CLOUD-TURN-FAIL: $*"; exit 1; }  # hard receipt — probe rc=1
HARD()   { echo "CLOUD-HARD-NOTE: $*"; }          # 정직 기록(라벨 근거)
# finish: 라벨 1회 인쇄 + ollama serve 종료 상태 규칙(턴 성립 시에만 UP 유지).
finish() {
  if printf '%s' "$1" | grep -aq 'TURN-OK'; then
    echo "OLLAMA-SERVE-LEFT-UP: 턴 성립 — 후속 실측 보존(정지: pkill -f '[o]llama serve')"
  else
    pkill -9 -f '[o]llama serve' 2>/dev/null   # 스크립트 파일 경유 — 브래킷 무해
    echo "OLLAMA-SERVE-STOPPED: 턴 미성립 — 폰 자원 보존(원인은 위 라벨)"
  fi
  echo "CLOUD-VERDICT: $1"
  exit 0
}
command -v curl >/dev/null 2>&1 || FAIL "폰에 curl 없음 — 턴 실측 불가"

echo "=== A. 폰 ollama 인벤토리 (버전·서브커맨드·cloud 지원) ==="
OL_BIN=$(command -v ollama) || OL_BIN=""
OL_VER=""
HELP=""
if [ -n "$OL_BIN" ]; then
  echo "binary: $OL_BIN ($(stat -c '%s bytes' "$OL_BIN" 2>/dev/null))"
  # 버전 1차 진실 = 서버 /api/version API(구판 CLI에 version 서브커맨드가 없는
  # 경우가 있다 — 실측: `ollama version`이 빈 응답, API는 0.23.2 회신).
  OL_VER=$(curl -s -m 5 http://127.0.0.1:11434/api/version | grep -aoE '[0-9]+\.[0-9]+\.[0-9]+' | head -1)
  [ -n "$OL_VER" ] || OL_VER=$(timeout 20 ollama version 2>&1 | grep -aoE '[0-9]+\.[0-9]+\.[0-9]+' | head -1)
  echo "ollama version (API 선, CLI 폴백): ${OL_VER:-UNKNOWN}"
  HELP=$(timeout 15 ollama --help 2>&1)
  for sub in serve pull signin launch; do
    printf '%s' "$HELP" | grep -aqE "^[[:space:]]+$sub([[:space:]]|$)" \
      && echo "  subcommand $sub: PRESENT" || echo "  subcommand $sub: ABSENT"
  done
  ls -l ~/.ollama/id_ed25519 2>/dev/null && echo "identity key present" || echo "identity key absent"
else
  HARD "command -v ollama 실패 — 폰에 ollama 미설치(조달 대상)"
fi
# 구판 판정: ollama 미설치, signin 서브커맨드 부재, 또는 버전 < 0.12.0
# (0.12 진입선 = cloud 모델 지원; signin 부재는 서명 인가 경로 없음을 뜻한다).
IS_OLD=0
OLD_REASON=""
if [ -z "$OL_BIN" ]; then
  IS_OLD=1; OLD_REASON="ollama 미설치"
elif printf '%s' "$HELP" | grep -aqE '^[[:space:]]+signin([[:space:]]|$)'; then
  echo "판정: signin 서브커맨드 존재 — cloud 지원판(조달 불요 경로)"
else
  IS_OLD=1; OLD_REASON="signin 서브커맨드 부재(버전 ${OL_VER:-UNKNOWN})"
fi
if [ "$IS_OLD" -eq 0 ] && [ -n "$OL_VER" ]; then
  OL_MAJ=$(printf '%s' "$OL_VER" | cut -d. -f1)
  OL_MIN=$(printf '%s' "$OL_VER" | cut -d. -f2)
  if [ "$OL_MAJ" -eq 0 ] && [ "$OL_MIN" -lt 12 ]; then
    IS_OLD=1; OLD_REASON="버전 ${OL_VER} < 0.12.0(cloud 지원 진입선)"
  fi
fi
echo "구판 판정: IS_OLD=$IS_OLD reason=${OLD_REASON:-없음 — 조달 스킵}"

echo "=== B. 조달 (구판일 때만 — pkg → pinned release fallback, sha256 기록) ==="
PROCURED=0
PROC_ERR=""
if [ "$IS_OLD" -eq 1 ]; then
  echo "--- B1. pkg install ollama (timeout 600)"
  timeout 600 pkg install -y ollama >"$PLOG/ph_cloud_pkg.log" 2>&1
  P_RC=$?
  echo "pkg rc=$P_RC log tail:"; tail -8 "$PLOG/ph_cloud_pkg.log"
  NOL=$(command -v ollama) || NOL=""
  if [ -n "$NOL" ]; then
    NV=$(curl -s -m 5 http://127.0.0.1:11434/api/version | grep -aoE '[0-9]+\.[0-9]+\.[0-9]+' | head -1)
    [ -n "$NV" ] || NV=$(timeout 20 ollama version 2>&1 | grep -aoE '[0-9]+\.[0-9]+\.[0-9]+' | head -1)
    if [ -n "$NV" ] && [ "$NV" != "${OL_VER:-}" ]; then
      PROCURED=1
      echo "PKG-PROCURE-OK: ollama ${OL_VER:-NONE} → $NV"
    else
      PROC_ERR="pkg 성공했으나 버전 미변화(${NV:-no-version})"
    fi
  else
    PROC_ERR="pkg install ollama 실패(rc=$P_RC — termux 원천에 패키지 부재 가능)"
  fi
  if [ "$PROCURED" -eq 0 ]; then
    echo "--- B2. pinned GitHub release fetch (aarch64, timeout 600)"
    REL_URL="https://github.com/ollama/ollama/releases/download/v0.12.6/ollama-linux-arm64.tar"
    echo "pinned URL: $REL_URL"
    timeout 600 curl -sL -m 600 -o "$PLOG/ollama-arm64.tar" "$REL_URL"
    F_RC=$?
    echo "fetch rc=$F_RC"
    ls -l "$PLOG/ollama-arm64.tar" 2>/dev/null
    sha256sum "$PLOG/ollama-arm64.tar" 2>/dev/null || HARD "sha256sum 기록 실패"
    if [ "$F_RC" -eq 0 ] && [ -s "$PLOG/ollama-arm64.tar" ]; then
      mkdir -p "$HOME/bin"
      tar -xf "$PLOG/ollama-arm64.tar" -C "$PLOG" 2>/dev/null
      FOUND_BIN=$(find "$PLOG" -maxdepth 2 -name ollama -type f 2>/dev/null | head -1)
      if [ -n "$FOUND_BIN" ]; then
        cp -f "$FOUND_BIN" "$HOME/bin/ollama"
        chmod +x "$HOME/bin/ollama"
        NV=$(timeout 20 "$HOME/bin/ollama" version 2>&1 | grep -aoE '[0-9]+\.[0-9]+\.[0-9]+' | head -1)  # 신설 바이너리 — CLI 버전 직접 실측(서버 폴백 없이)
        if [ -n "$NV" ]; then
          PROCURED=1
          echo "FETCH-PROCURE-OK: $HOME/bin/ollama version=$NV (sha256 위 로그)"
        else
          PROC_ERR="fetch 바이너리가 version을 못 낸다 — glibc/bionic exec 거부 의심(표시는 \"No such file or directory\"로 기만)"
          echo "--- exec 시도 표시 메시지(기만 패턴 영수증):"
          "$HOME/bin/ollama" version 2>&1 || echo "  exec rc nonzero"
        fi
      else
        PROC_ERR="fetch tar에서 ollama 바이너리를 못 찾았다"
      fi
    else
      PROC_ERR="pinned release fetch 실패(rc=$F_RC)"
    fi
  fi
  if [ "$PROCURED" -eq 0 ]; then
    finish "PROCURE-FAIL (원인: ${PROC_ERR:-알 수 없음})"
  fi
else
  echo "조달 스킵 — 폰 ollama(${OL_VER:-UNKNOWN})가 signin·cloud 지원판(IS_OLD=0)"
fi
if [ -x "$HOME/bin/ollama" ]; then export PATH="$HOME/bin:$PATH"; fi
OL_CALL="ollama"
[ -x "$HOME/bin/ollama" ] && OL_CALL="$HOME/bin/ollama"
OL_NOW=$(timeout 20 "$OL_CALL" version 2>&1 | grep -aoE '[0-9]+\.[0-9]+\.[0-9]+' | head -1)
[ -n "$OL_NOW" ] || OL_NOW=$(curl -s -m 5 http://127.0.0.1:11434/api/version | grep -aoE '[0-9]+\.[0-9]+\.[0-9]+' | head -1)
echo "procure 후 유효 ollama version (${OL_CALL}): ${OL_NOW:-${OL_VER:-UNKNOWN}}"

echo "=== C. ollama serve 기동 (선 절사 후 nohup — 브래킷 pkill은 스크립트 경유라 무해) ==="
pkill -9 -f '[o]llama serve' 2>/dev/null
sleep 1
nohup "$OL_CALL" serve >"$PLOG/ph_cloud_serve.log" 2>&1 &
sleep 4
V=""
for i in 1 2 3 4 5 6 7 8; do
  V=$(curl -s -m 5 http://127.0.0.1:11434/api/version)
  [ -n "$V" ] && break
  sleep 2
done
if [ -n "$V" ]; then
  echo "OLLAMA-SERVE-UP: health reply = $V"
else
  echo "--- serve log tail:"; tail -30 "$PLOG/ph_cloud_serve.log" 2>/dev/null
  finish "PROCURE-FAIL (원인: ollama serve 미성립 — serve log 위 참조; 조달 ${PROCURED:+성공}${PROCURED:-불요})"
fi

echo "=== D. ollama list — glm-5.3-flash:cloud 성립 여부 (없으면 pull/레퍼런스 등록) ==="
timeout 30 ollama list >"$PLOG/ph_cloud_list_pre.log" 2>&1
echo "ollama list rc=$? (head 12):"; head -12 "$PLOG/ph_cloud_list_pre.log"
CLOUD_IN_LIST=$(grep -ac 'glm-5.3-flash:cloud' "$PLOG/ph_cloud_list_pre.log")  # 0건이면 0 인쇄·rc=1 — set -e 없어 무해, 값만 쓴다
echo "glm-5.3-flash:cloud in list: $CLOUD_IN_LIST"
if [ "$CLOUD_IN_LIST" -eq 0 ]; then
  echo "--- pull glm-5.3-flash:cloud (cloud 모델 — blob 없이 레퍼런스 등록, timeout 180)"
  timeout 180 ollama pull glm-5.3-flash:cloud >"$PLOG/ph_cloud_pull.log" 2>&1
  PL_RC=$?
  echo "pull rc=$PL_RC log tail:"; tail -5 "$PLOG/ph_cloud_pull.log" | tr '\r' '\n'
  CLOUD_IN_LIST=$(timeout 30 ollama list 2>/dev/null | grep -ac 'glm-5.3-flash:cloud')  # 위와 동일 — 값만 쓴다
  echo "list after pull: glm-5.3-flash:cloud count=$CLOUD_IN_LIST"
  if [ "$PL_RC" -ne 0 ]; then
    PULL_LOG=$(tail -5 "$PLOG/ph_cloud_pull.log" | tr '\r' '\n')
    if printf '%s' "$PULL_LOG" | grep -aqiE 'sign|auth|permission|401|403'; then
      finish "NEEDS-SIGNIN (근거: pull이 인가 계열 에러 — 폰 브라우저 ollama.com 서명 필요, signin은 본 태스크 범위 밖)"
    fi
    HARD "pull 실패(rc=$PL_RC) — 인가 계열 에러 아님, 턴 시도로 진행"
  fi
else
  echo "pull 불요 — list에 이미 성립"
fi

echo "=== E. cloud 모델 턴 실측 (curl /api/chat 1회 — timeout 60) ==="
T_BEGIN=$(date +%s)
TURN=$(curl -s -m 60 -w '\nCURL_TIME_TOTAL=%{time_total}' \
  http://127.0.0.1:11434/api/chat \
  -H 'Content-Type: application/json' \
  -d '{"model":"glm-5.3-flash:cloud","stream":false,"messages":[{"role":"user","content":"지뢰찾기 켜줘 — 가능한 행동 한 개를 JSON {\"action\":\"launch\",\"app\":\"minesweeper\"} 형태로만 답해줘"}]}')
E_RC=$?
T_ELAPSED=$(( $(date +%s) - T_BEGIN ))
echo "curl rc=$E_RC wrapper-elapsed=${T_ELAPSED}s"
printf '%s\n' "$TURN" | head -c 2500
echo ""
TURN_BODY=$(printf '%s' "$TURN" | sed '/^CURL_TIME_TOTAL=/d' | tr -d '\n')
if printf '%s' "$TURN_BODY" | grep -aqiE '"error".*(sign|auth|permission|401|403)'; then
  finish "NEEDS-SIGNIN (근거: 턴 에러가 인가 계열 — signin은 본 태스크 범위 밖)"
fi
TURN_TIME=$(printf '%s' "$TURN" | grep -ao 'CURL_TIME_TOTAL=[0-9.]*' | head -1 | cut -d= -f2)
CONTENT=$(printf '%s' "$TURN_BODY" | grep -ao '"content":"[^"]*"' | head -1)
EVAL_N=$(printf '%s' "$TURN_BODY" | grep -ao '"eval_count":[0-9]*' | head -1)
API_MS=$(printf '%s' "$TURN_BODY" | grep -ao '"duration_api_ms":[0-9]*' | head -1)
echo "TURN-TIME: ${TURN_TIME:-N/A}s (wrapper=${T_ELAPSED}s) $EVAL_N $API_MS"
if [ -n "$CONTENT" ] && [ "$CONTENT" != '"content":""' ]; then
  echo "터닝 응답 비교 근거 — 내용 head: ${CONTENT:0:200}"
  if [ -n "$TURN_TIME" ]; then
    finish "TURN-OK(${TURN_TIME}s)"
  else
    finish "TURN-OK(wrapper ${T_ELAPSED}s)"
  fi
else
  finish "PROCURE-FAIL (원인: 조달 후에도 턴 응답에 content 부실(rc=$E_RC) — 응답 원문은 위 E단계)"
fi
PHONEEOF

# 폰 측 절차는 항상 이 시점에 전송(스크립트 파일 경유 — pkill 브래킷 자기제거 트랩 회피).
tar -cf "$SCRATCH/phone_cloud_turn.tar" -C "$SCRATCH" "$RSRC_NAME" || fail "tar (remote script) failed"
$SSH "tar -xf - -C ~/JKENGINE" < "$SCRATCH/phone_cloud_turn.tar" || fail "remote tar extract failed"
rm -f "$SCRATCH/phone_cloud_turn.tar"
$SSH "stat -c '%n %s' ~/JKENGINE/$RSRC_NAME" || fail "remote receipt script missing on phone"

SECONDS=0
$SSH "bash ~/JKENGINE/$RSRC_NAME" > "$RLOG" 2>&1
R_RC=$?
cat "$RLOG"
PROBE_ELAPSED=$SECONDS
echo "phone-side receipt script rc=$R_RC duration=${PROBE_ELAPSED}s"
[ "$R_RC" -eq 0 ] || fail "phone-side receipt script rc=$R_RC (see above)"
grep -aq '^CLOUD-VERDICT:' "$RLOG" || fail "receipt script emitted no CLOUD-VERDICT line"
if grep -aq 'CLOUD-TURN-FAIL' "$RLOG"; then fail "receipt script emitted CLOUD-TURN-FAIL"; fi

echo ""
echo "=== 3. 종료 상태 — 서버 UP + jkweb 유지(끊지 않는다) ==="
$SSH 'command -v termux-wake-lock >/dev/null 2>&1 && termux-wake-lock >/dev/null 2>&1; true'
P2=$($SSH "pgrep -f 'buildterm/[j]kdesktop' | tr '\n' ' '") || fail "end-state server check failed"
[ -n "$P2" ] || fail "end-state: server not alive"
echo "server pids: $P2"
JW2=$($SSH "pgrep -f 'buildterm/[j]kweb' | tr '\n' ' '") || fail "end-state jkweb check failed"
[ -n "$JW2" ] || fail "end-state: jkweb not alive"
echo "jkweb pids: $JW2"
grep -a 'CLOUD-VERDICT' "$RLOG"
if grep -aq 'CLOUD-VERDICT: TURN-OK' "$RLOG"; then
  PHONE_TURN_TIME=$(sed -n 's/.*CLOUD-VERDICT: TURN-OK(\([0-9.]*\)s).*/\1/p' "$RLOG" | head -1)
  if [ -n "$PC_TURN" ]; then
    echo "대차: 폰 ${PHONE_TURN_TIME}s vs PC동일세션 ${PC_TURN}s"
  else
    echo "대차: 폰 ${PHONE_TURN_TIME}s vs PC헤더상수 ${PC_BASELINE_S}s (동일 세션 PC 재실측 없음)"
  fi
fi
rm -f "$SCRATCH/$RSRC_NAME"
echo "PHONE-CLOUD-TURN-OK"
exit 0
# EOF — 끝 개행 유지(스크립트 산술 무해 습관).
