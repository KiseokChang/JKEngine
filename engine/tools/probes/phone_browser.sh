#!/usr/bin/env bash
# 폰 브라우저 배포+측정 수확 probe (#123 폰 브라우저 T4).
# 스펙: docs/superpowers/specs/2026-11-01-phone-browser-webview-design.md §2 T4.
# 관문 승계 원존: phone_apk.sh(#121 T3 확정판 — PHONE_HOST 셀프판명·bytes+md5
# 등호 hard 게이트·scp 실패 시 cat-over-ssh 페일오버·설치 = D5 사용자 속 1회
# 절사 금지·주소 미인쇄 계약·stdout 수취 요약 스트림).
# 영수증 목표:
#  ① APK 폰 반입(engine/android/jkapk/app/build/outputs/apk/debug/app-debug.apk
#     → /sdcard/Download/jkapk-debug.apk, bytes+md5 등호) — 설치 안내 인쇄만
#     (probe는 pm/adb install 절사 금지 — D5).
#  ② 측정 JSON 수확: /sdcard/Download/jkbrowser_measure_*.json → engine/tmp/
#     browser_measure/ 로 1개씩 cat-over-ssh 수취 + md5 등어 게이트 각각.
#     측정 JSON은 사용자가 APK [측정] 버튼 누른 뒤 산출물 — 폰 Download에는
#     남겨 둔다(사용자 데이터 무삭제 원존 — 잔상 소거는 probe가 만든 것만).
#  ③ 요약 스트림: BROWSER-RUN-VALS 1행 + PHONE-BROWSER-OK / PHONE-BROWSER-FAIL.
#  폰 미도달 = 정직 BLOCKED 원장행(갱신 68 폰 원문 승계 대기 동형).
#  실행법(윈도 Git Bash, 저장소 루트 어디서든):
#     BROWSER_RUN=1 bash engine/tools/probes/phone_browser.sh
set -u
export MSYS_NO_PATHCONV=1  # MSYS가 ssh/scp 인수의 posix 경로를 찢는 것 차단

ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
cd "$ROOT" || exit 1
SCRATCH="$ROOT/engine/tmp/browser_measure"
mkdir -p "$SCRATCH"
RLOG="$SCRATCH/phone_browser_run${BROWSER_RUN:-1}.log"

APK_LOCAL="$ROOT/engine/android/jkapk/app/build/outputs/apk/debug/app-debug.apk"
# 조율 계약(#123 2026-11-01 — jkengine-89 세션 요청): 사용자 설치 가능 판
# 덮어쓰기 전 사전 통지·시험용은 별도 파일명 → env로 원격 파일명 교체 가능.
# 기본은 jkapk-debug.apk(기존 계약 유지 — 런처/사용자 안내 계약 축).
PHONE_BROWSER_APK_REMOTE="${PHONE_BROWSER_APK_REMOTE:-/sdcard/Download/jkapk-debug.apk}"
APK_REMOTE="$PHONE_BROWSER_APK_REMOTE"
RUN_N=${BROWSER_RUN:-1}

PHONE_HOST="${PHONE_HOST:-}"
PHONE_PORT="${PHONE_PORT:-8022}"
PHONE_USER="${PHONE_USER:-u0_a4}"
PHONE_KEY="${PHONE_KEY:-$HOME/.ssh/termux_jkengine}"
# 셀프판명(phone_apk.sh 원존 블록 — 주소 인쇄 0)
if [ -z "$PHONE_HOST" ]; then
  CANDS=$(awk '{split($1,a,","); for(k in a) print a[k]}' "$HOME/.ssh/known_hosts" 2>/dev/null |
          sed 's/^\[//;s/\]:.*$//;s/:.*$//' | sort -u)
  for c in $CANDS; do
    case "$c" in \|1\|*) continue;; esac
    out=$(ssh -p "$PHONE_PORT" -o BatchMode=yes -o ConnectTimeout=6 \
          -i "$PHONE_KEY" "$PHONE_USER@$c" 'echo SELFRESOLVE-OK' 2>/dev/null)
    if printf '%s' "$out" | grep -aq SELFRESOLVE-OK; then
      PHONE_HOST="$c"; echo "SELFRESOLVE: MATCH (주소 미인쇄 계약)"; break
    fi
  done
fi
[ -n "$PHONE_HOST" ] || {
  echo "#123-T4 런 BLOCKED 원장행: 폰 미접속(PHONE_HOST env 없음·known_hosts 후보 BatchMode 전원 MISS)"
  echo "PHONE-BROWSER-FAIL: 폰 미접속 — 폰 원문 승계 대행 원장(갱신 68 동형)"
  exit 1
}
SSH="ssh -p $PHONE_PORT -o BatchMode=yes -o ConnectTimeout=15 -o ServerAliveInterval=15 -o ServerAliveCountMax=10 -i $PHONE_KEY $PHONE_USER@$PHONE_HOST"
SCPCMD=(scp -P "$PHONE_PORT" -o BatchMode=yes -o ConnectTimeout=15 -i "$PHONE_KEY")

FAIL() { echo "PHONE-BROWSER-FAIL: $*"; exit 1; }

exec > >(tee "$RLOG") 2>&1
echo "PHONE-BROWSER-START $(date +%F' '%H:%M:%S) run=$RUN_N (폰 주소 미인쇄 계약)"

# ── 1. APK 폰 반입(bytes+md5 등어 hard 게이트 — 설치는 D5 사용자 속 1회) ──
[ -f "$APK_LOCAL" ] || FAIL "로컬 APK 부재 — assembleDebug 먼저(#123 T1-T3)"
LB=$(stat -c %s "$APK_LOCAL"); [ "$LB" -gt 0 ] || FAIL "로컬 APK 0바이트"
LM=$(md5sum "$APK_LOCAL" | cut -d" " -f1)
if ! "${SCPCMD[@]}" "$APK_LOCAL" "$PHONE_USER@$PHONE_HOST:$APK_REMOTE" 2>"$SCRATCH/phone_browser_scp.err"; then
  echo "--- scp 원문 오류(페일오버 cat-over-ssh 전 원문 보존):"
  cat "$SCRATCH/phone_browser_scp.err"
  cat "$APK_LOCAL" | $SSH "cat > $APK_REMOTE" || FAIL "APK 반입 실패(scp+cat 양축)"
  echo "APK-DEPLOY-PATH=cat-over-ssh (scp 실패 — 원문 위)"
else
  echo "APK-DEPLOY-PATH=scp rc=0"
fi
RB=$($SSH "stat -c %s $APK_REMOTE") || FAIL "폰 APK stat 실패"
RM=$($SSH "md5sum $APK_REMOTE" | awk "{print \$1}") || FAIL "폰 APK md5 수취 실패"
[ "$RB" = "$LB" ] || FAIL "APK bytes 불등호 — local=$LB remote=$RB"
[ "$RM" = "$LM" ] || FAIL "APK md5 불등호 — local=$LM remote=$RM"
echo "APK-DEPLOY-OK bytes=$LB md5=$LM (양측 등어)"
echo "D5-INSTALL-NOTE: 설치는 사용자 속 1회 — 기기에서 설치 후 [측정] 누르면 본 probe 재실행으로 수확."
echo "D5-INSTALL-NOTE: (기존 설치판 갱신 시 adb 없이 '패키지 설치 프로그램>알려진 앱 표시 허용' 시스템 프롬프트 따름)"

# ── 2. 측정 JSON 수확(md5 등어 게이트 각각 — 사용자 데이터 무삭제) ──
LIST=$($SSH 'ls /sdcard/Download/jkbrowser_measure_*.json 2>/dev/null') || LIST=""
HARVESTED=0
F_JSONS=""
for f in $LIST; do
  base=$(basename "$f")
  LMJ=$($SSH "md5sum $f" | awk "{print \$1}") || FAIL "측정 md5 수취 실패 — $f"
  # scp 1순위, 실패 시 cat-over-ssh 페일오버(phone_apk.sh 원존 패턴 승계)
  if ! "${SCPCMD[@]}" "$PHONE_USER@$PHONE_HOST:$f" "$SCRATCH/$base" 2>/dev/null; then
    $SSH "cat $f" > "$SCRATCH/$base" || FAIL "측정 수취 실패(scp+cat 양축) — $f"
    echo "HARVEST-PATH=cat-over-ssh — $base"
  else
    echo "HARVEST-PATH=scp rc=0 — $base"
  fi
  LMJ2=$(md5sum "$SCRATCH/$base" | cut -d" " -f1)
  [ "$LMJ" = "$LMJ2" ] || FAIL "측정 md5 불등어 — $f local=$LMJ2 phone=$LMJ"
  HARVESTED=$((HARVESTED+1))
  F_JSONS="$F_JSONS $base"
  echo "HARVEST-OK: $base md5=$LMJ"
done
[ "$HARVESTED" -gt 0 ] || \
  echo "HARVEST-NONE: 폰 Download에 jkbrowser_measure_*.json 부재 — 사용자 [측정] 실측 전(정직 결측)"

# ── 3. 종료 상태 + 요약 스트림(stdout 수취 계약 — APK-RUN-VALS 승계) ──
$SSH 'pgrep -f "buildterm/[j]kdesktop" >/dev/null && echo END-SRV-UP || echo END-SRV-DOWN' \
  || FAIL "폰 상태 판정 ssh 실패"
echo "BROWSER-RUN-VALS: run=$RUN_N | APK-DEPLOY=OK | APK-BYTES=$LB | APK-MD5-EQ=OK | MEASURE-HARVEST=$HARVESTED$F_JSONS | OUTDIR=engine/tmp/browser_measure | STATUS=PHONE-BROWSER-OK"
echo "PHONE-BROWSER-OK (run $RUN_N — 2런은 BROWSER_RUN=2 재실행)"