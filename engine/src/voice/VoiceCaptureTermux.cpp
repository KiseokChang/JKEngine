// push-to-talk 발화 수집 본체 — 폰(Termux/DeX) 축 (#112 T5 — 스펙
// 2026-10-25-stt-voice-design §3 ① 폰 축·③⑥ 폰 사슬·⑤ 무음 계약). 계약은
// include/voice/SttVoice.h namespace capture가 진실원 — VoiceCaptureSDL.cpp
// (Win 축)과 **동형 계약**의 폰 수형.
//
// 사슬 원문(스펙 §3 ①): Termux:API `termux-microphone-record`(사용자 설치+
// 마이크 권한 승인 — 스펙 R3 확정) → `-f <rec>` start → stop →
// `ffmpeg -ar 16000 -ac 1` 정규화 → 16 kHz 모노 s16le WAV →
// jk::voice::turn::RunVoiceTurn 소비. **MediaRecorder는 PCM을 내지 않는다**(
// 컨테이너 = 확장자에서 결정 — .m4a/aac) — 정규화가 필수라 정규화 사전
// 계약(wav::Validate16kMono 재독)이 여기서 실물로 단다.
//
// 스폰 leg = jk::process 어댑터만 접점(SttVoice.cpp와 같은 어댑터 — 축 무접점
// 복제 금지 계약의 재용). 캡처 본체는 selftest 밖(스펙 §3 ⑦) — 전사 비결정
// 원칙(§2.4)에 따라 selftest 어셜션 0 — 실측은 폰 probe 원문(T5/T6)·EYES 몫.
//
// 실패(Termux:API 부재·권한 미승인·ffmpeg 부재·결실 0) = 전부 정직 거부
// (fail-closed — 스펙 §3 ② 게이트 문화의 캡처 면).
//
// 컴파일 축: __ANDROID__ 만이 본체를 산다(Termux clang — bionic). Win/WSL/
// posix는 빈 TU(정적 lib 멤버만 존재 — 심볼 충돌 없음, 무영항 0) —
// VoiceCaptureSDL.cpp는 자기 쪽 축에서 !__ANDROID__로 본체를 산다(쌍방
// 가드 — 캡처 계약의 축별 수형이 1개만 링크된다 — 심볼 충돌 방지 계약).

#ifdef __ANDROID__

#include <voice/SttVoice.h>

#include <process/JKProcess.h>

#include <sys/stat.h>

#include <chrono>
#include <cstdint>
#include <cstdio>   // std::remove
#include <cstdlib>  // getenv — TERMUX_PREFIX
#include <fstream>
#include <iterator>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace jk {
namespace voice {
namespace capture {

namespace {

// 세션 상태 — 표면(UI) 스레드만 만진다(Start/Stop 동일 스레드 사이 — SDL 수형의
// 장치 스레드가 없는 축). 뮤텍스는 상태 읽기(IsCapturing/GetCaptureState)와의
// 순서 보장 몫.
std::mutex g_capMtx;
bool g_active = false;            // start~stop 사이 push-to-talk 세션
std::string g_wavPath;            // StopCapture가 WAV를 쓰는 예약 경로
std::string g_recPath;            // MediaRecorder의 컨테이너(정산 대상 — 잔산 금지)
std::string g_lastError;          // 최근 정직 거부 원문(표면 표기)

// termux-microphone-record의 컨테이너 확장 계약 — MediaRecorder가 확장자로
// OUTPUT_FORMAT을 결정하므로 PCM(.wav)을 직접 내지 않아 .m4a로 녹음한다
// (스펙 §3 ① 사슬의 ffmpeg 정규화가 그 뒤를 닫는다).
constexpr char kRecExtension[] = ".rec.m4a";

// 스폰 마이크 세션의 상한(push-to-talk — 짧은 발화 도메인). crash residue의
// 마이크 무한 held 방지 — -l은 초 단위 자동 종료(unlimited for 0). 정상 경로
// 에서는 StopCapture의 -q가 먼저 온다.
constexpr char kRecLimitSec[] = "60";

// 마감 — termux-api 명령은 수신기 미응답(Termux:API 부재)에서 hang할 수 있어
// 스폰 계약의 마감 문화(SttVoice kTranscribeDeadlineMs 선형)를 축소 재용.
// start/stop = 15s·ffmpeg = 60s(3s 원음 정규화 — 남은 것은 대기 여유).
constexpr std::chrono::milliseconds kCmdDeadlineMs(15000);
constexpr std::chrono::milliseconds kFfmpegDeadlineMs(60000);

// 파이프 정지 판정 코드 — jk::process::PeekPipeAvail의 동료 상수(SttVoice.cpp
// 수형과 같은 값 FAMILY — brokenError 계약).
constexpr int kErrBrokenPipeCode = 109;

// posix sh 이중 따옴표 지역 이스케이프 — SttVoice.cpp QuoteArg posix 원문의
// 파일-local(static)이므로 같은 규약의 축소 수형을 여기 독립으로 둔다(복제
// 금지 원칙의 "파서"가 아닌 인용 계약 — T3 리포트 §3 원문 계약 유지: 캡처
// 본체는 어댑터 접점 공유 — SpawnOptions만).
std::string Quoted(const std::string& s) {
    std::string out = "\"";
    for (const char ch : s) {
        if (ch == '"' || ch == '\\' || ch == '$' || ch == '`')
            out += '\\';
        out += ch;
    }
    out += '"';
    return out;
}

// Termux prefix — $PREFIX(Termux 표준 env)·TERMUX_PREFIX(명시 스펠링)의
// 폴백 2종. 실측 원장행(런 1): ssh/부팅 셸은 $PREFIX만 실어 TERMUX_PREFIX는
// 부재 — 2종 폴백으로 흡수. 미설정 = 실패(fail-closed — Termux 밖에서 이 TU는
// 빈 TU라 도달하지 않는다; 도달 = Termux env 붕괴).
std::string TermuxPrefix() {
    const char* p = std::getenv("TERMUX_PREFIX");
    if (!p || !*p) p = std::getenv("PREFIX");
    return (p && *p) ? std::string(p) : std::string();
}

// Termux:API/ffmpeg 바이너리 resolve — $PREFIX/bin/<name>(부팅 패키지 조달:
// termux-api pkg·ffmpeg pkg — termux_bootstrap.sh:21 ffmpeg 실측 선존).
// 절대 경로 선호(spawn은 sh -c PATH 조회 — PATH 상실 서버(env 스피커 스폰
// 계통)에서도 성립하는 정직 resolve). 부재 = false — 호출측이 정직 표기.
bool ResolveTermuxBin(const char* name, std::string& abs) {
    const std::string prefix = TermuxPrefix();
    if (!prefix.empty()) {
        const std::string cand = prefix + "/bin/" + name;
        struct stat st;
        if (::stat(cand.c_str(), &st) == 0 && S_ISREG(st.st_mode)) {
            abs = cand;
            return true;
        }
    }
    return false;
}

// 스폰+드레인 — SttVoice.cpp WhisperTranscribe의 drain 루프 수형(어댑터 접점
// 동형 — job kill-on-close + peek/read + 마감 kill + 수확). cmd는 sh -c로
// 도착하므로 인용은 Quoted 계약. rc/타임아웃/빈 stdout을 정직하게 돌려준다.
bool RunCaptureCommand(const std::string& cmd,
                       const std::chrono::milliseconds deadlineMs,
                       const std::string& what, std::string& out,
                       std::string& err) {
    jk::process::SpawnOptions opt;
    opt.commandLineUtf8 = cmd;
    opt.hideWindow = true;
    opt.inheritedStdioPipes = true;
    const jk::process::SpawnResult spawned = jk::process::Spawn(opt);
    if (!spawned.ok) {
        err = what + " (스폰 실패 err=" + std::to_string(spawned.errorCode) +
              ")";
        return false;
    }
    void* jobTree = jk::process::CreateKillOnCloseJob();
    jk::process::AssignToJob(jobTree, spawned);
    const auto t0 = std::chrono::steady_clock::now();
    bool outOpen = true, errOpen = true, timedOut = false;
    std::string stdoutBuf, stderrBuf;
    char chunk[4096];
    while (outOpen || errOpen) {
        bool progressed = false;
        if (outOpen) {
            uint32_t avail = 0;
            int broken = 0;
            if (jk::process::PeekPipeAvail(spawned.stdoutRead, &avail,
                                           &broken) &&
                avail > 0) {
                const int got = jk::process::ReadPipeData(
                    spawned.stdoutRead, chunk, sizeof(chunk));
                if (got > 0) {
                    progressed = true;
                    stdoutBuf.append(chunk, static_cast<size_t>(got));
                } else {
                    outOpen = false;
                }
            } else if (broken == kErrBrokenPipeCode) {
                outOpen = false;
            }
        }
        if (errOpen) {
            uint32_t avail = 0;
            int broken = 0;
            if (jk::process::PeekPipeAvail(spawned.stderrRead, &avail,
                                           &broken) &&
                avail > 0) {
                const int got = jk::process::ReadPipeData(
                    spawned.stderrRead, chunk, sizeof(chunk));
                if (got > 0) {
                    progressed = true;
                    stderrBuf.append(chunk, static_cast<size_t>(got));
                } else {
                    errOpen = false;
                }
            } else if (broken == kErrBrokenPipeCode) {
                errOpen = false;
            }
        }
        if (std::chrono::steady_clock::now() - t0 >= deadlineMs) {
            timedOut = true;
            break;
        }
        if (!progressed) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }
    if (timedOut) jk::process::TerminateJobTree(jobTree, 1);
    jk::process::CloseHandleLike(spawned.stdoutRead);
    jk::process::CloseHandleLike(spawned.stderrRead);
    jk::process::WaitForExit(spawned.process, 5000);
    uint32_t exitCode = jk::process::kStillActiveExit;
    jk::process::GetExitCode(spawned.process, &exitCode);
    jk::process::CloseHandleLike(spawned.process);
    jk::process::CloseHandleLike(jobTree);
    out = stdoutBuf;
    err = stderrBuf.substr(stderrBuf.size() > 300 ? stderrBuf.size() - 300
                                                  : 0);
    if (timedOut) {
        err = what + " (마감 초과 — Termux:API 수신기 무응답)";
        return false;
    }
    if (exitCode != 0 && exitCode != jk::process::kStillActiveExit) {
        err = what + " (rc=" + std::to_string(exitCode) +
              (out.empty() ? (err.empty() ? std::string() : " " + err)
                           : " " + out) +
              ")";
        return false;
    }
    return true;
}

// Termux:API 수신기 JSON의 거부 원문 판정 — `{"code":-1,...}`(권한 미승인 등)
// 가 들어오면 스폰 rc가 0이어도 정직 거부(조용한 실패 금지).
bool ApiRejected(const std::string& out) {
    return out.find("\"code\":-1") != std::string::npos ||
           out.find("\"code\": -1") != std::string::npos;
}

// Termux:API -i(JSON)의 `\/` 슬래시 이스케이프 정리 — 경로 대차의 단일화.
std::string UnescapeSlash(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size() && s[i + 1] == '/') {
            out += '/';
            ++i;
        } else {
            out += s[i];
        }
    }
    return out;
}

// -i JSON의 문자열 값 절단 — "outputFile":"<path>" (값 안 escape는 슬래시만
// 실측 — UnescapeSlash 선형). 부재 = 빈 문자열.
std::string JsonStringValue(const std::string& json, const char* key) {
    const std::string needle = std::string("\"") + key + "\"";
    const size_t at = json.find(needle);
    if (at == std::string::npos) return std::string();
    const size_t colon = json.find(':', at + needle.size());
    if (colon == std::string::npos) return std::string();
    const size_t q0 = json.find('"', colon + 1);
    if (q0 == std::string::npos) return std::string();
    const size_t q1 = json.find('"', q0 + 1);
    if (q1 == std::string::npos) return std::string();
    return UnescapeSlash(json.substr(q0 + 1, q1 - q0 - 1));
}

// -i 원문의 isRecording 판정 — true/false 불린(JSON — 공백 스펠링 흡수).
bool JsonRecordingActive(const std::string& json) {
    const size_t at = json.find("\"isRecording\"");
    if (at == std::string::npos) return false;
    const size_t colon = json.find(':', at);
    if (colon == std::string::npos) return false;
    return json.find("true", colon) != std::string::npos &&
           json.find("false", colon) == std::string::npos;
}

// 세션 사전·사후 판정 1눈 — termux-microphone-record -i(스폰 0 마감 계약).
// 활성 세션의 outputFile 원문(스페이스 이스케이프 정리)을 돌려준다.
bool QueryTermuxSession(const std::string& recorder, bool* active,
                        std::string& outputFile, std::string& err) {
    std::string out, _err;
    if (!RunCaptureCommand(Quoted(recorder) + " -i", kCmdDeadlineMs,
                           "구두 캡처 세션 판정 실패 (Termux:API)", out, _err)) {
        err = _err;
        return false;
    }
    const std::string js = UnescapeSlash(out);
    *active = JsonRecordingActive(js);
    outputFile = JsonStringValue(js, "outputFile");
    return true;
}

std::string Trim(const std::string& s) {
    const size_t beg = s.find_first_not_of(" \t\r\n");
    if (beg == std::string::npos) return std::string();
    const size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(beg, end - beg + 1);
}

}  // namespace

bool StartCapture(const std::string& wavPath, std::string& err) {
    const auto Reject = [&](const std::string& e) {
        err = e;
        std::lock_guard<std::mutex> lock(g_capMtx);
        g_lastError = err;
        return false;
    };
    std::string recorder;
    if (!ResolveTermuxBin("termux-microphone-record", recorder)) {
        return Reject("구두 캡처 불가 (termux-microphone-record 부재 — "
                      "Termux:API 설치·권한 확인)");
    }
    bool already = false;
    {
        std::lock_guard<std::mutex> lock(g_capMtx);
        already = g_active;
    }
    if (already) {
        return Reject("구두 캡처 이미 진행 중");  // (이중 개방 — 정직 거부)
    }
    std::string startOut, startErr;
    const std::string recPath = wavPath + kRecExtension;
    // Termux 마이크 세션은 기기 전역 1개 — 사전 판정(런 2 원장행: 스폰 rc=0
    // 이어도 기존 세션이 살면 "Recording already in progress"만 인쇄하고 새
    // 예약 경로는 조용히 건너뛴다 — 성공 위장 결손). 남의 세션 = 정직 거부(
    // 자동 -q 금지 — 남의 녹음 소각 금지), 같은 예약 경로의 잔상 = 내것 정산.
    bool sessActive = false;
    std::string outputFile, sessErr;
    if (QueryTermuxSession(recorder, &sessActive, outputFile, sessErr)) {
        if (sessActive && outputFile != recPath) {
            return Reject("구두 캡처 거부 (Termux:API 기존 세션: " +
                          (outputFile.empty() ? std::string("(unknown)")
                                              : outputFile) +
                          ")");
        }
        if (sessActive) {
            std::string qOut, qErr;
            (void)RunCaptureCommand(
                Quoted(recorder) + " -q", kCmdDeadlineMs,
                "구두 캡처 잔상 정산 실패 (Termux:API)", qOut, qErr);
        }
    }  // -i 실패 = 보류(사후 판정 1눈이 마감한다 — 정직)
    std::remove(recPath.c_str());  // 전 run 잔산 제거(예약 경로 정산 선행)
    // 실측 계약(런 2 원문 — usage): start 어휘 없음 — `-f <file>`이 개방·
    // `-q`가 종착(finalize·"Recording finished" 원문)·`-i`가 세션 판정.
    const std::string cmd = Quoted(recorder) + " -f " + Quoted(recPath) +
                            " -l " + kRecLimitSec;
    if (!RunCaptureCommand(cmd, kCmdDeadlineMs,
                           "구두 캡처 개방 실패 (Termux:API)", startOut,
                           startErr)) {
        return Reject(startErr.empty() ? "구두 캡처 개방 실패 (Termux:API)"
                                       : startErr);
    }
    if (ApiRejected(startOut)) {
        // 권한 미승인 등의 수신기 거부 — 원문 그대로 표면이 먹는다.
        return Reject("구두 캡처 거부 (Termux:API: " + Trim(startOut) + ")");
    }
    // 사후 판정 1눈 — 예약 경로로의 개방이 실제로 성립했는지(출력 행만으로는
    // 승락 없음 — 성공 위장 결손 소각).
    {
        bool nowActive = false;
        std::string nowFile;
        if (!QueryTermuxSession(recorder, &nowActive, nowFile, sessErr) ||
            !nowActive || nowFile != recPath) {
            return Reject("구두 캡처 개방 확인 실패 (Termux:API — " +
                          Trim(startOut) + ")");
        }
    }
    {
        std::lock_guard<std::mutex> lock(g_capMtx);
        g_active = true;
        g_wavPath = wavPath;
        g_recPath = recPath;
        g_lastError.clear();
    }
    return true;
}

bool StopCapture(std::string& err) {
    const auto Reject = [&](const std::string& e) {
        err = e;
        std::lock_guard<std::mutex> lock(g_capMtx);
        g_lastError = err;
        return false;
    };
    std::string wavPath, recPath, recorder, ffmpeg;
    {
        std::lock_guard<std::mutex> lock(g_capMtx);
        if (!g_active) {
            err = "구두 캡처 진행 중 아님";
            return false;  // 상태 무변(SDL 수형 동일 짐 — 진행 중 아니었음)
        }
        wavPath = g_wavPath;
        recPath = g_recPath;
    }
    // 세션은 여기서 끝낸다 — 이후의 모든 실패는 부분 상태를 남기지 않는다
    // (rec 잔산은 아래 정산 경로에서 소각 — SDL 수형의 "절반 WAV 잔산 금지"
    // 선형).
    {
        std::lock_guard<std::mutex> lock(g_capMtx);
        g_active = false;
        g_wavPath.clear();
        g_recPath.clear();
    }
    if (!ResolveTermuxBin("termux-microphone-record", recorder)) {
        return Reject("구두 캡처 정지 실패 (termux-microphone-record 부재 — "
                      "Termux:API 확인)");
    }
    std::string stopOut, stopErr;
    // 실측 계약(런 2 원문): 어휘 stop 없음 — `-q`가 종착(파일 finalize —
    // "Recording finished: <file>" 원문 행). quit 어휘 = 이 계약의 실측판.
    if (!RunCaptureCommand(Quoted(recorder) + " -q", kCmdDeadlineMs,
                           "구두 캡처 정지 실패 (Termux:API)", stopOut,
                           stopErr)) {
        return Reject(stopErr);
    }
    if (ApiRejected(stopOut)) {
        return Reject("구두 캡처 정지 거부 (Termux:API: " + Trim(stopOut) +
                      ")");
    }
    // MediaRecorder finalize — stop 수신기가 돌아온 뒤 파일이 커지는 꼬리를
    // 안정 재판정(2회 연속 등크)까지 소화한다(최대 ~4s — 3s 원음 대기 여유).
    size_t lastSize = 0;
    for (int i = 0; i < 8; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        struct stat st;
        size_t now = 0;
        if (::stat(recPath.c_str(), &st) == 0) now = static_cast<size_t>(st.st_size);
        if (now > 0 && now == lastSize) break;
        lastSize = now;
    }
    if (lastSize == 0) {
        std::remove(recPath.c_str());
        return Reject("구두 캡처 빈 결실 (Termux:API 녹음 0바이트)");
    }
    if (!ResolveTermuxBin("ffmpeg", ffmpeg)) {
        std::remove(recPath.c_str());
        return Reject("구두 캡처 정규화 불가 (ffmpeg 부재 — 부팅 조달 계약)");
    }
    std::string ffOut, ffErr;
    const std::string ffCmd = Quoted(ffmpeg) + " -y -i " + Quoted(recPath) +
                              " -ar 16000 -ac 1 -c:a pcm_s16le " +
                              Quoted(wavPath);
    if (!RunCaptureCommand(ffCmd, kFfmpegDeadlineMs, "구두 캡처 정규화 실패",
                           ffOut, ffErr)) {
        std::remove(recPath.c_str());
        std::remove(wavPath.c_str());  // 부분 산물 잔산 금지
        return Reject(ffErr.empty()
                          ? "구두 캡처 정규화 실패 (ffmpeg)"
                          : "구두 캡처 정규화 실패 (ffmpeg: " + ffErr + ")");
    }
    std::remove(recPath.c_str());  // 정산 — 미들 컨테이너 잔산 금지
    // 정규화 산물의 정직 재독(wav::Validate16kMono — RunVoiceTurn (2)와 같은
    // 진실원 — "리샘플 재합성 금지" 계약의 소독 검증).
    std::ifstream wavFile(wavPath, std::ios::binary);
    std::vector<unsigned char> wavBytes;
    if (wavFile) {
        wavBytes.assign(std::istreambuf_iterator<char>(wavFile),
                        std::istreambuf_iterator<char>());
    }
    wavFile.close();
    if (!wav::Validate16kMono(wavBytes.data(), wavBytes.size())) {
        std::remove(wavPath.c_str());  // 부적합 산물 잔산 금지
        return Reject("구두 캡처 wav 부적합 (16 kHz 모노 PCM 아님 — 정규화 "
                      "사전 계약)");
    }
    return true;
}

bool IsCapturing() {
    std::lock_guard<std::mutex> lock(g_capMtx);
    return g_active;
}

CaptureState GetCaptureState() {
    std::lock_guard<std::mutex> lock(g_capMtx);
    CaptureState st;
    st.capturing = g_active;
    st.wavPath = g_wavPath;
    st.lastError = g_lastError;
    return st;
}

}  // namespace capture
}  // namespace voice
}  // namespace jk

#endif  // __ANDROID__ (폰 축 외 = 빈 TU — SDL 수형과의 축별 상호 배타 계약)