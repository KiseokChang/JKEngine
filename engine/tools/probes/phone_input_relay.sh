#!/usr/bin/env bash
# 폰 입력 릴레이 수리 실측 probe (#96 r1 — fit-to-display+유실 분절).
#
# 승계 원천:
#   · phone_music.sh(music 라인 T4/T5) — PHONE_HOST fail-closed 가드(exec 앞)
#     ·전수 sweep canary(HEAD blob 크기 대차·SWEEP-DIFF·기대 집합 분류·
#     SWEEP-POST-DIFF=0 hard 게이트)·원격 스크립트=파일+자기 소각·REMNANT rc
#     게이트·브래킷 pkill+-9 에스컬레이션+잔존 게이트(서버 재기동 계약 —
#     #96 배치 상위 승인)·jkweb 상시 유지(절사 금지)·x11grab 캡처(1920x1005
#     폰 실측 화면)·base64 1파이프 회수+md5 대차·permissions 병합+바이트
#     원복(선존 런타임 파일 존중 — 프로브 소유 아님 계약)·원격 `< /dev/null`
#     금지·rc 봉합 exit 전파.
#   · phone-input-spike-report.md(.superpowers/sdd/2026-10-10-music-scan-cancel)
#     — 진단 근거 원장: (c) 확정=Termux:X11 native 화면 1920x1005 vs 서버 창
#     1280x720 @(320,142) → 터치 면적 ~50% 죽은 마진(클릭 사각지대)·(d) 유력
#     =버튼/키 릴레이 유실([cpustat] input 3→2→1→0 멸·모션 상주 — 서버 Send
#     이전/클라 수취 미분절). Iron Law: **원인 확정 전 수리 금지 승계** — 본
#     probe의 수리 배치 = ①fit-to-display(스파이크 권장 수형 — 지리적 진원
#     소멸) ②관문 대차 계측(서버 [input] sdl/sent/swallow vs 클라 [cpustat]
#     input — 원격 분재) — 유실 재현 시에만 소량 수리(별행 커밋).
#
# 본 probe 신설 세그먼트(기존 probe 무 훅 파손 — 신설 파일):
#   XT-XTEST  : 폰 clang+libXtst로 X 수준 주입기(xtap/xkey) 합성 — XTEST가
#               성립하면 **실기기 터치와 같은 X-윈도우 의미론**(X 서버 사건
#               스트림 → SDL 종착)으로 재현한다(Agent INJECT_EVENTS 불가
#               원장 — /system/bin/input tap SecurityException 실측).
#   XT-FIT    : 부트 로그 fit-to-display(native 모드)와 ffmpeg X 화면 크기
#               파싱 대차+fit-scale log↔phys 등호(셀 픽셀 불변 — DeX
#               fit-scale 함정 원장 재실측 확정행).
#   XT-CALIB  : 캘리브레이션 탭 1건 — 새로고침(45,50 — idempotent)의 X 좌표
#               = 서버 [input] down px 등호 → 데스크톱 원점=X (0,0) 단정
#               (fit 후 창=화면 — 착탄 좌표가 직행 등호).
#   XT-MARGIN : 구 마진 지점(X (100,100) — 구 창 1280x720 @(320,142) 밖)
#               탭 → down px 행+swl(desktop) — 수정 전에는 서버 창 자체가
#               없어 절대 도달 불가한 지점. RELAY 0 = 도달처 부재의 정직
#               원문(바탕 탭 — 유실 아님).
#   XT-BURST  : 버튼 burst 5탭(30ms 간격 — 스파이크 "burst 유실" 구도) →
#               서버 sdl mdn==5==sent·relay t=2==5·swallow 0·클라 수취 합계
#               ≥15 — 유실 소멸 여부의 1차 판정축(fit 적용 후).
#   XT-SINGLE : 싱글 2탭 2.5s 페이싱(스파이크 "싱별 down/up도 input=0" 구도)
#               → relay 2건+클라 수취.
#   XT-KEY    : 키/Char 경로 — xkey s×5+a×2(KeyDown/KeyUp/TEXTINPUT→Char) →
#               sdl kdn==sent kdn·relay t=5/t=7 원문·클라 수취 합계.
#   XT-SYNTH  : 합성 대조군 send_input 5탭(permissions 병합 — 호출마다
#               핫리드 계약) — X11 우회 직송 경로의 전송 보장 대조(
#               [input] down px 행 0 = X11 미경유 단정 — 두 경로 분리 원문).
#
# 폰 selftest 캐논(계보): 638(music 라인 T4 정산 — 2p 7·2q 7 흡수 — 1a4d651
#   라인 캐논 661/638/296/폰 638). #96 r1은 selftest 어설션 신설 0(관측+
#   fit뿔) — 등호 승계 기대. OTHER-N은 정직 원장행(실측 정산 몫).
#
# 실행법(윈도 Git Bash, 저장소 루트 어디서든):
#   PHONE_HOST=<폰 IP> bash engine/tools/probes/phone_input_relay.sh
# PHONE_HOST 필수(환경변수 — 기록 금지 계약). PHONE_PORT/USER/KEY/DISPLAY만
# 기본값. 풀 로그 engine/tmp/phone_input_relay.log·런 원문
# engine/tmp/phone_input_relay_run.log.
#
# 함정 원장(sty: phone_music 수형 승계):
#   · 원격 복합문 1행 다중 명령은 셸 quoting 파열 원장(수동 캘리브레이션 1런
#     실측 — xtap 무출력·캡처만 성립) — 액션은 1 ssh 1 명령 원칙.
#   · 클라 [cpustat] 태그 부재(서버·클라 같은 stderr 파일 인터리브) — 서버
#     행은 ^\[cpustat\] sdl=/[input]·클라 행은 ^\[cpustat\] timer= 로 분해.
#   · 1초 창 경계 드리프트 — 분절 판정은 burst의 **바이트 델타 합계** 원문
#     (행별 1초 정합 아님).
set -u
export MSYS_NO_PATHCONV=1  # MSYS가 ssh 인수의 posix 경로를 찢는 것 차단

ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
cd "$ROOT" || exit 1
SCRATCH="$ROOT/engine/tmp"
mkdir -p "$SCRATCH"
TARBALL="$SCRATCH/pinp_deploy.tar"
RLOG="$SCRATCH/phone_input_relay.log"
RUNLOG="$SCRATCH/phone_input_relay_run.log"
RSRC_TAR_NAME="pinp_phone_remote.sh"
RSRC_PHONE="~/JKENGINE/$RSRC_TAR_NAME"
GITROOT="$(cygpath -w "$ROOT" 2>/dev/null || echo "$ROOT")"

PHONE_HOST="${PHONE_HOST:-}"
PHONE_PORT="${PHONE_PORT:-8022}"
PHONE_USER="${PHONE_USER:-u0_a4}"
PHONE_KEY="${PHONE_KEY:-$HOME/.ssh/termux_jkengine}"
PHONE_DISPLAY="${PHONE_DISPLAY:-:1}"
PHONE_DEPLOY_BASE="${PHONE_DEPLOY_BASE:-10c7bf7}"  # #95 그리드 posix 개방 — #96 r1(kdektop 서버 2파일) 계보 앵커

if [ -z "$PHONE_HOST" ]; then
  echo "PINP-FAIL: PHONE_HOST not set — 기록 금지 계약상 기본값·스캔 폴백 없음 (환경변수로 폰 호스트만 지정: 자리표시 PHONE_HOST=<폰>)"
  exit 1
fi

exec > >(tee "$RLOG") 2>&1
FAIL() { echo "PINP-FAIL: $*"; exit 1; }
echo "PHONE-HOST: 환경변수 지정 사용 (기록 금지 — 자리표시 PHONE_HOST=<폰>)"
echo "HEAD: $(git -C "$GITROOT" rev-parse HEAD 2>/dev/null || echo rev-parse-failed)"
echo "BASE-ANCHOR: $PHONE_DEPLOY_BASE"
echo "LINEAGE(폰): selftest 638(2p 7·2q 7 — music 라인 정산) — #96 r1 캐논 등호 승계(접촉 0 기대)"
SSH="ssh -p $PHONE_PORT -o BatchMode=yes -o ConnectTimeout=15 -o ServerAliveInterval=15 -o ServerAliveCountMax=10 -i $PHONE_KEY $PHONE_USER@$PHONE_HOST"

# ------------------------------------------------------------------ 0. 전수 sweep
echo "=== 0. 전수 sweep(트리 낙후 발각 — D1 원장 수형) — HEAD blob vs 폰(CR 정규화) ==="
TREE="$SCRATCH/pinp_tree.txt"
git -C "$GITROOT" ls-tree -rl HEAD -- engine/src engine/include engine/CMakeLists.txt |
    sed 's/\t/ /' | awk '{print $4, $5}' | sort > "$TREE"
NTREE=$(wc -l < "$TREE")
echo "sweep 대상: $NTREE 파일"
LC_ALL=C sort "$TREE" > "$SCRATCH/pinp_head_sizes.txt"
NTREE_OK=$(awk 'NF==2 && $1 ~ /^[0-9]+$/ && $2 ~ /^engine\//' "$TREE" | wc -l)
[ "$NTREE_OK" -eq "$NTREE" ] || FAIL "sweep parse broken ($NTREE_OK/$NTREE valid)"
awk '{print $2}' "$TREE" > "$SCRATCH/pinp_head_files.txt"
$SSH "cat > \$HOME/.pinp_files.txt" < "$SCRATCH/pinp_head_files.txt" || FAIL "tree files push failed"
$SSH 'cd ~/JKENGINE && : > ~/.pinp_sizes.txt
while read -r f; do
  if [ -f "$f" ]; then printf "%s %s\n" "$(tr -d "\r" < "$f" | wc -c)" "$f"; else echo "MISSING $f"; fi
done < ~/.pinp_files.txt' > "$SCRATCH/pinp_phone_sizes.txt" || FAIL "phone size sweep failed"
LC_ALL=C sort "$SCRATCH/pinp_phone_sizes.txt" > "$SCRATCH/pinp_phone_sizes.s" \
    || FAIL "phone size sweep sort failed"
mv -f "$SCRATCH/pinp_phone_sizes.s" "$SCRATCH/pinp_phone_sizes.txt"
MISSN=$(grep -ac MISSING "$SCRATCH/pinp_phone_sizes.txt" || true)
echo "phone-tree-missing=$MISSN (배포 전 신선도 원문 — 신규 파일 부재 포함)"
DIFFOUT=$(diff "$SCRATCH/pinp_head_sizes.txt" "$SCRATCH/pinp_phone_sizes.txt" || true)
DIFFN=$(printf '%s\n' "$DIFFOUT" | grep -ac '^[<>]')
DIFFFILES=$(printf '%s\n' "$DIFFOUT" | grep -a '^[<>]' | sed 's/^[<>] //' | awk '{print $2}' | sort -u)
echo "SWEEP-DIFF=$DIFFN / $NTREE"
git -C "$GITROOT" diff --name-only "$PHONE_DEPLOY_BASE" HEAD -- engine/src engine/include engine/CMakeLists.txt | sort \
    > "$SCRATCH/pinp_expected.txt"
NEXP=$(wc -l < "$SCRATCH/pinp_expected.txt")
echo "EXPECTED-DEPLOY=$NEXP (계보 파일 — anchor $PHONE_DEPLOY_BASE 이후)"
echo "$DIFFFILES" | grep -a . > "$SCRATCH/pinp_difffiles.txt" || true
[ -s "$SCRATCH/pinp_difffiles.txt" ] || { echo > "$SCRATCH/pinp_difffiles.txt"; }
UNEXPECTED=$(comm -23 "$SCRATCH/pinp_difffiles.txt" "$SCRATCH/pinp_expected.txt" | head -10)
if [ -z "${UNEXPECTED:-}" ] && [ "$DIFFN" != "0" ]; then
    echo "SWEEP-SCOPE=EXPECTED (DIFF ⊆ 계보 — tar blob 배포로 진행)"
elif [ "$DIFFN" = "0" ]; then
    echo "SWEEP-SCOPE=CLEAN (폰 트리=HEAD 동일 — 소스 배포 불요)"
else
    echo "SWEEP-SCOPE=UNEXPECTED-FILES (기대 집합 밖 결손 트리 낙후 — D1 원장 전량 git archive 경로):"
    echo "$UNEXPECTED"
fi

# ------------------------------------------------------------------ 1. pre-clean
echo "=== 1. pre-flight + pre-clean (재링크 ETXTBSY 방지 — jkweb 무접촉) ==="
$SSH 'echo PHONE-REACHABLE; uname -m; echo TMPDIR=$TMPDIR; command -v termux-wake-lock >/dev/null 2>&1 && termux-wake-lock || echo WARN-no-wake-lock' \
    || FAIL "ssh failed (phone unreachable — LAN 한정, sshd 기동: 폰 Termux에서 sshd)"
JKWEB_CKT=$($SSH 'pgrep -c -f "[j]kweb" 2>/dev/null' | tr -d ' \r')
echo "JKWEB-COUNT-PRE: ${JKWEB_CKT:-0} (상시 유지 계약 — 절사·pkill 대상 아님)"
$SSH "pkill -f '[j]kdesktop' 2>/dev/null; sleep 2; pkill -9 -f '[j]kdesktop' 2>/dev/null; sleep 1; pgrep -f '[j]kdesktop' >/dev/null 2>&1 && { echo PRECLEAN-FAIL; exit 1; }; echo SRV-DOWN" \
    || FAIL "pre-clean could not bring the old phone server down"
echo "PRECLEAN-SRV-DOWN=OK (#96 서버 재기동 계약 — 브래킷+복원)"

echo "=== 2. XT-XTEST 주입기 — 폰 clang 합성 (X 수준 재현기) ==="
XTAP_C="$SCRATCH/pinp_xtap.c"
XKEY_C="$SCRATCH/pinp_xkey.c"
cat > "$XTAP_C" <<'XTEOF'
/* [probe] XTEST injector — #96 폰 X 수준 탭 재현기 (probe 소유 — 자기 소각) */
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
int main(int argc, char** argv) {
    if (argc < 4) { fprintf(stderr, "usage: xtap :DISPLAY X Y [taps] [gap_ms] [hold_ms]\n"); return 2; }
    Display* d = XOpenDisplay(argv[1]);
    if (!d) { fprintf(stderr, "XTAP-FAIL open(%s)\n", argv[1]); return 3; }
    int xtst = -1, xerr = -1, maj = 2, min = 3;
    if (!XTestQueryExtension(d, &xtst, &xerr, &maj, &min)) {
        fprintf(stderr, "XTAP-FAIL no-xtest\n"); return 4;
    }
    int x = atoi(argv[2]), y = atoi(argv[3]);
    int taps = argc > 4 ? atoi(argv[4]) : 1;
    int gap = argc > 5 ? atoi(argv[5]) : 120;
    int hold = argc > 6 ? atoi(argv[6]) : 120;
    for (int i = 0; i < taps; i++) {
        XTestFakeMotionEvent(d, -1, x, y, CurrentTime);
        usleep(50000);
        XTestFakeButtonEvent(d, 1, True, CurrentTime);
        usleep(hold * 1000);
        XTestFakeButtonEvent(d, 1, False, CurrentTime);
        usleep(50000);   /* 릴리즈 flush 완수 대기 — 종료 race 원장(XTEST flush) */
        XFlush(d);
        if (i + 1 < taps) usleep(gap * 1000);
    }
    XCloseDisplay(d);
    printf("XTAP-OK taps=%d at (%d,%d) gap=%dms hold=%dms xtst=%d.%d\n",
           taps, x, y, gap, hold, maj, min);
    return 0;
}
XTEOF
cat > "$XKEY_C" <<'XTKEOF'
/* [probe] XTEST key injector — keysym press+release (probe 소유 — 자기 소각) */
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
int main(int argc, char** argv) {
    if (argc < 3) { fprintf(stderr, "usage: xkey :DISPLAY SYM [n] [gap_ms]\n"); return 2; }
    Display* d = XOpenDisplay(argv[1]);
    if (!d) { fprintf(stderr, "XKEY-FAIL open\n"); return 3; }
    int xtst = -1, xerr = -1, maj = 2, min = 3;
    if (!XTestQueryExtension(d, &xtst, &xerr, &maj, &min)) {
        fprintf(stderr, "XKEY-FAIL no-xtest\n"); return 4;
    }
    KeyCode k = XKeysymToKeycode(d, (KeySym)XStringToKeysym(argv[2]));
    if (!k) { fprintf(stderr, "XKEY-FAIL keycode(0) for %s\n", argv[2]); return 5; }
    int n = argc > 3 ? atoi(argv[3]) : 1;
    int gap = argc > 4 ? atoi(argv[4]) : 200;
    for (int i = 0; i < n; i++) {
        XTestFakeKeyEvent(d, k, True, CurrentTime);
        XTestFakeKeyEvent(d, k, False, CurrentTime);
        usleep(50000);   /* 릴리즈 flush 완수 대기 — sdl kup 1건 모자람 원장 수형 */
        XFlush(d);
        if (i + 1 < n) usleep(gap * 1000);
    }
    XCloseDisplay(d);
    printf("XKEY-OK sym=%s keycode=%d n=%d xtst=%d.%d\n", argv[2], (int)k, n, maj, min);
    return 0;
}
XTKEOF
TAPB64=$(cygpath -w "$XTAP_C" 2>/dev/null || echo "$XTAP_C")
KEYB64=$(cygpath -w "$XKEY_C" 2>/dev/null || echo "$XKEY_C")
$SSH "cat > \$TMPDIR/xtap.c" < "$XTAP_C" || FAIL "xtap.c push failed"
$SSH "cat > \$TMPDIR/xkey.c" < "$XKEY_C" || FAIL "xkey.c push failed"
$SSH 'clang "$TMPDIR/xtap.c" -o "$TMPDIR/xtap" -lX11 -lXtst && clang "$TMPDIR/xkey.c" -o "$TMPDIR/xkey" -lX11 -lXtst && echo XT-TOOLS-OK' \
    || FAIL "XTEST injector compile failed (폰 clang+libXtst — probe 전제)"
echo "XT-XTEST-BUILT: OK (probe 소유 — END에서 소각)"

# ------------------------------------------------------------------ 3. 배포
echo "=== 3. 배포 (sweep 산치 따라 tar blob 스테이징 / 전량 git archive — D1 원장) ==="
if [ "$DIFFN" != "0" ]; then
    if [ -z "${UNEXPECTED:-}" ]; then
        STAGE="$SCRATCH/pinp_headstage"
        rm -rf "$STAGE"
        GERR="$SCRATCH/pinp_gshow.err"
        : > "$GERR"
        for f in $DIFFFILES; do
            mkdir -p "$STAGE/$(dirname "$f")"
            GS_OK=0
            for try in 1 2 3; do
                git -C "$GITROOT" show "HEAD:$f" > "$STAGE/$f" 2>>"$GERR" && { GS_OK=1; break; }
                sleep 3
            done
            [ "$GS_OK" -eq 1 ] || FAIL "git show HEAD:$f 스테이징 실패(재시도 3회) — $(tail -1 "$GERR")"
        done
        echo "CONTAMINATION-GATE=CLEAN (tar 조립=HEAD blob 전량 — 워킹 카피 접촉 0)"
        WIPPED_TOTAL=$DIFFN
        tar -cf "$TARBALL" -C "$STAGE" $DIFFFILES || FAIL "tar creation failed"
    else
        echo "REDEPLOY-SKIP: 기대 집합 밖 결손 — 이 라인은 전량 배포를 하지 않는다(수리 배치는 tar blob 최소)". 1>&2
        FAIL "UNEXPECTED sweep diff (수동 확인 — D1 전량 archive는 이 probe 스코프 밖)"
    fi
else
    echo "DEPLOY-SKIP (폰 트리=HEAD 동일)"
    unset TARBALL
fi
if [ -n "${TARBALL:-}" ]; then
    echo "tar size: $(wc -c < "$TARBALL" | tr -d ' ') bytes — files: $WIPPED_TOTAL"
    [ "$(wc -c < "$TARBALL")" -gt 2000 ] || FAIL "tar suspiciously small — deploy list broken?"
    $SSH "tar -xf - -C ~/JKENGINE" < "$TARBALL" || FAIL "remote tar extract failed"
    rm -f "$TARBALL"
fi
# 원격 영수증 스크립트는 4에서 생성·복사한다(아래).

# 배포 신선도 마감(POST-DIFF=0 hard 게이트 — phone_music 수형)
$SSH 'cd ~/JKENGINE && : > ~/.pinp_sizes2.txt
while read -r f; do
  if [ -f "$f" ]; then printf "%s %s\n" "$(tr -d "\r" < "$f" | wc -c)" "$f"; else echo "MISSING $f"; fi
done < ~/.pinp_files.txt' > "$SCRATCH/pinp_phone_sizes2.txt" || FAIL "post-deploy sweep failed"
LC_ALL=C sort "$SCRATCH/pinp_phone_sizes2.txt" > "$SCRATCH/pinp_phone_sizes2.s" \
    || FAIL "post-deploy sweep sort failed"
mv -f "$SCRATCH/pinp_phone_sizes2.s" "$SCRATCH/pinp_phone_sizes2.txt"
grep -aq MISSING "$SCRATCH/pinp_phone_sizes2.txt" && FAIL "phone tree missing a tracked file (배포 원천 결손)"
DIFFN2=$(comm -3 "$SCRATCH/pinp_head_sizes.txt" "$SCRATCH/pinp_phone_sizes2.txt" | grep -ac '.')
echo "SWEEP-POST-DIFF=$DIFFN2"
[ "$DIFFN2" -eq 0 ] || FAIL "post-deploy sweep not clean — 배포 후에도 트리 오차 (원장)"
echo "SWEEP-POST-DIFF=0 — DEPLOY-FRESHNESS-OK"
MMK=$($SSH "cd ~/JKENGINE/engine && grep -ac 'fit-to-display' src/server/JKWindowServer.cpp && grep -ac 'InputGateCounters' include/server/JKWindowServer.h && grep -ac 'traceInputs_' src/server/JKWindowServer.cpp" | tr '\n' ' ')
echo "DEPLOY-MARKERS: fit=$MMK (fit-to-display·InputGateCounters·traceInputs_ — 3개 모두 ≥1)"
printf '%s' "$MMK" | grep -aq '0 ' && FAIL "deploy markers missing (배포 결손 — #96 r1 원천)"

# ------------------------------------------------------------------ 4. 원격 스크립트
cat > "$SCRATCH/$RSRC_TAR_NAME" <<'PINPEOF'
#!/bin/bash
# 폰 측 입력 릴레이 영수증 절차 (#96 r1 — phone_input_relay.sh가 생성).
# 브래킷 pkill+복원(서버 재기동 계약 — 상위 승인): 부팅→XT-FIT→selftest→
# XT-CALIB→XT-BURST→XT-SINGLE→XT-KEY→XT-SYNTH→캡처→복원(무-트레이스 부팅+
# Music 창 복원 원상)→소각.
set -u
cd ~/JKENGINE/engine || exit 1
FAIL() { echo "PINP-FAIL: $*"; exit 1; }
DSP="__PHONE_DISPLAY__"   # 드라이버가 PHONE_DISPLAY로 치환한다(기본 1)
TMPD="${TMPDIR:-/data/data/com.termux/files/usr/tmp}"
PERM=buildterm/permissions.json
PERM_ORIG="$TMPD/pinp_orig_permissions.json"
NLOG="$TMPD/pinp_ninja.log"
STLOG="$TMPD/pinp_selftest.log"
CANON_PHONE_EXPECT=638     # music 라인 정산 — #96 r1 등호 승계 기대(접촉 0)
CLK=$(getconf CLK_TCK 2>/dev/null); [ -n "$CLK" ] || CLK=100

# 클릭 상수(데스크톱 좌표 — 창 상대): 새로고침(45,50)·폴더관리 토글(185,77)
# — music probe T4 원장 상수 승계.
CAL_X=45  CAL_Y=50
TGL_X=185 TGL_Y=77
TAB2_X=90 TAB2_Y=77

mkdir -p "$TMPD"
echo "=== A. 진입 마커 — 배포 전 상태 원문 ==="
ENTRY_SRV=$(pgrep -f 'buildterm/[j]kdesktop --server' | tr '\n' ' ')
echo "ENTRY-SERVER-PIDS: ${ENTRY_SRV:-none} (종료 시 원복 판정 원문)"
echo "SETTINGS-TOUCH: 이 probe는 settings.json을 만지지 않는다(music.dirs 병합 없음 — 사용자 프리스테인 [] 유지 계약)"
# probe 소유 유산(전 run 실패 — RUN-DROPPED 재실행 계약 흡수): 캡처·로그 소각.
# (주입기 xtap/xkey·소스는 **삭제 금지** — 드라이버가 런 전 컴파일해 두었다(
#  2런 원장: 진입 소각이 주입기를 지워 XT-CALIB..KEY 전부 무주효 0 델타).
#  이름 지정 삭제 — $TMPDIR 와일드카드 광역 소각 금지 — 유저 파일 보존 계약)
rm -f "$TMPD"/pinp_phone_*.png \
      "$TMPD"/pinp_srv_*.log "$TMPD"/pinp_ninja.log "$TMPD"/pinp_selftest.log \
      "$TMPD/pinp_delta.txt" "$TMPD/pinp_xtap_cal.out"
echo "TMP-RESIDUE-BURIED-AT-ENTRY: pinp_phone_captures=$(ls "$TMPD"/pinp_phone_*.png 2>/dev/null | wc -l) left"

echo "=== B. permissions 병합(+send_input allow) — END 바이트 등호 원복 ==="
if [ -s "$PERM_ORIG" ]; then
    if ! cmp -s "$PERM" "$PERM_ORIG"; then
        cp "$PERM_ORIG" "$PERM" || FAIL "permissions self-heal copy failed"
        echo "PERM-SELF-HEAL: 전 run 진품 백업으로 원복 ($(wc -c < "$PERM") bytes)"
    else
        echo "PERM-PRIOR-BACKUP-PRESENT: $PERM_ORIG ($(wc -c < "$PERM_ORIG") bytes — 현재와 등호)"
    fi
fi
ENTRY_PERM=0
if [ -f "$PERM" ]; then
    ENTRY_PERM=1
    cp "$PERM" "$PERM_ORIG" || FAIL "permissions backup failed"
    cmp -s "$PERM" "$PERM_ORIG" || FAIL "permissions backup byte mismatch"
    echo "PERM-ENTRY-PRE-EXISTING: $(wc -c < "$PERM") bytes — 원문: $(tr -d '\n' < "$PERM")"
    SI=$(sed -n 's/.*"send_input"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' "$PERM" | head -1)
    if [ "$SI" = "allow" ]; then
        echo "PERM-MERGE: send_input none (선존 send_input=allow)"
    else
        awk -v newline="    \"send_input\": \"allow\"" '
            BEGIN { n = 0 }
            { lines[n++] = $0 }
            END {
                last = -1
                for (i = n - 1; i >= 0; i--)
                    if (lines[i] ~ /^[ \t]*\}[ \t\r]*$/) { last = i; break }
                if (last < 1) { print "PERM-MERGE-FAIL"; exit 1 }
                prev = lines[last - 1]
                sub(/[ \t\r]*$/, "", prev)
                if (prev !~ /,$/) prev = prev ","
                lines[last - 1] = prev
                for (i = 0; i < n; i++) {
                    if (i == last) print newline
                    print lines[i]
                }
            }' "$PERM" > "$PERM.new" && mv "$PERM.new" "$PERM" \
            || FAIL "permissions merge failed (구조 친화 실패)"
        echo "PERM-MERGE: +send_input allow (선존 키 보존 — merge=$(wc -c < "$PERM") bytes, END에서 바이트 등호 원복)"
    fi
else
    printf '{\n    "send_input": "allow"\n}\n' > "$PERM" || FAIL "permissions write failed"
    echo "PERM-WIRE-OK: $PERM (선존 부재 — END에서 소각)"
fi
grep -aq 'send_input' "$PERM" || FAIL "permissions.json has no send_input (병합 미성립)"

echo "=== C. ninja 리빌드 (aarch64 — 증분) ==="
pkill -f '[j]kdesktop' 2>/dev/null
sleep 1
pkill -9 -f '[j]kdesktop' 2>/dev/null
sleep 1
if pgrep -f '[j]kdesktop' >/dev/null 2>&1; then
    FAIL "server respawned mid-build (ETXTBSY 위험 — 상위 감독자 원장)"
fi
ninja -C buildterm -j4 >"$NLOG" 2>&1
N_RC=$?
tail -3 "$NLOG"
echo "NINJA-RC=$N_RC"
[ "$N_RC" -eq 0 ] || FAIL "ninja rebuild rc=$N_RC (aarch64 compile failure)"
[ -x buildterm/jkdesktop ] || FAIL "buildterm/jkdesktop missing after rebuild"

echo "=== D. 배포 마커 — fit+계측 원천 실존 단정 ==="
MF=$(grep -c 'fit-to-display' src/server/JKWindowServer.cpp)
MI=$(grep -c 'InputGateCounters' include/server/JKWindowServer.h)
MT=$(grep -c 'traceInputs_' src/server/JKWindowServer.cpp)
echo "MARKER-AFTER fit($MF) counters_h($MI) counters_cpp($MT)"
[ "$MF" -ge 1 ] || FAIL "fit marker missing on phone (배포 결손)"
[ "$MI" -ge 1 ] || FAIL "InputGateCounters marker missing on phone (배포 결손)"
[ "$MT" -ge 1 ] || FAIL "traceInputs_ marker missing on phone (배포 결손)"

echo "=== E. selftest — 폰 캐논 등호 판정 (기대 638 — #96 접촉 0 승계) ==="
timeout 900 ./buildterm/jkdesktop test >"$STLOG" 2>&1
S_RC=$?
ST_PASS=$(grep -ac '^\[PASS\]' "$STLOG")
ST_FAIL=$(grep -ac '^\[FAIL\]' "$STLOG")
P2M=$(grep -ac '^\[PASS\] 2m' "$STLOG")
P2N=$(grep -ac '^\[PASS\] 2n' "$STLOG")
P2O=$(grep -ac '^\[PASS\] 2o' "$STLOG")
P2P=$(grep -ac '^\[PASS\] 2p' "$STLOG")
P2Q=$(grep -ac '^\[PASS\] 2q' "$STLOG")
P2I=$(grep -ac '^\[PASS\] 2i' "$STLOG")
P2G=$(grep -ac '^\[PASS\] 2g' "$STLOG")
echo "PHONE-SELFTEST rc=$S_RC PASS=$ST_PASS FAIL=$ST_FAIL 2m=$P2M 2i=$P2I 2g=$P2G 2n=$P2N 2o=$P2O 2p=$P2P 2q=$P2Q"
grep -a 'AppSelfTest' "$STLOG" | tail -2
grep -aq 'AppSelfTest: 0 failure(s)' "$STLOG" || FAIL "AppSelfTest not 0 failure(s)"
[ "$S_RC" -eq 0 ] || FAIL "selftest rc=$S_RC"
[ "$ST_FAIL" -eq 0 ] || FAIL "selftest FAIL=$ST_FAIL (폰축 회귀)"
awk -v p="$ST_PASS" -v f="$ST_FAIL" -v m="$P2M" -v n="$P2N" -v o="$P2O" \
    -v pf="$P2P" -v qf="$P2Q" -v expect="$CANON_PHONE_EXPECT" 'BEGIN{
    if (p == expect && f == 0)
        printf "CANON-INCLUSION=FULL-%d 등호(=music 라인 정산 %d — #96 관측+fit 접촉 0 승계 — 2m %d 2n %d 2o %d 2p %d 2q %d 보존)\n", p, expect, m, n, o, pf, qf
    else
        printf "CANON-INCLUSION=OTHER-N(%d — 기대 %d 등호 미충 — 2m %d 2n %d 2o %d 2p %d 2q %d — 실측 정산 원장행)\n", p, expect, m, n, o, pf, qf
}'

# ---------------------------------------------------------------- 레그 공통
ctl() { timeout 20 ./buildterm/jkdesktop agentctl "$1" 2>/dev/null | grep -a '{' | head -1; }
ok()  { printf '%s' "${1:-}" | grep -aq '"ok":true'; }
CAPTURE() {
    local OUT=$1
    rm -f "$OUT"
    ffmpeg -hide_banner -loglevel error -f x11grab -video_size 1920x1005 \
        -framerate 1 -i ":$DSP" -frames:v 1 "$OUT" >/dev/null 2>&1
    if [ ! -s "$OUT" ]; then
        SCR=$(ffmpeg -hide_banner -loglevel error -f x11grab -video_size 1920x1005 \
            -framerate 1 -i ":$DSP" -frames:v 1 /dev/null 2>&1 \
            | grep -aoE 'screen size [0-9]+x[0-9]+' | head -1 | sed 's/screen size //')
        [ -n "$SCR" ] || SCR=1920x1005
        echo "CAPTURE-SIZE-ADJUST: $SCR"
        ffmpeg -hide_banner -loglevel error -f x11grab -video_size "$SCR" \
            -framerate 1 -i ":$DSP" -frames:v 1 "$OUT" >/dev/null 2>&1
    fi
    [ -s "$OUT" ] || FAIL "capture failed: $OUT"
    echo "CAPTURE-OK: $OUT ($(wc -c < "$OUT") bytes)"
}
boot_server() { # $1 leg 라벨 $2 extra-env — setsid 부팅+ping
    local LEG=$1
    local LOG="$TMPD/pinp_srv_${LEG}.log"
    pkill -f '[j]kdesktop' 2>/dev/null
    sleep 2
    pkill -9 -f '[j]kdesktop' 2>/dev/null
    sleep 1
    if pgrep -f '[j]kdesktop' >/dev/null 2>&1; then
        FAIL "leg $LEG: server survived pre-boot clean"
    fi
    env DISPLAY=":$DSP" $2 setsid nohup ./buildterm/jkdesktop --server >"$LOG" 2>&1 &
    sleep 9
    SRVPID=$(pgrep -f 'buildterm/[j]kdesktop --server' | head -1)
    [ -n "$SRVPID" ] || { echo "--- log tail:"; tail -5 "$LOG"; FAIL "leg $LEG: no server 9s after boot"; }
    echo "leg $LEG: server pid=$SRVPID (DISPLAY=:$DSP)"
    PING=""
    for i in 1 2 3 4 5; do
        PING=$(ctl '{"tool":"ping","args":{}}')
        ok "$PING" && break
        sleep 2
    done
    ok "$PING" || FAIL "leg $LEG: ping did not ok — $PING"
    sleep 10   # taskbar 자동 스폰 관측 여유
}
wait_music() {
    MW=""
    for i in $(seq 1 30); do
        WIN=$(ctl '{"tool":"list_windows","args":{}}')
        MW=$(printf '%s' "$WIN" | grep -aoE '\{"id":[^}]*"title":"Music"[^}]*\}' | head -1)
        [ -n "$MW" ] && break
        sleep 3
    done
    [ -n "$MW" ] || FAIL "no Music window (폰 스폰 15-18s 원장 + 마진 90s 초과) — last: ${WIN:-none}"
    MUSWIN="$MW"
    echo "MUSWIN-$LEG: $MUSWIN"
}
parse_geo() {
    MUS_ID=$(printf '%s' "$MUSWIN" | sed -n 's/.*"id":\([0-9]*\),.*/\1/p')
    MUS_X=$(printf '%s' "$MUSWIN" | sed -n 's/.*"x":\(-\?[0-9]*\),.*/\1/p')
    MUS_Y=$(printf '%s' "$MUSWIN" | sed -n 's/.*"y":\(-\?[0-9]*\),.*/\1/p')
    MUS_W=$(printf '%s' "$MUSWIN" | sed -n 's/.*"w":\([0-9]*\),.*/\1/p')
    MUS_H=$(printf '%s' "$MUSWIN" | sed -n 's/.*"h":\([0-9]*\),.*/\1/p')
    [ -n "$MUS_ID" ] || FAIL "geo parse failed (id empty) — MUSWIN: $MUSWIN"
    echo "MUS-GEO-$1: id=$MUS_ID x=$MUS_X y=$MUS_Y w=$MUS_W h=$MUS_H (meta 560x520)"
}
DELTA() { # $1=LOG $2=start_byte → stdout=끝바이트(델타는 pinp_delta.txt)
    local LOG=$1 L0=$2
    local L1; L1=$(wc -c < "$LOG")
    tail -c +$((L0 + 1)) "$LOG" > "$TMPD/pinp_delta.txt" 2>/dev/null
    echo "DELTA-GAUGE: bytes=$((L1 - L0)) timer_rows=$(grep -aE '^\[cpustat\] timer=' "$TMPD/pinp_delta.txt" | wc -l) srv_input_rows=$(grep -ac '^\[input\]' "$TMPD/pinp_delta.txt")" >&2
    echo "$L1"
}
SCLI() { # 델타의 클라 [cpustat] input 합계 — 선행 .* 필수(그룹 1만 남기는
          # 치환 원장 — 접두 잔존행 "[cpustat] timer=60"이 합계 0 가짜를
          # 냈다(2런 실측 전판정))
    grep -aE '^\[cpustat\] timer=' "$TMPD/pinp_delta.txt" \
        | sed -n 's/.* input=\([0-9]*\) .*/\1/p' | awk '{s+=$1} END{print s+0}'
}
SDLC() { # $1=필드 — 델타의 [input] **sdl() 괄호 안** 필드 합계(sent()/swallow()
          # 동명 필드(mdn 등)와 분리 — 괄호 절단 후 정확 일치 매칭).
    awk -v f="$1" '
        /^\[input\] sdl\(/ {
            s = $0
            sub(/^.*\bsdl\(/, "", s)
            sub(/\).*$/, "", s)
            n = split(s, a, " ")
            for (i = 1; i <= n; i++) {
                k = a[i]; sub(/=.*/, "", k)
                if (k == f) { v = a[i]; sub(/^[^=]*=/, "", v); t += v }
            }
        }
        END { print t + 0 }' "$TMPD/pinp_delta.txt"
}
SCNT() {
    (grep -ac "$1" "$TMPD/pinp_delta.txt" || true) | tr -d ' \r'
}

# ══════════════════════ XT-FIT — 부팅·fit 게이트 ══════════════════════
LEG=trace
echo "=== F. leg trace — JK_CPU_TRACE 부팅 + XT-FIT 게이트 (마진 소멸 부팅 원문) ==="
boot_server trace "JK_CPU_TRACE=1"
LOG="$TMPD/pinp_srv_trace.log"
FFIT=$(grep -a 'phone fit-to-display' "$LOG" | head -1)
FSC=$(grep -a 'fit-scale log=' "$LOG" | head -1)
echo "FIT-LOG-1: ${FFIT:-MISS}"
echo "FIT-LOG-2: ${FSC:-MISS}"
printf '%s' "$FFIT" | grep -aq 'fit-to-display 1920x1005' \
    || echo "PINP-FAIL-SOFT(fit: 창 크기가 폰 native 실측 1920x1005 미일치 — $FFIT)"
SCR=$(ffmpeg -hide_banner -loglevel error -f x11grab -video_size 1920x1005 \
    -framerate 1 -i ":$DSP" -frames:v 1 /dev/null 2>&1 \
    | grep -aoE 'screen size [0-9]+x[0-9]+' | head -1 | sed 's/screen size //')
echo "XSCREEN-PARSED: ${SCR:-parse-fail} (fit 근거 화면 원문 — 1920x1005 대조)"
printf '%s' "$FSC" | grep -aq 'log=1920x1005 phys=1920x1005 (equal' \
    || echo "PINP-FAIL-SOFT(fit-scale: log↔phys 비등호 — 셀 픽셀 불변성 원문 대조 필요 — $FSC)"
printf '%s' "$FSC" | grep -aq '(equal' \
    && echo "FIT-SCALE-CELL-INVARIANT: OK — 창 확장=논리 캔버스 확장(셀/창 픽셀 그대로 — fit-scale 축소 아님 단정 행)" \
    || echo "FIT-SCALE-CELL-INVARIANT: MISS(원장행)"
CAPTURE "$TMPD/pinp_phone_fit_full.png"

echo "=== G. music 런치 + MUS-GEO ==="
LEG=G
L=$(ctl '{"tool":"launch_app","args":{"app":"music"}}')
ok "$L" || FAIL "launch_app music failed — $L"
echo "launch reply: $L"
wait_music
parse_geo G
sleep 3

echo "=== XT-CALIB — 캘리브레이션 탭(새로고침 45,50) — 원점 단정 ==="
CX=$((MUS_X + CAL_X)); CY=$((MUS_Y + CAL_Y))
echo "CLICK-CAL: id=$MUS_ID desk=($CX,$CY) — X == 데스크톱(fit 후 창 원점=화면 원점 가정 — 이 탭으로 단정)"
L0=$(wc -c < "$LOG")
$TMPD/xtap ":$DSP" "$CX" "$CY" 1 120 100 >"$TMPD/pinp_xtap_cal.out" 2>&1
XTAP_CAL=$(sed -n 's/^XTAP-OK.*/OK/p' "$TMPD/pinp_xtap_cal.out" | head -1)
echo "XTAP-CAL: ${XTAP_CAL:-$(head -1 "$TMPD/pinp_xtap_cal.out")}"
sleep 2.5
DL=$(DELTA "$LOG" "$L0")
echo "[CAL-DELTA] relay_t2=$(SCNT 'relay t=2') down=$(SCNT '^\[input\] down px') client_input=$(SCLI)"
DOWNTXT=$(grep -a '^\[input\] down px' "$TMPD/pinp_delta.txt" | head -1 | sed 's/^\[input\] //')
echo "DOWN-ROW: ${DOWNTXT:-MISS} (기대: px=$CX,$CY)"
SWLD=$(SCNT 'swl(desktop)')
echo "SWL-DESKTOP-ROW: $SWLD"
if printf '%s' "$DOWNTXT" | grep -aq "px=$CX,$CY"; then
    echo "ORIGIN-CALIB: OK — 탭 X 좌표 == 서버 착점 px(등호 — 데스크톱 원점=X 원점 단정 — 착탄 직행 원문)"
else
    echo "PINP-FAIL-SOFT(calib: 착점 ≠ 탭 좌표 — 원점 오프 산식 재정산 필요: $DOWNTXT)"
fi

echo "=== XT-MARGIN — 구 죽은 마진 지점(X (100,100)) 탭 — 도달 원문 ==="
L0=$(wc -c < "$LOG")
$TMPD/xtap ":$DSP" 100 100 1 120 80 >/dev/null 2>&1
sleep 2.5
DL=$(DELTA "$LOG" "$L0")
echo "[MARGIN-DELTA] down=$(SCNT '^\[input\] down px') swl_desktop=$(SCNT 'swl(desktop)') relay_t2=$(SCNT 'relay t=2')"
DOWNTXT=$(grep -a '^\[input\] down px' "$TMPD/pinp_delta.txt" | head -1 | sed 's/^\[input\] //')
echo "MARGIN-DOWN-ROW: ${DOWNTXT:-MISS} (기대: px=100,100 — 구 창 1280x720 @(320,142) 밖 지점)"
if printf '%s' "$DOWNTXT" | grep -aq 'px=100,100'; then
    echo "MARGIN-NOW-REACHABLE: OK — 수정 전 서버 창이 없던 지점에 X 탭이 SDL 종착에 도달(마진 소멸의 실측 영수증)"
    if [ "$(SCNT 'swl(desktop)')" -ge 1 ] && [ "$(SCNT 'relay t=2')" -eq 0 ]; then
        echo "MARGIN-TARGET: bare-desktop (도달했으나 클라 표면 없음 — swl(desktop) 정직 원문 — 유실 아님)"
    fi
else
    echo "PINP-FAIL-SOFT(margin: 구 마진 지점 탭이 SDL 종착에 미도달 — $DOWNTXT)"
fi

echo "=== XT-BURST — 버튼 burst 5탭(30ms 간격) — 유실 분절 1차 판정축 ==="
BX=$((MUS_X + TAB2_X)); BY=$((MUS_Y + TAB2_Y))
L0=$(wc -c < "$LOG")
$TMPD/xtap ":$DSP" "$BX" "$BY" 5 30 60 >/dev/null 2>&1
sleep 2.5
DL=$(DELTA "$LOG" "$L0")
R_T1=$(SCNT 'relay t=1'); R_T2=$(SCNT 'relay t=2'); R_T3=$(SCNT 'relay t=3')
MDN=$(SDLC 'mdn'); MUP=$(SDLC 'mup')
CLI=$(SCLI); SWLB=$(SCNT 'swl')
echo "[BURST-DELTA] sdl(mdn=$MDN mup=$MUP) relay(t1=$R_T1 t2=$R_T2 t3=$R_T3) swallow=$SWLB client_input=$CLI"
BURST_OK=0
if [ "$MDN" -eq 5 ] && [ "$R_T2" -eq 5 ] && [ "$R_T3" -eq 5 ] && [ "$CLI" -ge 15 ]; then
    BURST_OK=1
    echo "BURST-CLEAN: OK — sdl mdn 5 == relay 5 == 클라 수취 ≥15(5탭×모션+다운+업) — **버튼 burst 유실 미재현(fit 후)**"
    echo "BURST-BISECT: 서버 Send=클라 수취 등호 — (d) 서버→클라 릴레이 소실은 이 관문에서 부정(X11 종착까지 전량 도달)"
else
    echo "PINP-FAIL-SOFT(burst: 유실/미달 — mdn=$MDN relay=$R_T2/$R_T3 client=$CLI — (d) 원격 분재 재판정: sdl>sent면 서버측, sent>client면 수취측)"
fi

echo "=== XT-SINGLE — 싱글 2탭 2.5s 페이싱 (스파이크 싱별 down/up input=0 구도) ==="
L0=$(wc -c < "$LOG")
$TMPD/xtap ":$DSP" "$BX" "$BY" 1 2500 100 >/dev/null 2>&1
sleep 1.5
$TMPD/xtap ":$DSP" "$BX" "$BY" 1 2500 100 >/dev/null 2>&1
sleep 2.5
DL=$(DELTA "$LOG" "$L0")
R_T2=$(SCNT 'relay t=2'); R_T3=$(SCNT 'relay t=3'); MDN=$(SDLC 'mdn'); CLI=$(SCLI)
DOWNS=$(SCNT '^\[input\] down px')
echo "[SINGLE-DELTA] single_downs=$DOWNS sdl(mdn=$MDN) relay(t2=$R_T2 t3=$R_T3) client_input=$CLI"
if [ "$MDN" -eq 2 ] && [ "$R_T2" -eq 2 ] && [ "$CLI" -ge 6 ]; then
    echo "SINGLE-SPACED-CLEAN: OK — 2.5s 간격 싱글 down/up 2건 전량 릴레이+수취 — 스파이크 '싱글도 유실' 구도 미재현(fit 후)"
else
    echo "PINP-FAIL-SOFT(single: 싱별 페이싱 유실 재판정 필요 — mdn=$MDN r_t2=$R_T2 client=$CLI)"
fi

echo "=== XT-KEY — 키/Char 경로 (xkey s×5·a×2 — KeyDown/TEXTINPUT→Char 릴레이) ==="
KL0=$(wc -c < "$LOG")
$TMPD/xkey ":$DSP" s 5 200 >/dev/null 2>&1
sleep 1.5
$TMPD/xkey ":$DSP" a 2 200 >/dev/null 2>&1
sleep 2.5
DL=$(DELTA "$LOG" "$KL0")
R_KDN=$(SCNT 'relay t=5'); R_CHR=$(SCNT 'relay t=7')
KDN=$(SDLC 'kdn'); KUP=$(SDLC 'kup'); CHR=$(SDLC 'char')
CLI=$(SCLI); SWLK=$(SCNT 'swl(key)')
echo "[KEY-DELTA] sdl(kdn=$KDN kup=$KUP char=$CHR) relay(t5=$R_KDN t7text=$R_CHR) swl(key)=$SWLK client_input=$CLI"
if [ "$KDN" -eq 7 ] && [ "$R_KDN" -eq 7 ] && [ "$CHR" -eq 7 ] && [ "$CLI" -ge 20 ]; then
    echo "KEY-CHAR-CLEAN: OK — KeyDown 7 == relay 7, TEXTINPUT→Char 7(텍스트 원문 s5+a2) — 키 릴레이 유실 미재현"
else
    echo "PINP-FAIL-SOFT(key: kdn=$KDN relay=$R_KDN char=$CHR client=$CLI — sdl>sent면 서버 관문, sent>client면 수취 관문 — 대차 원문 상단)"
fi
CAPTURE "$TMPD/pinp_phone_after_burst.png"

echo "=== XT-SYNTH — 합성 대조군 send_input 5탭 (X11 우회 직송 — 전송 보장 대조) ==="
L0=$(wc -c < "$LOG")
for E in 1 2 3 4 5; do
    TP=$(ctl "{\"tool\":\"send_input\",\"args\":{\"op\":\"click\",\"id\":$MUS_ID,\"x\":$((MUS_X + TAB2_X)),\"y\":$((MUS_Y + TAB2_Y)),\"button\":1,\"clicks\":1}}")
    ok "$TP" || echo "NOTE-SYNTH-$E: send_input failed — $TP"
    sleep 0.3
done
sleep 2.5
DL=$(DELTA "$LOG" "$L0")
S_T1=$(SCNT 'relay t=1'); S_T2=$(SCNT 'relay t=2'); S_T3=$(SCNT 'relay t=3')
SDOWNS=$(SCNT '^\[input\] down px')
CLI=$(SCLI)
echo "[SYNTH-DELTA] synthetic relay(t1=$S_T1 t2=$S_T2 t3=$S_T3) down_px_rows=$SDOWNS(기대 0 — X11 미경유 단정) client_input=$CLI"
if [ "$S_T2" -eq 5 ] && [ "$S_T3" -eq 5 ] && [ "$CLI" -ge 15 ] && [ "$SDOWNS" -eq 0 ]; then
    echo "SYNTH-TRANSPORT-CLEAN: OK — 합성 직송 5탭도 전량 릴레이+수취(X11 무경유 — down px 0행 단정) — 전송 보장 경로 원문"
else
    echo "PINP-FAIL-SOFT(synth: t2=$S_T2 t3=$S_T3 client=$CLI down_rows=$SDOWNS — 전송 경로 대차 원장)"
fi

echo "=== H. 분절 대차 집계 — 관문별 합계 원문 ==="
grep -aE '^\[input\] sdl' "$LOG" | awk '{for(i=1;i<=NF;i++) if ($i ~ /^(sdl|sent|swallow)/) print}' \
    | tail -20 | sed 's/^/  /'
echo "[cpustat] 클라 수취 꼬리:"
grep -aE '^\[cpustat\] timer=' "$LOG" | tail -10 | sed 's/^/  /'

# ══════════════════════ 복원 — 무-트레이스 부팅 + Music 창 복원 원상 ══════════════════════
echo "=== I. 복원 — permissions 바이트 원복 + 무-트레이스 부팅 + Music 창 복원 ==="
if [ "$ENTRY_PERM" -eq 1 ]; then
    cp "$PERM_ORIG" "$PERM" || FAIL "permissions restore copy failed"
    cmp -s "$PERM" "$PERM_ORIG" || FAIL "permissions restore byte mismatch"
    echo "PERM-RESTORED-BYTES: $(wc -c < "$PERM") (선존 원문 $(wc -c < "$PERM_ORIG") 등호 — 병합 복원)"
else
    rm -f "$PERM"
    [ -f "$PERM" ] && echo "WARN: permissions.json survived rm" || echo "PERM-BURIED: permissions.json removed"
fi
LEG=restore
boot_server restore ""
RLM=$(ctl '{"tool":"launch_app","args":{"app":"music"}}')
ok "$RLM" || echo "NOTE-RESTORE-MUSIC: launch 미성립 — $RLM (원장행)"
wait_music
parse_geo restore
sleep 2
CAPTURE "$TMPD/pinp_phone_restored.png"

echo "=== J. 종료 게이트 — 서버 UP + jkweb 생존 + 소각 ==="
pgrep -f 'buildterm/[j]kdesktop --server' >/dev/null 2>&1 || FAIL "server not UP at end (종료 게이트 위반)"
JKWEB_AFTER=$(pgrep -f '[j]kweb' | tr '\n' ' ')
echo "JKWEB-AFTER: ${JKWEB_AFTER:-none} (BEFORE: __JKWEB_PRE__ — 절사 없음 단정 재료)"
FINAL=$(ctl '{"tool":"list_windows","args":{}}')
echo "FINAL-WINDOWS: $(printf '%s' "$FINAL" | head -c 400)"
rm -f "$TMPD/xtap" "$TMPD/xkey" "$TMPD/xtap.c" "$TMPD/xkey.c" \
      "$TMPD/pinp_delta.txt" "$TMPD/pinp_xtap_cal.out" "$TMPD/pinp_ninja.log" \
      "$TMPD/pinp_selftest.log" "$TMPD/pinp_srv_trace.log" "$TMPD/pinp_srv_restore.log" \
      "$PERM_ORIG"
echo "PINP-BURIED: injector+logs+perm-orig 소각 (probe 소유 잔상 0 — 캡처 3종는 드라이버 회수 후 소각)"
rm -f "$0"   # 자기 소각 — REMNANT rc=2 게이트(phone_music 수형)
echo "PINP-FINISHED"
exit 0
# honest-fail 원칙: 수치 미달(FAIL-SOFT 행)은 원장 목적이라 rc=0 — 드라이버가
# 판정행에서 집계한다. hard FAIL(ssh·sweep·배포·ninja·selftest 회귀·permissions
# 원복·부팅·ping·launch·창·캡처·소각·XT 합성 실패)만 FAIL()에서 exit 1.
# EOF — 끝 개행 유지.
PINPEOF
sed -i "s/__PHONE_DISPLAY__/${PHONE_DISPLAY#:}/" "$SCRATCH/$RSRC_TAR_NAME" \
    || FAIL "remote script substitution failed"
grep -aq '__PHONE_DISPLAY__' "$SCRATCH/$RSRC_TAR_NAME" && FAIL "placeholder unresolved (display)"
sed -i "s/__JKWEB_PRE__/${JKWEB_CKT:-0}/" "$SCRATCH/$RSRC_TAR_NAME" \
    || FAIL "jkweb count substitution failed"
grep -aq '__JKWEB_PRE__' "$SCRATCH/$RSRC_TAR_NAME" && FAIL "placeholder unresolved (jkweb pre)"
$SSH "cat > $RSRC_PHONE" < "$SCRATCH/$RSRC_TAR_NAME" || FAIL "remote script copy failed"
$SSH "test -f $RSRC_PHONE && echo REMOTE-SCRIPT-PRESENT || echo REMOTE-SCRIPT-ABSENT" \
  | grep -aq REMOTE-SCRIPT-PRESENT || FAIL "remote receipt script missing on phone"

# ------------------------------------------------------------------ 5. 실행
echo "=== 5. 폰 리빌드+영수증 절차 실행 ==="
$SSH "bash $RSRC_PHONE" > "$RUNLOG" 2>&1
R_RC=$?
RUN_RC=$R_RC
if [ "$R_RC" -eq 255 ]; then
  echo "RUN-DROPPED: ssh 채널 단절(Wi-Fi 낙하 함정 — 폰 로그는 \$TMPDIR에 생존, 재실행=증분:"
  echo "  PHONE_HOST=<폰 IP> bash engine/tools/probes/phone_input_relay.sh)"
fi
cat "$RUNLOG"
grep -aq '^PINP-FINISHED$' "$RUNLOG" \
  || FAIL "receipt script did not finish (rc=$RUN_RC — RUN-DROPPED면 재실행)"
if grep -a '^PINP-FAIL:' "$RUNLOG" >/dev/null 2>&1; then
# (honest-fail 원장: hard 마커는 'PINP-FAIL:' 콜론 행뿐 — 'PINP-FAIL-SOFT(...)'
#  행은 수치 미달 원장행이라 rc=0 집계 대상. 접두 일치하던 오판 2런 실측 폐곡.)
  HARD=$(grep -a '^PINP-FAIL:' "$RUNLOG" | head -1)
  FAIL "phone hard receipt: $HARD"
fi

# ------------------------------------------------------------------ 6. REMNANT
echo "=== 6. REMNANT — 원격 스크립트 자기 소각 확인 (rc 게이트) ==="
$SSH "ls $RSRC_PHONE" >/dev/null 2>&1
REMNANT_RC=$?
echo "REMNANT-LS-RC=$REMNANT_RC"
[ "$REMNANT_RC" -eq 2 ] || FAIL "REMNANT survived (원격 스크립트 자기 소각 실패 — rc=$REMNANT_RC)"
REM_XT=$($SSH '[ -f "$TMPDIR/xtap" ] && echo present || echo gone' | tr -d ' \r')
REM_XK=$($SSH '[ -f "$TMPDIR/xkey" ] && echo present || echo gone' | tr -d ' \r')
REM_PNG=$($SSH 'ls "$TMPDIR"/pinp_phone_*.png 2>/dev/null | wc -l' | tr -d ' \r')
echo "REMNANT-XT: xtap=$REM_XT xkey=$REM_XK (probe 소유 잔산 게이트 — 캡처는 회수 후 소각)"
{ [ "$REM_XT" = "gone" ] && [ "$REM_XK" = "gone" ]; } \
    || FAIL "REMNANT survived: injector residue left on phone (xtap=$REM_XT xkey=$REM_XK)"
PRE_CAPS=${REM_PNG:-0}
[ "$PRE_CAPS" -eq 3 ] || echo "NOTE-REMNANT: captures=$PRE_CAPS (기대 3 — 회수 전 원장행)"
$SSH "rm -f \$HOME/.pinp_tree.txt \$HOME/.pinp_files.txt \$HOME/.pinp_sizes.txt \$HOME/.pinp_sizes2.txt" 2>/dev/null
echo "SWEEP-FILES-BURIED-REMOTE: 4"

# ------------------------------------------------------------------ 7. 캡처 회수
echo "=== 7. 캡처 회수 — ssh 호출당 base64 1파이프 계약 (+md5 대차) ==="
recover() {
    local REMOTE_PNG=$1 LOCAL_PNG=$2
    local WTXT WPNG WB64 RMD5 LMD5
    WB64=$(cygpath -w "$SCRATCH/pinp_b64.txt" 2>/dev/null || echo "$SCRATCH/pinp_b64.txt")
    WPNG=$(cygpath -w "$LOCAL_PNG" 2>/dev/null || echo "$LOCAL_PNG")
    RMD5=$($SSH "md5sum $REMOTE_PNG" 2>/dev/null | sed -n 's/^\([0-9a-f]*\) .*$/\1/p' | tr -d ' \r')
    $SSH "base64 -w0 $REMOTE_PNG" > "$SCRATCH/pinp_b64.txt" 2>"$SCRATCH/pinp_b64.err" \
        || FAIL "recover failed: $REMOTE_PNG"
    [ -s "$SCRATCH/pinp_b64.err" ] && echo "NOTE: stderr noise — $(tail -1 "$SCRATCH/pinp_b64.err")"
    python - "$WB64" "$WPNG" <<'PYEOF' || FAIL "base64 decode failed: $LOCAL_PNG"
import base64, re, sys
d = open(sys.argv[1], "rb").read().decode("ascii", "ignore")
d = re.sub(r"\s+", "", d)
open(sys.argv[2], "wb").write(base64.b64decode(d))
print("RECOVER-OK:", sys.argv[2])
PYEOF
    [ -s "$LOCAL_PNG" ] || FAIL "recovered png empty: $LOCAL_PNG (폰쪽 삭제 전 확인)"
    LMD5=$(md5sum "$LOCAL_PNG" | sed -n 's/^\([0-9a-f]*\) .*$/\1/p')
    if [ -n "$RMD5" ] && [ "$RMD5" = "$LMD5" ]; then
        echo "RECOVER-MD5-OK: $LMD5 ($(wc -c < "$LOCAL_PNG" | tr -d ' ') bytes)"
    else
        FAIL "recover md5 mismatch: $LOCAL_PNG phone=$RMD5 local=$LMD5 (전송 파열 방어)"
    fi
    rm -f "$SCRATCH/pinp_b64.txt"
}
recover "\$TMPDIR/pinp_phone_fit_full.png" "$SCRATCH/pinp_phone_fit_full.png"
recover "\$TMPDIR/pinp_phone_after_burst.png" "$SCRATCH/pinp_phone_after_burst.png"
recover "\$TMPDIR/pinp_phone_restored.png" "$SCRATCH/pinp_phone_restored.png"
$SSH "rm -f \$TMPDIR/pinp_phone_fit_full.png \$TMPDIR/pinp_phone_after_burst.png \$TMPDIR/pinp_phone_restored.png"
REM_PNG2=$($SSH 'ls "$TMPDIR"/pinp_phone_*.png 2>/dev/null | wc -l' | tr -d ' \r')
echo "CAPTURES-WRITTEN: pinp_phone_{fit_full,after_burst,restored}.png → engine/tmp/ · REMNANT-CAPTURES-AFTER-BURIED=${REM_PNG2:-0}"
[ "${REM_PNG2:-0}" = "0" ] || FAIL "REMNANT survived: capture residue left on phone (png=$REM_PNG2)"

# ------------------------------------------------------------------ 8. 로컬 실측
PYRUNLOG=$(cygpath -w "$RUNLOG" 2>/dev/null || echo "$RUNLOG")
PYTHONIOENCODING=utf-8 python - "$PYRUNLOG" <<'ANALYSIS_EOF'
# -*- coding: utf-8 -*-
# #96 폰 로컬 실측 — 분절 대차+fit 원문 파싱.
import re, sys
try:
    with open(sys.argv[1], encoding="utf-8", errors="ignore") as f:
        txt = f.read()
except OSError:
    txt = ""
def grab(label):
    m = re.search(r"^" + re.escape(label) + r"[: ].*$", txt, re.M)
    return m.group(0) if m else None
for line in ("FIT-LOG-1", "FIT-LOG-2", "XSCREEN-PARSED", "DOWN-ROW",
             "ORIGIN-CALIB", "MARGIN-DOWN-ROW", "MARGIN-NOW-REACHABLE",
             "[BURST-DELTA]", "BURST-CLEAN", "BURST-BISECT",
             "SINGLE-SPACED-CLEAN", "[KEY-DELTA]", "KEY-CHAR-CLEAN",
             "[SYNTH-DELTA]", "SYNTH-TRANSPORT-CLEAN"):
    v = grab(line.replace("[", r"\[").replace("]", r"\]"))
    if v:
        print("P96| " + v)
c = re.search(r"CANON-INCLUSION=([^\r\n]*)", txt)
if c:
    print("CANON-LINE: %s" % c.group(1))
st = re.search(r"PHONE-SELFTEST rc=(\d+) PASS=(\d+) FAIL=(\d+)", txt)
if st:
    print("SELFTEST-LINE: rc=%s PASS=%s FAIL=%s" % st.groups())
ANALYSIS_EOF

# ------------------------------------------------------------------ 9. 판정
echo "=== 8. 입력 릴레이 수리 판정 (측정 원문은 상단 analysis 행 — 최종 결제는 육안 스탭) ==="
CAP_OK=1
for f in fit_full after_burst restored; do
    P="$SCRATCH/pinp_phone_$f.png"
    [ -s "$P" ] || { echo "PIN-INPUT-FAIL(capture missing: $P)"; CAP_OK=0; }
done
CANON=$(grep -a '^CANON-INCLUSION=' "$RUNLOG" | tail -1 | sed 's/^CANON-INCLUSION=//' | tr -d '\r')
CANON_OK=0
case "$CANON" in
  FULL-638*) CANON_OK=1 ;;
  *) echo "NOTE-CANON: 캐논 계보 ${CANON:-n/a} — 원장행" ;;
esac
FIT_OK=0
FL1=$(grep -a '^FIT-LOG-1: ' "$RUNLOG" | tail -1 | sed 's/^FIT-LOG-1: //' | tr -d '\r')
printf '%s' "$FL1" | grep -aq 'fit-to-display 1920x1005' && FIT_OK=1
CALB=$(grep -a '^ORIGIN-CALIB: OK' "$RUNLOG" | tail -1 | tr -d '\r')
CAL_OK=0
[ -n "$CALB" ] && CAL_OK=1
MARG=$(grep -a '^MARGIN-NOW-REACHABLE: OK' "$RUNLOG" | tail -1 | tr -d '\r')
MARG_OK=0
[ -n "$MARG" ] && MARG_OK=1
BURST=$(grep -a '^BURST-CLEAN: OK' "$RUNLOG" | tail -1 | tr -d '\r')
BURST_OK=0
[ -n "$BURST" ] && BURST_OK=1
SGL=$(grep -a '^SINGLE-SPACED-CLEAN: OK' "$RUNLOG" | tail -1 | tr -d '\r')
KEYC=$(grep -a '^KEY-CHAR-CLEAN: OK' "$RUNLOG" | tail -1 | tr -d '\r')
KEY_OK=0
[ -n "$KEYC" ] && KEY_OK=1
SYNT=$(grep -a '^SYNTH-TRANSPORT-CLEAN: OK' "$RUNLOG" | tail -1 | tr -d '\r')
SYN_OK=0
[ -n "$SYNT" ] && SYN_OK=1
if [ "$CAP_OK" -eq 1 ] && [ "$CANON_OK" -eq 1 ] && [ "$FIT_OK" -eq 1 ] \
   && [ "$CAL_OK" -eq 1 ] && [ "$MARG_OK" -eq 1 ] && [ "$BURST_OK" -eq 1 ] \
   && [ "$KEY_OK" -eq 1 ] && [ "$SYN_OK" -eq 1 ]; then
    echo "PIN-INPUT-VERDICT: INPUT-RELAY-OK(fit-to-display 1920x1005 부팅 원문·원점 캘리브레이션 등호·구 마진 지점 도달·burst 5탭+싱별 2탭+키 7건 전부 릴레이=수취 등호·합성 직송 대조 등호 — **서버 Send→클라 수취 유실 미재현((d) 관문 부정 — 유실은 X11 종착 상류[터치→Termux:X11] 축 승계)** — 캡처 3종 receipt·육안 스탭 대기)"
else
    echo "PIN-INPUT-VERDICT: PIN-INPUT-FAIL(canon=$CANON_OK captures=$CAP_OK fit=$FIT_OK calib=$CAL_OK margin=$MARG_OK burst=$BURST_OK key=$KEY_OK synth=$SYN_OK — 행별 사유는 각 FAIL-SOFT 원문)"
fi
echo "PIN-WCAPTURES:"
for f in fit_full after_burst restored; do
    P="$SCRATCH/pinp_phone_$f.png"
    [ -s "$P" ] && echo "  $P ($(wc -c < "$P" | tr -d ' ') bytes)"
done
echo ""
echo "사용자 결제 게이트(대기 — probe가 결제를 기록하지 않는다):"
echo "  ① 폰 fit(mus_phone_fit_full.png): 데스크톱(런처 그리드+바탕)이 X 화면"
echo "     전체(1920x1005)를 채우고 죽은 마진이 없는지 — 셀·아이콘 크기가"
echo "     이전(1280x720 창)과 동일 픽셀인지(fit-scale 축소 아님 — 육안)."
echo "  ② 폰 after_burst(mus_phone_after_burst.png): burst 뒤 Music 창 —"
echo "     탭/패널 상태가 탭 수와 정합하는지(첫 탭 취식 소멸 육안)."
echo "  ③ 폰 restored(mus_phone_restored.png): 종료 상태 서버 UP+Music 창"
echo "     복원(사용자 화면 원상 — 무-트레이스 부팅)."
echo "  ④ 실기기 육안 재판정(XTEST는 X-윈도우 의미론 재현 — 진짜 손가락 터치"
echo "     의 Trackpad 탭 의미론은 상류 원장): 필드 탭 → 키보드 소환은 Back"
echo "     버튼/EK KEYBOARD 키 계약(TX11 소환 경로 원천 부재 — 스파이크 §3)."
echo "PIN-INPUT-END"
# honest-fail 원칙: 수치 미달(PIN-INPUT-FAIL 라인)은 원장 목적이라 rc=0.
# hard FAIL만 FAIL()에서 exit 1.
# EOF — 끝 개행 유지.
exit 0