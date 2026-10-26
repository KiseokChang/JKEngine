#!/usr/bin/env bash
# T6 폰 구두 E2E probe (#112 — plan task-6). T5 원문의 형식화: 폰 [구두] 표면
# 절차(t112_t5_voice.sh+t112_t5_chat_tap.sh — T5 리포트 §6 "Produces(T6 소비)")
# 을 probe 산출물로 일원화 — **폰 E2E = T5 원문 승계 + 재측정 런 원문**.
#
# 절차(3단):
#   A. capture/사슬 probe(폰 $TMPDIR — clang++ buildterm 직링크 — T5 사다리
#      원존) — Termux:API 개방 3s→ffmpeg 정규화→whisper-cli 전사 1회+gate
#      3판+fixture 사슬+직접 스폰 대조 = T5 리포트 §3의 PROBE 행 원문 재실측.
#   B. 서버(WHISPER_ROOT 보유 — 구두 게이트의 클라 상속 계약 — T5 부팅 계약
#      계승) 부팅+chat 개봉+[구두] 버튼 탭(T5 실측 좌표 기본 — env 재보정) →
#      폰 E2E(개시→녹음→마침→전사→기록) — send_input 데스크 좌표 탭(T5 절차
#      원존)·캡처 3종 md5 등호 회수·permissions 병합+END 바이트 원복(함정 7
#      수형)·tap 런 반복(PHV_TAP_RUNS — 기본 2 = 재측정 런 2).
#   B2. #115b T2 신설 어휘 라이브 소비 세그먼트(WSL판 wsl_voice.sh §13b 동형 —
#      발화 8건 라우터 원문 등호 = §A probe VOCAB 행·app_tool 릴레이→vplayer
#      소비(합성 wav open+토글 2탭+get_status 대차)·'알림 보여줘'→Notifications
#      Launch 원문 — 폰 캡처 2종 회수+잔상 소각·permissions 바이트 원복.
#   C. 복원 — chat close+Music 창 복원(T5 restore 계약)+폰 $TMPDIR 잔상 소각.
#
# 원존 수혀·함정 승계(T5 함정 12건 중 2·6·7·9·10 = 본 probe의 실측 관행):
#   * (2) Termux:API 실측 어휘 = -f 개방/-q 종착/-i 세션 판정(start/stop 아님) —
#         probe leg가 소비하는 계약(SttVoice.cpp의 폰 수형 원장).
#   * (6) Windows python의 MSYS 경로 찢김 → recover() = cygpath -w 폴백 전문.
#   * (7) send_input의 파일 부재 기본 = Ask — 승인 구독자 없는 세션 = 파킹 봉쇄
#         → permissions 병합(핫리드)+END 바이트 원복(t112_t5 수형).
#   * (9) -l 60 상한의 안전판 의존 금지 — StopCapture(-q)가 종착 진실원.
#   * (10) 상태 전이 판독 = 캡처 2장 이상(done 바이트 증가 = 기록 행 렌 render의
#         영수증)+termux -i 진실원(렌더 스태일 감안) → 본 probe의 정찰 =
#         VOICE_RECON=1(ready 캡처 후 복원·종료 — 좌표 재보정 런).
#   * ssh 다중행 인라인 블록의 본문 pkill 브래킷 자기 세션 즉사(함정 1) → 원격
#     스크립트 = 로컬 파일 발행→ssh stdin push→rm -f -- "$0" 자기 소각(원존).
#   * 폰 /tmp 쓰기 불가(정찰 실측) → $TMPDIR 몫(phone_chat 원존).
#
# CANON 밖(§2.4): 전사 본문(무발화 환각·fixture 전사)은 VOICE-PHONE 원문 행
#   캡처만 — selftest 녹임 0. 판정 어설션 = gate 3판 원문 등호+probe leg의
#   mic/fixture 행+탭 런의 캡처 회수 등호+done>rec 바이트 증가(기록 도착의
#   결정 proxy — 육안 행 원문은 EYES 몫).
#
# 실행 표준(윈도 Git Bash, 저장소 루트 어디서든):
#   MSYS_NO_PATHCONV=1 bash engine/tools/probes/phone_voice.sh \
#     > engine/tmp/phone_voice_run.log 2>&1
# 환경변수 전부 옵션: PHONE_HOST/PHONE_PORT/PHONE_USER/PHONE_KEY·
# PHV_TAP_RUNS(2=기본)·PHV_BTN_X/PHV_BTN_Y(835/602 — T5 실측: chat 표준 기동
# = (172,114) 720x540·[구두] = 데스크 (835,602)).
set -u
export MSYS_NO_PATHCONV=1

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT" || exit 1
SCRATCH="$ROOT/engine/tmp"
mkdir -p "$SCRATCH"
RLOG="$SCRATCH/phone_voice_run.log"
FAIL() { echo "PHONE-VOICE-FAIL: $*"; exit 1; }

PHV_TAP_RUNS=${PHV_TAP_RUNS:-2}
# [fix r2 — env 지정 판별은 **기본치 채우기 전에** 수취해야 한다(115b-2phone
# 런 원장행: 상단 기정을 먼저 채워 SET 판별이 만능화 — AUTO 재산출 스킵 →
# tap 2런 전량 (835,602) 미달 반복)].
PHV_BTN_X_SET=${PHV_BTN_X:-}
PHV_BTN_Y_SET=${PHV_BTN_Y:-}
PHV_BTN_X=${PHV_BTN_X_SET:-835}
PHV_BTN_Y=${PHV_BTN_Y_SET:-602}

PHONE_HOST="${PHONE_HOST:-}"
if [ -z "$PHONE_HOST" ]; then
  CANDS=$(awk '{split($1,a,","); for(k in a) print a[k]}' "$HOME/.ssh/known_hosts" 2>/dev/null |
          sed 's/^\[//;s/\]:.*$//;s/:.*$//' | sort -u)
  for c in $CANDS; do
    case "$c" in \|1\|*) continue;; esac
    out=$(ssh -p "${PHONE_PORT:-8022}" -o BatchMode=yes -o ConnectTimeout=6 \
          -i "${PHONE_KEY:-$HOME/.ssh/termux_jkengine}" \
          "${PHONE_USER:-u0_a4}@$c" 'echo SELFRESOLVE-OK' 2>/dev/null)
    if printf '%s' "$out" | grep -aq SELFRESOLVE-OK; then
      PHONE_HOST="$c"; echo "SELFRESOLVE: MATCH (주소 미인쇄 계약)"; break
    fi
  done
fi
[ -n "$PHONE_HOST" ] || FAIL "폰 미접속 (정찰 — PHONE_HOST env)"
SSH="ssh -p ${PHONE_PORT:-8022} -o BatchMode=yes -o ConnectTimeout=15 -o ServerAliveInterval=15 -o ServerAliveCountMax=10 -i ${PHONE_KEY:-$HOME/.ssh/termux_jkengine} ${PHONE_USER:-u0_a4}@$PHONE_HOST"

$SSH 'command -v termux-wake-lock >/dev/null 2>&1 && termux-wake-lock || echo WARN-no-wake-lock' \
  || FAIL "ssh failed (폰 불도달)"

exec > >(tee "$RLOG") 2>&1
echo "PHONE-VOICE-START $(date +%F' '%H:%M:%S) btn=($PHV_BTN_X,$PHV_BTN_Y) runs=$PHV_TAP_RUNS"

# ── A. 사슬 probe — 로컬 산출→push→폰 직링크(T5 수형) ───────────────────────
cat > "$SCRATCH/t6pv_probe.cpp" <<'CPPEOF'
// T5 폰 capture+사슬 probe 원문의 형식화(#112 T5/T6 — CANON 밖 — selftest
// 녹임 0). 폰에서 컴파일(phone_voice.sh가 수행):
//   clang++ -std=c++20 -I include $TMPDIR/t5_voice_probe.cpp \
//       buildterm/libjkcore.a buildterm/libquickjs.a -lSDL2 -lSDL2_mixer \
//       -llog -lpthread -o $TMPDIR/t5_voice_probe
#include <voice/SttVoice.h>
#include <agent/JKLlmEngine.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>

int main(int argc, char** argv) {
    // argv: 1=마이크 wav 2=fixture wav 3=whisper-cli 경로 4=모델 경로
    // (probe exe 형제 부재 → env 오버라이드의 argv 직달판 — T2/T5 계약).
    const std::string micWav = argc > 1 ? argv[1] : "tmp/jk_t5_mic.wav";
    const std::string fixtureWav = argc > 2 ? argv[2] : "tmp/jk_t5_fixture.wav";
    if (argc > 3 && argv[3][0]) ::setenv("WHISPER_BIN", argv[3], 1);
    if (argc > 4 && argv[4][0]) ::setenv("STT_MODEL", argv[4], 1);
    const std::string model = jk::voice::run::ResolveModelPath();
    printf("model-resolved=%s\n", model.c_str());

    {
        std::string e1, e2, e3;
        jk::voice::turn::VoiceTurnResult r;
        const auto envOf = [](const char* k) {
            const char* v = ::getenv(k);
            return std::string(v ? v : "");
        };
        const std::string keepRoot = envOf("WHISPER_ROOT");
        ::unsetenv("WHISPER_ROOT");
        jk::voice::turn::RunVoiceTurn(fixtureWav, jk::agent::ChatConfig{},
                                      nullptr, r);
        e1 = r.err;
        if (!keepRoot.empty()) ::setenv("WHISPER_ROOT", keepRoot.c_str(), 1);
        else ::setenv("WHISPER_ROOT", "1", 1);
        const std::string keepBin = envOf("WHISPER_BIN");
        ::setenv("WHISPER_BIN", "/no/such/whisper-cli", 1);
        jk::voice::turn::RunVoiceTurn(fixtureWav, jk::agent::ChatConfig{},
                                      nullptr, r);
        e2 = r.err;
        if (!keepBin.empty()) ::setenv("WHISPER_BIN", keepBin.c_str(), 1);
        else ::unsetenv("WHISPER_BIN");
        const std::string keepModel = envOf("STT_MODEL");
        ::setenv("STT_MODEL", "/no/such/model.bin", 1);
        jk::voice::turn::RunVoiceTurn(fixtureWav, jk::agent::ChatConfig{},
                                      nullptr, r);
        e3 = r.err;
        if (!keepModel.empty()) ::setenv("STT_MODEL", keepModel.c_str(), 1);
        else ::unsetenv("STT_MODEL");
        printf("gate-env-unset=[%s]\n", e1.c_str());
        printf("gate-bin-missing=[%s]\n", e2.c_str());
        printf("gate-model-missing=[%s]\n", e3.c_str());
    }

    std::string stateErr;
    bool startOk = jk::voice::capture::StartCapture(micWav, stateErr);
    printf("mic-start=%d err=[%s]\n", static_cast<int>(startOk),
           stateErr.c_str());
    if (startOk) {
        const auto stMid = jk::voice::capture::GetCaptureState();
        std::printf("mic-mid capturing=%d wav=[%s]\n",
                    static_cast<int>(stMid.capturing), stMid.wavPath.c_str());
        std::this_thread::sleep_for(std::chrono::seconds(3));
        std::string stopErr;
        const bool stopOk = jk::voice::capture::StopCapture(stopErr);
        std::printf("mic-stop=%d err=[%s]\n", static_cast<int>(stopOk),
                    stopErr.c_str());
        if (stopOk) {
            std::FILE* f = std::fopen(micWav.c_str(), "rb");
            long bytes = -1;
            if (f) {
                std::fseek(f, 0, SEEK_END);
                bytes = std::ftell(f);
                std::fclose(f);
            }
            std::printf("mic-wav-bytes=%ld\n", bytes);
            jk::voice::turn::VoiceTurnResult r;
            int calls = 0;
            const jk::voice::turn::ToolDispatcher disp =
                [&](const std::string& tool, const std::string& args) {
                    ++calls;
                    std::printf("dispatch tool=%s args=%s\n", tool.c_str(),
                                args.c_str());
                    return true;
                };
            const bool ok =
                jk::voice::turn::RunVoiceTurn(micWav,
                                              jk::agent::ChatConfig{}, disp,
                                              r);
            std::printf("mic-turn ok=%d usedLlm=%d stt=[%s] guide-len=%d "
                        "err=[%s] calls=%d\n",
                        static_cast<int>(ok), static_cast<int>(r.usedLlm),
                        r.sttText.c_str(), static_cast<int>(r.guide.size()),
                        r.err.c_str(), calls);
        }
    }

    // ---- #115b T2 신설 어휘 세그먼트 (발화→라우터 즉발→질의 조립 원문) -----
    // WSL판(wsl_voice.sh §4)과 동형 — kRouteTable/kAliasTable(ChatRouter.cpp
    // 단일 진실원 — T1 어휘 표)을 ChatRouteTurn(구두 표면과 같은 진입 —
    // RunVoiceTurn (4) 접점 계약)으로 직발+ChatActionQuery 조립 원문 캡처.
    // cfg 미구성 = 라우터 즉발+usedLlm 0(2y-① 원존)·배제 어휘(D3)·무인자 강제
    // (D1)·기존 회귀("창 목록 보여줘" — 2y-⑩ 방어 행)가 같은 루프로 재실측.
    {
        const char* const kVocabSay[] = {
            "멈춰줘", "재생해줘", "정지", "알림 보여줘", "볼륨 50으로 멈춰줘",
            "다음 곡", "알림 뭐 왔어", "창 목록 보여줘",
        };
        for (const char* say : kVocabSay) {
            bool used = false;
            jk::ChatAction va;
            const std::string g = jk::ChatRouteTurn(say, va, jk::agent::ChatConfig{}, &used);
            std::string vtool, vargs;
            const bool vq = jk::voice::turn::ChatActionQuery(va, vtool, vargs);
            // printf 인자 평가 순서 함정(T3 probe fix r1 원존) — 조립을 먼저.
            const std::string toolCol = vq ? vtool : "no";
            const std::string argsCol = vq ? vargs : "no";
            printf("VOCAB text=[%s] kind=%d app=[%s] tool=[%s] usedLlm=%d "
                   "q=%d query=[%s %s] guide-len=%d\n",
                   say, static_cast<int>(va.kind), va.app.c_str(),
                   va.tool.c_str(), static_cast<int>(used),
                   static_cast<int>(vq), toolCol.c_str(), argsCol.c_str(),
                   static_cast<int>(g.size()));
            std::fflush(stdout);
        }
    }

    if (!model.empty()) {
        jk::voice::turn::VoiceTurnResult r;
        std::string directOut, directErr;
        const bool ok = jk::voice::turn::RunVoiceTurn(
            fixtureWav, jk::agent::ChatConfig{}, nullptr, r);
        std::printf("fixture-turn ok=%d usedLlm=%d stt=[%s] guide-len=%d "
                    "err=[%s]\n",
                    static_cast<int>(ok), static_cast<int>(r.usedLlm),
                    r.sttText.c_str(), static_cast<int>(r.guide.size()),
                    r.err.c_str());
        const bool direct = jk::voice::run::WhisperTranscribe(
            fixtureWav, model, directOut, directErr);
        std::printf("fixture-direct=%d out=[%s] err=[%s]\n",
                    static_cast<int>(direct), directOut.c_str(),
                    directErr.c_str());
    }
    std::printf("state.end capturing=%d lastErr=[%s]\n",
                static_cast<int>(jk::voice::capture::IsCapturing()),
                jk::voice::capture::GetCaptureState().lastError.c_str());
    return 0;
}
CPPEOF

# 합성 fixture(3s 사인 — T5 fixture의 레시피 가족) + mic 레그용 fixture 푸시
cat > "$SCRATCH/t6pv_fixture_gen.py" <<FIXEOF
# 합성 16 kHz/모노/s16le/3s 사인 — T5 fixture(3s 변조 사인 — 96044B)의 레시피
# 가족(의존 0 — wave stdlib). **바이트 등호 재현은 실패 원장행**(T5 세션의
# 생성기 원문 미보존 — local 대차 행이 md5 원문을 수취한다).
import hashlib, math, struct, sys, wave
rate, secs, freq = 16000, 3.0, 440.0
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
FIXEOF

echo "=== A. 폰 push+compile+probe 런 (T5 사다리 원존) ==="
$SSH "cat > \$TMPDIR/t5_voice_probe.cpp" < "$SCRATCH/t6pv_probe.cpp" || FAIL "probe cpp push failed"
$SSH "cat > \$TMPDIR/t6pv_fixture.py" < "$SCRATCH/t6pv_fixture_gen.py" || FAIL "fixture py push failed"
$SSH "python3 \$TMPDIR/t6pv_fixture.py \$TMPDIR/jk_t6pv_fixture.wav" | sed 's/^/FIXTURE: /' \
  || FAIL "phone fixture generation failed"
if [ -s "$SCRATCH/t5_fixture.wav" ]; then
    echo "FIXTURE-T5-STAGE: local md5=$(md5sum "$SCRATCH/t5_fixture.wav" | cut -c1-12) — T5 리포트 §3 원문 계보(96044B — 폰 push 등호 원문) — 본 런의 fixture = 레시피 가족 자가 생성"
fi

OK_LINK=0
$SSH "cd ~/JKENGINE/engine && rm -f \$TMPDIR/t5_voice_probe" 2>/dev/null
for LIBS in \
  "buildterm/libjkcore.a buildterm/libquickjs.a -lSDL2 -lSDL2_mixer -llog -lpthread" \
  "buildterm/libjkcore.a buildterm/libquickjs.a -lSDL2 -lSDL2_mixer -llog -lOpenSLES -lEGL -lGLESv2 -lstdc++ -lm -lpthread"; do
    echo "TRY-LIBS: $LIBS"
    tryrc=$($SSH "cd ~/JKENGINE/engine && clang++ -std=c++20 -I include \"\$TMPDIR/t5_voice_probe.cpp\" $LIBS -o \"\$TMPDIR/t5_voice_probe\" >/dev/null 2>\$TMPDIR/t6_link.err; echo RC=\$?" | tr -d ' \r' | tail -1)
    echo "LINK-RC=$tryrc ($(tail -2 "$SCRATCH/t6_link.err" 2>/dev/null | tr '\n' ' '))"
    [ "$tryrc" = "RC=0" ] && { OK_LINK=1; echo "LINK-OK-LIBS: $LIBS"; break; }
    $SSH "tail -3 \$TMPDIR/t6_link.err" 2>/dev/null | sed 's/^/LINK-ERR: /'
done
[ "$OK_LINK" -eq 1 ] || FAIL "probe link ladder exhausted (멤버 원문 = 위 TRY-LIBS/LINK-ERR — T5 원존 승계)"

$SSH 'cd ~/JKENGINE/engine && export WHISPER_ROOT="$HOME/JKENGINE/engine/third_party/whisper.cpp" && unset STT_MODEL WHISPER_BIN && timeout 300 "$TMPDIR/t5_voice_probe" "$TMPDIR/t5_mic.wav" "$TMPDIR/jk_t6pv_fixture.wav" "$HOME/JKENGINE/engine/buildterm/whisper-build/whisper-cli" "$HOME/JKENGINE/engine/buildterm/stt-models/ggml-tiny-ko.bin" 2>&1; echo PROBE-RC=$?' \
  | sed 's/^/PROBE: /' | tee "$SCRATCH/phone_voice_probe.out"
grep -aq '^PROBE: PROBE-RC=0$' "$SCRATCH/phone_voice_probe.out" \
  || echo "VOICE-PHONE-FAIL: probe leg rc non0(전사 빈 결실 = 정직 실패 계약 — fail-closed 3판+mic 행이 본 실측의 주 산치 — T5 원장 승계)"
grep -aqF 'gate-env-unset=[구두 끔 (WHISPER_ROOT 미설정)]' "$SCRATCH/phone_voice_probe.out" \
  || echo "VOICE-PHONE-FAIL: gate-env-unset 원문 미일치"
grep -aqF 'gate-bin-missing=[구두 끔 (whisper-cli 부재)]' "$SCRATCH/phone_voice_probe.out" \
  || echo "VOICE-PHONE-FAIL: gate-bin-missing 원문 미일치"
# [원장행 — probe 계보 갱신 #115b T2 재런 실측]: 판 3 기대 원문 = T6 시점
# '구두 rc=3 stderr: ...'(스폰 leg). 이후 resolve 계약(park-5 쌍둥이 정합 수형
# — env 오버라이드 경로 부재 = 빈 반환 — SttVoice.cpp ResolveModelPath 주석
# 원문)으로 스폰 전 fail-closed가 됐다 — 현재 계약 원문(결정론) 우선+T6
# 스폰 leg 원문 호환 양쪽 수용(WSL판 VOCFG-3 동형).
grep -aq 'gate-model-missing=\[구두 끔 (whisper 모델 부재)\]' "$SCRATCH/phone_voice_probe.out" \
  || grep -aq 'gate-model-missing=\[구두 rc=3 stderr: ' "$SCRATCH/phone_voice_probe.out" \
  || echo "VOICE-PHONE-FAIL: gate-model-missing 원문 미일치(3판 = 현재 계약 원문[구두 끔 (whisper 모델 부재)] 또는 T6 계보 원문[rc=3 stderr] 어느 쪽도 아니었음)"
MIC_START=$(grep -a 'mic-start=' "$SCRATCH/phone_voice_probe.out" | tail -1)
MIC_STOP=$(grep -a 'mic-stop=' "$SCRATCH/phone_voice_probe.out" | tail -1)
echo "PROBE-MIC: $MIC_START | $MIC_STOP (Termux:API 폰 실기기 마이크 leg — 권한 = R3 사용자 승인 계승)"
FIX_TURN=$(grep -a 'fixture-turn ok=' "$SCRATCH/phone_voice_probe.out" | tail -1)
FIX_DRECT=$(grep -a 'fixture-direct=' "$SCRATCH/phone_voice_probe.out" | tail -1)
echo "PROBE-FIXTURE: $FIX_TURN"
echo "PROBE-FIXTURE-DIRECT: $FIX_DRECT"
if printf '%s' "$FIX_DRECT" | grep -aq 'fixture-direct=1'; then
    echo "VOICE-PHONE-FIXTURE-OK: fixture 사슬+직접 스폰 양성 등호(1눈 — RunVoiceTurn의 전사 = whisper-cli 스폰과 동일 — T5 §3 원문 계약)"
else
    echo "VOICE-PHONE-FAIL(fixture-direct: 직접 스폰 대조 계약 미성립 — $FIX_DRECT)"
fi

# ── #115b T2 신설 어휘 질의 양식 등호 (#115b T1 어휘 표 재실측 — probe VOCAB
#    행 — WSL판과 동형 판정. 라우터 = buildterm/libjkcore.a 방금 배포 진실원 —
#    배포 원문 승계의 검증을 겸한다) ──
# [fix r1: 수파일은 sed PROBE 접두를 붙여 저장한다 — grep 전 접두 벗기기(WSL raw와
#  정합 — 115b-1phone 런 원장행: route=0/5 오진. 행 원문 자체는 정상이었다)]
VROWS=$(sed 's/^PROBE: //' "$SCRATCH/phone_voice_probe.out" | grep -a '^VOCAB ')
printf '%s' "$VROWS" | sed 's/^/PROBE-VOCAB: /'
VROUTE=0
for S in 멈춰줘 재생해줘 정지; do
    printf '%s' "$VROWS" | grep -aqF "text=[$S] kind=4 app=[vplayer] tool=[play_pause] usedLlm=0 q=1" \
        && VROUTE=$((VROUTE+1)) || echo "VOICE-PHONE-FAIL(vocab-route: $S — AppTool 즉발·무인자 질의 양식 원문 불일치)"
done
printf '%s' "$VROWS" | grep -aqF 'text=[알림 보여줘] kind=0 app=[notify] tool=[] usedLlm=0 q=1 query=[launch_app {"app":"notify"}]' \
    && VROUTE=$((VROUTE+1)) || echo "VOICE-PHONE-FAIL(vocab-route: 알림 보여줘 — Launch(notify) 원문 불일치)"
# 무인자 강제(D1) — 자체 판정행(fix: notify 행 grep tool=[] 누락+exact-invoke 산치
# 이중 계수의 상쇄 쌍 소각 — VROUTE 산치 = 즉발 5(q=1)+무인자 강제 1(q=0) = 6 —
# kind=5 만의 느슨 패턴은 배제 stub(동 kind) 충돌 원문 — q=0 원문까지 등호 — WSL판 동형)
VEXACT=0
printf '%s' "$VROWS" | grep -aqF 'text=[볼륨 50으로 멈춰줘] kind=5 app=[] tool=[] usedLlm=0 q=0 query=[no no]' \
    && VEXACT=1 || echo "VOICE-PHONE-FAIL(vocab-exact: 볼륨 50으로 멈춰줘 — 무인자 강제(D1) 원문 불일치)"
BEX=0
for S in 'text=[다음 곡]' 'text=[알림 뭐 왔어]'; do
    printf '%s' "$VROWS" | grep -aqF "$S" \
      && printf '%s' "$VROWS" | grep -aqF "$S kind=5" \
      && BEX=$((BEX+1)) || echo "VOICE-PHONE-FAIL(vocab-excl: $S — D3 배제·stub 원문 불일치)"
done
printf '%s' "$VROWS" | grep -aqF 'text=[창 목록 보여줘] kind=3' \
    && VROUTE=$((VROUTE+1)) || echo "VOICE-PHONE-FAIL(vocab-regress: 창 목록 보여줘 — 2y-⑩ 방어 행 원문 불일치)"
echo "PROBE-VOCAB-ROUTE-OK: rows=$((VROUTE+VEXACT))/6 (route 산치 분리 — route=$VROUTE/5+exact=$VEXACT/1) excl=$BEX/2"

# ── B. 서버 부팅(WHISPER_ROOT 보유)+chat 개봉+탭 런 반복(폰 E2E — T5 §6) ────
echo "=== B. WHISPER_ROOT 보유 부팅 + chat 개봉 + [구두] 탭 런 x$PHV_TAP_RUNS ==="
cat > "$SCRATCH/t6pv_boot_remote.sh" <<'BOOTEOF'
#!/bin/bash
# (phone_voice.sh가 생성 — 폰 주소 불포함) 서버 WHISPER_ROOT 보유 부팅+chat
# 개봉+ready 프레임 캡처. 종료 후 자기 소각(t112_t5 boot 수형).
set -u
cd ~/JKENGINE/engine || exit 1
TMPD="${TMPDIR:-/data/data/com.termux/files/usr/tmp}"
pkill -f '[j]kdesktop' 2>/dev/null; sleep 2; pkill -9 -f '[j]kdesktop' 2>/dev/null; sleep 1
if pgrep -f '[j]kdesktop' >/dev/null 2>&1; then echo "PV-FAIL: server survived pre-clean"; exit 1; fi
echo "SRV-DOWN-OK (브래킷 — jkweb 불접촉)"
command -v termux-wake-lock >/dev/null 2>&1 && termux-wake-lock || true
env DISPLAY=:1 WHISPER_ROOT="$HOME/JKENGINE/engine/third_party/whisper.cpp" \
    LD_LIBRARY_PATH="$PREFIX/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
    setsid nohup ./buildterm/jkdesktop --server > "$TMPD/t6pv_srv.log" 2>&1 &
sleep 8
SRVPID=$(pgrep -f 'buildterm/[j]kdesktop --server' | head -1)
[ -n "$SRVPID" ] || { tail -5 "$TMPD/t6pv_srv.log"; exit 1; }
echo "SRV-BOOT-OK pid=$SRVPID"
P=""
for i in $(seq 1 30); do
  P=$(timeout 15 ./buildterm/jkdesktop agentctl '{"tool":"ping","args":{}}' 2>/dev/null | grep -a '{' | head -1)
  printf '%s' "$P" | grep -aq '"ok":true' && break
  sleep 2
done
printf '%s' "$P" | grep -aq '"ok":true' || { echo "PV-FAIL: ping no-ok"; exit 1; }
echo "PING-OK"
L=$(timeout 20 ./buildterm/jkdesktop agentctl '{"tool":"launch_app","args":{"app":"chat"}}' 2>/dev/null | grep -a '{' | head -1)
echo "CHAT-LAUNCH: $L"
printf '%s' "$L" | grep -aq '"ok":true' || { echo "PV-FAIL: launch_app chat failed — $L"; exit 1; }
CH=""
WIN=""
for i in $(seq 1 15); do
  WIN=$(timeout 15 ./buildterm/jkdesktop agentctl '{"tool":"list_windows","args":{}}' 2>/dev/null | grep -a '{' | head -1)
  CH=$(printf '%s' "$WIN" | grep -aoE '\{"id":[^}]*"title":"[Cc]hat"[^}]*\}' | head -1)
  [ -n "$CH" ] && break
  sleep 2
done
[ -n "$CH" ] || { echo "PV-FAIL: no Chat window — $WIN"; exit 1; }
echo "CHAT-WINDOW: $CH"
sleep 4
rm -f "$TMPD/t5_chat_ready.png"
R=$(timeout 25 ./buildterm/jkdesktop agentctl '{"tool":"capture_region","args":{"x":0,"y":0,"w":1920,"h":1080}}' 2>/dev/null | grep -a '"ok"' | head -1)
P2=$(printf '%s' "$R" | grep -ao '"path":"[^"]*"' | sed 's/"path":"//;s/"$//')
[ -n "$P2" ] && cp "$P2" "$TMPD/t5_chat_ready.png"
[ -s "$TMPD/t5_chat_ready.png" ] || { echo "PV-FAIL: capture failed (R=${R:0:160})"; exit 1; }
echo "CAPTURE-OK: t5_chat_ready.png ($(wc -c < "$TMPD/t5_chat_ready.png") bytes)"
rm -f -- "$0"
echo "BOOT-FINISHED"
BOOTEOF
$SSH "cat > \$TMPDIR/t6pv_boot.sh" < "$SCRATCH/t6pv_boot_remote.sh" || FAIL "boot script push failed"
BOUT=$($SSH "bash \$TMPDIR/t6pv_boot.sh 2>&1" )
sed 's/^/BOOT: /' <<< "$BOUT"
grep -aq 'BOOT-FINISHED' <<< "$BOUT" || FAIL "boot script did not finish"
if grep -aq 'PV-FAIL' <<< "$BOUT"; then
  FAIL "boot hard: $(grep -a 'PV-FAIL' <<< "$BOUT" | head -1)"
fi
CHLINE=$(grep -a '^CHAT-WINDOW: ' <<< "$BOUT" | tail -1 | sed 's/^CHAT-WINDOW: //' | tr -d '\r')
echo "CHAT-GEOM: $CHLINE"
[ -n "$CHLINE" ] || FAIL "chat window line missing"
# [원장행 소화 — 탭 좌표 재보정(갱신 65 잔여 원장행 — 115b-1phone 런 실측)]:
# T5 실측 [구두] = 데스크 (835,602) = chat(172,114) 기준 — **서면 계수 (663,488)**
# 가 진실원이고 데스크 좌표는 창마다 다른 cascade 위치를 따라 이동한다(본 런
# 실측: chat (620,232)에서 (835,602) 탭 = 미달 — rec/done 0 전이 + isRecording
# false). env 미지정 런 = 창 위치에서 재산출한다. env 지정 = 수동 우선 원존
# (SET 판별 = 상단 원존 — fix r2).
PHV_CHX=$(printf '%s' "$CHLINE" | sed -n 's/.*"x":\(-\?[0-9]*\),.*/\1/p' | tr -d '\r')
PHV_CHY=$(printf '%s' "$CHLINE" | sed -n 's/.*"y":\(-\?[0-9]*\),.*/\1/p' | tr -d '\r')
if [ -z "$PHV_BTN_X_SET" ] && [ -z "$PHV_BTN_Y_SET" ] \
   && [ -n "$PHV_CHX" ] && [ -n "$PHV_CHY" ] \
   && [ "$PHV_CHX" -eq "$PHV_CHX" ] 2>/dev/null && [ "$PHV_CHY" -eq "$PHV_CHY" ] 2>/dev/null; then
    PHV_BTN_X=$((PHV_CHX + 663))
    PHV_BTN_Y=$((PHV_CHY + 488))
    echo "PHV-BTN-AUTO: ($PHV_BTN_X,$PHV_BTN_Y) = chat($PHV_CHX,$PHV_CHY)+서면 계수 (663,488) — 탭 좌표 재보정 수형(115b-1phone 런 원장)"
elif [ -z "$PHV_BTN_X_SET" ] && [ -z "$PHV_BTN_Y_SET" ]; then
    echo "PHV-BTN-AUTO-MISS: chat 좌표 파싱 실패 — T5 기정 좌표 (835,602) 유지(원장행)"
fi

# tap remote 원격 스크립트 = T5 tap 수형(permissions 병합+tap 2회+캡처 3종+
# 원복) — 런별 캡처 회수는 호출자(per-run 복사)가 소유.
cat > "$SCRATCH/t6pv_tap_remote.sh" <<'TAPEOF'
#!/bin/bash
# (phone_voice.sh가 생성 — 폰 주소 불포함) chat [구두] 탭 절차: 진입 원문 →
# permissions 병합 → tap(rec) → tap(마침) → 캡처 3종 → 바이트 원복 → 잔상
# 소각. 종료 후 자기 소각(t112_t5 tap_remote 수형).
set -u
cd ~/JKENGINE/engine || exit 1
TX="${1:-835}" TY="${2:-602}" GAP="${3:-4}"
TMPD="${TMPDIR:-/data/data/com.termux/files/usr/tmp}"
FAIL() { echo "TAP-FAIL: $*"; exit 1; }
ctl() { timeout 20 ./buildterm/jkdesktop agentctl "$1" 2>/dev/null | grep -a '{' | head -1; }
ok()  { printf '%s' "${1:-}" | grep -aq '"ok":true'; }

WIN=$(ctl '{"tool":"list_windows","args":{}}')
CH=$(printf '%s' "$WIN" | grep -aoE '\{"id":[^}]*"title":"[Cc]hat"[^}]*\}' | head -1)
[ -n "$CH" ] || FAIL "no Chat window — $WIN"
echo "CHAT-WINDOW-BEFORE: $CH"
CHID=$(printf '%s' "$CH" | sed -n 's/.*"id":\([0-9]*\).*/\1/p')
[ -n "$CHID" ] || FAIL "chat id parse failed — $CH"

PERM=buildterm/permissions.json
cp "$PERM" "$TMPD/t6pv_perm_backup" || FAIL "perm backup failed"
echo "PERM-BACKUP-OK ($(wc -c < "$PERM") bytes)"
awk -v newline="    \"send_input\": \"allow\"" '
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
    }' "$PERM" > "$PERM.new" && mv "$PERM.new" "$PERM" || FAIL "perm merge failed"
grep -aq 'send_input' "$PERM" || FAIL "send_input not merged"
echo "PERM-MERGED: $(tr -d "\n" < "$PERM") (핫리드 — 호출마다 읽음 — 탭 뒤 바이트 원복)"

TAP() {
    R=$(ctl "{\"tool\":\"send_input\",\"args\":{\"op\":\"click\",\"id\":$CHID,\"x\":$1,\"y\":$2,\"button\":1,\"clicks\":1}}")
    echo "SEND-INPUT($1,$2): ${R:-none}"
    ok "$R" || FAIL "send_input tap no ok — $R"
}

TAP "$TX" "$TY"          # [구두] start — 클라 StartCapture(Termux:API 스폰)
sleep 3
capture() {
    rm -f "$TMPD/$1"
    R=$(timeout 25 ./buildterm/jkdesktop agentctl '{"tool":"capture_region","args":{"x":0,"y":0,"w":1920,"h":1080}}' 2>/dev/null | grep -a '"ok"' | head -1)
    P=$(printf '%s' "$R" | grep -ao '"path":"[^"]*"' | sed 's/"path":"//;s/"$//')
    [ -n "$P" ] && cp "$P" "$TMPD/$1"
    [ -s "$TMPD/$1" ] || FAIL "capture failed: $1 (R=${R:0:120})"
    echo "CAP-OK: $1 ($(wc -c < "$TMPD/$1") bytes)"
}
capture t5_chat_rec.png
sleep "$GAP"
TAP "$TX" "$TY"          # 같은 좌표 = 라벨 마침(토글 계약 R2) — 정지+전사
echo "WAIT-TRANSCRIBE: 25s (tiny-ko 폰 전사 원가 대기)"
sleep 25
capture t5_chat_done.png
sleep 2
capture t5_chat_done2.png
I=$(command -v termux-microphone-record >/dev/null 2>&1 && termux-microphone-record -i 2>/dev/null | head -2 | tr '\n' ' ')
echo "TERMAPI-AFTER: $I (세션 판정 진실원 — 함정 2/9 수형)"
W=$(ctl '{"tool":"list_windows","args":{}}')
echo "FINAL-WINDOWS: $W"
cmp -s "$PERM" "$TMPD/t6pv_perm_backup" || cp "$TMPD/t6pv_perm_backup" "$PERM"
cmp -s "$PERM" "$TMPD/t6pv_perm_backup" || FAIL "perm restore mismatch"
echo "PERM-RESTORED-OK ($(wc -c < "$PERM") bytes 등호 — 원복 계약)"
rm -f "$TMPD/t6pv_perm_backup"
rm -f -- "$0"
echo "TAP-FINISHED"
exit 0
TAPEOF
recover() { # $1 폰png $2 로컬png — base64 1파이프+md5 대차(T5 수형 — cygpath -w
            # 폴백 = 함정 6의 수형 전문)
    local REMOTE_PNG=$1 LOCAL_PNG=$2 RMD5 LMD5 WB64 WPNG
    RMD5=$($SSH "md5sum $REMOTE_PNG" 2>/dev/null | sed -n 's/^\([0-9a-f]*\) .*$/\1/p' | tr -d ' \r')
    $SSH "base64 -w0 $REMOTE_PNG" > "$SCRATCH/t6pv_b64.txt" \
        || { echo "RECOVER-MISS: $REMOTE_PNG"; return 0; }
    WB64=$(cygpath -w "$SCRATCH/t6pv_b64.txt" 2>/dev/null || echo "$SCRATCH/t6pv_b64.txt")
    WPNG=$(cygpath -w "$LOCAL_PNG" 2>/dev/null || echo "$LOCAL_PNG")
    PYTHONIOENCODING=utf-8 python - "$WB64" "$WPNG" <<'PYRB64' \
      || { echo "RECOVER-DECODE-FAIL: $LOCAL_PNG"; return 0; }
import base64, re, sys
d = open(sys.argv[1], "rb").read().decode("ascii", "ignore")
d = re.sub(r"\s+", "", d)
open(sys.argv[2], "wb").write(base64.b64decode(d))
PYRB64
    [ -s "$LOCAL_PNG" ] || { echo "RECOVER-EMPTY: $LOCAL_PNG"; return 0; }
    LMD5=$(md5sum "$LOCAL_PNG" | awk '{print $1}')
    if [ -n "$RMD5" ] && [ "$RMD5" = "$LMD5" ]; then
        echo "RECOVER-MD5-OK: $(basename "$LOCAL_PNG") ($(wc -c < "$LOCAL_PNG" | tr -d ' ') bytes)"
    else
        FAIL "recover md5 mismatch: $LOCAL_PNG"
    fi
    rm -f "$SCRATCH/t6pv_b64.txt"
}

ready_remote="\$TMPDIR/t5_chat_ready.png"
recover "$ready_remote" "$SCRATCH/phone_voice_ready.png" \
    || FAIL "ready frame recovery failed"
cp "$SCRATCH/phone_voice_ready.png" "$SCRATCH/pv_ready.png" 2>/dev/null || true

RUN_VERB="OK_ALL"
for RNO in $(seq 1 "$PHV_TAP_RUNS"); do
    echo "--- 탭 런 $RNO (재측정) ---"
    # 탭 원격 스크립트 = 자기 소각 계약(rm -f -- "$0" — T5 원존)이라 매 런 push.
    $SSH "cat > \$TMPDIR/t6pv_tap.sh" < "$SCRATCH/t6pv_tap_remote.sh" \
        || FAIL "tap script push failed (런 $RNO)"
    UOUT=$($SSH "bash \$TMPDIR/t6pv_tap.sh $PHV_BTN_X $PHV_BTN_Y 4" 2>&1)
    URC=$?
    printf '%s\n' "$UOUT" | sed "s/^/TAP[$RNO]: /"
    if [ "$URC" -eq 255 ]; then
        echo "RUN-DROPPED: ssh 채널 단절 — \$TMPDIR 영수증 생존 재실행=증분(T5 원장)"
        UOUT=$($SSH "bash \$TMPDIR/t6pv_tap.sh $PHV_BTN_X $PHV_BTN_Y 4" 2>&1)
        printf '%s\n' "$UOUT" | sed "s/^/TAP[$RNO-2nd]: /"
    elif [ "$URC" -ne 0 ]; then
        echo "VOICE-PHONE-FAIL: 탭 절차 rc=$URC (원문 = 위 TAP[$RNO] 행)"
    fi
    grep -aq 'TAP-FINISHED' <<< "$UOUT" \
        || { echo "VOICE-PHONE-FAIL: 탭 런 $RNO 미완주 — 풀런 종료(honest)"; RUN_VERB="MISS_RUN"; break; }
    if grep -aq 'TAP-FAIL' <<< "$UOUT"; then
        echo "VOICE-PHONE-FAIL(탭 런 $RNO): $(grep -a 'TAP-FAIL' <<< "$UOUT" | head -1)"
        RUN_VERB="FAIL_RUN"
        break
    fi
    # 캡처 per-run 회수+기록 도착 proxy(done 바이트 > rec 바이트 = 기록 행 렌더)
    SREC=$($SSH "wc -c < \$TMPDIR/t5_chat_rec.png" 2>/dev/null | tr -d ' \r')
    SDONE=$($SSH "wc -c < \$TMPDIR/t5_chat_done.png" 2>/dev/null | tr -d ' \r')
    echo "VOICE-PHONE-RUN-VALS: run=${RNO} rec=${SREC} done=${SDONE} btn=($PHV_BTN_X,$PHV_BTN_Y)"
    recover "\$TMPDIR/t5_chat_rec.png"   "$SCRATCH/pv_run${RNO}_rec.png"
    recover "\$TMPDIR/t5_chat_done.png"  "$SCRATCH/pv_run${RNO}_done.png"
    recover "\$TMPDIR/t5_chat_done2.png" "$SCRATCH/pv_run${RNO}_done2.png"
    cp -f "$SCRATCH/pv_run${RNO}_rec.png"   "$SCRATCH/pv_latest_rec.png"
    cp -f "$SCRATCH/pv_run${RNO}_done.png"  "$SCRATCH/pv_latest_done.png"
    if [ -n "$SREC" ] && [ -n "$SDONE" ] && [ "$SDONE" -gt "$SREC" ]; then
        DL=$((SDONE - SREC))
        echo "VOICE-PHONE-TURN-OK: 런 $RNO — done>rec 바이트 증가(+${DL}B = 기록 행 렌더 영수증 — 표기 원문은 캡처 육안/Read 몫; 런간 전이 귀속 = T5 함정 9/10 원장행 참조)"
    else
        echo "VOICE-PHONE-FAIL(turn$RNO: done($SDONE) <= rec($SREC) — 기록 전이 미검출 — T5 함정 10 수형 원장)"
        RUN_VERB="MISS_TURN"
    fi
done

# ── B.2 #115b 신설 어휘 라이브 소비 세그먼트 (WSL판 §13b 동형 — T1 리포트
#    §2 릴레이 양식 소비 계약: app_tool 릴레이→vplayer open/play_pause 토글 2탭
#    +get_status 기록 반영·'알림 보여줘' 질의(launch_app notify)→Notifications
#    창 Launch 원문. 폰 캡처 = capture_region 도구(폰 X 소켓 계통 ffmpeg
#    ENOENT 원장 승계)·close_window 병합+바이트 원복(함정 7 수형) ──
echo "=== B2. 신설 어휘 라이브 소비 — vplayer 토글+notify Launch (1런) ==="
cat > "$SCRATCH/t6pv_vocab_remote.sh" <<'VOCEOF'
#!/bin/bash
# (phone_voice.sh가 생성 — 폰 주소 불포함) 신설 어휘 라이브 소비: vplayer 창
# (릴레이 대상)→app_tool open(합성 wav — pmus M 세그먼트 원존 계약: 폰 vplayer
# 가 pcm wav를 연다)→play_pause 토글 2탭+get_status 대차→launch_app notify→
# 창 원문→close_window(vplayer+notify)→permissions 바이트 원복→잔상 소각.
# 종료 후 자기 소각(t6pv 원존).
set -u
cd ~/JKENGINE/engine || exit 1
TMPD="${TMPDIR:-/data/data/com.termux/files/usr/tmp}"
ctl() { timeout 25 ./buildterm/jkdesktop agentctl "$1" 2>/dev/null | grep -a '{' | head -1; }
ok()  { printf '%s' "${1:-}" | grep -aq '"ok":true'; }
# [fix r1 — 115b-1phone 런 원장행]: FAIL 출구가 permissions 원복+leftover 창
# 소각을 건너뛰었다(본 런 실측: permissions.json 33→62B 중복 close_window 잔상
# +Video Player/Notifications 창 잔존 — 수기 원복 소각) — FAIL도 완수 계약
# (wsl_music HARV-TEARDOWN 래더 원존 승계). cleanup() = 무해 재입(멱등).
VPERMP="$TMPD/t6pv_vocab_perm_backup"
vcleanup() {
    if [ -s "$VPERMP" ]; then
        cp "$VPERMP" buildterm/permissions.json 2>/dev/null
        cmp -s buildterm/permissions.json "$VPERMP" \
            && echo "VOCAB-PERM-RESTORED ($(wc -c < buildterm/permissions.json) bytes — FAIL 출구 포함) — backups 원본" \
            || echo "VOCAB-FAIL(perm-fin: 원복 실패 — 수기 소각 필요)"
        rm -f "$VPERMP"
    fi
    WL=$(ctl '{"tool":"list_windows","args":{}}' 2>/dev/null)
    for T in "Video Player" "Notifications"; do
        WID=$(printf '%s' "$WL" | grep -aoE "\{\"id\":[0-9]+,\"title\":\"${T}" | grep -aoE '[0-9]+' | head -1)
        if [ -n "$WID" ]; then
            CR=$(ctl "{\"tool\":\"close_window\",\"args\":{\"id\":$WID}}" 2>/dev/null)
            echo "VOCAB-LEFTOVER-CLOSED ($T id=$WID): ${CR:-none}"
        fi
    done
}
FAIL() { echo "VOCAB-FAIL: $*"; vcleanup; exit 1; }

PERM=buildterm/permissions.json
cp "$PERM" "$VPERMP" || FAIL "perm backup failed"
echo "VOCAB-PERM-BACKUP-OK ($(wc -c < "$PERM") bytes)"
if grep -aq 'close_window' "$PERM"; then
    echo "VOCAB-PERM-KEEP (close_window 이미 allow — 병합 생략 = **무변경 0** — 중복 행 방지 [fix r1])"
else
    awk -v newline="    \"close_window\": \"allow\"" '
        BEGIN { n = 0 }
        { lines[n++] = $0 }
        END {
            last = -1
            for (i = n - 1; i >= 0; i--)
                if (lines[i] ~ /^[ \t]*\}[ \t\r]*$/) { last = i; break }
            if (last < 1) { print "VOCAB-PERM-MERGE-FAIL"; exit 1 }
            prev = lines[last - 1]
            sub(/[ \t\r]*$/, "", prev)
            if (prev !~ /,$/) prev = prev ","
            lines[last - 1] = prev
            for (i = 0; i < n; i++) {
                if (i == last) print newline
                print lines[i]
            }
        }' "$PERM" > "$PERM.new" && mv "$PERM.new" "$PERM" || FAIL "close_window merge failed"
    grep -aq 'close_window' "$PERM" || FAIL "close_window not merged"
    echo "VOCAB-PERM-MERGED (핫리드 — close_window allow — END 바이트 원복)"
fi

VW=$(ctl '{"tool":"list_windows","args":{}}')
VP0=$(printf '%s' "$VW" | grep -aoE '\{"id":[^}]*"title":"Video Player"[^}]*\}' | head -1)
if [ -n "$VP0" ]; then
    VP0ID=$(printf '%s' "$VP0" | sed -n 's/.*"id":\([0-9]*\),.*/\1/p')
    ctl "{\"tool\":\"close_window\",\"args\":{\"id\":$VP0ID}}" >/dev/null 2>&1
    sleep 3
    echo "VOCAB-STALE-VPLAYER-CLOSED (선행 창 소각 — 이중 인스턴스 러링 방지)"
fi
VL=$(ctl '{"tool":"launch_app","args":{"app":"vplayer"}}')
echo "VOCAB-VPLAYER-LAUNCH: ${VL:-none}"
ok "$VL" || FAIL "launch_app vplayer failed — $VL"
VP=""
for i in $(seq 1 20); do
    sleep 3
    VW=$(ctl '{"tool":"list_windows","args":{}}')
    VP=$(printf '%s' "$VW" | grep -aoE '\{"id":[^}]*"title":"Video Player"[^}]*\}' | head -1)
    [ -n "$VP" ] && break
done
[ -n "$VP" ] || FAIL "no Video Player window (폰 스폰 15-18s 원장+마진) — last: ${VW:-none}"
echo "VOCAB-VPLAYER-WIN: $VP"
VPID=$(printf '%s' "$VP" | sed -n 's/.*"id":\([0-9]*\),.*/\1/p')
OPEN=$(ctl "{\"tool\":\"app_tool\",\"args\":{\"app\":\"vplayer\",\"tool\":\"open\",\"args\":{\"path\":\"$TMPD/jk_t6pv_fixture.wav\"}}}")
echo "VOCAB-VOPEN: ${OPEN:-none} (accepted 원문 — 합성 wav = §A fixture 재용 — pmus M 원존 계약 승계: 폰 vplayer가 pcm wav를 연다)"
# [원장: app_tool 중계 응답은 봉투 랩 — {"ok":true,"windowId":N,
#  "result":{...}} — T5 봉투/본문 이층 원존 수형 — 본문 부분열 매치]
printf '%s' "$OPEN" | grep -aqF '"accepted":true' || FAIL "vplayer open not accepted — $OPEN"
OPENED=0
STS=""
for i in $(seq 1 16); do
    sleep 2
    STS=$(ctl '{"tool":"app_tool","args":{"app":"vplayer","tool":"get_status","args":{}}}')
    printf '%s' "$STS" | grep -aq '"opened":true' && { OPENED=1; break; }
done
echo "VOCAB-VSTATUS-OPENED: $OPENED — ${STS:-none}"
[ "$OPENED" -eq 1 ] || FAIL "get_status opened 원문 미도달 — 폰 open leg 원장"
S0=$(ctl '{"tool":"app_tool","args":{"app":"vplayer","tool":"get_status","args":{}}}')
echo "VOCAB-TG0: $S0 (get_status — 토글 선행 상태 원문·기록 반영 대차)"
T1R=$(ctl '{"tool":"app_tool","args":{"app":"vplayer","tool":"play_pause","args":{}}}')
sleep 1
S1=$(ctl '{"tool":"app_tool","args":{"app":"vplayer","tool":"get_status","args":{}}}')
echo "VOCAB-TOGGLE1: reply=$T1R status=$S1 ('재생해줘' 질의 소비 1탭 — 중계 응답+기록 반영)"
T2R=$(ctl '{"tool":"app_tool","args":{"app":"vplayer","tool":"play_pause","args":{}}}')
sleep 1
S2=$(ctl '{"tool":"app_tool","args":{"app":"vplayer","tool":"get_status","args":{}}}')
echo "VOCAB-TOGGLE2: reply=$T2R status=$S2 (2탭 = 토글 복귀)"
TOG=0
if printf '%s' "$T1R" | grep -aqF '"paused":true' && printf '%s' "$S1" | grep -aqF '"paused":true' \
   && printf '%s' "$T2R" | grep -aq '"paused":false' && printf '%s' "$S2" | grep -aq '"paused":false'; then
    TOG=1
    echo "VOCAB-TOGGLE-OK: 토글 2탭 상반+기록 반영 등호"
else
    echo "VOCAB-FAIL(toggle: 중계 응답/기록 반영 원문 불일치 — T1R=$T1R T2R=$T2R)"
fi
for f in t5_chat_vocab_vp.png; do
    rm -f "$TMPD/$f"
    R=$(timeout 25 ./buildterm/jkdesktop agentctl '{"tool":"capture_region","args":{"x":0,"y":0,"w":1920,"h":1080}}' 2>/dev/null | grep -a '"ok"' | head -1)
    P=$(printf '%s' "$R" | grep -ao '"path":"[^"]*"' | sed 's/"path":"//;s/"$//')
    [ -n "$P" ] && cp "$P" "$TMPD/$f"
    [ -s "$TMPD/$f" ] || FAIL "vocab capture failed: $f (R=${R:0:120})"
    echo "VOCAB-CAPTURE-OK: $f ($(wc -c < "$TMPD/$f") bytes)"
done
NL=$(ctl '{"tool":"launch_app","args":{"app":"notify"}}')
echo "VOCAB-NOTIFY-LAUNCH: ${NL:-none} ('알림 보여줘' 질의 소비 — Launch 원문)"
ok "$NL" || FAIL "launch_app notify failed — $NL"
NT=""
for i in $(seq 1 20); do
    sleep 3
    VW=$(ctl '{"tool":"list_windows","args":{}}')
    # [fix r1 — notify 창 제목은 배지를 실는다 "Notifications (1)"(누적 —
    #  115b-1phone 런 실측) — 접두 매치(WSL판 동형 수리)]
    NT=$(printf '%s' "$VW" | grep -aoE '\{"id":[^}]*"title":"Notifications[^}]*\}' | head -1)
    [ -n "$NT" ] && break
done
[ -n "$NT" ] || FAIL "no Notifications window — last: ${VW:-none}"
echo "VOCAB-NOTIFY-WIN: $NT (알림 별명 2행+kAliasTable 1행 소비)"
for f in t5_chat_vocab_notify.png; do
    rm -f "$TMPD/$f"
    R=$(timeout 25 ./buildterm/jkdesktop agentctl '{"tool":"capture_region","args":{"x":0,"y":0,"w":1920,"h":1080}}' 2>/dev/null | grep -a '"ok"' | head -1)
    P=$(printf '%s' "$R" | grep -ao '"path":"[^"]*"' | sed 's/"path":"//;s/"$//')
    [ -n "$P" ] && cp "$P" "$TMPD/$f"
    [ -s "$TMPD/$f" ] || FAIL "vocab capture failed: $f (R=${R:0:120})"
    echo "VOCAB-CAPTURE-OK: $f ($(wc -c < "$TMPD/$f") bytes)"
done
VPID2=$(printf '%s' "$VP" | sed -n 's/.*"id":\([0-9]*\),.*/\1/p')
NTID=$(printf '%s' "$NT" | sed -n 's/.*"id":\([0-9]*\),.*/\1/p')
CR1=$(ctl "{\"tool\":\"close_window\",\"args\":{\"id\":$NTID}}")
echo "VOCAB-NOTIFY-CLOSE: ${CR1:-none}"
CR2=$(ctl "{\"tool\":\"close_window\",\"args\":{\"id\":$VPID2}}")
echo "VOCAB-VPLAYER-CLOSE: ${CR2:-none} (차분 오염 소각)"
vcleanup                                  # [fix r1 — permissions 원복+leftover 소각 — 무해 재입(성공 출구 = 병합 없었으면 바이트 등호)]
rm -f -- "$0"
echo "VOCAB-LIVE-FINISHED toggle=$TOG"
exit 0
VOCEOF
VOCAB_VERB=OK
$SSH "cat > \$TMPDIR/t6pv_vocab.sh" < "$SCRATCH/t6pv_vocab_remote.sh" || FAIL "vocab script push failed"
VOUT=$($SSH "bash \$TMPDIR/t6pv_vocab.sh" 2>&1)
VRC=$?
printf '%s\n' "$VOUT" | sed 's/^/VOCAB: /'
if [ "$VRC" -eq 255 ]; then
    echo "RUN-DROPPED: ssh 채널 단절 — 재실행=증분(T5 원장)"
    VOUT=$($SSH "bash \$TMPDIR/t6pv_vocab.sh" 2>&1)
    VRC=$?
    printf '%s\n' "$VOUT" | sed 's/^/VOCAB[2nd]: /'
fi
if grep -aq 'VOCAB-LIVE-FINISHED toggle=1' <<< "$VOUT" && ! grep -aq 'VOCAB-FAIL' <<< "$VOUT"; then
    VPLIVE=1
    echo "VOCAB-LIVE-OK"
else
    VPLIVE=0
    VOCAB_VERB="MISS_VOCAB"
    echo "VOICE-PHONE-FAIL(vocab-live: 라이브 소비 세그먼트 미성립 — rc=$VRC — 위 원문)"
fi
recover "\$TMPDIR/t5_chat_vocab_vp.png"     "$SCRATCH/pv_vocab_vp.png"
recover "\$TMPDIR/t5_chat_vocab_notify.png" "$SCRATCH/pv_vocab_notify.png"
# 폰 측 캡처 원문 소각 — 잔상 0 계약(B.2 산출)
$SSH "rm -f \$TMPDIR/t5_chat_vocab_vp.png \$TMPDIR/t5_chat_vocab_notify.png \$TMPDIR/t6pv_vocab.sh" 2>/dev/null
echo "VOCAB-SCRATCH-BURIED"

# ── C. 복원(T5 restore 계약 — chat close+Music 복원+permissions 원복+잔상) ──
echo "=== C. 복원 — chat close + Music 창 복원 ==="
$SSH "cat > \$TMPDIR/t6pv_rest.sh" <<'REOF'
#!/bin/bash
set -u
cd ~/JKENGINE/engine || exit 1
TMPD="${TMPDIR:-/data/data/com.termux/files/usr/tmp}"
PERM=buildterm/permissions.json
cp "$PERM" "$TMPD/t6pv_restperm"
awk -v newline="    \"close_window\": \"allow\"" '
    BEGIN { n = 0 }
    { lines[n++] = $0 }
    END {
        last = -1
        for (i = n - 1; i >= 0; i--)
            if (lines[i] ~ /^[ \t]*\}[ \t\r]*$/) { last = i; break }
        if (last < 1) { print "PV-PERM-MERGE-FAIL"; exit 1 }
        prev = lines[last - 1]
        sub(/[ \t\r]*$/, "", prev)
        if (prev !~ /,$/) prev = prev ","
        lines[last - 1] = prev
        for (i = 0; i < n; i++) {
            if (i == last) print newline
            print lines[i]
        }
    }' "$PERM" > "$PERM.new" && mv "$PERM.new" "$PERM" || { echo "PV-PERM-FAIL"; exit 1; }
ctl() { timeout 20 ./buildterm/jkdesktop agentctl "$1" 2>/dev/null | grep -a '{' | head -1; }
CH=$(printf '%s' "$(ctl '{"tool":"list_windows","args":{}}')" | grep -aoE '\{"id":[0-9]+,"title":"[Cc]hat"' | grep -oE '[0-9]+' | head -1)
if [ -n "$CH" ]; then
    CR=$(ctl "{\"tool\":\"close_window\",\"args\":{\"id\":$CH}}")
    echo "CHAT-CLOSE: $CR"
    sleep 2
else
    echo "CHAT-ALREADY-CLOSED"
fi
cmp -s "$PERM" "$TMPD/t6pv_restperm" || { cp "$TMPD/t6pv_restperm" "$PERM"; }
cmp -s "$PERM" "$TMPD/t6pv_restperm" || { echo "PV-PERM-RESTORE-FAIL"; exit 1; }
echo "PERM-RESTORED-OK ($(wc -c < "$PERM") bytes 등호)"
rm -f "$TMPD/t6pv_restperm" "$TMPD/t5_chat_rec.png" "$TMPD/t5_chat_done.png" \
      "$TMPD/t5_chat_done2.png" "$TMPD/t5_chat_ready.png" 2>/dev/null
rm -f "$TMPD"/t6pv_*.m4a 2>/dev/null   # 중간 컨테이너 잔산 — 인용 밖 glob(죽은 rm 수형)
ls "$TMPD" | grep 'jkchat_voice.*rec.m4a' | while read -r z; do rm -f "$TMPD/$z"; done
rm -f \$0 2>/dev/null
echo "RESTORE-FINISHED"
REOF
ROUT=$($SSH "bash \$TMPDIR/t6pv_rest.sh" 2>&1) || true
sed 's/^/RESTORE: /' <<< "$ROUT"
grep -aq 'RESTORE-FINISHED' <<< "$ROUT" || echo "VOICE-PHONE-FAIL: 복원 절차 미완주(위 원문)"
$SSH "cd ~/JKENGINE/engine && timeout 20 ./buildterm/jkdesktop agentctl '{\"tool\":\"launch_app\",\"args\":{\"app\":\"music\"}}' 2>/dev/null | head -1" | sed 's/^/MUSIC-RESTORE: /'
sleep 5
$SSH "cd ~/JKENGINE/engine && timeout 20 ./buildterm/jkdesktop agentctl '{\"tool\":\"list_windows\",\"args\":{}}' 2>/dev/null | head -1" | sed 's/^/FINAL-WINDOWS: /'
NLEFT=$($SSH 'ls $TMPDIR 2>/dev/null | grep -c "jkchat_voice.*\(\.wav\|\.rec\.m4a\)"' 2>/dev/null)
echo "REMNANT: phone jkchat_voice temp files left=$NLEFT (0 = 잔상 0 계약)"
[ "$NLEFT" = "0" ] || echo "VOICE-PHONE-FAIL(remnant: 폰 음성 잔존 $NLEFT건 — 수동 소각)"
$SSH "rm -f \$TMPDIR/t6pv_boot.sh \$TMPDIR/t6pv_tap.sh \$TMPDIR/t6pv_rest.sh \$TMPDIR/t5_voice_probe \$TMPDIR/t5_voice_probe.cpp \$TMPDIR/t6pv_fixture.py \$TMPDIR/jk_t6pv_fixture.wav" 2>/dev/null
echo "T-PUSH-SCRATCH-BURIED: 폰 probe 스크래치 소각"

echo "=== 폰 VERDICT (요약 스트림 계약) ==="
# fail-closed 게이트 = 3판 전부(WSL 축 FC=3과 등형 — T6 fix r1: fc3=3판의
# gate-model-missing 원문 등호가 본 판정 부울에도 채용될 수 있었다 — 2판만
# 검사한 fc3 누적 = 리뷰 Important(게이트 비대칭의 phone 축)).
FC3=1
# [원장행]: 판 3 = 현재 계약 원문(env 부재 경로 = 빈 반환 — park-5 쌍둥이 정합)
# 또는 T6 스폰 leg 계보 원문(rc=3 stderr) 양쪽 수용(§A 게이트 행 동형).
{ grep -aqF 'gate-model-missing=[구두 끔 (whisper 모델 부재)]' "$SCRATCH/phone_voice_probe.out" \
  || grep -aqF 'gate-model-missing=[구두 rc=3 stderr: ' "$SCRATCH/phone_voice_probe.out"; } || FC3=0
GATE_OK=1
grep -aqF 'gate-env-unset=[구두 끔 (WHISPER_ROOT 미설정)]' "$SCRATCH/phone_voice_probe.out" || GATE_OK=0
grep -aqF 'gate-bin-missing=[구두 끔 (whisper-cli 부재)]' "$SCRATCH/phone_voice_probe.out" || GATE_OK=0
[ "$FC3" -eq 1 ] || GATE_OK=0
FIX_OK=1
printf '%s' "$FIX_DRECT" | grep -aq 'fixture-direct=1' || FIX_OK=0
MIC_OK=1
printf '%s' "$MIC_START" | grep -aq 'mic-start=1' || MIC_OK=0
printf '%s' "$MIC_STOP" | grep -aq 'mic-stop=1' || MIC_OK=0
TAP_OK=$RUN_VERB
PV="VOICE-PHONE-FAIL"
[ "$GATE_OK" -eq 1 ] && [ "$FIX_OK" -eq 1 ] && [ "$MIC_OK" -eq 1 ] && [ "$TAP_OK" = "OK_ALL" ] && PV="VOICE-PHONE-OK"
if [ "$PV" = "VOICE-PHONE-OK" ]; then
    echo "VOICE-PHONE-VERDICT: VOICE-PHONE-OK(폰 축 — T5 원문 승계+재측정 $PHV_TAP_RUNS런: fail-closed 3판 원문 등호·Termux:API 마이크 leg(개방/정지/세션 판정 — 어휘 실측 계약)·fixture 사슬==직접 스폰·[구두] 탭 표면 캡처 런 md5 등호+기록 도착 바이트 증가·permissions 병합/원복 등호 — 전사 본문=CANON 밖(§2.4)·무발화 환경의 전사 원문 = 캡처 육안/Read — 실발화(발화→앱 실행 체감) = 사용자 EYES ①② 몫)"
else
    echo "VOICE-PHONE-VERDICT: $PV(honest-fail — gate=$GATE_OK fc3=$FC3 fixture=$FIX_OK mic=$MIC_OK tap=$TAP_OK — 사유는 상단 VOICE-PHONE-FAIL 행)"
fi
# VOICE-PHONE-RUN-VALS(요약 스트림 계약) = 하단 단일 행(신설 vocab 필드 합산 —
#   기존 필드 전량 원존 — probe stdout 수취 5종 보존 승계).
echo "=== 폰 VOCAB-VERDICT (#115b 신설 어휘 세그먼트 — 별행 1행 — VOICE-PHONE-VERDICT 무변경) ==="
VOC_OK=0
[ "$VROUTE" -eq 5 ] && [ "$VEXACT" -eq 1 ] && [ "$BEX" -eq 2 ] && [ "${VPLIVE:-0}" -eq 1 ] && VOC_OK=1
if [ "$VOC_OK" -eq 1 ]; then
    echo "VOCAB-PHONE-VERDICT: VOCAB-PHONE-OK(#115b 신설 어휘 폰 축 — 발화 8건 라우터 원문 등호(즉발 5+무인자 강제 1+배제 stub 2 — buildterm 배포 진실원 재실측)·app_tool 릴레이→vplayer 소비(accepted+opened 원문+토글 2탭 상반+get_status 기록 반영)·'알림 보여줘'→Notifications Launch 창 원문 — 폰 캡처 2종 md5 등호 회수 — 전사 본문=CANON 밖(§2.4)·발화 체감 = 사용자 EYES ① 몫)"
else
    echo "VOCAB-PHONE-VERDICT: VOCAB-PHONE-FAIL(honest — route=${VROUTE:-0}/5 exact=${VEXACT:-0}/1 excl=${BEX:-0}/2 live=${VPLIVE:-0} — 사유 열은 상단 VOICE-PHONE-FAIL 행)"
fi
echo "VOICE-PHONE-RUN-VALS: tap_runs=${PHV_TAP_RUNS} gate=3판 fixture_direct=${FIX_OK} mic=${MIC_OK} tap=${TAP_OK} runverdict=${PV} vocab=${VOC_OK} vocab_route=${VROUTE} vocab_excl=${BEX} vocab_live=${VPLIVE:-0} captures=engine/tmp/pv_run*_*.png engine/tmp/pv_vocab_*.png"
echo "PHONE-VOICE-END"
# EOF — 끝 개행 유지.