#!/usr/bin/env bash
# T4 WSL music 실측 probe (스펙 2026-10-09-music-library — plan task-4).
#
# 목적: T1-T3 music 모듈(디렉토리 탭 스트립+비동기 스캔 워커+경로/크기/수정
#   표+이름 필터+더블클릭 vplayer 위임 T3 폴백)을 WSL 실측한다 —
#   부팅 → launch_app music → 탭 클릭(리스캔 도착 대기) → 리스트 캡처 →
#   필터 타이핑 캡처 → 표행 더블클릭(위임 — vplayer 콜드 부팅 경기 포함) →
#   Video Player 창 출현 + get_status 재생 수치 → 접착제 직행(app_tool open) →
#   get_status 재실측 → 복원.
#
# 계약 원문(승계 — wsl_gallery.sh 전체 수형):
#   * 테스트 미디어 — **합성 WAV**(python3 wave stdlib, 자가 생성 — 엔진
#     소유): i:\@keep 등 사용자 미디어 사용 금지(19금 계약 — vplayer 원문).
#     ffmpeg 부재 실측(WSL Ubuntu-24.04) — wave 모듈이 의존 0 경로.
#   * music.dirs 주입 — buildwsl/state/settings.json 1행(music.dirs 배열,
#     갤러리가 gallery.dirs를 주입하는 수형은 아니고 settings 부재 ENTRY를
#     기록+원복한다 — 전면 allow 런타임 파일은 프로브 소유 아님 계약).
#     스캔 폴더는 **$TMPDIR(/tmp) 아래 임시 폴더** — WSL 클라가 직접 스캔.
#   * idle 측정 불요 — 이 probe는 계약·모양·위임만(idle 영수증은 이 라인
#     스코프 밖 — client_idle 계열 probe가 소유).
#   * 위임 접착제 — app_tool open/get_status는 **직행 2콜**로 검증한다(music
#     앱을 경유하지 않는다 — app_tool은 남의 앱에 보내는 도구 허브 릴레이).
#     music 창의 실제 더블클릭 위임은 send_input 2탭(합성 마우스 — 선행
#     MouseMove 필수 원문 계약)으로 시도하고, ImGui 더블클릭 시간창
#     (MouseClickedTime 0.30s 기본)에 agentctl 왕복이 못 미치면 그 사유를
#     원장에 남긴다(MUSIC-DELEGATE 라인 — honest-fail). 위임 open의 도착은
#     **직행 open을 넣기 전** get_status 폴링으로 귀속한다(v2 실측 렛슨 —
#     직행 open이 먼저 가면 귀속이 사라진다).
#   * T3 콜드 경기 관측 (T4 재실측 — 라인 신설 결함 원장): 콜드 위임의 open은
#     vplayer에 도달하지 않았다 — 진원 = 서버 즉답 경로(:6747)의 봉투
#     ok=고정 1 × PollReplies의 reply.ok 성공 분류(ClientMusicApp :393) —
#     sync unknown_app_tool 거부가 "vplayer 재생 요청됨"으로 읽혀 재청구
#     크레딧이 즉시 소멸한다(폴백 자기 소멸 — MUSIC-DELEGATE-OPEN MISS
#     라인+리포트 원장). 접착제 직행 open은 등록 후라 정상(accepted+opened).
#   * 캡처 회수 — 단열 파이프: /tmp 캡처 → engine/tmp 영수증 단일 cp
#     (ls·혼입 금지). mus_wsl_{list,filter,after}.png. 캡처기=wsl_gallery.sh
#     원문의 X11 창 단위 shot(python3 ctypes — XImage 오프셋 원장 포함 전문).
#   * 복원 — 시드 wav+settings.json+permissions.json 소각(ENTRY 기록+원복)·
#     서버 DOWN(ENTRY에 UP이면 재부팅)·REMNANT(시드 dir 잔존=hard FAIL —
#     probe 소유 잔상 0).
#
# 판정(스크립트 규약 — gallery 원문 수형): 인프라 실패(빌드·부팅·ping·launch·
# 창 부재·캡처 실패·시드·소각)만 hard FAIL rc=1. 수치·육안 이벤트는
# MUSIC-OK/MUSIC-FAIL 원장 라인으로 남기고 rc=0(honest-fail) — 최종 판정은
# 캡처 육안 스탭.
#
# 표준: WSLg :0, setsid 부팅, 브래킷 pkill+(-9 에스컬레이션), stale 소켓 rm,
# selftest 출력 WSL 내부 리다이렉트, ninja 풀 로그 리다이렉트.
#
# 실행 표준(WSL 밖 Git Bash에서):
#   MSYS_NO_PATHCONV=1 wsl.exe -d Ubuntu-24.04 -- bash /mnt/i/progwork/JKENGINE/engine/tools/probes/wsl_music.sh \
#     > engine/tmp/wsl_music_run.log 2>&1
# MUS_RECON=1 로 실행하면 리스트 캡처 직후 복원·종료(클릭 좌표 정찰 런 —
# 상수 재보정 후 풀 런).
set -u
cd /mnt/i/progwork/JKENGINE/engine

FAIL() { echo "MUS-FAIL: $*"; exit 1; }
LOGDIR=/tmp/mus
mkdir -p "$LOGDIR"
CANON_WSL_SELFTEST=590   # 캐논 WSL 축 (progress 원장 — T3 fix r2 재실측)
RECEIVE=/mnt/i/progwork/JKENGINE/engine/tmp
SEED_DIR=/tmp/mus_seed
SHOT_PY="$LOGDIR/mus_shot.py"

# 합성 입력(클릭·타이핑) 좌표 — 클라 좌표(desktop point = 창 xy + 오프셋,
# wsl_gallery 셀 0 클릭 원문 수형). 정찰 런의 캡처로 재보정한다:
#   MUS_TAB2  = dir 탭 스트립 2번째 탭(mus_seed) — 라벨행 center
#   MUS_FILTER= 이름 필터 InputText 상단행 center
#   MUS_ROW0  = 표행 0의 경로 셀 필터 1행 상태의 d_seed 행
MUS_TAB2_X=${MUS_TAB2_X:-90}    MUS_TAB2_Y=${MUS_TAB2_Y:-77}
MUS_FILTER_X=${MUS_FILTER_X:-250} MUS_FILTER_Y=${MUS_FILTER_Y:-42}
MUS_ROW0_X=${MUS_ROW0_X:-50}    MUS_ROW0_Y=${MUS_ROW0_Y:-132}
FILTER_TEXT="d_seed"

# ---------------------------------------------------------------- fail-closed 가드
if [ -z "${WSL_DISTRO_NAME:-}" ]; then
  echo "MUS-WFAIL: not inside WSL — 실행 축 계약 위반 (wsl.exe -d Ubuntu-24.04 -- bash /mnt/i/progwork/JKENGINE/engine/tools/probes/wsl_music.sh)"
  exit 1
fi
if [ -n "${PHONE_HOST:-}" ]; then
  echo "MUS-NOTE: PHONE_HOST set — WSL 축은 PHONE_HOST 불요(무시하고 진행)"
fi

echo "=== 0. ENTRY 상태 기록 (종료 시 원복 판정용) ==="
ENTRY_UP=0
pgrep -f 'buildwsl/[j]kdesktop' >/dev/null 2>&1 && ENTRY_UP=1
ENTRY_PERM=0
[ -f buildwsl/permissions.json ] && ENTRY_PERM=1
ENTRY_SET=0
[ -f buildwsl/state/settings.json ] && ENTRY_SET=1
ENTRY_SEED=0
[ -d "$SEED_DIR" ] && ENTRY_SEED=1
echo "ENTRY-STATE: server-up=$ENTRY_UP permissions.json=$ENTRY_PERM settings.json=$ENTRY_SET seed-dir=$ENTRY_SEED"

[ "$ENTRY_SEED" -eq 0 ] || FAIL "$SEED_DIR already exists — 이전 probe 잔상, 수동 소각 필요"
[ "$ENTRY_SET" -eq 0 ] || FAIL "buildwsl/state/settings.json already exists — probe는 런타임 settings 파일을 건드릴 수 없다(원문 계약: 전면 allow 런타임 파일은 프로브 소유 아님)"

restore_and_exit() { # 정찰 런 등 중간 탈출 — 복원 다형(스텝 12와 동일 본문)
    echo "=== 복원 (early exit) ==="
    pkill -f 'buildwsl/[j]kdesktop' 2>/dev/null
    sleep 1
    pkill -9 -f 'buildwsl/[j]kdesktop' 2>/dev/null
    sleep 1
    rm -rf "$SEED_DIR"
    [ -d "$SEED_DIR" ] && FAIL "seed dir 소각 실패 — 수동 소각 필요" || echo "SEED-BURIED: $SEED_DIR removed"
    if [ "$ENTRY_SET" -eq 0 ]; then
        rm -f buildwsl/state/settings.json
        echo "SET-BURIED: settings.json removed (ENTRY 부재 원복)"
    fi
    if [ "$ENTRY_PERM" -eq 0 ]; then
        rm -f buildwsl/permissions.json
        echo "PERM-BURIED: permissions.json removed"
    fi
    if [ "$ENTRY_UP" = "1" ]; then
        env DISPLAY=:0 setsid nohup ./buildwsl/jkdesktop --server >"$LOGDIR/mus_srv_restore.log" 2>&1 &
        sleep 6
        pgrep -f 'buildwsl/[j]kdesktop --server' >/dev/null 2>&1 || FAIL "restore boot failed"
        echo "END-STATE: WSL 서버 UP 재부팅 — 진입 상태 원복 완료"
    else
        echo "END-STATE: WSL 서버 DOWN — 진입 상태 원복 완료"
    fi
}

echo "=== 1. bracketed pre-clean (살아있는 jkdesktop은 drvfs 재링크를 막는다) ==="
pkill -f 'buildwsl/[j]kdesktop' 2>/dev/null
sleep 1
pkill -9 -f 'buildwsl/[j]kdesktop' 2>/dev/null
sleep 1
LEFT=$(ps -eo args | grep -i 'buildwsl/[j]kdesktop' | wc -l)
[ "$LEFT" -eq 0 ] || FAIL "pre-clean left $LEFT jkdesktop process(es) alive"

echo "=== 2. ninja rebuild (buildwsl — 풀 로그 리다이렉트; 코드 무변경 no-op 확인 몫) ==="
ninja -C buildwsl -j4 >"$LOGDIR/mus_build.log" 2>&1
B_RC=${PIPESTATUS[0]}
echo "WSL-BUILD-RC=$B_RC"
[ "$B_RC" -eq 0 ] || FAIL "ninja rebuild rc=$B_RC — tail: $(tail -3 "$LOGDIR/mus_build.log" | tr '\n' ' ')"
[ -x buildwsl/jkdesktop ] || FAIL "buildwsl/jkdesktop missing after rebuild"
[ -f buildwsl/jkapp_music.so ] || FAIL "buildwsl/jkapp_music.so missing (T1-T3 배포 확인 실패)"
[ -f buildwsl/jkapp_vplayer.so ] || FAIL "buildwsl/jkapp_vplayer.so missing (위임 대상 배포 확인 실패)"

echo "=== 3. WSL selftest (캐논 $CANON_WSL_SELFTEST — 출력 WSL 내부 리다이렉트) ==="
timeout 420 ./buildwsl/jkdesktop test >"$LOGDIR/mus_st.log" 2>&1
ST_RC=$?
ST_FAIL=$(grep -ac '^\[FAIL\]' "$LOGDIR/mus_st.log")
ST_PASS=$(grep -ac '^\[PASS\]' "$LOGDIR/mus_st.log")
echo "selftest rc=$ST_RC PASS=$ST_PASS FAIL=$ST_FAIL"
grep -aq 'AppSelfTest: 0 failure(s)' "$LOGDIR/mus_st.log" \
    || FAIL "selftest summary missing 'AppSelfTest: 0 failure(s)'"
[ "$ST_FAIL" -eq 0 ] || FAIL "selftest FAIL=$ST_FAIL"
[ "$ST_RC" -eq 0 ] || FAIL "selftest rc=$ST_RC"
[ "$ST_PASS" -ge "$CANON_WSL_SELFTEST" ] \
    || echo "WARN: selftest PASS=$ST_PASS < 캐논 $CANON_WSL_SELFTEST (계보 이탈 — 리포트 원장 기록 필요)"
# music 라인 신설 케이스 (2m 계열): T1-T3가 +27 (progress 원장 등호 근거).
ST2M=$(grep -ac '^\[PASS\] 2m' "$LOGDIR/mus_st.log")
echo "SELFTEST-2M-CASES=$ST2M (music 라인 신설 2m 계열 — 캐논 +27분의 원료)"

echo "=== 4. 합성 WAV 시드 + music.dirs 주입 (엔진 소유 — i:\\@keep 불접촉) ==="
cat > "$LOGDIR/mus_seed.py" <<'PYEOF'
# 시드 4곡 — python3 wave stdlib(의존 0): 44.1kHz mono 16bit 8초 사인.
# 주파수 = 트랙 식별 마크(스펙트럼 구별 가능), 4곡 = 3곡 루트+1곡 sub(D3 재귀
# 실측 몫 — rel열에 "sub/..." 원문 수형 확인). 생성 순서 = 트랙 정렬 계약
# (mtime desc)에서 d_seed가 최신이 되게 한다.
import math, os, struct, sys, wave
rate = 44100
seconds = 8
out = sys.argv[1]
def write_wav(path, freq):
    with wave.open(path, "wb") as f:
        f.setnchannels(1)
        f.setsampwidth(2)
        f.setframerate(rate)
        frames = bytearray()
        for n in range(rate * seconds):
            v = math.sin(2.0 * math.pi * freq * n / rate)
            frames += struct.pack("<h", int((v + 1.0) * 32767 / 2.0 - 16384))
        f.writeframes(bytes(frames))
    print("SEED-OK %s %d bytes" % (path, os.path.getsize(path)))
os.makedirs(out + "/sub", exist_ok=True)
write_wav(out + "/a_seed.wav", 220.0)
write_wav(out + "/b_seed.wav", 330.0)
write_wav(out + "/c_seed.wav", 440.0)
write_wav(out + "/sub/d_seed.wav", 550.0)
PYEOF
python3 "$LOGDIR/mus_seed.py" "$SEED_DIR" || FAIL "seed WAV generation failed (python3)"
SEEDED=$(find "$SEED_DIR" -name '*.wav' | wc -l)
[ "$SEEDED" -eq 4 ] || FAIL "seeded $SEEDED wav files (need 4)"
SEED_BYTES=$(find "$SEED_DIR" -name '*.wav' -printf '%s\n' | awk '{s+=$1} END{print s}')
echo "MUSIC-LIST-EXPECTED=4 (seed 3 root + 1 sub — 표행 수 관측 원문 1행; 표기 일치는 캡처 육안 몫)"

printf '{\n    "music": {\n        "dirs": ["/tmp/mus_seed"]\n    }\n}\n' \
    > buildwsl/state/settings.json
[ -f buildwsl/state/settings.json ] || FAIL "settings.json write failed"
echo "DIRS-WIRE-OK: buildwsl/state/settings.json music.dirs=[/tmp/mus_seed] (END에서 소각)"

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
LOG="$LOGDIR/mus_srv.log"
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

echo "=== 7. launch_app music (윈도우 목록 실측) ==="
L=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"launch_app","args":{"app":"music"}}' 2>/dev/null | grep -a '{' | head -1)
printf '%s' "$L" | grep -aq '"ok":true' || FAIL "launch_app music failed — $L"
echo "launch reply: $L"
MUSWIN=""
WIN=""
for i in 1 2 3 4 5 6 7 8; do
    sleep 2
    WIN=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)
    MUSWIN=$(printf '%s' "$WIN" | grep -aoE '\{"id":[^}]*"title":"Music"[^}]*\}' | head -1)
    [ -n "$MUSWIN" ] && break
done
[ -n "$MUSWIN" ] || FAIL "no Music window in list_windows — last: ${WIN:-none}"
echo "MUSWIN: $MUSWIN"
MUS_ID=$(printf '%s' "$MUSWIN" | sed -n 's/.*"id":\([0-9]*\),.*/\1/p')
MUS_X=$(printf '%s' "$MUSWIN" | sed -n 's/.*"x":\(-\?[0-9]*\),.*/\1/p')
MUS_Y=$(printf '%s' "$MUSWIN" | sed -n 's/.*"y":\(-\?[0-9]*\),.*/\1/p')
MUS_W=$(printf '%s' "$MUSWIN" | sed -n 's/.*"w":\([0-9]*\),.*/\1/p')
MUS_H=$(printf '%s' "$MUSWIN" | sed -n 's/.*"h":\([0-9]*\),.*/\1/p')
echo "MUS-GEO: id=$MUS_ID x=$MUS_X y=$MUS_Y w=$MUS_W h=$MUS_H (meta 560x520 상대 검증)"
sleep 3   # 부팅 스캔(기본 폴더 state/music — 부재) 도착 안정화

# === 캡처기 자가 생성 (wsl_gallery.sh gal_shot.py 원문 — XImage 오프셋 원장 포함) ===
cat > "$SHOT_PY" <<'PYEOF'
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
# XImage 구조체 오프셋(x86_64 — 1차 실측 트랩 원장: 48/56/64는
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
        # WSL 서버 데스크톱 합성 창(전체 뷰). XWayland 배경 창 8192x8192 배제
        # 상한 선례 원문(wsl_gallery 동형).
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

        def chan(v, mask):  # 마스크 비트폭 무관 확장(2차 트랩 원장: 폭=popcount).
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
[ -f "$SHOT_PY" ] || FAIL "capture script generation failed"

RECEIVE="$RECEIVE"
mkdir -p "$RECEIVE"
shot() { # $1 = 영수증 상대명 — /tmp 단열 → engine/tmp 단일 cp
    local NAME="$1" TMP="/tmp/mus_shot_cur.png" SLOG="/tmp/mus_shot_cur.log"
    rm -f "$TMP"
    python3 "$SHOT_PY" "$TMP" >"$SLOG" 2>&1 \
        || FAIL "capture $NAME failed — $(tail -2 "$SLOG" | tr '\n' ' ')"
    grep -aq 'XSHOT wrote' "$SLOG" \
        || FAIL "capture $NAME: no XSHOT wrote — $(tail -2 "$SLOG" | tr '\n' ' ')"
    grep -a 'XSHOT hit\|XSHOT cand' "$SLOG" | sed 's/^/  /'
    cp "$TMP" "$RECEIVE/$NAME" || FAIL "capture copy $NAME failed"
    rm -f "$TMP"
    echo "CAPTURE: $RECEIVE/$NAME ($(wc -c < "$RECEIVE/$NAME") bytes)"
}
tap() { # $1=id $2=x $3=y — 합성 클릭 1탭(send_input — permissions allow 게이트)
    timeout 12 ./buildwsl/jkdesktop agentctl \
        "{\"tool\":\"send_input\",\"args\":{\"op\":\"click\",\"id\":$1,\"x\":$2,\"y\":$3,\"button\":1,\"clicks\":1}}" \
        2>/dev/null | grep -a '{' | head -1
}

echo "=== 8. 탭 전환(mus_seed) → 리스캔 도착 대기 → 리스트 캡처 ==="
CLICK_X=$((MUS_X + MUS_TAB2_X))
CLICK_Y=$((MUS_Y + MUS_TAB2_Y))
echo "CLICK-TAB2: id=$MUS_ID desk=($CLICK_X,$CLICK_Y) — 상수 MUS_TAB2_X=$MUS_TAB2_X MUS_TAB2_Y=$MUS_TAB2_Y"
TCL=$(tap "$MUS_ID" "$CLICK_X" "$CLICK_Y")
printf '%s' "$TCL" | grep -aq '"ok":true' \
    || FAIL "send_input click gate/execute failed — reply: $TCL (permissions.json send_input allow 확인)"
echo "tab click reply: $TCL"
sleep 3   # 탭 전환 재스캔 도착(유일 더티 원 #89) 안정화
shot mus_wsl_list.png

if [ "${MUS_RECON:-0}" = "1" ]; then
    echo "MUS-RECON-EXIT: 리스트 캡처 확보 — 상수 재보정 후 풀 런(MUS_RECON=0)"
    restore_and_exit
    echo "MUSIC-RECON-END"
    exit 0
fi

echo "=== 9. 접착제 콜드 전조 — 등록 전 app_tool open = unknown_app_tool 실측 ==="
OR0=$(timeout 15 ./buildwsl/jkdesktop agentctl \
    '{"tool":"app_tool","args":{"app":"vplayer","tool":"open","args":{"path":"/tmp/mus_seed/a_seed.wav"}}}' \
    2>/dev/null | grep -a '{' | head -1)
echo "MUSIC-GLUE-COLD-REPLY: ${OR0:-none}"
printf '%s' "$OR0" | grep -aq '"unknown_app_tool"' \
    || echo "MUSIC-FAIL(glue-cold: 등록 전 릴레이가 unknown_app_tool이 아님 — T3 콜드 부팅 경기 전조 부정: $OR0)"

echo "=== 10. 이름 필터 관측 (필터 박스 클릭 → 타이핑) → 필터 캡처 ==="
CLICK_X=$((MUS_X + MUS_FILTER_X))
CLICK_Y=$((MUS_Y + MUS_FILTER_Y))
echo "CLICK-FILTER: id=$MUS_ID desk=($CLICK_X,$CLICK_Y) — 상수 MUS_FILTER_X=$MUS_FILTER_X MUS_FILTER_Y=$MUS_FILTER_Y"
FCL=$(tap "$MUS_ID" "$CLICK_X" "$CLICK_Y")
printf '%s' "$FCL" | grep -aq '"ok":true' || FAIL "send_input filter click failed — $FCL"
echo "filter click reply: $FCL"
sleep 1
TYP=$(timeout 12 ./buildwsl/jkdesktop agentctl \
    "{\"tool\":\"send_input\",\"args\":{\"op\":\"type\",\"id\":$MUS_ID,\"text\":\"$FILTER_TEXT\"}}" \
    2>/dev/null | grep -a '{' | head -1)
printf '%s' "$TYP" | grep -aq '"ok":true' || FAIL "send_input type failed — $TYP"
echo "type reply: $TYP"
sleep 2   # 타이핑 = 입력 활동 더티(자연 귀결) — 표 정산(1행) 렌더 도달
shot mus_wsl_filter.png
echo "MUS-FILTER-OBSERVATION: text=$FILTER_TEXT — 표 1행(sub/d_seed.wav)·카운터 1곡 일치는 캡처 육안 몫"

echo "=== 11. 표행 0 더블클릭 → vplayer 위임 (T3 실측 — 콜드 부팅 경기 포함) ==="
CLICK_X=$((MUS_X + MUS_ROW0_X))
CLICK_Y=$((MUS_Y + MUS_ROW0_Y))
echo "CLICK-ROW0: id=$MUS_ID desk=($CLICK_X,$CLICK_Y) — 상수 MUS_ROW0_X=$MUS_ROW0_X MUS_ROW0_Y=$MUS_ROW0_Y (합성 2탭 — ImGui 더블클릭 시간창 0.30s 내 착탄 몫)"
VPWIN=""
OR=""
GS1=""
GS2=""
for PAIR in 1 2 3; do
    C1=$(tap "$MUS_ID" "$CLICK_X" "$CLICK_Y")
    printf '%s' "$C1" | grep -aq '"ok":true' || FAIL "send_input row0 tap1 failed — $C1"
    TAP1_NS=$(date +%s%N)
    C2=$(tap "$MUS_ID" "$CLICK_X" "$CLICK_Y")
    printf '%s' "$C2" | grep -aq '"ok":true' || FAIL "send_input row0 tap2 failed — $C2"
    TAP2_NS=$(date +%s%N)
    echo "dblclick pair $PAIR replies: $C1 | $C2 (tap gap $(( (TAP2_NS - TAP1_NS) / 1000000 )) ms — ImGui 더블클릭 시간창 300ms 대조 몫)"
    for i in 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24; do
        sleep 1
        VP=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)
        VPWIN=$(printf '%s' "$VP" | grep -aoE '\{"id":[^}]*"title":"Video Player"[^}]*\}' | head -1)
        [ -n "$VPWIN" ] && break
    done
    [ -n "$VPWIN" ] && break
    echo "MUSIC-DELEGATE: pair $PAIR 재탭 — 2탭 사이 2s 대기(중복 스폰 스로틀 500ms 회피)"
    sleep 2
done
if [ -n "$VPWIN" ]; then
    echo "MUSIC-DELEGATE: OK — 더블클릭 위임이 Video Player 창을 냈다 (music 클라의 launch_app 쿼리 상통)"
    echo "VPWIN: $VPWIN"
else
    echo "MUSIC-DELEGATE: MISS — 3쌍의 합성 2탭이 표행 더블클릭 핸들에 도달하지 않았다(ImGui 더블클릭 시간창 0.30s vs agentctl 왕복 — EYES 승계 사유 원장)"
fi

echo "=== 12. 위임 open 도착 귀속 — 직행 open 없이 get_status 고밀도 폴링 ==="
# attribution: 이 폴링 창엔 probe의 직행 open이 없다 — opened:true 전이는
# **music 위임의 open**(재청구 폴백 포함)만이 만들 수 있다. 창 출현 직후
# 캡처 1장(실패 분류 문구는 vplayer UI 상태행 — statusText 빨강) + 0.4s 폴링
# (1s 간격은 흡수 1프레임(16ms) 전이를 놓친다 — v2 실측 렛슨).
VPWIN_ID=$(printf '%s' "$VPWIN" | sed -n 's/.*"id":\([0-9]*\),.*/\1/p')
sleep 1
shot mus_wsl_vp_delegate.png
DELEG_OPEN=0
for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16; do
    GS=$(timeout 15 ./buildwsl/jkdesktop agentctl \
        '{"tool":"app_tool","args":{"app":"vplayer","tool":"get_status","args":{}}}' \
        2>/dev/null | grep -a '{' | head -1)
    echo "MUSIC-DELEGATE-GET-STATUS-$i: ${GS:-none}"
    printf '%s' "$GS" | grep -aq '"opened":true' && { DELEG_OPEN=1; break; }
    sleep 0.4
done
if [ "$DELEG_OPEN" -eq 1 ]; then
    echo "MUSIC-DELEGATE-OPEN: OK — 위임 open이 vplayer에 도달해 열었다(재청구 폴백 포함)"
else
    echo "MUSIC-DELEGATE-OPEN: MISS — 창 출현 후 ~7s 간 opened:true 전이 없음. 원인 진원(서버 즉답 경로 JKWindowServer :6747-6749 — sync 거부 답신의 봉투 ok=고정 1): 콜드 open의 sync unknown_app_tool 거부가 봉투 ok=1로 회송되고 PollReplies의 reply.ok 분류(ClientMusicApp :393-398)가 그것을 성공(" '"vplayer 재생 요청됨"' ")으로 읽어 재청구 크레딧을 0으로 소멸 — 폴백이 자기 판정과 모순되지 않고 자기 소멸. 리포트 원장(진원·고침 후보) 참조"
fi

echo "=== 13. music status 표기 관측 — vplayer 창을 치우고 캡처 ==="
MV=$(timeout 12 ./buildwsl/jkdesktop agentctl \
    '{"tool":"window_move","args":{"id":'"$(printf '%s' "$VPWIN" | sed -n 's/.*"id":\([0-9]*\),.*/\1/p')"', "x":820,"y":100}}' \
    2>/dev/null | grep -a '{' | head -1)
echo "window_move (vplayer 치우기 — music status 행 노출): ${MV:-none}"
sleep 2
shot mus_wsl_delegate_status.png

echo "=== 14. 위임 접착제 직행 — app_tool open(get_status 재생 수치) ==="
OPEN_PATH=/tmp/mus_seed/sub/d_seed.wav
OR=$(timeout 15 ./buildwsl/jkdesktop agentctl \
    "{\"tool\":\"app_tool\",\"args\":{\"app\":\"vplayer\",\"tool\":\"open\",\"args\":{\"path\":\"$OPEN_PATH\"}}}" \
    2>/dev/null | grep -a '{' | head -1)
echo "MUSIC-APP-OPEN-REPLY: $OR"
printf '%s' "$OR" | grep -aq '"accepted":true' \
    || echo "MUSIC-FAIL(app_tool open not accepted — $OR)"
sleep 3   # open 비동기 — 워커 bring-up(10s 상한보다 짧아야 정상)
GS2=$(timeout 15 ./buildwsl/jkdesktop agentctl \
    '{"tool":"app_tool","args":{"app":"vplayer","tool":"get_status","args":{}}}' \
    2>/dev/null | grep -a '{' | head -1)
echo "MUSIC-GET-STATUS-1: $GS2"
sleep 3
GS3=$(timeout 15 ./buildwsl/jkdesktop agentctl \
    '{"tool":"app_tool","args":{"app":"vplayer","tool":"get_status","args":{}}}' \
    2>/dev/null | grep -a '{' | head -1)
echo "MUSIC-GET-STATUS-2: $GS3 (pos 증가 = 재생 진행 원문 1행)"

echo "=== 15. stderr 진단 수취 (서버 로그 — 클라 스폰+vplayer 진단) ==="
grep -a 'spawn\|music\|vplayer\|\[vplayer\]' "$LOG" | tail -14 | sed 's/^/  /'

echo "=== 16. 위임 후 모양 캡처 → mus_wsl_after.png ==="
shot mus_wsl_after.png

restore_and_exit

echo "=== 17. REMNANT — /tmp 자가 스크립트·중간파일 소각 ==="
LEFTS=$(ls "$LOGDIR" 2>/dev/null | wc -l)
REMNANT_LIST=$(ls "$LOGDIR")
echo "REMNANT-COUNT=$LEFTS (서버·빌드·셀프테스트·shot 로그만 잔존 — 원장 수형)"
echo "$REMNANT_LIST" | sed 's/^/  /'

echo "=== 18. music 실측 판정 ==="
CAP_OK=1
for f in list filter vp_delegate delegate_status after; do
    P="$RECEIVE/mus_wsl_$f.png"
    if [ ! -s "$P" ]; then
        echo "MUSIC-FAIL(capture missing/empty: $P)"
        CAP_OK=0
    fi
done
DEL_OK=0
[ -n "${VPWIN:-}" ] && DEL_OK=1
GLUE_OK=0
printf '%s' "${OR:-}" | grep -aq '"accepted":true' && GLUE_OK=1
PLAY_OK=0
printf '%s' "${GS2:-}${GS3:-}" | grep -aq '"opened":true' && PLAY_OK=1
if [ "$CAP_OK" -eq 1 ] && [ "$DEL_OK" -eq 1 ] && [ "$GLUE_OK" -eq 1 ] && [ "$PLAY_OK" -eq 1 ]; then
    echo "MUSIC-VERDICT: MUSIC-OK(WSL 빌드 rc=$B_RC·selftest FAIL=$ST_FAIL 계보 PASS=$ST_PASS(캐논 $CANON_WSL_SELFTEST) 2m=$ST2M·delegate=창 출현(deleg-open=$DELEG_OPEN)·glue=accepted·get_status opened=true — 육안 스탭 대기)"
else
    echo "MUSIC-VERDICT: MUSIC-FAIL(행별 사유는 위 각 행 — delegate=$DEL_OK deleg-open=$DELEG_OPEN glue=$GLUE_OK play=$PLAY_OK captures=$CAP_OK; 인프라 실패는 hard FAIL로 상단 중단, 수치·육안 미달은 rc=0 honest-fail)"
fi
echo "MUSIC-WCAPTURES:"
for f in list filter vp_delegate delegate_status after; do
    P="$RECEIVE/mus_wsl_$f.png"
    [ -s "$P" ] && echo "  $P ($(wc -c < "$P") bytes)"
done
echo "MUSIC-END"
exit 0
# honest-fail 원칙: 수치·육안 대행 미달(MUSIC-FAIL 라인·DELEGATE MISS)은 원장
# 목적이라 rc=0. hard FAIL(가드·빌드·selftest·시드·settings 주입·부팅·ping·
# launch·창·캡처·복원 실패)만 exit 1.
# EOF — 끝 개행 유지.