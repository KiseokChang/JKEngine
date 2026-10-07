#!/usr/bin/env bash
# engine/scripts/install_lf_helix_posix.sh — lf/hx posix 조달기
# (앱 커버리지 라인 Task 3 — 설치 트윈). Windows 축 쌍대 = 같은 폴더의
# install_lf_helix.ps1(docs/44; "바이너리는 git에 넣지 않는다" 정책 계승).
# 하는 일: buildwsl/apps-bin/{lf/lf, helix/hx}에 linux 바이너리를 확보한다.
# 조달 경로: GitHub release 바이너리 다운로드 (원장 명시 — 네트워크 사용).
#   - 버전은 Windows 축 설치물과 동일 핀(docs/44_phase_a_tui.md:11 실측 원문
#     "lf r42, helix 25.07.1" — 이 스크립트 작성일 Windows 측 재실측 동일:
#     lf -version=r42, hx --version=25.07.1 a05c151b). 크로스축 패리티 우선,
#     latest 추격은 의도적으로 안 한다.
#   - aarch64 자산도 동일 릴리스에 실려 있다(lf-android-arm64.tar.gz /
#     helix-*-aarch64-linux.tar.xz) — 폰 축(Task 4) 재용 포인트.
# 바이너리는 커밋하지 않는다(실측 판정: engine/build/는 .gitignore 'Build/'
# + '*.exe'로 무시, engine/buildwsl/는 무시 아닌 무추적 — 그러나 서드파티
# fat 바이너리 2개(~25 MiB) 블롭 커밋은 레포 비대 — docs/44의 "기계만 커밋"
# 정책을 따른다. 본 스크립트가 그 기계).
# 멱등: 바이너리가 이미 있으면 네트워크 없이 버전 원문만 인쇄하고 끝낸다.
#
# 실행(Git Bash에서):
#   MSYS_NO_PATHCONV=1 wsl.exe -d Ubuntu-24.04 -- bash /mnt/i/progwork/JKENGINE/engine/scripts/install_lf_helix_posix.sh
set -u
cd /mnt/i/progwork/JKENGINE/engine

FAIL() { echo "APPS-BIN-SETUP-FAIL: $*"; exit 1; }

LF_VER=r42
HX_VER=25.07.1
LF_URL="https://github.com/gokcehan/lf/releases/download/${LF_VER}/lf-linux-amd64.tar.gz"
HX_URL="https://github.com/helix-editor/helix/releases/download/${HX_VER}/helix-${HX_VER}-x86_64-linux.tar.xz"

echo "=== install_lf_helix_posix (Task 3 — 설치 트윈 조달기) ==="
if [ -x buildwsl/apps-bin/lf/lf ] && [ -x buildwsl/apps-bin/helix/hx ]; then
    echo "ALREADY-PRESENT (no network use)"
else
    mkdir -p buildwsl/apps-bin/lf buildwsl/apps-bin/helix
fi

# --- lf (Go 단일 바이너리, tar.gz에 실행 비트 내장 — extract만으로 충분)
if [ ! -x buildwsl/apps-bin/lf/lf ]; then
    timeout 180 curl -fsSL -o /tmp/lf_setup.tar.gz "$LF_URL" \
        || FAIL "lf download failed (rc=$?, url=$LF_URL)"
    tar -xzf /tmp/lf_setup.tar.gz -C buildwsl/apps-bin/lf \
        || FAIL "lf extract failed"
    rm -f /tmp/lf_setup.tar.gz
fi

# --- hx (Rust; runtime/ 트리 필수 — exe 옆 배치가 helix의 탐색 규약)
if [ ! -x buildwsl/apps-bin/helix/hx ]; then
    timeout 600 curl -fsSL -o /tmp/hx_setup.tar.xz "$HX_URL" \
        || FAIL "hx download failed (rc=$?, url=$HX_URL)"
    rm -rf /tmp/hx_setup_extract
    mkdir -p /tmp/hx_setup_extract
    tar -xJf /tmp/hx_setup.tar.xz -C /tmp/hx_setup_extract \
        || FAIL "hx extract failed"
    SRC="/tmp/hx_setup_extract/helix-${HX_VER}-x86_64-linux"
    [ -f "$SRC/hx" ] || FAIL "hx binary missing in release tree: $SRC"
    [ -d "$SRC/runtime" ] || FAIL "hx runtime/ missing in release tree: $SRC"
    cp "$SRC/hx" buildwsl/apps-bin/helix/hx
    cp -r "$SRC/runtime" buildwsl/apps-bin/helix/runtime
    rm -rf /tmp/hx_setup_extract /tmp/hx_setup.tar.xz
fi

chmod +x buildwsl/apps-bin/lf/lf buildwsl/apps-bin/helix/hx \
    || FAIL "chmod +x failed"

# --- 영수증: 버전 원문 + 존재 게이트가 볼 경로 재확인
LF_V=$(buildwsl/apps-bin/lf/lf -version 2>&1 | head -1)
HX_V=$(buildwsl/apps-bin/helix/hx --version 2>&1 | head -1)
echo "lf-version=$LF_V (pinned $LF_VER)"
echo "hx-version=$HX_V (pinned $HX_VER)"
[ -x buildwsl/apps-bin/lf/lf ] || FAIL "buildwsl/apps-bin/lf/lf still not executable"
[ -x buildwsl/apps-bin/helix/hx ] || FAIL "buildwsl/apps-bin/helix/hx still not executable"
# hx 실구동 최소 검증 — runtime/ 탐색이 exe 옆 규약으로 사는지(--health는 UI
# 진입 없이 진단 전문을 인쇄하고 끝난다; "Config file"로 시작하는 진단 블록이
# 나오면 바이너리+runtime이 산 것 — 실측 첫 행).
HX_HEALTH=$(buildwsl/apps-bin/helix/hx --health 2>&1 | head -1)
echo "hx-health-first-line=$HX_HEALTH"
case "$HX_HEALTH" in
    Config*|"hx "*|"helix "*) : ;;
    *) FAIL "hx --health produced no diagnostic block (runtime/ or binary broken): '$HX_HEALTH'" ;;
esac
echo "APPS-BIN-SETUP-OK"
exit 0
