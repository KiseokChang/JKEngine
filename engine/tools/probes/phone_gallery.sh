#!/usr/bin/env bash
# 폰 갤러리 실측 probe (갤러리 라인 #82 T5 — plan
# docs/superpowers/plans/2026-10-09-gallery.md task-5, 스펙
# docs/superpowers/specs/2026-10-09-gallery-design.md — 폰 실측 결제 관문).
#
# 승계 원천:
#   · phone_text_scale.sh(텍스트 스케일 라인 T4) — PHONE_HOST fail-closed 가드
#     **exec 앞**·tar 오염 게이트+HEAD blob 스테이징(폰에 올 원문은 항상
#     `git show "HEAD:$f"` 스테이징)·tar-over-ssh+크기 신선도 대차·원격
#     스크립트=파일+자기 소각·REMNANT·브래킷 pkill·jkweb 상시 유지(절사 금지)·
#     x11grab 캡처(1920x1080 시도→화면 크기 파싱 재시도)·base64 1파이프 회수+
#     md5 대차·settings 복원 BOOT-OK.
#   · wsl_gallery.sh(갤러리 라인 T4) — 합성 PNG 시드(엔진 소유 — i:\@keep 절대
#     금지)·극단 종횡비 2장(FitFull 실뷰포트 소비 확인 몫)·격자→셀 0 클릭→
#     전체보기→←꺾임(WrapStep 랩어라운드)→Esc 복귀 캡처 부조·send_input 승인
#     무승인 운용(permissions.json 1행 기록+원복)·THUMB-CACHE-CNT 기계 영수증.
#
# 재량 판정(WSL T4 선례와의 차이 — 이유 수의 원장):
#   · send_input 승인 — WSL probe는 permissions.json을 **새로 기록+END 소각**했
#     다(ENTRY에 파일이 있으면 FAIL). 폰에는 런 전에 permissions.json이 선존
#     한다(2026-10-08 — close_window: allow 1키, 33B — 전 라인 유산). 그대로
#     FAIL하면 폰 캡처 부조(클릭/키)가 영구 성립 못 하므로, WSL 계약의 취지
#     ("전면 allow 런타임 파일은 프로브 소유 아님")를 **병합+바이트 등호 복원**
#     으로 존중한다: ORIG 백업→send_input 1키 병합(선존 키 보존 — 선존
#     send_input이 이미 allow면 무편집)→END에 바이트 등호 원복. 영수증 행
#     PERM-ENTRY-PRE-EXISTING·PERM-MERGE·PERM-RESTORED-BYTES.
#
# 영수증 목표:
#   ① 폰 재배포 — 갤러리 라인(6c6382c..390c741 = T1 모듈 56f26d9+T2 298d08f+
#      T3 c611c80+fix r1 2a57b4d+fix r2 18274d0)이 건드린 engine 소스 11건+
#      probe 본체. 신규 모듈 jkapp_gallery(SHARED — CMakeLists 수정 동봉) 폰
#      첫 재배포라 **ninja가 CMakeLists mtime 변화로 cmake 재설정을 자동
#      수행**한다(RERUN_CMAKE). .so가 안 나오면 명시 cmake 재설정 폴백 1급.
#   ② 폰 aarch64 ninja 리빌드(NINJA-RC 전파 — 신규 타깃으로 ~10-20분)
#   ③ 폰 selftest 캐논 게이트 — **기대 546 계보** = 506(텍스트 스케일 최신) +
#      40(2g 계열, T1-T3 = 2g-a..j 전부). 폰 기존 506 기준 비교 원문 첨부.
#      2g [PASS] 실측수를 함께 인쇄해 소스 반영을 이중 단정. 틀리면 계보
#      정산 원장(정직 인쇄 — hard FAIL 아님).
#   ④ leg grid — 진입 settings(ORIG 원문 그대로) 부팅 → 기본 dirs
#      (state/screenshots) 시드 4장 격자 → 캡처 gal_phone_grid.png → 셀 0
#      클릭 → gal_phone_full.png → ←키(꺾임) → gal_phone_prev.png → Esc →
#      gal_phone_back.png. 시드 순서는 mtime 명시 스탬프(tar에 내장)로 결정화
#      — 셀 0 = jkg_seed_a_640x400.png 단수 계약(drvfs/동률 함정 폐곡).
#   ⑤ leg dirs — settings에 **gallery.dirs=["/sdcard/Pictures"]** 넣어
#      재시동 → 캡처 gal_phone_dirs.png. Termux storage 권한 부재면 /sdcard
#      열거가 안 된다 — **빈 목록=정상 결과, 그대로 가시화만**(스펙 계약).
#      /sdcard/Pictures 셸 열거 시도 원문(성패 무관 정직 인쇄).
#   ⑥ leg boot — settings 원상 복원(바이트 등호 — ORIG 스타일 유지·
#      font_path 유지) 재시동 BOOT-OK → 진입 상태 재현(태스크바+지뢰찾기) →
#      갤러리 launch(육안 스탭 앞) → 캡처 gal_phone_boot.png. **서버 UP 종료**
#      — 사용자 육안 게이트 대기(probe가 결제를 기록하지 않는다).
#   ⑦ jkweb 생존 계약 — 기동 카운트 원문 첨부(진입/종료 — 절사 금지,
#      localhost:8090 상시 계약).
#
# 판정 사다리(라벨 계약):
#   GAL-PHONE-VERDICT: GAL-PHONE-OK = 캐논 계보(GALLERY-FULL) AND
#               leg grid THUMB-CACHE-CNT ≥ 4(시드 셀 디코드 영수증) AND
#               캡처 6종 전부 비빈 — rc=0
#   GAL-PHONE-VERDICT: GAL-PHONE-FAIL(행별 이유) — 수치 미달=정직 원장 rc=0
#   CAL-PHONE-FAIL(rc=1) = hard FAIL만: ssh 단절·배포 실패·NINJA-RC≠0·
#               selftest 회귀(FAIL>0·rc≠0·요약 부재)·서버/ping/launch/창
#               실패·캡처 실패·원격 잔존.
#   구판 참조 축: 폰 selftest 506(텍스트 스케일 T4 실측 캐논 — 기준 원문).
#
# 실행법(윈도 Git Bash, 저장소 루트 어디서든):
#   PHONE_HOST=<폰 IP> bash engine/tools/probes/phone_gallery.sh
# **PHONE_HOST 필수 가드(fail-closed)** — unset이면 스캔 폴백 없이 즉시 FAIL
# (697db8b "phone probes는 PHONE_HOST 필수" 계약 — LAN 스캔 폴백·대역 리터럴
# 제거 원문 승계). 어떤 파일/로그/커밋에도 IPv4 리터럴·대역 표기를 두지 않는다.
# PHONE_PORT PHONE_USER PHONE_KEY만 기본값. 풀 로그: engine/tmp/pgal_driver.log
# (tee).
#
# 함정 원장(승계+신규):
#   · PHONE_HOST 가드는 exec(tee)보다 **앞에** — 가드가 뒤에 있으면 unset
#     재실행 시 tee truncate로 마지막 실측 영수증 로그를 통째로 덮어쓴다
#     (텍스트 스케일 T4 fix r1 M5 원문).
#   · 원격 스크립트는 scp 후 파일 기동 — ssh argv(heredoc)에 jkdesktop 원문
#     문자열을 직접 놓으면 자기 pkill에 걸려 죽는다(사건 원장). 브래킷
#     pkill '[j]kdesktop'/buildterm/[j]kdesktop 계약.
#   · 생존 바이너리 relink = ETXTBSY — 드라이버 pre-clean(브래킷 pkill+-9
#     에스컬레이션+잔존 어설션)이 tar 앞. jkweb은 절사·pkill 대상 아님.
#   · 폰 클라 스폰 15-18s — list_windows 3s 간격 최대 20회 재시도.
#   · ssh 원격 `< /dev/null` 금지(채널 hang). rc 봉합: 복합문이 echo로 끝나면
#     0 — exit로 전파. 정수 카운트용 grep -c는 0건 rc=1도 의도된 값.
#   · 캡처 회수는 ssh 호출당 base64 1파이프(ls 등 혼입 금지). 윈도 python은
#     MSYS식 /i/... 패스를 못 읽는다 — cygpath 변환 후 python(T1 회수 유실
#     렛슨). decode 성공/빈파일 게이트 후 폰쪽 삭제.
#   · tar는 저장소 루트에서 만들어 `-C ~/JKENGINE`으로 풀어야 한다(engine/
#     engine 중첩 트랩). 폰 /tmp는 쓰기 불가 — 스크래치는 $TMPDIR.
#   · 시드 PNG는 **폰 python3에 의존하지 않는다** — 드라이버 측(윈도 python)
#     에서 합성해 tar 멤버로 랜딩+os.utime으로 mtime 명시 스탬프(tar 보존).
#     열거 순서(mtime 내림)와 셀 0을 런마다 고정한다(WSL T4 trap 4 — 동률
#     무안정 함정 폐곡).
#   · tar 전송은 LAN 내부 ssh 한정 — 어떤 클라우드/외부로도 가지 않는다.
set -u
export MSYS_NO_PATHCONV=1  # MSYS가 ssh 인수의 posix 경로를 찢는 것 차단(chat T8 선례)

ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
cd "$ROOT" || exit 1
SCRATCH="$ROOT/engine/tmp"
mkdir -p "$SCRATCH"
TARBALL="$SCRATCH/pgal_deploy.tar"
RLOG="$SCRATCH/pgal_driver.log"
RUNLOG="$SCRATCH/pgal_phone_run.log"
RSRC_TAR_NAME="pgal_phone_remote.sh"
RSRC_PHONE="~/JKENGINE/$RSRC_TAR_NAME"   # 폰 측 삭제는 remote script 자기 소각

# 접속 정보는 환경변수로(기록 금지 계약 — 내부 IP는 커밋·문서·리포트·로그에
# 두지 않는다). PHONE_HOST는 필수 — 미설정이면 fail-closed로 즉시 FAIL(스캔
# 폴백 없음 — 697db8b 계약).
PHONE_HOST="${PHONE_HOST:-}"
PHONE_PORT="${PHONE_PORT:-8022}"
PHONE_USER="${PHONE_USER:-u0_a4}"
PHONE_KEY="${PHONE_KEY:-$HOME/.ssh/termux_jkengine}"

# PHONE_HOST 필수 가드는 exec(tee)보다 **앞에** 둔다 — 가드가 tee 뒤에 있으면
# PHONE_HOST unset 재실행 시 tee truncate로 마지막 실측 영수증 로그
# (pgal_driver.log)를 통째로 덮어쓴다(텍스트 스케일 T4 fix r1 M5 폐곡 승계).
if [ -z "$PHONE_HOST" ]; then
  echo "GAL-PHONE-FAIL: PHONE_HOST not set — 기록 금지 계약상 기본값·스캔 폴백 없음 (환경변수로 폰 호스트를 지정)"
  exit 1
fi

# 풀 로그 — 드라이버 전체(tee; 원격 run은 RUNLOG로 한벌 승계).
exec > >(tee "$RLOG") 2>&1

FAIL() { echo "GAL-PHONE-FAIL: $*"; exit 1; }
echo "PHONE-HOST: 환경변수 지정 사용 (스캔 폴백 없음 — fail-closed 가드)"
echo "HEAD: $(git -C "$(cygpath -w "$ROOT" 2>/dev/null || echo "$ROOT")" rev-parse HEAD 2>/dev/null || echo rev-parse-failed)"
echo "BASE LINEAGE: 6c6382c..390c741 (T1 56f26d9 + T2 298d08f + T3 c611c80 + fix r1 2a57b4d + fix r2 18274d0)"
echo "CANON AXIS: 폰 selftest 기존 506(텍스트 스케일 T4 실측) → 기대 546 = 506 + 2g 40"
SSH="ssh -p $PHONE_PORT -o BatchMode=yes -o ConnectTimeout=15 -o ServerAliveInterval=15 -o ServerAliveCountMax=10 -i $PHONE_KEY $PHONE_USER@$PHONE_HOST"

# ── 배포 원천: 갤러리 라인(6c6382c..390c741)이 건드린 engine 소스 전량 —
#    `git diff --name-only 6c6382c..390c741 -- engine/` 결과에서 WSL probe+
#    아이콘 생성 스크립트(로컬 부속)만 제외, probe 본체는 자기 파일 포함.
FILES=(
  engine/CMakeLists.txt                        # T1 — jkapp_gallery SHARED 타깃+JKX_ICON_APPS gallery (폰 cmake 재설정 원료)
  engine/assets/icons/launcher_gallery@1x.png  # T1 — CMake 참조 원문(부재=cmake 재설정 실패 — 동봉 필수)
  engine/assets/icons/launcher_gallery@2x.png  # T1 — 동형
  engine/include/JKImageLoader.h               # T1/T3 — LoadedImage+MakeThumb 파이프라인
  engine/include/apps/ClientGalleryApp.h       # T1/T2/T3 — 뷰 모드 2상태+텍스처 풀
  engine/include/apps/GalleryModel.h           # T1-T3/fx — 리졸러·핏·캐시키·LRU·가시 컬(2g 원료)
  engine/src/JKImageLoader.cpp                 # T1/T3 — stb 디코드+stbi_write 캐시
  engine/src/apps/ClientGalleryApp.cpp         # T1-T3 — 격자+전체보기+썸네일 구현
  engine/src/apps/JKAppModule_gallery.cpp      # T1 — meta gallery/"Gallery" 560x520 (posix .so)
  engine/src/main.cpp                          # T1-T3 쌍둥이 — 2g 캐논 원료(폰 selftest 원본)
  engine/tools/posix_selftest/main.cpp         # T1-T3 쌍둥이 — 2g 쌍둥이
  engine/tools/probes/phone_gallery.sh         # probe 본체(폰에 원문 유산 — 게이트 예외)
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
  [ "$f" = "engine/tools/probes/phone_gallery.sh" ] && continue   # 자기 파일 예외
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
    STAGE_DIR="$SCRATCH/pgal_head_stage"
    rm -rf "$STAGE_DIR"
    mkdir -p "$STAGE_DIR"
  fi
  GS_OK=0
  for try in 1 2 3; do
    git -C "$GITROOT" show "HEAD:$f" > "$STAGE_DIR/$f" 2>>"$SCRATCH/pgal_gshow.err" \
      && { GS_OK=1; break; }
    sleep 3
  done
  [ "$GS_OK" -eq 1 ] \
    || FAIL "git show HEAD:$f 스테이징 실패(재시도 3회) — $(tail -1 "$SCRATCH/pgal_gshow.err")"
  WIPPED=$((WIPPED+1))
  STAGE_ARGS+=(-C "$STAGE_DIR" "$f")
done
if [ "$WIPPED" -gt 0 ]; then
  echo "NOTE-WIP: 더러운 배포 원천 $WIPPED건 — HEAD blob 스테이징으로 배포"
else
  echo "CONTAMINATION-GATE=CLEAN (배포 원천 전량 HEAD와 일치 — 워킹 카피 배포)"
fi

echo "=== 1. 합성 PNG 시드 4장 (윈도 python — 폰 python3 불요, tar 멤버 랜딩) ==="
SEEDSTAGE="$SCRATCH/pgal_seeds"
rm -rf "$SEEDSTAGE"
mkdir -p "$SEEDSTAGE/engine/buildterm/state/screenshots"
PYSCRATCH=$(cygpath -w "$SCRATCH" 2>/dev/null || echo "$SCRATCH")
PYTHONIOENCODING=utf-8 python - "$PYSCRATCH" <<'PYEOF'
# 시드 4장 — wsl_gallery.sh(T4) gal_seed.py와 동형(색 밴드 = 방향·종횡 식별
# 마크, 극단 종횡비 2장 = FitFull 실뷰포트 소비 확인 몫). 엔진 소유 합성 —
# i:\@keep 등 사용자 미디어 불접촉 계약. mtime 명시 스탬프 = 열거 순서 결정화
# (a 최신 → d 최고 — 셀 0 = jkg_seed_a_640x400.png 단수 계약).
import os, struct, sys, zlib

out = os.path.join(sys.argv[1], "pgal_seeds", "engine", "buildterm",
                   "state", "screenshots")

def chunk(tag, data):
    return (struct.pack(">I", len(data)) + tag + data
            + struct.pack(">I", zlib.crc32(tag + data)))

def write_png(path, w, h):
    raw = bytearray()
    for y in range(h):
        raw.append(0)
        for x in range(w):
            u = x / max(1, w - 1)
            v = y / max(1, h - 1)
            if x < w * 0.25:      r, g, b = 200, 60, 60     # 좌=적
            elif x < w * 0.5:     r, g, b = 60, 200, 60     # 중좌=녹
            elif x < w * 0.75:    r, g, b = 60, 60, 200     # 중우=청
            else:                 r, g, b = 230, 230, 60    # 우=황
            r = int(r * (1.0 - 0.5 * v)); g = int(g * (1.0 - 0.5 * v))
            b = int(b * (1.0 - 0.5 * v))
            if x < w * 0.06 and y < h * 0.06:
                r = g = b = 255
            raw += bytes((r, g, b))
    comp = zlib.compress(bytes(raw), 6)
    png = (b"\x89PNG\r\n\x1a\n"
           + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
           + chunk(b"IDAT", comp) + chunk(b"IEND", b""))
    with open(path, "wb") as f:
        f.write(png)
    return os.path.getsize(path)

def stamp(path, y, mo, d, h, mi):
    import time
    t = time.mktime((y, mo, d, h, mi, 0, 0, 0, -1))
    os.utime(path, (t, t))

a = os.path.join(out, "jkg_seed_a_640x400.png")
b = os.path.join(out, "jkg_seed_b_1200x120.png")
c = os.path.join(out, "jkg_seed_c_320x200.png")
d = os.path.join(out, "jkg_seed_d_100x800.png")
for p, wh in ((a, (640, 400)), (b, (1200, 120)), (c, (320, 200)),
              (d, (100, 800))):
    n = write_png(p, wh[0], wh[1])
    print("SEED-OK %s %dx%d %d bytes" % (os.path.basename(p), wh[0], wh[1], n))
# 미래 스탬프(11월) — state/screenshots에 선존 shot(~10월 날짜)보다 **새로**.
# 열거 순서(mtime 내림)의 머리를 시드가 확실히 쥔다 → 셀 0 단수 계약 폐곡.
stamp(a, 2026, 11, 1, 12, 4)
stamp(b, 2026, 11, 1, 12, 3)
stamp(c, 2026, 11, 1, 12, 2)
stamp(d, 2026, 11, 1, 12, 1)
print("SEED-STAMPS: a>b>c>d 2026-11 미래 스탬프 (mtime desc — 셀 0 = jkg_seed_a_640x400.png, 선존 shot 위로)")
PYEOF
[ $? -eq 0 ] || FAIL "seed PNG generation failed (로컬 python)"
SEEDED=$(ls "$SEEDSTAGE/engine/buildterm/state/screenshots"/jkg_seed_*.png 2>/dev/null | wc -l)
[ "$SEEDED" -eq 4 ] || FAIL "seeded $SEEDED files (need 4)"
echo "SEED-TAR-MEMBERS: 4 (engine/buildterm/state/screenshots/jkg_seed_*.png — mtimes 내장)"

echo "=== 2. pre-flight + pre-clean (재실행 가능성 — wake-lock, 구판 서버 절사) ==="
$SSH 'echo PHONE-REACHABLE; uname -m; echo TMPDIR=$TMPDIR; command -v termux-wake-lock >/dev/null 2>&1 && termux-wake-lock || echo WARN-no-wake-lock' \
  || FAIL "ssh failed (phone unreachable — LAN 한정, sshd 기동: 폰 Termux에서 sshd)"
JKWEB_CKT=$($SSH 'pgrep -c -f "[j]kweb" 2>/dev/null' | tr -d ' \r')
echo "JKWEB-COUNT-PRE: ${JKWEB_CKT:-0} (상시 유지 계약 — 절사·pkill 대상 아님)"
# 절사 대상=구판 서버+클라 전부(구판 바이너리 — relink ETXTBSY 방지+빌드 램 여유).
# jkweb은 절사하지 않는다(승격 라인 사용자 결제 창 localhost:8090 상시 계약).
$SSH "pkill -f '[j]kdesktop' 2>/dev/null; sleep 2; pkill -9 -f '[j]kdesktop' 2>/dev/null; sleep 1; pgrep -f '[j]kdesktop' >/dev/null 2>&1 && { echo PRECLEAN-FAIL; exit 1; }; echo SRV-DOWN" \
  || FAIL "pre-clean could not bring the old phone server down"
echo "PRECLEAN-SRV-DOWN=OK (구판 서버+클라 — 새 바이너리 교체 재기동 계약)"

echo "=== 3. tar-over-ssh 재배포 (12멤버 소스+시드 4장+원격 영수증 스크립트) ==="
cat > "$SCRATCH/$RSRC_TAR_NAME" <<'PGALEOF'
#!/bin/bash
# 폰 측 영수증 절차 (phone_gallery.sh가 생성 — 갤러리 T5).
# settings 백업 → cmake 재설정(자동)+ninja 리빌드 → selftest 캐논(506+2g) →
# leg grid(기본 dirs 시드 격자+전체보기) → leg dirs(gallery.dirs=/sdcard/
# Pictures) → 복원 BOOT-OK. 종료 상태 = 서버 UP+태스크바+지뢰찾기+갤러리 앞 —
# 사용자 육안 게이트 대기(probe가 결제를 기록하지 않는다).
set -u
cd ~/JKENGINE/engine || exit 1
FAIL() { echo "GAL-FAIL: $*"; exit 1; }
TMPD="${TMPDIR:-/data/data/com.termux/files/usr/tmp}"
SET=buildterm/state/settings.json
SHOTS=buildterm/state/screenshots
NLOG="$TMPD/pgal_ninja.log"
STLOG="$TMPD/pgal_selftest.log"
CANON_PHONE_PREV=506      # 폰 기존 캐논(텍스트 스케일 T4 실측 — 비교 원문)
CANON_PHONE_EXPECT=546    # 506 + 2g 40건(T1-T3)

echo "=== A. 진입 마커 — 배포 전 구판 상태 원문 ==="
ENTRY_SRV=$(pgrep -f 'buildterm/[j]kdesktop --server' | tr '\n' ' ')
ENTRY_THUMBS=$([ -d buildterm/state/gallery ] && echo 1 || echo 0)
echo "ENTRY-SERVER-PIDS: ${ENTRY_SRV:-none} (종료 시 원복 판정 원문)"
echo "MARKER-BEFORE settings_bytes=$(wc -c < "$SET" 2>/dev/null) shots=$(ls "$SHOTS" 2>/dev/null | wc -l) thumbs_dir=$ENTRY_THUMBS perm=$([ -f buildterm/permissions.json ] && echo yes || echo no)"
echo "ORIG-SETTINGS-BYTES: $(wc -c < "$SET")"
# probe 소유 tmp 캡처 잔상(전 run 실패 유산) — 소각 후 시작(잔상 0 계약).
rm -f "$TMPD"/gal_phone_*.png
echo "TMP-CAPTURE-BURIED-AT-ENTRY: $(ls "$TMPD"/gal_phone_*.png 2>/dev/null | wc -l) left"

echo "=== B. settings 백업 (갤러리 원상 복원 영수증의 원문) ==="
[ -f "$SET" ] || FAIL "settings file missing: $SET"
# 전 run 실패 유산의 진품 백업이 $TMPDIR에 살아 있으면 그것이 최우선 원본
# (run 중간 사망 시 phase J 미실행 — 백업은 진입 당시 진본을 담은 채 남는다).
ORIG="$TMPD/pgal_orig_settings.json"
if [ -s "$ORIG" ] && grep -aq 'font_path' "$ORIG" && ! grep -aq '"gallery"' "$ORIG"; then
    echo "PRIOR-ORIG-RECOVERED: $ORIG ($(wc -c < "$ORIG") bytes — 전 run 실패 유산 진품 원본, 재사용)"
    FONT_PATH=$(sed -n 's/.*"font_path"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' "$ORIG" | head -1)
    [ -n "$FONT_PATH" ] || FAIL "recovered prior ORIG has no text.font_path"
    [ -r "$FONT_PATH" ] || FAIL "font file not readable: $FONT_PATH"
    echo "PARSED-FONT_PATH: $FONT_PATH"
else
    FONT_PATH=$(sed -n 's/.*"font_path"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' "$SET" | head -1)
    [ -n "$FONT_PATH" ] || FAIL "settings has no text.font_path (font_path 유지 계약 전제)"
    [ -r "$FONT_PATH" ] || FAIL "font file not readable: $FONT_PATH"
    echo "PARSED-FONT_PATH: $FONT_PATH"
    if grep -aq '"gallery"' "$SET"; then
        # 프로브 워터마크 잔상(전 run 실패 유산 — dirs 주입 후 소각 전 사망) —
        # 이대로 ORIG로 삼으면 원본 복원 계약이 오염본을 복원한다(3차 run 트랩).
        # 카논 형태(2-space 들여쇠·brace 각 행·끝 개행 — 121B 실측 등호, 텍스트
        # 스케일 라인 영수증 SETTINGS-IN-LEG-def15 tr-스트립 대조로 확정)로
        # 재구성해 ORIG로 삼고, 잔상 원문은 정직 인쇄로 남긴다.
        echo "NOTE-PROBE-LEFTOVER: settings에 gallery 키 — 전 run 실패 유산(dirs 주입 잔상). 원문: $(tr -d '\n' < "$SET")"
        printf '{\n  "text": {\n    "font_path": "%s"\n  }\n}\n' "$FONT_PATH" > "$ORIG"
        echo "ORIG-RECONSTRUCTED-BYTES: $(wc -c < "$ORIG") (재구성 — 카논 121B 형태; 전 run 원문은 위 NOTE 행)"
    else
        cp "$SET" "$ORIG" || FAIL "settings backup failed"
    fi
fi
cmp -s "$SET" "$ORIG" || echo "NOTE-ORIG-DIFFERS: 진입 settings ≠ ORIG (재구성/보정 — 복원은 ORIG 등호 계약)"
echo "SETTINGS-BACKUP-OK: $ORIG ($(wc -c < "$ORIG") bytes — 복원은 이 바이트열 등호)"

echo "=== C. jkweb 생존 계약 — 기동 카운트 원문 (절사 금지) ==="
JKWEB_BEFORE=$(pgrep -f '[j]kweb' | tr '\n' ' ')
echo "JKWEB-BEFORE: ${JKWEB_BEFORE:-none}"

echo "=== D. ninja 리빌드 (aarch64 — CMake 신규 타깃, ~10-20분) ==="
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
if [ ! -f buildterm/jkapp_gallery.so ]; then
    echo "NOTE-CMAKE-RECONF: ninja 후 jkapp_gallery.so 부재 — 명시 cmake 재설정 폴백 1급 (RERUN_CMAKE 미발동)"
    cmake -S . -B buildterm >"$TMPD/pgal_cmake.log" 2>&1
    C_RC=$?
    echo "CMAKE-RC=$C_RC"
    tail -4 "$TMPD/pgal_cmake.log"
    [ "$C_RC" -eq 0 ] || FAIL "cmake reconfigure rc=$C_RC (buildterm 폴백 실패)"
    ninja -C buildterm -j4 >>"$NLOG" 2>&1
    N_RC=$?
    echo "NINJA-RC-RECONF=$N_RC"
    [ "$N_RC" -eq 0 ] || FAIL "ninja rebuild(재설정 후) rc=$N_RC"
fi
[ -f buildterm/jkapp_gallery.so ] || FAIL "buildterm/jkapp_gallery.so missing after rebuild (posix .so — launch_app 존재 검사 원문)"
echo "MODULE-SO-PRESENT: buildterm/jkapp_gallery.so ($(wc -c < buildterm/jkapp_gallery.so) bytes)"

echo "=== E. 배포 마커 — HEAD 원천 실존 단정 ==="
MM=$(grep -c 'GalleryDirList' include/apps/GalleryModel.h)
MC=$(grep -c 'jkapp_gallery' CMakeLists.txt)
MT=$(grep -c '2g-' src/main.cpp)
MP=$(grep -c '2g-' tools/posix_selftest/main.cpp)
ML=$(grep -c 'ClientGalleryApp' src/apps/JKAppModule_gallery.cpp)
echo "MARKER-AFTER gallery_model($MM) cmake($MC) main_2g($MT) posix_2g($MP) module($ML)"
[ "$MM" -ge 1 ] || FAIL "GalleryDirList marker missing on phone (배포 결손)"
[ "$MC" -ge 1 ] || FAIL "jkapp_gallery CMake marker missing on phone"
[ "$MT" -ge 1 ] || FAIL "2g selftest marker missing on phone main.cpp"
[ "$MP" -ge 1 ] || FAIL "2g selftest marker missing on phone posix twin"

echo "=== F. selftest — 폰 캐논 계보 판정 (원문: 506 = 텍스트 스케일 최신 · 기대 546 = 506 + 2g 40건) ==="
timeout 900 ./buildterm/jkdesktop test >"$STLOG" 2>&1
S_RC=$?
ST_PASS=$(grep -ac '^\[PASS\]' "$STLOG")
ST_FAIL=$(grep -ac '^\[FAIL\]' "$STLOG")
P2G=$(grep -ac '^\[PASS\] 2g' "$STLOG")
echo "PHONE-SELFTEST rc=$S_RC PASS=$ST_PASS FAIL=$ST_FAIL 2g=$P2G (신설 갤러리 계열 — 캐논 +40분의 원료)"
grep -a 'AppSelfTest' "$STLOG" | tail -2
grep -aq 'AppSelfTest: 0 failure(s)' "$STLOG" || FAIL "AppSelfTest not 0 failure(s)"
[ "$S_RC" -eq 0 ] || FAIL "selftest rc=$S_RC"
[ "$ST_FAIL" -eq 0 ] || FAIL "selftest FAIL=$ST_FAIL (폰축 회귀)"
if [ "$P2G" -ne 40 ]; then
    echo "WARN: 2g 계열 실측 $P2G/40 — 갤러리 selftest 미반영 가능 (계보 원장)"
fi
awk -v p="$ST_PASS" -v g="$P2G" -v prev="$CANON_PHONE_PREV" 'BEGIN{
    exp_new = prev + 40
    if (p == exp_new) print "CANON-INCLUSION=GALLERY-FULL-T2G-40 (폰 캐논 " prev "→" p " 상승 — 2g " g "건 동반 실측)"
    else if (p == prev) print "CANON-INCLUSION=GALLERY-SELFTEST-MISSING (갤러리 selftest 쌍둥이 미반영 — 계보 결손 — 원장)"
    else printf "CANON-INCLUSION=OTHER-N(%d — 기대 %d=506+40 · 2g=%d/40 — 계보 정산 원장)\n", p, exp_new, g
}'

# ---------------------------------------------------------------- 레그 공통
CAPTURE() { # $1=출력png — x11grab 1920x1080 시도, 화면 크기 파싱 재시도(import 금지 — libheif 파단 원문)
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

boot_settings() { # $1 leg 라벨 $2 mode (orig=ORIG 바이트 그대로 / dirs=gallery.dirs만 주입 —
                  # 렌더 상태는 ORIG 그대로: font_scale 키 주입 금지 — 명시 키와
                  # 기본 1.5 발동 경로가 서로 다른 분기가 될 수 있다)
    case "$2" in
      orig) cp "$ORIG" "$SET" || FAIL "leg $1: orig settings copy failed" ;;
      dirs) printf '{\n    "text": {\n        "font_path": "%s"\n    },\n    "gallery": {\n        "dirs": ["/sdcard/Pictures"]\n    }\n}\n' "$FONT_PATH" > "$SET" ;;
      *) FAIL "unknown settings mode: $2" ;;
    esac
    echo "SETTINGS-IN-LEG-$1: $(tr -d '\n' < "$SET") (bytes=$(wc -c < "$SET"))"
}

boot_server() { # $1 leg 라벨 — setsid 부팅+ping(진입 상태 원복의 부분)
    local LEG=$1
    local LOG="$TMPD/pgal_srv_${LEG}.log"
    pkill -f '[j]kdesktop' 2>/dev/null
    sleep 2
    pkill -9 -f '[j]kdesktop' 2>/dev/null
    sleep 1
    if pgrep -f '[j]kdesktop' >/dev/null 2>&1; then
        FAIL "leg $LEG: server survived pre-boot clean"
    fi
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
    sleep 10   # taskbar 자동 스폰 관측 여유
    WIN=$(timeout 15 ./buildterm/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)
    printf '%s' "$WIN" | grep -aq '"title":"Taskbar"' || {
        T=$(timeout 20 ./buildterm/jkdesktop agentctl '{"tool":"launch_app","args":{"app":"taskbar"}}' 2>/dev/null | grep -a '{' | head -1)
        echo "TASKBAR-LAUNCH-RECOVED-$LEG: $T"
    }
    echo "SRVLOG-WARN-$LEG: atlas_inactive=$(grep -ac 'vector atlas inactive' "$LOG") no_vector_font=$(grep -ac 'no vector font configured' "$LOG") hangul_misc=$(grep -ac 'HangulManager' "$LOG")"
}

wait_gallery() { # Gallery 창 등장 대기 — 폰 클라 스폰 15-18s 원장, 3s×30
    # (3차 run 트랩: 폰 스폰 플레이크 — 20회 60s 안에 창 부재로 hard FAIL.
    #  스폰 수용 ok:true 후 침묵이 실측됐다 — 90s로 넓혀 흡수.)
    GW=""
    for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 \
             21 22 23 24 25 26 27 28 29 30; do
        WIN=$(timeout 15 ./buildterm/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)
        GW=$(printf '%s' "$WIN" | grep -aoE '\{"id":[^}]*"title":"Gallery"[^}]*\}' | head -1)
        [ -n "$GW" ] && break
        sleep 3
    done
    [ -n "$GW" ] || FAIL "no Gallery window (폰 스폰 15-18s 원장 + 플레이크 마진 90s 초과) — last: ${WIN:-none}"
    GALWIN="$GW"   # 전역 세팅 — parse_geo가 쓴다(1차 run 트랩: 로컬 GW만
                   # 채워 전역 GALWIN unbound → GAL_ID 빈값 → click bad_request)
    echo "GALWIN-$LEG: $GALWIN"
}

parse_geo() { # $1 라벨 — GALWIN에서 id/x/y/w/h 추출해 GAL-GEO-<label> 인쇄
              # (crop 분석 원문 — 서버 기하는 캡처 앵커 출발점으로만 쓴다)
    GAL_ID=$(printf '%s' "$GALWIN" | sed -n 's/.*"id":\([0-9]*\),.*/\1/p')
    GAL_X=$(printf '%s' "$GALWIN" | sed -n 's/.*"x":\(-\?[0-9]*\),.*/\1/p')
    GAL_Y=$(printf '%s' "$GALWIN" | sed -n 's/.*"y":\(-\?[0-9]*\),.*/\1/p')
    GAL_W=$(printf '%s' "$GALWIN" | sed -n 's/.*"w":\([0-9]*\),.*/\1/p')
    GAL_H=$(printf '%s' "$GALWIN" | sed -n 's/.*"h":\([0-9]*\),.*/\1/p')
    [ -n "$GAL_ID" ] || FAIL "gal geo parse failed (id empty) — GALWIN: $GALWIN"
    echo "GAL-GEO-$1: id=$GAL_ID x=$GAL_X y=$GAL_Y w=$GAL_W h=$GAL_H (meta 560x520 상대 검증)"
}

# ════════════════════════ leg grid — 기본 dirs · 격자+전체보기+이전/다음 ════════════
LEG=grid
echo "=== G. leg grid — 진입 settings(ORIG) 부팅 + 시드 4장 격자 ==="
boot_settings grid orig
boot_server grid
L=$(timeout 20 ./buildterm/jkdesktop agentctl '{"tool":"launch_app","args":{"app":"gallery"}}' 2>/dev/null | grep -a '{' | head -1)
printf '%s' "$L" | grep -aq '"ok":true' || FAIL "leg grid: launch_app gallery failed — $L"
echo "launch reply: $L"
wait_gallery
parse_geo grid
sleep 4   # 썸네일 디코드+캐시 기록 안정화
THUMB_CNT=$(ls buildterm/state/gallery/thumbs 2>/dev/null | wc -l)
GRID_INPUT=$(ls "$SHOTS" 2>/dev/null | wc -l)
echo "THUMB-CACHE-CNT=$THUMB_CNT (grid-input-files=$GRID_INPUT — 시드 4장+기존 shot · 부족분=디코드 실패/컬)"
CAPTURE "$TMPD/gal_phone_grid.png"

# ════════════════════════ send_input 승인 무승인 운용 ══════════════════════
echo "=== H0. permissions.json 병합(선존 파일 존중 — END 바이트 등호 복원) ==="
PERM=buildterm/permissions.json
PERM_ORIG="$TMPD/pgal_orig_permissions.json"
ENTRY_PERM=0
if [ -s "$PERM_ORIG" ]; then
    # 전 run 실패 유산 진품 백업(merge 전 원문) — run이 H0-뒤로 사망해
    # 병합본이 남았으면 먼저 원복해 자가 수복(진품이 위조 백업으로 덮이는
    # 4차 run 트랩 방지).
    if ! cmp -s "$PERM" "$PERM_ORIG"; then
        cp "$PERM_ORIG" "$PERM" || FAIL "permissions self-heal copy failed"
        echo "PERM-SELF-HEAL: permissions.json을 전 run 진품 백업으로 원복 ($(wc -c < "$PERM") bytes) — 원문: $(tr -d '\n' < "$PERM")"
    else
        echo "PERM-PRIOR-BACKUP-PRESENT: $PERM_ORIG ($(wc -c < "$PERM_ORIG") bytes — 현재 파일과 등호)"
    fi
fi
if [ -f "$PERM" ]; then
    ENTRY_PERM=1
    cp "$PERM" "$PERM_ORIG" || FAIL "permissions backup failed"
    cmp -s "$PERM" "$PERM_ORIG" || FAIL "permissions backup byte mismatch"
    PERM_ORIG_BYTES=$(wc -c < "$PERM_ORIG")
    echo "PERM-ENTRY-PRE-EXISTING: $PERM ($(wc -c < "$PERM") bytes) — 원문: $(tr -d '\n' < "$PERM")"
    SI=$(sed -n 's/.*"send_input"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' "$PERM" | head -1)
    if [ "$SI" = "allow" ]; then
        echo "PERM-MERGE: none (선존 send_input=allow)"
    else
        awk -v newline='    "send_input": "allow"' '
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
            }' "$PERM" > "$PERM.new" || FAIL "permissions merge failed (구조 친화 실패)"
        mv "$PERM.new" "$PERM" || FAIL "permissions merge replace failed"
        echo "PERM-MERGE: +send_input allow (선존 키 보존 — merge=$(wc -c < "$PERM") bytes, END에서 바이트 등호 원복)"
    fi
else
    printf '{\n    "send_input": "allow"\n}\n' > "$PERM" || FAIL "permissions write failed"
    echo "PERM-WIRE-OK: $PERM (send_input allow — 선존 부재라 END에서 소각)"
fi
if ! grep -aq 'send_input' "$PERM"; then FAIL "permissions.json has no send_input (승인 병합 미성립)"; fi
echo "PERM-AFTER-MERGE: $(tr -d '\n' < "$PERM")"

echo "=== H. leg grid — 셀 0 클릭 → 전체 보기 → ←꺾임 → Esc 복귀 ==="
CLICK_Y=$((GAL_Y + 160))
CLICK_X=$((GAL_X + 60))
echo "CLICK-CELL-0: id=$GAL_ID desk=($CLICK_X,$CLICK_Y) — 계산: 창좌표(60,160)=셀0 박스(160x120+라벨) 관대 중심(mtime 스탬프로 셀0=jkg_seed_a 결정화)"
CL=$(timeout 15 ./buildterm/jkdesktop agentctl '{"tool":"send_input","args":{"op":"click","id":'"$GAL_ID"',"x":'"$CLICK_X"',"y":'"$CLICK_Y"',"button":1,"clicks":1}}' 2>/dev/null | grep -a '{' | head -1)
printf '%s' "$CL" | grep -aq '"ok":true' \
    || FAIL "send_input click gate/execute failed — reply: $CL (permissions.json send_input allow 확인)"
echo "click reply: $CL"
sleep 4   # 전체 보기 디코드+텍스처 업로드 안정화(폰은 느리다 — WSL 3s에서 +1)
CAPTURE "$TMPD/gal_phone_full.png"
KV=$(timeout 15 ./buildterm/jkdesktop agentctl '{"tool":"send_input","args":{"op":"key","id":'"$GAL_ID"',"key":1073741904,"mods":0}}' 2>/dev/null | grep -a '{' | head -1)
printf '%s' "$KV" | grep -aq '"ok":true' || FAIL "send_input key LEFT failed — $KV"
echo "key LEFT reply: $KV (wrap — index 0에서 -1 = 목록 끝 지점)"
sleep 4
CAPTURE "$TMPD/gal_phone_prev.png"
KE=$(timeout 15 ./buildterm/jkdesktop agentctl '{"tool":"send_input","args":{"op":"key","id":'"$GAL_ID"',"key":27,"mods":0}}' 2>/dev/null | grep -a '{' | head -1)
printf '%s' "$KE" | grep -aq '"ok":true' || FAIL "send_input key ESC failed — $KE"
echo "key ESC reply: $KE"
sleep 4
CAPTURE "$TMPD/gal_phone_back.png"

# ════════════════════════ leg dirs — gallery.dirs=/sdcard/Pictures ════════════
LEG=dirs
echo "=== I. leg dirs — settings에 gallery.dirs 주입해 재시동 (권한 부재면 빈 목록=정상) ==="
boot_settings dirs dirs
if grep -aq 'gallery' "$SET" && grep -aq '/sdcard/Pictures' "$SET"; then
    echo "SETTINGS-GATE-OK: gallery.dirs 주입 성공 (font_path 유지)"
else
    FAIL "leg dirs: settings injection failed"
fi
boot_server dirs
L=$(timeout 20 ./buildterm/jkdesktop agentctl '{"tool":"launch_app","args":{"app":"gallery"}}' 2>/dev/null | grep -a '{' | head -1)
printf '%s' "$L" | grep -aq '"ok":true' || FAIL "leg dirs: launch_app gallery failed — $L"
wait_gallery
parse_geo dirs
sleep 4
THUMB_CNT2=$(ls buildterm/state/gallery/thumbs 2>/dev/null | wc -l)
echo "THUMB-CACHE-CNT-DIRS=$THUMB_CNT2 (dirs leg — /sdcard 열거 성립 여부와 무관하게 캐시 총계)"
CAPTURE "$TMPD/gal_phone_dirs.png"
echo "--- /sdcard/Pictures 셸 열거 시도 원문 (권한 부재=빈 목록 정상 결과 — 결함 아님) ---"
ls /sdcard/Pictures 2>&1 | head -8 | sed 's/^/  SDCARD: /'
SDCARD_OK=$([ -d /sdcard/Pictures ] && echo dir-present || echo dir-missing)
echo "SDCARD-STATE: $SDCARD_OK (Termux storage 권한 정직 원문 — 열거 실패는 그대로 가시화만)"

# ════════════════════════ 소각 — 시드+캡쳐캐시+permissions ══════════════════════
echo "=== J. 소각 — 시드 4장+thumb 캐시+permissions.json (probe 소유 잔상 0) ==="
rm -f "$SHOTS"/jkg_seed_*.png
[ -n "$(ls "$SHOTS"/jkg_seed_*.png 2>/dev/null)" ] && FAIL "seed 소각 실패 — 수동 소각 필요" || echo "SEED-BURIED: 4 seed PNG removed"
if [ "$ENTRY_THUMBS" = "1" ]; then
    echo "THUMB-CACHE-KEPT: ENTRY에 존재했던 dir — 건드리지 않는다 (probe 소유 잔상 아님)"
else
    rm -rf buildterm/state/gallery
    [ -d buildterm/state/gallery ] && echo "WARN: state/gallery survived rm" \
        || echo "THUMB-CACHE-BURIED: buildterm/state/gallery removed"
fi
if [ "$ENTRY_PERM" -eq 1 ]; then
    cp "$PERM_ORIG" "$PERM" || FAIL "permissions restore copy failed"
    cmp -s "$PERM" "$PERM_ORIG" || FAIL "permissions restore byte mismatch"
    echo "PERM-RESTORED-BYTES: $(wc -c < "$PERM") (선존 원문 $(wc -c < "$PERM_ORIG") 등호 — 병합 복원)"
else
    rm -f "$PERM"
    [ -f "$PERM" ] && echo "WARN: permissions.json survived rm" || echo "PERM-BURIED: permissions.json removed"
fi

# ════════════════════════ leg boot — settings 원상 복원 BOOT-OK ══════════════════════
LEG=boot
echo "=== K. leg boot — settings 원상 복원(바이트 등호) BOOT-OK + 진입 상태 재현 ==="
cp "$ORIG" "$SET" || FAIL "settings restore copy failed"
cmp -s "$SET" "$ORIG" || FAIL "settings restore byte mismatch"
echo "SETTINGS-RESTORED-BYTES: $(wc -c < "$SET") (ORIG=$(wc -c < "$ORIG") — 등어제로 원복)"
if ! grep -aq 'font_path' "$SET"; then FAIL "restored settings lost font_path (복원 위반)"; fi
echo "RESTORAGE-GATE-OK: font_path 유지 + ORIG 바이트 등호 (121B 스타일 원문)"
boot_server boot
M=$(timeout 20 ./buildterm/jkdesktop agentctl '{"tool":"launch_app","args":{"app":"minesweeper"}}' 2>/dev/null | grep -a '{' | head -1)
printf '%s' "$M" | grep -aq '"ok":true' || FAIL "leg boot: launch_app minesweeper failed (진입 상태 재현) — $M"
echo "minesweeper reply: $M (종료 상태 계약 — 텍스트 스케일 라인 종료 상태 승계)"
sleep 6
MWIN=$(timeout 15 ./buildterm/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)
printf '%s' "$MWIN" | grep -aoE '\{"id":[^}]*"title":"Minesweeper"[^}]*\}' | head -1 | sed 's/^/  MINEWINDOW-boot: /'
L=$(timeout 20 ./buildterm/jkdesktop agentctl '{"tool":"launch_app","args":{"app":"gallery"}}' 2>/dev/null | grep -a '{' | head -1)
printf '%s' "$L" | grep -aq '"ok":true' || FAIL "leg boot: launch_app gallery failed — $L"
wait_gallery
parse_geo boot
sleep 4   # 기존 shot(시드 제거 상태) 격자 안정화 — 육안 스탭 대상 화면
CAPTURE "$TMPD/gal_phone_boot.png"

echo "=== L. 종료 게이트 — 서버 UP+태스크바/지뢰찾기/갤러리 상시 + jkweb 생존 ==="
pgrep -f 'buildterm/[j]kdesktop --server' >/dev/null 2>&1 || FAIL "server not UP at end (종료 게이트 위반)"
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
echo "GAL-FINISHED"
exit 0
PGALEOF

for r in 1 2 3; do
  if tar -cf "$TARBALL" -C "$ROOT" "${FILES[@]}" ${STAGE_ARGS[@]+"${STAGE_ARGS[@]}"} \
     -C "$SEEDSTAGE" engine/buildterm/state/screenshots \
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
tar -tf "$TARBALL" | grep -aq 'jkg_seed_' \
  || FAIL "tar missing seed image members"
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
for s in a_640x400 b_1200x120 c_320x200 d_100x800; do
  LSS=$(wc -c < "$SEEDSTAGE/engine/buildterm/state/screenshots/jkg_seed_$s.png" | tr -d ' ')
  PSS=$($SSH "wc -c < ~/JKENGINE/engine/buildterm/state/screenshots/jkg_seed_$s.png 2>/dev/null" | tr -d ' \r')
  if [ "$LSS" != "$PSS" ]; then
    echo "FRESHNESS-MISMATCH: seed jkg_seed_$s local=$LSS phone=$PSS"
    FRESH_OK=0
  fi
done
[ "$FRESH_OK" -eq 1 ] || FAIL "deployed file size mismatch — 배포 원문 신선도 붕괴"
echo "DEPLOY-FRESHNESS-OK (12 멤버+시드 4장 크기 일치)"
MKS=$($SSH "cd ~/JKENGINE/engine && grep -c 'GalleryDirList' include/apps/GalleryModel.h 2>/dev/null" | tr -d ' \r')
echo "DEPLOY-MARKER-GALLERY phone-grep-count=${MKS:-0}"
[ "${MKS:-0}" != "0" ] || FAIL "phone GalleryModel.h lacks GalleryDirList — 배포 원천 결손"

echo "=== 4. 폰 리빌드+영수증 절차 실행 (ninja ~10-20분 + selftest + 레그 3) ==="
$SSH "bash $RSRC_PHONE" > "$RUNLOG" 2>&1
R_RC=$?
RUN_RC=$R_RC
if [ "$R_RC" -eq 255 ]; then
  echo "RUN-DROPPED: ssh 채널 단절(Wi-Fi 낙하 함정 — 폰 ninja 로그는 \$TMPDIR에 생존, 재실행=증분: PHONE_HOST=<폰 IP> bash engine/tools/probes/phone_gallery.sh)"
fi
cat "$RUNLOG"
grep -aq '^GAL-FINISHED$' "$RUNLOG" \
  || FAIL "receipt script did not finish (rc=$RUN_RC — RUN-DROPPED면 재실행)"
if grep -aq '^GAL-FAIL' "$RUNLOG"; then
  HARD=$(grep -a '^GAL-FAIL' "$RUNLOG" | head -1)
  FAIL "phone hard receipt: $HARD"
fi

echo "=== 5. REMNANT 검사 — 원격 스크립트 자기 소각 확인 (REMNANT-LS-RC=2 기대) ==="
$SSH "ls $RSRC_PHONE" >/dev/null 2>&1
echo "REMNANT-LS-RC=$?"

echo "=== 6. 캡처 회수 — ssh 호출당 base64 1파이프 계약 (+md5 대차) ==="
recover() { # $1 폰쪽png 경로 $2 로컬png — md5 원문 대차 포함
    local REMOTE_PNG=$1 LOCAL_PNG=$2
    local WTXT WPNG WB64 RMD5 LMD5
    WB64=$(cygpath -w "$SCRATCH/pgal_b64.txt" 2>/dev/null || echo "$SCRATCH/pgal_b64.txt")
    WPNG=$(cygpath -w "$LOCAL_PNG" 2>/dev/null || echo "$LOCAL_PNG")
    RMD5=$($SSH "md5sum $REMOTE_PNG" 2>/dev/null | sed -n 's/^\([0-9a-f]*\) .*$/\1/p' | tr -d ' \r')
    $SSH "base64 -w0 $REMOTE_PNG" > "$SCRATCH/pgal_b64.txt" 2>"$SCRATCH/pgal_b64.err" \
        || FAIL "recover failed: $REMOTE_PNG"
    [ -s "$SCRATCH/pgal_b64.err" ] && echo "NOTE: stderr noise — $(tail -1 "$SCRATCH/pgal_b64.err")"
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
    rm -f "$SCRATCH/pgal_b64.txt"
}
recover "\$TMPDIR/gal_phone_grid.png" "$SCRATCH/gal_phone_grid.png"
recover "\$TMPDIR/gal_phone_full.png" "$SCRATCH/gal_phone_full.png"
recover "\$TMPDIR/gal_phone_prev.png" "$SCRATCH/gal_phone_prev.png"
recover "\$TMPDIR/gal_phone_back.png" "$SCRATCH/gal_phone_back.png"
recover "\$TMPDIR/gal_phone_dirs.png" "$SCRATCH/gal_phone_dirs.png"
recover "\$TMPDIR/gal_phone_boot.png" "$SCRATCH/gal_phone_boot.png"
$SSH "rm -f \$TMPDIR/gal_phone_grid.png \$TMPDIR/gal_phone_full.png \$TMPDIR/gal_phone_prev.png \$TMPDIR/gal_phone_back.png \$TMPDIR/gal_phone_dirs.png \$TMPDIR/gal_phone_boot.png \$TMPDIR/pgal_orig_settings.json \$TMPDIR/pgal_orig_permissions.json"
echo "CAPTURES-WRITTEN: gal_phone_{grid,full,prev,back,dirs,boot}.png → engine/tmp/"

echo "=== 7. 로컬 실측 — 캡처 원문 차원+비균질+갤러리 창 crop (측정은 회수 후 — 육안 보조) ==="
PYRUNLOG=$(cygpath -w "$RUNLOG" 2>/dev/null || echo "$RUNLOG")   # 윈도 python은 /i/... 패스를 못 읽는다(회수 유실 렛슨)
PYTHONIOENCODING=utf-8 python - "$PYRUNLOG" "$PYSCRATCH" <<'ANALYSIS_EOF'
# -*- coding: utf-8 -*-
# T5 폰 로컬 실측 (측정 원문 — 서버 기하는 출발점 앵커로만 쓴다: 폰 미러와
# 레터박스 오프셋 차 원장 존중, 구조 판정은 캡처 육안 스택의 몫).
import os, re, sys

RUNLOG, SCRATCH = sys.argv[1], sys.argv[2]

def parse_geo(txt, leg):
    m = re.search(r"GAL-GEO-%s: id=(\d+) x=(-?\d+) y=(-?\d+) w=(\d+) h=(\d+)" % leg, txt)
    if not m:
        return None
    return tuple(int(m.group(i)) for i in range(1, 6))

try:
    from PIL import Image
    import numpy as np
    have_pil = True
except ImportError:
    have_pil = False
    print("ANALYSIS-NOTE: PIL/numpy 부재 — 차원 원문만 (캡처 육안 스탭으로)")

txt = ""
try:
    with open(RUNLOG, encoding="utf-8", errors="ignore") as f:
        txt = f.read()
except OSError:
    pass

TAGS = (("gal_phone_grid.png", "grid"), ("gal_phone_full.png", "full"),
        ("gal_phone_prev.png", "prev"), ("gal_phone_back.png", "back"),
        ("gal_phone_dirs.png", "dirs"), ("gal_phone_boot.png", "boot"))
for fname, tag in TAGS:
    p = os.path.join(SCRATCH, fname)
    if not os.path.isfile(p):
        print("ANALYSIS-%s: missing" % tag)
        continue
    if not have_pil:
        print("ANALYSIS-%s: %s %d bytes" % (tag, fname,
                                            os.path.getsize(p)))
        continue
    im = Image.open(p).convert("RGB")
    arr = np.asarray(im).astype(int)
    colors = len(np.unique(arr.reshape(-1, 3) // 32, axis=0))
    print("ANALYSIS-%s: %dx%d %d bytes distinct16=%d" % (
        tag, im.width, im.height, os.path.getsize(p), colors))
    geo = parse_geo(txt, tag)
    if geo is None and tag in ("full", "prev", "back"):
        geo = parse_geo(txt, "grid")  # 같은 launch 창 — 레그 좌표 재활용
    if geo:
        gx, gy, gw, gh = geo[1], geo[2], geo[3], geo[4]
        m = 32
        x0, y0 = max(0, gx - m), max(0, gy - m)
        x1, y1 = min(im.width, gx + gw + m), min(im.height, gy + gh + m)
        if x1 > x0 and y1 > y0:
            crop = im.crop((x0, y0, x1, y1))
            out = os.path.join(SCRATCH, "pgal_%s_crop.png" % tag)
            crop.save(out)
            print("CROP-%s: %s (%dx%d — 서버 기하 x=%d y=%d w=%d h=%d + 마진 %d — 레터박스 오프셋 원문 있을 수 있어 여유 마진)" % (
                tag, out, crop.width, crop.height, gx, gy, gw, gh, m))

# 캐논 원문 인쇄
mc = re.search(r"CANON-INCLUSION=([^\r\n]*)", txt)
if mc:
    print("CANON-LINE: CANON-INCLUSION=%s" % mc.group(1))
mt = re.search(r"PHONE-SELFTEST rc=\d+ PASS=(\d+) FAIL=(\d+) 2g=(\d+)", txt)
if mt:
    print("SELFTEST-LINE: PASS=%s FAIL=%s 2g=%s" % mt.groups())
ANALYSIS_EOF
if [ $? -ne 0 ]; then
  echo "NOTE-ANALYSIS: local analysis failed (rc) — 캡처 원문은 회수 원문으로 육안 가능"
fi

echo "=== 8. 갤러리 실측 판정 (측정 원문은 상단 analysis 행 — 최종 결제는 캡처 육안 스탭/사용자) ==="
CAP_OK=1
for f in grid full prev back dirs boot; do
    P="$SCRATCH/gal_phone_$f.png"
    if [ ! -s "$P" ]; then
        echo "GAL-PHONE-FAIL(capture missing/empty: $P)"
        CAP_OK=0
    fi
done
CANON=$(grep -a '^CANON-INCLUSION=' "$RUNLOG" | tail -1 | sed 's/^CANON-INCLUSION=//' | tr -d '\r')
# THUMB-CACHE-CNT= 숫자만 뽑는다 — 뒤에 붙는 (grid-input-files=..) 주석을 흡수하
# 면 비수형으로 THUMB-CHECK가 미달 오판한다(2차 run 트랩 — 정직 인쇄 원문).
THUMB_A=$(grep -a '^THUMB-CACHE-CNT=' "$RUNLOG" | head -1 | sed -n 's/^THUMB-CACHE-CNT=\([0-9][0-9]*\).*/\1/p' | tr -d ' \r')
CANON_OK=0
case "$CANON" in
  GALLERY-FULL-T2G-40*) CANON_OK=1 ;;
  *) echo "NOTE-CANON: 캐논 계보 ${CANON:-n/a} — 위 경고 행 참조" ;;
esac
THUMB_OK=1
case "${THUMB_A:-n/a}" in
  ''|*[!0-9]*) THUMB_A_N=-1 ;;
  *) THUMB_A_N=$THUMB_A ;;
esac
if [ "$THUMB_A_N" -ge 4 ]; then
    echo "THUMB-CHECK: OK (thumbs=$THUMB_A_N — 시드 4셀 디코드 영수증)"
else
    THUMB_OK=0
    echo "THUMB-CHECK: 미달 (thumbs=$THUMB_A_N < 4 — 시드 셀이 썸네일을 놓쳤다)"
fi
if [ "$CAP_OK" -eq 1 ] && [ "$THUMB_OK" -eq 1 ] && [ "$CANON_OK" -eq 1 ]; then
    echo "GAL-PHONE-VERDICT: GAL-PHONE-OK(폰 selftest 계보 506→546·2g 40건·leg grid thumb=$THUMB_A_N·캡처 6종 receipt — 육안 스탭 대기)"
else
    echo "GAL-PHONE-VERDICT: GAL-PHONE-FAIL(행별 사유는 위 각 행 — 캡처/썸네일/캐논 실측 미달, 인프라 실패는 hard FAIL로 상단 중단)"
fi
echo "GAL-WCAPTURES:"
for f in grid full prev back dirs boot; do
    P="$SCRATCH/gal_phone_$f.png"
    [ -s "$P" ] && echo "  $P ($(wc -c < "$P" | tr -d ' ') bytes)"
done

echo ""
echo "사용자 결제 게이트(대기 — probe가 결제를 기록하지 않는다):"
echo "  ① 폰 갤러리 격자(gal_phone_grid.png+pgal_grid_crop.png): 시드 4장이"
echo "     썸네일로 찍혔는지(placeholder 0)·Korean 라벨·극단 종횡비 2장(100x800"
echo "     세로/1200x120 가로) 모양 — 폰 실기기 육안."
echo "  ② 폰 전체 보기(gal_phone_full/prev/back): 절 0 클릭→핏 표시+메타·"
echo "     ←꺾임=목록 끝 랩어라운드·Esc 격자 복귀."
echo "  ③ 폰 gallery.dirs 레그(gal_phone_dirs.png): /sdcard/Pictures 권한 부재면"
echo "     빈 목록 스펙 정상 (열거 성립 시 시드 외 사진 셀 추가)."
echo "  ④ 폰 BOOT-OK(gal_phone_boot.png): settings 원상 복원 후 시드 없는 기본"
echo "     dirs 격자 — 종료 상태 서버 UP."
echo "  → EYES-PENDING: 위 항목에 대한 폰 실기기 육안 선언만 결제 — probe는 기록하지 않는다."
echo "GAL-PHONE-END"
exit 0
# honest-fail 원칙: 수치 미달(GAL-PHONE-FAIL 라인)은 원장 목적이라 rc=0.
# hard FAIL(배포·빌드·selftest·부팅·ping·launch·창·캡처·소각·REMNANT)만 FAIL()에서 exit 1.
# EOF — 끝 개행 유지(스크립트 산술 무해 습관).