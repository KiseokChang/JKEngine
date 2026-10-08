#!/usr/bin/env bash
# T1 Step 2 — 폰 A/B 재현 probe (스펙 2026-10-09-phone-text-scale).
#
# 목표: 폰 클라 글리프가 font_scale을 따르지 않는(비트맵 고정) 결함의
# 레그 조건 단정. wsl_text_scale_diag.sh(T1 WSL leg)와 한 벌.
#
#   legP19 — settings text.font_path 유 + font_scale "1.9":
#            결함 재현(캡처=engine/tmp/tsd_p19.png, 기존 영수증 desk_s19 대조).
#            예상 클라 경고 0건(Init 성공) → 후보②③ 배제 원료.
#   legX19 — settings font_path 제거(font_scale 1.9 유지):
#            클라 경고 "no vector font configured"가 서버 로그에 나오면
#            = 클라가 같은 settings.json을 직독했다는 계약 증명(후보① 배제)
#            + 글리프 픽셀은 legP19와 동일한 비트맵 → 그리기 경로가 아틀라스를
#            전혀 상담하지 않는다는(후보⑤) 단정 원료.
#   legR   — settings 원본 복원(부팅 전 저장해 둔 워터마크 복사의 byte 원본)
#            + 기동 확인(BOOT-OK: 서버+taskbar+minesweeper) — 사용자 기상
#            전제 복구. probe가 결제를 기록하지 않는다(육안은 사용자).
#
# 표준(phone_dirty_present.sh 선례 승계): PHONE_HOST 환경변수 필수(내부 IP는
# 커밋·문서·리포트에 두지 않는다), x11grab(1920x1080 시도 → 화면 크기 파싱
# 재시도 — 폰 실측 1920x1005), ImageMagick import 금지(폰 libheif 파단),
# 원격 스크립트 자가 소각(rm -f -- "$0")+REMNANT-LS-RC=2 기대, 브래킷 pkill
# (원격 스크립트는 $TMPDIR 파일 기동 — argv에 jkdesktop 문자열 못 들이는
# 사건 원장 계약), ssh 원격 `< /dev/null` 금지, 캡처 회수는 ssh 호출당
# base64 1파이프(ls 등 다른 출력 혼입 금지 — 사건 원장), 임시는 $TMPDIR.
#
# 실행법(윈도 Git Bash):
#   PHONE_HOST=<폰 IP> bash engine/tools/probes/phone_text_scale_ab.sh
# 풀 로그: engine/tmp/tsd_ab_driver.log(tee).
set -u
export MSYS_NO_PATHCONV=1

ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
cd "$ROOT" || exit 1
SCRATCH="$ROOT/engine/tmp"
mkdir -p "$SCRATCH"
RLOG="$SCRATCH/tsd_ab_driver.log"
RSRC_NAME="tsd_phone_remote.sh"
RSRC_PHONE="~/JKENGINE/$RSRC_NAME"

PHONE_HOST="${PHONE_HOST:-}"
PHONE_PORT="${PHONE_PORT:-8022}"
PHONE_USER="${PHONE_USER:-u0_a4}"
PHONE_KEY="${PHONE_KEY:-$HOME/.ssh/termux_jkengine}"
PHONE_DISPLAY="${PHONE_DISPLAY:-:1}"
[ -n "$PHONE_HOST" ] || { echo "TSD-FAIL: PHONE_HOST unset — 폰 IP는 환경변수로(내부 IP는 파일에 두지 않는다)"; exit 1; }
SSH="ssh -p $PHONE_PORT -o BatchMode=yes -o ConnectTimeout=15 -o ServerAliveInterval=15 -o ServerAliveCountMax=10 -i $PHONE_KEY $PHONE_USER@$PHONE_HOST"

exec > >(tee "$RLOG") 2>&1
FAIL() { echo "TSD-FAIL: $*"; exit 1; }
echo "HEAD: $(git -C "$(cygpath -w "$ROOT" 2>/dev/null || echo "$ROOT")" rev-parse HEAD 2>/dev/null || echo rev-parse-failed)"

cat > "$SCRATCH/$RSRC_NAME" <<'TSDEOF'
#!/bin/bash
# 폰 측 영수증 절차 (phone_text_scale_ab.sh가 생성 — T1 Step 2).
# settings 원본을 워터마크 복사로 보존하고 3레그 기동+캡처를 자행한다.
# 진입 상태: 서버+taskbar+minesweeper UP — 종료 상태: 동일 원복 (BOOT-OK).
set -u
cd ~/JKENGINE/engine || exit 1
FAIL() { echo "TSD-FAIL: $*"; exit 1; }
TMPD="${TMPDIR:-/data/data/com.termux/files/usr/tmp}"
SET=buildterm/state/settings.json
rm -f "$TMPD/tsd_orig_settings.json"
cp "$SET" "$TMPD/tsd_orig_settings.json" 2>/dev/null \
    || cp "$SET" "$TMPD/tsd_orig_settings.json" || FAIL "settings backup failed"
ORIG="$TMPD/tsd_orig_settings.json"
echo "ORIG-SETTINGS: $(tr -d '\n' < "$ORIG")"
FONT_PATH=$(sed -n 's/.*"font_path"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' "$ORIG")
echo "PARSED-FONT_PATH: $FONT_PATH"
[ -n "$FONT_PATH" ] || FAIL "settings has no text.font_path (레그 설계 상 font_path 전제)"
[ -r "$FONT_PATH" ] || FAIL "font file not readable: $FONT_PATH"

CAPTURE() { # $1=출력png — x11grab 1920x1080 시도, 화면 크기 파싱 재시도
    local OUT=$1
    rm -f "$OUT"
    ffmpeg -hide_banner -loglevel error -f x11grab -video_size 1920x1080 \
        -framerate 1 -i :1 -frames:v 1 "$OUT" >/dev/null 2>&1
    if [ ! -s "$OUT" ]; then
        SCR=$(ffmpeg -hide_banner -loglevel error -f x11grab -video_size 1920x1080 \
            -framerate 1 -i :1 -frames:v 1 /dev/null 2>&1 \
            | grep -aoE 'screen size [0-9]+x[0-9]+' | head -1 | sed 's/screen size //')
        [ -n "$SCR" ] || SCR=1920x1005
        echo "CAPTURE-SIZE-ADJUST: $SCR"
        ffmpeg -hide_banner -loglevel error -f x11grab -video_size "$SCR" \
            -framerate 1 -i :1 -frames:v 1 "$OUT" >/dev/null 2>&1
    fi
    [ -s "$OUT" ] || FAIL "capture failed: $OUT"
    echo "CAPTURE-OK: $OUT ($(wc -c < "$OUT") bytes)"
}

wait_mine() { # Minesweeper 창 등장 대기 — 폰 클라 스폰 15-18s 원장, 3s×20
    MW=""
    for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20; do
        WIN=$(timeout 15 ./buildterm/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)
        MW=$(printf '%s' "$WIN" | grep -aoE '\{"id":[^}]*"title":"Minesweeper"[^}]*\}' | head -1)
        [ -n "$MW" ] && break
        sleep 3
    done
    [ -n "$MW" ] || FAIL "no Minesweeper window (폰 스폰 15-18s 원장 초과) — last: ${WIN:-none}"
    echo "MINEWINDOW: $MW"
}

boot_leg() { # $1 라벨
    LEG=$1
    LOG="$TMPD/tsd_srv_${LEG}.log"
    echo "=== leg $LEG boot ==="
    pkill -f '[j]kdesktop' 2>/dev/null
    sleep 2
    pkill -9 -f '[j]kdesktop' 2>/dev/null
    sleep 1
    env DISPLAY=:1 setsid nohup ./buildterm/jkdesktop --server >"$LOG" 2>&1 &
    sleep 8
    SRVPID=$(pgrep -f 'buildterm/[j]kdesktop --server' | head -1)
    [ -n "$SRVPID" ] || { echo "--- log tail:"; tail -5 "$LOG"; FAIL "leg $LEG: no server 8s after boot"; }
    echo "leg $LEG: server pid=$SRVPID"
    PING=""
    for i in 1 2 3 4 5; do
        PING=$(timeout 15 ./buildterm/jkdesktop agentctl '{"tool":"ping","args":{}}' 2>/dev/null | grep -a '{' | head -1)
        printf '%s' "$PING" | grep -aq '"ok":true' && break
        sleep 2
    done
    printf '%s' "$PING" | grep -aq '"ok":true' || FAIL "leg $LEG: ping did not ok — $PING"
    sleep 10   # taskbar 자동 스폰 관측 여유(진입 상태가 taskbar UP이었다)
    WIN=$(timeout 15 ./buildterm/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)
    printf '%s' "$WIN" | grep -aq '"title":"Taskbar"' || {
        T=$(timeout 20 ./buildterm/jkdesktop agentctl '{"tool":"launch_app","args":{"app":"taskbar"}}' 2>/dev/null | grep -a '{' | head -1)
        echo "TASKBAR-LAUNCH-RECOVED: $T"
    }
    M=$(timeout 20 ./buildterm/jkdesktop agentctl '{"tool":"launch_app","args":{"app":"minesweeper"}}' 2>/dev/null | grep -a '{' | head -1)
    printf '%s' "$M" | grep -aq '"ok":true' || FAIL "leg $LEG: launch_app minesweeper failed — $M"
    wait_mine
    sleep 6   # settle — 타이머 렌더 폴백 이후 화면 캡처
    echo "--- leg $LEG 클라/서버 stderr 경고 수집(atlas 3종+HangulManager) ---"
    grep -aE 'Warning: (no vector font|vector font init|fallback font|HangulManager)' "$LOG" \
        || echo "(no font warnings in leg $LEG log)"
    CAPTURE "$TMPD/tsd_${LEG}.png"
    echo "SETTINGS-IN-LEG-$LEG: $(cat "$SET" 2>/dev/null | tr -d '\n')"
}

echo "=== A. legP19 — font_path 유 + font_scale 1.9 ==="
printf '{\n    "text": {\n        "font_path": "%s",\n        "font_scale": "1.9"\n    }\n}\n' "$FONT_PATH" > "$SET"
cat "$SET"
boot_leg legP19

echo "=== B. legX19 — font_path 제거 + font_scale 1.9 (클라 settings 직독 계약 증명) ==="
printf '{\n    "text": {\n        "font_scale": "1.9"\n    }\n}\n' > "$SET"
cat "$SET"
boot_leg legX19

echo "=== C. legR — settings 원본 복원 + 기동 확인 (BOOT-OK) ==="
cp "$ORIG" "$SET" || FAIL "settings restore copy failed"
cmp -s "$ORIG" "$SET" || FAIL "settings restore byte mismatch"
echo "SETTINGS-RESTORED-BYTES: $(wc -c < "$SET")"
boot_leg legR

echo "=== D. 최종 상태 게이트 — 서버 UP + taskbar/minesweeper 상시 ==="
pgrep -f 'buildterm/[j]kdesktop --server' >/dev/null 2>&1 || FAIL "server not UP at end"
FINAL=$(timeout 15 ./buildterm/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)
echo "FINAL-WINDOWS: $FINAL"
rm -f -- "$0"
echo "TSD-FINISHED"
exit 0
TSDEOF

echo "=== 1. preflight (진입 창 상태 기록 — 원복 대조 원료) ==="
$SSH 'echo PHONE-REACHABLE; echo TMPDIR=$TMPDIR; uname -m' || FAIL "ssh failed (phone unreachable)"
$SSH "cd ~/JKENGINE/engine 2>/dev/null && timeout 15 ./buildterm/jkdesktop agentctl '{\"tool\":\"list_windows\",\"args\":{}}' 2>/dev/null | head -1" \
    || echo "NOTE: entry list_windows probe failed"
echo "ENTRY-WINDOWS-ABOVE"
$SSH "test -f ~/JKENGINE/engine/buildterm/state/settings.json && echo SETTINGS-PRESENT || { echo SETTINGS-ABSENT; exit 1; }" \
    | grep -aq SETTINGS-PRESENT || FAIL "target settings file missing on phone"

echo "=== 2. 원격 레그 절차 전송+실행 (3레그 — 캡처 포함) ==="
scp -P "$PHONE_PORT" -o BatchMode=yes -i "$PHONE_KEY" \
    "$SCRATCH/$RSRC_NAME" "$PHONE_USER@$PHONE_HOST:$RSRC_PHONE" \
    || FAIL "remote script upload (scp) failed"
$SSH "bash $RSRC_PHONE" > "$SCRATCH/tsd_phone_run.log" 2>&1
R_RC=$?
cat "$SCRATCH/tsd_phone_run.log"
[ "$R_RC" -ne 255 ] || echo "NOTE-RUN-DROPPED: ssh 채널 단절 — 재실행 증분 권장"
grep -aq '^TSD-FINISHED' "$SCRATCH/tsd_phone_run.log" \
    || FAIL "remote receipt script did not finish (rc=$R_RC)"
if grep -aq '^TSD-FAIL' "$SCRATCH/tsd_phone_run.log"; then
    FAIL "phone hard receipt: $(grep -a '^TSD-FAIL' "$SCRATCH/tsd_phone_run.log" | head -1)"
fi

echo "=== 3. REMNANT 검사 — 원격 스크립트 자기 소각 확인 (REMNANT-LS-RC=2 기대) ==="
$SSH "ls $RSRC_PHONE" >/dev/null 2>&1
echo "REMNANT-LS-RC=$?"

echo "=== 4. 캡처 회수 — ssh 호출당 base64 1파이프 계약 ==="
recover() { # $1 폰쪽png 경로 $2 로컬png
    local REMOTE_PNG=$1 LOCAL_PNG=$2
    local WTXT WPNG
    WTXT=$(cygpath -w "$SCRATCH/tsd_b64.txt" 2>/dev/null || echo "$SCRATCH/tsd_b64.txt")
    WPNG=$(cygpath -w "$LOCAL_PNG" 2>/dev/null || echo "$LOCAL_PNG")
    # 실측 함정: 윈도 python은 MSYS식 /i/... 패스를 못 읽는다(사건 원장) —
    # bash 리다이렉션(쓰기)과 python(읽기)의 패스 해석이 달라 회수 유실 1회.
    $SSH "base64 -w0 $REMOTE_PNG" > "$SCRATCH/tsd_b64.txt" 2>"$SCRATCH/tsd_b64.err" \
        || FAIL "recover failed: $REMOTE_PNG"
    [ -s "$SCRATCH/tsd_b64.err" ] && echo "NOTE: stderr noise — $(tail -1 "$SCRATCH/tsd_b64.err")"
    python - "$WTXT" "$WPNG" <<'PYEOF' || FAIL "base64 decode failed: $LOCAL_PNG"
import base64, re, sys
d = open(sys.argv[1], "rb").read().decode("ascii", "ignore")
d = re.sub(r"\s+", "", d)
open(sys.argv[2], "wb").write(base64.b64decode(d))
print("RECOVER-OK:", sys.argv[2])
PYEOF
    [ -s "$LOCAL_PNG" ] || FAIL "recovered png empty: $LOCAL_PNG (폰쪽 삭제 전 확인 — 회수 실패 시 폰 copy 남겨두기)"
    rm -f "$SCRATCH/tsd_b64.txt"
}
recover "\$TMPDIR/tsd_legP19.png" "$SCRATCH/tsd_p19.png"
recover "\$TMPDIR/tsd_legX19.png" "$SCRATCH/tsd_x19.png"
recover "\$TMPDIR/tsd_legR.png"   "$SCRATCH/tsd_r.png"

$SSH "rm -f \$TMPDIR/tsd_legP19.png \$TMPDIR/tsd_legX19.png \$TMPDIR/tsd_legR.png \$TMPDIR/tsd_orig_settings.json"
echo "TSD-AB-END"
exit 0