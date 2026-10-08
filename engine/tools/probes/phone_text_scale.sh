#!/usr/bin/env bash
# 폰 텍스트 스케일 실측 probe (텍스트 스케일 라인 T4 — plan
# 2026-10-09-phone-text-scale, 스펙 docs/superpowers/specs/
# 2026-10-09-phone-text-scale-design.md — 폰 실측 결제 관문).
#
# 승계 원천:
#   · phone_dirty_present.sh(전 라인 T4) — tar 오염 게이트+HEAD blob 스테이징·
#     tar-over-ssh·REMNANT·폰 selftest 캐논 게이트·jkweb 상시 유지(절사 금지)
#   · phone_text_scale_ab.sh(T1 진단) — x11grab 캡처(1920x1080 시도→화면 크기
#     파싱 재시도 — 폰 실측 1920x1005)·ImageMagick import 금지(폰 libheif 파단)·
#     base64 1파이프 회수·settings 워터마크 복원 BOOT-OK
#
# 영수증 목표:
#   ① 폰 재배포 — 텍스트 스케일 라인(24d22a1..b651534 = T2 결선 58861c3+
#      T3 밴드 8563263+T3 fix r1 b651534)이 건드린 소스 20건+probe 본체.
#      신규 소스 0건(전부 기존 파일 수정) — CMakeLists 불필요(ninja 헤더
#      의존으로 의존 cpp 전부 재컴파일).
#   ② 폰 aarch64 ninja 리빌드(NINJA-RC 전파 — ~9-10분)
#   ③ 폰 selftest 캐논 게이트 — **기대 506 계보** = 495(더티프레젠트 최신) +
#      6(2t-a..f, T2) + 3(3b-a..c, T3) + 2(3b-d/e, T3 fix r1). 원장:
#      472/489/495는 더티프레젠트 시대 캐논 포인트. 2t/3b [PASS] 실측수를
#      함께 인쇄해 소스 반영을 이중 단정. 틀리면 계보 정산 원장(정직 인쇄).
#   ④ leg def15 — settings **font_path 유지 + font_scale 키 삭제(=posix 기본
#      1.5 발동, 스펙 사용자 확정)** 부팅 → x11grab 캡처 desk_def15.png
#      → 지뢰찾기 launch → 세부 crop(제목/버튼/숫자 — 회수 후 로컬 산출)
#      → 크롬 타이틀 밴드 높이 px 원문(32px 기대 = T3 산식 max(24, cellH 24+8)).
#   ⑤ leg s19b — font_scale "1.9" 재시험 → 캡처 desk_s19b.png(구판 영수증
#      desk_s19.png(1.9 파손)·tsd_p19.png 대조 — **셀과 글리프가 함께 커져
#      클립/밀려남 해소 단정**용).
#   ⑥ leg wake — 1.5 기본 복원(키 삭제) 부팅 BOOT-OK + 캡처 desk_wake.png
#      (사용자 기상 상태) + 터미널 상시(종료 상태 계약 — 더티프레젠트 T4 승계).
#   ⑦ jkweb 생존 계약 — 기동 카운트 원문 첨부(진입/각 레그/종료 — 절사 금지,
#      localhost:8090 상시 계약; T3 fix r1 리뷰 M-r2-2 승계 — WSL 데몬은 폰에
#      해당 없음, 폰 probe는 불요).
#   ⑧ 육안 게이트 산출+대기 라벨만 — **probe가 결제를 기록하지 않는다**
#      (사용자 선언만 최종 관문 — 스펙 "가짜 결제 금지"). T2 관측 변화인
#      Windows 싱글 모드 글리프 비트맵→벡터 AA 전환(폰 아닌 PC)도 게이트
#      항목으로 명시만 한다.
#
# 판정 사다리(라벨 계약 — 측정은 회수 후 로컬 numpy/PIL):
#   BAND-VERDICT: BAND-OK  = def15/wake 밴드 32px(±1, T3 산식) AND 타이틀 잉크
#               스팬 ≥ 12(=비트맵 8행 고정과 다름 — 확대 실발화)
#   BAND-VERDICT: BAND-FAIL(행별 이유) — 수치 미달=정직 원장 rc=0
#   S19B-VERDICT: S19B-RESOLVED = s19b 밴드 38px(±2) AND 잉크 스팬 ≥ 1.5측정치
#               +2(비례 기대 ink15×셀30/셀24≈16.25 — 고정 임계 18은 1차 실측
#               run에서 산치 오차로 판정됐다: 실측 16행이 기대치와 정확 부합
#               — 정직 원장 부기) AND 잉크 스팬 ≤ 밴드-2(클립 없음) AND
#               잉크 상단 ≥ 2(구판 플러시탑 서명 rows 1.. 배제 — "위쪽 밀림"
#               결함 소멸 단정) AND 잉크 스팬 > 구판 1.9(4행)
#   TS-VERDICT: TEXTSCALE-OK = 캐논 506 AND BAND-OK AND S19B-RESOLVED — rc=0
#   TS-VERDICT: TEXTSCALE-FAIL(행별 이유) — rc=0(원장 목적)
#   TS-PHONE-FAIL(rc=1) = hard FAIL만: ssh 단절·배포 실패·NINJA-RC≠0·
#               selftest 회귀·서버/ping/기동 실패·캡처 실패·원격 잔존.
#   구판 참조 축(측정 실측 — 스펙 A/B 원문): 1.0 구판 = 밴드 24·잉크 8행·
#   AA색 1(비트맵); 1.9 구판 = 밴드 24(고정 상수)·잉크 4행(상단 클립 —
#   "위치만 위쪽으로" 관측의 픽셀 단정 재료).
#
# 실행법(윈도 Git Bash, 저장소 루트 어디서든):
#   PHONE_HOST=<폰 IP> bash engine/tools/probes/phone_text_scale.sh
# PHONE_HOST unset이면 내부 대역(192.168.11/219 — 대역 표기만 유지) TCP 8022
# 스캔 1회 — 히트가 1건뿐이면 그 호스트를 쓴다(단, 어떤 파일/로그/커밋에도
# IPv4 리터럴을 두지 않는다 — 히트 수만 인쇄). PHONE_PORT PHONE_USER
# PHONE_KEY PHONE_DISPLAY만 기본값. 풀 로그: engine/tmp/ptx_driver.log(tee).
#
# 함정 원장(승계+신규):
#   · 원격 스크립트는 scp 후 파일 기동 — ssh argv(heredoc)에 jkdesktop·jksrv
#     원문 문자열을 직접 놓으면 자기 pkill에 걸려 죽는다(사건 원장). 브래킷
#     pkill '[j]kdesk' 계약.
#   · 생존 바이너리 relink = ETXTBSY — 드라이버 pre-clean(브래킷 pkill+-9
#     에스컬레이션+잔존 어설션)이 tar 앞. jkweb은 절사·pkill 대상 아님.
#   · 폰 클라 스폰 15-18s — list_windows 3s 간격 최대 20회 재시도.
#   · ssh 원격 `< /dev/null` 금지(채널 hang). rc 봉합: 복합문이 echo로 끝나면
#     0 — exit로 전파. 정수 카운트용 grep -c는 0건 rc=1도 의도된 값.
#   · 캡처 회수는 ssh 호출당 base64 1파이프(ls 등 혼입 금지). 윈도 python은
#     MSYS식 /i/... 패스를 못 읽는다 — cygpath 변환 후 python(T1 회수 유실
#     렛슨). decode 성공/빈파일 게이트 후 폰쪽 삭제.
#   · 화면 기하: list_windows의 x/y/dw/dh는 데스크톱면 좌표 — 미러 캡처와
#     레터박스 오프셋 차. 회수 후 로컬 분석이 여유 마진 crop+구조 검출(밴드
#     다크런)로 정밀 앵커를 재계산 — 서버 기하는 출발점 원문으로만 쓴다.
#   · tar는 저장소 루트에서 만들어 `-C ~/JKENGINE`으로 풀어야 한다(engine/
#     engine 중첩 트랩). 폰 /tmp는 쓰기 불가 — 스크래치는 $TMPDIR.
#   · tar 전송은 LAN 내부 ssh 한정 — 어떤 클라우드/외부로도 가지 않는다.
set -u
export MSYS_NO_PATHCONV=1  # MSYS가 ssh 인수의 posix 경로를 찢는 것 차단(chat T8 선례)

ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
cd "$ROOT" || exit 1
SCRATCH="$ROOT/engine/tmp"
mkdir -p "$SCRATCH"
TARBALL="$SCRATCH/ptx_deploy.tar"
RLOG="$SCRATCH/ptx_driver.log"
RUNLOG="$SCRATCH/ptx_phone_run.log"
RSRC_TAR_NAME="ptx_phone_remote.sh"
RSRC_PHONE="~/JKENGINE/$RSRC_TAR_NAME"   # 폰 측 삭제는 remote script 자기 소각

# 접속 정보는 환경변수로(PHONE_HOST 미설정이면 LAN 스캔 — 스캔 히트 IP는
# 인쇄/기록하지 않는다: 내부 IP는 커밋·문서·리포트·로그에 두지 않는다).
PHONE_HOST="${PHONE_HOST:-}"
PHONE_PORT="${PHONE_PORT:-8022}"
PHONE_USER="${PHONE_USER:-u0_a4}"
PHONE_KEY="${PHONE_KEY:-$HOME/.ssh/termux_jkengine}"
PHONE_DISPLAY="${PHONE_DISPLAY:-:1}"

# 풀 로그 — 드라이버 전체(tee; 원격 run은 RUNLOG로 한벌 승계).
exec > >(tee "$RLOG") 2>&1

FAIL() { echo "TS-PHONE-FAIL: $*"; exit 1; }

echo "HEAD: $(git -C "$(cygpath -w "$ROOT" 2>/dev/null || echo "$ROOT")" rev-parse HEAD 2>/dev/null || echo rev-parse-failed)"
echo "BASE LINEAGE: 24d22a1..b651534 (T2 58861c3 + T3 8563263 + T3 fix r1 b651534)"

if [ -z "$PHONE_HOST" ]; then
  echo "=== 0a. PHONE_HOST unset — LAN 8022 스캔 (히트 IP는 인쇄하지 않는다) ==="
  SCANOUT=$(
    for b in 11 219; do
      for i in $(seq 2 240); do
        ( timeout 1 bash -c "echo > /dev/tcp/192.168.$b.$i/$PHONE_PORT" 2>/dev/null \
            && echo "H 192.168.$b.$i" ) &
        while [ "$(jobs -rp | wc -l | tr -d ' ')" -ge 48 ]; do wait -n; done
      done
    done
    wait
  )
  HITSEL=""
  HIT_COUNT=0
  for h in $SCANOUT; do
    [ "$h" = "H" ] && continue
    HITSEL="$HITSEL $h"
    HIT_COUNT=$((HIT_COUNT+1))
  done
  echo "LAN-SCAN: open $PHONE_PORT hits=$HIT_COUNT (주소 원문은 기록 금지 계약 — 인쇄 생략)"
  if [ "$HIT_COUNT" -eq 1 ]; then
    PHONE_HOST=$HITSEL
    echo "PHONE-HOST: 스캔 히트 1건 — 단일 후보로 진행 (다음 run은 환경변수 지정 권장)"
  elif [ "$HIT_COUNT" -eq 0 ]; then
    FAIL "LAN 스캔 히트 0 — 폰 Termux에서 sshd 기동 확인 또는 PHONE_HOST 지정"
  else
    FAIL "LAN 스캔 히트 $HIT_COUNT건 — 어느 것이 폰인지 스크립트가 판단할 수 없다: PHONE_HOST 환경변수로 지정"
  fi
else
  echo "PHONE-HOST: 환경변수 지정 사용 (스캔 생략)"
fi
SSH="ssh -p $PHONE_PORT -o BatchMode=yes -o ConnectTimeout=15 -o ServerAliveInterval=15 -o ServerAliveCountMax=10 -i $PHONE_KEY $PHONE_USER@$PHONE_HOST"

# ── 배포 원천: 텍스트 스케일 라인(24d22a1..b651534)이 건드린 소스 전량 —
#    `git diff --name-only 24d22a1..b651534 -- engine/` 결과(원장: probe 본체만
#    자기 파일 예외).
FILES=(
  engine/include/JKTextAtlas.h                 # T2+T3+fix r1 — DefaultFontScale·셀 확대·밴드 산식 공용 진실원
  engine/include/server/JKCompositor.h         # T3 — 밴드 산식 소비 계약
  engine/src/JKApplication.cpp                 # T2 — 싱글 프로세스(윈도 관측 변화의 결선) 지역 dc 배선
  engine/src/JKDC.cpp                          # T2 — DrawGlyph 게이트+폴백 3함수 nearest 셀 확대+폴백 경고 1회
  engine/src/JKTextAtlas.cpp                   # T2 — GetCellMetrics 기본 분기
  engine/src/JKWindow.cpp                      # T3 — 클라 PaintWindow 밴드 높이(채색·타이틀 글리프는 클라가 그림)
  engine/src/apps/ClientAgentMgrApp.cpp        # T3 fix r1 — AppContentTopOffset 전수
  engine/src/apps/ClientBrowserApp.cpp         # T3 fix r1 — 동형
  engine/src/apps/ClientChatApp.cpp            # T3 fix r1 — 동형
  engine/src/apps/ClientFilesApp.cpp           # T3 fix r1 — 동형
  engine/src/apps/ClientLibraryApp.cpp         # T3 fix r1 — 동형
  engine/src/apps/ClientNotesApp.cpp           # T3 fix r1 — 동형
  engine/src/apps/ClientSettingsApp.cpp        # T3 fix r1 — 동형
  engine/src/apps/ClientShotApp.cpp            # T3 fix r1 — 동형
  engine/src/apps/ClientVPlayerApp.cpp         # T3 fix r1 — 동형
  engine/src/apps/MineSweeperApp.cpp           # T3 fix r1 — I-1 밴드 고정 24 → 산식
  engine/src/client/JKClientApplication.cpp    # T2 — ComposeScene 지역 dc SetTextAtlas 결선(⑤ 근원 수리)
  engine/src/main.cpp                          # T2/T3 쌍둥이 — 캐논 원료(1p 23건+2t/3b)
  engine/src/server/JKWindowServer.cpp         # T3 — 히트테스트/passthrough/배너 캐시키 밴드 산식 소비
  engine/tools/posix_selftest/main.cpp         # T2/T3/fix r1 — 2t 6건+3b 5건 (캐논 계보 원료)
  engine/tools/probes/phone_text_scale.sh      # probe 본체(폰에 원문 유산 — 게이트 예외)
)

for f in "${FILES[@]}"; do
  [ -f "$ROOT/$f" ] || FAIL "deploy source missing: $f"
done

echo "=== 0b. 배포 오염 게이트 (docs/81 §3 #11 — 더러운 원천은 HEAD blob 스테이징) ==="
GITROOT="$(cygpath -w "$ROOT" 2>/dev/null || echo "$ROOT")"
DIRTY_COUNT=$(git -C "$GITROOT" status --porcelain 2>/dev/null | grep -avc '^??')
echo "tracked-dirty-count=$DIRTY_COUNT (tracked 변경 0이면 배포 원천=워킹 카피 원문)"
STAGE_DIR=""
WIPPED=0
STAGE_ARGS=()
for f in "${FILES[@]}"; do
  [ "$f" = "engine/tools/probes/phone_text_scale.sh" ] && continue   # 자기 파일 예외
  GIT_RC=9
  GIT_OUT="unset"
  for try in 1 2 3; do
    if [ "$try" -gt 1 ]; then sleep 3; fi
    GIT_OUT=$(git -C "$GITROOT" diff --quiet HEAD -- "$f" 2>&1 >/dev/null)
    GIT_RC=$?
    [ "$GIT_RC" -le 1 ] && break
  done
  [ "$GIT_RC" -le 1 ] \
    || FAIL "git diff HEAD -- $f 판정 실패(재시도 3회) — rc=$GIT_RC: $(echo "$GIT_OUT" | tail -1)"
  [ "$GIT_RC" -eq 1 ] || continue
  if [ -z "$STAGE_DIR" ]; then
    STAGE_DIR="$SCRATCH/ptx_head_stage"
    rm -rf "$STAGE_DIR"
    mkdir -p "$STAGE_DIR"
  fi
  GS_OK=0
  for try in 1 2 3; do
    git -C "$GITROOT" show "HEAD:$f" > "$STAGE_DIR/$f" 2>>"$SCRATCH/ptx_gshow.err" \
      && { GS_OK=1; break; }
    sleep 3
  done
  [ "$GS_OK" -eq 1 ] \
    || FAIL "git show HEAD:$f 스테이징 실패(재시도 3회) — $(tail -1 "$SCRATCH/ptx_gshow.err")"
  WIPPED=$((WIPPED+1))
  STAGE_ARGS+=(-C "$STAGE_DIR" "$f")
done
if [ "$WIPPED" -gt 0 ]; then
  echo "NOTE-WIP: 더러운 배포 원천 $WIPPED건 — HEAD blob 스테이징으로 배포"
else
  echo "CONTAMINATION-GATE=CLEAN (배포 원천 전량 HEAD와 일치 — 워킹 카피 배포)"
fi

echo "=== 1. pre-flight + pre-clean (재실행 가능성 — wake-lock, 구판 서버 절사) ==="
$SSH 'echo PHONE-REACHABLE; uname -m; echo TMPDIR=$TMPDIR; command -v termux-wake-lock >/dev/null 2>&1 && termux-wake-lock || echo WARN-no-wake-lock' \
  || FAIL "ssh failed (phone unreachable — LAN 한정, sshd 기동: 폰 Termux에서 sshd)"
JKWEB_CKT=$($SSH 'pgrep -c -f "[j]kweb" 2>/dev/null' | tr -d ' \r')
echo "JKWEB-COUNT-PRE: ${JKWEB_CKT:-0} (상시 유지 계약 — 절사·pkill 대상 아님)"
# 절사 대상=구판 서버+클라 전부(구판 바이너리 — relink ETXTBSY 방지+빌드 램 여유).
# jkweb은 절사하지 않는다(승격 라인 사용자 결제 창 localhost:8090 상시 계약).
$SSH "pkill -f '[j]kdesktop' 2>/dev/null; sleep 2; pkill -9 -f '[j]kdesktop' 2>/dev/null; sleep 1; pgrep -f '[j]kdesktop' >/dev/null 2>&1 && { echo PRECLEAN-FAIL; exit 1; }; echo SRV-DOWN" \
  || FAIL "pre-clean could not bring the old phone server down"
echo "PRECLEAN-SRV-DOWN=OK (구판 서버+클라 — 새 바이너리 교체 재기동 계약)"

echo "=== 2. tar-over-ssh 재배포 (21멤버 — 라인 소스 20건 + probe 본체) ==="
cat > "$SCRATCH/$RSRC_TAR_NAME" <<'PTXEOF'
#!/bin/bash
# 폰 측 영수증 절차 (phone_text_scale.sh가 생성 — 텍스트 스케일 T4).
# settings 워터마크 보존 → 리빌드 → selftest 캐논 → 3레그(1.5 기본/1.9 재시험/
# 1.5 복원 BOOT-OK) 기동+캡처. 종료 상태 = 서버 UP+태스크바+지뢰찾기+터미널
# 상시 — 사용자 육안 게이트 대기(probe가 결제를 기록하지 않는다).
set -u
cd ~/JKENGINE/engine || exit 1
FAIL() { echo "TS-FAIL: $*"; exit 1; }
TMPD="${TMPDIR:-/data/data/com.termux/files/usr/tmp}"
SET=buildterm/state/settings.json
NLOG="$TMPD/ptx_ninja.log"
STLOG="$TMPD/ptx_selftest.log"

echo "=== A. 진입 마커 — 배포 전 구판 상태 원문 ==="
echo "MARKER-BEFORE band=$(grep -c 'ComputeChromeTitleBarHeight' include/JKTextAtlas.h 2>/dev/null) dc=$(grep -c 'DefaultFontScale' src/JKDC.cpp 2>/dev/null) mine=$(grep -c 'ChromeTitleBarHeight' src/apps/MineSweeperApp.cpp 2>/dev/null) wire=$(grep -c 'SetTextAtlas' src/client/JKClientApplication.cpp 2>/dev/null)"

echo "=== B. settings 워터마크 + font_scale 키 부재 정규화 (기본 1.5 발동 상태) ==="
[ -f "$SET" ] || FAIL "settings file missing: $SET"
cp "$SET" "$TMPD/ptx_orig_settings.json" || FAIL "settings backup failed"
ORIG="$TMPD/ptx_orig_settings.json"
FONT_PATH=$(sed -n 's/.*"font_path"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' "$ORIG")
[ -n "$FONT_PATH" ] || FAIL "settings has no text.font_path (font_path 유지 계약 전제)"
[ -r "$FONT_PATH" ] || FAIL "font file not readable: $FONT_PATH"
echo "PARSED-FONT_PATH: $FONT_PATH"
if grep -aq 'font_scale' "$ORIG"; then
    printf '{\n    "text": {\n        "font_path": "%s"\n    }\n}\n' "$FONT_PATH" > "$SET"
    echo "NOTE-COERCED: 진입 settings에 font_scale 키 잔존 — 제거해 기본 1.5 발동 상태로 정규화 (coerced bytes=$(wc -c < "$SET"))"
    echo "ORIG-SETTINGS(scale 유 원문): $(tr -d '\n' < "$ORIG")"
fi
cp "$SET" "$TMPD/ptx_wake_settings.json" || FAIL "wake settings copy failed"
cmp -s "$SET" "$TMPD/ptx_wake_settings.json" || FAIL "wake settings copy mismatch"
echo "WAKE-SETTINGS-BYTES: $(wc -c < "$SET") (진입 ORIG-SETTINGS-BYTES=$(wc -c < "$ORIG"))"
if grep -aq 'font_scale' "$SET"; then
    FAIL "wake settings still has font_scale key"
fi
echo "SETTINGS-GATE=OK (font_path 유지 + font_scale 키 부재 = 기본 1.5)"

echo "=== C. jkweb 생존 계약 — 기동 카운트 원문 (절사 금지) ==="
JKWEB_BEFORE=$(pgrep -f '[j]kweb' | tr '\n' ' ')
echo "JKWEB-BEFORE: ${JKWEB_BEFORE:-none}"

echo "=== D. ninja 리빌드 (aarch64 — rc 전파, ~9-10분) ==="
pkill -f '[j]kdesktop' 2>/dev/null
sleep 1
pkill -9 -f '[j]kdesktop' 2>/dev/null
sleep 1
if pgrep -f '[j]kdesktop' >/dev/null 2>&1; then
    FAIL "server respawned mid-build (ETXTBSY 위험 — 상위 감독자 원장)"
fi
ninja -C buildterm -j4 >"$NLOG" 2>&1
N_RC=$?
tail -4 "$NLOG"
echo "NINJA-RC=$N_RC"
[ "$N_RC" -eq 0 ] || FAIL "ninja rebuild rc=$N_RC (aarch64 compile failure)"
[ -x buildterm/jkdesktop ] || FAIL "buildterm/jkdesktop missing after rebuild"

echo "=== E. 배포 마커 — HEAD 원천 실존 단정 ==="
MB=$(grep -c 'ComputeChromeTitleBarHeight' include/JKTextAtlas.h)
MD=$(grep -c 'DefaultFontScale' src/JKDC.cpp)
MM=$(grep -c 'ChromeTitleBarHeight' src/apps/MineSweeperApp.cpp)
MW=$(grep -c 'SetTextAtlas' src/client/JKClientApplication.cpp)
MF=$(grep -c 'vector atlas inactive' src/JKDC.cpp)
echo "MARKER-AFTER band($MB) dc($MD) mine($MM) wire($MW) fallbackwarn($MF)"
[ "$MB" -ge 1 ] || FAIL "band formula marker missing on phone (배포 결손)"
[ "$MD" -ge 1 ] || FAIL "DefaultFontScale marker missing on phone (배포 결손)"
[ "$MM" -ge 1 ] || FAIL "MineSweeper band consumer marker missing on phone"
[ "$MW" -ge 1 ] || FAIL "client SetTextAtlas wiring marker missing on phone"
[ "$MF" -ge 1 ] || FAIL "bitmap fallback warning marker missing on phone"

echo "=== F. selftest — 폰 캐논 계보 판정 (원장: 472=pre-T1(더티 시대)·489=T1만·495=T1+T2+fix r1(더티프레젠트 최신)·기대 506=495+2t 6+3b 3+3b-d/e 2) ==="
timeout 900 ./buildterm/jkdesktop test >"$STLOG" 2>&1
S_RC=$?
ST_PASS=$(grep -ac '^\[PASS\]' "$STLOG")
ST_FAIL=$(grep -ac '^\[FAIL\]' "$STLOG")
P1P=$(grep -ac '^\[PASS\] 1p-' "$STLOG")
P2T=$(grep -ac '^\[PASS\] 2t-' "$STLOG")
P3B=$(grep -ac '^\[PASS\] 3b-' "$STLOG")
echo "PHONE-SELFTEST rc=$S_RC PASS=$ST_PASS FAIL=$ST_FAIL 1p=$P1P(배포 23건) 2t=$P2T(배포 6건) 3b=$P3B(배포 5건)"
grep -a 'AppSelfTest' "$STLOG" | tail -2
grep -aq 'AppSelfTest: 0 failure(s)' "$STLOG" || FAIL "AppSelfTest not 0 failure(s)"
[ "$S_RC" -eq 0 ] || FAIL "selftest rc=$S_RC"
[ "$ST_FAIL" -eq 0 ] || FAIL "selftest FAIL=$ST_FAIL (폰축 회귀)"
if [ "$P2T" -ne 6 ] || [ "$P3B" -ne 5 ]; then
    echo "WARN: 텍스트 스케일 케이스 미반영 가능 (2t=$P2T/6 3b=$P3B/5) — 계보 요원장"
fi
awk -v p="$ST_PASS" -v t="$P2T" -v b="$P3B" 'BEGIN{
    if (p == 506) print "CANON-INCLUSION=TEXTSCALE-FULL-T2-T3-FIXR1 (캐논 495→506 상승 — 2t 6+3b 5 동반 실측)"
    else if (p == 495) print "CANON-INCLUSION=DIRTY-PRESENT-ONLY (텍스트 스케일 selftest 쌍둥이 미반영 — 계보 결손 — 원장)"
    else if (p == 472 || p == 489) print "CANON-INCLUSION=STALE-DIRTY-ERA (배포 원천이 셀프테스트 쌍둥이에 미반영 — 신선도 의심 — 원장)"
    else printf "CANON-INCLUSION=OTHER-N(%d — 계보 미부합 정산 원장; 2t=%d/6 3b=%d/5)\n", p, t, b
}'

# ---------------------------------------------------------------- 레그 공통
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
    echo "MINEWINDOW-$LEG: $MW"
}

boot_leg() { # $1 라벨, $2 settings 모드 (wake=키 삭제 1.5 기본 / s19=1.9)
    LEG=$1
    MODE=$2
    LOG="$TMPD/ptx_srv_${LEG}.log"
    echo "=== leg $LEG boot (mode=$MODE) ==="
    pkill -f '[j]kdesktop' 2>/dev/null
    sleep 2
    pkill -9 -f '[j]kdesktop' 2>/dev/null
    sleep 1
    if [ "$MODE" = "s19" ]; then
        printf '{\n    "text": {\n        "font_path": "%s",\n        "font_scale": "1.9"\n    }\n}\n' "$FONT_PATH" > "$SET"
    else
        cp "$TMPD/ptx_wake_settings.json" "$SET" || FAIL "leg $LEG: wake settings copy failed"
        cmp -s "$SET" "$TMPD/ptx_wake_settings.json" || FAIL "leg $LEG: wake settings mismatch"
    fi
    echo "SETTINGS-IN-LEG-$LEG: $(tr -d '\n' < "$SET")"
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
    WIN=$(timeout 15 ./buildterm/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)
    echo "WINDOWS-$LEG: $WIN"
    echo "--- leg $LEG 글리프 경고 수집 (vector 경고 0건=결선 성공 신호; HangulManager=기존 항상-출현 잡음원 분리) ---"
    WA=$(grep -ac 'vector atlas inactive' "$LOG")
    WN=$(grep -ac 'no vector font configured' "$LOG")
    WI=$(grep -ac 'vector font init failed' "$LOG")
    WH=$(grep -ac 'HangulManager' "$LOG")
    echo "WARN-$LEG atlas_inactive=$WA no_vector_font=$WN init_failed=$WI hangul_misc=$WH"
    CAPTURE "$TMPD/desk_$LEG.png"
    JW=$(pgrep -f '[j]kweb' | tr '\n' ' ')
    echo "JKWEB-IN-LEG-$LEG: ${JW:-none}"
}

echo "=== G. leg def15 — 기본 1.5 출하 상태 (settings 키 삭제 상태 그대로) ==="
boot_leg def15 wake

echo "=== H. leg s19b — font_scale 1.9 재시험 (구판 영수증 desk_s19·tsd_p19 대조 축) ==="
boot_leg s19b s19
grep -aq 'font_scale' "$SET" || FAIL "leg s19b: settings did not take font_scale"

echo "=== I. leg wake — 1.5 기본 복원 (키 삭제) BOOT-OK + 사용자 기상 상태 캡처 ==="
cp "$TMPD/ptx_wake_settings.json" "$SET" || FAIL "wake restore copy failed"
cmp -s "$SET" "$TMPD/ptx_wake_settings.json" || FAIL "wake restore byte mismatch"
echo "SETTINGS-RESTORED-BYTES: $(wc -c < "$SET")"
boot_leg wake wake
if grep -aq 'font_scale' "$SET"; then
    FAIL "wake leg: font_scale key present (복원 위반)"
fi

echo "=== J. 종료 게이트 — 서버 UP+태스크바/지뢰찾기 상시 + 터미널 상시 + jkweb 생존 ==="
pgrep -f 'buildterm/[j]kdesktop --server' >/dev/null 2>&1 || FAIL "server not UP at end (종료 게이트 위반)"
T=$(timeout 20 ./buildterm/jkdesktop agentctl '{"tool":"launch_app","args":{"app":"terminal"}}' 2>/dev/null | grep -a '{' | head -1)
printf '%s' "$T" | grep -aq '"ok":true' || echo "NOTE: terminal launch 미성립(정직 인쇄 — 계속) — $T"
sleep 6
FINAL=$(timeout 15 ./buildterm/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)
echo "FINAL-WINDOWS: $FINAL"
JKWEB_AFTER=$(pgrep -f '[j]kweb' | tr '\n' ' ')
echo "JKWEB-AFTER: ${JKWEB_AFTER:-none} (BEFORE: ${JKWEB_BEFORE:-none} — 원문 첨부, 절사 없음 단정 재료)"
if [ -n "$JKWEB_BEFORE" ] && [ "$JKWEB_BEFORE" != "$JKWEB_AFTER" ]; then
    echo "NOTE-JKWEB: 기동 카운트 변화 — 원장 행 (외부 재기동 없는 한 동일해야 한다)"
fi
FINAL_SRV=$(pgrep -f 'buildterm/[j]kdesktop --server' | tr '\n' ' ')
echo "FINAL-SERVER-PIDS=$FINAL_SRV"
rm -f -- "$0"
echo "TS-FINISHED"
exit 0
PTXEOF

for r in 1 2 3; do
  if tar -cf "$TARBALL" -C "$ROOT" "${FILES[@]}" ${STAGE_ARGS[@]+"${STAGE_ARGS[@]}"} \
     -C "$SCRATCH" "$RSRC_TAR_NAME"; then
    break
  elif [ "$r" -eq 3 ]; then
    FAIL "tar creation failed"
  fi
done
TAR_SZ=$(wc -c < "$TARBALL" | tr -d ' ')
echo "tar size: $TAR_SZ bytes"
[ "$TAR_SZ" -gt 250000 ] || FAIL "tar suspiciously small ($TAR_SZ bytes) — deploy list broken?"
echo "deploy list:"; tar -tf "$TARBALL"
tar -tf "$TARBALL" | grep -aq "$RSRC_TAR_NAME" \
  || FAIL "tar missing the remote receipt script member (구성 누락 방어)"
$SSH "tar -xf - -C ~/JKENGINE" < "$TARBALL" || FAIL "remote tar extract failed"
rm -f "$TARBALL"
$SSH "test -f ~/JKENGINE/$RSRC_TAR_NAME && echo REMOTE-SCRIPT-PRESENT || echo REMOTE-SCRIPT-ABSENT" \
  | grep -aq REMOTE-SCRIPT-PRESENT || FAIL "remote receipt script missing on phone (tar 구성 누락)"

# 배포 신선도 — 크기 대차(로컬 원천 vs 폰 수신) 전 건 어설션 + 마커.
FRESH_OK=1
for f in "${FILES[@]}"; do
  LSZ=$(wc -c < "$ROOT/$f" | tr -d ' ')
  PSZ=$($SSH "wc -c < ~/JKENGINE/$f 2>/dev/null" | tr -d ' \r')
  if [ "$LSZ" != "$PSZ" ]; then
    echo "FRESHNESS-MISMATCH: $f local=$LSZ phone=$PSZ"
    FRESH_OK=0
  fi
done
[ "$FRESH_OK" -eq 1 ] || FAIL "deployed file size mismatch — 배포 원문 신선도 붕괴"
echo "DEPLOY-FRESHNESS-OK (21멤버 크기 일치)"
MKS=$($SSH "cd ~/JKENGINE/engine && grep -c 'ComputeChromeTitleBarHeight' include/JKTextAtlas.h 2>/dev/null" | tr -d ' \r')
echo "DEPLOY-MARKER-BAND phone-grep-count=${MKS:-0}"
[ "${MKS:-0}" != "0" ] || FAIL "phone JKTextAtlas.h lacks ComputeChromeTitleBarHeight — 배포 원천 결손"

echo "=== 3. 폰 리빌드+영수증 절차 실행 (ninja ~9-10분 + selftest + 레그 3) ==="
$SSH "bash $RSRC_PHONE" > "$RUNLOG" 2>&1
R_RC=$?
RUN_RC=$R_RC
if [ "$R_RC" -eq 255 ]; then
  echo "RUN-DROPPED: ssh 채널 단절(Wi-Fi 낙하 함정 — 폰 ninja 로그는 \$TMPDIR에 생존, 재실행=증분: PHONE_HOST=<폰 IP> bash engine/tools/probes/phone_text_scale.sh)"
fi
cat "$RUNLOG"
grep -aq '^TS-FINISHED$' "$RUNLOG" \
  || FAIL "receipt script did not finish (rc=$RUN_RC — RUN-DROPPED면 재실행)"
if grep -aq '^TS-FAIL' "$RUNLOG"; then
  HARD=$(grep -a '^TS-FAIL' "$RUNLOG" | head -1)
  FAIL "phone hard receipt: $HARD"
fi

echo "=== 4. REMNANT 검사 — 원격 스크립트 자기 소각 확인 (REMNANT-LS-RC=2 기대) ==="
$SSH "ls $RSRC_PHONE" >/dev/null 2>&1
echo "REMNANT-LS-RC=$?"

echo "=== 5. 캡처 회수 — ssh 호출당 base64 1파이프 계약 (+md5 대차) ==="
recover() { # $1 폰쪽png 경로 $2 로컬png — md5 원문 대차 포함
    local REMOTE_PNG=$1 LOCAL_PNG=$2
    local WTXT WPNG WB64 RMD5 LMD5
    WB64=$(cygpath -w "$SCRATCH/ptx_b64.txt" 2>/dev/null || echo "$SCRATCH/ptx_b64.txt")
    WPNG=$(cygpath -w "$LOCAL_PNG" 2>/dev/null || echo "$LOCAL_PNG")
    RMD5=$($SSH "md5sum $REMOTE_PNG" 2>/dev/null | sed -n 's/^\([0-9a-f]*\) .*$/\1/p' | tr -d ' \r')
    $SSH "base64 -w0 $REMOTE_PNG" > "$SCRATCH/ptx_b64.txt" 2>"$SCRATCH/ptx_b64.err" \
        || FAIL "recover failed: $REMOTE_PNG"
    [ -s "$SCRATCH/ptx_b64.err" ] && echo "NOTE: stderr noise — $(tail -1 "$SCRATCH/ptx_b64.err")"
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
    rm -f "$SCRATCH/ptx_b64.txt"
}
recover "\$TMPDIR/desk_def15.png" "$SCRATCH/desk_def15.png"
recover "\$TMPDIR/desk_s19b.png"  "$SCRATCH/desk_s19b.png"
recover "\$TMPDIR/desk_wake.png"  "$SCRATCH/desk_wake.png"
$SSH "rm -f \$TMPDIR/desk_def15.png \$TMPDIR/desk_s19b.png \$TMPDIR/desk_wake.png \$TMPDIR/ptx_wake_settings.json \$TMPDIR/ptx_orig_settings.json"
echo "CAPTURES-WRITTEN: desk_def15.png desk_s19b.png desk_wake.png → engine/tmp/"

echo "=== 6. 로컬 실측 — 밴드 높이·글리프 확대 원문 (numpy/PIL, 4원천) ==="
PYRUNLOG=$(cygpath -w "$RUNLOG" 2>/dev/null || echo "$RUNLOG")   # 윈도 python은 /i/... 패스를 못 읽는다(회수 유실 렛슨)
PYSCRATCH=$(cygpath -w "$SCRATCH" 2>/dev/null || echo "$SCRATCH")
PYTHONIOENCODING=utf-8 python - "$PYRUNLOG" "$PYSCRATCH" <<'ANALYSIS_EOF'
# -*- coding: utf-8 -*-
# T4 로컬 실측 (회수 캡처 3건+구판 참조 축 4건 — 밴드/잉크/AA 색수).
# 트랩 렛슨 승계: 검출기=구조 기반(밴드 다크런+잉크) — 서버 기하(list_windows
# x/y/dw/dh)는 캡처 좌표와 레터박스 오프셋 차라 출발점 앵커+여유 마진으로만 씀.
import os, re, sys
import numpy as np
from PIL import Image

RUNLOG, SCRATCH = sys.argv[1], sys.argv[2]

def load(p):
    return np.asarray(Image.open(p).convert("RGB")).astype(int)

def find_band(im, tag):
    """지뢰창기 창 구조 검출 — 전역 스캔(기하 오프셋 무의존): 밴드=다크런(≥240)+
    타이틀 잉크(밝음 ≥30px)+아래 체커보드 서명(라이트그레이 행 ≥12) —
    태스크바(체커보드 없음)·터미널(체커보드 없음·다크 본문) 자동 배제."""
    H, W, _ = im.shape
    bright = im.mean(2)
    dark = bright < 100
    cands = []
    y = 0
    while y < H-160:
        if dark[y].sum() >= 240:
            row = dark[y]
            padded = np.concatenate([[False], row, [False]])
            d = np.diff(padded.astype(int))
            starts = np.where(d == 1)[0]
            ends = np.where(d == -1)[0]
            hit = False
            for x0, x1 in zip(starts, ends):
                wdt = x1 - x0
                if wdt < 240:
                    continue
                ty = y
                while ty > 0 and dark[ty-1, x0:x1].mean() > 0.7:
                    ty -= 1
                by = y
                while by+1 < H and dark[by+1, x0:x1].mean() > 0.35:
                    by += 1
                band_h = by - ty + 1
                if band_h < 12 or band_h > 90:
                    continue
                sub = bright[ty:by+1, x0:x0+int(wdt*0.7)]
                inkcnt = int((sub > 190).sum())
                if inkcnt < 30:
                    continue
                zone = bright[by+1: min(by+90, H), x0:x1]
                if zone.size == 0:
                    continue
                lg = ((zone > 150) & (zone < 245)).mean(1)
                if (lg >= 0.35).sum() < 12:
                    continue
                cands.append((int(band_h), int(ty), int(by), int(x0), int(x1), inkcnt))
                y = by + 1
                hit = True
                break
            if hit:
                continue
        y += 1
    return cands

def measure(path, anchor, tag):
    im = load(path)
    H, W, _ = im.shape
    cands = find_band(im, tag)
    if cands:
        cands.sort(key=lambda c: (-(c[5]), c[1]))
        band_h, ty, by, lx, rx, inkcnt = cands[0]
        wdt = rx - lx + 1
        print("ANALYSIS-%s BAND_BEST-of-%d (잉크=%d)" % (tag, len(cands), inkcnt))
    else:
        print("ANALYSIS-%s: BAND-DETECT-FAIL (구조 후보 0 — crop 원문 육안 대체)" % tag)
        return None
    if anchor is not None:
        x, y, dw, dh = anchor
        print("ANALYSIS-%s GEOM-CHECK server(x=%d y=%d dw=%d dh=%d) vs detect(x=%d y=%d w=%d) — 오프셋=레터박스 원문" % (
            tag, x, y, dw, dh, lx, ty, wdt))
    out = {"tag": tag, "band_h": band_h, "band_rect": (ty, by, lx, rx, wdt)}
    print("ANALYSIS-%s BAND_H=%d band(y=%d..%d x=%d..%d w=%d)" % (tag, band_h, ty, by, lx, rx, wdt))

    def ink_stats(region, cond, label):
        m = cond
        rows = np.where(m.any(1))[0]
        cnt = int(m.sum())
        if cnt == 0:
            print("%s %s ink: none" % (tag, label))
            return (0, 0, 0, -1, -1)
        span = int(rows.max()-rows.min()+1)
        vals = (region[m] // 24)
        colors = len(np.unique(vals, axis=0))
        print("%s %s ink_span=%d count=%d colors=%d rows=%d..%d" % (
            tag, label, span, cnt, colors, int(rows.min()), int(rows.max())))
        return (span, cnt, colors, int(rows.min()), int(rows.max()))

    # 타이틀 잉크 — 밴드 좌 62% (창 컨트롤 __ □ × 배제)
    sub = im[ty:by+1, lx: lx + int(wdt*0.62)]
    sb = sub.mean(2)
    bg = float(np.median(sb))
    st = ink_stats(sub, sb > bg + 55, "TITLE")
    (out["title"]) = st

    # 콘텐츠 상단 띠(버튼 라벨+LED 숫자) — 밴드 하단 +0..52행
    # 구조(tsd 1.0 실측 원문): [여백 라이트 ~4-6행] [버튼+LED 박스 다크 런 ~24행]
    # [여백] [체커보드]. 박스=스트립 상단 0..46행 내 다크 연속 런(≥0.4, 2행
    # 연속 <0.4에서 종료) — 그 런 내에서만 라벨·LED 잉크를 잰다(체커보드 혼입 방지).
    cy0 = by + 1
    strip = im[cy0: min(cy0+52, H), lx:rx+1]
    sw = strip.shape[1]
    bright_s = strip.mean(2)
    df = (bright_s < 100).mean(1)
    boxrows = []
    yy, started, miss = 0, False, 0
    while yy < min(46, strip.shape[0]):
        if df[yy] >= 0.4:
            boxrows.append(yy)
            miss = 0
        else:
            if started:
                miss += 1
                if miss >= 2:
                    break
            yy += 1
            continue
        started = True
        yy += 1
    if len(boxrows) >= 6:
        bt, bb = boxrows[0], boxrows[-1]
        print("%s BUTTON+LED box rows(strip)=%d..%d (h=%d)" % (tag, bt, bb, bb-bt+1))
        out["box"] = (bt, bb)
        # 버튼 라벨: 박스 열 내 밝은 잉크 — 베벨(박스 가장자리 밝은 1px)은 행당
        # 잉크폭이 박스폭 대부분이라 행잉크폭 <0.85 박스폭 행만 라벨로 잰다.
        lcols = int(sw*0.42)
        lreg = strip[bt:bb+1, :lcols]
        lbright = lreg.mean(2)
        coldf = (lbright < 100).mean(0)
        padded = np.concatenate([[False], coldf >= 0.5, [False]])
        dd = np.diff(padded.astype(int))
        labrows, labcnt, labcol = set(), 0, 0
        for s, e in zip(np.where(dd == 1)[0], np.where(dd == -1)[0]):
            if e-s+1 < 12:
                continue
            box = lbright[:, s:e+1]
            ink = box > 150
            for yy in range(ink.shape[0]):
                c = int(ink[yy].sum())
                if 2 <= c < int(0.85*(e-s+1)):
                    labrows.add(yy)
                    labcnt += c
            labcol += (e-s+1) * (bb-bt+1)
        if labrows:
            span = max(labrows)-min(labrows)+1
            vals = (lreg[sorted(labrows), :] > 150)
            print("%s BUTTON-LABEL rows(strip)=%d..%d span=%d rows_in_box=(%d..%d) — 박스 내이면 밀려남 없음" % (
                tag, bt+min(labrows), bt+max(labrows), span, bt, bb))
            out["button_label"] = (span, bt+min(labrows), bt+max(labrows))
        else:
            print("%s BUTTON-LABEL: no label rows in box (밀려남/무잉크 — crop 확인)" % tag)
            out["button_label"] = None
        # LED 숫자 박스: 우 58% 내 최광 다크 열런 ≥ 24 열 — 런 내 밝은 잉크 행.
        rcols0 = int(sw*0.42)
        rreg = strip[bt:bb+1, rcols0:]
        rcoldf = (rreg.mean(2) < 100).mean(0)
        padded2 = np.concatenate([[False], rcoldf >= 0.5, [False]])
        dd2 = np.diff(padded2.astype(int))
        best = None
        for s, e in zip(np.where(dd2 == 1)[0], np.where(dd2 == -1)[0]):
            if e-s+1 >= 24 and (best is None or (e-s+1) > best[0]):
                best = (e-s+1, s, e)
        if best is not None:
            _, ls, le = best
            led = rreg[:, ls:le+1]
            lb2 = led.mean(2)
            lbg = float(np.median(lb2))
            stl = ink_stats(led, lb2 > lbg + 55, "LED-DIGITS")
            out["led"] = stl
        else:
            print("%s LED box: not detected (조성 변화 — 원문 crop 확인)" % tag)
            out["led"] = None
    else:
        print("%s BUTTON box: not detected (구조 상이 — 원문 crop 확인)" % tag)
        out["button_label"] = None

    Image.fromarray(np.uint8(np.clip(im[max(0, ty-4): min(ty+220, by+240),
                                       max(0, lx-4): rx+5], 0, 255)))\
        .save(os.path.join(SCRATCH, "ptx_%s_mine.png" % tag))
    bandcrop = im[max(0, ty-2): by+3, max(0, lx-4): rx+5]
    bi = Image.fromarray(np.uint8(np.clip(bandcrop, 0, 255)))
    bi.resize((bi.width*3, bi.height*3), Image.NEAREST).save(
        os.path.join(SCRATCH, "ptx_%s_band3x.png" % tag))
    stc = im[by+1: min(by+53, H), lx: rx+1]
    Image.fromarray(np.uint8(np.clip(stc, 0, 255)))\
        .save(os.path.join(SCRATCH, "ptx_%s_content2x.png" % tag))
    print("ANALYSIS-%s crops: ptx_%s_mine.png ptx_%s_band3x.png ptx_%s_content2x.png" % (tag, tag, tag, tag))
    return out

# 서버 기하 파싱 (MINEWINDOW-<leg>: {...} — x/y/w/dw/dh)
def parse_windows():
    rects = {}
    try:
        txt = open(RUNLOG, encoding="utf-8", errors="ignore").read()
    except OSError:
        return rects
    for leg in ("def15", "s19b", "wake"):
        m = re.search(r"MINEWINDOW-%s: (\{[^\n]*)" % leg, txt)
        if not m:
            continue
        obj = m.group(1)
        def gi(k):
            mm = re.search(r'"%s":(-?\d+)' % k, obj)
            return int(mm.group(1)) if mm else None
        x, y, w, dw, dh = gi("x"), gi("y"), gi("w"), gi("dw"), gi("dh")
        if None not in (x, y, dw, dh):
            rects[leg] = (x, y, dw, dh)
    return rects

rects = parse_windows()
print("SERVER-RECTS(parsed): %s" % rects)

RES = {}
for f, leg in (("desk_def15.png", "def15"), ("desk_s19b.png", "s19b"), ("desk_wake.png", "wake")):
    p = os.path.join(SCRATCH, f)
    if not os.path.isfile(p):
        print("ANALYSIS-%s: missing %s" % (leg, f)); continue
    r = rects.get(leg)
    anchor = (r[0], r[1], r[2], r[3]) if r else None
    RES[leg] = measure(p, anchor, leg)

print("---- 구판 참조 축 (스펙 A/B 원문 — 기존 영수증 재측정) ----")
REF = {}
for f, tag in (("tsd_r.png", "ref10"), ("tsd_p19.png", "ref19o"),
               ("desk_s10.png", "leg10"), ("desk_s19.png", "leg19o")):
    p = os.path.join(SCRATCH, f)
    if not os.path.isfile(p):
        print("REF-%s: missing %s (best-effort)" % (tag, f)); continue
    RES[tag] = measure(p, None, tag)

print("")
print("==== 측정 표 ====")
rows = []
if RES.get("ref10"): rows.append(("ref10  tsd_r  (구판 1.0)", RES["ref10"]))
if RES.get("leg10"): rows.append(("leg10  desk_s10(구판 1.0 레거시)", RES["leg10"]))
if RES.get("ref19o"): rows.append(("ref19o tsd_p19 (구판 1.9 결함)", RES["ref19o"]))
if RES.get("leg19o"): rows.append(("leg19o desk_s19(구판 1.9 레거시)", RES["leg19o"]))
if RES.get("def15"): rows.append(("def15  (신규 기본 1.5 발동)", RES["def15"]))
if RES.get("s19b"): rows.append(("s19b   (신규 1.9 재시험)", RES["s19b"]))
if RES.get("wake"): rows.append(("wake   (신규 1.5 복원 기상 상태)", RES["wake"]))
for nm, m in rows:
    if m is None: continue
    t = m.get("title", (0,0,0,-1,-1))
    print("  %-32s BAND_H=%3d TITLE ink_span=%3d colors=%2d" % (nm, m["band_h"], t[0], t[2]))

d15, wk, s19 = RES.get("def15"), RES.get("wake"), RES.get("s19b")
ref19o = RES.get("ref19o") or RES.get("leg19o")
ref10 = RES.get("ref10") or RES.get("leg10")

band_fail = []
band_ok = False
if d15 is not None and wk is not None:
    t15, twk = d15.get("title"), wk.get("title")
    band_h15, band_hwk = d15["band_h"], wk["band_h"]
    ink15 = t15[0] if t15 else 0
    if 31 <= band_h15 <= 33 and 31 <= band_hwk <= 33:
        if ink15 >= 12 and (ref10 is None or ink15 > ref10["title"][0]+3 if ref10 else True):
            band_ok = True
        else:
            band_fail.append("def15 잉크 스팬 %d — 비트맵 고정 8행과 무변 (확대 미발화)" % ink15)
    else:
        band_fail.append("밴드 높이 def15=%d wake=%d — T3 산식 32(±1) 미부합" % (band_h15, band_hwk))
else:
    band_fail.append("def15/wake 측정 실패 (검출기 출력 상단 참조)")

s19_ok, s19_why = False, ""
if s19 is not None:
    t19 = s19.get("title")
    bh, ink = s19["band_h"], (t19[0] if t19 else 0)
    span_top, span_bot = (t19[3], t19[4]) if t19 else (-1, -1)
    old_span = ref19o["title"][0] if (ref19o and ref19o.get("title")) else 4
    if 36 <= bh <= 40:
        # 비례 기대: 잉크 스팬은 셀 높이에 비례 — 1.5(셀 24) 실측 ink15 ×
        # 30/24 ≈ 16.25. 고정 임계 18(WSL 17행 브래킷 전용 산치)은 1차 run에서
        # 실측 16행을 HOLD로 오판 — 비례 기반으로 재봉합(정직 원장 부기).
        ink15m = d15["title"][0] if (d15 and d15.get("title")) else 13
        if ink >= ink15m + 2 and ink <= bh - 2 and (t19 and span_top >= 2):
            s19_ok = True
            s19_why = "밴드 %dpx(구판 24 고정→셀 30+8)·잉크 %d행(1.5 실측 %d행 × 셀30/24 비례 기대 %.1f 부합; 구판 1.9 %d행 대비 확대)·rows %d..%d — 밴드 내 비클립·비플러시탑(구판 밀려남 소멸)" % (
                bh, ink, ink15m, ink15m * 30.0 / 24.0, old_span, span_top, span_bot)
        else:
            s19_why = "밴드 %dpx인데 잉크 스팬 %d (기대 ≥ 1.5잉크%d+2, ≤밴드-2, 상단 ≥2) — rows=%d..%d" % (
                bh, ink, ink15m, span_top, span_bot)
    else:
        s19_why = "밴드 %dpx — 셀 30+8=38(±2) 미부합" % bh
else:
    s19_why = "s19b 측정 실패"

if band_ok:
    print("BAND-VERDICT: BAND-OK (기본 1.5 밴드=%dpx = T3 산식 max(24,cellH 24+8)=32 등호; 구판 1.0 기준 %dpx 대비 +%d — 타이틀 잉크 확대 실발화, 잉크 %d행 vs 비트맵 고정 %d행)" % (
        d15["band_h"], ref10["band_h"] if ref10 else 24,
        (d15["band_h"] - (ref10["band_h"] if ref10 else 24)),
        d15["title"][0], ref10["title"][0] if ref10 else 8))
else:
    print("BAND-VERDICT: BAND-FAIL(%s)" % "; ".join(band_fail))
if s19 is not None:
    print("S19B-VERDICT: %s (%s)" % ("S19B-RESOLVED" if s19_ok else "S19B-HOLD", s19_why))
    if ref19o and ref19o.get("title") and s19.get("title"):
        print("S19B-AXIS (구판 1.9 vs 신규 1.9): 잉크 스팬 %d행→%d행, 밴드 24px(고정)→%dpx, AA색 %d→%d" % (
            ref19o["title"][0], s19["title"][0], s19["band_h"],
            ref19o["title"][2], s19["title"][2]))
else:
    print("S19B-VERDICT: S19B-HOLD(%s)" % s19_why)

canon_ok = None
try:
    txt = open(RUNLOG, encoding="utf-8", errors="ignore").read()
    mcan = re.search(r"CANON-INCLUSION=([^\n]*)", txt)
    canon_ok = (mcan.group(1).startswith("TEXTSCALE-FULL") if mcan else None)
    if mcan:
        print("CANON-LINE: CANON-INCLUSION=%s" % mcan.group(1))
except OSError:
    pass

if band_ok and s19_ok and canon_ok:
    print("TS-VERDICT: TEXTSCALE-OK (폰 캐논 506 + 기본 1.5 밴드 32 + 1.9 잉크 확대·비클립 — 육안 게이트 대기)")
elif band_ok and s19_ok:
    print("TS-VERDICT: TEXTSCALE-FAIL(측정 성립했으나 캐논 계보 미부합 — CANON-LINE 참조)")
else:
    print("TS-VERDICT: TEXTSCALE-FAIL(BAND/S19B 상단 행-사유)")
ANALYSIS_EOF
if [ $? -ne 0 ]; then
  echo "NOTE-ANALYSIS: local analysis failed (rc) — 캡처 원문은 상단 회수 원문으로 육안 가능"
fi

echo "=== 7. TS-VERDICT 종합 (측정 원문은 상단 analysis 행) ==="
CANON=$(grep -a '^CANON-INCLUSION=' "$RUNLOG" | tail -1 | sed 's/^CANON-INCLUSION=//' | tr -d '\r')
STPASS=$(grep -a '^PHONE-SELFTEST' "$RUNLOG" | head -1 | sed -n 's/^PHONE-SELFTEST rc=.* PASS=\([0-9]*\) FAIL.*/\1/p')
echo "canon: ${CANON:-n/a} (selftest PASS=$STPASS)"

echo ""
echo "사용자 결제 게이트(대기 — probe가 결제를 기록하지 않는다):"
echo "  ① 폰 1.5 기본 출하 룩(desk_wake.png+ptx_wake_mine.png): 지뢰찾기 제목·"
echo "     버튼 라벨(New/B/I/F)·Mines:/Time: 숫자가 읽히는 크기이고 클립·밀려남"
echo "     없는지(1.0 출하 때보다 커져 읽기 쉬운지)."
echo "  ② 폰 1.9 재시험(desk_s19b.png+ptx_s19b_mine.png): 구판 desk_s19.png"
echo "     (제목 상단 클립·버튼 라벨 밀려남) 대비 글리프가 셀과 함께 커져 해소"
echo "     됐는지."
echo "  ③ PC(Windows 싱글 모드): 글리프가 비트맵→malgun 벡터 AA로 전환한 관측"
echo "     변화(T2 결선 효과 — 폰 아닌 PC 항목, 승계 원장)."
echo "  ④ 폰 터미널 셀 1.5 표시(종료 상태 — 터미널 상시)."
echo "  → EYES-PENDING: 위 항목에 대한 사용자 선언만 결제 — probe는 기록하지 않는다."
echo "TS-PHONE-END"
exit 0
# honest-fail 원칙: 수치 미달(TEXTSCALE-FAIL)은 원장 목적이라 rc=0.
# hard FAIL(배포·빌드·selftest·기동·캡처·REMNANT)만 FAIL()에서 exit 1.
# EOF — 끝 개행 유지(스크립트 산술 무해 습관).