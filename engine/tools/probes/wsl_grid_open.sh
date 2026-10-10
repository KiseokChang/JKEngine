#!/usr/bin/env bash
# WSL 런처 그리드 posix 개방 probe (#95 — 스펙 2026-10-10-grid-open-plan §probe).
# 배경: JKDesktopShell.cpp ScanJkxApps/ScanConsoleApps가 _WIN32 전용 — posix 축
# (폰/WSL) 그리드가 내장 4셀(minesweeper/tetris/lf/hx 폴백)만 점화해 설치
# .jkx(갤러리/뮤직)가 안 떴다(사용자 라이브 보고 "뮤직 아이콘 부터가 안보여요").
# posix leg 개방의 WSL 실측 세그먼트: ① 스캔 stderr 원문(jkx 행 등장) ② 그리드
# 캡처 ③ 그리드 더블클릭이 SpawnClient(--jkx) 전파되는지 원문(서버 spawn 행).
#
# 표준 승계(wsl_text_scale_diag.sh/wsl_gallery.sh 원문): WSLg :0, setsid 부팅,
# 브래킷 pkill+(-9 에스컬레이션), stale 소켓 rm, 창 단위 X11 캡처(python
# ctypes — 3틀림 교정본), permissions.json은 ENTRY 기록+END 소각(probe 소유
# 1행만), selftest는 별도 세그먼트(본 probe는 부팅 스캔 실측이 목적).
#
# 합성 더블클릭 경로: ExecuteSendInputOp는 셸을 대상에서 제외(JKWindowServer.cpp
# :2105 주석 — "셸은 대상에서 제외")라 send_input으로 그리드 접촉 불가 →
# XTEST(libXtst) 합성 경로만 실측 가능(WSLg XWayland root 절대 좌표).
#
# 실행 표준(WSL 밖):
#   MSYS_NO_PATHCONV=1 wsl.exe -d Ubuntu-24.04 -- bash /mnt/i/progwork/JKENGINE/engine/tools/probes/wsl_grid_open.sh
# 폰 축은 본 배치에서 보류(컨트롤러 사후 — 폰 ssh 점유 경합 방지).
set -u
cd /mnt/i/progwork/JKENGINE/engine

FAIL() { echo "GRID-OPEN-FAIL: $*"; exit 1; }
LOGDIR=/tmp/gridopen
mkdir -p "$LOGDIR"
RECEIVE=/mnt/i/progwork/JKENGINE/engine/tmp
SRVLOG="$LOGDIR/srv.log"

# ---------------------------------------------------------------- fail-closed 가드
# (phone_text_scale.sh T4 fix r1 "가드를 exec 앞으로" 원문 승계 — 출력 유도 앞.)
if [ -z "${WSL_DISTRO_NAME:-}" ]; then
  echo "GRID-OPEN-WFAIL: not inside WSL — 실행 축 계약 위반"
  exit 1
fi

ENTRY_UP=0
pgrep -f 'buildwsl/[j]kdesktop' >/dev/null 2>&1 && ENTRY_UP=1
ENTRY_PERM=0
[ -f buildwsl/permissions.json ] && ENTRY_PERM=1
echo "ENTRY-STATE: wsl-server-up=$ENTRY_UP permissions.json=$ENTRY_PERM"

echo "=== 0. 잡석 정리 — 캡처·더블클릭 주입기를 /tmp에 자가 생성 ==="
cat > "$LOGDIR/grid_shot.py" <<'PYEOF'
# 창 단위 X11 캡처+최소 PNG 기록기 — wsl_text_scale_diag.sh tsd_shot.py 원문 승계
# (3틀림 교정본 오프셋: 40=depth·44=bytes_per_line·48=bits_per_pixel·56/64/72=RGB).
import ctypes, struct, sys, zlib
c_void_p = ctypes.c_void_p
X11 = ctypes.CDLL("libX11.so.6")
X11.XOpenDisplay.restype = c_void_p
X11.XOpenDisplay.argtypes = [c_void_p]
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
OFF_WIDTH, OFF_HEIGHT, OFF_DATA = 0, 4, 16  # 원본 tsd_shot.py 동일 — data=16
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

def big_window():
    # 서버 데스크톱 창(전체 합성 뷰) 선택 — 캡처기 원문 승계(8192 XWayland
    # 배경 창 배제 상한 4096, SDL WM_NAME 빈 문자열 관용).
    d = X11.XOpenDisplay(None)
    if not d:
        return None
    try:
        H = ctypes.CFUNCTYPE(ctypes.c_int, c_void_p, c_void_p)(err_handler)
        X11.XSetErrorHandler(H)
        root = X11.XDefaultRootWindow(d)
        rr = ctypes.c_ulong(); pw = ctypes.c_ulong()
        kids = c_void_p(); n = ctypes.c_uint()
        if not X11.XQueryTree(d, root, ctypes.byref(rr), ctypes.byref(pw),
                              ctypes.byref(kids), ctypes.byref(n)) or not kids:
            return None
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
            if 500 <= gw.value <= 4096 and 400 <= gh.value <= 4096:
                cands.append((gw.value * gh.value, w, x.value, y.value, gw.value, gh.value))
        X11.XFree(kids)
        if not cands:
            return None
        cands.sort(reverse=True)
        return cands[0][1:]
    finally:
        X11.XCloseDisplay(d)

def main():
    out = sys.argv[1]
    pick = big_window()
    if pick is None:
        print("XSHOT-FAIL: no candidate window")
        return 1
    w, wx, wy, gw, gh = pick
    d = X11.XOpenDisplay(None)
    if not d:
        print("XSHOT-FAIL: cannot open display")
        return 1
    try:
        H = ctypes.CFUNCTYPE(ctypes.c_int, c_void_p, c_void_p)(err_handler)
        X11.XSetErrorHandler(H)
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
            print("XSHOT-FAIL: empty channel mask(s)")
            return 1

        def chan(v, mask):
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
        print("XSHOT wrote %s win-geo=(%d,%d %dx%d)" % (out, wx, wy, gw, gh))
        return 0
    finally:
        X11.XCloseDisplay(d)

if __name__ == "__main__":
    sys.exit(main())
PYEOF
cat > "$LOGDIR/grid_dblclick.py" <<'PYEOF'
# XTEST 합성 더블클릭 — 그리드 셀 0 논리 (75,75) 근방. 셸은 send_input 제외
# (ExecuteSendInputOp 주석)라 본 XTEST 경로만 셸 접촉이 산다. root 절대 좌표 =
# 캡처기가 기록한 win-geo + 논리 오프셋(WSLg scale=1 실측 — 오프셋은 서면 px).
import ctypes, struct, sys, time
X11 = ctypes.CDLL("libX11.so.6")
XTST = ctypes.CDLL("libXtst.so.6")
X11.XOpenDisplay.restype = ctypes.c_void_p
X11.XOpenDisplay.argtypes = [ctypes.c_char_p]
X11.XCloseDisplay.argtypes = [ctypes.c_void_p]
XTST.XTestFakeButtonEvent.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_int,
                                      ctypes.c_ulong]
XTST.XTestFakeMotionEvent.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_int,
                                      ctypes.c_int, ctypes.c_ulong]
XTST.XTestFlush.argtypes = [ctypes.c_void_p]

def main():
    ax, ay = int(sys.argv[1]), int(sys.argv[2])
    d = X11.XOpenDisplay(None)
    if not d:
        print("DBL-FAIL: cannot open display")
        return 1
    try:
        XTST.XTestFakeMotionEvent(d, -1, ax, ay, 0)
        for _ in range(2):
            XTST.XTestFakeButtonEvent(d, 1, 1, 0)
            time.sleep(0.02)
            XTST.XTestFakeButtonEvent(d, 1, 0, 0)
            time.sleep(0.12)
        XTST.XTestFlush(d)
        print("DBL-OK: root(%d,%d) x" % (ax, ay))
        return 0
    finally:
        X11.XCloseDisplay(d)

if __name__ == "__main__":
    sys.exit(main())
PYEOF
[ -f "$LOGDIR/grid_shot.py" ] && [ -f "$LOGDIR/grid_dblclick.py" ] \
    || FAIL "capture/injector script generation failed"

echo "=== 1. bracketed pre-clean ==="
pkill -f 'buildwsl/[j]kdesktop' 2>/dev/null
sleep 1
pkill -9 -f 'buildwsl/[j]kdesktop' 2>/dev/null
sleep 1
LEFT=$(ps -eo args | grep 'buildwsl/[j]kdesktop' | wc -l)
[ "$LEFT" -eq 0 ] || FAIL "pre-clean left $LEFT jkdesktop process(es) alive"

echo "=== 2. ninja 빌드 현행화 (idempotent) ==="
ninja -C buildwsl -j3 >"$LOGDIR/build.log" 2>&1
B_RC=${PIPESTATUS[0]}
echo "WSL-BUILD-RC=$B_RC"
[ "$B_RC" -eq 0 ] || FAIL "ninja build rc=$B_RC — $(tail -4 "$LOGDIR/build.log" | tr '\n' ' ')"
[ -x buildwsl/jkdesktop ] || FAIL "buildwsl/jkdesktop missing"

echo "=== 3. boot server (setsid nohup detached, WSLg :0) ==="
rm -f /tmp/JKWindowServerPipe.sock
env DISPLAY=:0 setsid nohup ./buildwsl/jkdesktop --server >"$SRVLOG" 2>&1 &
sleep 6
[ -n "$(pgrep -f 'buildwsl/[j]kdesktop --server' | head -1)" ] \
    || FAIL "no server 6s after boot — log tail: $(tail -5 "$SRVLOG" | tr '\n' ' ')"
PING=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"ping","args":{}}' 2>/dev/null | grep -a '{' | head -1)
printf '%s' "$PING" | grep -aq '"ok":true' || FAIL "ping failed — $PING"
sleep 3   # Init 스캔 로그 플러시 안정화

echo "=== 4. 스캔 원문 — installed app 행(jkx) + console app 행 ==="
JKX_LINES=$(grep -ac "installed app " "$SRVLOG")
CONS_LINES=$(grep -ac "console app " "$SRVLOG")
echo "SCAN-JKX-ROWS=$JKX_LINES SCAN-CONSOLE-ROWS=$CONS_LINES"
[ "$JKX_LINES" -ge 1 ] || FAIL "no 'installed app' scan rows — posix leg 미개방(원장 갭 재현)"
grep -a "installed app " "$SRVLOG"
grep -a "console app " "$SRVLOG" || echo "(no console app rows)"

echo "--- 갤러리/뮤직 원문 행(이번 수리의 표적 행):"
GAL_ROW=$(grep -a "installed app 'gallery'" "$SRVLOG" | head -1)
MUS_ROW=$(grep -a "installed app 'music'" "$SRVLOG" | head -1)
[ -n "$GAL_ROW" ] || FAIL "no scan row for 'gallery' — posix leg 확산 불충분"
[ -n "$MUS_ROW" ] || FAIL "no scan row for 'music' — posix leg 확산 불충분"
echo "GAL-ROW: $GAL_ROW"
echo "MUS-ROW: $MUS_ROW"

echo "=== 5. 더블클릭 표적 셀 식별 (스캔 로그 순서 = 점화 순서 — 표적=settings) ==="
# 셀 0은 legacy trigger .jkx(drvfs 디코드 이름 — 아트 부재·모듈 실패 여지)라
# 실측 표적은 모듈이 확실히 산(buildwsl/jkapp_settings.so) settings 셀로.
CELL_TARGET="settings"
CELL_ORD=$(( $(grep -a "installed app " "$SRVLOG" | grep -an "installed app '$CELL_TARGET'" | head -1 | cut -d: -f1) - 1 ))
[ -n "$CELL_ORD" ] || FAIL "cannot find '$CELL_TARGET' scan row (cell ordinal)"
echo "CELL-TARGET: $CELL_TARGET ord=$CELL_ORD (0-based — grep 행 1-based에서 1 차감)"

echo "=== 6. 그리드 캡처 → engine/tmp/grid_open_wsl.png ==="
rm -f "$RECEIVE/grid_open_wsl.png"
python3 "$LOGDIR/grid_shot.py" /tmp/grid_cap.png >"$LOGDIR/shot.log" 2>&1 \
    || FAIL "capture failed — $(tail -2 "$LOGDIR/shot.log" | tr '\n' ' ')"
grep -a "XSHOT" "$LOGDIR/shot.log"
cp /tmp/grid_cap.png "$RECEIVE/grid_open_wsl.png" || FAIL "capture copy failed"
cp /tmp/grid_cap.png "$LOGDIR/grid_cap.png" || FAIL "picker input copy failed"
rm -f /tmp/grid_cap.png
echo "CAPTURE: $RECEIVE/grid_open_wsl.png ($(wc -c < "$RECEIVE/grid_open_wsl.png") bytes)"

cat > "$LOGDIR/grid_pick.py" <<'PYEOF'
# 그리드 셀 픽커 — 캡처 PNG에서 1행 아이콘 블롭 중심을 검출해 root 절대 좌표를
# 계산한다(scale·레터박스·타이틀바 함정 회피: 캡처 이미지 원점 = X 창 원점이므로
# 이미지 px + XGetGeometry (x,y)가 곧 root 좌표).
import ctypes, struct, sys, zlib

def read_png_rgb(path):
    raw = open(path, "rb").read()
    assert raw[:8] == b"\x89PNG\r\n\x1a\n", "not a png"
    pos = 8
    w = h = None
    idat = b""
    while pos < len(raw):
        ln = struct.unpack(">I", raw[pos:pos + 4])[0]
        tag = raw[pos + 4:pos + 8]
        data = raw[pos + 8:pos + 8 + ln]
        if tag == b"IHDR":
            w, h, bit, ct = struct.unpack(">IIBB", data[:10])[:4] if False else (
                struct.unpack(">I", data[0:4])[0], struct.unpack(">I", data[4:8])[0],
                data[8], data[9])
        elif tag == b"IDAT":
            idat += data
        elif tag == b"IEND":
            break
        pos += 12 + ln
    ch = {0: 1, 2: 3, 6: 4}[ct]
    stride = w * ch
    flat = zlib.decompress(idat)
    rows = []
    off = 0
    for _ in range(h):
        assert flat[off] == 0, "unexpected filter %d" % flat[off]  # 본 probe 기록기는 filter 0
        row = flat[off + 1:off + 1 + stride]
        rows.append(row)
        off += 1 + stride
    return w, h, ch, rows

def main():
    path = sys.argv[1]
    want = int(sys.argv[2])
    w, h, ch, rows = read_png_rgb(path)
    def dark(x, y):
        row = rows[y]
        o = x * ch
        return max(row[o], row[o + 1], row[o + 2]) < 70
    # 1행 밴드: 어두운 픽셀이 두꺼운 y 띄 (아이콘 아트 64논리 ≈ 밴드). 스캔 y=60..400.
    counts = {y: sum(1 for x in range(40, w - 40) if dark(x, y)) for y in range(60, min(400, h))}
    band = [y for y in sorted(counts) if counts[y] > 200]
    if not band:
        print("PICK-FAIL: no dark band in top rows")
        return 1
    y0 = band[0]
    yend = y0
    for y in band:
        if y - yend <= 3:
            yend = y
        else:
            break
    yc = (y0 + yend) // 2
    # 열 런 추출 — 갭 병합 ≤25px(아이콘 내부의 밝은 아트 글리프가 런을 자르는
    # 실측: F/N/S/P 전면 아트 셀은 어두운 배경이 아니다 — 윈도 프레임 레터박스
    # 구조라 오히려 배경 밝기 요동. 실측 본사 검증: 병합 후 12셀 중 7 런 —
    # 폭·위치가 정확히 100px 피치 정합).
    runs = []
    start = None
    lastdark = None
    for x in range(10, w - 10):
        if dark(x, yc):
            if start is None:
                start = x
            lastdark = x
        elif start is not None and x - lastdark > 25:
            runs.append((start, lastdark))
            start = None
    if start is not None:
        runs.append((start, lastdark))
    runs = [(a, b) for a, b in runs if 40 <= b - a <= 95]
    centers = [(a + b) // 2 for a, b in runs]
    print("PICK: band y=%d..%d yc=%d row0-blobs=%d centers=%s"
          % (y0, yend, yc, len(centers), centers))
    if not centers:
        print("PICK-FAIL: no blobs detected")
        return 1
    # 앵커=첫 런(셀 0) + 피치=러 consecutive 최소차(실측 100 — 논리 피치 1:1).
    pitch = min((centers[i + 1] - centers[i] for i in range(len(centers) - 1)),
                key=lambda d: d if d > 0 else 1 << 30)
    if pitch < 90:
        print("PICK-FAIL: implausible pitch=%d" % pitch)
        return 1
    cx = centers[0] + want * pitch
    print("PICK-CELL: %d %d (anchor=%d pitch=%d)" % (cx, yc, centers[0], pitch))
    return 0

if __name__ == "__main__":
    sys.exit(main())
PYEOF
[ -f "$LOGDIR/grid_pick.py" ] || FAIL "cell picker generation failed"

echo "=== 7. 더블클릭 → SpawnClient --jkx 전파 원문 ==="
echo "--- permissions.json 1행 (ENTRY 원복 — END에서 소각; send_input 불요라도"
echo "--- 본 probe는 셀 좌표 유니폼 표기를 위해 파일만 기록하지 않는다 — 스킵)"
# XTEST 경로는 agent 승인 가동층 불요 — permissions.json 건드리지 않음(본
# probe는 파일을 소유하지 않는다 — wsl_gallery.sh 존중 계약 그대로).
GEO_LINE=$(grep -a 'win-geo' "$LOGDIR/shot.log" | head -1)
[ -n "$GEO_LINE" ] || FAIL "no win-geo line in shot log — capture did not record window geometry"
echo "GEO-LINE: $GEO_LINE"
# 형식: "... win-geo=(WX,WY GWxGH)" — bash regex 1회 파스(sed 백트래킹 함정:
# "\([0-9]*\). .*"가 148을 14로 자르는 실측 — fix r1).
if [[ "$GEO_LINE" =~ win-geo=\(([0-9]+),([0-9]+)\ ([0-9]+)x([0-9]+)\) ]]; then
    WX=${BASH_REMATCH[1]}
    WY=${BASH_REMATCH[2]}
    GW=${BASH_REMATCH[3]}
    GH=${BASH_REMATCH[4]}
else
    FAIL "win-geo parse failed — line: $GEO_LINE"
fi
# 그리드 계약(JKDesktopShell.cpp RelayoutLauncherIcons): 열 수=(논리 폭-50)/100
# — 논리 폭은 window_frame dw(id=0 desktop wide view의 dw/dh 논리 데스크톱).
# 실측(1차 실행): X 서면 1356x817 — 창 크기=논리 가정은 틀렸다(스케일 ≠1).
# 논리 축비는 dw/dh에서 서면비와 분리해 산출해 클릭 좌표를 보정한다.
DW_FRAME=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"window_frame","args":{"id":0}}' 2>/dev/null | grep -a '{' | head -1)
DW=$(printf '%s' "$DW_FRAME" | grep -aoE '"dw":[0-9]+' | grep -ao '[0-9]*')
DH=$(printf '%s' "$DW_FRAME" | grep -aoE '"dh":[0-9]+' | grep -ao '[0-9]*')
[ -n "$DW" ] && [ -n "$DH" ] || FAIL "window_frame reply has no dw/dh — $DW_FRAME"
COLS=$(( (DW - 50) / 100 ))
[ "$COLS" -ge 1 ] || FAIL "grid cols=$COLS from dw=$DW — 계약 위반"
TARGET_COL=$(( CELL_ORD % COLS )); TARGET_ROW=$(( CELL_ORD / COLS ))
LX=$(( 50 + TARGET_COL * 100 + 32 )); LY=$(( 50 + TARGET_ROW * 100 + 40 ))
# 셀 좌표 산출 = 이미지 블롭 검출(스케일·레터박스·타이틀바 무관 — 실측 정밀).
# 1행 밴드만 검출 → row 0 표적만 지원(표적 row>0이면 FAIL로 정직 실패).
[ "$TARGET_ROW" -ne 0 ] && FAIL "target cell row=$TARGET_ROW != 0 — 픽커는 1행만 지원(표적 순서 실측 갱신 필요)"
PICK_OUT=$(python3 "$LOGDIR/grid_pick.py" "$LOGDIR/grid_cap.png" "$TARGET_COL" 2>&1)
echo "$PICK_OUT"
PICK_CELL=$(printf '%s\n' "$PICK_OUT" | grep -a '^PICK-CELL:' | head -1 | sed -n 's/^PICK-CELL: \([0-9]*\) \([0-9]*\).*/\1 \2/p')
[ -n "$PICK_CELL" ] || FAIL "cell pick failed — $PICK_OUT"
PCX=$(printf '%s' "$PICK_CELL" | cut -d' ' -f1)
PCY=$(printf '%s' "$PICK_CELL" | cut -d' ' -f2)
AX=$((WX + PCX)); AY=$((WY + PCY))
echo "CLICK-ABS-ROOT-CALC: ($AX,$AY) = win($WX,$WY ${GW}x$GH dw=${DW}x${DH}) + img-cell($PCX,$PCY)"
python3 "$LOGDIR/grid_dblclick.py" "$AX" "$AY" >>"$LOGDIR/dbl.log" 2>&1 \
    || FAIL "double-click injection failed — $(tail -2 "$LOGDIR/dbl.log" | tr '\n' ' ')"
grep -a "DBL-" "$LOGDIR/dbl.log"
sleep 6   # 클라 프로세스 기동+창 반영
SPAWN_LINE=$(grep -a "spawned " "$SRVLOG" | grep -a -- "--jkx" | head -1)
echo "SPAWN-LINE: ${SPAWN_LINE:-none}"
[ -n "$SPAWN_LINE" ] || FAIL "no '--jkx' spawn line after grid double-click — 더블클릭이 launch wiring에 전파 안 됨(원장 갭 잔존?)"
# 창 반영: 표적=settings — 모듈 존재(jkapp_settings.so)라 창이 떠야 정상.
# (셀 0이 legacy trigger로 잡히던 케이스와 달리 표적이 모듈 확산이므로 하드
# 어설션 유지 — 리트라이 8×2s.)
SETT_WIN=""
for i in 1 2 3 4 5 6 7 8; do
    sleep 2
    WIN_OUT=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)
    SETT_WIN=$(printf '%s' "$WIN_OUT" | grep -aoE '\{"id":[^}]*"title":"Settings"[^}]*\}' | head -1)
    [ -n "$SETT_WIN" ] && break
done
[ -n "$SETT_WIN" ] || FAIL "no Settings window after grid double-click — reply: ${WIN_OUT:-none}"
echo "SETTINGS-WINDOW: $SETT_WIN"

echo "=== 8. cleanup (bracketed pkill + 잔존 검사) ==="
pkill -f 'buildwsl/[j]kdesktop' 2>/dev/null
sleep 1
pkill -9 -f 'buildwsl/[j]kdesktop' 2>/dev/null
sleep 1
LEFT=$(ps -eo args | grep 'buildwsl/[j]kdesktop' | wc -l)
[ "$LEFT" -eq 0 ] || echo "WARN: $LEFT jkdesktop process(es) survived teardown"
if [ "$ENTRY_UP" = "0" ]; then
    echo "END-STATE: WSL 서버 DOWN — 진입 상태 원복 완료"
else
    env DISPLAY=:0 setsid nohup ./buildwsl/jkdesktop --server >"$LOGDIR/srv_restore.log" 2>&1 &
    sleep 6
    pgrep -f 'buildwsl/[j]kdesktop --server' >/dev/null 2>&1 || FAIL "restore boot failed"
    echo "END-STATE: WSL 서버 UP — 진입 상태 원복 완료"
fi
if [ "$ENTRY_PERM" = "0" ]; then
    [ -f buildwsl/permissions.json ] \
        && FAIL "probe left permissions.json behind (probe 소유 아님 — 원복 규약)"
    echo "PERM-RESTORE: absent (probe는 파일을 만지지 않았다)"
else
    [ -f buildwsl/permissions.json ] || echo "WARN: ENTRY에 있던 permissions.json 소실"
fi

echo "=== 9. REMNANT — /tmp 진단 잔존 소각 ==="
LEFTS=$(ls "$LOGDIR" 2>/dev/null | wc -l)
echo "REMNANT-COUNT=$LEFTS (서버로그·빌드로그·주입기만 잔존 — 자가 보수)"

echo "RECEIPTS:"
echo "  scan: jkx rows=$JKX_LINES console rows=$CONS_LINES"
echo "  capture: $RECEIVE/grid_open_wsl.png"
echo "GRID-OPEN-OK"
exit 0
# EOF — 끝 개행 유지.