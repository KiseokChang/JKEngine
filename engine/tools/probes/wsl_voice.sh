#!/usr/bin/env bash
# T6 구두 E2E 실측 probe (#112 — 스펙 2026-10-25-stt-voice-design §3 ③⑥⑧·§2.4,
# plan task-6). WSL 축 — **비결정 전사를 제외한 전 사슬 원문**:
# fixture(합성 16k WAV)→전사→ChatRouteTurn→ChatAction→launch_app 질의 원문.
# WSL에서 chat [구두]의 마이크는 WSLg SDL 캡처(실발화 E2E 표면 = 폰 T5 원문
# 승계)라 본 probe의 본질 = ①fixture 양성 대조 사슬(T2/T3 케이스의 픽스처
# 승계 — RunVoiceTurn 접점 직발 — 사슬 전사 == whisper-cli 직접 스폰 등호)
# ②fail-closed 원문(WHISPER_ROOT unset·cli 부재·모델 부재 3판) ③chat [구두]
# 표면(F6 hold-to-talk + [구두] 버튼 클릭 — WSLg SDL 캡처 스트림= pactl 영수증)
# ④launch_app 질의 소비(도구 경로 재용 — §2.1 "STT 텍스트 == 타이핑 텍스트").
#
# 원존 수혀(대부분 원문 승계 — 본문 재작 0 지향):
#   * 서버 기동·창·캡처 브래킷 = wsl_music.sh 전체 수형(set :0·setsid·#98
#     runner 가드·bracketed pkill(-9 에스컬레이션)·stale 소켓 rm 가드·
#     permissions 스테이지(ENTRY 스냅샷+바이트 원복)·X11 창 단위 shot(XImage
#     오프셋 원장 포함 전문)·tapsettle(무음 판정=재시도 안전)·캡처 회수 단열
#     파이프·REMNANT 가드).
#   * 사슬 probe cpp = tmp/t5_voice_probe.cpp(T5 폰판 — gate 3판 env 셋/언셋
#     블록) + tmp/t3_voice_probe.cpp(T3 — launch-query 양성 대조+dispatch
#     기록형)의 WSL판 — probe가 /tmp scratch에서 **자가 생성**(heredoc — 커밋
#     1파일 계약)·buildwsl lib 직링크(T5 사다리 원존 — 멤버 원문 원장).
#   * T5 함정 2·6·9·10 원천 원문 승계(브리프 지시):
#       (2) Termux:API 어휘 실측 상쇄 → WSL 축 접점 없음(마이크 = SDL 캡처
#       본체 — 다른 축) — StopCapture가 마침의 종착 진실원(함정 9 승계:
#       상한 안전판 의존 금지의 WSL판 — F6 up/마침 클릭이 곧 정지).
#       (6) cygpath -w recover 폴백 → WSL 축은 /tmp→engine/tmp 단일 cp 단열
#       파이프(wsl_music 원존 — cygpath 계통 무접촉).
#       (10) 언포커스/초기 합성 클릭 미도달 → **런 중 재실측**: 클라 스폰 뒤
#       첫 합성 입력(클릭·키 무관)은 무음 소멸(#97 웜업 렛슨의 chat 재판정 —
#       정찰 런 실측: 첫 클릭 0-diff·2번째 클릭 이후 착탄·F6 키는 초발부터
#       도달) — 본 probe의 착탄 판정 = 전체 프레임 diff(2장 이상 대조+세트
#       무음 판정)·[구두] 클릭은 F6 워밍 후 실시.
#
# CANON 밖 캡처 규약(§2.4·T1): whisper 전사 본문(마이크 환각·fixture 전사)은
#   **결정론이 아니다** — VOICE-STT-TEXT 원문 행으로 캡처만 하고 selftest에
#   녹이지 않는다(2v = selftest 소관). probe의 게이트 어설션 = fail-closed
#   3판 원문·chain==direct 등호·selftest 수취 등호·라우트 질의 양식(결정 축).
#
# 판정(스크립트 규약 — wsl_music 원문 수형): 인프라 실패(빌드·부팅·ping·
#   launch·창 부재·캡처 실패·소각·permissions)만 hard FAIL rc=1. 수치·육안
#   이벤트는 VOICE-FAIL 원장 라인으로 남기고 rc=0(honest-fail).
#
# 요약 스트림 계약(라인 원장 — probe stdout 수취 5종 보존 승계):
#   * VOICE-RUN-VALS: run=<n> canon=<n> twov=<n> fc3=<0..3> direct=<0|1>
#     stt_t2_canon=<n/a|0|1> fixt=<0|1> keyturn=<0..2> clickturn=<0..2>
#     route=<0|1> probe_rc=<n> captures=… (폰 축 라벨 = phone_voice.sh 소관 —
#     VOICE-PHONE-RUN-VALS·fix r1: fc3=3판 전부가 VERDICT 본조건으로 채용)
#   * VOICE-VERDICT: 1행 결정(MUSIC-TAGWRITE-PHONE-VERDICT 동형).
#
# #115b T2 신설 세그먼트(§13b) — 구두 어휘 확장 v1.5(T1 커밋 21ee4dd+ab97462)
#   소비 계약: 발화 8건(kRouteTable/kAliasTable 단일 진실원 재실측 — 즉발 5+
#   무인자 강제 1+D3 배제 stub 2)→ChatRouteTurn 직발(구두 표면과 같은 진입)→
#   ChatActionQuery 조립→agentctl 도구 경로 중계 소비(app_tool — vplayer
#   open/play_pause 토글 2탭+get_status 기록 반영·launch_app notify 창 원문).
#   기존 세그먼트 원존 — 말단 VOICE-VERDICT 산치 무침범(신설 판정 = 별도
#   VOCAB-VERDICT 1행 — VERDICT 본조건 부합 계약 미변경).
#
# 실행 표준(WSL 밖 Git Bash에서 — MSYS 경로 변환 찢김 가드 필수):
#   MSYS_NO_PATHCONV=1 wsl.exe -d Ubuntu-24.04 -- bash /mnt/i/progwork/JKENGINE/engine/tools/probes/wsl_voice.sh \
#     > engine/tmp/wsl_voice_run.log 2>&1
# VOICE_RECON=1 로 실행하면 chat ready 캡처+캘리브 직후 복원·종료(정찰 런).
set -u
cd /mnt/i/progwork/JKENGINE/engine

RUN_NO=${VOICE_RUN_NO:-auto}
FAIL() { echo "VOC-FAIL: $*"; exit 1; }
LOGDIR=/tmp/voc
SCRATCH=/tmp/voc_scratch
RECEIVE=/mnt/i/progwork/JKENGINE/engine/tmp
SHOT_PY="$LOGDIR/voc_shot.py"
CANON_WSL_SELFTEST=892   # #115b WSL 캐논(#112 T5 814 → #113+2w 4·#115+2x 15 = 833 →
                         # #115b T1+2y 17 = 850 → #116·#117·v1.6 계보 +2z 9·2x+4·
                         # 2y+4 = 867 → #118 T1+3a 13 = 880 → #118 T2 2y+10·
                         # 3a+2 = **892** — 본 라인 T2 런 실측 — 880 승계 계보)
CANON_WSL_2V=14          # 구두 라인 신설 2v ①-⑭(T2 +6·T3 +4) — 계보 원문
CANON_WSL_2Y=31          # 어휘 2y 계보(17 → v1.6+4 → #118 T2 ⑲+10 = 31 — §3 수취)
CANON_WSL_2Z=9           # #117 TTS 2z ①-⑨ — 계보 원문(캐논 수취)
CANON_WSL_3A=15          # #118 오버레이 3a 13(T1) + T2 ⑤·⑤(b) 2 = 15 — §3 수취

if [ -z "${WSL_DISTRO_NAME:-}" ]; then
  echo "VOC-WFAIL: not inside WSL — 실행 축 계약 위반 (wsl.exe -d ... probes/wsl_voice.sh)"
  exit 1
fi
if [ -n "${PHONE_HOST:-}" ]; then
  echo "VOC-NOTE: PHONE_HOST set — WSL 축은 PHONE_HOST 불요(무시하고 진행)"
fi

# #98 runner guard (wsl_music 원문 수형 — LIVE 서버 = 사용자 실세션 — [SKIP]).
if pgrep -f '[j]kdesktop --server' >/dev/null 2>&1; then
    echo "[SKIP] voice: a live jkdesktop server is running — #98 runner guard (nothing pkilled, nothing rm'd)"
    exit 0
fi

echo "=== 0. ENTRY 상태 기록 (종료 시 원복 판정용) ==="
ENTRY_UP=0
pgrep -f 'buildwsl/[j]kdesktop' >/dev/null 2>&1 && ENTRY_UP=1
ENTRY_PERM=0
[ -f buildwsl/permissions.json ] && ENTRY_PERM=1
ENTRY_SCRATCH=0
[ -d "$SCRATCH" ] && ENTRY_SCRATCH=1
echo "ENTRY-STATE: server-up=$ENTRY_UP permissions.json=$ENTRY_PERM scratch=$ENTRY_SCRATCH"
[ "$ENTRY_SCRATCH" -eq 0 ] || FAIL "$SCRATCH already exists — 이전 probe 잔상, 수동 소각 필요"

restore_and_exit() { # 정찰 런 등 중간 탈출 — 복원 다형(END 본문 동형)
    echo "=== 복원 (early exit) ==="
    pkill -f 'buildwsl/[j]kdesktop' 2>/dev/null
    sleep 1
    pkill -9 -f 'buildwsl/[j]kdesktop' 2>/dev/null
    sleep 1
    if [ "$ENTRY_PERM" -eq 0 ]; then
        rm -f buildwsl/permissions.json
        echo "PERM-BURIED: permissions.json removed"
    elif [ -n "${PERM_SNAP:-}" ] && [ -f "$PERM_SNAP" ]; then
        cp "$PERM_SNAP" buildwsl/permissions.json \
            || FAIL "user permissions.json restore copy failed"
        cmp -s buildwsl/permissions.json "$PERM_SNAP" \
            || echo "VOICE-FAIL(perm-restore: 원복 바이트 불일치 — 무손상 계약 위반 원장)"
        echo "PERM-RESTORED: user permissions.json restored byte-exact ($(wc -c < buildwsl/permissions.json) bytes)"
    fi
    if [ "$ENTRY_UP" = "1" ]; then
        env DISPLAY=:0 setsid nohup ./buildwsl/jkdesktop --server >"$LOGDIR/voc_srv_restore.log" 2>&1 &
        sleep 6
        pgrep -f 'buildwsl/[j]kdesktop --server' >/dev/null 2>&1 \
            || echo "VOICE-FAIL(restore-boot: 원복 부팅 실패)"
        echo "END-STATE: WSL 서버 UP 재부팅 — 진입 상태 원복 완료"
    else
        echo "END-STATE: WSL 서버 DOWN — 진입 상태 원복 완료"
    fi
    rm -rf "$SCRATCH" "$LOGDIR"
    if [ -d "$SCRATCH" ] || [ -d "$LOGDIR" ]; then
        echo "VOICE-FAIL(bury: probe dir 소각 실패 — $SCRATCH $LOGDIR — 수동 소각)"
    else
        echo "PROBE-DIR-BURIED: $SCRATCH+$LOGDIR removed (REMNANT 0)"
    fi
}

if [ "$ENTRY_PERM" -eq 1 ]; then
    mkdir -p "$LOGDIR"
    PERM_SNAP="$LOGDIR/voc_perm_user.json"
    cp buildwsl/permissions.json "$PERM_SNAP" || FAIL "user permissions.json snapshot failed"
    cmp -s buildwsl/permissions.json "$PERM_SNAP" || FAIL "user permissions.json snapshot mismatch"
    echo "PERM-SNAPSHOT: $(wc -c < "$PERM_SNAP") bytes — END 바이트 등호 원복 계약"
fi

echo "=== 1. bracketed pre-clean (살아있는 jkdesktop 종료 — wsl_music 원존) ==="
pkill -f 'buildwsl/[j]kdesktop' 2>/dev/null
sleep 1
pkill -9 -f 'buildwsl/[j]kdesktop' 2>/dev/null
sleep 1
LEFT=$(ps -eo args | grep -i 'buildwsl/[j]kdesktop' | wc -l)
[ "$LEFT" -eq 0 ] || FAIL "pre-clean left $LEFT jkdesktop process(es) alive"
mkdir -p "$SCRATCH" "$LOGDIR" "$RECEIVE"

echo "=== 2. 배포 확인 (ninja 접촉 0 — 빌드 접촉 최소 계약(2v 등호 검증 1회) — 존재 점검만) ==="
[ -x buildwsl/jkdesktop ] || FAIL "buildwsl/jkdesktop missing (배포 확인 실패)"
[ -f buildwsl/jkapp_chat.so ] || FAIL "buildwsl/jkapp_chat.so missing (T4 배포 확인 실패)"
[ -f buildwsl/whisper-build/whisper-cli ] || FAIL "buildwsl/whisper-build/whisper-cli missing (T1 빌드 산출 — 조달 확인 실패)"
[ -s buildwsl/stt-models/ggml-small.bin ] || FAIL "buildwsl/stt-models ggml-small.bin missing (조달기 산출 부재 — T1)"
[ -d third_party/whisper.cpp ] || FAIL "third_party/whisper.cpp missing (R1 vendor 부재 — WHISPER_ROOT 게이트가 정직 끔이 된다)"
CLI_ABS=$(cd buildwsl/whisper-build && pwd)/whisper-cli
MOD_ABS=$(ls buildwsl/stt-models/*.bin 2>/dev/null | head -1)
WHSP_ROOT_ABS=$(cd third_party/whisper.cpp && pwd)
[ -n "$MOD_ABS" ] || FAIL "stt-models .bin resolve empty"
echo "VOC-ASSETS: cli ok model=$(basename "$MOD_ABS") whisper-root-dir ok"
# #115b T2 신설 어휘 세그먼트의 릴레이 소비(vplayer) 재료 — 엔진 소유 합성 클립
# (wsl_vp_cycle/wsl_jog_flow 원존 계약 — 사용자 보유 클립 금지 계약 승계).
CLIP=/mnt/i/progwork/JKENGINE/tmp/vpt2_test.mp4
[ -s "$CLIP" ] || FAIL "vplayer 소비 클립 부재 ($CLIP — 엔진 소유 합성 클립 — wsl_vp_cycle 원존)"
echo "VOC-CLIP: $CLIP ($(wc -c < "$CLIP") bytes)"

echo "=== 3. WSL selftest (캐논 $CANON_WSL_SELFTEST·2v $CANON_WSL_2V — 수취 등호) ==="
timeout 420 ./buildwsl/jkdesktop test >"$LOGDIR/voc_st.log" 2>&1
ST_RC=$?
ST_FAIL=$(grep -ac '^\[FAIL\]' "$LOGDIR/voc_st.log")
ST_PASS=$(grep -ac '^\[PASS\]' "$LOGDIR/voc_st.log")
echo "selftest rc=$ST_RC PASS=$ST_PASS FAIL=$ST_FAIL"
grep -aq 'AppSelfTest: 0 failure(s)' "$LOGDIR/voc_st.log" \
    || FAIL "selftest summary missing 'AppSelfTest: 0 failure(s)'"
[ "$ST_FAIL" -eq 0 ] || FAIL "selftest FAIL=$ST_FAIL"
[ "$ST_RC" -eq 0 ] || FAIL "selftest rc=$ST_RC"
[ "$ST_PASS" -eq "$CANON_WSL_SELFTEST" ] \
    && echo "CANON-WSL: OK — PASS=$ST_PASS 캐논 등호" \
    || echo "WARN: selftest PASS=$ST_PASS != 캐논 $CANON_WSL_SELFTEST (계보 이탈 — 리포트 원장)"
# 구두 라인 신설 케이스 (2v 계열): T2 +6(wav/판정/파서)·T3 +4(접점) = 14.
# **2v 등호 검증 (WSL 축 1회 — 빌드 접촉 최소 계약 축)**. 전사 본문 비결정
# 단정 0(§2.4 — 2v는 순수 부품+판정부만).
ST2V=$(grep -ac '^\[PASS\] 2v' "$LOGDIR/voc_st.log")
echo "SELFTEST-2V-CASES=$ST2V (구두 라인 신설 2v — 캐논 $CANON_WSL_2V 대조)"
[ "$ST2V" -eq "$CANON_WSL_2V" ] || echo "VOICE-FAIL(2v: $ST2V != $CANON_WSL_2V — 계보 원장)"
ST2Y=$(grep -ac '^\[PASS\] 2y' "$LOGDIR/voc_st.log")
echo "SELFTEST-2Y-CASES=$ST2Y (구두 어휘 라인 신설 2y — 캐논 $CANON_WSL_2Y 대조)"
[ "$ST2Y" -eq "$CANON_WSL_2Y" ] || echo "VOICE-FAIL(2y: $ST2Y != $CANON_WSL_2Y — 계보 원장)"
ST2Z=$(grep -ac '^\[PASS\] 2z' "$LOGDIR/voc_st.log")
echo "SELFTEST-2Z-CASES=$ST2Z (TTS 부품 2z — 캐논 $CANON_WSL_2Z 대조)"
[ "$ST2Z" -eq "$CANON_WSL_2Z" ] || echo "VOICE-FAIL(2z: $ST2Z != $CANON_WSL_2Z — 계보 원장)"
ST3A=$(grep -ac '^\[PASS\] 3a' "$LOGDIR/voc_st.log")
echo "SELFTEST-3A-CASES=$ST3A (오버레이 부품 3a — 캐논 $CANON_WSL_3A 대조)"
[ "$ST3A" -eq "$CANON_WSL_3A" ] || echo "VOICE-FAIL(3a: $ST3A != $CANON_WSL_3A — 계보 원장)"

echo "=== 4. 사슬 probe cpp 자가 생성+컴파일 (T5 폰판 원존 승계 — /tmp scratch 사다리) ==="
cat > "$SCRATCH/wsl_voice_probe.cpp" <<'CPPEOF'
// T6 WSL 축 scratch probe — RunVoiceTurn 사슬 원문 실측(CANON 밖 — 전사
// 비결정 원칙 §2.4 — selftest 녹임 0). wsl_voice.sh가 생성 — 커밋 0.
// 원존 수형 = tmp/t5_voice_probe.cpp(T5 gate 3판 env 셋/언셋 블록)+
// tmp/t3_voice_probe.cpp(T3 launch-query 양성 대조+dispatch 기록형).
#include <voice/SttVoice.h>
#include <agent/JKLlmEngine.h>

#include <cstdio>
#include <cstdlib>
#include <string>

int main(int argc, char** argv) {
    // argv: 1=fixture wav 2=whisper-cli 경로 3=ggml 모델 경로 — probe exe가
    // 형제 dir 없음(/tmp scratch) → exe 형제 resolve 우회 = env
    // 오버라이드의 argv 직달판(T5 원존 계약 — WHISPER_BIN/STT_MODEL).
    const std::string fixture =
        argc > 1 ? argv[1] : "tmp/t2_jkengine_synth.wav";
    if (argc > 2 && argv[2][0]) ::setenv("WHISPER_BIN", argv[2], 1);
    if (argc > 3 && argv[3][0]) ::setenv("STT_MODEL", argv[3], 1);
    std::printf("model-resolved=%s\n",
                jk::voice::run::ResolveModelPath().c_str());
    std::fflush(stdout);

    // ---- fail-closed 판정 3판 (T5 gate 블록 원존 승계) ----------------------
    {
        const auto envOf = [](const char* k) {
            const char* v = ::getenv(k);
            return std::string(v ? v : "");
        };
        std::string e1, e2, e3;
        jk::voice::turn::VoiceTurnResult r;
        const std::string keepRoot = envOf("WHISPER_ROOT");
        ::unsetenv("WHISPER_ROOT");
        jk::voice::turn::RunVoiceTurn(fixture, jk::agent::ChatConfig{},
                                      nullptr, r);
        e1 = r.err;
        if (!keepRoot.empty()) ::setenv("WHISPER_ROOT", keepRoot.c_str(), 1);
        else ::setenv("WHISPER_ROOT", "1", 1);
        const std::string keepBin = envOf("WHISPER_BIN");
        ::setenv("WHISPER_BIN", "/no/such/whisper-cli", 1);
        jk::voice::turn::RunVoiceTurn(fixture, jk::agent::ChatConfig{},
                                      nullptr, r);
        e2 = r.err;
        if (!keepBin.empty()) ::setenv("WHISPER_BIN", keepBin.c_str(), 1);
        else ::unsetenv("WHISPER_BIN");
        const std::string keepModel = envOf("STT_MODEL");
        ::setenv("STT_MODEL", "/no/such/model.bin", 1);
        jk::voice::turn::RunVoiceTurn(fixture, jk::agent::ChatConfig{},
                                      nullptr, r);
        e3 = r.err;
        if (!keepModel.empty()) ::setenv("STT_MODEL", keepModel.c_str(), 1);
        else ::unsetenv("STT_MODEL");
        std::printf("gate-env-unset=[%s]\n", e1.c_str());
        std::printf("gate-bin-missing=[%s]\n", e2.c_str());
        std::printf("gate-model-missing=[%s]\n", e3.c_str());
        std::fflush(stdout);
    }

    // ---- fixture 사슬 (양성 대조 — dispatch 기록형, 질의 전달 생략 경로) ----
    jk::agent::ChatConfig cfg;   // 미구성 — 라우터 즉발+stub 폴백 원존
    jk::voice::turn::VoiceTurnResult r;
    int dispatchCalls = 0;
    std::string lastTool, lastArgs;
    const jk::voice::turn::ToolDispatcher disp =
        [&](const std::string& tool, const std::string& args) {
            ++dispatchCalls;
            lastTool = tool;
            lastArgs = args;
            return true;
        };
    const bool ok = jk::voice::turn::RunVoiceTurn(fixture, cfg, disp, r);
    std::printf("fixture-turn ok=%d usedLlm=%d stt=[%s] guide-len=%d "
                "err=[%s] calls=%d tool=[%s] args=[%s]\n",
                static_cast<int>(ok), static_cast<int>(r.usedLlm),
                r.sttText.c_str(), static_cast<int>(r.guide.size()),
                r.err.c_str(), dispatchCalls, lastTool.c_str(),
                lastArgs.c_str());
    std::fflush(stdout);

    // ---- launch-query 양성 대조 (2v-⑪ 동형 — 라우터 텍스트 직발 질의) ------
    std::string tool, args;
    jk::ChatAction a;
    jk::ChatRouterRoute("지뢰찾기 켜줘", a);
    // printf 인자 평가 순서 함정(T3 probe fix r1 원존) — 조립을 먼저.
    const bool qOk = jk::voice::turn::ChatActionQuery(a, tool, args);
    std::printf("launch-query=%s %s\n", qOk ? tool.c_str() : "no",
                qOk ? args.c_str() : "no");
    std::fflush(stdout);

    // ---- #115b T2 신설 어휘 세그먼트 (발화→라우터 즉발→질의 조립 원문) -----
    // kRouteTable exact 행(ChatRouter.cpp 단일 진실원 — T1 리포트 §2)을
    // ChatRouteTurn(구두 표면과 같은 진입 — RunVoiceTurn (4) 접점 계약의
    // 동일 함수)으로 직발한다. cfg 미구성 = 라우터 즉발+usedLlm 0(2y-① 원존).
    // 질의 조립 = ChatActionQuery(SttVoice turn — app_tool 릴레이 양식 1종).
    // 배제 어휘 2건(D3 — 행 부재 = Info stub)·무인자 강제 1건(D1)·기존 회귀
    // 1건("창 목록 보여줘" — 2y-⑩ 방어 행)이 같은 루프로 재실측된다.
    // #118 T2 신설(#118-RUN-VALS 계보): 오버레이 어휘 2행("창 번호 보여줘"·
    // "번호 보여줘" — overlay_numbers — kind 6)+숫자 화법 4건("창 3"·
    // "3번 창" — Focus winNum·kind 2 — 접미수 판정 부품 2y-⑲)+미매치 2건
    // ("창 0"·"창 삼" — Info stub). num 칼럼 = 행 끝 추가(기존 행 패턴
    // 무침범 — T1 부품 원존).
    const char* const kVocabSay[] = {
        "멈춰줘", "재생해줘", "정지", "알림 보여줘", "볼륨 50으로 멈춰줘",
        "다음 곡", "알림 뭐 왔어", "창 목록 보여줘",
        "창 번호 보여줘", "번호 보여줘", "창 3", "3번 창", "창 0", "창 삼",
    };
    for (const char* say : kVocabSay) {
        bool used = false;
        jk::ChatAction va;
        const std::string g = jk::ChatRouteTurn(say, va, cfg, &used);
        std::string vtool, vargs;
        const bool vq = jk::voice::turn::ChatActionQuery(va, vtool, vargs);
        // printf 인자 평가 순서 함정(T3 fix r1 원존) — 조립을 먼저.
        const std::string toolCol = vq ? vtool : "no";
        const std::string argsCol = vq ? vargs : "no";
        std::printf("VOCAB text=[%s] kind=%d app=[%s] tool=[%s] usedLlm=%d "
                    "q=%d query=[%s %s] num=%d guide-len=%d\n",
                    say, static_cast<int>(va.kind), va.app.c_str(),
                    va.tool.c_str(), static_cast<int>(used),
                    static_cast<int>(vq), toolCol.c_str(), argsCol.c_str(),
                    va.winNum, static_cast<int>(g.size()));
        std::fflush(stdout);
    }

    std::printf("state.capturing=%d lastErr=[%s]\n",
                static_cast<int>(jk::voice::capture::IsCapturing()),
                jk::voice::capture::GetCaptureState().lastError.c_str());
    return 0;
}
CPPEOF

OK_LINK=0
LINK_LIBS=""
for LIBS in \
  "buildwsl/libjkcore.a buildwsl/libimgui.a buildwsl/libquickjs.a -lSDL2 -lSDL2_mixer -lpthread -ldl -lm" \
  "buildwsl/libjkcore.a buildwsl/libquickjs.a -lSDL2 -lSDL2_mixer -lpthread -ldl -lm" \
  "buildwsl/libjkcore.a buildwsl/libquickjs.a -lSDL2 -lSDL2_mixer -lpthread"; do
    [ "$OK_LINK" -eq 1 ] && break
    echo "TRY-LIBS: $LIBS"
    g++ -std=c++20 -I include "$SCRATCH/wsl_voice_probe.cpp" $LIBS \
        -o "$SCRATCH/wsl_voice_probe" >"$LOGDIR/voc_link.err" 2>&1
    RC=$?
    echo "LINK-RC=$RC"
    if [ "$RC" -eq 0 ]; then
        OK_LINK=1
        LINK_LIBS="$LIBS"
        break
    fi
    tail -3 "$LOGDIR/voc_link.err" | sed 's/^/LINK-ERR: /'
done
[ "$OK_LINK" -eq 1 ] || FAIL "probe link ladder exhausted (링크 멤버 원문은 위 TRY-LIBS/LINK-ERR — T5 사다리 원존 승계 원장행)"
echo "LINK-OK: $SCRATCH/wsl_voice_probe (멤버 = $LINK_LIBS)"

echo "=== 5. 합성 WAV fixture 자가 생성 (합성 재료만 — T2 레시피 가족 승계) ==="
cat > "$LOGDIR/voc_fixture.py" <<'PYEOF'
# fixture 생성기 — python3 wave stdlib(의존 0): 16 kHz/모노/s16le/1.2s/440 Hz
# 사인 — T2 본판정 픽스처와 같은 레시피 가족. **바이트 등호 재현은 실패
# 실측**(T2 세션의 생성기 원문이 산출물로 보존되지 않았다 — recon 런 원장
# 2판 miss) — 본 probe의 결정 축 = chain==direct 등호(전사 본문 비결정 무관)
# 이고 T2 원본이 있으면 승계 대차 행(fixture-t2)으로 [구독 / 좋아요] 원문
# 등호를 CANON 밖 캡처로 수취한다.
import hashlib, math, struct, sys, wave
rate, freq, secs = 16000, 440.0, 1.2
out = sys.argv[1]
with wave.open(out, "wb") as f:
    f.setnchannels(1)
    f.setsampwidth(2)
    f.setframerate(rate)
    frames = bytearray()
    for n in range(int(rate * secs)):
        v = math.sin(2.0 * math.pi * freq * n / rate)
        s = int(v * 32767)
        if s > 32767:
            s = 32767
        frames += struct.pack("<h", s)
    f.writeframes(bytes(frames))
b = open(out, "rb").read()
print("FIXTURE-OK %s bytes=%d md5=%s" % (out, len(b), hashlib.md5(b).hexdigest()))
PYEOF
python3 "$LOGDIR/voc_fixture.py" "$SCRATCH/t6_fixture.wav" || FAIL "fixture generation failed (python3)"
FIX_MD5=$(python3 - "$SCRATCH/t6_fixture.wav" <<'PYEOF'
import hashlib, sys
print(hashlib.md5(open(sys.argv[1], "rb").read()).hexdigest())
PYEOF
)
FIX_SIZE=$(wc -c < "$SCRATCH/t6_fixture.wav")
echo "FIXTURE-GEN: $SCRATCH/t6_fixture.wav bytes=$FIX_SIZE md5=$FIX_MD5 (합성 — 16k/모노/s16le/1.2s/440Hz — T2 레시피 가족)"

FIXTURE_T2=""
if [ -s /mnt/i/progwork/JKENGINE/engine/tmp/t2_jkengine_synth.wav ]; then
    FIXTURE_T2="$SCRATCH/t2_jkengine_synth.wav"
    cp /mnt/i/progwork/JKENGINE/engine/tmp/t2_jkengine_synth.wav "$FIXTURE_T2" || FAIL "T2 fixture copy failed"
    T2_MD5=$(python3 - "$FIXTURE_T2" <<'PYEOF'
import hashlib, sys
print(hashlib.md5(open(sys.argv[1], "rb").read()).hexdigest())
PYEOF
)
    echo "FIXTURE-T2-STAGE: T2 본판정 픽스처 승계 대차 (md5=$T2_MD5 — T2/T3/T4 런 계보)"
else
    echo "FIXTURE-T2-STAGE: MISS — T2 픽스처 원본 부재(engine/tmp) — 자가 생성 fixture만"
fi

echo "=== 6. 사슬 probe 런 — fail-closed 3판 + fixture 사슬 (WHISPER_ROOT 보유) ==="
export WHISPER_ROOT="$WHSP_ROOT_ABS"
PROBE_OUT="$SCRATCH/probe_selfgen.out"
"$SCRATCH/wsl_voice_probe" "$SCRATCH/t6_fixture.wav" "$CLI_ABS" "$MOD_ABS" \
    >"$PROBE_OUT" 2>"$SCRATCH/probe_selfgen.err"
PP=$?
echo "PROBE-RC=$PP"
sed 's/^/PROBE: /' "$PROBE_OUT"
[ "$PP" -eq 0 ] || echo "VOICE-FAIL(probe-rc: 사슬 probe rc=$PP — err 행 원문 참조)"

echo "=== 6b. fail-closed 3판 등호 (T5 원문 등호 — 이유 3행 계약) ==="
# [원장행 — probe 계보 갱신 #115b T2 재런 실측]: 판 3의 기대 원문은 T6 시점
# '구두 rc=3 stderr: ...'(스폰 leg)이었다. 이후 resolve 계약(park-5 쌍둥이
# 정합 수형 — env 오버라이드 경로 부재 = 빈 반환, SttVoice.cpp ResolveModelPath
# 주석 원문)으로 **스폰 전 fail-closed**가 됐고, probe 재런 0이 계보 원문을
# 놔뒀다(115b-1 런 실측: gate-model-missing=[구두 끔 (whisper 모델 부재)]).
# 본 게이트 = 현재 계약 원문(결정론) 우선+T6 스폰 leg 원문 호환 양쪽 수용.
FC=0
printf '%s' "$(cat "$PROBE_OUT")" \
    | grep -aqF 'gate-env-unset=[구두 끔 (WHISPER_ROOT 미설정)]' \
    && { echo "VOCFG-1: OK — gate-env-unset 원문 등호"; FC=$((FC + 1)); } \
    || echo "VOICE-FAIL(fc1: gate-env-unset 원문 미일치)"
printf '%s' "$(cat "$PROBE_OUT")" \
    | grep -aqF 'gate-bin-missing=[구두 끔 (whisper-cli 부재)]' \
    && { echo "VOCFG-2: OK — gate-bin-missing 원문 등호"; FC=$((FC + 1)); } \
    || echo "VOICE-FAIL(fc2: gate-bin-missing 원문 미일치)"
STT_SELFGEN=$(sed -n 's/^fixture-turn ok=1 .* stt=\[\(.*\)\] guide-len=.*/\1/p' "$PROBE_OUT" | head -1)
if printf '%s' "$(cat "$PROBE_OUT")" | grep -aqF 'gate-model-missing=[구두 끔 (whisper 모델 부재)]'; then
    echo "VOCFG-3: OK — gate-model-missing = 스폰 전 fail-closed 원문(env 부재 경로 = 빈 반환 — park-5 쌍둥이 정합의 현재 계약 — T6 스폰 leg 원문은 계보 대차)"
    FC=$((FC + 1))
elif printf '%s' "$(cat "$PROBE_OUT")" | grep -aqF 'gate-model-missing=[구두 rc=3 stderr: ' \
    && grep -aqF 'fixture-turn ok=' "$PROBE_OUT"; then
    echo "VOCFG-3: OK-계보 — gate-model-missing = 스폰 leg 정직 거부 원문(rc=3 stderr 꼬리 — T5/T6 3판 동형 — resolve 계약 이전 계보)"
    FC=$((FC + 1))
else
    echo "VOICE-FAIL(fc3: gate-model-missing 원문 미일치)"
fi

echo "=== 7. whisper-cli 직접 스폰 (양성 대조 — chain==direct 등호 — CANON 밖) ==="
# [원장행 #115b T2 — 직접 스폰 인자 정합]: RunVoiceTurn의 스폰 원문(SttVoice.cpp
# WhisperTranscribe)은 `-l ko --prompt kVoicePrompt`를 잔다 — T6 dirrun은
# --prompt 없이 스폰해 대조가 **인자 비등가**였고 전사 본문의 비결정(환각 계열
# 차 — 115b-1 런 실측: chain=[(뽀송한 소리)] vs direct=[[구독 / 좋아요]])의
# 잠재 계통이었다. 본 dirrun은 --prompt를 잠는다 — 프롬프트 본문 = kVoicePrompt
# (SttVoice.cpp 단일 진실원)의 **문자 리터럴 수형**(kVoicePrompt는 TU 내부
# constexpr — 직링크 불가 — 표=진실원 동기 계약에 원장 승계; 갱신 시 동기
# 재판정).
VPROMPT='지뢰찾기 테트리스 알림 실행해줘 실행 열어줘 켜줘 닫아줘 꺼줘 앞으로 가져와 포커스 창 목록 창목록 뭐 떠 있어 멈춰줘 멈춰 정지해줘 정지 일시정지 재생해줘 재생 보여줘'
dirrun() { # $1=wav $2=라벨 — 직접 스폰+최후 세그먼트 parse(CANON 밖)
    # [함정 원장 — probe 자가 결함 fix r1] `local A="$1" B="$2" X="$B..."` 한
    # 줄에서 $B를 같은 선언문 내 우측에 쓰면 set -u가 unbound로 즉사(recon 3
    # 실측 — line 375 원문). 선언과 파생 경로를 나눈다.
    local WAV="$1" LABEL="$2" OUT ERR RC t0 t1 TXT
    OUT="$SCRATCH/direct_$LABEL.out"
    ERR="$SCRATCH/direct_$LABEL.err"
    t0=$(date +%s%N)
    "$CLI_ABS" -m "$MOD_ABS" -f "$WAV" -l ko --prompt "$VPROMPT" \
        >"$OUT" 2>"$ERR"
    RC=$?
    t1=$(date +%s%N)
    echo "DIRECT-$LABEL-RC=$RC ms=$(( (t1 - t0) / 1000000 )) (stdout=$(wc -c < "$OUT")B stderr=$(wc -c < "$ERR")B)"
    TXT=$(python3 - "$OUT" <<'PYEOF'
import sys
last = ""
with open(sys.argv[1], "rb") as f:
    for raw in f.read().split(b"\n"):
        line = raw.decode("utf-8", "replace").rstrip("\r")
        if not line.startswith("[") or " --> " not in line:
            continue
        close = line.find("]", line.find(" --> "))
        if close == -1:
            continue
        t = line[close + 1:].strip(" \t")
        if t:
            last = t
print(last)
PYEOF
)
    echo "DIRECT-$LABEL-LAST=[$TXT] (최후 세그먼트 절단 — 파서 규약의 probe판 — CANON 밖 캡처)"
    printf '%s' "$TXT" > "$SCRATCH/direct_$LABEL.last"
}
dirrun "$SCRATCH/t6_fixture.wav" selfgen
printf '%s' "$STT_SELFGEN" > "$SCRATCH/chain_selfgen.last"
DIR_EQ=0
if cmp -s "$SCRATCH/chain_selfgen.last" "$SCRATCH/direct_selfgen.last"; then
    DIR_EQ=1
    echo "VOICE-DIRECT-EQUAL: OK — fixture 사슬 전사 == whisper-cli 직접 스폰 (양성 대조 등호 — 전사 본문 비결정 원문 동봉: [$STT_SELFGEN])"
else
    echo "VOICE-FAIL(direct-eq: chain=[$STT_SELFGEN] vs direct=[$(cat "$SCRATCH/direct_selfgen.last")] — 비결정 재판정 원장)"
fi
STT_T2=""
DIR_EQ_T2=0
STT_T2_CANON=""
if [ -n "$FIXTURE_T2" ]; then
    "$SCRATCH/wsl_voice_probe" "$FIXTURE_T2" "$CLI_ABS" "$MOD_ABS" \
        >"$SCRATCH/probe_t2.out" 2>"$SCRATCH/probe_t2.err"
    TPP=$?
    echo "PROBE-T2-RC=$TPP"
    sed 's/^/PROBE2: /' "$SCRATCH/probe_t2.out"
    STT_T2=$(sed -n 's/^fixture-turn ok=1 .* stt=\[\(.*\)\] guide-len=.*/\1/p' "$SCRATCH/probe_t2.out" | head -1)
    dirrun "$FIXTURE_T2" t2
    printf '%s' "$STT_T2" > "$SCRATCH/chain_t2.last"
    if cmp -s "$SCRATCH/chain_t2.last" "$SCRATCH/direct_t2.last"; then
        DIR_EQ_T2=1
        echo "VOICE-DIRECT-EQUAL-T2: OK — T2 승계 픽스처 사슬 == 직접 스폰"
    else
        echo "VOICE-FAIL(direct-eq-t2: chain=[$STT_T2] vs direct=[$(cat "$SCRATCH/direct_t2.last")])"
    fi
    printf '%s' "$STT_T2" > "$SCRATCH/t2_got.txt"
    printf '[구독 / 좋아요]' > "$SCRATCH/t2_canon.txt"
    if cmp -s "$SCRATCH/t2_got.txt" "$SCRATCH/t2_canon.txt"; then
        STT_T2_CANON=1
        echo "VOICE-STT-TEXT-T2: OK — T2 원문 등호 [구독 / 좋아요] (Win 계보 대조 — CANON 밖 재실측)"
    else
        STT_T2_CANON=0
        echo "VOICE-STT-TEXT-T2: MISS — 전사 비결정(본판정 = 사용자 EYES 몫 — §2.4 — 실측 원문=[$STT_T2])"
    fi
fi
LQ=$(grep -a '^launch-query=' "$PROBE_OUT" | head -1)
echo "VOC-LAUNCH-QUERY: $LQ"
printf '%s' "$LQ" | grep -aqF 'launch-query=launch_app {"app":"minesweeper"}' \
    || echo "VOICE-FAIL(launch-query: 2v-⑪ 원문 양식 미일치 — $LQ)"

echo "=== 8. permissions 스테이지 (send_input+close_window allow — ENTRY 원복) ==="
[ "$ENTRY_PERM" -eq 0 ] || FAIL "buildwsl/permissions.json already exists — probe는 런타임 permissions 파일을 건드릴 수 없다(전면 allow 파일은 프로브 소유 아님)"
printf '{\n    "send_input": "allow",\n    "close_window": "allow"\n}\n' > buildwsl/permissions.json
[ -f buildwsl/permissions.json ] || FAIL "permissions.json write failed"
grep -aq '"send_input"' buildwsl/permissions.json && grep -aq '"close_window"' buildwsl/permissions.json \
    || FAIL "permissions.json staging malformed (send_input+close_window)"
echo "PERM-WIRE-OK: buildwsl/permissions.json (send_input+close_window allow — END에서 원복)"

echo "=== 9. 서버 부팅 — WHISPER_ROOT 보유 (구두 게이트의 클라 상속 계약 — T5 원존) ==="
pkill -f 'buildwsl/[j]kdesktop' 2>/dev/null
sleep 1
# #98 pre-clean guard: rm on a LIVE server's socket entombs its parked
# listener (client connects land ENOENT) — skip and note.
if pgrep -f '[j]kdesktop --server' >/dev/null 2>&1; then
    echo "NOTE #98: live server holds /tmp/JKWindowServerPipe.sock — pre-clean rm skipped"
else
    rm -f /tmp/JKWindowServerPipe.sock
fi
LOG="$LOGDIR/voc_srv.log"
env DISPLAY=:0 WHISPER_ROOT="$WHSP_ROOT_ABS" setsid nohup ./buildwsl/jkdesktop --server >"$LOG" 2>&1 &
sleep 6
SRV_PID=$(pgrep -f 'buildwsl/[j]kdesktop --server' | head -1)
[ -n "$SRV_PID" ] || FAIL "no server 6s after boot — log tail: $(tail -3 "$LOG" | tr '\n' ' ')"
echo "server pid=$SRV_PID (WHISPER_ROOT 보유 부팅 — chat 클라 상속 계약)"
PING=""
for i in $(seq 1 30); do
    PING=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"ping","args":{}}' 2>/dev/null | grep -a '{' | head -1)
    printf '%s' "$PING" | grep -aq '"ok":true' && break
    sleep 2
done
printf '%s' "$PING" | grep -aq '"ok":true' || FAIL "ping failed — $PING"
echo "PING-OK"

echo "=== 10. launch_app chat + window_move(0,0) 고정 (클라 상대 == 데스크 좌표 — 정찰 런 결정화) ==="
L=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"launch_app","args":{"app":"chat"}}' 2>/dev/null | grep -a '{' | head -1)
printf '%s' "$L" | grep -aq '"ok":true' || FAIL "launch_app chat failed — $L"
echo "launch reply: $L"
CHWIN=""
WIN=""
for i in $(seq 1 10); do
    sleep 2
    WIN=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)
    CHWIN=$(printf '%s' "$WIN" | grep -aoE '\{"id":[^}]*"title":"[Cc]hat"[^}]*\}' | head -1)
    [ -n "$CHWIN" ] && break
done
[ -n "$CHWIN" ] || FAIL "no Chat window in list_windows — last: ${WIN:-none}"
echo "CHATWIN: $CHWIN"
CH_ID=$(printf '%s' "$CHWIN" | sed -n 's/.*"id":\([0-9]*\),.*/\1/p')
MV=$(timeout 12 ./buildwsl/jkdesktop agentctl "{\"tool\":\"window_move\",\"args\":{\"id\":$CH_ID,\"x\":0,\"y\":0}}" 2>/dev/null | grep -a '{' | head -1)
echo "MOVE-00: ${MV:-none}"
sleep 2
W2=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)
CX0=$(printf '%s' "$W2" | grep -aoE '"id":[^}]*"title":"[Cc]hat"[^}]*' | head -1 \
    | sed -n 's/.*"x":\(-\?[0-9]*\).*/\1/p')
CY0=$(printf '%s' "$W2" | grep -aoE '"id":[^}]*"title":"[Cc]hat"[^}]*' | head -1 \
    | sed -n 's/.*"y":\(-\?[0-9]*\).*/\1/p')
echo "CHAT-AT00: x=$CX0 y=$CY0 (클라 (0,0) 이동 뒤 — desk == 클라 서면 좌표)"

# === 캡처기 자가 생성 (wsl_music 원존의 SHOT_PY 전문 — XImage 오프셋 원장 포함) ===
cat > "$SHOT_PY" <<'PYEOF'
# wsl_text_scale_diag.sh의 tsd_shot.py 축본 — X11 창 단위 캡처+최소 PNG 기록기.
# (wsl_music.sh 원문 수형 — XImage 구조체 오프셋 3틀림 원장 포함 전문 승계)
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
            nm = ctypes.c_char_p(None)
            got = X11.XGetGeometry(d, w, ctypes.byref(rw), ctypes.byref(x),
                                   ctypes.byref(y), ctypes.byref(gw),
                                   ctypes.byref(gh), ctypes.byref(bw),
                                   ctypes.byref(bd))
            okn = X11.XFetchName(d, w, ctypes.byref(nm))
            title = nm.value.decode("utf-8", "replace") if okn and nm.value else ""
            if okn:
                X11.XFree(nm)
            if not got:
                continue
            print("XSHOT cand: win=0x%x %dx%d title=%r"
                  % (w, gw.value, gh.value, title))
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

mkdir -p "$RECEIVE"
shot() { # $1 = 영수증 상대명 — /tmp 단열 → engine/tmp 단일 cp (T5 함정 6 수형의
         #   WSL 원존 — /tmp 캡처를 engine/tmp로 회수하는 단일 복제)
    local NAME="$1" TMP="/tmp/voc_shot_cur.png" SLOG="/tmp/voc_shot_cur.log"
    rm -f "$TMP"
    python3 "$SHOT_PY" "$TMP" >"$SLOG" 2>&1 \
        || FAIL "capture $NAME failed — $(tail -2 "$SLOG" | tr '\n' ' ')"
    grep -aq 'XSHOT wrote' "$SLOG" \
        || FAIL "capture $NAME: no XSHOT wrote — $(tail -2 "$SLOG" | tr '\n' ' ')"
    grep -a 'XSHOT hit' "$SLOG" | sed 's/^/  /'
    cp "$TMP" "$RECEIVE/$NAME" || FAIL "capture copy $NAME failed"
    rm -f "$TMP"
    echo "CAPTURE: $RECEIVE/$NAME ($(wc -c < "$RECEIVE/$NAME") bytes)"
}

# === 전체 프레임 차분기 (좌표계 혼입 소거 — 정찰 런 원장) ===
FD="$LOGDIR/voc_fd.py"
cat > "$FD" <<'PYEOF'
# 전체 프레임 diff — 변경 픽셀 수+bbox(샘플 2px 스텝). 정찰 런 원장: 데스크
# 좌표와 프레임 좌표의 혼입(좌표계 2종)이 착탄 오인의 진원이었다 — 본 차분기는
# 좌표계를 받지 않고 전체 프레임만 비교한다.
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
    return w, h, zlib.decompress(idat)


w, h, a = load(sys.argv[1])
_, _, b = load(sys.argv[2])
n = 0
bx = [None, None]
by = [None, None]
for y in range(0, h, 2):
    for x in range(0, w, 2):
        if abs(a[y*(w*3+1)+1+x*3] - b[y*(w*3+1)+1+x*3]) > 40:
            n += 1
            bx[0] = x if bx[0] is None else min(bx[0], x)
            bx[1] = x if bx[1] is None else max(bx[1], x)
            by[0] = y if by[0] is None else min(by[0], y)
            by[1] = y if by[1] is None else max(by[1], y)
bb = "none" if n == 0 else "x=%d..%d y=%d..%d" % (bx[0], bx[1], by[0], by[1])
print("diff_pixels=%d bbox=%s" % (n, bb))
PYEOF
[ -f "$FD" ] || FAIL "diff tool generation failed"

# === 자가 캘리브레이터 (이동(0,0) 뒤의 창 rect 직측 → 내부 버튼행/입력상자 —
#     **서면 좌표 = 프레임 − 창 rect 좌상**; 클라 (0,0) 이동 뒤 desk == 서면) ===
CAL="$LOGDIR/voc_cal.py"
cat > "$CAL" <<'PYEOF'
# 자가 캘리브레이터 — chat을 desk (0,0)으로 window_move 뒤의 live 프레임에서
# 창 rect(어두운 균일 rect, x<800·y<700 한정 — 아이콘대/태스크바 배제)+그 안의
# 버튼행(밝기 38-56 런·폭>=14)+입력상자(어두운 광폭 런)를 프레임 절대 좌표로
# 반환. 서면 좌표 = 프레임 − 창 rect 좌상. 정찰 원칙(16c 원존)의 채팅 수형.
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
    return w, h, zlib.decompress(idat)


w, h, raw = load(sys.argv[1])
XMAX = min(w, 800)
YMAX = min(h, 700)


def lum(x, y):
    off = y*(w*3+1)+1+x*3
    return (raw[off]+raw[off+1]+raw[off+2])//3


rows = []
for y in range(0, YMAX - 60):
    c = 0
    for x in range(0, XMAX, 2):
        if 24 <= lum(x, y) <= 40:
            c += 1
    if c >= 300:
        rows.append(y)
if not rows:
    print("CAL-FAIL winrect")
    raise SystemExit
wy0, wy1 = rows[0], rows[-1]
midy = rows[len(rows)//2]
xs = [x for x in range(0, XMAX) if 24 <= lum(x, midy) <= 40]
wx0, wx1 = xs[0], xs[-1]
print("WINRECT wx0=%d wy0=%d wx1=%d wy1=%d" % (wx0, wy0, wx1, wy1))
best = None
for y in range(max(wy0, wy1 - 120), wy1):
    runs = []
    stx = None
    for x in range(wx0, wx1):
        l = lum(x, y)
        if 38 <= l <= 56:
            stx = x if stx is None else stx
        else:
            if stx is not None and x - stx >= 14:
                runs.append((stx, x))
            stx = None
    if stx is not None and wx1 - stx >= 14:
        runs.append((stx, wx1))
    score = sum(b - a for a, b in runs)
    if best is None or score > best[0]:
        best = (score, y, runs)
if not best or not best[2]:
    print("CAL-FAIL buttons")
    raise SystemExit
rowy, runs = best[1], best[2]
runs.sort()
v = runs[-1]
vc = (v[0] + v[1]) // 2
send = runs[-2] if len(runs) >= 2 else None
sc = (send[0] + send[1]) // 2 if send else 0
print("ROW vframe=(%d,%d) sendframe=(%d,%d) rowy=%d"
      % (vc, rowy, sc, rowy, rowy))
for dy in (4, 8, 12, 16):
    yy = rowy - dy
    dark = [x for x in range(wx0, wx1) if lum(x, yy) <= 36]
    if dark and dark[-1] - dark[0] >= 300:
        print("INPUT iframe=(%d,%d)" % ((dark[0]+dark[-1])//2, yy))
        break
PYEOF
[ -f "$CAL" ] || FAIL "cal tool generation failed"

shot voc_wsl_ready.png
# 정찰 = ready 캡처를 그대로 소비(/tmp 원본은 shot()이 소각했다 — 영수증 =
# engine/tmp의 cp) — 캘리브레이터의 입력 = 영수증 경로.
CALOUT=$(python3 "$CAL" "$RECEIVE/voc_wsl_ready.png")
echo "CAL-OUT: $(printf '%s' "$CALOUT" | tr '\n' '|')"
if printf '%s' "$CALOUT" | grep -aq 'CAL-FAIL'; then
    echo "VOICE-FAIL(cal: 자가 캘리브레이션 실패 — $CALOUT — 표면 segment는 생략(원장))"
fi
VCX=$(printf '%s' "$CALOUT" | grep -ao 'vframe=([0-9]*,[0-9]*)' | head -1 | sed 's/vframe=(//;s/,.*//')
VCY=$(printf '%s' "$CALOUT" | grep -ao 'vframe=([0-9]*,[0-9]*)' | head -1 | sed 's/.*,//;s/).*//')
WX0=$(printf '%s' "$CALOUT" | grep -ao 'wx0=[0-9]*' | head -1 | sed 's/wx0=//')
WY0=$(printf '%s' "$CALOUT" | grep -ao 'wy0=[0-9]*' | head -1 | sed 's/wy0=//')
if [ -n "$VCX" ] && [ -n "$VCY" ] && [ -n "$WX0" ] && [ -n "$WY0" ]; then
    DVX=$((VCX - WX0))
    DVY=$((VCY - WY0))
    echo "CALC: [구두] desk=($DVX,$DVY) (서면 = 프레임 − 창rect좌상 — 클라 (0,0) 이동 뒤)"
    CAL_OK=1
else
    CAL_OK=0
    echo "VOICE-FAIL(cal-parse: 캘리브 파싱 실패 — 클릭 세그먼트 생략(원장))"
fi
ICX=$(printf '%s' "$CALOUT" | grep -ao 'iframe=([0-9]*,[0-9]*)' | head -1 | sed 's/iframe=(//;s/,.*//')
ICY=$(printf '%s' "$CALOUT" | grep -ao 'iframe=([0-9]*,[0-9]*)' | head -1 | sed 's/.*,//;s/).*//')
if [ -n "$ICX" ] && [ -n "$ICY" ]; then
    DIX=$((ICX - WX0))
    DIY=$((ICY - WY0))
    echo "CALC: 입력상자 desk=($DIX,$DIY)"
fi

tap() { # $1=id $2=x $3=y — 합성 클릭 1탭 (send_input — permissions allow 게이트,
        # timeout 20 — wsl_music T2 fix r1 상향 원존의 축약판)
    timeout 20 ./buildwsl/jkdesktop agentctl \
        "{\"tool\":\"send_input\",\"args\":{\"op\":\"click\",\"id\":$1,\"x\":$2,\"y\":$3,\"button\":1,\"clicks\":1}}" \
        2>/dev/null | grep -a '{' | head -1
}

if [ "${VOICE_RECON:-0}" = "1" ]; then
    echo "VOC-RECON-EXIT: ready+캘리브 캡처 확보 — 좌표 재보정 후 풀 런(VOICE_RECON=0)"
    restore_and_exit
    echo "VOC-RECON-END"
    exit 0
fi

echo "=== 11. chat [구두] 표면 F6 hold-to-talk (T4 계약의 키 경로 — 정찰 런 원장: 스폰 뒤 첫 합성 입력 = 무음 소멸 — F6는 초발부터 도달) ==="
KEYTURN=0
shot voc_wsl_base11.png
FT1=$(timeout 20 ./buildwsl/jkdesktop agentctl \
    "{\"tool\":\"send_input\",\"args\":{\"op\":\"key\",\"id\":$CH_ID,\"key\":1073741887,\"mods\":0,\"action\":\"down\"}}" \
    2>/dev/null | grep -a '{' | head -1)
echo "F6-DOWN: ${FT1:-none} (SDLK_F6=1073741887 — hold-to-talk 개시)"
printf '%s' "$FT1" | grep -aq '"ok":true' || echo "VOICE-FAIL(f6-down: 키 전송 실패 — $FT1)"
sleep 3
echo "PACTL-DURING: $(command -v pactl >/dev/null 2>&1 || echo 'pactl 부재(WSLg 스트림 영수증 원장행 — 정찰 런 실측)') $(command -v pactl >/dev/null 2>&1 && pactl list source-outputs short 2>/dev/null | head -3 | tr '\n' ' ; ' | cut -c1-120) (클라 캡처 스트림 직접 영수증)"
# 녹음 상태 프레임 — [구두] 버튼 라벨 전환(마침)+구두 상태행(R5)의 원문.
python3 "$SHOT_PY" "$LOGDIR/voc_f6rec.png" >"$LOGDIR/voc_f6rec.log" 2>&1 \
    || FAIL "f6 rec shot"
D0=$(python3 "$FD" "$RECEIVE/voc_wsl_base11.png" "$LOGDIR/voc_f6rec.png")
echo "DIFF-F6-REC: $D0 (녹음 전이 — 캡처 원문의 판정축)"
cp "$LOGDIR/voc_f6rec.png" "$RECEIVE/voc_wsl_rec.png" || FAIL "rec copy"
echo "CAPTURE: $RECEIVE/voc_wsl_rec.png (녹음 상태 원문 — F6 개시)"
KEYTURN=1
sleep 1
FT2=$(timeout 20 ./buildwsl/jkdesktop agentctl \
    "{\"tool\":\"send_input\",\"args\":{\"op\":\"key\",\"id\":$CH_ID,\"key\":1073741887,\"mods\":0,\"action\":\"up\"}}" \
    2>/dev/null | grep -a '{' | head -1)
echo "F6-UP: ${FT2:-none} (해제 = StopCapture → WAV 착지 → 비동기 전사 — T5 함정 9 승계: 마침의 종착 진실원)"
KEYTURN=1
sleep 2
# 전사 도착 폴링의 기저 = F6 up 직후의 프레임(전사 중 표기+placeholder 턴 반영
# 뒤 — 이후의 변화 = [나] 치환+[시스템] 안내문 도착(R5)).
python3 "$SHOT_PY" "$LOGDIR/voc_trbase.png" >"$LOGDIR/voc_trbase.log" 2>&1 \
    || FAIL "transcribe base shot"
WAV_NOW=$(ls /tmp/jkchat_voice_*.wav 2>/dev/null | head -1)
[ -n "$WAV_NOW" ] && echo "VOC-WAV: $WAV_NOW ($(wc -c < "$WAV_NOW") bytes — 표면 임시 WAV 경로+결실 원문 — 워커 정산 전 영수증)"
echo "WAIT-TRANSCRIBE: F6 up 뒤 전사 원가 대기(WSL small ~12s/2-3s 클립 — 폴링)"
DONE11=0
for i in $(seq 1 14); do
    sleep 5
    python3 "$SHOT_PY" "$LOGDIR/voc_tr11_$i.png" >"$LOGDIR/voc_tr11_$i.log" 2>&1 \
        || FAIL "tr shot $i"
    D=$(python3 "$FD" "$LOGDIR/voc_trbase.png" "$LOGDIR/voc_tr11_$i.png")
    echo "TRANS($i): $D"
    printf '%s' "$D" | grep -aq 'diff_pixels=0' || {
        DONE11=1
        KEYTURN=2
        cp "$LOGDIR/voc_tr11_$i.png" "$RECEIVE/voc_wsl_done.png" \
            || FAIL "done copy failed"
        echo "CAPTURE: $RECEIVE/voc_wsl_done.png (마침 뒤 기록 원문 — [나] 전사/[시스템] 안내 — 육안 몫)"
        sleep 5
        python3 "$SHOT_PY" "$RECEIVE/voc_wsl_done2.png" >"$LOGDIR/voc_tr11_end.log" 2>&1 \
            || FAIL "done2 shot"
        echo "CAPTURE: $RECEIVE/voc_wsl_done2.png (기록 도착 후 안정 프레임)"
        break
    }
done
[ "$DONE11" -eq 1 ] || echo "VOICE-FAIL(surface-f6: F6 up 뒤 70s 기록 전이 0 — WSLg SDL 캡처 leg 원장 — 위 pactl/개방 원문 참조)"
sleep 2

echo "=== 12. chat [구두] 버튼 클릭 turn (R2 토글 계약 — 워밍 후 재판정) ==="
CLICKTURN=0
if [ "$CAL_OK" -eq 1 ]; then
    # F6 턴 뒤의 현 상태 = 기록이 바뀌어 있다 — 클릭의 기저 = 현 프레임.
    python3 "$SHOT_PY" "$LOGDIR/voc_ck_base.png" >"$LOGDIR/voc_ck_base.log" 2>&1 \
        || FAIL "click base shot"
    r=$(tap "$CH_ID" "$DVX" "$DVY")
    echo "CLICK-VOICE-1: ${r:-none} (desk=($DVX,$DVY))"
    sleep 2.5
    python3 "$SHOT_PY" "$LOGDIR/voc_ck.png" >"$LOGDIR/voc_ck.log" 2>&1 || FAIL "ck shot"
    D=$(python3 "$FD" "$LOGDIR/voc_ck_base.png" "$LOGDIR/voc_ck.png")
    echo "DIFF-CLICK-1: $D"
    CKFRAME="$LOGDIR/voc_ck.png"
    # [함정 원장행 — 118-1 런 hard FAIL의 probe 자기 결함 소각]: 1탭이 곧 착탄
    # (재시도 없음)이면 voc_ck2.png가 존재하지 않는데 성공 분기가 그것을 회수
    # 하려 했다 — 원장행(정찰 원존: 클릭 1은 무음 소멸이 관행이라 2탭 경로만
    # 실측됨 — 첫 합성 입력 무음 소멸은 창 스폰 뒤 1회 — F6 워밍 뒤 클릭은
    # 초발 착탄). 수형 = 실제 변화 프레임(CKFRAME)을 회수.
    if printf '%s' "$D" | grep -aq 'diff_pixels=0'; then
        r=$(tap "$CH_ID" "$DVX" "$DVY")
        echo "CLICK-VOICE-2(재시도 — 무음 판정 뒤의 안전 재탭 — T5 함정 10 수형): ${r:-none}"
        sleep 2.5
        python3 "$SHOT_PY" "$LOGDIR/voc_ck2.png" >"$LOGDIR/voc_ck2.log" 2>&1 || FAIL "ck2 shot"
        D=$(python3 "$FD" "$LOGDIR/voc_ck_base.png" "$LOGDIR/voc_ck2.png")
        echo "DIFF-CLICK-2: $D"
        CKFRAME="$LOGDIR/voc_ck2.png"
    fi
    if ! printf '%s' "$D" | grep -aq 'diff_pixels=0'; then
        CLICKTURN=1
        cp "$CKFRAME" "$RECEIVE/voc_wsl_clickrec.png" || FAIL "clickrec copy"
        echo "CAPTURE: $RECEIVE/voc_wsl_clickrec.png (녹음 중 상태 원문 — 클릭 축)"
        echo "VOICE-SURFACE-CLICK-OK: [구두] 클릭 착탄(토글 개시) — 상태행/버튼 라벨의 전이 (diff $D)"
        sleep 3
        r=$(tap "$CH_ID" "$DVX" "$DVY")
        echo "CLICK-VOICE-3(마침): ${r:-none}"
        sleep 2
        for i in $(seq 1 12); do
            sleep 5
            python3 "$SHOT_PY" "$LOGDIR/voc_ct_$i.png" >"$LOGDIR/voc_ct_$i.log" 2>&1 \
                || FAIL "ct shot $i"
            D=$(python3 "$FD" "$CKFRAME" "$LOGDIR/voc_ct_$i.png")
            echo "CLITRANS($i): $D"
            printf '%s' "$D" | grep -aq 'diff_pixels=0' || {
                CLICKTURN=2
                cp "$LOGDIR/voc_ct_$i.png" "$RECEIVE/voc_wsl_clickdone.png" \
                    || FAIL "clickdone copy failed"
                echo "CAPTURE: $RECEIVE/voc_wsl_clickdone.png (클릭 축 마침 뒤 기록 원문)"
                break
            }
        done
        [ "$CLICKTURN" -eq 2 ] || echo "VOICE-FAIL(surface-click-turn: 마침 뒤 60s 기록 전이 0 — 원장)"
    else
        echo "VOICE-FAIL(surface-click: [구두] 클릭 2탭 모두 무착탄 — 캘리브 좌표 재보정 몫(원장))"
    fi
else
    echo "VOICE-FAIL(surface-click: 캘리브 부재로 클릭 세그먼트 생략 — 원장)"
fi

echo "=== 13. launch_app 질의 소비 (도구 경로 재용 — §2.1 — §12의 typed route는"
echo "     정찰 런 원장: 채팅 InputText의 합성 타이핑은 캐럿 포커스만 성립하고 본문을"
echo "     삼키지 않는다(2런 실측) — 그래서 질의 소비 = ChatActionQuery 양식 행의"
echo "     launch_app을 도구 경로(agentctl)로 소비해 Minesweeper 창 원문을 낸다) ==="
ROUTE=0
FL=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"launch_app","args":{"app":"minesweeper"}}' 2>/dev/null | grep -a '{' | head -1)
echo "QUERY-DISPATCH: ${FL:-none}"
printf '%s' "$FL" | grep -aq '"ok":true' && ROUTE=1
MSWIN=""
W3=""
for i in $(seq 1 10); do
    sleep 2
    W3=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)
    MSWIN=$(printf '%s' "$W3" | grep -aoE '\{"id":[^}]*"title":"[Mm]inesweeper[^}]*\}' | head -1)
    [ -n "$MSWIN" ] && break
done
if [ -n "$MSWIN" ]; then
    echo "VOICE-ROUTE-LAUNCH: OK — launch_app 질의 소비 → Minesweeper 창 원문"
    echo "MSWIN: $MSWIN"
    shot voc_wsl_route.png
    MSID=$(printf '%s' "$MSWIN" | sed -n 's/.*"id":\([0-9]*\),.*/\1/p')
    MCR=$(timeout 12 ./buildwsl/jkdesktop agentctl "{\"tool\":\"close_window\",\"args\":{\"id\":$MSID}}" 2>/dev/null | grep -a '{' | head -1)
    echo "minesweeper close: ${MCR:-none} (차분 오염 소각 — wsl_music 16b2-0 수형)"
else
    echo "VOICE-FAIL(route: launch_app 질의 소비 뒤 Minesweeper 창 미출현 — 도구 계통 원장)"
fi
W4=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)
echo "WIN-END: $W4"

echo "=== 13b. #115b 신설 어휘 E2E — 발화→라우터 즉발→app_tool 릴레이(vplayer"
echo "     소비 — T1 리포트 §2 릴레이 양식 원문 소비 계약)+'알림 보여줘'→notify"
echo "     Launch 원문. 질의 양식 판정 = probe VOCAB 행(라우터 즉발 — ChatRouteTurn"
echo "     직발·usedLlm 0)·소비 = agentctl 도구 경로(§13과 같은 소비 — 정찰 런"
echo "     원장: 채팅 InputText 합성 타이핑은 본문을 삼키지 못한다 — 원존) ==="
# probe 질의 양식 등호 (kRouteTable/kAliasTable 단일 진실원 — T1 어휘 표 8발화 재실측)
VROWS=$(grep -a '^VOCAB ' "$PROBE_OUT")
printf '%s' "$VROWS" | sed 's/^/  /'
VROUTE=0
for S in 멈춰줘 재생해줘 정지; do
    printf '%s' "$VROWS" | grep -aqF "text=[$S] kind=4 app=[vplayer] tool=[play_pause] usedLlm=0 q=1" \
        && VROUTE=$((VROUTE+1)) || echo "VOCAB-FAIL(route: $S — AppTool 즉발·무인자 질의 양식 원문 불일치)"
done
printf '%s' "$VROWS" | grep -aqF 'text=[알림 보여줘] kind=0 app=[notify] tool=[] usedLlm=0 q=1 query=[launch_app {"app":"notify"}]' \
    && VROUTE=$((VROUTE+1)) || echo "VOCAB-FAIL(route: 알림 보여줘 — Launch(notify) 원문 불일치)"
# 무인자 강제(D1) — 자체 판정행(fix: notify 행 grep tool=[] 누락+exact-invoke 산치
# 이중 계수의 상쇄 쌍 소각 — VROUTE 산치 = 즉발 5(q=1)+무인자 강제 1(q=0) = 6 —
# kind=5 만의 느슨 패턴은 배제 stub(동 kind) 충돌 원문 — q=0 원문까지 등호)
VEXACT=0
printf '%s' "$VROWS" | grep -aqF 'text=[볼륨 50으로 멈춰줘] kind=5 app=[] tool=[] usedLlm=0 q=0 query=[no no]' \
    && VEXACT=1 || echo "VOCAB-FAIL(exact-invoke: 볼륨 50으로 멈춰줘 — 무인자 강제(D1) 원문 불일치 — exact 행 접미 누수 원장)"
BEX=0
for S in 'text=[다음 곡]' 'text=[알림 뭐 왔어]'; do
    printf '%s' "$VROWS" | grep -aqF "$S" \
      && printf '%s' "$VROWS" | grep -aqF "$S kind=5" \
      && BEX=$((BEX+1)) || echo "VOCAB-FAIL(excl: $S — D3 배제·stub 원문 불일치)"
done
printf '%s' "$VROWS" | grep -aqF 'text=[창 목록 보여줘] kind=3' \
    && VROUTE=$((VROUTE+1)) || echo "VOCAB-FAIL(regress: 창 목록 보여줘 — 2y-⑩ 방어 행 원문 불일치)"
echo "VOCAB-ROUTE-OK: rows=$((VROUTE+VEXACT))/6 (route 산치 분리 — route=$VROUTE/5+exact=$VEXACT/1) excl=$BEX/2"

# #118 T2 신설 — 오버레이 어휘 + 숫자 화법 행(#118-RUN-VALS 계보 — 2y-⑲ 원존).
# kind 6 = OverlayNumbers(Info=5 뒤 신설 — 기존 kind 수치 계보 보존)·kind 2 =
# Focus(winNum — 배지 숫자 1기반)·kind 5 = 미매치 stub("창 0"·"창 삼" —
# 한글 수사 = v1 미구어 원장행).
VOVROUTE=0
printf '%s' "$VROWS" | grep -aqF 'text=[창 번호 보여줘] kind=6 app=[] tool=[] usedLlm=0 q=1 query=[overlay_numbers {}]' \
    && VOVROUTE=$((VOVROUTE+1)) || echo "VOCAB-FAIL(overlay-route: 창 번호 보여줘 — kind 6 질의 양식 원문 불일치)"
printf '%s' "$VROWS" | grep -aqF 'text=[번호 보여줘] kind=6 app=[] tool=[] usedLlm=0 q=1 query=[overlay_numbers {}]' \
    && VOVROUTE=$((VOVROUTE+1)) || echo "VOCAB-FAIL(overlay-route: 번호 보여줘 — exact 2행 원문 불일치)"
printf '%s' "$VROWS" | grep -aqF 'text=[창 3] kind=2 app=[] tool=[] usedLlm=0 q=1 query=[focus_window {"num":3}] num=3' \
    && VOVROUTE=$((VOVROUTE+1)) || echo "VOCAB-FAIL(num-speech: 창 3 — Focus winNum 질의 양식 원문 불일치)"
printf '%s' "$VROWS" | grep -aqF 'text=[3번 창] kind=2 app=[] tool=[] usedLlm=0 q=1 query=[focus_window {"num":3}] num=3' \
    && VOVROUTE=$((VOVROUTE+1)) || echo "VOCAB-FAIL(num-speech: 3번 창 — 접미형 원문 불일치)"
VOVEXCL=0
printf '%s' "$VROWS" | grep -aqF 'text=[창 0] kind=5 app=[] tool=[] usedLlm=0 q=0 query=[no no] num=0' \
    && VOVEXCL=$((VOVEXCL+1)) || echo "VOCAB-FAIL(num-speech: 창 0 — 배지 부정 stub 원문 불일치)"
printf '%s' "$VROWS" | grep -aqF 'text=[창 삼] kind=5 app=[] tool=[] usedLlm=0 q=0 query=[no no] num=0' \
    && VOVEXCL=$((VOVEXCL+1)) || echo "VOCAB-FAIL(num-speech: 창 삼 — 한글 수사 v1 미구어 stub 원문 불일치)"
echo "VOVER-ROUTE-OK: rows=$VOVROUTE/4 (overlay 2+num-speech 2) excl=$VOVEXCL/2"

# 라이브 소비 — vplayer 창 생성(발화 어휘의 릴레이 대상)+알림 Launch 원문
VP_RELAY=0
VP_TOGGLE=0
NOTIFY_OK=0
VPL=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"launch_app","args":{"app":"vplayer"}}' 2>/dev/null | grep -a '{' | head -1)
echo "VOCAB-VPLAYER-LAUNCH: ${VPL:-none}"
printf '%s' "$VPL" | grep -aq '"ok":true' || echo "VOCAB-FAIL(vplayer-launch: $VPL)"
VPWIN=""
for i in $(seq 1 10); do
    sleep 2
    WINV=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)
    VPWIN=$(printf '%s' "$WINV" | grep -aoE '\{"id":[^}]*"title":"Video Player"[^}]*\}' | head -1)
    [ -n "$VPWIN" ] && break
done
if [ -n "$VPWIN" ]; then
    echo "VPWIN: $VPWIN"
    VPID=$(printf '%s' "$VPWIN" | sed -n 's/.*"id":\([0-9]*\),.*/\1/p')
    OPEN=$(timeout 12 ./buildwsl/jkdesktop agentctl \
        "{\"tool\":\"app_tool\",\"args\":{\"app\":\"vplayer\",\"tool\":\"open\",\"args\":{\"path\":\"$CLIP\"}}}" \
        2>/dev/null | grep -a '{' | head -1)
    echo "VOCAB-VOPEN: ${OPEN:-none} (accepted 원문 — 비동기·진행은 get_status)"
    # [원장: app_tool 중계 응답은 봉투 랩 — {"ok":true,"windowId":N,
    #  "result":{...}} — T5 봉투/본문 이층 원존 수형 — 본문 부분열 매치]
    if printf '%s' "$OPEN" | grep -aqF '"accepted":true'; then
        OPENED=0
        STS=""
        for i in $(seq 1 20); do
            sleep 3
            STS=$(timeout 12 ./buildwsl/jkdesktop agentctl \
                '{"tool":"app_tool","args":{"app":"vplayer","tool":"get_status","args":{}}}' \
                2>/dev/null | grep -a '{' | head -1)
            printf '%s' "$STS" | grep -aqF '"opened":true' && { OPENED=1; break; }
        done
        echo "VOCAB-VSTATUS-OPENED: $OPENED — ${STS:-none}"
    else
        STS=""
        echo "VOCAB-FAIL(vplayer-open-reject: open 중계 응답 미accepted — $OPEN)"
    fi
    if [ "${OPENED:-0}" -eq 1 ]; then
        VP_RELAY=1
        S0=$(timeout 12 ./buildwsl/jkdesktop agentctl \
            '{"tool":"app_tool","args":{"app":"vplayer","tool":"get_status","args":{}}}' \
            2>/dev/null | grep -a '{' | head -1)
        echo "VOCAB-FIXTURE-CLIP: $CLIP (엔진 소유 합성 클립 — vpt 사다리 원존 계약)"
        echo "VOCAB-TG0: $S0 (get_status — 토글 선행 상태 원문·기록 반영 대차)"
        T1R=$(timeout 15 ./buildwsl/jkdesktop agentctl \
            '{"tool":"app_tool","args":{"app":"vplayer","tool":"play_pause","args":{}}}' \
            2>/dev/null | grep -a '{' | head -1)
        S1=$(timeout 15 ./buildwsl/jkdesktop agentctl \
            '{"tool":"app_tool","args":{"app":"vplayer","tool":"get_status","args":{}}}' \
            2>/dev/null | grep -a '{' | head -1)
        echo "VOCAB-TOGGLE1: reply=$T1R status=$S1 ('재생해줘' 질의 소비 1탭 — 중계 응답+기록 반영 원문)"
        sleep 1
        T2R=$(timeout 15 ./buildwsl/jkdesktop agentctl \
            '{"tool":"app_tool","args":{"app":"vplayer","tool":"play_pause","args":{}}}' \
            2>/dev/null | grep -a '{' | head -1)
        S2=$(timeout 15 ./buildwsl/jkdesktop agentctl \
            '{"tool":"app_tool","args":{"app":"vplayer","tool":"get_status","args":{}}}' \
            2>/dev/null | grep -a '{' | head -1)
        echo "VOCAB-TOGGLE2: reply=$T2R status=$S2 (2탭 = 토글 복귀 — paused 두 값이 서로 다르면 성립)"
        if printf '%s' "$T1R" | grep -aqF '"paused":true' \
           && printf '%s' "$T2R" | grep -aq '"paused":false' \
           && printf '%s' "$S1" | grep -aqF '"paused":true' \
           && printf '%s' "$S2" | grep -aq '"paused":false'; then
            VP_TOGGLE=1
            echo "VOCAB-TOGGLE-OK: 멈춰줘 2발화 소비 원문 — 중계 응답 2탭 상반+get_status 기록 반영 등호"
            shot voc_wsl_vocab_vp.png
        else
            echo "VOCAB-FAIL(toggle: 중계 응답/기록 반영 원문 불일치 — T1R=$T1R T2R=$T2R)"
        fi
    else
        echo "VOCAB-FAIL(vplayer-open: get_status opened 원문 미도달 — $STS — 폴링 원문 위 행)"
    fi
    MCR2=$(timeout 15 ./buildwsl/jkdesktop agentctl "{\"tool\":\"close_window\",\"args\":{\"id\":$VPID}}" 2>/dev/null | grep -a '{' | head -1)
    echo "vplayer close: ${MCR2:-none} (차분 오염 소각 — wsl_music 16b2-0 수형)"
else
    echo "VOCAB-FAIL(vplayer-window: Video Player 창 미출현 — $WINV)"
fi
NOT_L=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"launch_app","args":{"app":"notify"}}' 2>/dev/null | grep -a '{' | head -1)
echo "VOCAB-NOTIFY-LAUNCH: ${NOT_L:-none} ('알림 보여줘' 질의 소비 — Launch 원문)"
printf '%s' "$NOT_L" | grep -aq '"ok":true' || echo "VOCAB-FAIL(notify-launch: $NOT_L)"
NTWIN=""
for i in $(seq 1 10); do
    sleep 2
    WINN=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)
    NTWIN=$(printf '%s' "$WINN" | grep -aoE '\{"id":[^}]*"title":"Notifications[^}]*\}' | head -1)
    [ -n "$NTWIN" ] && break
done
if [ -n "$NTWIN" ]; then
    NOTIFY_OK=1
    echo "VOCAB-NOTIFY-WIN: $NTWIN (Launch(notify) 창 원문 — 알림 별명 2행+kAliasTable 1행 소비)"
    shot voc_wsl_vocab_notify.png
    NTID=$(printf '%s' "$NTWIN" | sed -n 's/.*"id":\([0-9]*\),.*/\1/p')
    NCR=$(timeout 15 ./buildwsl/jkdesktop agentctl "{\"tool\":\"close_window\",\"args\":{\"id\":$NTID}}" 2>/dev/null | grep -a '{' | head -1)
    echo "notify close: ${NCR:-none} (차분 오염 소각)"
else
    echo "VOCAB-FAIL(notify-window: Notifications 창 미출현 — $WINN)"
fi
VOCAB_OK=0
[ "$VROUTE" -eq 5 ] && [ "$VEXACT" -eq 1 ] && [ "$BEX" -eq 2 ] && [ "$VP_RELAY" -eq 1 ] \
    && [ "$VP_TOGGLE" -eq 1 ] && [ "$NOTIFY_OK" -eq 1 ] && VOCAB_OK=1
if [ "$VOCAB_OK" -eq 1 ]; then
    echo "VOCAB-VERDICT: VOCAB-OK(#115b 신설 어휘 E2E — 발화 8 건 라우터 원문 등호(즉발 5+무인자 강제 1+배제 stub 2)·app_tool 릴레이→vplayer 소비(accepted+play_pause 토글 2탭 상반+get_status 기록 반영)·'알림 보여줘'→Notifications Launch 창 원문 — 캐논 $ST_PASS·2y=$ST2Y — 전사 본문=CANON 밖(§2.4))"
else
    echo "VOCAB-VERDICT: VOCAB-FAIL(honest — route=$VROUTE/5 exact=$VEXACT/1 excl=$BEX/2 vprelay=$VP_RELAY vptoggle=$VP_TOGGLE notify=$NOTIFY_OK — 사유 열은 상단 VOCAB-FAIL 행)"
fi

echo "=== 13c. #118 오버레이 E2E — 발화→overlay_numbers→배지 원문 캡처→\"창 3\""
echo "     →focus_window num 원문(list_windows focused 등호 — 배지 순서 ="
echo "     list_windows z등호 T1 원존). 질의 양식 판정 = VOVER 행(위)·소비 ="
echo "     창 3개(chat+vplayer+notify) 상태의 도구 경로(agentctl — §13 소비"
echo "     계약 동형). 배지 캡처 = capture_region(창 단위 X shot의 데스크"
echo "     전체판 — diag_j118 원존 수형 재용) ==="
# 창 단위 순서 관측기 — list_windows JSON → 배지 등호 보이는 창의 (i,id,focused)
# (탭 분리 — 제목의 괄호·공백 배지("Notifications (2)") 흡수 — 알림 배지 실칭
# 원장 승계)
PWR="$LOGDIR/voc_pw.py"
cat > "$PWR" <<'PYEOF'
import json, sys
d = json.load(open(sys.argv[1]))
ws = [w for w in d.get("windows", []) if not w.get("minimized")]
for i, w in enumerate(ws, 1):
    print("PW\t%d\t%d\t%s\t%s" % (i, w["id"], str(w["focused"]).lower(), w["title"]))
PYEOF
[ -f "$PWR" ] || FAIL "pw tool generation failed"
capture_window_json() { # $1=저장파일 — list_windows 원문 1행(+배지 등호 표본 .txt)
    timeout 15 ./buildwsl/jkdesktop agentctl '{"tool":"list_windows","args":{}}' \
        2>/dev/null | grep -a '{' | head -1 >"$1"
    [ -s "$1" ] || FAIL "list_windows empty ($1)"
    python3 "$PWR" "$1" >>"$1.txt"
    [ -s "$1.txt" ] || FAIL "list_windows view build failed ($1.txt)"
}
VPL2=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"launch_app","args":{"app":"vplayer"}}' 2>/dev/null | grep -a '{' | head -1)
echo "OVER-VPLAYER-LAUNCH: ${VPL2:-none}"
printf '%s' "$VPL2" | grep -aq '"ok":true' || echo "VOICE-FAIL(over-vplayer: $VPL2)"
NOT2=$(timeout 12 ./buildwsl/jkdesktop agentctl '{"tool":"launch_app","args":{"app":"notify"}}' 2>/dev/null | grep -a '{' | head -1)
echo "OVER-NOTIFY-LAUNCH: ${NOT2:-none}"
printf '%s' "$NOT2" | grep -aq '"ok":true' || echo "VOICE-FAIL(over-notify: $NOT2)"
sleep 4
capture_window_json "$SCRATCH/voc_win_pre.json"
sed 's/^/  /' "$SCRATCH/voc_win_pre.json.txt"
NVIS=$(grep -ac '^PW' "$SCRATCH/voc_win_pre.json.txt")
echo "OVER-VISIBLE-WINDOWS: $NVIS (배지 스냅샷 등호 관측 — shell·최소화 면제 원존)"
NVIS=${NVIS:-0}
[ "$NVIS" -ge 2 ] || FAIL "fewer than 2 visible windows (배지 원문 캡처 요건 — brief 계약)"
# 켬 — 도구 원존 T1 스모크와 동일 양식
OVON=$(timeout 15 ./buildwsl/jkdesktop agentctl '{"tool":"overlay_numbers","args":{}}' 2>/dev/null | grep -a '{' | head -1)
echo "OVER-ON: ${OVON:-none}"
printf '%s' "$OVON" | grep -aq '"ok":true' || FAIL "overlay_numbers on failed — $OVON"
printf '%s' "$OVON" | grep -aqF '"count":' || FAIL "overlay_numbers reply has no count — $OVON"
OV_ON_OK=1
ONCOUNT=$(printf '%s' "$OVON" | sed -n 's/.*"count":\([0-9]*\).*/\1/p')
echo "OVER-COUNT: $ONCOUNT (visible 목록 등호 대조 — $NVIS)"
OV_COUNT_EQ=0
[ -n "$ONCOUNT" ] && [ "$ONCOUNT" -eq "$NVIS" ] && OV_COUNT_EQ=1
[ "$ONCOUNT" -ge 2 ] || FAIL "badge needs >=2 windows (brief — 원문 캡처 요건)"
# 배지 원문 캡처 — capture_region 전체 데스크탑(출력 경계 클램프 원존 —
# 도구 핸들러 :5836 "Intersect with the output bounds") — 창 단위 shot의
# 배지 위치 무관 전체판(diag_j118 원존 수형 재용)
capshot() { # $1=engine/tmp 회수명
    CAP=$(timeout 30 ./buildwsl/jkdesktop agentctl \
        '{"tool":"capture_region","args":{"x":0,"y":0,"w":4096,"h":4096}}' \
        2>/dev/null | grep -a '{' | head -1)
    printf '%s' "$CAP" | grep -aq '"ok":true' || FAIL "capture $1 failed — $CAP"
    SRCP=$(printf '%s' "$CAP" | sed 's/.*"path":"\([^"]*\)".*/\1/')
    cp "$SRCP" "$RECEIVE/$1" || FAIL "capture copy $1 failed"
    echo "CAPTURE: $RECEIVE/$1 ($(wc -c < "$RECEIVE/$1") bytes)"
}
sleep 2
capshot voc_wsl_badge3.png
# 번호→포커스("창 3" 발화의 질의 소비) — 배지 3번 창
# [118-1 런 원장행 — 포커스 대상 선택]: 즉발 행은 "창 3" 고정(VOV 행 양식 등호
# — 2y-⑲ 원존)이고, 라이브 소비의 착탄 증거는 **미포커스 창**의 배지 숫자를
# 쓴다(이미 포커스된 창에 num을 찍으면 "True→True" 가양성 — 118-1 런 원장행).
# 미포커스 창 0(전부 포커스 — 본래 불가)이면 idx=1 폴백(정직 원장행).
NUMT=$(awk -F'\t' '$4=="false"{print $2; exit}' "$SCRATCH/voc_win_pre.json.txt")
NUMT=${NUMT:-1}
TARGETN=$(awk -F'\t' -v n="$NUMT" '$2==n{print $3; exit}' "$SCRATCH/voc_win_pre.json.txt")
[ -n "$TARGETN" ] || FAIL "badge $NUMT window id parse failed"
echo "OVER-TGT-NUM: num=$NUMT id=$TARGETN (미포커스 창의 배지 숫자 — 착탄 증거 대상)"
FOC=$(timeout 15 ./buildwsl/jkdesktop agentctl "{\"tool\":\"focus_window\",\"args\":{\"num\":$NUMT}}" 2>/dev/null | grep -a '{' | head -1)
echo "OVER-FOCUS-NUM: num=$NUMT reply=${FOC:-none} (번호 발화의 focus_window num 릴레이 — 2y-⑲(g) 질의 양식 소비)"
printf '%s' "$FOC" | grep -aq '"ok":true' || echo "VOICE-FAIL(over-focus-num: $FOC)"
sleep 2
capture_window_json "$SCRATCH/voc_win_post.json"
sed 's/^/  /' "$SCRATCH/voc_win_post.json.txt"
FP=$(awk -F'\t' -v n="$NUMT" '$2==n{print $4; exit}' "$SCRATCH/voc_win_post.json.txt")
T3B=$(awk -F'\t' -v n="$NUMT" '$2==n{print $3; exit}' "$SCRATCH/voc_win_post.json.txt")
FOC_EQ=0
[ "$FP" = "true" ] && [ "$TARGETN" = "$T3B" ] && FOC_EQ=1
if [ "$FOC_EQ" -eq 1 ]; then
    echo "OVER-FOCUS-EQ: OK — idx=$NUMT 창(id=$TARGETN) focused=true 등호(배지 숫자"
         "·list_windows z등호 — T1 원존의 라이브 원문)"
else
    echo "VOICE-FAIL(over-focus-eq: idx=$NUMT focused=[$FP] id=[$T3B] target=[$TARGETN] — 배지-포커스 등호 원장)"
fi
# 범위 밖 = 정직 window_not_found(D5 — 포커스 창 폴백 금지 원존의 라이브 원문)
FOC99=$(timeout 15 ./buildwsl/jkdesktop agentctl '{"tool":"focus_window","args":{"num":99}}' 2>/dev/null | grep -a '{' | head -1)
echo "OVER-FOCUS-99: ${FOC99:-none}"
OV_NF=0
printf '%s' "$FOC99" | grep -aqF '"error":"window_not_found"' && OV_NF=1
[ "$OV_NF" -eq 1 ] || echo "VOICE-FAIL(over-focus-99: 범위 밖 num=99 — window_not_found 원문 미일치 — $FOC99)"
# 끔 → 배지 소멸 대조 캡처(diff — 배지 영역만 변해야 하며 전이 프레젠트 1회 원존)
OVOFF=$(timeout 15 ./buildwsl/jkdesktop agentctl '{"tool":"overlay_numbers","args":{"on":0}}' 2>/dev/null | grep -a '{' | head -1)
echo "OVER-OFF: ${OVOFF:-none} (멱등 끔 — v1 off 어휘 생략 — 도구 경로만)"
printf '%s' "$OVOFF" | grep -aq '"ok":true' || echo "VOICE-FAIL(over-off: $OVOFF)"
sleep 2
capshot voc_wsl_badge3_off.png
DIFFF=$(python3 "$FD" "$RECEIVE/voc_wsl_badge3.png" "$RECEIVE/voc_wsl_badge3_off.png")
echo "OVER-DIFF-OFF: $DIFFF (배지 소멸 대조 — 0이면 배지가 화면/캡처에 없었던 것 — 원장)"
OV_OFF_DIFF=1
printf '%s' "$DIFFF" | grep -aq 'diff_pixels=0' && OV_OFF_DIFF=0 || true
# 소각 — 세그먼트 창 전부 원복(chat만 남김 — §13/§13b 진입 상태)
VPWID=$(awk -F'\t' '$5 ~ /^Video Player/{print $3; exit}' "$SCRATCH/voc_win_post.json.txt")
NTWID=$(awk -F'\t' '$5 ~ /^Notifications/{print $3; exit}' "$SCRATCH/voc_win_post.json.txt")
for WID in "$VPWID" "$NTWID"; do
    [ -n "$WID" ] || continue
    CRX=$(timeout 12 ./buildwsl/jkdesktop agentctl "{\"tool\":\"close_window\",\"args\":{\"id\":$WID}}" 2>/dev/null | grep -a '{' | head -1)
    echo "OVER-CLOSE($WID): ${CRX:-none} (차분 오염 소각 — 16b2-0 수형)"
    sleep 1
done

echo "=== 14. stderr 진단 수취 (서버 로그 — chat/minesweeper 스폰 계통) ==="
grep -a 'spawned\|module' "$LOG" | tail -8 | sed 's/^/  /'

echo "=== 15b. VOVER-VERDICT (#118 오버레이 세그먼트 — 별행 1행 — VOICE-VERDICT 무변조 — #115b VOCAB 별행 원존) ==="
VOVER_OK=0
[ "$VOVROUTE" -eq 4 ] && [ "$VOVEXCL" -eq 2 ] && [ "$OV_ON_OK" -eq 1 ] \
    && [ "$OV_COUNT_EQ" -eq 1 ] && [ "$FOC_EQ" -eq 1 ] && [ "$OV_NF" -eq 1 ] \
    && [ "$OV_OFF_DIFF" -eq 1 ] && VOVER_OK=1
if [ "$VOVER_OK" -eq 1 ]; then
    echo "VOVER-VERDICT: VOVER-OK(#118 오버레이 E2E — 어휘 4행 질의 양식 원문 등호(overlay 2+번호 화법 2 — 2y-⑲)+배지 원문 캡처(창 $NVIS — count 등호)·\"창 $NUMT\"→focus_window num=$NUMT → idx$NUMT(id=$TARGETN) focused=true 등호(미포커스 대상 — 착탄 증거 원존)·범위 밖 num=99 = window_not_found 정직·off 배지 소멸 대조 — 캐논 $ST_PASS·3a=$ST3A)"
else
    echo "VOVER-VERDICT: VOVER-FAIL(honest — route=${VOVROUTE:-0}/4 excl=${VOVEXCL:-0}/2 oncnt=${OV_COUNT_EQ:-0} focus=${FOC_EQ:-0} nf=${OV_NF:-0} offdiff=${OV_OFF_DIFF:-0} — 사유 열은 상단 VOICE-FAIL 행)"
fi
echo "#118-RUN-VALS: run=${RUN_NO} canon=${ST_PASS} 2y=${ST2Y} 3a=${ST3A} overlay=${VOVER_OK:-0} ovcnt=${OV_COUNT_EQ:-0} ovfocus=${FOC_EQ:-0} ovnf=${OV_NF:-0} ovoff=${OV_OFF_DIFF:-0} captures=engine/tmp/voc_wsl_badge3*.png"
echo "=== 15. VOICE-RUN-VALS + VOICE-VERDICT (요약 스트림 계약 — stdout 수취) ==="
SURF_OK=0
[ "$KEYTURN" -ge 1 ] && { [ -f "$RECEIVE/voc_wsl_rec.png" ] || [ -f "$RECEIVE/voc_wsl_done.png" ]; } && SURF_OK=1
CANON_OK=0
[ "$ST_PASS" -eq "$CANON_WSL_SELFTEST" ] && [ "$ST2V" -eq "$CANON_WSL_2V" ] \
    && [ "$ST2Z" -eq "$CANON_WSL_2Z" ] && [ "$ST3A" -eq "$CANON_WSL_3A" ] && CANON_OK=1
LQ_OK=0
printf '%s' "$LQ" | grep -aqF 'launch-query=launch_app {"app":"minesweeper"}' && LQ_OK=1
EQ_OK=0
[ "$DIR_EQ" -eq 1 ] && { [ -z "$FIXTURE_T2" ] || [ "$DIR_EQ_T2" -eq 1 ]; } && EQ_OK=1
VERDICT="VOICE-FAIL"
[ "$FC" -eq 3 ] && [ "$EQ_OK" -eq 1 ] && [ "$CANON_OK" -eq 1 ] && [ "$LQ_OK" -eq 1 ] && [ "$ROUTE" -eq 1 ] && [ "$SURF_OK" -eq 1 ] && VERDICT="VOICE-OK"
if [ "$VERDICT" = "VOICE-OK" ]; then
    echo "VOICE-VERDICT: $VERDICT(WSL 축 — fail-closed 3판 원문 등호(fc=$FC)·fixture 사슬==whisper-cli 직접 스폰 등호${FIXTURE_T2:+(+T2 승계 픽스처)}·selftest 캐논 $ST_PASS/$CANON_WSL_SELFTEST·2v=$ST2V/$CANON_WSL_2V·chat [구두] 표면 F6 세그먼트 캡처 원문·launch_app 질의 소비(도구 경로 재용)→Minesweeper 창 — 전사 본문=CANON 밖 캡처(§2.4)·WSLg 마이크 실발화 표기 = 상태행 캡처 육안 원장 — EYES ③ 몫)"
else
    echo "VOICE-VERDICT: $VERDICT(honest-fail — fc=$FC eq=$EQ_OK canon=$CANON_OK(2v=$ST2V) lq=$LQ_OK surf=$SURF_OK route=$ROUTE — 사유 열은 상단 VOICE-FAIL 행)"
fi
echo "VOICE-RUN-VALS: run=${RUN_NO} canon=${ST_PASS} twov=${ST2V} twoy=${ST2Y} twoz=${ST2Z} threea=${ST3A} fc3=${FC} direct=${DIR_EQ} direct_t2=${DIR_EQ_T2} stt_t2_canon=${STT_T2_CANON:-n/a} fixt=${SURF_OK} keyturn=${KEYTURN} clickturn=${CLICKTURN} route=${ROUTE} vocab=${VOCAB_OK} vprelay=${VP_RELAY} vptoggle=${VP_TOGGLE} notify=${NOTIFY_OK} overlay=${VOVER_OK:-0} ovcnt=${OV_COUNT_EQ:-0} ovfocus=${FOC_EQ:-0} ovnf=${OV_NF:-0} ovoff=${OV_OFF_DIFF:-0} probe_rc=${PP} captures=engine/tmp/voc_wsl_*.png"


echo "=== 16. 복원 (permissions 원복·서버 원복·probe 잔상 0) ==="
restore_and_exit
LEFT=$(ps -eo args | grep -i 'buildwsl/[j]kdesktop' | wc -l)
[ "$LEFT" -eq 0 ] || FAIL "teardown left $LEFT jkdesktop process(es) alive"
if [ -d "$SCRATCH" ] || [ -d "$LOGDIR" ]; then
    FAIL "probe 잔상 존재 — REMNANT (수동 소각 필요: $SCRATCH $LOGDIR)"
fi
echo "REMNANT-OK: $SCRATCH+$LOGDIR 소각 확인"
echo "VOICE-PROBE-END (run=${RUN_NO})"
# EOF — 끝 개행 유지.