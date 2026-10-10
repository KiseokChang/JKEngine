#!/usr/bin/env bash
# WSL 런처 그리드 posix 개방 probe (#95 — 스펙 2026-10-10-grid-open-plan §probe).
# 배경: JKDesktopShell.cpp ScanJkxApps/ScanConsoleApps가 _WIN32 전용 — posix 축
# (폰/WSL) 그리드가 내장 4셀(minesweeper/tetris/lf/hx 폴백)만 점화해 설치
# .jkx(갤러리/뮤직)가 안 떴다(사용자 라이브 보고 "뮤직 아이콘 부터가 안보여요").
# posix leg 개방의 WSL 실측 세그먼트: ① 스캔 stderr 원문(jkx 행 등장) ② 그리드
# 캡처 ③ 그리드 더블클릭이 SpawnClient(--jkx) 전파되는지 원문(서버 spawn 행)
# ④ 콘솔 셀(sampletodo) 클릭 → terminal 스폰 원문+Terminal 창+서면 캡처 — M-1
# 수형(스포 키 절대키 해상) 실측 (fix r1).
#
# 표준 승계(wsl_text_scale_diag.sh/wsl_gallery.sh 원문): WSLg :0, setsid 부팅,
# 브래킷 pkill+(-9 에스컬레이션), stale 소켓 rm, 창 단위 X11 캡처(python
# ctypes — 3틀림 교정본), selftest는 별도 세그먼트(본 probe는 부팅 스캔 실측이
# 목적). permissions.json은 wsl_gallery.sh 원복 계약 그대로: ENTRY에 부재할 때만
# probe가 1행 기록(send_input·close_window allow — 세그먼트 간 창 치움 용)·END
# 에서 소각(원문 보존). ENTRY에 파일이 있으면 건드리지 않고 FAIL(probe 런타임
# permissions 파일은 프로브 소유 아님 — 본 파일에 존중 명문). XTEST 합성
# 클릭 자체는 승인 가동층 불용(send_input 도구가 아니라 X11 root 계열 경로).
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
PROBE_WROTE_PERM=0
ENTRY_TRUST=0
[ -f buildwsl/state/trust.json ] && ENTRY_TRUST=1
echo "ENTRY-STATE: wsl-server-up=$ENTRY_UP permissions.json=$ENTRY_PERM trust.json=$ENTRY_TRUST"

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

echo "=== 0b. permissions.json 1행 — ENTRY 원복 계약 (wsl_gallery.sh 원문 승계) ==="
[ "$ENTRY_PERM" -eq 0 ] \
    || FAIL "buildwsl/permissions.json already exists — probe는 런타임 permissions 파일을 건드릴 수 없다"
printf '{\n    "send_input": "allow",\n    "close_window": "allow"\n}\n' > buildwsl/permissions.json
PROBE_WROTE_PERM=1
[ -f buildwsl/permissions.json ] || FAIL "permissions.json write failed"
echo "PERM-WIRE-OK: send_input+close_window allow (EXIT trap·END에서 소각)"
# EXIT trap — 어샥션 FAIL 경로에서도 원복을 존중한다(wsl_gallery.sh는 END 원복
# 만이라 중도 FAIL 시 permissions.json을 남기던 구멍 — trap으로 봉함).
trap '[ "$PROBE_WROTE_PERM" = "1" ] && rm -f buildwsl/permissions.json' EXIT

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

echo "=== 2b. exec 마커 스크래치 콘솔 앱 자가 시딩 (probe 소유 — END에서 소각) ==="
# M-1 수형의 실행 단정: /bin/sh -c <해상 키>가 실제로 스크립트를 살렸는지.
# 트윈 스크립트가 마커 파일을 절대 경로(/tmp/gridopen — cwd 무관)에 남긴다 —
# spawner cwd가 어디든 마커 = 키 해상+exec 성립의 물증. cmd_posix가 존재 게이트
# 통과이므로 spawn 키 = basePath + '/apps/zprobe_exec/run.sh' (절대키).
mkdir -p buildwsl/apps/zprobe_exec || FAIL "scratch console app mkdir failed"
cat > buildwsl/apps/zprobe_exec/manifest.json <<'MJ'
{
    "name": "zprobe_exec",
    "cmd": "apps\\zprobe_exec\\run.cmd",
    "cmd_posix": "apps/zprobe_exec/run.sh",
    "desc": "grid-open probe exec marker (probe-owned scratch)"
}
MJ
cat > buildwsl/apps/zprobe_exec/run.sh <<'RS'
#!/bin/sh
# grid-open probe exec marker twin (probe-owned) — the marker lands on an
# absolute path so any cwd can satisfy the resolution (M-1 수형 물증).
printf '%s\n' "$(pwd)" > /tmp/gridopen/exec_marker.txt
RS
chmod +x buildwsl/apps/zprobe_exec/run.sh
[ -x buildwsl/apps/zprobe_exec/run.sh ] || FAIL "scratch twin exec bit missing"
echo "SCRATCH-OK: apps/zprobe_exec (name=zprobe_exec, cmd_posix twin — 정렬상 콘솔 last)"

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

# 정렬 키 = .jkx 파일명(app name 아님! — legacy trigger는 파일명에 '\'가 들어
# 있는 drvfs 디코드 이름이라 'triggers\…' 블록이 t~v 사이에 정렬된다 — 본사
# C++ byFileName(파일명 소문자) 어샥션과 동일 키. C-2 1차 실측 교정: app name
# 뷰로 잡으면 triggers 블록이 tetris 뒤에 와 disorder 오판).
grep -a "installed app " "$SRVLOG" | sed -n 's/.* from \(.*\) (icon .*/\1/p' | tr 'A-Z' 'a-z' > /tmp/gridopen/scan_names.txt

echo "--- 갤러리/뮤직 원문 행(이번 수리의 표적 행):"
GAL_ROW=$(grep -a "installed app 'gallery'" "$SRVLOG" | head -1)
MUS_ROW=$(grep -a "installed app 'music'" "$SRVLOG" | head -1)
[ -n "$GAL_ROW" ] || FAIL "no scan row for 'gallery' — posix leg 확산 불충분"
[ -n "$MUS_ROW" ] || FAIL "no scan row for 'music' — posix leg 확산 불충분"
echo "GAL-ROW: $GAL_ROW"
echo "MUS-ROW: $MUS_ROW"

echo "--- C-2 결정론 순서 어설션 (fix r1 — posix leg 이름 정렬, Win NTFS 뷰 동형):"
if LC_ALL=C sort -c /tmp/gridopen/scan_names.txt 2>/dev/null; then
    echo "SCAN-SORTED-OK (이름 사전순 — 부팅마다 흔들리지 않는다)"
else
    FAIL "scan rows not name-sorted — posix 셀 순서 비결정론(C-2 재발)"
fi

echo "=== 5. 더블클릭 표적 셀 식별 (스캔 로그 순서 = 점화 순서 — 표적=settings) ==="
# 표적=settings — 모듈이 확실히 산(buildwsl/jkapp_settings.so) + 제목 어샥션
# 가능(Settings — 컨트롤러가 클릭 착지 셀의 정체까지 잠그는 꼬리콜).
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
# 그리드 셀 픽커 — 캡처 PNG에서 아이콘 블롭(밴드=행, 런=열)을 검출해 root 절대
# 좌표를 계산한다(scale·레터박스·타이틀바 함정 회피: 캡처 이미지 원점 = X 창
# 원점이므로 이미지 px + XGetGeometry (x,y)가 곧 root 좌표).
# fix r1: 다중 밴드(행 R 지원 — 콘솔 셀은 2행) + 열 앵커 X0(셀 0 = 첫 런 —
# 셀 0이 밝은 전면 아트면 앵커 파기 → 정직 FAIL) + 피치 스냅(연속 최소차).
import struct, sys, zlib

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
            w = struct.unpack(">I", data[0:4])[0]
            h = struct.unpack(">I", data[4:8])[0]
            ct = data[9]
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
        rows.append(flat[off + 1:off + 1 + stride])
        off += 1 + stride
    return w, h, ch, rows

def main():
    path = sys.argv[1]
    want_row = int(sys.argv[2])
    want_col = int(sys.argv[3])
    w, h, ch, rows = read_png_rgb(path)
    def dark(x, y):
        row = rows[y]
        o = x * ch
        return max(row[o], row[o + 1], row[o + 2]) < 70
    # 좌표 캘리브레이션 (WSL 1356x817 창 실측 — 3런 검증):
    #   셀(c,r) 아트 중심 = (X0 + 100c, Y0 + 100r), (X0, Y0) = (120, 141).
    #   근거: ① 1차 실행(무정렬 스캔)에서 셀 ord 6 클릭 → Settings 스폰
    #   (착지 좌표 720,141 — ord 6 = col 6 ✓) ② 2차(정렬 스캔) 셀 ord 18
    #   (col 6 row 1) 클릭 → Task Manager 스폰(착지 720,242 — 예상 ord 18
    #   = row1 col6 = taskmgr ✓ — 서버 spawn 원문으로 확정) ③ X 이미지 =
    #   Wayland 장식(좌 32 + 회색 6 / 상 32 + 바) + 1:1 SDL 서면 — 피치
    #   실측 100 = 논리 피치 1:1. 장식 오프셋은 WSLg 환경/창 크기 의존 —
    #   본 캘리브레이션은 창 1356x817 기준이고 아래 어샥션이 이탈을 감시한다.
    X0 = 120
    Y0 = 141
    # 밴드(행) 검출 — 상단 어두운 띄만 (y 60..375: 사진 하부 숲의 어두운
    # 띄가 밴드로 흡수되는 마쇁 회피 — 그리드 3행 범위).
    counts = {y: sum(1 for x in range(40, w - 40) if dark(x, y))
              for y in range(60, min(375, h))}
    bands = []
    cur = None
    for y in sorted(counts):
        if counts[y] > 200:
            if cur is None:
                cur = [y, y]
            elif y - cur[1] <= 3:
                cur[1] = y
            else:
                bands.append(cur)
                cur = [y, y]
    if cur is not None:
        bands.append(cur)
    if not bands:
        print("PICK-FAIL: no dark band in top rows")
        return 1
    # 밴드↔행 대응: 소음 밴드(높이 <20 — 사진 요동 1-2행 띄)는 스킵, 나머지는
    # 캘리브레이션 행 중심(Y0+100r) ±15 안으로 스냅 (사진과 접한 행 밴드는
    # 아래로 넓게 병합될 수 있다 — yc 스냅은 ±15 이내면 수용).
    row_bands = {}
    for b in bands:
        if b[1] - b[0] < 20:
            continue
        bc = (b[0] + b[1]) // 2
        if bc < Y0 - 50:
            print("PICK-FAIL: band %s above row 0 (bc=%d < Y0-50) — bands=%s" % (b, bc, bands))
            return 1
        r = (bc - Y0 + 50) // 100
        exp = Y0 + r * 100
        if abs(bc - exp) > 15:
            print("PICK-FAIL: band center %d off calibration row (Y0=%d) — bands=%s"
                  % (bc, Y0, bands))
            return 1
        row_bands.setdefault(r, b)
    if want_row not in row_bands:
        print("PICK-FAIL: want_row=%d bands=%s row_bands keys=%s" % (want_row, bands, sorted(row_bands)))
        return 1
    def runs_at(r):
        b = row_bands[r]
        yc = (b[0] + b[1]) // 2
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
        runs = [(a, b2) for a, b2 in runs if 40 <= b2 - a <= 95]
        return yc, [(a + b2) // 2 for a, b2 in runs]
    yct, centers_r = runs_at(want_row)
    # 어샥션: 검출된 모든 런 중심은 캘리브레이션 잔류군(≡X0 mod 100 ±3 — 아트
    # 가장자리 안티앨리어싱 1-2px 흔들림 실측)이어야 한다(피치·앵커 이탈 감시).
    # 밝은 전면 아트 셀은 런이 없어 통과한다.
    for c in centers_r:
        off = (c - X0) % 100
        if 3 < off < 97:
            print("PICK-FAIL: run center %d off residue (X0=%d, off=%d) — runs=%s"
                  % (c, X0, off, centers_r))
            return 1
    tcx = X0 + want_col * 100
    print("PICK: bands=%s rows=%s" % (bands, sorted(row_bands)))
    print("PICK: row=%d yc=%d runs=%s target-x=%d"
          % (want_row, yct, centers_r, tcx))
    print("PICK-CELL: %d %d (X0=%d Y0=%d row=%d col=%d)" % (tcx, yct, X0, Y0, want_row, want_col))
    return 0

if __name__ == "__main__":
    sys.exit(main())
PYEOF
[ -f "$LOGDIR/grid_pick.py" ] || FAIL "cell picker generation failed"

echo "=== 7. 더블클릭 → SpawnClient --jkx 전파 원문 ==="
# (C-4 fix r1) XTEST 경로는 agent 승인 가동층 불요 — permissions.json은 본
# probe 소유 0(만지지 않는다; END에서 absence 검증).
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
# 셀 좌표 산출 = 이미지 블롭 검출(스케일·레터박스·타이틀바 무관 — 실측 정밀).
# fix r1: 다중 밴드 — 행 R 표적 지원(콘솔 셀은 2행).
PICK_OUT=$(python3 "$LOGDIR/grid_pick.py" "$LOGDIR/grid_cap.png" "$TARGET_ROW" "$TARGET_COL" 2>&1)
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
# 표적 동일성 어샥션 — 클릭한 셀(settings)이 실제로 스포된 컨테이너와 동일한지
# (피커·캘리브레이션 이탈은 여기서 정직하게 잡힌다).
printf '%s' "$SPAWN_LINE" | grep -aq 'settings.jkx' \
    || FAIL "spawned container is not settings.jkx — cell/target identity mismatch — line: $SPAWN_LINE"
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
SE_ID=$(printf '%s' "$SETT_WIN" | grep -aoE '"id":[0-9]+' | grep -ao '[0-9]*' | head -1)
# 7b 콘솔 셀 클릭을 청화면에서 — Settings 창이 row 2 콘솔 셀(y 250..330)을
# 덮는다(900x620 @210,50): open 창 위 클릭은 클라 입력으로 흡수되어 LaunchAt
# 미호출(1차 fix r1 실측 — Terminal 무스폰 + spawn 행 0). close_window로 치운다
# (gate none/allow — kPermMatrix 행).
CL=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"close_window","args":{"id":'"$SE_ID"'}}' 2>/dev/null | grep -a '{' | head -1)
echo "CLOSE-SETTINGS: $CL"
printf '%s' "$CL" | grep -aq '"ok":true' || FAIL "close_window(Settings id=$SE_ID) failed — $CL"
for i in 1 2 3 4 5; do
    sleep 1
    WIN_OUT=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)
    printf '%s' "$WIN_OUT" | grep -avq '"title":"Settings"' && break
done
printf '%s' "$WIN_OUT" | grep -aq '"title":"Settings"' \
    && FAIL "Settings window survived close — can't get clean grid for console click" \
    || echo "GRID-CLEAN: Settings closed — desktop empty"

echo "=== 7b. 콘솔 셀 클릭 → terminal 스폰 — M-1 수형 실측 (spawnKey 절대키 해상) ==="
# 콘솔 셀 = jkx 행 이후 스캔(console scan leg) — 아이콘 순서 = jkx 행 수
# 이후 첫 console 행. 표적=sampletodo(jkapp_script 불요 — .sh 트윈 존재).
CONS_IDX=$(grep -a "console app '" "$SRVLOG" | grep -an "console app 'sampletodo'" | head -1 | cut -d: -f1)
[ -n "$CONS_IDX" ] || FAIL "no console app 'sampletodo' row — manifest.json 스캔 leg 미점화"
CONS_ORD=$(( JKX_LINES + CONS_IDX - 1 ))
echo "CELL-TARGET-CONSOLE: sampletodo icon-ord=$CONS_ORD (jkx=$JKX_LINES + console 행 $CONS_IDX)"
CCOL=$(( CONS_ORD % COLS )); CROW=$(( CONS_ORD / COLS ))
C_PICK=$(python3 "$LOGDIR/grid_pick.py" "$LOGDIR/grid_cap.png" "$CROW" "$CCOL" 2>&1)
echo "$C_PICK"
C_CELL=$(printf '%s\n' "$C_PICK" | grep -a '^PICK-CELL:' | head -1 | sed -n 's/^PICK-CELL: \([0-9]*\) \([0-9]*\).*/\1 \2/p')
[ -n "$C_CELL" ] || FAIL "console cell pick failed — $C_PICK"
CBX=$((WX + $(printf '%s' "$C_CELL" | cut -d' ' -f1)))
CBY=$((WY + $(printf '%s' "$C_CELL" | cut -d' ' -f2)))
SNAP_BEFORE=$(grep -a "spawned jkdesktop terminal" "$SRVLOG" | wc -l)
python3 "$LOGDIR/grid_dblclick.py" "$CBX" "$CBY" >>"$LOGDIR/dbl.log" 2>&1 \
    || FAIL "console cell double-click injection failed"
echo "DBL-CONSOLE: root($CBX,$CBY) ordinal icon $CONS_ORD (row $CROW col $CCOL)"
TERM_WIN=""
TERM_ID=""
for i in $(seq 1 60); do
    WIN_OUT=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)
    TERM_WIN=$(printf '%s' "$WIN_OUT" | grep -aoE '\{"id":[^}]*"title":"Terminal"[^}]*\}' | head -1)
    [ -n "$TERM_WIN" ] && break
    sleep 0.1
done
echo "TERMINAL-WINDOW: ${TERM_WIN:-none}"
if [ -n "$TERM_WIN" ]; then
    TERM_ID=$(printf '%s' "$TERM_WIN" | grep -aoE '"id":[0-9]+' | grep -ao '[0-9]*' | head -1)
fi
# 원문 1: 서버 spawn 행 — spawnKey 절대키(--shell '"..."' 의 '/mnt/…' 개두)인지
# 직격 어설션. 상대키(apps/sampletodo/sampletodo.sh)라면 /bin/sh child cwd=
# apps/<dir>에서 apps/<dir>/apps/… 중복 접두 소실 = M-1 재발.
sleep 1
CONS_SPAWN=$(grep -a "spawned jkdesktop terminal" "$SRVLOG" | tail -1)
echo "CONSOLE-SPAWN-LINE: ${CONS_SPAWN:-none}"
SHELL_KEY=$(printf '%s' "$CONS_SPAWN" | grep -ao -- '--shell "[^"]*"' | sed 's/^--shell "//; s/"$//')
[ -n "$SHELL_KEY" ] || FAIL "console spawn line has no --shell key — line: $CONS_SPAWN"
printf '%s' "$SHELL_KEY" | grep -aq '^/' || FAIL "posix spawn key is not absolute (M-1 재발 — 죽은 키) — key: $SHELL_KEY"
printf '%s' "$SHELL_KEY" | grep -aq 'apps/sampletodo/sampletodo.sh' || FAIL "spawn key lost the twin path — key: $SHELL_KEY"
echo "SPAWN-KEY-ABS-OK: --shell '$SHELL_KEY' (절대키 해상 — cwd 무관 성립)"
# 원문 2: 터미널 서면 캡처 — /bin/sh 내 명령 실행 화면 (todo.txt hint 행 육안
# receipt → engine/tmp/grid_open_terminal_wsl.jpg).
# 원문 3: 터미널 서면 캡처 — /bin/sh 내 명령 실행 화면 (todo.txt hint 행 육안
# receipt → engine/tmp/grid_open_terminal_wsl.jpg). 트윈은 출력 후 즉사하고
# 터미널 창이 닫히므로(1차 fix r1 실측: 창 수명 < 1s) 캡처는 경주 — 잡히면
# 서면 receipt, 놓치면 정직 NOTE(스폰 원문 행이 이미 하드 receipt).
rm -f "$RECEIVE/grid_open_terminal_wsl.jpg"
if [ -n "$TERM_ID" ]; then
    timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"window_frame","args":{"id":'"$TERM_ID"'}}' >/tmp/gridopen/frame_term.json 2>/dev/null \
        || echo "FRAME-NOTE: window_frame(Terminal id=$TERM_ID) call failed"
    python3 - "$RECEIVE/grid_open_terminal_wsl.jpg" <<'PYEOF'
import base64, json, sys
try:
    d = json.load(open("/tmp/gridopen/frame_term.json"))
except Exception as e:
    print("FRAME-NOTE: no reply JSON (%s)" % e)
    sys.exit(0)
if not d.get("ok"):
    print("FRAME-NOTE: reply ok=%s error=%s — 터미널 소멸 후 캡처 경주(정직 기록)" % (d.get("ok"), d.get("error")))
    sys.exit(0)
open(sys.argv[1], "wb").write(base64.b64decode(d["data"]))
print("FRAME-OK: saved %s (%d bytes)" % (sys.argv[1], len(d["data"])))
PYEOF
fi
[ -f "$RECEIVE/grid_open_terminal_wsl.jpg" ] \
    && echo "EXEC-CAPTURE: $RECEIVE/grid_open_terminal_wsl.jpg ($(wc -c < "$RECEIVE/grid_open_terminal_wsl.jpg") bytes — 1행 '(todo.txt not found…)' 육안 receipt)" \
    || echo "EXEC-CAPTURE: 미수취(트윈 즉사 — 창 수명 < 폴 간격) — 터미널 클라 스폰 원문 행(--shell 절대키)이 본 세그먼트의 실측 운반체"

echo "=== 7c. 스크래치 콘솔 셀 클릭 — exec 마커 실측 (키 해상 → 실살 단정) ==="
Z_IDX=$(grep -a "console app '" "$SRVLOG" | grep -an "console app 'zprobe_exec'" | head -1 | cut -d: -f1)
[ -n "$Z_IDX" ] || FAIL "no console app 'zprobe_exec' row — scratch manifest 미스캔"
Z_ORD=$(( JKX_LINES + Z_IDX - 1 ))
ZCOL=$(( Z_ORD % COLS )); ZROW=$(( Z_ORD / COLS ))
Z_PICK=$(python3 "$LOGDIR/grid_pick.py" "$LOGDIR/grid_cap.png" "$ZROW" "$ZCOL" 2>&1)
Z_CELL=$(printf '%s\n' "$Z_PICK" | grep -a '^PICK-CELL:' | head -1 | sed -n 's/^PICK-CELL: \([0-9]*\) \([0-9]*\).*/\1 \2/p')
echo "$Z_PICK"
[ -n "$Z_CELL" ] || FAIL "zprobe_exec cell pick failed — $Z_PICK"
ZBX=$((WX + $(printf '%s' "$Z_CELL" | cut -d' ' -f1)))
ZBY=$((WY + $(printf '%s' "$Z_CELL" | cut -d' ' -f2)))
python3 "$LOGDIR/grid_dblclick.py" "$ZBX" "$ZBY" >>"$LOGDIR/dbl.log" 2>&1 \
    || FAIL "zprobe_exec cell double-click injection failed"
echo "DBL-SCRATCH: root($ZBX,$ZBY) ordinal icon $Z_ORD (row $ZROW col $ZCOL)"
sleep 1   # 스폰 stderr 플러시 경주 방지(서버가 클릭 이벤트 → spawn 즉행이나
          # 로그 파싱 대기 없이 읽으면 none 오판 — 1차 fix r1 실측)
Z_SPAWN=$(grep -a "spawned jkdesktop terminal" "$SRVLOG" | grep -a 'zprobe_exec' | head -1)
echo "SCRATCH-SPAWN-LINE: ${Z_SPAWN:-none}"
printf '%s' "$Z_SPAWN" | grep -aq -- '--shell "/' \
    || FAIL "scratch spawn key not absolute — line: ${Z_SPAWN:-none}"
# 원문 3: 마커 수취 — /bin/sh -c <해상 키>가 실살됐는지의 단정(트윈이
# 절대경로 마커에 pwd를 쓴다 — cwd 무관).
for i in 1 2 3 4 5 6 7 8 9 10; do
    [ -f /tmp/gridopen/exec_marker.txt ] && break
    sleep 0.5
done
[ -f /tmp/gridopen/exec_marker.txt ] \
    && echo "EXEC-MARKER-OK: twin ran under resolved absolute key — marker pwd: $(cat /tmp/gridopen/exec_marker.txt)" \
    || FAIL "exec marker never landed — 키 해상 스크립트가 실행되지 않았다(M-1 재발)"

echo "=== 8. cleanup (bracketed pkill + 잔존 검사) ==="
pkill -f 'buildwsl/[j]kdesktop' 2>/dev/null
sleep 1
pkill -9 -f 'buildwsl/[j]kdesktop' 2>/dev/null
sleep 1
LEFT=$(ps -eo args | grep 'buildwsl/[j]kdesktop' | wc -l)
[ "$LEFT" -eq 0 ] || echo "WARN: $LEFT jkdesktop process(es) survived teardown"
# probe 소유 스크래치 콘솔 앱 소각 — apps/ 트리 잔상 0(서버 재부팅 전, rescan
# 스캔 오염 방지).
rm -rf buildwsl/apps/zprobe_exec
[ -d buildwsl/apps/zprobe_exec ] \
    && FAIL "scratch console app survived cleanup — apps/zprobe_exec 수동 소각 필요"
echo "SCRATCH-REMOVED: apps/zprobe_exec (probe 소유 잔상 0)"
# probe가 유발한 trust ledger 기록(state/trust.json — 콘솔 스캔 EnsureTrustRecord
# upsert): ENTRY에 파일이 없었으면 소각(런타임 유저 파일이지만 본 probe가
# 만든 잔상 — 원복 계약), 있었으면 건드리지 않는다.
if [ "$ENTRY_TRUST" = "0" ] && [ -f buildwsl/state/trust.json ]; then
    rm -f buildwsl/state/trust.json
    echo "TRUST-RESTORE: probe-made trust.json removed (ENTRY 부재 원복)"
fi
if [ "$ENTRY_UP" = "0" ]; then
    echo "END-STATE: WSL 서버 DOWN — 진입 상태 원복 완료"
else
    env DISPLAY=:0 setsid nohup ./buildwsl/jkdesktop --server >"$LOGDIR/srv_restore.log" 2>&1 &
    sleep 6
    pgrep -f 'buildwsl/[j]kdesktop --server' >/dev/null 2>&1 || FAIL "restore boot failed"
    echo "END-STATE: WSL 서버 UP — 진입 상태 원복 완료"
fi
if [ "$ENTRY_PERM" = "0" ]; then
    # probe가 1행 기록한 소유 파일 — END에서 소각(EXIT trap 동일 소각).
    PROBE_WROTE_PERM=0   # trap 재발 방지 — 여기서 정식 소각
    rm -f buildwsl/permissions.json
    [ -f buildwsl/permissions.json ] \
        && FAIL "permissions.json removal failed (END 소각 누락)"
    echo "PERM-RESTORE: removed probe-owned copy (ENTRY 부재 원복 완료)"
else
    [ -f buildwsl/permissions.json ] || echo "WARN: ENTRY에 있던 permissions.json 소실"
fi
rm -f /tmp/gridopen/exec_marker.txt

echo "=== 9. REMNANT — /tmp 진단 잔존 소각 ==="
LEFTS=$(ls "$LOGDIR" 2>/dev/null | wc -l)
echo "REMNANT-COUNT=$LEFTS (서버로그·빌드로그·주입기만 잔존 — 자가 보수)"

echo "RECEIPTS:"
echo "  scan: jkx rows=$JKX_LINES console rows=$CONS_LINES"
echo "  capture: $RECEIVE/grid_open_wsl.png"
if [ -f "$RECEIVE/grid_open_terminal_wsl.jpg" ]; then
    echo "  exec-capture: $RECEIVE/grid_open_terminal_wsl.jpg (M-1 수형 — /bin/sh 내 sampletodo 실행 서면)"
else
    echo "  exec-capture: 미수취 — 터미널 클라 스폰 원문 행(--shell 절대키)으로 대용"
fi
echo "GRID-OPEN-OK"
exit 0
# EOF — 끝 개행 유지 (C-5 fix r1: 실물 개행 확인 — 본 뒤 1개 LF).
