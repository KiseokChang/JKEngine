#!/usr/bin/env bash
# T4 WSL music 실측 probe (스펙 2026-10-09-music-library — plan task-4).
#
# T3 확장 (plan 2026-10-10-music-dirs-ui, 2026-10-10):
#   * 16b dirs 도구 세그먼트 — music_dir_add/remove/list(등록 6종 — spatial
#     3종 병합 유지 원존)의 settings.json 실측 쓰기: 합성 폴더($TMPDIR) 등록
#     →list 원문→settings.json 원문(music.dirs 실기록+기존 audio/retention 키
#     보존 — C1 보존의 라이브 수취)→제거→원복 바이트 등호(④ T5 수형 — seed를
#     ComposeKeyed 출하형 compact으로 심어 등호 판정 가능).
#   * 16c I-1 라이브 실측(T2 리뷰 M-1r) — CP949 혼입(quickjs 부적합) 원문
#     시딩 → 부팅 stderr 경고 1행 → 쓰기 정문(settings_set) 거부 detail 원문
#     → 저장소 스캐너 수용(remove 1콜 — CP949 문서에서 dirs 소각) →
#     [패널 제거 치유 루트](UI [폴더 관리] 토글→[제거] 클릭 — 도구 인자는
#     quickjs JSON 문자열 = UTF-8 검사라 CP949 바이트 수송 불가인 원문 근거
#     동봉) → 치유 후 music_dir_add 재시도+settings_set 성공(부팅 재기동
#     불요) → 백업 원복 바이트 등호. 서버 재기동 1회(부팅 경고 캡처).
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
#   * spatial leg 변형(T3 신설 — 스펙 2026-10-10-music-spatial-leg): leg 런타임
#     실측의 최초 동작 실측이 되는 세그먼트. SPATIAL_PLAYER_ROOT env는
#     **호출자 전달 축**으로만 쓴다(커밋 본문에 경로 리터럴을 쓰지 않는
#     계약 — wsl.exe -- env SPATIAL_PLAYER_ROOT=<spatial-player 루트> bash ...).
#       LEG_MODE=1(설정) — scratch(/tmp/mus_scratch) cmake+build로
#         jkapp_music.so를 leg 있는 빌드로 만들어 buildwsl에 배포한다
#         (시차 배치 — buildwsl 빌드/셀프테스트가 끝난 뒤 scratch 빌드.
#         동시 -j는 공유 트리 I/O 경합 fail 실측 원장). END에서 relink+
#         scratch 소각으로 원복(relink 후 AL 심볼 0 — 원복 강도 영수증).
#       LEG_MODE=0(미설정) — leg-less .so 그대로: spatial_play는
#         DeviceFailed 종착(kDelegationHint 원문 — fail-closed 영수증).
#     어느 변형에서든 ALC 디바이스가 실패하면(WSLg 환경 가능성) start_failed
#     detail=kDelegationHint의 라벨 표기가 **정상 영수증**이다 —
#     LEG-FALLBACK-OK(폰 leg는 T4 몫).
#   * 초장 경로 트랙(M-1 승계 — T2 review: spatial_status의 절단 정직 가드는
#     path 이스케이프 768B 버퍼 초과 축에서만 도달): ~1KB 경로의 트랙 1건을
#     시드하고 spatial_status가 truncated:true(정직 축소 전문)를 내는 원문을
#     실측한다. 단 truncation은 legState_.path가 채워진 뒤(디코더 open 성공)
#     봉합이므로, ALC 실패 런(path 미채움)에서는 MISS 원장이 정상이다.
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
CANON_WSL_SELFTEST=608   # 캐논 WSL 축 (spatial leg 라인 등호 승계 — T1 591+2n 17, T2 fix r1 3축 재실측 608 리뷰어 독립 확증)
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

# [T3] I-1 패널 치유 정찰 상수(env 오버라이드 — 정찰 캡처 재보정 선례).
# 라이브 캘리브레이션(2026-10-10 — 클릭 차분×툴팁 증거 캡처 engine/tmp/cal_*)
# : 합성 캡처 = 클라 + (417,173); [폴더 관리] 토글 = 클라 (130,70) — 패널
# 열림+토글 툴팁이 그 마우스 자리에서 뜬 캡처(cal_510_170.png)로 증명; 상태행
# 감김(+17px 시프트) 상태의 (160,77) 클릭은 토글 x존(~91-133) 오른쪽 밖(
# 클릭-트랩 원장). 패널 첫 행 [제거] SmallButton = 클라 (86,118) 실증 치유 콜
# — 이웃 격자 폴백. 클릭 판정: 토글 = 캡처 차분, [제거] = settings.json
# 바이트(무음 실패 금지 원장).
MUS_TOGGLE_TWEAK_X=${MUS_TOGGLE_TWEAK_X:-0}
MUS_TOGGLE_X=${MUS_TOGGLE_X:-130}
MUS_TOGGLE_Y=${MUS_TOGGLE_Y:-70}
MUS_REMOVE_XS=${MUS_REMOVE_XS:-"86 90 94 98"}
MUS_REMOVE_YS=${MUS_REMOVE_YS:-"118 123 113 128"}

# spatial leg 변형 디스패치(SPATIAL_PLAYER_ROOT env — 위 원문). 경로 원문은
# 로그에도 무표기(엔진 tmp 로그 세척 원장 — 값이 아니라 상태만 기록한다).
LEG_MODE=0
case "${SPATIAL_PLAYER_ROOT:-}" in
    "" )
        echo "MUS-LEG-ENV: unset — fail-closed 변형 런(leg-less .so — spatial_play는 DeviceFailed 종착 전이 원문)" ;;
    /* )
        [ -f "$SPATIAL_PLAYER_ROOT/CMakeLists.txt" ] \
            || FAIL "SPATIAL_PLAYER_ROOT set but no CMakeLists.txt at its root(경로 무표기 원칙 — 호출자 확인 몫)"
        LEG_MODE=1
        echo "MUS-LEG-ENV: set — scratch 빌드 변형 런(env 설정 빌드를 배포한다)" ;;
    * )
        FAIL "SPATIAL_PLAYER_ROOT는 WSL 절대경로(/mnt/... 축)로 전달하지 않은 것 같다(값 무표기 — 호출자 재확인)" ;;
esac
SCRATCH=${MUS_SCRATCH:-/tmp/mus_scratch}   # scratch 빌드 dir — END에서 소각
LONG_SEG=$(printf 'x%.0s' $(seq 1 125))   # 125자 세그먼트(컴포넌트 255 상한 안전)

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
    if [ "$LEG_MODE" -eq 1 ]; then
        # spatial leg 배포 원복(T3): leg-less .so로 relink — 재링크 뒤 AL 심볼
        # 0이 원복 강도의 영수증이다(0이 아니면 leg 잔상 = hard FAIL).
        rm -f buildwsl/jkapp_music.so
        ninja -C buildwsl jkapp_music.so >"$LOGDIR/mus_relink.log" 2>&1 \
            || FAIL "buildwsl jkapp_music.so relink failed — tail: $(tail -3 "$LOGDIR/mus_relink.log" | tr '\n' ' ')"
        RELINK_SYM=$(ALCOUNT buildwsl/jkapp_music.so)
        echo "MUS-LEG-RESTORE-SYM: alcOpenDevice refs=$RELINK_SYM (0 = leg-less 원복 성립)"
        [ "$RELINK_SYM" -eq 0 ] || FAIL "relinked jkapp_music.so still carries AL symbols($RELINK_SYM) — 원복 불충분"
        rm -rf "$SCRATCH"
        [ -d "$SCRATCH" ] && FAIL "scratch dir 소각 실패 — 수동 소각 필요" \
            || echo "SCRATCH-BURIED: scratch dir removed (트리 잔산 0)"
    fi
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

# === 3b. spatial 변형 빌드+배포 (T3 — 시차 배치 원장 준수) ===
# 2n selftest는 main.cpp 하네스 TU 소비(빌드wsl 설정은 env 미설정으로 고착)
# 이라 셀프테스트는 배포와 무관 — 배포 뒤 재실측 불요(T1 원문 계보 유지).
echo "=== 3b. spatial leg 빌드+배포 (LEG_MODE=$LEG_MODE — buildwsl 빌드 완료 뒤 scratch — 시차 배치 원장) ==="
COUNTER() { sed 's|/mnt/[^ ]*|<WSLPATH>|g'; }   # 로그 tail 열람 경로 위생 세척 원장
ALCOUNT() { # $1=.so — AL 심볼 개수(nm symtab 우선 .symtab 부재 시 dynsym 폴백)
    { nm "$1" 2>/dev/null || nm -D "$1" 2>/dev/null; } | grep -ac 'alcOpenDevice'
}
if [ "$LEG_MODE" -eq 0 ]; then
    UNSET_SYM=$(ALCOUNT buildwsl/jkapp_music.so)
    echo "MUS-LEG-UNSET-SYM: alcOpenDevice refs=$UNSET_SYM (0 = leg 부재 fail-closed 영수증)"
    [ "$UNSET_SYM" -eq 0 ] || FAIL "env 미설정 buildwsl jkapp_music.so에 AL 심볼($UNSET_SYM) — fail-closed 배선 위반"
else
    rm -rf "$SCRATCH"
    echo "SPATIAL-SCRATCH: /tmp/mus_scratch (소스=engine, 경로 env 전달 무표기)"
    env SPATIAL_PLAYER_ROOT="$SPATIAL_PLAYER_ROOT" \
        GIT_CONFIG_COUNT=1 GIT_CONFIG_KEY_0=safe.directory GIT_CONFIG_VALUE_0='*' \
        cmake -S . -B "$SCRATCH" -G Ninja >"$LOGDIR/mus_spatial_config.log" 2>&1
    C_RC=${PIPESTATUS[0]}
    echo "SPATIAL-CONFIG-RC=$C_RC"
    [ "$C_RC" -eq 0 ] || FAIL "scratch configure rc=$C_RC — tail: $(COUNTER < <(tail -3 "$LOGDIR/mus_spatial_config.log") | tr '\n' ' ')"
    grep -aq 'audio_core\|openal-soft' "$LOGDIR/mus_spatial_config.log" \
        && echo "SPATIAL-CONFIG-SIGNAL: audio_core/openal 흡수 확인(전문은 /tmp 로그만)" \
        || echo "WARN: spatial config log에 audio_core/openal 표식 0건 — 배선 재확인 몫"
    env SPATIAL_PLAYER_ROOT="$SPATIAL_PLAYER_ROOT" \
        GIT_CONFIG_COUNT=1 GIT_CONFIG_KEY_0=safe.directory GIT_CONFIG_VALUE_0='*' \
        ninja -C "$SCRATCH" jkapp_music.so -j4 >"$LOGDIR/mus_spatial_build.log" 2>&1
    B2_RC=${PIPESTATUS[0]}
    echo "SPATIAL-BUILD-RC=$B2_RC"
    [ "$B2_RC" -eq 0 ] || FAIL "scratch jkapp_music.so build rc=$B2_RC — tail: $(COUNTER < <(tail -3 "$LOGDIR/mus_spatial_build.log") | tr '\n' ' ')"
    [ -f "$SCRATCH/jkapp_music.so" ] || FAIL "scratch jkapp_music.so missing after build"
    SET_SYM=$(ALCOUNT "$SCRATCH/jkapp_music.so")
    echo "MUS-LEG-SYM-SET: alcOpenDevice refs=$SET_SYM (>=1 = leg 설정 축 영수증)"
    [ "$SET_SYM" -ge 1 ] || FAIL "env 설정 jkapp_music.so에 AL 심볼 0건 — leg 배선 빠짐"
    cp "$SCRATCH/jkapp_music.so" buildwsl/jkapp_music.so \
        || FAIL "deploy scratch jkapp_music.so -> buildwsl failed"
    DEP_SYM=$(ALCOUNT buildwsl/jkapp_music.so)
    echo "MUS-LEG-DEPLOY: buildwsl/jkapp_music.so <- scratch (배포 뒤 AL 심볼 refs=$DEP_SYM — END relink 원복)"
fi

echo "=== 4. 합성 WAV 시드 + music.dirs 주입 (엔진 소유 — i:\\@keep 불접촉) ==="
cat > "$LOGDIR/mus_seed.py" <<'PYEOF'
# 시드 5곡(+mp3 가변) — python3 wave stdlib(의존 0): 44.1kHz mono 16bit 8초
# 사인. 주파수 = 트랙 식별 마크(스펙트럼 구별 가능), 4곡 = 3곡 루트+1곡 sub(D3
# 재귀 실측 몫 — rel열에 "sub/..." 원문 수형 확인)+1곡 초장 경로(M-1 승계 —
# ~1KB 경로: 7단 × 132자 세그먼트, 컴포넌트 255·PATH_MAX 상한 안전). 생성
# 순서 = 트랙 정렬 계약(mtime desc)에서 l_seed가 최신이 되게 한다.
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
deep_file = sys.argv[2]
os.makedirs(os.path.dirname(deep_file), exist_ok=True)
write_wav(deep_file, 660.0)
PYEOF
LONG_PATH="$SEED_DIR"
for i in 1 2 3 4 5 6 7; do LONG_PATH="$LONG_PATH/deep_${i}_${LONG_SEG}"; done
LONG_TRACK="$LONG_PATH/l_seed.wav"
LONG_LEN=$(printf '%s' "$LONG_TRACK" | wc -c)
echo "MUS-LONGTRACK-LEN=$LONG_LEN (M-1 승계 — >=700B 요구)"
[ "$LONG_LEN" -ge 700 ] || FAIL "long track path $LONG_LEN B < 700 (M-1 truncation 축 미달)"
python3 "$LOGDIR/mus_seed.py" "$SEED_DIR" "$LONG_TRACK" \
    || FAIL "seed WAV generation failed (python3)"
echo "=== 4b. mp3 합성 여부 실측 (ffmpeg+libmp3lame — 플랜 원문; 부재면 wav 단축) ==="
if command -v ffmpeg >/dev/null 2>&1; then
    if ffmpeg -loglevel error -f lavfi -i "sine=frequency=880:sample_rate=44100" \
        -t 6 -codec:a libmp3lame -b:a 128k "$SEED_DIR/m_seed.mp3" \
        >"$LOGDIR/mus_mp3.log" 2>&1 && [ -s "$SEED_DIR/m_seed.mp3" ]; then
        echo "MUS-AUDIO-MP3: ffmpeg+libmp3lame 합성 OK ($(wc -c < "$SEED_DIR/m_seed.mp3") bytes — durSec 미상(M-2) 원문 관측 몫)"
    else
        echo "MUS-AUDIO-MP3: ffmpeg 있음·libmp3lame 인코딩 실패 — wav 단축 계속 ($(tail -1 "$LOGDIR/mus_mp3.log"))"
    fi
else
    echo "MUS-AUDIO-MP3: ffmpeg 부재(T1/T4 원문 승계) — wav 단축 계속(플랜 원문 수형)"
fi
SEEDED=$(find "$SEED_DIR" \( -name '*.wav' -o -name '*.mp3' \) | wc -l)
EXPECT_SEEDS=5
[ -f "$SEED_DIR/m_seed.mp3" ] && EXPECT_SEEDS=6
[ "$SEEDED" -eq "$EXPECT_SEEDS" ] || FAIL "seeded $SEEDED media files (need $EXPECT_SEEDS)"
SEED_BYTES=$(find "$SEED_DIR" -name '*.wav' -printf '%s\n' | awk '{s+=$1} END{print s}')
echo "MUSIC-LIST-EXPECTED=$EXPECT_SEEDS (seed 3 root + 1 sub + 1 초장 경로[+1 mp3 if any] — 표행 수 관측 원문 1행; 표기 일치는 캡처 육안 몫)"

# [T3] seed를 ComposeKeyed 출하형(compact)으로 — 관리 3키의 audio/retention 원문
# 슬라이스를 심어 C1 보존의 라이브 수취 대상으로 쓴다(16b) + 말미 개행 없음
# (원자적 쓰기가 데이터 크기만큼만 기록 — ④ 등호 판정 축). 스냅샷 = 16b/16c의
# 백업 원문(T5 수형 — 백업+원복 바이트 등호; END에서 소각).
SNAP="$LOGDIR/mus_settings_snap.json"
printf '{"audio": {"mute": 0, "volume": 50},"retention": {"days": 14},"music":{"dirs":["/tmp/mus_seed"]}}' \
    > buildwsl/state/settings.json
[ -f buildwsl/state/settings.json ] || FAIL "settings.json write failed"
cp buildwsl/state/settings.json "$SNAP" || FAIL "settings.json snapshot failed"
echo "DIRS-WIRE-OK: buildwsl/state/settings.json music.dirs=[/tmp/mus_seed]+audio/retention (백업 스냅샷=$SNAP — END에서 소각)"

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

# ==================================================================
# 8b. spatial leg 실측 (T3 신설 — 스펙 2026-10-10 §2, 도구 3종 원문 계약)
# ==================================================================
atool() { # $1=tool $2=args-json — music 앱 직행(도구 허브 릴레이 원문 — music 창
          #   등록 전제이므로 이 세그먼트는 launch_app music 뒤에만 온다)
    timeout 15 ./buildwsl/jkdesktop agentctl \
        "{\"tool\":\"app_tool\",\"args\":{\"app\":\"music\",\"tool\":\"$1\",\"args\":$2}}" \
        2>/dev/null | grep -a '{' | head -1
}
spstatus() { sleep 1.5; atool spatial_status '{}'; }
jpos() { # status JSON에서 pos 수치만 뽑는다
    printf '%s' "$1" | sed -n 's/.*"pos":\([0-9][0-9.eE+\-]*\).*/\1/p'
}

echo "=== 8b. spatial leg 실측 (LEG_MODE=$LEG_MODE — app_tool 직행 릴레이) ==="
ST0=$(atool spatial_status '{}')
echo "MUS-SPATIAL-STATUS-0: ${ST0:-none} (재생 전 기저 — active:false·path 빈 원문)"
PLAY1=$(atool spatial_play "{\"path\":\"$SEED_DIR/a_seed.wav\"}")
echo "MUS-SPATIAL-PLAY-REPLY: ${PLAY1:-none}"
sleep 1   # open(디코더+최초 ALC lazy — 프레임 스레드 수십 ms급 I-3 정직 원장)
S1=$(atool spatial_status '{}')
echo "MUS-SPATIAL-STATUS-1: ${S1:-none}"
S2=$(spstatus)
echo "MUS-SPATIAL-STATUS-2: ${S2:-none} (pos 증가 = 재생 진행 원문 1행)"
POS1=$(jpos "$S1"); POS2=$(jpos "$S2")
POS_DELTA=""
if [ -n "$POS1" ] && [ -n "$POS2" ]; then
    POS_DELTA=$(awk -v a="$POS1" -v b="$POS2" 'BEGIN{printf "%.3f", b - a}')
fi
echo "MUS-SPATIAL-POS: $POS1 -> $POS2 (delta=$POS_DELTA — 원문 등호는 리포트 몫)"
shot mus_wsl_spatial.png   # spatial 패널(진행 bar·모드·디바이스 표기) — EYES 캡처

STP=$(atool spatial_stop '{}')
echo "MUS-SPATIAL-STOP-REPLY: ${STP:-none} (idempotent — idle leg도 {\"active\":false,\"idle\":true} 원문)"
ST3=$(atool spatial_status '{}')
echo "MUS-SPATIAL-STATUS-3(post-stop): ${ST3:-none} (active:false·직전 pos/path·deviceOk 보존 원문)"

M1_TRUNC=0
if [ "$LEG_MODE" -eq 1 ]; then
    echo "--- 8b-2. 초장 경로 트랙 — M-1 절단 정직 가드 원문 실측 ---"
    echo "MUS-LONGTRACK: len=$LONG_LEN (경로 원문 무단열 — /tmp/mus_seed 아래 deep_*)"
    PLAY2=$(atool spatial_play "{\"path\":\"$LONG_TRACK\"}")
    echo "MUS-SPATIAL-PLAY-LONG-REPLY: ${PLAY2:-none}"
    sleep 1.5
    SL1=$(atool spatial_status '{}')
    echo "MUS-SPATIAL-STATUS-LONG: ${SL1:-none}"
    printf '%s' "$SL1" | grep -aq '"truncated":true' \
        && { M1_TRUNC=1; echo "MUS-M1-TRUNCATED: OK — 절단 시 path 생략+truncated:true 정직 축소 전문 원문 성립"; } \
        || echo "MUS-M1-TRUNCATED: MISS(need>=768 미도달 또는 재생 성립 전 고장 — 사유는 위 PLAY-LONG·STATUS-LONG 원문)"
    STP2=$(atool spatial_stop '{}')
    echo "MUS-SPATIAL-STOP-LONG: ${STP2:-none}"
    if [ -f "$SEED_DIR/m_seed.mp3" ]; then
        echo "--- 8b-3. mp3 트랙 — durSec 미상(M-2) 원문 관측 ---"
        PLAY3=$(atool spatial_play "{\"path\":\"$SEED_DIR/m_seed.mp3\"}")
        echo "MUS-SPATIAL-PLAY-MP3-REPLY: ${PLAY3:-none}"
        sleep 1.5
        SM=$(atool spatial_status '{}')
        echo "MUS-SPATIAL-STATUS-MP3: ${SM:-none} (dur:0.000 = 길이 미상 원문 — 진행 표기는 캡처 육안 몫)"
        STP3=$(atool spatial_stop '{}')
        echo "MUS-SPATIAL-STOP-MP3: ${STP3:-none}"
    fi
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

# ==================================================================
# 16b. [T3 — dirs 도구 세그먼트] music_dir_add/remove/list — music.dirs의
#      settings.json 실측 쓰기(등록 6종 — spatial 3종 병합 유지 원존)+기존
#      audio/retention 키 보존 원문+add/remove 왕복 후 원복 바이트 등호(④ —
#      T5 수형). 도구 경로만(클릭 없음 — UI 패널은 16c 치유 루트에서 1회).
# ==================================================================
SETTINGS=buildwsl/state/settings.json
echo "=== 16b. dirs 도구 세그먼트 (music_dir_add/remove/list — settings 쓰기) ==="
DL0=$(atool music_dir_list '{}')
echo "MUSIC-DIR-LIST-0: ${DL0:-none} (시딩 기저 원문 — /tmp/mus_seed 단일)"
[ -d /tmp/mus_dirs_x ] || mkdir -p /tmp/mus_dirs_x || FAIL "synthetic dir create failed"
DADD=$(atool music_dir_add '{"path":"/tmp/mus_dirs_x"}')
echo "MUSIC-DIR-ADD-REPLY: ${DADD:-none}"
DL1=$(atool music_dir_list '{}')
echo "MUSIC-DIR-LIST-1: ${DL1:-none} (등록 인증 원문 1행)"
printf '%s' "$DADD" | grep -aq '"ok":true' \
    || echo "MUSIC-FAIL(dir-add: ok 수취 실패 — $DADD)"
printf '%s' "$DL1" | grep -aq 'mus_dirs_x' \
    || echo "MUSIC-FAIL(dir-list-1: 합성 폴더 등록 미반영 — $DL1)"
echo "MUSIC-DIR-SETTINGS-1: $(cat "$SETTINGS") (settings.json 원문 실측 — 등록 후 파일)"
SETCHK=$(python3 - "$SETTINGS" <<'PYEOF'
import json, sys
with open(sys.argv[1], "rb") as f:
    obj = json.loads(f.read().decode("utf-8"))
dirs = obj.get("music", {}).get("dirs", [])
ok = ("audio" in obj and "retention" in obj
      and "/tmp/mus_dirs_x" in dirs and "/tmp/mus_seed" in dirs)
print("SET-OK" if ok else "SET-MISS audio=%s retention=%s dirs=%s"
      % ("audio" in obj, "retention" in obj, dirs))
PYEOF
) || SETCHK="SET-MISS(python3 파서 실패)"
echo "MUSIC-DIR-SETTINGS-CHECK: $SETCHK (독립 파서 수취 — music.dirs 실제 기록+기존 audio/retention 키 보존)"
printf '%s' "$SETCHK" | grep -aq 'SET-OK' \
    || echo "MUSIC-FAIL(dir-settings: 기록/보존 원문 미달 — $SETCHK)"
DREM=$(atool music_dir_remove '{"path":"/tmp/mus_dirs_x"}')
echo "MUSIC-DIR-REMOVE-REPLY: ${DREM:-none}"
DL2=$(atool music_dir_list '{}')
echo "MUSIC-DIR-LIST-2: ${DL2:-none} (제거 인증 원문 1행)"
printf '%s' "$DREM" | grep -aq '"ok":true' \
    || echo "MUSIC-FAIL(dir-remove: ok 수취 실패 — $DREM)"
if printf '%s' "$DL2" | grep -aq 'mus_dirs_x'; then
    echo "MUSIC-FAIL(dir-list-2: 제거 미반영 — $DL2)"
else
    echo "MUSIC-DIR-GONE: mus_dirs_x 소각 확인 (제거 원문 성립)"
fi
if cmp -s "$SETTINGS" "$SNAP"; then
    echo "MUSIC-DIR-BYTE-RESTORE: OK — add/remove 왕복 == seed 원문 바이트 등호(④ T5 수형)"
else
    echo "MUSIC-FAIL(dir-byte-restore: 왕복 후 원문 불일치)"
    echo "  now : $(cat "$SETTINGS")"
    echo "  snap: $(cat "$SNAP")"
fi
rmdir /tmp/mus_dirs_x || echo "MUSIC-FAIL(dir-bury: /tmp/mus_dirs_x 소각 실패 — 수동 소각)"

# ==================================================================
# 16c. [T3 — I-1 라이브 실측 (T2 리뷰 M-1r)] 부적합(CP949 혼입) 문서 시딩 →
#      부팅 stderr 경고 1행 → 쓰기 정문(settings_set) 거부 detail 원문 →
#      저장소 스캐너 수용(remove 1콜 — CP949 문서에서 dirs 소각) →
#      [패널 제거 치유 루트] — CP949 항목의 in-엔진 치유는 UI [제거] 원촉이
#      유일(도구 인자는 quickjs JSON 문자열 = UTF-8 검사라 CP949 바이트 수송
#      불가 — 16c-4에서 bad_args/파서 거부 원문 실측) — T2 리뷰 I-1 §(1)
#      "music 패널에서 CP949 항목 제거 → 문서 순수 회귀" 원문) → 치유 후
#      music_dir_add 재시도 성공+settings_set 성공(부팅 재기동 불요 — 이후
#      쓰기 정상) → 백업 원복 바이트 등호.
#      * 전각 치유(CP949 변환)·Win 축 pid tmp 출하는 라인 별도(원장).
# ==================================================================
echo "=== 16c. I-1 라이브 (CP949 혼입 시딩 → 거부 detail 원문 → 패널 치유) ==="
CP949_FIX=$(printf '\xB0\xA1\xBF\xE4')   # 2o-f corpus 동형 — CP949 '가요'
PAT="$LOGDIR/mus_cp949.pat"
printf '%s' "$CP949_FIX" > "$PAT"
CPENTRY="$CP949_FIX"
CP_HERE() { grep -aq -F -f "$PAT" "$SETTINGS"; }
printf '{"audio": {"mute": 0, "volume": 50},"retention": {"days": 14},"music":{"dirs":["%s"]}}' \
    "$CP949_FIX" > "$SETTINGS"
CP_HERE || FAIL "CP949 seed write failed"
echo "MUSIC-I1-SEEDED: OK — settings.json을 부적합(CP949 혼입) 원문으로 교체(백업 원문=$SNAP — 원복은 바이트 등호로 증명)"

echo "--- 16c-1. 서버 재기동 1회 (부팅 stderr 경고 캡처용 — 브래킷 pkill 재용) ---"
pkill -f 'buildwsl/[j]kdesktop' 2>/dev/null
sleep 1
pkill -9 -f 'buildwsl/[j]kdesktop' 2>/dev/null
sleep 1
rm -f /tmp/JKWindowServerPipe.sock
I1LOG="$LOGDIR/mus_srv_i1.log"
env DISPLAY=:0 setsid nohup ./buildwsl/jkdesktop --server >"$I1LOG" 2>&1 &
sleep 6
SRV2=$(pgrep -f 'buildwsl/[j]kdesktop --server' | head -1)
[ -n "$SRV2" ] || FAIL "no server after I-1 restart — log tail: $(tail -3 "$I1LOG" | tr '\n' ' ')"
echo "server pid=$SRV2"
I1_BOOT=0
I1BOOT_WARN=$(grep -a 'settings.json 파싱 실패 — 기본값으로 기동' "$I1LOG" | head -1)
echo "MUSIC-I1-BOOT-WARN: ${I1BOOT_WARN:-MISS}"
[ -n "$I1BOOT_WARN" ] && { I1_BOOT=1; echo "MUS-I1-BOOTW: OK — 부팅 stderr 경고 원문 1행 실측(파일 무접촉 · 기본값 기동 표기 포함)"; } \
    || echo "MUSIC-FAIL(i1-boot-warn: 부팅 경고 미검출 — $I1LOG)"
P2=""
for i in 1 2 3 4 5; do
    P2=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"ping","args":{}}' 2>/dev/null | grep -a '{' | head -1)
    printf '%s' "$P2" | grep -aq '"ok":true' && break
    sleep 2
done
printf '%s' "$P2" | grep -aq '"ok":true' || FAIL "ping after I-1 restart failed — $P2"
L2=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"launch_app","args":{"app":"music"}}' 2>/dev/null | grep -a '{' | head -1)
printf '%s' "$L2" | grep -aq '"ok":true' || FAIL "relaunch music after I-1 restart failed — $L2"
MUSWIN=""
WIN2=""
for i in 1 2 3 4 5 6 7 8; do
    sleep 2
    WIN2=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)
    MUSWIN=$(printf '%s' "$WIN2" | grep -aoE '\{"id":[^}]*"title":"Music"[^}]*\}' | head -1)
    [ -n "$MUSWIN" ] && break
done
[ -n "$MUSWIN" ] || FAIL "no Music window after I-1 restart — last: ${WIN2:-none}"
MUS_ID=$(printf '%s' "$MUSWIN" | sed -n 's/.*"id":\([0-9]*\),.*/\1/p')
MUS_X=$(printf '%s' "$MUSWIN" | sed -n 's/.*"x":\(-\?[0-9]*\),.*/\1/p')
MUS_Y=$(printf '%s' "$MUSWIN" | sed -n 's/.*"y":\(-\?[0-9]*\),.*/\1/p')
echo "MUSWIN(restart, step-7 수형 재사용): $MUSWIN"
sleep 2   # 부팅 스캔(부적합 문서 — 스캐너 수용 파) 도착 안정화

echo "--- 16c-2. music_dir_add 1콜 (부적합 문서 위 — 수용/거부 원문 실측) ---"
A1=$(atool music_dir_add '{"path":"/tmp/mus_i1_dir"}')
echo "MUSIC-I1-ADD-REPLY: ${A1:-none}"
if printf '%s' "$A1" | grep -aq '"ok":true'; then
    echo "MUS-I1-ADD-OK: 수용 — AddDir의 자기 스캐너는 CP949 문서를 수용·보존한다(2o-f 계열 라이브 원문 — 거부 detail 'settings.json 파싱 실패 — 수기 치유 필요'는 서버 정문(settings_set) 소유 — 16c-3)"
else
    echo "MUSIC-I1-ADD-REJECT: 거부 — 위 reply 원문(브리프 예상 경로가 이쪽이면 리포트 원장)"
fi
I1_KEEP=0
if CP_HERE; then
    I1_KEEP=1
    echo "MUSIC-I1-ADD-KEEP: OK — add 쓰기 뒤 CP949 바이트 보존 원문(무음 소각 봉합의 라이브 영수증)"
else
    echo "MUSIC-FAIL(i1-add-keep: add 쓰기가 CP949 항목을 소각 — T1 fix r1 봉합 부정)"
fi
IL1=$(atool music_dir_list '{}')
echo "MUSIC-I1-LIST-1: ${IL1:-none} (보존-가시 read leg 원문 — CP949 항목도 수취)"

echo "--- 16c-3. 쓰기 정문(settings_set) 거부 detail 원문 (M-1r 본안) ---"
SS1=$(timeout 15 ./buildwsl/jkdesktop agentctl \
    '{"tool":"settings_set","args":{"key":"audio_master_volume","value":50}}' \
    2>/dev/null | grep -a '{' | head -1)
echo "MUSIC-I1-SS-REPLY: ${SS1:-none}"
I1_SS=0
if printf '%s' "$SS1" | grep -aq '수기 치유 필요'; then
    I1_SS=1
    echo "MUS-I1-SS-OK: OK — 정문 거부 detail 'settings.json 파싱 실패 — 수기 치유 필요' 라이브 실측"
else
    echo "MUSIC-FAIL(i1-ss: 정문 거부 detail 미검출 — $SS1)"
fi
SS_STDERR=$(grep -a 'settings_set 거부' "$I1LOG" | head -1)
echo "MUSIC-I1-SS-STDERR: ${SS_STDERR:-MISS} (서버 stderr 거부 1행)"
[ -n "$SS_STDERR" ] || echo "MUSIC-FAIL(i1-ss-stderr: 거부 stderr 1행 미검출)"

echo "--- 16c-4. remove 1콜 — 스캐너 수용으로 CP949 문서에서 dirs 소각 ---"
DREM2=$(atool music_dir_remove '{"path":"/tmp/mus_i1_dir"}')
echo "MUSIC-I1-REMOVE-REPLY: ${DREM2:-none}"
I1_RSC=0
printf '%s' "$DREM2" | grep -aq '"ok":true' \
    && { I1_RSC=1; echo "MUS-I1-RSC-OK: OK — remove가 CP949 문서(quickjs 거부 원문)를 자기 스캐너로 수용해 dirs를 소각(원본 보존 — 2o-f 계열)"; } \
    || echo "MUSIC-FAIL(i1-rsc: remove의 스캐너 수용 미관측 — $DREM2)"
IL2=$(atool music_dir_list '{}')
echo "MUSIC-I1-LIST-2: ${IL2:-none}"
if [ "$I1_RSC" -eq 1 ]; then
    CP_HERE && I1_KEEP=1 && echo "MUSIC-I1-RM-KEEP: OK — 제거 뒤에도 CP949 항목 보존(대상 아님 소각 금지 원문)" \
        || echo "MUSIC-FAIL(i1-rm-keep: 다른 항목 제거가 CP949 항목을 소각)"
fi
RA=$(atool music_dir_remove "{\"path\":\"$CPENTRY\"}")
echo "MUSIC-I1-REMOVE-CP949-REPLY: ${RA:-none} (도구 인자 경계 — AgentJson JSON 문자열은 UTF-8 검사라 CP949 바이트 수송 불가 — 패널 [제거] 원촉이 유일 인-엔진 치유 루트의 원문 근거)"

echo "--- 16c-5. [패널 제거 치유 루트] — [폴더 관리] 토글 → [제거] 클릭 ---"
# 정찰 원칙(T4 수형 — env 재보정): 클라 상대 좌표는 라이브 캘리브레이션 세션
# (2026-10-10)의 클릭-차분+툴팁 검증으로 찍었다. 합성 캡처 좌표계 = 클라 좌표
# + (417,173) — 캡처 원점이 데스크톱 (0,0)이 아니어서 절대 좌표 직용 불가(레슨
# — 캡처↔클라 변환 필수). [폴더 관리] 토글 = 클라 (130,70) — 툴팁("settings.
# music.dirs 폴더 추가/제거…")이 그 마우스 밑에서 뜬 채 패널이 열린 캡처가
# 증거. 상태행 감김(+17px 시프트)이 토글 x존 오른쪽 밖을 가리키게 한 (160,77)
# 클릭은 MISS였다(트랩 원장 — 상태행 1행 보장 단계에서 재측정). 패널 첫 행
# [제거] SmallButton = 클라 x=86,y=118(실증 치유 콜) — 이웃 격자 폴백. 클릭
# 판정: 토글 = 캡처 차분(열림 = 레이아웃 전이 — 임계 15행), [제거] =
# settings.json 바이트(무음 실패 금지 원장).
PX="$LOGDIR/mus_px.py"
cat > "$PX" <<'PYEOF'
# [T3] 캡처 차분기 — 두 PNG(필터 0·RGB8, 자가 캡처 전제)의 유의 행 수(픽셀
# 차합 > 75 합, 행별 ≥8 픽셀, y 60-700 2픽셀 스텝)를 센다. 패널 열림 =
# 레이아웃 전이(수십 행), 무효 클릭 = ≤ 몇 행 — 임계 15행 = 토글 판정.
import struct, sys, zlib

def load(p):
    d = open(p, "rb").read()
    pos, idat, w, h = 8, b"", 0, 0
    while pos < len(d):
        ln = struct.unpack(">I", d[pos:pos+4])[0]
        tag = d[pos+4:pos+8]
        body = d[pos+8:pos+8+ln]
        pos += 12 + ln
        if tag == b"IHDR":
            w, h = struct.unpack(">II", body[:8])
        elif tag == b"IDAT":
            idat += body
        elif tag == b"IEND":
            break
    raw = zlib.decompress(idat)
    return w, h, raw

w, h, a = load(sys.argv[1])
_, _, b = load(sys.argv[2])
n = 0
for y in range(60, min(h, 700), 2):
    c = 0
    for x in range(400, min(w, 1030)):
        va = a[y*(w*3+1)+1+x*3]
        vb = b[y*(w*3+1)+1+x*3]
        if abs(va - vb) > 75:
            c += 1
            if c >= 8:
                break
    if c >= 8:
        n += 1
print("diff_rows=%d" % n)
PYEOF
I1_OPEN=0
I1_CURE=0
if [ "$I1_BOOT" -eq 1 ] && [ "$I1_SS" -eq 1 ]; then
    # 웜업 클릭(무해 지점 — 표 남는칸의 빈 행역): 재기동 뒤 앱의 **첫 합성
    # 클릭은 무음 소멸한다**(2026-10-10 실측 렛슨 — cal/exp/repro/run-4/run-5
    # 5세션 전부 첫 클릭 0-diff, 2nd부터 착탄) — 착탄 판정 전 소거.
    WK=$(tap "$MUS_ID" "$((MUS_X + 300))" "$((MUS_Y + 300))")
    printf '%s' "$WK" | grep -aq '"ok":true' || echo "  warmup tap reply: $WK (계속 — 무해 지점)"
    sleep 1
    rm -f "$LOGDIR/mus_px_pre.png"
    python3 "$SHOT_PY" "$LOGDIR/mus_px_pre.png" >"$LOGDIR/mus_px_pre.log" 2>&1 \
        || FAIL "I-1 pre-state capture failed — $(tail -2 "$LOGDIR/mus_px_pre.log" | tr '\n' ' ')"
    TOG_TRIES=0
    while [ "$TOG_TRIES" -lt 3 ]; do
        TOG_TRIES=$((TOG_TRIES + 1))
        TCK=$(tap "$MUS_ID" "$((MUS_X + MUS_TOGGLE_X + MUS_TOGGLE_TWEAK_X))" \
            "$((MUS_Y + MUS_TOGGLE_Y))")
        printf '%s' "$TCK" | grep -aq '"ok":true' \
            || FAIL "I-1 toggle tap failed — $TCK"
        echo "CLICK-TOGGLE#$TOG_TRIES: id=$MUS_ID 클라($MUS_TOGGLE_X,$MUS_TOGGLE_Y) — repl: $TCK"
        sleep 1.3
        rm -f "$LOGDIR/mus_px_tog.png"
        python3 "$SHOT_PY" "$LOGDIR/mus_px_tog.png" >"$LOGDIR/mus_px_tog.log" 2>&1 \
            || FAIL "I-1 toggle-open capture failed — $(tail -2 "$LOGDIR/mus_px_tog.log" | tr '\n' ' ')"
        OP=$(python3 "$PX" "$LOGDIR/mus_px_pre.png" "$LOGDIR/mus_px_tog.png")
        echo "MUSIC-I1-PX-OPEN-CHECK#$TOG_TRIES: $OP (토글 판정 — 임계 15행)"
        if printf '%s' "$OP" | sed -n 's/.*diff_rows=\([0-9]*\).*/\1/p' | \
            awk '{exit ($1 >= 15) ? 0 : 1}'; then
            I1_OPEN=1
            echo "MUS-I1-TOGGLE-OK: [폴더 관리] 패널 열림 ($TOG_TRIES 시도)"
            break
        fi
        echo "  무변화(0-diff) — 첫 클릭 소멸 렛슨(웜업 후에도 간헐) — 재시도"
        cp "$LOGDIR/mus_px_tog.png" "$LOGDIR/mus_px_pre.png"
    done
    [ "$I1_OPEN" -eq 1 ] \
        || echo "MUS-I1-TOGGLE-MISS: 3시도 무변화 — MUS_TOGGLE_X/Y 재보정 몫"
else
    echo "MUS-I1-PANEL-SKIP: 부팅 경고/정문 거부 전항 미달 — 치유 루트 단계 생략(원장)"
fi
if [ "$I1_OPEN" -eq 1 ]; then
    shot mus_wsl_dirs_panel.png   # [폴더 관리] 패널 모양 1캡처(T3 브리프 — 치유 직전 상태)
    for RY in $MUS_REMOVE_YS; do
        for RX in $MUS_REMOVE_XS; do
            RK=$(tap "$MUS_ID" "$((MUS_X + RX))" "$((MUS_Y + RY))")
            printf '%s' "$RK" | grep -aq '"ok":true' \
                || FAIL "I-1 cure grid tap failed — $RK"
            sleep 1
            if ! CP_HERE; then
                I1_CURE=1
                echo "MUS-I1-CURE-OK: OK — 패널 [제거] 클릭(클라 x=$RX,y=$RY)이 CP949 항목을 소각했다 (tap reply: ${RK:-none} — 도구 인자 경계 원문은 16c-4)"
                break 2
            fi
            echo "  cure try (클라 x=$RX,y=$RY) — CP949 잔존(클릭 미적중)"
        done
    done
fi
if [ "$I1_CURE" -eq 1 ]; then
    echo "MUS-I1-PANEL-OK: 패널 제거 치유 루트 성립 — [폴더 관리] 토글 → [제거] 1콜로 문서 순수 회귀(T2 리뷰 I-1 §(1) 원문)"
else
    echo "MUS-I1-PANEL-MISS: 치유 루트 미검출 — 재정찰 런 필요(MUS_TOGGLE_X/Y·MUS_REMOVE_XS/YS — 위 mus_wsl_dirs_panel·mus_px_tog 원문)"
fi
IL3=$(atool music_dir_list '{}')
echo "MUSIC-I1-LIST-3: ${IL3:-none} (치유 후 list 원문)"
if [ "$I1_CURE" -eq 1 ]; then
    PURE=$(python3 - "$SETTINGS" <<'PYEOF'
import json, sys
try:
    with open(sys.argv[1], "rb") as f:
        obj = json.loads(f.read().decode("utf-8"))
except Exception as e:
    print("PURE-MISS %r" % e)
    raise SystemExit
print("PURE-OK audio=%s retention=%s dirs=%s"
      % ("audio" in obj, "retention" in obj,
         obj.get("music", {}).get("dirs", [])))
PYEOF
) || PURE="PURE-MISS(python3 런처 실패)"
    echo "MUSIC-I1-PURE-JSON: $PURE (독립 파서 — 문서 순수 회귀 영수증: 부팅 리더 회복 대리)"
    if printf '%s' "$PURE" | grep -aq 'PURE-OK'; then
        echo "MUS-I1-PURE-OK: OK — CP949 바이트 0(문서 순수)"
    else
        echo "MUSIC-FAIL(i1-pure: 치유 후 문서 비순수 — $PURE)"
        I1_CURE=0
    fi
    echo "--- 16c-6. 치유 후 music_dir_add 재시도 + settings_set 성공 (이후 쓰기 정상) ---"
    # 관측 원장(2026-10-10 런6): 패널 [제거]의 저장소 쓰기(원자적 — .bak 잔산)
    # 직후 music 클라가 세그폴트 사망한다(cal 세션과 2전 2 — tool_gone/
    # unknown_app_tool 답신). 서버는 생존(settings_set 계속 성립) — 신규 결함
    # 원장은 리포트(치유 경로의 P1 — T2 UI/스캔 leg 어느 축 — gdb 원문 몫).
    A2=$(atool music_dir_add '{"path":"/tmp/mus_i1_retry"}')
    echo "MUSIC-I1-ADD-RETRY-REPLY: ${A2:-none}"
    if printf '%s' "$A2" | grep -aq '"ok":true'; then
        I1_RETRY=1
        echo "MUS-I1-RETRY-OK: OK — 치유 후 music_dir_add 재시도 성공(부팅 재기동 불요)"
    elif printf '%s' "$A2" | grep -aq 'unknown_app_tool\|tool_gone'; then
        echo "MUSIC-FAIL(i1-add-retry: music 클라 [제거] 쓰기 뒤 세그폴트 사망 — 치유 후 도구 경로 소멸 — 재기동 후 재실측(앱 재스폰 — 서버 부팅 불요))"
        RL=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"launch_app","args":{"app":"music"}}' 2>/dev/null | grep -a '{' | head -1)
        printf '%s' "$RL" | grep -aq '"ok":true' || FAIL "music relaunch after client crash failed — $RL"
        echo "MUSIC-I1-APP-RELAUNCH: OK"
        sleep 3
        A2=$(atool music_dir_add '{"path":"/tmp/mus_i1_retry"}')
        echo "MUSIC-I1-ADD-RETRY-REPLY-2: ${A2:-none}"
        printf '%s' "$A2" | grep -aq '"ok":true' \
            && { I1_RETRY=1; echo "MUS-I1-RETRY-OK: OK — 재스폰 뒤 add 재시도 성공(이후 쓰기 정상)"; } \
            || echo "MUSIC-FAIL(i1-add-retry-2: 재스폰 뒤 등록 실패 — $A2)"
    else
        echo "MUSIC-FAIL(i1-add-retry: 치유 후 등록 실패(기타) — $A2)"
    fi
    IL4=$(atool music_dir_list '{}')
    echo "MUSIC-I1-LIST-4: ${IL4:-none} (재시도 등록 목록 원문)"
    SS2=$(timeout 15 ./buildwsl/jkdesktop agentctl \
        '{"tool":"settings_set","args":{"key":"audio_master_volume","value":50}}' \
        2>/dev/null | grep -a '{' | head -1)
    echo "MUSIC-I1-SS-POST-REPLY: ${SS2:-none}"
    I1_SS2=0
    if printf '%s' "$SS2" | grep -aq '"ok":true'; then
        I1_SS2=1
        echo "MUS-I1-SS2-OK: 치유 후 정문 쓰기 성공(부팅 재기동 불요 — 이후 쓰기 정상 원문)"
    else
        echo "MUSIC-FAIL(i1-ss2: 치유 후 정문 쓰기 실패 — $SS2)"
    fi
    # C1 라이브 등가는 **파일 수준** 판정으로 바꾼다(app 사망 시 tool_gone이
    # C1 부정으로 오독하는 함정 — 정직 부기). settings_set(정문)이 미관리
    # music 키를 원문 그대로 보존했는지 = 독립 파서 수취.
    C1CHK=$(python3 - "$SETTINGS" <<'PYEOF'
import json, sys
with open(sys.argv[1], "rb") as f:
    obj = json.loads(f.read().decode("utf-8"))
print("C1-OK music=%s audio=%s retention=%s" %
      (obj.get("music"), "audio" in obj, "retention" in obj))
PYEOF
) || C1CHK="C1-MISS(python3 파서 실패)"
    echo "MUSIC-I1-C1-FILE: $C1CHK (정문 쓰기 뒤 파일 원문 — music 키 생존 = C1 라이브 등가)"
    printf '%s' "$C1CHK" | grep -aq 'C1-OK' \
        || echo "MUSIC-FAIL(i1-c1: settings_set이 미관리 music 키를 소각 — C1 부정: $C1CHK)"
else
    I1_RETRY=0
    I1_SS2=0
    echo "MUS-I1-POST-CURE-SKIP: 치유 MISS — 이후 단계는 정찰 재보정 런에서(settings는 백업 원복으로 정리)"
fi
cp "$SNAP" "$SETTINGS" || echo "MUSIC-FAIL(i1-restore: 백업 복사 실패)"
I1_RESTORE=0
cmp -s "$SETTINGS" "$SNAP" && I1_RESTORE=1 \
    && echo "MUSIC-I1-RESTORE-BYTE: OK — 백업 원복 바이트 등호(T5 수형)" \
    || echo "MUSIC-FAIL(i1-restore: 원복 바이트 불일치)"

restore_and_exit

echo "=== 17. REMNANT — /tmp 자가 스크립트·중간파일 소각 ==="
rm -f "$SNAP" "$PAT"
LEFTS=$(ls "$LOGDIR" 2>/dev/null | wc -l)
REMNANT_LIST=$(ls "$LOGDIR")
echo "REMNANT-COUNT=$LEFTS (서버·빌드·셀프테스트·shot 로그만 잔존 — 원장 수형)"
echo "$REMNANT_LIST" | sed 's/^/  /'
if [ -d "$SCRATCH" ]; then
    echo "MUS-FAIL: scratch dir remnant — 수동 소각 필요"
else
    echo "SCRATCH-REMNANT: 0 (트리 잔산 0)"
fi
[ -d "$SEED_DIR" ] && { echo "MUS-FAIL: seed dir remnant — 수동 소각 필요"; exit 1; }
SETL=$(ls buildwsl/state/ 2>/dev/null | grep -a 'settings.json' \
    | grep -av '^settings.json$' | tr '\n' ' ')
echo "SETTINGS-REMNANT: ${SETL:-0} (bak/<pid>.tmp — 원자적 쓰기의 성공세대 이주/크래시 잔산 정직 부기 — T2 fix r1 M-1 영수증)"
[ -z "$SETL" ] || echo "MUSIC-FAIL(settings remnant: $SETL — 원장)"
rm -f buildwsl/state/settings.json.bak buildwsl/state/settings.json.*.tmp
[ -d /tmp/mus_dirs_x ] && echo "MUSIC-FAIL(dir remnant: /tmp/mus_dirs_x — 수동 소각)"
rm -rf /tmp/mus_dirs_x

echo "=== 18. music 실측 판정 ==="
CAP_OK=1
for f in spatial list filter vp_delegate delegate_status after; do
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
for f in spatial list filter vp_delegate delegate_status after dirs_panel; do
    P="$RECEIVE/mus_wsl_$f.png"
    [ -s "$P" ] && echo "  $P ($(wc -c < "$P") bytes)"
done
echo "=== 18c. [T3] dirs 도구 세그먼트/I-1 판정 (rc는 0 유지 — 원문은 위 각 행) ==="
DIRS_OK=0
if printf '%s' "${DADD:-}" | grep -aq '"ok":true' \
    && printf '%s' "${DL1:-}" | grep -aq 'mus_dirs_x' \
    && printf '%s' "${DREM:-}" | grep -aq '"ok":true' \
    && ! printf '%s' "${DL2:-}" | grep -aq 'mus_dirs_x' ; then
    DIRS_OK=1
fi
[ "$DIRS_OK" -eq 1 ] \
    && echo "MUSIC-DIRS-VERDICT: DIRS-OK(add=ok·list 등록/제거 원문·settings 기록+audio/retention 보존·원복 바이트 등호) — 도구 6종 원존(spatial 3종 병합 유지), 육안 스탭 대기" \
    || echo "MUSIC-DIRS-VERDICT: DIRS-FAIL(add=${DADD:-none} list1=${DL1:-none} remove=${DREM:-none} list2=${DL2:-none} — 행별 사유는 16b)"
I1_SUM="boot=$I1_BOOT ss=$I1_SS keep=$I1_KEEP rsc=${I1_RSC:-0} cure=$I1_CURE retry=${I1_RETRY:-0} ss2=${I1_SS2:-0} restore=${I1_RESTORE:-0}"
if [ "$I1_BOOT" -eq 1 ] && [ "$I1_SS" -eq 1 ] && [ "$I1_KEEP" -eq 1 ] \
    && [ "${I1_RSC:-0}" -eq 1 ] && [ "${I1_RESTORE:-0}" -eq 1 ]; then
    echo "MUSIC-I1-VERDICT: I1-OK(부팅 stderr 경고 1행·정문 거부 detail 원문·CP949 보존·스캐너 수용 remove 1콜·백업 원복 바이트 등호) — $I1_SUM cure=$I1_CURE(패널 치유 — MISS면 재정찰/EYES 승계)"
else
    echo "MUSIC-I1-VERDICT: I1-FAIL(honest-fail — 구성항목 원문은 16c 행별; $I1_SUM)"
fi
echo "=== 18b. spatial leg 판정 (T3 — rc는 0 유지, 원문은 위 각 행) ==="
LEG_FALLBACK=0
if printf '%s' "${PLAY1:-}" | grep -aq '"error":"start_failed"' \
    && printf '%s' "${PLAY1:-}" | grep -aq 'vplayer 위임'; then
    LEG_FALLBACK=1   # kDelegationHint 원문 라벨 — fail-closed 정상 영수증
fi
LEG_OK=0
printf '%s' "${PLAY1:-}" | grep -aq '"accepted":true' \
    && printf '%s' "${S2:-}" | grep -aq '"active":true' \
    && printf '%s' "${S2:-}" | grep -aq '"deviceOk":true' \
    && { [ -n "$POS_DELTA" ] && awk -v d="$POS_DELTA" 'BEGIN{exit (d > 0.05) ? 0 : 1}' \
        && LEG_OK=1; }
if [ "$LEG_MODE" -eq 1 ]; then
    if [ "$LEG_OK" -eq 1 ]; then
        echo "MUS-LEG-VERDICT: LEG-OK(env 설정 배포 런 — accepted+active:true+deviceOk:true+pos 진행 delta=$POS_DELTA·stop 원문 — 청안 게이트는 EYES 별도)"
    elif [ "$LEG_FALLBACK" -eq 1 ]; then
        echo "MUS-LEG-VERDICT: LEG-FALLBACK-OK(WSLg에서 ALC 디바이스 실패 — kDelegationHint 라벨 표기 = fail-closed 정상 영수증(rc=0) — 폰 leg는 T4 몫)"
    else
        echo "MUSIC-FAIL(leg verdict 미달 — LEG_MODE=1에서도 시작/진행/폴백 어느 성질도 관측 못함: play=${PLAY1:-none} status=${S2:-none})"
    fi
else
    if [ "$LEG_FALLBACK" -eq 1 ]; then
        echo "MUS-LEG-UNSET-VERDICT: LEG-UNSET-OK(leg-less .so의 spatial_play = DeviceFailed 종착 — kDelegationHint 원문 라벨 — fail-closed 영수증 성립)"
    else
        echo "MUSIC-FAIL(env 미설정 런에서 spatial_play의 DeviceFailed 종착이 관측되지 않음: ${PLAY1:-none} — fail-closed 배선 원장 대조 몫)"
    fi
fi
if [ "$LEG_MODE" -eq 1 ] && [ "$LEG_OK" -eq 1 ]; then
    [ "$M1_TRUNC" -eq 1 ] \
        && echo "MUS-LEG-M1: OK — 초장 경로(~${LONG_LEN}B) spatial_status가 truncated:true를 냈다(절단 정직 가드 원문 실측)" \
        || echo "MUS-LEG-M1: MISS — truncated 도달 실패(SL1 원문) — M-1 가드 런타임 실측 미충족(리포트 원장)"
fi
echo "MUSIC-END"
exit 0
# honest-fail 원칙: 수치·육안 대행 미달(MUSIC-FAIL 라인·DELEGATE MISS)은 원장
# 목적이라 rc=0. hard FAIL(가드·빌드·selftest·시드·settings 주입·부팅·ping·
# launch·창·캡처·복원 실패)만 exit 1.
# EOF — 끝 개행 유지.