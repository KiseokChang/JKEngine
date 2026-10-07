#!/usr/bin/env bash
# 폰 앱 커버리지 실측 probe (앱 커버리지 라인 Task 4 — docs/81 §1·§2.3의 미실측 폰 축).
# 영수증 목표: ① tar-over-ssh 재배포(T1/T3 변분 10파일 — main.cpp jkx-pack posix
#   leg·CMakeLists dual-path repack·JKLibraryCatalog cmd_posix·sampletodo 3파일·
#   조달기·T5 taskmgr/JKClientSurface 신선도·본 probe) ② 폰 aarch64 ninja 리빌드
#   ③ AppSelfTest 0 failure(s)(폰 축 캐논 첫 실측) ④ **auto-repack 폰 CMake 레일
#   도달 판정**(태스크의 진실 질문 — REACHED/PARTIAL/NOT-REACHED를 정직 채점하고
#   못 도달한 몫은 수동 pack 폴백으로 채운 뒤 그 판정을 인쇄) ⑤ jkx-pack 수동 1회
#   실측(posix dlopen leg — rc=0 + packed 행 + jkx-list TOC + negative path)
# ⑥ library-list count 실측(~29 목표 — 조성 = launcher .jkx 24 + phoneprobe 유산 +
#   콘솔 트윈 + chat builtin + 조달 성공분; 어설션은 실측 조성으로 자기합산)
# ⑦ launch_app jkx 2종(settings·notes) → list_windows 창 오브젝트 단정
# ⑧ 서버·창을 **켜 둔 채 종료**(사용자 육안 게이트 — teardown은 probe 임시 파일만).
#   실행법(윈도 Git Bash, 저장소 루트 어디서든):
#     bash engine/tools/probes/phone_apps.sh
#   접속 정보는 환경변수로(PHONE_HOST 필수 — 내부 IP는 커밋하지 않는다, fix r1
#   M4 정화; PHONE_PORT PHONE_USER PHONE_KEY만 기본값 존재):
#     PHONE_HOST=<폰 IP> bash engine/tools/probes/phone_apps.sh
#   재실행 가능: 서버가 살아 있으면 선 절사(bracketed pkill — jkweb은 절사 안 함,
#   preclean 전후 생존 어설션) 후 재부팅. 영수증 하나라도 빠지면 APPS-PHONE-FAIL
#   (rc=1). 구조 = phone_library.sh 기계: 이 파일은 윈도 측 드라이버이고 폰 측
#   절차는 engine/tmp/phone_apps_remote.sh(스크래치 — gitignore/무추적)로 생성해
#   tar로 밀어 넣고 `bash ~/JKENGINE/phone_apps_remote.sh`로 폰에서 실행한다.
#   함정 원장(docs/81 §3 — T1-T5·chat T7 실측 승계): tar는 저장소 루트에서 만들어
#   `-C ~/JKENGINE`으로 풀어야 한다(engine/engine 중첩 트랩). CMakeLists를 배포하되
#   그 안이 참조하는 신규 소스를 누락하면 폰 cmake 재생성이 `No SOURCES given to
#   target`로 사망(T7 실측) — 배포 목록은 §3 #6 원칙(T1/T3/T5 신규 소스 전량 동반).
#   agentctl 와이어는 서브커맨드 agentctl·키 tool/args. 폰 /tmp는 쓰기 불가 —
#   원격 스크래치는 $TMPDIR(Termux 기본 존재, 실측). 조달 바이너리 glibc/bionic
#   불일치 가능성은 조달 단계(D)에서 실측 판정 — dead 바이너리는 apps-bin에 두지
#   않는다(존재 게이트=existence만이라 죽은 내장 행이 카탈로그에 남는 오염 방지).
#   tar 전송은 LAN 내부 ssh 한정 — 어떤 클라우드/외부로도 가지 않는다.
set -u
export MSYS_NO_PATHCONV=1  # MSYS가 ssh 인수의 posix 경로를 찢는 것 차단(chat T8 선례)

ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
cd "$ROOT" || exit 1
# git 네이티브(git.exe) 인수 경로 — MSYS_NO_PATHCONV=1 위에서도 유효한
# Windows형 경로로 준다(run 5a/5b 실측: -C /i/... posix형은 git.exe가
# "cannot change to"로 거부 → 게이트 오판 원인). cygpath 부재 시 조용한 폴백.
GITROOT="$(cygpath -w "$ROOT" 2>/dev/null || echo "$ROOT")"
SCRATCH="$ROOT/engine/tmp"
mkdir -p "$SCRATCH"
TARBALL="$SCRATCH/phone_apps.tar"
RLOG="$SCRATCH/phone_apps_run.log"
RSRC_TAR_NAME="phone_apps_remote.sh"
RSRC_PHONE="~/JKENGINE/$RSRC_TAR_NAME"   # 폰 측 삭제는 remote script 마지막 단계(임시 파일만)

PHONE_HOST="${PHONE_HOST:-}"
PHONE_PORT="${PHONE_PORT:-8022}"
PHONE_USER="${PHONE_USER:-u0_a4}"
PHONE_KEY="${PHONE_KEY:-$HOME/.ssh/termux_jkengine}"
# IP 정화(fix r1 M4 — 내부 IP는 커밋하지 않는다, phone_library.sh:32 3번째 사본
# 회피): HOST 기본값 공백+미설정 FAIL — 실행은 `PHONE_HOST=<폰 IP> bash …`로.
[ -n "$PHONE_HOST" ] || { echo "APPS-PHONE-FAIL: PHONE_HOST unset — 폰 IP를 환경변수로 지정하세요 (내부 IP는 커밋하지 않는다: fix r1 M4)"; exit 1; }
SSH="ssh -p $PHONE_PORT -o BatchMode=yes -o ConnectTimeout=15 -o ServerAliveInterval=15 -o ServerAliveCountMax=10 -i $PHONE_KEY $PHONE_USER@$PHONE_HOST"

FAIL() { echo "APPS-PHONE-FAIL: $*"; exit 1; }

# ── 배포 원천: BASE(9a2174e — chat 라인 원장) 이후 이 라인이 건드린 소스 전량
#    (git diff 9a2174e..HEAD --stat 실측 기록) + probe 본체. phone 소스는
#    9a2174e 시점이라 전부 신선도 재배포(T1/T3 변분 — brief 계약).
FILES=(
  engine/src/main.cpp                          # T1 jkx-pack posix leg + T3 selftest 1m-t
  engine/CMakeLists.txt                        # T1 repack dual-path + T3 console_apps
  engine/src/JKLibraryCatalog.cpp              # T3 cmd_posix 확장 계약
  engine/apps/sampletodo/manifest.json         # T3 cmd_posix 1행
  engine/apps/sampletodo/sampletodo.cmd        # 트윈 쌍쌍 — console_apps가 참조
  engine/apps/sampletodo/sampletodo.sh         # T3 신설 트윈
  engine/scripts/install_lf_helix_posix.sh     # T3 조달기(aarch64 자산 핀 기록 — D 절 재용)
  engine/src/apps/ClientTaskmgrApp.cpp         # T5 /proc leg(신선도 — §3 #6 원칙)
  engine/src/client/JKClientSurface.cpp        # T5 Hello pid 보수(신선도)
  engine/tools/probes/phone_apps.sh            # probe 본체(폰에 원문 유산)
)

for f in "${FILES[@]}"; do
  [ -f "$ROOT/$f" ] || FAIL "deploy source missing: $f"
done

# ── 배포 오염 게이트(T4 fix r1 — 병렬 세션 WIP 유입 봉합): 타 세션의 미커밋
#    WIP를 타르에 실지 않는다. run 5 실측: 병렬 chat-close-fix 세션의 미커밋
#    main.cpp(1n-s 블록 — AgentWindowRef 참조)가 폰의 커밋 상태 헤더
#    (JKWindowServer.h — 배포 목록 밖, AgentWindowRef 미신설)와 어긋나 폰
#    빌드를 깠다(NINJA-RC=1, undeclared identifier main.cpp:3332/3341).
#    원칙: 배포 원천은 커밋 상태 — 워킹 카피가 더러우면 HEAD blob 스테이징.
#    단 probe 본체(자기 파일)는 예외 — 워킹 카피를 실어 최신 수리본 유지.
#    스테이징 원문은 타르 끝에 추가(-C)하여 추출 순서상 나중 항목이 이긴다.
WIPPED=0
STAGE_ARGS=()
for f in "${FILES[@]}"; do
  [ "$f" = "engine/tools/probes/phone_apps.sh" ] && continue
  # git 판정 재시도(공유 repo — 병렬 세션의 index/commit-graph 쓰기 경합 가능,
  # run 5a 실측: 일시 git 불능이 dirty 분기 진입+staging 실패로 이어짐).
  # 분류는 rc로 — 0(클린)/1(더러움)은 정상, 2+는 git 불능 → 재시도 3회 후
  # loud FAIL(오타 분류 아님; 경고행은 stderr에서 무해 통과 — commit-graph
  # 경고가 상존하는 repo 실측). 판정 재시도 3회.
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
  if [ "$f" = "engine/src/main.cpp" ]; then
    STAGE="$ROOT/engine/tmp/head_stage"
    rm -rf "$STAGE"
    mkdir -p "$STAGE/engine/src"
    GS_OK=0
    for try in 1 2 3; do
      git -C "$GITROOT" show "HEAD:engine/src/main.cpp" > "$STAGE/engine/src/main.cpp" \
        2>>"$SCRATCH/head_gshow.err" && { GS_OK=1; break; }
      sleep 3
    done
    [ "$GS_OK" -eq 1 ] \
      || FAIL "git show HEAD:engine/src/main.cpp 스테이징 실패(재시도 3회) — $(tail -1 "$SCRATCH/head_gshow.err")"
    STAGE_ARGS=(-C "$STAGE" "engine/src/main.cpp")
    WIPPED=1
  else
    FAIL "deploy source dirty — 타 세션 미커밋 WIP 의심, 수동 확인 필요: $f (원칙: 커밋 상태만 배포)"
  fi
done
[ "$WIPPED" -eq 1 ] && \
  echo "NOTE-WIP: main.cpp 워킹 카피는 미커밋 WIP(병렬 세션) — HEAD blob으로 배포(스테이징)"

echo "=== 0. pre-clean (재실행 가능성) — wake-lock + 서버 절사(jkweb은 절사 안 함) ==="
$SSH 'command -v termux-wake-lock >/dev/null 2>&1 && termux-wake-lock || echo WARN-no-wake-lock' \
  || FAIL "ssh failed (phone unreachable?)"
# jkweb 생존 원장(사용자 육안 게이트 진행 중 — 이 probe가 죽이지 않는다):
# pkill 패턴은 jkdesktop만 겨냥 — 폰 pgrep 함정(toybox -x 놓침) 회피는 -f 계열,
# 인라인 ssh 자기매칭은 브래킷 [j] 트릭(phone_library.sh 선례).
$SSH "pgrep -f '[j]kweb' >/dev/null 2>&1 && echo JKWEB-ALIVE-BEFORE || echo JKWEB-ABSENT-BEFORE" \
  || FAIL "jkweb liveness probe failed"
$SSH "pkill -f 'buildterm/[j]kdesktop' 2>/dev/null; sleep 2; pkill -9 -f 'buildterm/[j]kdesktop' 2>/dev/null; sleep 1; pgrep -f 'buildterm/[j]kdesktop' >/dev/null 2>&1 && { echo PRECLEAN-FAIL; exit 1; } || echo SRV-DOWN" \
  || FAIL "pre-clean could not bring server down"
$SSH "pgrep -f '[j]kweb' >/dev/null 2>&1 && echo JKWEB-ALIVE-AFTER || { echo JKWEB-KILLED-BYPRECLEAN; exit 1; }" \
  || FAIL "pre-clean killed jkweb (사용자 육안 게이트 침해)"

echo "=== 1. tar-over-ssh 재배포 (10파일 + 폰 측 영수증 스크립트) ==="
cat > "$SCRATCH/$RSRC_TAR_NAME" <<'PHONEEOF'
#!/bin/bash
# 폰 측 영수증 절차 (phone_apps.sh가 생성 — 앱 커버리지 Task 4). 서버·창은 끄지 않는다.
set -u
cd ~/JKENGINE/engine
FAIL() { echo "APPS-PHONE-FAIL: $*"; exit 1; }
TMPD="${TMPDIR:-/data/data/com.termux/files/usr/tmp}"
NLOG="$TMPD/phone_apps_ninja.log"

# 24 launcher 컨테이너 — CMakeLists jkx_packages DEPENDS 목록과 1:1(Windows/WSL
# auto-repack 기준선 동일 집합 — docs/81 §2.2). browser는 WIN32 전용이라 여기 없다.
LAUNCHERS="minesweeper tetris testwin jango occ pcx vector iconedit recog vfont vpres terminal imguidemo taskmgr vplayer notify snap shot settings notes files agentmgr scriptdemo passworddemo"

echo "=== A. selftest (계약: AppSelfTest 0 failure(s) — 폰 축 캐논 첫 실측) ==="
timeout 300 ./buildterm/jkdesktop test >"$TMPD/ph_st.log" 2>&1
S_RC=$?
echo "selftest rc=$S_RC"
ST_PASS=$(grep -ac '^\[PASS\]' "$TMPD/ph_st.log")
ST_FAIL=$(grep -ac '^\[FAIL\]' "$TMPD/ph_st.log")
echo "PHONE-SELFTEST-PASS=$ST_PASS FAIL=$ST_FAIL (WSL 399·Windows 420 캐논 — 폰 첫 실측치)"
grep -a 'AppSelfTest' "$TMPD/ph_st.log" | tail -2
grep -aq 'AppSelfTest: 0 failure(s)' "$TMPD/ph_st.log" \
    || FAIL "AppSelfTest not 0 failure(s) — aarch64 계약 붕괴"
[ "$S_RC" -eq 0 ] || FAIL "selftest rc=$S_RC"
[ "$ST_FAIL" -eq 0 ] || FAIL "selftest FAIL=$ST_FAIL (폰축 회귀) — log: $TMPD/ph_st.log"

echo "=== B. auto-repack 도달 판정 (태스크의 진실 질문 — 증거 2원) ==="
# 증거 1원: ninja 로그의 Repacking 행(빌드 레일이 repack 스텝을 실행한 원문).
RL=0
if [ -f "$NLOG" ]; then
    RL=$(grep -ac 'Repacking apps/' "$NLOG")
    echo "AUTO-REPACK-NINJA-LOG-LINES=$RL"
    grep -a 'Repacking apps/' "$NLOG" | head -30
else
    echo "AUTO-REPACK-NINJA-LOG-LINES=0 (ninja log missing: $NLOG)"
fi
# 증거 2원: 빌드 레일 산출 실물 — 24 launcher .jkx 중 몇 개가 리빌드만으로 존재하나.
AUTO_N=0
AUTO_HAD=""
for a in $LAUNCHERS; do
    if [ -f "buildterm/apps/$a.jkx" ]; then AUTO_N=$((AUTO_N+1)); AUTO_HAD="$AUTO_HAD $a"; fi
done
echo "AUTO-REPACK-JKX-N=$AUTO_N/24"
echo "AUTO-REPACK-JKX-HAD:$AUTO_HAD"
# 증거 2원 합의(fix r1 I1): 둘 다 단정 분기에 합류 — RL(ninja 로그 Repacking
# 행)과 AUTO_N(.jkx 실재)가 합의(==)하지 않으면 hard FAIL.스테일 .jkx 잔존
# 구가 레일 죽음을 오-REACHED로 남기는 단일원 채점 차단(레일이 산 것만
# 카운트로 산다). 합의 실패는 폴백이 아니라 수동 확인 요구(증거 자체가 찢어짐).
echo "EVIDENCE-CONSENSUS: ninja-log-RL=$RL vs jkx-FILE-N=$AUTO_N"
if [ "$RL" -ne "$AUTO_N" ]; then
    FAIL "AUTO-REPACK 증거 2원 불합의: ninja Repacking 행 RL=$RL != .jkx 실재 AUTO_N=$AUTO_N (스테일 .jkx/레일 이변 오채점 차단 — fix r1 I1, 수동 확인 필요)"
fi
if [ "$AUTO_N" -eq 24 ]; then
    echo "AUTO-REPACK=REACHED"
elif [ "$AUTO_N" -gt 0 ]; then
    echo "AUTO-REPACK=PARTIAL (빠진 몫은 수동 pack 폴백 — 판정은 원장에 정직 기록)"
else
    echo "AUTO-REPACK=NOT-REACHED (수동 pack 폴백 — 판정은 원장에 정직 기록)"
fi

echo "=== C. jkx-pack 수동 pack 1회 실측 (posix dlopen leg) ==="
timeout 60 ./buildterm/jkdesktop jkx-pack settings >"$TMPD/ph_pack.out" 2>"$TMPD/ph_pack.err"
P_RC=$?
echo "manual pack rc=$P_RC"
echo "--- pack stdout: $(tr '\n' ' ' <"$TMPD/ph_pack.out")"
echo "--- pack stderr: $(tr '\n' ' ' <"$TMPD/ph_pack.err")"
[ "$P_RC" -eq 0 ] || FAIL "manual jkx-pack settings rc=$P_RC — $(cat "$TMPD/ph_pack.err" | tr '\n' ' ')"
grep -aq '^packed ' "$TMPD/ph_pack.out" || FAIL "manual pack no 'packed' line"
grep -aq 'apps/settings.jkx' "$TMPD/ph_pack.out" \
    || FAIL "pack outPath not apps/settings.jkx (posix forward-separator 계약 위반) — $(tr '\n' ' ' <"$TMPD/ph_pack.out")"
# TOC 원형 단정: 컨테이너가 MANI(원문)+MODL(jkapp_settings.dll — 플랫폼 무관 키
# 계약)을 산다. T1 WSL 패리티 원장과 같은 어설션 폼.
timeout 30 ./buildterm/jkdesktop jkx-list buildterm/apps/settings.jkx >"$TMPD/ph_toc.out" 2>&1
T_RC=$?
echo "jkx-list rc=$T_RC"
cat "$TMPD/ph_toc.out"
[ "$T_RC" -eq 0 ] || FAIL "jkx-list rc=$T_RC"
grep -aq 'MANI' "$TMPD/ph_toc.out" || FAIL "container TOC missing MANI entry"
grep -aq 'jkapp_settings.dll' "$TMPD/ph_toc.out" \
  || FAIL "container TOC missing platform-neutral module key jkapp_settings.dll"
# negative path — nosuchapp: dlopen leg의 정직한 실패(rc≠0 + cannot load 상세)
timeout 30 ./buildterm/jkdesktop jkx-pack nosuchapp >"$TMPD/ph_neg.out" 2>"$TMPD/ph_neg.err"
NEG_RC=$?
echo "NEGATIVE-PATH-RC=$NEG_RC"
echo "negative stderr: $(tail -1 "$TMPD/ph_neg.err")"
[ "$NEG_RC" -ne 0 ] || FAIL "jkx-pack nosuchapp unexpectedly rc=0 — fail-closed 붕괴"
grep -aq 'cannot load' "$TMPD/ph_neg.err" \
    || FAIL "jkx-pack nosuchapp stderr missing 'cannot load' — $(tr '\n' ' ' <"$TMPD/ph_neg.err")"
[ -f buildterm/apps/nosuchapp.jkx ] && FAIL "jkx-pack nosuchapp left nosuchapp.jkx behind"

echo "=== C2. 수동 pack 폴백 (auto-repack 빠진 몫만 — REACHED 시 0개) ==="
MISSING=""
for a in $LAUNCHERS; do
    [ -f "buildterm/apps/$a.jkx" ] || MISSING="$MISSING $a"
done
if [ -n "$MISSING" ]; then
    echo "MANUAL-PACK-FALLBACK-FOR:$MISSING"
    for a in $MISSING; do
        timeout 60 ./buildterm/jkdesktop jkx-pack "$a" >"$TMPD/ph_pack_$a.out" 2>&1 \
            || FAIL "manual pack $a failed — $(tr '\n' ' ' <"$TMPD/ph_pack_$a.out")"
        [ -f "buildterm/apps/$a.jkx" ] || FAIL "pack $a did not produce container"
    done
    echo "MANUAL-PACK-FALLBACK-OK"
else
    echo "MANUAL-PACK-FALLBACK-NONE (auto-repack이 전원 산출)"
fi
FIN_N=0
for a in $LAUNCHERS; do [ -f "buildterm/apps/$a.jkx" ] && FIN_N=$((FIN_N+1)); done
echo "LAUNCHER-JKX-FINAL-N=$FIN_N/24"
[ "$FIN_N" -eq 24 ] || FAIL "launcher containers incomplete after fallback: $FIN_N/24"

echo "=== D. lf/hx 폰 조달 판정 (핀 URL 직접 fetch — 조달기 헤더의 aarch64 재용점) ==="
# 조달기(/mnt/i 하드코드)와 동일 핀·동일 자산 패밀리의 폰 변분. 판정은 **실측**:
# glibc 빌드가 bionic(Termux)에서 사는지 — 사는 것만 apps-bin에 둔다(카탈로그
# 내장 게이트는 existence만이라 죽은 바이너리를 두면 죽은 내장 행이 카탈로그에
# 남는다 — 오염 금지). 실패는 FAIL이 아니라 영수증 행(원장 honest-fail).
mkdir -p buildterm/apps-bin/lf buildterm/apps-bin/helix
LF_KEPT=0
HX_KEPT=0
LFR="buildterm/apps-bin/lf/lf"
HXR="buildterm/apps-bin/helix/hx"
if [ -x "$LFR" ] && "$LFR" -version >/dev/null 2>&1; then
    LF_KEPT=1
    echo "LF-PHONE=ALREADY ($(timeout 10 "$LFR" -version 2>&1 | head -1))"
else
    LF_URL="https://github.com/gokcehan/lf/releases/download/r42/lf-android-arm64.tar.gz"
    if curl -fsSL -m 180 -o "$TMPD/lf_phone.tar.gz" "$LF_URL" 2>"$TMPD/lf_fetch.err"; then
        tar -xzf "$TMPD/lf_phone.tar.gz" -C buildterm/apps-bin/lf 2>"$TMPD/lf_x.err" \
            && chmod +x "$LFR" 2>/dev/null
        rm -f "$TMPD/lf_phone.tar.gz"
        if [ -x "$LFR" ] && timeout 10 "$LFR" -version >"$TMPD/lf_v.out" 2>&1; then
            LF_KEPT=1
            echo "LF-PHONE=RUNS ($(head -1 "$TMPD/lf_v.out"))"
        else
            echo "LF-PHONE=DEAD-OR-NOT-EXEC ($(timeout 10 "$LFR" -version 2>&1 | head -1; tr '\n' ' ' <"$TMPD/lf_x.err" 2>/dev/null))"
            rm -rf buildterm/apps-bin/lf
            mkdir -p buildterm/apps-bin/lf
        fi
    else
        echo "LF-PHONE=FETCH-FAILED (rc=$? — $(tail -1 "$TMPD/lf_fetch.err" 2>/dev/null))"
    fi
fi
if [ -x "$HXR" ] && "$HXR" --version >/dev/null 2>&1; then
    HX_KEPT=1
    echo "HX-PHONE=ALREADY ($(timeout 10 "$HXR" --version 2>&1 | head -1))"
else
    # 멱등+파편회피: 첫 실측에서 dead 판정 박힌 뒤엔 20MiB 재다운로드를 반복하지
    # 않는다 — 마커가 판정 원문을 보존하고 count 합산은 계속 hx=0(정직 무영향).
    HX_DEADMARK="buildterm/apps-bin/helix/.dead-on-phone"
    if [ -f "$HX_DEADMARK" ]; then
        echo "HX-PHONE=SKIPPED-DEAD-MARKER ($(head -1 "$HX_DEADMARK")) — 첫 실측 판정 유지"
    else
    HX_URL="https://github.com/helix-editor/helix/releases/download/25.07.1/helix-25.07.1-aarch64-linux.tar.xz"
    rm -rf "$TMPD/hx_phone_ext"
    mkdir -p "$TMPD/hx_phone_ext"
    if curl -fsSL -m 600 -o "$TMPD/hx_phone.tar.xz" "$HX_URL" 2>"$TMPD/hx_fetch.err"; then
        tar -xJf "$TMPD/hx_phone.tar.xz" -C "$TMPD/hx_phone_ext" 2>"$TMPD/hx_x.err" \
            && cp "$TMPD/hx_phone_ext/helix-25.07.1-aarch64-linux/hx" "$HXR" 2>/dev/null \
            && cp -r "$TMPD/hx_phone_ext/helix-25.07.1-aarch64-linux/runtime" buildterm/apps-bin/helix/ 2>/dev/null \
            && chmod +x "$HXR" 2>/dev/null
        rm -rf "$TMPD/hx_phone_ext" "$TMPD/hx_phone.tar.xz"
        if [ -x "$HXR" ] && timeout 10 "$HXR" --version >"$TMPD/hx_v.out" 2>&1; then
            HX_KEPT=1
            echo "HX-PHONE=RUNS ($(head -1 "$TMPD/hx_v.out"))"
        else
            echo "HX-PHONE=DEAD-OR-NOT-EXEC ($(timeout 10 "$HXR" --version 2>&1 | head -1; tr '\n' ' ' <"$TMPD/hx_x.err" 2>/dev/null; tr '\n' ' ' <"$TMPD/hx_fetch.err" 2>/dev/null)) — glibc/bionic 불일치 후보"
            rm -rf buildterm/apps-bin/helix
            mkdir -p buildterm/apps-bin/helix
            echo "glibc aarch64 build refused exec on Termux bionic (hx --version 실행불능 — 첫 실측 원문)" >"$HX_DEADMARK"
        fi
    else
        echo "HX-PHONE=FETCH-FAILED (rc=$? — $(tail -1 "$TMPD/hx_fetch.err" 2>/dev/null))"
    fi
    fi
fi
echo "PROVISION-KEPT: lf=$LF_KEPT hx=$HX_KEPT"

echo "=== E. library-list (serverless catalog CLI — 조성 자기합산 어설션) ==="
timeout 60 ./buildterm/jkdesktop library-list >"$TMPD/ph_list.out" 2>"$TMPD/ph_list.err"
L_RC=$?
echo "library-list rc=$L_RC"
cat "$TMPD/ph_list.out"
[ "$L_RC" -eq 0 ] || FAIL "library-list rc=$L_RC"
COUNTLINE=$(grep -aE '^count=[0-9]+ base=' "$TMPD/ph_list.out" | head -1)
[ -n "$COUNTLINE" ] || FAIL "library-list no well-formed count= tail line"
NAMED=$(grep -ac '^name=' "$TMPD/ph_list.out")
COUNT=$(printf '%s' "$COUNTLINE" | sed -n 's/^count=\([0-9]*\) .*$/\1/p')
[ "$NAMED" -eq "$COUNT" ] || FAIL "library-list line/count mismatch: name= lines=$NAMED count=$COUNT"
# 조성 합산: launcher .jkx 24 + phoneprobe(라인 시작 유산 .jkx) + 콘솔 트윈 1 +
# chat builtin 1 + 조달 성공분. 폰 baseline(관측 4) 대비 상승 원장의 재료.
EXPECTED=$((24 + 1 + 1 + 1 + LF_KEPT + HX_KEPT))
echo "PHONE-LIBRARY-COUNT=$COUNT (expected=$EXPECTED — 24 launcher + phoneprobe + twin + chat + 조달 $LF_KEPT/$HX_KEPT)"
[ "$COUNT" -eq "$EXPECTED" ] || FAIL "library-list count=$COUNT expected=$EXPECTED (조성 이탈 — 원장 진술 갱신 필요)"
# 행 단정: 유산 .jkx + launcher 표본(.jkx가 내장을 흡수하는 것도 함께)
PP=$(grep -a '^name=phoneprobe ' "$TMPD/ph_list.out" | head -1)
[ -n "$PP" ] || FAIL "library-list missing phoneprobe entry (라인 시작 유산 소실)"
printf '%s\n' "$PP" | grep -aq 'source=jkx' || FAIL "phoneprobe entry not source=jkx — $PP"
# launcher 표본 7 — .jkx가 내장(minesweeper·tetris)을 흡수하는 것도 같은 어설션으로
for APP in settings notes files terminal taskmgr minesweeper tetris; do
    ROW=$(grep -a "^name=$APP " "$TMPD/ph_list.out" | head -1)
    [ -n "$ROW" ] || FAIL "library-list missing row name=$APP"
    printf '%s\n' "$ROW" | grep -aq 'source=jkx' || FAIL "row name=$APP not source=jkx — $ROW"
done
# 콘솔 트윈(Task 3 확장 계약 — cmd_posix 개발 키) + chat builtin
TWIN_ROW=$(grep -aF 'name=terminal:apps/sampletodo/sampletodo.sh' "$TMPD/ph_list.out" | head -1)
[ -n "$TWIN_ROW" ] || FAIL "library-list has no console twin row (cmd_posix 승격 실패) — output: $(tr '\n' ' ' <"$TMPD/ph_list.out")"
printf '%s' "$TWIN_ROW" | grep -aq ' source=console caps= size=0 ' \
    || FAIL "console twin row is not source=console — row: $TWIN_ROW"
CHAT_ROW=$(grep -a '^name=chat ' "$TMPD/ph_list.out" | head -1)
[ -n "$CHAT_ROW" ] || FAIL "library-list missing chat builtin row"
printf '%s\n' "$CHAT_ROW" | grep -aq 'source=builtin' || FAIL "chat row not source=builtin — $CHAT_ROW"
# 조달 행 — 존재 판정과 1:1(죽은 바이너리는 절두산해 존재 행도 없어야 한다)
if [ "$LF_KEPT" -eq 1 ]; then
    LROW=$(grep -aF 'name=terminal:apps-bin/lf/lf title=lf source=builtin' "$TMPD/ph_list.out" | head -1)
    [ -n "$LROW" ] || FAIL "lf provisioned but no builtin row (posix 무접미 게이트 미충족)"
    echo "row: $LROW"
else
    grep -aqF 'name=terminal:apps-bin/lf/lf' "$TMPD/ph_list.out" \
        && FAIL "lf NOT provisioned but dead row listed — apps-bin 오염(존재 게이트가 -x 를 못 보는 정직 노트)"
fi
if [ "$HX_KEPT" -eq 1 ]; then
    HROW=$(grep -aF 'name=terminal:apps-bin/helix/hx title=hx source=builtin' "$TMPD/ph_list.out" | head -1)
    [ -n "$HROW" ] || FAIL "hx provisioned but no builtin row (posix 무접미 게이트 미충족)"
    echo "row: $HROW"
else
    grep -aqF 'name=terminal:apps-bin/helix/hx' "$TMPD/ph_list.out" \
        && FAIL "hx NOT provisioned but dead row listed — apps-bin 오염(존재 게이트가 -x 를 못 보는 정직 노트)"
fi

ASSERT_FIELD() { printf '%s' "$1" | grep -aq "$2" || FAIL "$3 — reply: $1"; }

echo "=== F. boot server (bash ~/tx4_boot.sh — DISPLAY=:1 표준) ==="
bash ~/tx4_boot.sh
sleep 6
P=$(pgrep -f 'buildterm/jkdesktop' | tr '\n' ' ')
if [ -z "$P" ]; then
    echo "--- srvx.log tail:"; tail -20 ~/srvx.log
    FAIL "no jkdesktop process 6s after tx4_boot (server died)"
fi
echo "server pids: $P"

echo "=== G. agentctl ping ==="
PING=$(timeout 12 ./buildterm/jkdesktop agentctl '{"tool":"ping","args":{}}' 2>/dev/null | grep -a '{' | head -1)
echo "ping reply: $PING"
[ -n "$PING" ] || FAIL "agentctl ping got no reply JSON (server unresponsive)"
ASSERT_FIELD "$PING" '"ok":true' "ping did not ok"

echo "=== H. launch_app jkx 2종 (T1 posix pack 산출 — 이전 폰 미제공) ==="
L1=$(timeout 15 ./buildterm/jkdesktop agentctl '{"tool":"launch_app","args":{"app":"settings"}}' 2>/dev/null | grep -a '{' | head -1)
echo "launch settings: $L1"
[ -n "$L1" ] || FAIL "launch_app settings got no reply (server gone?)"
ASSERT_FIELD "$L1" '"ok":true' "launch_app settings did not ok"
L2=$(timeout 15 ./buildterm/jkdesktop agentctl '{"tool":"launch_app","args":{"app":"notes"}}' 2>/dev/null | grep -a '{' | head -1)
echo "launch notes: $L2"
[ -n "$L2" ] || FAIL "launch_app notes got no reply (server gone?)"
ASSERT_FIELD "$L2" '"ok":true' "launch_app notes did not ok"

echo "=== I. list_windows (영수증: Settings/Notes 창 오브젝트 — meta 기하) ==="
sleep 18  # 폰은 클라 스폰(.so 16MB 로드+폰트 아틀라스)이 느리다 — phone_library 원장
WIN=$(timeout 12 ./buildterm/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)
echo "list_windows: $WIN"
[ -n "$WIN" ] || FAIL "list_windows got no reply"
S1=$(printf '%s' "$WIN" | grep -aoE '\{"id":[^}]*"title":"Settings"[^}]*\}' | head -1)
[ -n "$S1" ] || FAIL "list_windows has no Settings window object (jkx launch failed on aarch64?) — reply: $WIN"
printf '%s' "$S1" | grep -aqE '"id":[0-9]+' || FAIL "Settings window object missing id — $S1"
printf '%s' "$S1" | grep -aq '"w":900,"h":620' || FAIL "Settings window geometry not 900x620 — $S1"
S2=$(printf '%s' "$WIN" | grep -aoE '\{"id":[^}]*"title":"Notes"[^}]*\}' | head -1)
[ -n "$S2" ] || FAIL "list_windows has no Notes window object — reply: $WIN"
printf '%s' "$S2" | grep -aqE '"id":[0-9]+' || FAIL "Notes window object missing id — $S2"
printf '%s' "$S2" | grep -aq '"w":760,"h":520' || FAIL "Notes window geometry not 760x520 — $S2"

echo "=== J. 서버·창을 켜 둔 채 종료 (사용자 육안 게이트 대기) ==="
# teardown은 probe 임시 파일만 — 서버는 끄지 않는다. 자기 삭제는 bash가 fd로
# 읽은 뒤라 안전(스크래치 정책 — 재실행시 드라이버가 재생성).
rm -f ~/JKENGINE/phone_apps_remote.sh 2>/dev/null
echo "APPS-PHONE-OK"
exit 0
PHONEEOF

for r in 1 2 3; do
  # STAGE_ARGS 비어 있으면 무소음(정상 워킹 배포), 채워지면 타르 마지막 항목
  # 으로 main.cpp HEAD blob이 들어가 추출(후행 항목 승리) 시 스테이징본이 채택.
  if tar -cf "$TARBALL" -C "$ROOT" "${FILES[@]}" -C "$SCRATCH" "$RSRC_TAR_NAME" "${STAGE_ARGS[@]+"${STAGE_ARGS[@]}"}"; then
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
$SSH 'test -f ~/JKENGINE/engine/src/main.cpp && test -f ~/JKENGINE/engine/CMakeLists.txt \
      && test -f ~/JKENGINE/engine/src/JKLibraryCatalog.cpp \
      && test -f ~/JKENGINE/engine/apps/sampletodo/sampletodo.sh \
      && test -f ~/JKENGINE/engine/apps/sampletodo/sampletodo.cmd \
      && test -f ~/JKENGINE/engine/apps/sampletodo/manifest.json \
      && test -f ~/JKENGINE/engine/scripts/install_lf_helix_posix.sh \
      && test -f ~/JKENGINE/engine/src/apps/ClientTaskmgrApp.cpp \
      && test -f ~/JKENGINE/engine/src/client/JKClientSurface.cpp \
      && test -f ~/JKENGINE/engine/tools/probes/phone_apps.sh \
      && test -f ~/JKENGINE/phone_apps_remote.sh \
      && echo DEPLOY-FILES-OK' \
  || FAIL "deployed files missing on phone"

echo "=== 2. 폰 리빌드 (ninja -C buildterm -j4, aarch64 — auto-repack이 같은 레일에 실린다) ==="
# rc 봉합(phone_library 선례): 원격 복합문이 echo로 끝나면 종료코드가 항상 0 —
# PIPESTATUS를 rc로 삼아 exit로 전파해야 실패가 드라이버에 도달한다.
# ninja 로그 전문은 $TMPDIR에 남겨 나중에 auto-repack(Repacking 행) 증거로 쓴다.
N_RC=0
NOUT=$($SSH 'cd ~/JKENGINE/engine && ninja -C buildterm -j4 >"$TMPDIR/phone_apps_ninja.log" 2>&1; rc=$?; tail -12 "$TMPDIR/phone_apps_ninja.log"; echo NINJA-RC=$rc; exit $rc') || N_RC=$?
echo "$NOUT"
[ "$N_RC" -eq 0 ] || FAIL "ninja rebuild rc=$N_RC (aarch64 compile failure — NINJA tail above)"
# 존재 게이트(T7 함정 — 스테일 .so 잔존 시 거짓통과): 스테이징 산출까지 본다.
$SSH 'cd ~/JKENGINE/engine; test -x buildterm/jkdesktop \
      && test -f buildterm/apps/sampletodo/sampletodo.sh \
      && echo BUILD-TREE-RECEIPTS-OK || { ls buildterm/jkdesktop 2>&1; exit 1; }' \
  || FAIL "buildtree receipts missing after rebuild (console staging did not run?)"

echo "=== 3. 폰 영수증 절차 실행 (auto-repack 판정 → 수동 pack → 조달 → count → launch) ==="
$SSH "bash $RSRC_PHONE" > "$RLOG" 2>&1
R_RC=$?
cat "$RLOG"
[ "$R_RC" -eq 0 ] || FAIL "phone-side receipt script rc=$R_RC (see above)"
grep -aq '^APPS-PHONE-OK$' "$RLOG" || FAIL "receipt script did not emit APPS-PHONE-OK"
if grep -aq 'APPS-PHONE-FAIL' "$RLOG"; then FAIL "receipt script emitted APPS-PHONE-FAIL"; fi
# auto-repack 판정 전파(진실 질문의 원장 행 — 드라이버가 다시 인쇄해 산다)
[ "$(grep -ac '^AUTO-REPACK=' "$RLOG")" -ge 1 ] || FAIL "no AUTO-REPACK= verdict in phone log"
grep -a '^AUTO-REPACK=' "$RLOG"

echo ""
echo "APPS-PHONE-OK"
echo "서버는 살아 있고 Settings·Notes 창이 떠 있다 — 사용자 눈확인 대기. 서버·jkweb을 끄지 마세요."
exit 0
# EOF — 끝 개행 유지(스크립트 산술 무해 습관).
