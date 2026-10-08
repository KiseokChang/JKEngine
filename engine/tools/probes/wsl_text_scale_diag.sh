#!/usr/bin/env bash
# T1 posix 텍스트 스케일 근원 규명 — WSL 공통 재현+결선 단정 probe
# (스펙 2026-10-09-phone-text-scale T1 — plan task-1 Step 1).
#
# 배경: 폰 settings text.font_scale=1.9에서 셀 기하만 커지고 글리프는 고정
# 비트맵(desk_s10/desk_s19 영수증). 코드 원문 분석(T1 선행): 클라의 실제
# 그리기 경로는 ComposeScene의 지역 JKDC(JKClientApplication.cpp:751)인데
# SetTextAtlas는 죽은 멤버 dc_(:149/:200)에만 걸려 있다 — 글리프는 DrawGlyph
# (src/JKDC.cpp:198) 진입 불가 → EngPutCh/HanPutCh 비트맵 폴백. 동형 조건이
# 싱글 프로세스 ComposeScene(JKApplication.cpp:471)에도 있다(포탄 조사용).
#
# 본 probe가 수집하는 증거(5후보 원장 입력):
#   leg0 — settings 부재(WSL 출하 상태): 클라 stderr 경고 원문 수집
#          ("no vector font configured" 예상 → 후보②의 계약 동작 증명).
#   legA — settings font_path(윈도 malgun, drvfs) + font_scale 1.0:
#          Init 성공(경고 0)인데 글리프 비트맵 → 후보③ 배제.
#   legB — font_path 동일 + font_scale 1.9: 폰 결함 동형 재현(posix 공통).
#   legC — [임시 배선 실험, 커밋 금지] ComposeScene 지역 dc에만 SetTextAtlas
#          3줄 주입 → 재빌드 → 1.9 캡처: 벡터 글리프로 나오면 ⑤ 단정(원인).
#   legD — 임시 배선 유지 + font_scale 삭제(1.0): 벡터 8px — T2 목표 상태
#          참조표. (legC/D 끝나면 원복+재빌드 클린.)
# 판정은 캡처 PNG(engine/tmp/tsd_w_*.png) 육안+원장 표(리포트)로 봉합한다.
#
# 표준(wsl_dirty_present 선례 승계): WSLg :0, setsid 부팅, 브래킷 pkill+(-9
# 에스컬레이션), stale 소켓 rm, agentctl ping/launch_app, selftest 불요(소스
# 무변경 진단 — legC/D 임시 배선 원복+수산 클린 재빌드로 소스 무변경 회복).
# 실행법(WSL 밖): MSYS_NO_PATHCONV=1 wsl.exe -d Ubuntu-24.04 -- bash \
#   /mnt/i/progwork/JKENGINE/engine/tools/probes/wsl_text_scale_diag.sh
set -u
cd /mnt/i/progwork/JKENGINE/engine

FAIL() { echo "TSD-FAIL: $*"; exit 1; }
LOGDIR=/tmp/tsd
mkdir -p "$LOGDIR"
SET=buildwsl/state/settings.json
FONT=/mnt/c/Windows/Fonts/malgun.ttf

ENTRY_UP=0
pgrep -f 'buildwsl/[j]kdesktop' >/dev/null 2>&1 && ENTRY_UP=1
echo "ENTRY-STATE: wsl-server-up=$ENTRY_UP (종료 시 원복)"

echo "=== 0. 잡석 정리 — 임시 진단 캡처 스크립트를 /tmp에 자가 생성 ==="
cat > "$LOGDIR/tsd_shot.py" <<'PYEOF'
# t5_xshot.py (tmp 선례) 축본 — 창 단위 X11 캡처+최소 PNG 기록기.
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
OFF_WIDTH, OFF_HEIGHT, OFF_DATA = 0, 4, 16
OFF_DEPTH, OFF_BPL = 40, 44
OFF_RED, OFF_GREEN, OFF_BLUE = 48, 56, 64

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
    want = sys.argv[2] if len(sys.argv) > 2 else None
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
        if want:
            for w, t, gw, gh in cands:
                if want.lower() in t.lower():
                    pick = (w, gw, gh); break
        if pick is None:
            # 서버 데스크톱 창(전체 합성 뷰 — 폰 x11grab 프레임과 동형).
            # 실측 함정: XWayland 시스템 배경 창이 8192x8192로 root 자식이라
            # "최대 면적"이 그놈을 잡는다 — 4K 상한으로 배제, SDL WM_NAME이
            # 빈 문자열이라 타이틀 검색도 못 쓴다.
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
        data_ptr = c_void_p.from_address(img + OFF_DATA).value
        red = ctypes.c_uint64.from_address(img + OFF_RED).value
        green = ctypes.c_uint64.from_address(img + OFF_GREEN).value
        blue = ctypes.c_uint64.from_address(img + OFF_BLUE).value
        print("XSHOT hit: win=0x%x depth=%d %dx%d" % (w, depth, gw, gh))
        bpp = bpl // gw if gw else 4
        if bpp not in (3, 4):
            print("XSHOT-FAIL: unsupported bpp=%d" % bpp)
            return 1
        raw = ctypes.string_at(data_ptr, bpl * gh)
        rgb = bytearray(gw * gh * 3)
        for yy in range(gh):
            row = yy * bpl
            for xx in range(gw):
                off = row + xx * bpp
                v = int.from_bytes(raw[off:off + bpp], "little")
                o = (yy * gw + xx) * 3
                rgb[o] = ((v & red) * 255 // red if red else 0) & 0xFF
                rgb[o + 1] = ((v & green) * 255 // green if green else 0) & 0xFF
                rgb[o + 2] = ((v & blue) * 255 // blue if blue else 0) & 0xFF
        X11.XDestroyImage(img)
        write_png(out, gw, gh, bytes(rgb))
        print("XSHOT wrote %s" % out)
        return 0
    finally:
        X11.XCloseDisplay(d)

if __name__ == "__main__":
    sys.exit(main())
PYEOF
[ -f "$LOGDIR/tsd_shot.py" ] || FAIL "capture script generation failed"

write_settings() { # $1=font_path(공백=삭제) $2=font_scale(공백=삭제)
    if [ -z "$1" ] && [ -z "$2" ]; then
        rm -f "$SET"
        echo "settings: removed ($SET absent)"
        return 0
    fi
    mkdir -p buildwsl/state
    local keys=""
    if [ -n "$1" ]; then keys="\"font_path\": \"$1\""; fi
    if [ -n "$2" ]; then
        [ -n "$keys" ] && keys="$keys, "
        keys="$keys\"font_scale\": \"$2\""
    fi
    printf '{\n    "text": {\n        %s\n    }\n}\n' "$keys" > "$SET"
    echo "settings: wrote $SET -> $(tr -d '\n' < "$SET")"
}

echo "=== 1. bracketed pre-clean ==="
pkill -f 'buildwsl/[j]kdesktop' 2>/dev/null
sleep 1
pkill -9 -f 'buildwsl/[j]kdesktop' 2>/dev/null
sleep 1
LEFT=$(ps -eo args | grep 'buildwsl/[j]kdesktop' | wc -l)
[ "$LEFT" -eq 0 ] || FAIL "pre-clean left $LEFT jkdesktop process(es) alive"
[ -f "$FONT" ] || FAIL "drvfs font missing: $FONT (WSL 폰트 원천 — 실측 불가)"

# ---------------------------------------------------------------- 레그 계측
run_leg() { # $1 라벨
    LEG=$1
    LOG="$LOGDIR/tsd_srv_${LEG}.log"
    echo "=== leg $LEG boot ==="
    pkill -f 'buildwsl/[j]kdesktop' 2>/dev/null
    sleep 1
    pkill -9 -f 'buildwsl/[j]kdesktop' 2>/dev/null
    sleep 1
    rm -f /tmp/JKWindowServerPipe.sock
    env DISPLAY=:0 setsid nohup ./buildwsl/jkdesktop --server >"$LOG" 2>&1 &
    sleep 6
    SRV_PID=$(pgrep -f 'buildwsl/[j]kdesktop --server' | head -1)
    [ -n "$SRV_PID" ] || FAIL "leg $LEG: no server 6s after boot — log tail: $(tail -3 "$LOG" | tr '\n' ' ')"
    echo "leg $LEG: server pid=$SRV_PID"
    PING=""
    for i in 1 2 3 4 5; do
        PING=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"ping","args":{}}' 2>/dev/null | grep -a '{' | head -1)
        printf '%s' "$PING" | grep -aq '"ok":true' && break
        sleep 2
    done
    printf '%s' "$PING" | grep -aq '"ok":true' || FAIL "leg $LEG: ping failed — $PING"
    L=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"launch_app","args":{"app":"minesweeper"}}' 2>/dev/null | grep -a '{' | head -1)
    printf '%s' "$L" | grep -aq '"ok":true' || FAIL "leg $LEG: launch_app minesweeper failed — $L"
    for i in 1 2 3 4 5 6 7 8; do
        sleep 2
        WIN=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)
        MW=$(printf '%s' "$WIN" | grep -aoE '\{"id":[^}]*"title":"Minesweeper"[^}]*\}' | head -1)
        [ -n "$MW" ] && break
    done
    [ -n "$MW" ] || FAIL "leg $LEG: no Minesweeper window — last: ${WIN:-none}"
    echo "MINEWINDOW-$LEG: $MW"
    sleep 5   # settle — 1s 폴백 렌더 이후 화면
    echo "--- leg $LEG 클라/서버 stderr 경고 수집 (atlas 3종+HangulManager) ---"
    grep -aE 'Warning: (no vector font|vector font init|fallback font|HangulManager)' "$LOG" \
        || echo "(no font warnings in leg $LEG log)"
    OUT=/mnt/i/progwork/JKENGINE/engine/tmp/tsd_w_${LEG}.png
    rm -f "$OUT"
    python3 "$LOGDIR/tsd_shot.py" "/tmp/tsd_${LEG}_x.png" >>"$LOGDIR/shot_${LEG}.log" 2>&1 \
        || FAIL "leg $LEG: capture failed — $(tail -2 "$LOGDIR/shot_${LEG}.log" | tr '\n' ' ')"
    cp /tmp/tsd_${LEG}_x.png "$OUT" || FAIL "leg $LEG: capture copy failed"
    rm -f /tmp/tsd_${LEG}_x.png
    echo "CAPTURE-$LEG: $OUT ($(wc -c < "$OUT") bytes)"
}

echo "=== 2. leg0 — WSL 출하 상태(settings 부재) 경고 계약 수집 ==="
write_settings "" ""
run_leg leg0

echo "=== 3. legA — font_path(malgun drvfs) + scale 1.0 ==="
write_settings "$FONT" ""
run_leg legA

echo "=== 4. legB — font_path 동일 + font_scale 1.9 (폰 결함 동형 재현) ==="
write_settings "$FONT" "1.9"
run_leg legB

echo "=== 5. legC — [임시 배선 실험] ComposeScene 지역 dc SetTextAtlas 주입 ==="
# 원복은 git checkout으로 하지 않는다(WSL git은 drvfs CRLF 팬텀을 M로 보고
# — 실측: LICENSE가 실제 무변경인데 M 출력 — 진작 필요 이상 위험). 백업 복사
# + cmp 바이트 대조로 소스 무변경을 사수한다.
cp src/client/JKClientApplication.cpp /tmp/tsd_src_jka.bak \
    || FAIL "pristine backup copy failed"
python3 - <<'PYEOF'
import sys
p = "src/client/JKClientApplication.cpp"
src = open(p, encoding="utf-8").read()
anchor = (          "    auto cmdList = std::make_unique<JKRenderCommandList>();\n"
                    "    JKDC dc(cmdList.get());\n"
                    "    dc.SetHangulManager(hangulManager_.get());\n")
if src.count(anchor) != 1:
    raise SystemExit("PATCH-FAIL: anchor occurrences=%d (need 1)" % src.count(anchor))
wire = ("    // [T1 diag temp — 커밋 금지, 원복 필수] 후보⑤ 단정 실험 — compose\n"
        "    // 지역 dc에 벡터 아틀라스 주입(JKClientApplication.cpp:751 인접).\n"
        "    if (textAtlas_ && textAtlas_->IsLoaded() && resourceCache_) {\n"
        "        dc.SetTextAtlas(textAtlas_.get(), resourceCache_.get());\n"
        "    }\n")
open(p, "w", encoding="utf-8").write(src.replace(anchor, anchor + wire, 1))
print("PATCH-OK: compose dc SetTextAtlas wired (temp, revert before commit)")
PYEOF
[ ${PIPESTATUS[0]} -eq 0 ] || FAIL "temp patch failed — source untouched, aborting before build"
grep -aq 'T1 diag temp' src/client/JKClientApplication.cpp || FAIL "patch marker missing after patch"
# 실측 함정: legB 서버가 drvfs로 buildwsl/jkdesktop 실행 중이면 링커가
# 출력 파일을 못 연다("cannot open output file ... No such file or directory")
# — ninja 전에 브래킷 정지.
pkill -f 'buildwsl/[j]kdesktop' 2>/dev/null; sleep 1
pkill -9 -f 'buildwsl/[j]kdesktop' 2>/dev/null; sleep 1
ninja -C buildwsl -j4 >"$LOGDIR/tsd_build_legC.log" 2>&1
B_RC=${PIPESTATUS[0]}
echo "WSL-BUILD-legC-RC=$B_RC"
[ "$B_RC" -eq 0 ] || FAIL "ninja build rc=$B_RC — $(tail -4 "$LOGDIR/tsd_build_legC.log" | tr '\n' ' ')"
run_leg legC

echo "=== 6. legD — 임시 배선 유지 + font_scale 삭제(1.0) — T2 목표 상태 참조 ==="
write_settings "$FONT" ""
run_leg legD

echo "=== 7. 원복 + 클린 재빌드 + 소스 무변경 사수 ==="
cp /tmp/tsd_src_jka.bak src/client/JKClientApplication.cpp \
    || FAIL "restore of pristine copy failed — 수동 원복 필요"
cmp -s /tmp/tsd_src_jka.bak src/client/JKClientApplication.cpp \
    || FAIL "JKClientApplication.cpp byte-differs after restore (수동 원복 필요)"
echo "SOURCE-CLEAN-RESTORE=OK (byte-identical to pre-patch copy)"
pkill -f 'buildwsl/[j]kdesktop' 2>/dev/null; sleep 1
pkill -9 -f 'buildwsl/[j]kdesktop' 2>/dev/null; sleep 1
ninja -C buildwsl -j4 >"$LOGDIR/tsd_build_clean.log" 2>&1
C_RC=${PIPESTATUS[0]}
echo "WSL-BUILD-clean-RC=$C_RC"
[ "$C_RC" -eq 0 ] || FAIL "clean rebuild rc=$C_RC"

echo "=== 8. 클린 바이너리 부팅 확인 → 진입 상태 원복 ==="
run_leg legZ
pkill -f 'buildwsl/[j]kdesktop' 2>/dev/null
sleep 1
pkill -9 -f 'buildwsl/[j]kdesktop' 2>/dev/null
sleep 1
LEFT=$(ps -eo args | grep 'buildwsl/[j]kdesktop' | wc -l)
[ "$LEFT" -eq 0 ] || FAIL "shutdown left $LEFT process(es) alive (진입 상태=DOWN 원복)"
if [ "$ENTRY_UP" = "0" ]; then
    echo "END-STATE: WSL 서버 DOWN — 진입 상태 원복 완료"
else
    env DISPLAY=:0 setsid nohup ./buildwsl/jkdesktop --server >"$LOGDIR/tsd_srv_restore.log" 2>&1 &
    sleep 6
    pgrep -f 'buildwsl/[j]kdesktop --server' >/dev/null 2>&1 \
        || FAIL "restore boot failed"
    echo "END-STATE: WSL 서버 UP — 진입 상태 원복 완료"
fi

# settings 원복 검사 — leg0 출하 상태=settings 부재.
[ -f "$SET" ] && echo "NOTE: $SET left behind ($(cat "$SET"))" \
    || echo "SETTINGS-RESTORED: absent (출하 상태 원복)"

echo "=== 9. REMNANT 검사 — /tmp 진단 잔존 소각 확인 ==="
rm -f "$LOGDIR"/shot_leg*.log
LEFTS=$(ls "$LOGDIR" 2>/dev/null | wc -l)
echo "REMNANT-COUNT=$LEFTS (서버로그·빌드로그만 잔존 — 자가 보수)"
ls "$LOGDIR" | head

echo "TSD-CAPTURES:"
for f in /mnt/i/progwork/JKENGINE/engine/tmp/tsd_w_leg*.png; do
    echo "  $f ($(wc -c < "$f") bytes)"
done
echo "TASKTSD-END"
exit 0
# EOF — 끝 개행 유지.