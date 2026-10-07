#!/usr/bin/env bash
# 폰 claude CLI 조달 + `ollama launch claude` 턴 첫 실측 probe (채팅 LLM 승격 라인
# T2 — 2026-10-08-chat-llm-promotion task-2 brief). honest-fail 허용: 실패 영수증이
# 곧 결과물 — hard receipt(ssh·창 서버·jkweb) 실패만 rc=1이고, 조달/launch 실패는
# CLAUDE-VERDICT 라벨로 정직 기록하며 rc=0으로 통과한다.
# 영수증 목표:
#   ① 폰 ollama launch 서브커맨드 + claude CLI 소유 여부 실측 ② 조달(pkg install
#      nodejs-lts → npm install -g @anthropic-ai/claude-code) — 경과초·용량
#      (du -sk $PREFIX 전후 델타)·에러 시 원문 ③ claude --version 실측
#      ④ `ollama launch claude --model glm-5.3-flash:cloud -- -p "say ok"
#      --output-format stream-json` 1회 실측 — ⑤ stream-json에 --verbose 요구 에러가
#      보이면 동일 명령+--verbose 재시도, 그래도 미성립이면 --output-format json
#      최후 1회(jkchat 관례 — docs/31 §6) — 각 시도 stdout/stderr 원문 + 경과초
#      ⑥ PC 동일 명령 기준선(best-effort — PC ollama·claude가 있으면 실측, 아니면 스킵)
#      ⑦ claude 기동 실패 시 원인 봉합(vendor postinstall 수기 실행·npm registry
#      android 패키지 존재 확인·musl 우회 exec 시험) — honest receipt 그대로 원장.
# 판정 규칙(CLAUDE-VERDICT):
#   LAUNCH-OK(초) — launch 턴 stdout에 비공백 result 구성(rc=0).
#   NO-CLAUDE-CLI(원인) — claude CLI 층 조달/실행 불성립(npm 실패·설치 후에도
#     claude 미성립·launch 서브커맨드 부재·launch 턴 미성립/타임아웃) — T3 cfg
#     분기("ollama-direct" 폰 1차)의 근거.
#   PROCURE-FAIL(원인) — 기반 스택 층 실패(pkg nodejs-lts 미성립·ollama serve 미성립).
#   hard receipt(ssh·창 서버·jkweb) 실패만 rc=1 — 위 라벨 실패는 rc=0으로 소화
#     (라벨이 곧 결론, 스크립트 rc는 채널 건강만 뜻한다).
# 실행법(윈도 Git Bash, 저장소 루트 어디서든):
#   PHONE_HOST=<폰 IP> bash engine/tools/probes/phone_claude_cli.sh
# 접속 정보는 환경변수로(PHONE_HOST 필수 — 내부 IP는 커밋하지 않는다, phone_apps.sh
# fix r1 M4 계약 승계; PHONE_PORT PHONE_USER PHONE_KEY만 기본값).
# 풀 로그: engine/tmp/phone_claude_cli.log (고정명, 재실행 덮어씀 — R3 유예;
#   누적 사본 engine/tmp/phone_claude_cli_all.log에 run 구분 헤더로 어펜드).
# 재실행 가능: 창 서버·jkweb은 **끊지 않는다**(진행 중 사용자 육안 게이트 잔존 —
#   죽어 있으면 hard FAIL; 이 probe는 서버 기동 지점이 아니다). ollama serve도
#   **끊지 않는다**(T1이 턴 성립으로 UP 유지한 상태 — 이 probe는 살아 있는지만
#   검사하고, 죽어 있으면 기동만 한다: pkill 지점 없음).
# 함정 원장(phone_cloud_turn.sh/docs 승계 + 본 태스크 신규):
#   · tar는 저장소 루트에서 만들어 `-C ~/JKENGINE`으로 풀어야 한다(engine/engine
#     중첩 트랩). 폰 /tmp는 쓰기 불가 — 폰 측 로그는 $TMPDIR.
#   · pkill -f 브래킷은 **원격 스크립트 파일 안에서만 안전** — ssh 인라인 복합문
#     문자열에 대상 cmdline 부분열이 있으면 원격 셸 자신을 매칭해 자기 제거
#     (T1 실측 trap). 본 probe는 원격 스크립트 파일 경유이며 pkill 지점 자체가 없다.
#   · ssh 원격 명령에 로컬 `< /dev/null` 금지(채널 hang — 구계약 승계).
#   · rc 봉합: 원격 복합문이 echo로 끝나면 종료코드가 항상 0 — exit/PIPESTATUS로
#     전파, `|| true` 금지(정수 카운트용 grep -c는 의도된 예외 — 주석 명시).
#   · 폰 bionic에서 glibc 바이너리 exec 거부는 "No such file or directory"로
#     기만 표시(docs/81 §3). nodejs-lts는 Termux pkg 원산(bionic native)이라
#     무관하지만, 조달 성공 판정은 파일 존재가 아니라 `claude --version`/`node
#     --version` 재실측으로만 한다.
#   · MSYS_NO_PATHCONV=1 하엔 PC 측 MSYS 경로 변환이 죽는다 — PC 측 파일 인수는
#     본 probe에서 쓰지 않는다(출력 리다이렉트뿐). powershell interop CRLF는
#     `tr -d '\r'` — 본 probe는 powershell 미사용, npm 진행바 \r만 tr로 평상화.
#   · bare `ollama launch`(인자 없음)는 비TTY TUI 메뉴로 **rc=0**으로 돌아온다
#     (docs/80 §8 보조 함정 승계) — 항상 통합 인자(claude --model …)를 개입시켜
#     TUI 진입을 배제한다.
#   · `ollama launch claude -- -p --output-format stream-json`이 --verbose를
#     요구하는 CLI 조합 에러일 수 있다(docs/31 §6 계열) — stderr/stdout을 분리
#     파이프해 `[claude-code:unrecognized_model]` 같은 stderr 경고가 stdout JSON
#     파싱을 오염하지 않게 한다(docs/31 §6 승계).
#   · `ollama launch`가 브라우저 OAuth/온보딩 플로우를 시도하면 TTY 없는 ssh
#     채널에서 정체한다 — timeout 240으로 절사하고 stderr 원문을 그대로 원장한다
#     (정체 자체가 launch 경로 불가의 정직 근거).
#   · npm -g는 Termux $PREFIX에 설치 — 용량 실측은 du -sk $PREFIX 전후 델타.
#   · npm의 @anthropic-ai/claude-code는 **wrapper 패키지**(~223KB, postinstall이
#     native installer) — npm rc=0·`command -v claude` 성립만으로는 판정 불가
#     (bin 스텁이 PATH에 올라온다). 기동성의 진실 = `claude --version` rc.
#     2026-10-08 폰 실측: postinstall 메시지 "Native binaries for
#     linux-arm64-android are not available on this release channel" + npm
#     registry에 android 플랫폼 패키지 E404 — bionic(안드로이드)은 지원 플랫폼 밖.
#   · musl/glibc 우회 바이너리(bun 링크)도 bionic에서 rc=127 — 표시는
#     "No such file or directory"(interpreter /lib/ld-musl-aarch64.so.1 부재,
#     bionic 기만 패턴 위와 동일). 봉합 1회 후 마커($TMPDIR/ph_cc_musl_seal.txt)
#     저장 — 재실행은 스킵(111MB 재다운로드 회피).
set -u
export MSYS_NO_PATHCONV=1  # MSYS가 ssh 인수의 posix 경로를 찢는 것 차단(chat T8 선례)

ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
cd "$ROOT" || exit 1
SCRATCH="$ROOT/engine/tmp"
mkdir -p "$SCRATCH"
RLOG="$SCRATCH/phone_claude_cli.log"
RHIST="$SCRATCH/phone_claude_cli_all.log"
RSRC_NAME="phone_claude_cli_remote.sh"   # 폰 측 삭제는 remote script 마지막(NR1)
# 전 세션 누적 기록: RLOG는 폰 측 스크립트 출력만, RHIST는 드라이버 섹션(PC 기준선·
# 종료 상태 포함)까지 run 구분 헤더로 모두 보존(덮어씀 유예 R3에 대한 조치).
{ echo "=== RUN $(date '+%F %T') MODEL=glm-5.3-flash:cloud ==="; } >> "$RHIST"
exec > >(tee -a "$RHIST")

PHONE_HOST="${PHONE_HOST:-}"
PHONE_PORT="${PHONE_PORT:-8022}"
PHONE_USER="${PHONE_USER:-u0_a4}"
PHONE_KEY="${PHONE_KEY:-$HOME/.ssh/termux_jkengine}"
[ -n "$PHONE_HOST" ] || { echo "CLAUDE-CLI-FAIL: PHONE_HOST unset — 폰 IP를 환경변수로 지정하세요 (내부 IP는 커밋하지 않는다)"; exit 1; }

SSH="ssh -p $PHONE_PORT -o BatchMode=yes -o ConnectTimeout=15 -o ServerAliveInterval=15 -o ServerAliveCountMax=10 -i $PHONE_KEY $PHONE_USER@$PHONE_HOST"

MODEL="glm-5.3-flash:cloud"   # PC·폰 공통 관례 모델(docs/44·docs/57 · T1 실측)
fail() { echo "CLAUDE-CLI-FAIL: $*"; exit 1; }

echo "=== 0. pre-flight — ssh 생존 + 창 서버·jkweb·ollama serve 상태(끊지 않는다) ==="
$SSH 'echo PHONE-REACHABLE; uname -m' || fail "ssh failed (phone unreachable)"
SRV_STATE=$($SSH "pgrep -f 'buildterm/[j]kdesktop' >/dev/null 2>&1 && echo SRV-UP || echo SRV-DOWN") || fail "server state check failed"
WEB_STATE=$($SSH "pgrep -f 'buildterm/[j]kweb' >/dev/null 2>&1 && echo WEB-UP || echo WEB-DOWN") || fail "jkweb state check failed"
echo "server state at entry: $SRV_STATE"
echo "jkweb state at entry: $WEB_STATE"
[ "$SRV_STATE" = "SRV-UP" ] || fail "창 서버가 죽어 있다 — 이 probe는 서버 기동 지점이 아니다(진행 중 육안 게이트 상태 보존 계약)"
[ "$WEB_STATE" = "WEB-UP" ] || fail "jkweb이 죽어 있다 — 수동 복구 후 재실행(서버 보존 계약)"
OL_ENTRY=$($SSH "curl -s -m 5 http://127.0.0.1:11434/api/version" 2>/dev/null) || OL_ENTRY=""
echo "ollama serve at entry: ${OL_ENTRY:-DOWN(필요 시 폰 측 C단계에서 기동 — 기존 프로세스는 끊지 않는다)}"

echo "=== 1. PC 기준선 동일 명령 실측 (best-effort — ollama·claude 있으면 실측) ==="
PC_TIME=""
PC_NOTE=""
PC_ATMPT() { # $1=tag $2=fmt $3=추가 claude 플래그 — 폰 측 attempt()와 같은 체계 (선시험 2026-10-08:
  #           PC a1 stream-json은 "requires --verbose" rc=1, a2(+--verbose)는 result "ok" 4s 실측)
  PC_T0=$(date +%s)
  # cwd는 스크래치(claude CLI 세션 히스토리는 cwd 바인딩 — 저장소 루트 오염 회피)
  # shellcheck disable=SC2086
  ( cd "$SCRATCH" && timeout 180 ollama launch claude --model "$MODEL" -- -p "say ok" --output-format "$2" $3 ) \
    >"$SCRATCH/pc_claude_launch_$1.out" 2>"$SCRATCH/pc_claude_launch_$1.err"
  PC_RC=$?
  PC_ELAPSED=$(( $(date +%s) - PC_T0 ))
  echo "PC attempt $1: fmt=$2${3:+ flags=$3} rc=$PC_RC elapsed=${PC_ELAPSED}s"
  echo "--- PC stdout head:"; head -c 800 "$SCRATCH/pc_claude_launch_$1.out" 2>/dev/null; echo ""
  echo "--- PC stderr head:"; head -c 500 "$SCRATCH/pc_claude_launch_$1.err" 2>/dev/null | tr '\r' '\n'
  grep -aqE '"result":"[^"]' "$SCRATCH/pc_claude_launch_$1.out" 2>/dev/null && PC_AT_OK=1
  return 0   # shellcheck 경고 아님 — judge는 PC_AT_OK로
}
if command -v ollama >/dev/null 2>&1 && command -v claude >/dev/null 2>&1; then
  PC_OL_VER=$(ollama --version 2>/dev/null | grep -aoE '[0-9]+\.[0-9]+\.[0-9]+' | head -1)
  echo "PC ollama $PC_OL_VER / claude $(claude --version 2>/dev/null | head -1)"
  PC_AT_OK=""
  PC_ATMPT a1 stream-json ""
  if [ -z "$PC_AT_OK" ] && grep -aqi 'verbose' "$SCRATCH/pc_claude_launch_a1.err" 2>/dev/null; then
    echo "PC a1이 --verbose 요구 — a2 재시도(폰과 동일 체계)"
    PC_ATMPT a2 stream-json "--verbose"
  fi
  if [ -n "$PC_AT_OK" ]; then
    PC_TIME="$PC_ELAPSED"
    echo "PC-LAUNCH-BASELINE-OK: ${PC_ELAPSED}s (result 도달)"
  else
    PC_NOTE="PC last rc=${PC_RC:-N/A} ${PC_ELAPSED:-0}s result 미도달 — best-effort 스킵(hard receipt가 아니다), 폰 실측 대차 근거로 쓰지 않는다"
    echo "PC-LAUNCH-BASELINE-SKIP: $PC_NOTE"
  fi
else
  echo "PC-LAUNCH-BASELINE-SKIP: PC에 ollama/claude 부재 — 대차 생략"
fi

echo "=== 2. 폰 영수증 절차 생성·전송·실행 (전 로그: $RLOG) ==="
cat > "$SCRATCH/$RSRC_NAME" <<'PHONEEOF'
#!/bin/bash
# 폰 측 영수증 절차 (phone_claude_cli.sh가 생성 — T2). 창 서버·jkweb·ollama serve
# 기존 프로세스는 끊는 지점이 아니다(serve는 죽어 있을 때 기동만 한다).
set -u
PLOG="${TMPDIR:-$HOME/tmp}"     # 폰 /tmp 쓰기 불가 — $TMPDIR(Termux 기본 존재)
RNAME="phone_claude_cli_remote.sh"
PFX="${PREFIX:-/data/data/com.termux/files/usr}"
mkdir -p "$PLOG" "$HOME/JKENGINE" 2>/dev/null
REMNANT() {   # NR1 — 잔존 검사(라벨 경로 전부 여기로 모인다)
  echo "--- 폰 측 원격 스크립트 잔존 검사 (NR1)"
  ls "$HOME/JKENGINE/$RNAME" 2>/dev/null
  echo "REMNANT-LS-RC=$?"
}
# finish/fail: 자기 소각(rm -f $0) → 잔존 검사 → 라벨. (T1 fix r1 계약 승계)
finish() { rm -f -- "$0" 2>/dev/null; REMNANT; echo "CLAUDE-VERDICT: $1"; exit 0; }
fail()   { rm -f -- "$0" 2>/dev/null; REMNANT; echo "CLAUDE-CLI-FAIL: $*"; exit 1; }
HARD()   { echo "CLAUDE-HARD-NOTE: $*"; }
DUPRE()  { du -sk "$PFX" 2>/dev/null | head -1 | cut -f1; }
DUSUM()  { du -sh "$PFX" 2>/dev/null | head -1; }
FREEKB() { df -k "$PFX" 2>/dev/null | tail -1 | awk '{print $4}'; }
command -v curl >/dev/null 2>&1 || fail "폰에 curl 없음 — ollama health 실측 불가"
MODEL="glm-5.3-flash:cloud"

echo "=== A. 폰 ollama 인벤토리 — launch 서브커맨드 ==="
OL_VER=$(curl -s -m 5 http://127.0.0.1:11434/api/version | grep -aoE '[0-9]+\.[0-9]+\.[0-9]+' | head -1)
[ -n "$OL_VER" ] || OL_VER=$(timeout 20 ollama version 2>&1 | grep -aoE '[0-9]+\.[0-9]+\.[0-9]+' | head -1)
echo "ollama version (API 선, CLI 폴백): ${OL_VER:-UNKNOWN}"
HELP=$(timeout 15 ollama --help 2>&1)
if printf '%s' "$HELP" | grep -aqE '^[[:space:]]+launch([[:space:]]|$)'; then
  echo "subcommand launch: PRESENT"
  OLLAMA_LAUNCH=1
else
  echo "subcommand launch: ABSENT"
  OLLAMA_LAUNCH=0
fi
echo "ollama launch claude --help (첫 14행 — 통합 플래그 면):"
timeout 20 ollama launch claude --help 2>&1 | head -14

echo "=== B. 폰 node/claude CLI 인벤토리 ==="
# 진실 원천 = `claude --version` rc(사본 스텁은 PATH에 올라 있어도 기동에 실패한다 —
# 2026-10-08 실측: npm rc=0·bin 스텁 존재·native 미설치). 존재 검사만으로 판정 금지.
claude_check() {
  timeout 60 claude --version >"$PLOG/ph_cc_ver.log" 2>&1
  return $?
}
CLAUDE_BIN=""
CLAUDE_BIN=$(command -v claude 2>/dev/null)
CLAUDE_HEALTHY=0
NODE_BIN=""
NODE_BIN=$(command -v node 2>/dev/null)
echo "claude at entry: ${CLAUDE_BIN:-ABSENT}"
echo "node at entry: ${NODE_BIN:-ABSENT}"
if [ -n "$CLAUDE_BIN" ] && claude_check && [ -n "$CLAUDE_BIN" ]; then
  echo "claude existing healthy: $(head -2 "$PLOG/ph_cc_ver.log" | tr '\r' '\n')"
  CLAUDE_HEALTHY=1
  echo "claude 조달 스킵 — 이미 성립"
elif [ -n "$CLAUDE_BIN" ]; then
  echo "claude 바이너리는 PATH에 있으나 기동 실패 — 조달·봉합 진행"
fi
echo "du $PFX 진입: $(DUSUM) (free=$(FREEKB)KB, $PFX=$(DUPRE)KB)"
DUPRE_NODE_B="$(DUPRE)"     # 용량 실측 기준선(du -sk $PREFIX — pkg/npm -g 설치 모두 $PFX로 간다)

if [ "$CLAUDE_HEALTHY" -eq 1 ] && [ -z "$NODE_BIN" ]; then
  echo "=== C. node 조달 스킵 (claude가 이미 성립 — node 자체 불요) ==="
elif [ -z "$NODE_BIN" ]; then
  echo "=== C. 조달 1단계 — pkg install nodejs-lts (timeout 900) ==="
  C_BEGIN=$(date +%s)
  timeout 900 pkg install -y nodejs-lts >"$PLOG/ph_cc_pkg_node.log" 2>&1
  C_RC=$?
  C_ELAPSED=$(( $(date +%s) - C_BEGIN ))
  DUPRE_NODE_A="$(DUPRE)"
  echo "pkg rc=$C_RC elapsed=${C_ELAPSED}s du델타=$(( DUPRE_NODE_A - DUPRE_NODE_B ))KB log tail:"
  tail -8 "$PLOG/ph_cc_pkg_node.log" | tr '\r' '\n'
  NODE_BIN=""
  NODE_BIN=$(command -v node 2>/dev/null)
  if [ -z "$NODE_BIN" ]; then
    finish "PROCURE-FAIL (원인: pkg install nodejs-lts 실패 rc=$C_RC — 설치 후에도 node 미성립, log tail 위)"
  fi
  echo "NODE-INSTALL-OK: node=$(node --version 2>/dev/null) elapsed=${C_ELAPSED}s du델타=$(( DUPRE_NODE_A - DUPRE_NODE_B ))KB"
else
  echo "=== C. node 조달 스킵 (node가 이미 성립) ==="
fi
if [ -n "$NODE_BIN" ]; then
  echo "node: $NODE_BIN $(node --version 2>/dev/null)"
  echo "du $PFX node 후: $(DUSUM) (free=$(FREEKB)KB, $PFX=$(DUPRE)KB — 진입 ${DUPRE_NODE_B}KB)"
fi

if [ "$CLAUDE_HEALTHY" -eq 0 ]; then
  echo "=== D. 조달 2단계 — npm install -g @anthropic-ai/claude-code (timeout 900) ==="
  DUPRE_CLAUDE_B="$(DUPRE)"
  D_BEGIN=$(date +%s)
  timeout 900 npm install -g @anthropic-ai/claude-code >"$PLOG/ph_cc_npm.log" 2>&1
  D_RC=$?
  D_ELAPSED=$(( $(date +%s) - D_BEGIN ))
  DUPRE_CLAUDE_A="$(DUPRE)"
  echo "npm rc=$D_RC elapsed=${D_ELAPSED}s du델타=$(( DUPRE_CLAUDE_A - DUPRE_CLAUDE_B ))KB ($PFX 진입 ${DUPRE_CLAUDE_B}KB → 후 ${DUPRE_CLAUDE_A}KB)"
  echo "npm log tail:"; tail -10 "$PLOG/ph_cc_npm.log" | tr '\r' '\n'
fi

echo "=== E. claude 기동성 실측 (version — PATH 스텁 존재와 별개의 진실) ==="
CC_MOD="${PFX}/lib/node_modules/@anthropic-ai/claude-code"
claude_check
E_RC=$?
if [ "$E_RC" -eq 0 ]; then
  CLAUDE_HEALTHY=1
  echo "claude healthy: $(head -2 "$PLOG/ph_cc_ver.log" | tr '\r' '\n')"
else
  echo "claude --version rc=$E_RC out:"; head -5 "$PLOG/ph_cc_ver.log" | tr '\r' '\n'
  echo "=== E1. 원인 봉합 — vendor postinstall 수기 실행 + android 플랫폼 레지스트리 ==="
  if [ -f "$CC_MOD/install.cjs" ]; then
    ( cd "$CC_MOD" && timeout 120 node install.cjs ) >"$PLOG/ph_cc_pinstall.log" 2>&1
    PI_RC=$?
    echo "manual postinstall rc=$PI_RC out:"; head -6 "$PLOG/ph_cc_pinstall.log" | tr '\r' '\n'
  else
    echo "install.cjs 부재 — $CC_MOD 구조 변동(npm log 위 참조)"
  fi
  echo "--- registry: android 플랫폼 패키지 존재 여부"
  timeout 60 npm view @anthropic-ai/claude-code-linux-arm64-android version >"$PLOG/ph_cc_view_android.log" 2>&1
  echo "npm view android rc=$? out head:"; head -3 "$PLOG/ph_cc_view_android.log"
  echo "--- registry: musl 플랫폼 패키지 존재 여부"
  timeout 60 npm view @anthropic-ai/claude-code-linux-arm64-musl version >"$PLOG/ph_cc_view_musl.log" 2>&1
  echo "npm view musl rc=$? out head:"; head -3 "$PLOG/ph_cc_view_musl.log"
  echo "=== E2. musl 우회 exec 봉합 (bionic 대체로 musl도 없다 — 두 번째 실행부터 마커 스킵) ==="
  MUSL_MARK="${PLOG}/ph_cc_musl_seal.txt"
  if [ -f "$MUSL_MARK" ]; then
    echo "MUSL-SEAL-SKIP: 이전 봉합 마커 존재:"; cat "$MUSL_MARK"
  else
    MDIR="${TMPDIR}/ph_cc_musl"
    rm -rf -- "$MDIR" 2>/dev/null
    mkdir -p "$MDIR"
    cd "$MDIR" || HARD "musl scratch cd 실패 — 봉합 스킵"
    if [ "$(pwd)" = "$MDIR" ]; then
      M_T0=$(date +%s)
      timeout 600 npm pack @anthropic-ai/claude-code-linux-arm64-musl >pack.log 2>&1
      M_RC=$?
      M_ELAPSED=$(( $(date +%s) - M_T0 ))
      echo "npm pack rc=$M_RC elapsed=${M_ELAPSED}s (tgz):"; ls -la ./*.tgz 2>/dev/null
      M_BIN=""
      if [ "$M_RC" -eq 0 ]; then
        tar -xf ./*.tgz 2>/dev/null
        M_BIN=$(find . -maxdepth 3 -name claude -type f 2>/dev/null | head -1)
      fi
      if [ -n "$M_BIN" ]; then
        chmod +x "$M_BIN"
        command -v file >/dev/null 2>&1 && file "$M_BIN" || echo "file cmd 미보유 — exec 거부 메시지 자체가 근거"
        M_OUT=$(timeout 30 "$MDIR/$M_BIN" --version 2>&1)
        M_EXEC_RC=$?
        echo "musl exec rc=$M_EXEC_RC out+err: ${M_OUT:-(empty)}"
        printf 'musl exec rc=%s out: %s\n' "$M_EXEC_RC" "${M_OUT:-(empty)}" > "$MUSL_MARK"
        HARD "musl 봉합 영수증 마커 저장: $MUSL_MARK"
      else
        printf 'musl tar에서 claude 바이너리 미발견 (pack rc=%s)\n' "$M_RC" > "$MUSL_MARK"
        echo "musl tar에서 claude 바이너리 미발견 — 봉합 마커만 저장"; tail -5 pack.log 2>/dev/null
      fi
      cd "$HOME" || true
      rm -rf -- "$MDIR" 2>/dev/null
      echo "MUSL-SCRATCH-CLEANED (tgz+바이너리 회수)"
    fi
  fi
fi

echo "=== F. ollama serve 상태 (끊지 않는다 — 죽어 있으면 기동만) ==="
V=$(curl -s -m 5 http://127.0.0.1:11434/api/version)
if [ -n "$V" ]; then
  echo "OLLAMA-SERVE-ALREADY-UP: $V"
else
  echo "serve DOWN — nohup ollama serve (기존 kill 없음)"
  nohup ollama serve >"$PLOG/ph_cc_serve.log" 2>&1 &
  V=""
  for i in 1 2 3 4 5 6 7 8; do
    V=$(curl -s -m 5 http://127.0.0.1:11434/api/version)
    [ -n "$V" ] && break
    sleep 2
  done
  if [ -n "$V" ]; then
    echo "OLLAMA-SERVE-UP: health reply = $V"
  else
    echo "--- serve log tail:"; tail -20 "$PLOG/ph_cc_serve.log" 2>/dev/null
    finish "PROCURE-FAIL (원인: ollama serve 미성립 — serve log 위)"
  fi
fi

echo "=== G. ollama list — $MODEL 성립 여부 (없으면 pull 레퍼런스 등록) ==="
timeout 30 ollama list >"$PLOG/ph_cc_list_pre.log" 2>&1
echo "ollama list rc=$? (head 12):"; head -12 "$PLOG/ph_cc_list_pre.log"
CLOUD_IN_LIST=0
CLOUD_IN_LIST=$(grep -ac 'glm-5.3-flash:cloud' "$PLOG/ph_cc_list_pre.log")  # 0건이면 0 인쇄·rc=1 — set -e 없어 무해, 값만 쓴다
echo "glm-5.3-flash:cloud in list: $CLOUD_IN_LIST"
if [ "$CLOUD_IN_LIST" -eq 0 ]; then
  echo "--- pull $MODEL (cloud 모델 — blob 없이 레퍼런스 등록, timeout 180)"
  timeout 180 ollama pull "$MODEL" >"$PLOG/ph_cc_pull.log" 2>&1
  PL_RC=$?
  echo "pull rc=$PL_RC log tail:"; tail -5 "$PLOG/ph_cc_pull.log" | tr '\r' '\n'
  HARD "pull rc=$PL_RC 인가 계열 판정은 T1 원장(폰 인가 이미 성립) — 미성립이어도 launch 턴 시도로 진행"
else
  echo "pull 불요 — list에 이미 성립"
fi

if [ "$OLLAMA_LAUNCH" -eq 0 ]; then
  finish "NO-CLAUDE-CLI (원인: ollama($OL_VER)에 launch 서브커맨드 부재 — launch-claude 경로 불가, claude CLI 조달/A-E 영수증은 위)"
fi

echo "=== H. ollama launch claude 턴 실측 (timeout 240/시도 — OAuth 브라우저 정체 절사) ==="
# 시도 체계: a1=brief 원문(stream-json) → stderr가 --verbose 요구면 a2=+--verbose →
# 그래도 미성립이면 a3=--output-format json(jkchat 관례, docs/31 §6). 성공 판정은
# stdout에 비공백 "result" + rc=0. bare launch TUI(rc=0) 함정 회피 — 항상 인자 개입.
AT_OK=""
AT_TIME=""
AT_TAG=""
attempt() { # $1=tag  $2=output-format  $3=claude 추가 플래그(공백 분리 word-split 의도)
  A_TAG="$1"
  A_BEGIN=$(date +%s)
  # shellcheck disable=SC2086
  timeout 240 ollama launch claude --model "$MODEL" -- -p "say ok" --output-format "$2" $3 \
    >"$PLOG/ph_cc_la_$1.out" 2>"$PLOG/ph_cc_la_$1.err"
  A_RC=$?
  A_ELAPSED=$(( $(date +%s) - A_BEGIN ))
  echo "--- attempt $A_TAG: fmt=$2${3:+ flags=$3} rc=$A_RC elapsed=${A_ELAPSED}s"
  echo "  stdout head:"; head -c 1600 "$PLOG/ph_cc_la_$1.out" 2>/dev/null; echo ""
  echo "  stderr head:"; head -c 1200 "$PLOG/ph_cc_la_$1.err" 2>/dev/null | tr '\r' '\n' | head -12
  RES=$(grep -aoE '"result":"[^"]+' "$PLOG/ph_cc_la_$1.out" 2>/dev/null | head -1)
  echo "  result extract: ${RES:-NONE}"
  if [ "$A_RC" -eq 0 ] && [ -n "$RES" ]; then
    AT_OK="$A_TAG"; AT_TIME="$A_ELAPSED"
  fi
}
attempt a1 stream-json ""
if [ -z "$AT_OK" ] && [ "$CLAUDE_HEALTHY" -eq 1 ] && grep -aqi 'verbose' "$PLOG/ph_cc_la_a1.err" 2>/dev/null; then
  echo "a1 stderr가 --verbose를 요구 — a2 재시도"
  attempt a2 stream-json "--verbose"
fi
if [ -z "$AT_OK" ] && [ "$CLAUDE_HEALTHY" -eq 1 ]; then
  echo "마지막 재시도 — --output-format json (jkchat 관례 docs/31 §6)"
  attempt a3 json ""
fi
if [ -n "$AT_OK" ]; then
  echo "LAUNCH-RESULT-TEXT: $(grep -aoE '"result":"[^"]*' "$PLOG/ph_cc_la_${AT_OK}.out" 2>/dev/null | head -1 | head -c 200)"
  echo "PC 대차: ${PC_TIME:+PC동일명령 ${PC_TIME}s / }폰 launch 턴 ${AT_TIME}s (attempt $AT_OK)"
  finish "LAUNCH-OK(${AT_TIME}s, attempt=${AT_OK}${PC_TIME:+, PC ${PC_TIME}s})"
fi
LAST_RC="${A_RC:-N/A}"
if [ "$CLAUDE_HEALTHY" -eq 0 ]; then
  finish "NO-CLAUDE-CLI (원인: claude CLI 기동 실패 — vendor postinstall 수기 영수증(E1)·android 플랫폼 404·musl exec 거부 봉합(E2) 위 참조; ollama launch의 claude 부재 취급은 H단계 a1 원문)"
fi
finish "NO-CLAUDE-CLI (원인: ollama launch claude 턴 미성립 — last rc=${LAST_RC}, stderr 원문은 위 H단계; OAuth/브라우저·onboarding 정체이면 stderr에 흔적)"
PHONEEOF

# 폰 측 절차는 항상 이 시점에 전송(스크립트 파일 경유 — pkill 브래킷 자기제거 트랩 회피).
tar -cf "$SCRATCH/phone_claude_cli.tar" -C "$SCRATCH" "$RSRC_NAME" || fail "tar (remote script) failed"
$SSH "tar -xf - -C ~/JKENGINE" < "$SCRATCH/phone_claude_cli.tar" || fail "remote tar extract failed"
rm -f "$SCRATCH/phone_claude_cli.tar"
$SSH "stat -c '%n %s' ~/JKENGINE/$RSRC_NAME" || fail "remote receipt script missing on phone"

SECONDS=0
$SSH "bash ~/JKENGINE/$RSRC_NAME" > "$RLOG" 2>&1
R_RC=$?
cat "$RLOG"
PROBE_ELAPSED=$SECONDS
echo "phone-side receipt script rc=$R_RC duration=${PROBE_ELAPSED}s"
[ "$R_RC" -eq 0 ] || fail "phone-side receipt script rc=$R_RC (see above)"
grep -aq '^CLAUDE-VERDICT:' "$RLOG" || fail "receipt script emitted no CLAUDE-VERDICT line"
if grep -aq 'CLAUDE-CLI-FAIL' "$RLOG"; then fail "receipt script emitted CLAUDE-CLI-FAIL"; fi
# NR1 — 드라이버 측 2차 확인(원격 소각 후 잔존 무결)
echo "--- 드라이버 측 잔존 2차 확인"
$SSH "ls ~/JKENGINE/$RSRC_NAME 2>&1; echo REMNANT-LS-RC=\$?"

echo ""
echo "=== 3. 종료 상태 — 서버 UP + jkweb 유지(끊지 않는다) ==="
$SSH 'command -v termux-wake-lock >/dev/null 2>&1 && termux-wake-lock >/dev/null 2>&1; true'
P2=$($SSH "pgrep -f 'buildterm/[j]kdesktop' | tr '\n' ' '") || fail "end-state server check failed"
[ -n "$P2" ] || fail "end-state: server not alive"
echo "server pids: $P2"
JW2=$($SSH "pgrep -f 'buildterm/[j]kweb' | tr '\n' ' '") || fail "end-state jkweb check failed"
[ -n "$JW2" ] || fail "end-state: jkweb not alive"
echo "jkweb pids: $JW2"
OL_END=$($SSH "curl -s -m 5 http://127.0.0.1:11434/api/version" 2>/dev/null) || OL_END=""
echo "ollama serve at exit: ${OL_END:-DOWN}"
grep -a 'CLAUDE-VERDICT' "$RLOG"
rm -f "$SCRATCH/$RSRC_NAME"
echo "PHONE-CLAUDE-CLI-OK"
exit 0
# EOF — 끝 개행 유지(스크립트 산술 무해 습관).
