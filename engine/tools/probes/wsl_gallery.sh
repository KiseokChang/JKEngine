#!/usr/bin/env bash
# T4 WSL 갤러리 실측 probe (스펙 2026-10-09-gallery-design — plan task-4).
#
# 목적: T1-T3 gallery 모듈(격자+전체보기+썸네일 캐시)을 WSL 실측한다 —
#   부팅 → launch_app gallery → 격자 캡처(시드 이미지 썸네일 찍힘) → 셀 1장
#   클릭 → 전체보기 캡처 → ←키(리처드: WrapStep 꺾임 — 0에서 -1 = 끝 지점)
#   캡처 → Esc 격자 복귀 캡처 → 복원.
#
# 계약 원문(승계):
#   * vplayer 미디어 계약 — 테스트 이미지는 **합성 PNG**(자가 생성 스크립트,
#     engine 소유): i:\@keep 등 사용자 미디어 사용 금지. 시드 색 밴드 배치는
#     방향·종횡비를 캡처에서 식별 가능하게 한다(극단 종횡비 2장 = FitFull
#     2g-f 산치의 실뷰포트 소비 확인 몫).
#   * IP 게이트 — 폰 축의 PHONE_HOST는 **본 probe에 불요**(WSL 축, PHONE_HOST
#     없음 계약). 대신 충족되는 fail-closed 가드: WSL 내부 실행만 수용 —
#     WSL_DISTRO_NAME 부재(Windows Git Bash 직행)면 즉시 FAIL(가드는 출력
#     유도 앞에 둔다 — phone_text_scale.sh T4 fix r1 "가드를 exec 앞으로"
#     원문 승계).
#   * 캡처 회수 — 단열 파이프: /tmp 캡처 → engine/tmp 영수증 4종 **단일 cp**
#     (ls·혼입 금지). 영수증 파일명 gal_wsl_{grid,full,prev,back}.png.
#     캡처기=wsl_text_scale_diag.sh의 X11 창 단위 shot(python3 ctypes) — WSLg
#     데스크톱 합성 창 1280x720(폰 x11grab 프레임과 동형의 전체 합성 뷰).
#     plan 원문의 "ffmpeg x11grab"은 폰 축 계약 — WSL 축은 가장 최근 선례
#     (wsl_text_scale_diag.sh)의 X11 경로를 승계한다(실측 완결된 경로만 쓴다).
#   * 복원 — 시드 이미지 소각·서버 DOWN(ENTRY에 UP이면 재부팅)·캐시 dir
#     소각(state/gallery는 probe가 만들었으면 rm -rf — probe 소유 잔상 0)·
#     permissions.json 잔상 소각(send_input 승인 무승인 운용용 — ENTRY에
#     파일이 있으면 건드리지 않고 FAIL, 계약 원문 "전면 allow 런타임 파일은
#     프로브 소유 아님" 존중).
#   * send_input(합성 클릭/키) 기본 게이트=ask(스펙 2026-09-21-conquest-ladder
#     §3.1) — agentctl 단발 연결은 승인 가동층이 없어 approval_unavailable로
#     접힌다. 그래서 probe가 buildwsl/permissions.json에 {"send_input":"allow"}
#     를 1행 기록하고 END에 소각한다(ENTRY 상태 기록+원복 — 위 계약).
#
# 판정(스크립트 규약): 인프라 실패(빌드·부팅·ping·launch·창 부재·캡처 실패·
# 합성 입력 게이트 미성립)만 hard FAIL rc=1. 수치·육안 이벤트는 GAL-OK/GAL-FAIL
# 원장 라인으로 남기고 rc=0(honest-fail) — 최종 판정은 캡처 육안 스탭(사용자).
#
# 표준(wsl_dirty_present/wsl_text_scale_diag 선례): WSLg :0, setsid 부팅,
# 브래킷 pkill+(-9 에스컬레이션), stale 소켓 rm, selftest 출력 **WSL 내부**
# 리다이렉트(Git Bash 파이프 조각 유실 렛슨), ninja 풀 로그 리다이렉트.
#
# 실행 표준(WSL 밖에서):
#   MSYS_NO_PATHCONV=1 wsl.exe -d Ubuntu-24.04 -- bash /mnt/i/progwork/JKENGINE/engine/tools/probes/wsl_gallery.sh
set -u
cd /mnt/i/progwork/JKENGINE/engine

FAIL() { echo "GAL-FAIL: $*"; exit 1; }
LOGDIR=/tmp/gal
mkdir -p "$LOGDIR"
SHOTS=buildwsl/state/screenshots
CANON_WSL_SELFTEST=546   # 캐논 WSL 축 (plan Global Constraints — T1-T3 실측)

# ---------------------------------------------------------------- fail-closed 가드
# 출력 유도(log/write)보다 앞 — 가드가 뒤에 있으면 잘못된 축의 재실행이
# 영수증을 망가뜨린다(phone_text_scale.sh T4 fix r1 원문 승계).
if [ -z "${WSL_DISTRO_NAME:-}" ]; then
  echo "GAL-WFAIL: not inside WSL — 실행 축 계약 위반 (wsl.exe -d Ubuntu-24.04 -- bash /mnt/i/progwork/JKENGINE/engine/tools/probes/wsl_gallery.sh)"
  exit 1
fi
if [ -n "${PHONE_HOST:-}" ]; then
  echo "GAL-NOTE: PHONE_HOST set — WSL 축은 PHONE_HOST 불요(무시하고 진행)"
fi

echo "=== 0. ENTRY 상태 기록 (종료 시 원복 판정용) ==="
ENTRY_UP=0
pgrep -f 'buildwsl/[j]kdesktop' >/dev/null 2>&1 && ENTRY_UP=1
STALE_SEED=$(ls "$SHOTS"/jkg_seed_*.png 2>/dev/null | wc -l)
[ "$STALE_SEED" -eq 0 ] || FAIL "stale seed images exist in $SHOTS — 이전 probe 잔상, 수동 소각 필요"
ENTRY_PERM=0
[ -f buildwsl/permissions.json ] && ENTRY_PERM=1
ENTRY_THUMBS=0
[ -d buildwsl/state/gallery ] && ENTRY_THUMBS=1
PRE_FILES=$(ls "$SHOTS" 2>/dev/null | wc -l)
echo "ENTRY-STATE: wsl-server-up=$ENTRY_UP permissions.json=$ENTRY_PERM thumbs-dir=$ENTRY_THUMBS preshot-files=$PRE_FILES"

echo "=== 1. bracketed pre-clean (살아있는 jkdesktop은 drvfs 재링크를 막는다) ==="
pkill -f 'buildwsl/[j]kdesktop' 2>/dev/null
sleep 1
pkill -9 -f 'buildwsl/[j]kdesktop' 2>/dev/null
sleep 1
LEFT=$(ps -eo args | grep -i 'buildwsl/[j]kdesktop' | wc -l)
[ "$LEFT" -eq 0 ] || FAIL "pre-clean left $LEFT jkdesktop process(es) alive"

echo "=== 2. ninja rebuild (buildwsl — 풀 로그 리다이렉트) ==="
ninja -C buildwsl -j4 >"$LOGDIR/gal_build.log" 2>&1
B_RC=${PIPESTATUS[0]}
echo "WSL-BUILD-RC=$B_RC"
[ "$B_RC" -eq 0 ] || FAIL "ninja rebuild rc=$B_RC — tail: $(tail -3 "$LOGDIR/gal_build.log" | tr '\n' ' ')"
[ -x buildwsl/jkdesktop ] || FAIL "buildwsl/jkdesktop missing after rebuild"
[ -f buildwsl/jkapp_gallery.so ] || FAIL "buildwsl/jkapp_gallery.so missing (T1-T3 배포 확인 실패)"

echo "=== 3. WSL selftest (캐논 $CANON_WSL_SELFTEST — 출력 WSL 내부 리다이렉트) ==="
timeout 420 ./buildwsl/jkdesktop test >"$LOGDIR/gal_st.log" 2>&1
ST_RC=$?
ST_FAIL=$(grep -ac '^\[FAIL\]' "$LOGDIR/gal_st.log")
ST_PASS=$(grep -ac '^\[PASS\]' "$LOGDIR/gal_st.log")
echo "selftest rc=$ST_RC PASS=$ST_PASS FAIL=$ST_FAIL"
grep -aq 'AppSelfTest: 0 failure(s)' "$LOGDIR/gal_st.log" \
    || FAIL "selftest summary missing 'AppSelfTest: 0 failure(s)'"
[ "$ST_FAIL" -eq 0 ] || FAIL "selftest FAIL=$ST_FAIL"
[ "$ST_RC" -eq 0 ] || FAIL "selftest rc=$ST_RC"
[ "$ST_PASS" -ge "$CANON_WSL_SELFTEST" ] \
    || echo "WARN: selftest PASS=$ST_PASS < 캐논 $CANON_WSL_SELFTEST (계보 이탈 — 리포트 원장 기록 필요)"
# 갤러리 라인 신설 케이스 (2g 계열 — 라인 형식 "[PASS] 2g-..."): T1-T3가 +40.
ST2G=$(grep -ac '^\[PASS\] 2g' "$LOGDIR/gal_st.log")
echo "SELFTEST-2G-CASES=$ST2G (신설 2g 계열 — 캐논 +40분의 원료)"

echo "=== 4. 합성 PNG 시드 (엔진 소유 — vplayer 계약; i:\\@keep 불접촉) ==="
cat > "$LOGDIR/gal_seed.py" <<'PYEOF'
# 시드 4장 — 극단 종횡비 포함(FitFull 2g-f 소비 확인용)·색 밴드 = 방향/종횡
# 식별 마크. stb_image가 디코드하는 최소 유효 PNG(8bit RGB) — PNG 기록 기법은
# wsl_text_scale_diag.sh의 write_png와 동형(엔진 자가 스크립트, 외부 미디어 0).
import struct, sys, zlib, os
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
            # 좌우 대비 밴드(종횡·방향 식별)+상단/하단 구별 톤
            if x < w * 0.25:      r, g, b = 200, 60, 60     # 좌=적
            elif x < w * 0.5:     r, g, b = 60, 200, 60     # 중좌=녹
            elif x < w * 0.75:    r, g, b = 60, 60, 200     # 중우=청
            else:                 r, g, b = 230, 230, 60    # 우=황
            # 수직 그라디언트 섞기 — 상=밝게, 하=어둡게(방향 핀)
            r = int(r * (1.0 - 0.5 * v)); g = int(g * (1.0 - 0.5 * v))
            b = int(b * (1.0 - 0.5 * v))
            # 좌상단 흰 시드 마크(센터 마킹용)
            if x < w * 0.06 and y < h * 0.06:
                r = g = b = 255
            raw += bytes((r, g, b))
    comp = zlib.compress(bytes(raw), 6)
    png = (b"\x89PNG\r\n\x1a\n"
           + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
           + chunk(b"IDAT", comp) + chunk(b"IEND", b""))
    with open(path, "wb") as f:
        f.write(png)
    print("SEED-OK %s %dx%d %d bytes" % (path, w, h, os.path.getsize(path)))
out = sys.argv[1]
write_png(out + "/jkg_seed_a_640x400.png", 640, 400)
write_png(out + "/jkg_seed_b_1200x120.png", 1200, 120)
write_png(out + "/jkg_seed_c_320x200.png", 320, 200)
write_png(out + "/jkg_seed_d_100x800.png", 100, 800)
PYEOF
python3 "$LOGDIR/gal_seed.py" $SHOTS || FAIL "seed PNG generation failed (python3)"
SEEDED=$(ls "$SHOTS"/jkg_seed_*.png | wc -l)
[ "$SEEDED" -eq 4 ] || FAIL "seeded $SEEDED files (need 4)"
NEW_FILES=$(ls "$SHOTS" 2>/dev/null | wc -l)
echo "GRID-INPUT-FILES=$NEW_FILES (pre-existing $PRE_FILES + seeded 4)"
# 격자 열거 순서(mtime 내림 = ListImageFiles 계약) — 썸네일 갯수 실측 원료.
echo "GRID-ORDER(newest first):"
ls -t "$SHOTS" | head -12 | sed 's/^/  /'

echo "=== 5. send_input 승인 무승인 운용 — permissions.json 1행 (ENTRY 원복) ==="
[ "$ENTRY_PERM" -eq 0 ] || FAIL "buildwsl/permissions.json already exists — probe는 런타임 permissions 파일을 건드릴 수 없다(전면 allow 파일은 프로브 소유 아님)"
printf '{\n    "send_input": "allow"\n}\n' > buildwsl/permissions.json
[ -f buildwsl/permissions.json ] || FAIL "permissions.json write failed"
echo "PERM-WIRE-OK: buildwsl/permissions.json (send_input allow — END에서 소각)"

echo "=== 6. 서버 부팅 (setsid — taskbar 자동 스폰 선례) ==="
pkill -f 'buildwsl/[j]kdesktop' 2>/dev/null
sleep 1
pkill -9 -f 'buildwsl/[j]kdesktop' 2>/dev/null
sleep 1
rm -f /tmp/JKWindowServerPipe.sock
LOG="$LOGDIR/gal_srv.log"
env DISPLAY=:0 setsid nohup ./buildwsl/jkdesktop --server >"$LOG" 2>&1 &
sleep 6
SRV_PID=$(pgrep -f 'buildwsl/[j]kdesktop --server' | head -1)
[ -n "$SRV_PID" ] || FAIL "no server 6s after boot — log tail: $(tail -3 "$LOG" | tr '\n' ' ')"
echo "server pid=$SRV_PID"
PING=""
for i in 1 2 3 4 5; do
    PING=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"ping","args":{}}' 2>/dev/null | grep -a '{' | head -1)
    printf '%s' "$PING" | grep -aq '"ok":true' && break
    sleep 2
done
printf '%s' "$PING" | grep -aq '"ok":true' || FAIL "ping failed — $PING"

echo "=== 7. launch_app gallery (격자 부팅 — 썸네일 찍힘 대기 4s) ==="
L=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"launch_app","args":{"app":"gallery"}}' 2>/dev/null | grep -a '{' | head -1)
printf '%s' "$L" | grep -aq '"ok":true' || FAIL "launch_app gallery failed — $L"
echo "launch reply: $L"
GALWIN=""
WIN=""
for i in 1 2 3 4 5 6 7 8; do
    sleep 2
    WIN=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)
    GALWIN=$(printf '%s' "$WIN" | grep -aoE '\{"id":[^}]*"title":"Gallery"[^}]*\}' | head -1)
    [ -n "$GALWIN" ] && break
done
[ -n "$GALWIN" ] || FAIL "no Gallery window in list_windows — last: ${WIN:-none}"
echo "GALWIN: $GALWIN"
GAL_ID=$(printf '%s' "$GALWIN" | sed -n 's/.*"id":\([0-9]*\),.*/\1/p')
GAL_X=$(printf '%s' "$GALWIN" | sed -n 's/.*"x":\(-\?[0-9]*\),.*/\1/p')
GAL_Y=$(printf '%s' "$GALWIN" | sed -n 's/.*"y":\(-\?[0-9]*\),.*/\1/p')
GAL_W=$(printf '%s' "$GALWIN" | sed -n 's/.*"w":\([0-9]*\),.*/\1/p')
GAL_H=$(printf '%s' "$GALWIN" | sed -n 's/.*"h":\([0-9]*\),.*/\1/p')
echo "GAL-GEO: id=$GAL_ID x=$GAL_X y=$GAL_Y w=$GAL_W h=$GAL_H (meta 560x520 상대 검증)"
sleep 4   # 썸네일 디코드+캐시 기록 안정화

# 썸네일 파이프라인 실측 영수증 — 디스크 캐시 파일 수 = 실제 디코드가 끝난 셀.
THUMB_CNT=$(ls buildwsl/state/gallery/thumbs 2>/dev/null | wc -l)
echo "THUMB-CACHE-CNT=$THUMB_CNT (grid-input-files=$NEW_FILES — 부족분=디코드 실패/컬)"

echo "=== 8. 캡처기 자가 생성+격자 캡처 → gal_wsl_grid.png ==="
cat > "$LOGDIR/gal_shot.py" <<'PYEOF'
# wsl_text_scale_diag.sh의 tsd_shot.py 축본 — X11 창 단위 캡처+최소 PNG 기록기.
import ctypes, struct, sys, zlib
c_void_p = ctypes.c_void_p
X11 = ctypes.CDLL("libX11.so.6")
X11.XOpenDisplay.restype = c_void_p
X11.XOpenDisplay.argtypes = [ctypes.c_char_p]
X11.XCloseDisplay.argtypes = [c_void_p]
X11.XDefaultRootWindow.restype = ctypes.c_ulong
X11.XGetGeometry.restype = ctypes.c_int
X11.XFetchName.argtypes = [c_void_p, ctypes.c_ulong, c_void_p]
X11.XFree.argtypes = [c_void_p]
X11.XSetErrorHandler.restype = c_void_p
X11.XSetErrorHandler.argtypes = [c_void_p]
X11.XQueryTree.restype = ctypes.c_int
X11.XGetImage.restype = c_void_p
X11.XGetImage.argtypes = [c_void_p, ctypes.c_ulong, ctypes.c_int, ctypes.c_int,
                          ctypes.c_uint, ctypes.c_uint, ctypes.c_ulong,
                          ctypes.c_int]
X11.XDestroyImage.argtypes = [c_void_p]
X11.XSync.argtypes = [c_void_p, ctypes.c_int]
ZPIXMAP = 2
ALL_PLANES = 0xFFFFFFFF
GOT_ERROR = {"n": 0}
# XImage 구조체 오프셋(x86_64 — 1차 실측 트랩 원장: tsd 선례의 48/56/64는
# bits_per_pixel을 red 마스크로 읽는 3틀림 — 캡처 색이 자유로이 거짓됐다.
# 40=depth·44=bytes_per_line·48=bits_per_pixel·56/64/72=R/G/B 마스크.)
OFF_DATA = 16
OFF_DEPTH, OFF_BPL, OFF_BPP = 40, 44, 48
OFF_RED, OFF_GREEN, OFF_BLUE = 56, 64, 72

def err_handler(dpy, ev):
    GOT_ERROR["n"] += 1
    return 0

def write_png(path, w, h, rgb):
    raw = b"".join(b"\x00" + rgb[y * w * 3:(y + 1) * w * 3] for y in range(h))
    comp = zlib.compress(raw, 6)
    def chunk(tag, data):
        return (struct.pack(">I", len(data)) + tag + data
                + struct.pack(">I", zlib.crc32(tag + data)))
    png = (b"\x89PNG\r\n\x1a\n"
           + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
           + chunk(b"IDAT", comp)
           + chunk(b"IEND", b""))
    with open(path, "wb") as f:
        f.write(png)

def main():
    out = sys.argv[1]
    d = X11.XOpenDisplay(None)
    if not d:
        print("XSHOT-FAIL: cannot open display")
        return 1
    try:
        H = ctypes.CFUNCTYPE(ctypes.c_int, c_void_p, c_void_p)(err_handler)
        X11.XSetErrorHandler(H)
        root = X11.XDefaultRootWindow(d)
        rr = ctypes.c_ulong(); pw = ctypes.c_ulong()
        kids = c_void_p(); n = ctypes.c_uint()
        if not X11.XQueryTree(d, root, ctypes.byref(rr), ctypes.byref(pw),
                              ctypes.byref(kids), ctypes.byref(n)) or not kids:
            print("XSHOT-FAIL: XQueryTree failed")
            return 1
        arr = ctypes.cast(kids, ctypes.POINTER(ctypes.c_ulong))
        cands = []
        for i in range(n.value):
            w = arr[i]
            x, y, gw, gh, bw, bd = [ctypes.c_int() for _ in range(6)]
            rw = ctypes.c_ulong()
            if not X11.XGetGeometry(d, w, ctypes.byref(rw), ctypes.byref(x),
                                    ctypes.byref(y), ctypes.byref(gw),
                                    ctypes.byref(gh), ctypes.byref(bw),
                                    ctypes.byref(bd)):
                continue
            name = ctypes.c_char_p(None)
            nm = X11.XFetchName(d, w, ctypes.byref(name))
            title = name.value.decode("utf-8", "replace") if nm and name.value else ""
            if nm:
                X11.XFree(name)
            print("XSHOT cand: win=0x%x %dx%d title=%r" % (w, gw.value, gh.value, title))
            cands.append((w, title, gw.value, gh.value))
        X11.XFree(kids)
        pick = None
        # WSL 서버 데스크톱 합성 창(전체 뷰 — 폰 x11grab 프레임과 동형). XWayland
        # 배경 창 8192x8192 배제 상한 선례 원문(wsl_text_scale_diag 동형).
        big = [(w, gw, gh) for w, t, gw, gh in cands
               if 500 <= gw <= 4096 and 400 <= gh <= 4096]
        if big:
            big.sort(key=lambda t: -(t[1] * t[2]))
            pick = big[0]
        if pick is None:
            print("XSHOT-FAIL: no candidate window")
            return 1
        w, gw, gh = pick
        X11.XSync(d, False)
        got0 = GOT_ERROR["n"]
        img = X11.XGetImage(d, w, 0, 0, gw, gh, ALL_PLANES, ZPIXMAP)
        X11.XSync(d, False)
        if not img or GOT_ERROR["n"] > got0:
            print("XSHOT-FAIL: XGetImage failed (BadMatch?)")
            return 1
        depth = ctypes.c_int32.from_address(img + OFF_DEPTH).value
        bpl = ctypes.c_int32.from_address(img + OFF_BPL).value
        bpp = ctypes.c_int32.from_address(img + OFF_BPP).value
        data_ptr = c_void_p.from_address(img + OFF_DATA).value
        red = ctypes.c_uint64.from_address(img + OFF_RED).value
        green = ctypes.c_uint64.from_address(img + OFF_GREEN).value
        blue = ctypes.c_uint64.from_address(img + OFF_BLUE).value
        print("XSHOT hit: win=0x%x depth=%d %dx%d bpp=%d masks r=%#x g=%#x b=%#x"
              % (w, depth, gw, gh, bpp, red, green, blue))
        bpp_bytes = (bpp + 7) // 8
        if bpp_bytes not in (3, 4):
            print("XSHOT-FAIL: unsupported bpp=%d" % bpp)
            return 1
        if not (red and green and blue):
            print("XSHOT-FAIL: empty channel mask(s) r=%#x g=%#x b=%#x"
                  % (red, green, blue))
            return 1

        def chan(v, mask):  # 마스크 비트폭 무관 확장(2차 트랩 원장: 폭=bit_length
            # 24(0xff0000) — 3틀림 유발. 폭=popcount(연속 마스크 가정) 8이 정답.
            lsb = (mask & -mask).bit_length() - 1
            span = bin(mask).count("1")
            val = (v >> lsb) & ((1 << span) - 1)
            return (val * 255) // ((1 << span) - 1) & 0xFF
        raw = ctypes.string_at(data_ptr, bpl * gh)
        rgb = bytearray(gw * gh * 3)
        for yy in range(gh):
            row = yy * bpl
            for xx in range(gw):
                off = row + xx * bpp_bytes
                v = int.from_bytes(raw[off:off + bpp_bytes], "little")
                o = (yy * gw + xx) * 3
                rgb[o] = chan(v, red)
                rgb[o + 1] = chan(v, green)
                rgb[o + 2] = chan(v, blue)
        X11.XDestroyImage(img)
        write_png(out, gw, gh, bytes(rgb))
        print("XSHOT wrote %s" % out)
        return 0
    finally:
        X11.XCloseDisplay(d)

if __name__ == "__main__":
    sys.exit(main())
PYEOF
[ -f "$LOGDIR/gal_shot.py" ] || FAIL "capture script generation failed"

RECEIVE=/mnt/i/progwork/JKENGINE/engine/tmp
mkdir -p "$RECEIVE"
shot() { # $1 = 영수증 상대명 (gal_wsl_grid.png) — /tmp 단열 → engine/tmp 단일 cp
    local NAME="$1" TMP="/tmp/gal_shot_cur.png" SLOG="/tmp/gal_shot_cur.log"
    rm -f "$TMP"
    python3 "$LOGDIR/gal_shot.py" "$TMP" >"$SLOG" 2>&1 \
        || FAIL "capture $NAME failed — $(tail -2 "$SLOG" | tr '\n' ' ')"
    grep -aq 'XSHOT wrote' "$SLOG" \
        || FAIL "capture $NAME: no XSHOT wrote — $(tail -2 "$SLOG" | tr '\n' ' ')"
    grep -a 'XSHOT hit\|XSHOT cand' "$SLOG" | sed 's/^/  /'
    cp "$TMP" "$RECEIVE/$NAME" || FAIL "capture copy $NAME failed"
    rm -f "$TMP"
    echo "CAPTURE: $RECEIVE/$NAME ($(wc -c < "$RECEIVE/$NAME") bytes)"
}

shot gal_wsl_grid.png

echo "=== 9. 셀 0 클릭 → 전체 보기 → gal_wsl_full.png ==="
CLICK_Y=$((GAL_Y + 160))
CLICK_X=$((GAL_X + 60))
echo "CLICK-CELL-0: id=$GAL_ID desk=($CLICK_X,$CLICK_Y) — 계산: 창좌표(60,160)=셀0 박스(160x120+라벨) 중심부"
CL=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"send_input","args":{"op":"click","id":'"$GAL_ID"',"x":'"$CLICK_X"',"y":'"$CLICK_Y"',"button":1,"clicks":1}}' 2>/dev/null | grep -a '{' | head -1)
printf '%s' "$CL" | grep -aq '"ok":true' \
    || FAIL "send_input click gate/execute failed — reply: $CL (permissions.json send_input allow 확인)"
echo "click reply: $CL"
sleep 3   # 전체 보기 디코드+텍스처 업로드 안정화
shot gal_wsl_full.png

echo "=== 10. 이전(← 꺾임 — WrapStep 0에서 -1 = 끝 지점) → gal_wsl_prev.png ==="
KV=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"send_input","args":{"op":"key","id":'"$GAL_ID"',"key":1073741904,"mods":0}}' 2>/dev/null | grep -a '{' | head -1)
printf '%s' "$KV" | grep -aq '"ok":true' || FAIL "send_input key LEFT failed — $KV"
echo "key LEFT reply: $KV"
sleep 3
shot gal_wsl_prev.png

echo "=== 11. Esc 격자 복귀 → gal_wsl_back.png ==="
KE=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"send_input","args":{"op":"key","id":'"$GAL_ID"',"key":27,"mods":0}}' 2>/dev/null | grep -a '{' | head -1)
printf '%s' "$KE" | grep -aq '"ok":true' || FAIL "send_input key ESC failed — $KE"
echo "key ESC reply: $KE"
sleep 3
shot gal_wsl_back.png

echo "=== 12. 복원 — 서버 DOWN·시드 소각·캐시 dir 소각·permissions 소각 ==="
pkill -f 'buildwsl/[j]kdesktop' 2>/dev/null
sleep 1
pkill -9 -f 'buildwsl/[j]kdesktop' 2>/dev/null
sleep 1
LEFT=$(ps -eo args | grep -i 'buildwsl/[j]kdesktop' | wc -l)
[ "$LEFT" -eq 0 ] || echo "WARN: $LEFT jkdesktop process(es) survived teardown"
rm -f "$SHOTS"/jkg_seed_*.png
[ -n "$(ls "$SHOTS"/jkg_seed_*.png 2>/dev/null)" ] && FAIL "seed 소각 실패 — 수동 소각 필요" || echo "SEED-BURIED: 4 seed PNG removed"
if [ "$ENTRY_THUMBS" -eq 0 ]; then
    rm -rf buildwsl/state/gallery
    [ -d buildwsl/state/gallery ] && echo "WARN: state/gallery survived rm (drvfs 지연 링크?)" \
        || echo "THUMB-CACHE-BURIED: buildwsl/state/gallery removed"
else
    echo "THUMB-CACHE-KEPT: ENTRY에 존재했던 dir — 건드리지 않는다 (probe 소유 잔상 아님)"
fi
if [ "$ENTRY_PERM" -eq 0 ]; then
    rm -f buildwsl/permissions.json
    echo "PERM-BURIED: permissions.json removed (ENTRY 값 1행 기록 복기)"
else
    echo "WARN: permissions.json pre-existed — 건드리지 않았어야 한다(도입부 FAIL과 모순)"
fi
if [ "$ENTRY_UP" = "1" ]; then
    env DISPLAY=:0 setsid nohup ./buildwsl/jkdesktop --server >"$LOGDIR/gal_srv_restore.log" 2>&1 &
    sleep 6
    pgrep -f 'buildwsl/[j]kdesktop --server' >/dev/null 2>&1 || FAIL "restore boot failed"
    echo "END-STATE: WSL 서버 UP 재부팅 — 진입 상태 원복 완료"
else
    echo "END-STATE: WSL 서버 DOWN — 진입 상태 원복 완료"
fi

echo "=== 13. REMNANT — /tmp 자가 스크립트·중간파일 소각 ==="
LEFTS=$(ls "$LOGDIR" 2>/dev/null | wc -l)
REMNANT_LIST=$(ls "$LOGDIR")
echo "REMNANT-COUNT=$LEFTS (서버·빌드·셀프테스트·shot 로그만 잔존 — 원장 수형)"
echo "$REMNANT_LIST" | sed 's/^/  /'

echo "=== 14. 갤러리 실측 판정 ==="
CAP_OK=1
for f in grid full prev back; do
    P="$RECEIVE/gal_wsl_$f.png"
    if [ ! -s "$P" ]; then
        echo "GAL-FAIL(capture missing/empty: $P)"
        CAP_OK=0
    fi
done
THUMB_OK=1
[ "$THUMB_CNT" -ge 4 ] || { echo "GAL-FAIL(thumb cache $THUMB_CNT < 4 — 시드 셀이 썸네일을 놓쳤다)"; THUMB_OK=0; }
if [ "$CAP_OK" -eq 1 ] && [ "$THUMB_OK" -eq 1 ]; then
    echo "GAL-VERDICT: GAL-OK(WSL 빌드 rc=$B_RC·selftest FAIL=$ST_FAIL 계보 PASS=$ST_PASS(캐논 $CANON_WSL_SELFTEST)·thumb-cache=$THUMB_CNT·capture 4종 receipt — 육안 스탭 대기)"
else
    echo "GAL-VERDICT: GAL-FAIL(행별 사유는 위 각 행 — 캡처/썸네일 실측 미달, 인프라 실패는 hard FAIL로 상단 중단)"
fi
echo "GAL-WCAPTURES:"
for f in grid full prev back; do
    P="$RECEIVE/gal_wsl_$f.png"
    [ -s "$P" ] && echo "  $P ($(wc -c < "$P") bytes)"
done
echo "GALLERY-END"
exit 0
# honest-fail 원칙: 수치·육안 대행 미달(GAL-FAIL 라인)은 원장 목적이라 rc=0.
# hard FAIL(가드·빌드·selftest·시드·부팅·ping·launch·창·캡처·복원 실패)만 exit 1.
# EOF — 끝 개행 유지.
