// STT 구두 스폰 leg (#112 T2 — 스펙 2026-10-25-stt-voice-design §3 ②).
// 순수 부품(wav/transcript)은 헤더 인라인(include/voice/SttVoice.h —
// GalleryModel/MusicModel 선례)이고, 스폰 본체는 TU — JKLmEngine.cpp 선형
// (include/agent/JKLlmEngine.h 선언 + src/agent/JKLlmEngine.cpp 본체).
//
// 원존 수형 = JKLmEngine.cpp LlmTurnThread의 스폰 스케치 그대로 —
// jk::process 어댑터 Spawn(inheritedStdioPipes)·kill-on-close job
// (AssignToJob (job, process) 계약)·peek/read 드레인 루프·마감 kill
// (TerminateJobTree)·WaitForExit(5000) 수확·stderr 꼬리 진단. 라이브러리
// 링크 금지 계약(폰 #3117 segfault 원존) — whisper/ggml 정적 lib에 접점 0.

#include <voice/SttVoice.h>

#include <fs/JKFs.h>
#include <process/JKProcess.h>

#include <chrono>
#include <cstdlib>  // getenv — WHISPER_BIN 지역 오버라이드
#include <filesystem>
#include <string>
#include <system_error>
#include <thread>

namespace jk {
namespace voice {
namespace run {

// cli.cpp 원문 인자 형식의 고정 언어 핀(§1.3 한국어 품질 중심 — 발화 예의
// "지뢰찾기 켜줘"). tiny-ko(komixv2)도 small도 -l ko로 결정론 적용 —
// 무지정(cli 기본 "en")은 한국어 발화를 암묵 망각하므로 금지. 영어 발화
// 축 재판정은 T3/T4 사용자 EYES 몫.
constexpr char kVoiceLanguage[] = "ko";

// 전체 마감 — JKLmEngine kTurnIdleKillMs(10분) 선형. transcribe는 모든
// 진행 로그가 stderr라 관측 유무로 목을 줄 수 없어 **전체 마감** 1종만
// 두었다(T2 계약 "타임아웃"). 초과 = TerminateJobTree kill(격리 계약).
constexpr std::chrono::milliseconds kTranscribeDeadlineMs(600000);

// win32: CreateProcessW(commandLine)의 argv 분해 규약 — 인용 안 백슬래시
// 런+따옴표 이스케이프(CommandLineToArgvW 규약). posix: sh 이중 따옴표
// 지역 — ShellDqEscape posix leg 원문(\ $ ` 백틱 생존 이스케이프).
static std::string QuoteArg(const std::string& s) {
#ifdef _WIN32
    std::string out = "\"";
    size_t backslashes = 0;
    for (char ch : s) {
        if (ch == '\\') {
            ++backslashes;
            out += ch;
        } else if (ch == '"') {
            out.append(backslashes + 1, '\\');
            out += '"';
            backslashes = 0;
        } else {
            backslashes = 0;
            out += ch;
        }
    }
    out.append(backslashes, '\\');  // 닫는 따옴표 앞 런도 2배(규약 동형)
    out += '"';
#else
    std::string out = "\"";
    for (char ch : s) {
        if (ch == '\\')
            out += "\\\\";
        else if (ch == '"')
            out += "\\\"";
        else if (ch == '$')
            out += "\\$";
        else if (ch == '`')
            out += "\\`";
        else
            out += ch;
    }
    out += '"';
#endif
    return out;
}

// whisper-cli 바이너리 resolve — exe 형제(dir)/whisper-build/whisper-cli(.exe)
// (build/whisper-build·buildwsl·buildterm 공통 모양 — T1 빌드 산출 경로 원문),
// WHISPER_BIN env가 지역 오버라이드(probe/현장 대응 몫). 부재 = nullopt.
static std::string ResolveCliPath(bool* found) {
    *found = false;
    std::string bin;
    if (const char* envBin = std::getenv("WHISPER_BIN")) {
        if (*envBin) bin = envBin;
    }
    if (bin.empty()) {
        const std::string exe = jk::fs::GetExecutablePath();
        if (exe.empty()) return "";
        std::filesystem::path leaf("whisper-cli");
#ifdef _WIN32
        leaf.replace_extension(".exe");
#endif
        const std::filesystem::path resolved =
            std::filesystem::path(exe).parent_path() / "whisper-build" / leaf;
        bin = resolved.string();
    }
    std::error_code ec;
    if (std::filesystem::exists(std::filesystem::path(bin), ec) && !ec) {
        *found = true;
        return bin;
    }
    return bin;  // 부재 경로 원문 그대로 — 찾는 쪽이 정직 표기한다
}

bool WhisperTranscribe(const std::string& wavPath,
                       const std::string& modelPath, std::string& out,
                       std::string& err) {
    out.clear();
    err.clear();

    // 1) 바이너리 resolve — fail-closed(T1 판정부 reason 원문 승계).
    bool binFound = false;
    const std::string bin = ResolveCliPath(&binFound);
    if (!binFound) {
        err = "구두 끔 (whisper-cli 부재): " + bin;
        return false;
    }

    // 2) 스폰 — jk::process 어댑터(LlmTurnThread 원존 수형). cmd.exe 접두
    //    불요 — 절대 경로 직접 CreateProcessW(shell 접점 소거).
    jk::process::SpawnOptions opt;
    const std::string args =
        QuoteArg(bin) + " -m " + QuoteArg(modelPath) + " -f " +
        QuoteArg(wavPath) + " -l " + kVoiceLanguage;
    opt.commandLineUtf8 = args;
    opt.hideWindow = true;           // CREATE_NO_WINDOW + SW_HIDE
    opt.inheritedStdioPipes = true;  // stdout/stderr 분리 부모 파이프
    const jk::process::SpawnResult spawned = jk::process::Spawn(opt);
    if (!spawned.ok) {
        err = "구두 스폰 실패 (err=" + std::to_string(spawned.errorCode) + ")" +
              (spawned.error.empty() ? "" : " " + spawned.error);
        return false;
    }
    void* readOut = spawned.stdoutRead;
    void* readErr = spawned.stderrRead;

    // 3) kill-on-close job — close == tree death(JKLmEngine 계약 b). grandchild
    //    파이프 홀딩으로 stdout EOF 안 오는 데드락을 마감 kill로 봉한다.
    void* jobTree = jk::process::CreateKillOnCloseJob();
    jk::process::AssignToJob(jobTree, spawned);

    // 4) 드레인 루프 — peek/read(비봉쇄)·전체 마감 1종(kTranscribeDeadlineMs —
    //    판정 매 루프·stderr 진행률 유무 무관 — fix r1).
    const auto tTurn = std::chrono::steady_clock::now();
    bool outOpen = true, errOpen = true;
    std::string stdoutBuf, stderrBuf;
    char chunk[4096];
    constexpr int kErrBrokenPipe = 109;  // ERROR_BROKEN_PIPE
    constexpr int kErrNoData = 232;      // ERROR_NO_DATA (pipe closed)
    bool timedOut = false;
    while (outOpen || errOpen) {
        bool progressed = false;
        if (outOpen) {
            uint32_t avail = 0;
            int broken = 0;
            if (jk::process::PeekPipeAvail(readOut, &avail, &broken) &&
                avail > 0) {
                const int got =
                    jk::process::ReadPipeData(readOut, chunk, sizeof(chunk));
                if (got > 0) {
                    progressed = true;
                    stdoutBuf.append(chunk, static_cast<size_t>(got));
                } else {
                    outOpen = false;  // read==0 (EOF) / -1 — 이 파이프 끝남
                }
            } else if (broken == kErrBrokenPipe || broken == kErrNoData) {
                outOpen = false;
            }
        }
        if (errOpen) {
            uint32_t avail = 0;
            int broken = 0;
            if (jk::process::PeekPipeAvail(readErr, &avail, &broken) &&
                avail > 0) {
                const int got =
                    jk::process::ReadPipeData(readErr, chunk, sizeof(chunk));
                if (got > 0) {
                    progressed = true;
                    stderrBuf.append(chunk, static_cast<size_t>(got));
                } else {
                    errOpen = false;
                }
            } else if (broken == kErrBrokenPipe || broken == kErrNoData) {
                errOpen = false;
            }
        }
        // 마감 판정 = 매 루프 무조건(fix r1 — 리뷰 Important 1) — stderr
        // 진행률이 계속 흘러 progressed=true인 동안에도 전체 마감 600s가
        // 산다. (!progressed 안에 두면 진행률이 흘러 마감이 불지 않는
        // idle-kill로 퇴화 — 선언 계약(전체 마감)과 부정합.) break 뒤의
        // kill은 아래 TerminateJobTree 명시 — 마감 계약의 문자 그대로.
        if (std::chrono::steady_clock::now() - tTurn >=
            kTranscribeDeadlineMs) {
            timedOut = true;
            break;
        }
        if (!progressed) {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }
    if (timedOut) jk::process::TerminateJobTree(jobTree, 1);
    jk::process::CloseHandleLike(readOut);
    jk::process::CloseHandleLike(readErr);
    // 수확 — WaitForExit(5000) 원문 관측(기다리고 실패 무시) + job close kill.
    jk::process::WaitForExit(spawned.process, 5000);
    uint32_t exitCode = jk::process::kStillActiveExit;
    jk::process::GetExitCode(spawned.process, &exitCode);
    jk::process::CloseHandleLike(spawned.process);
    jk::process::CloseHandleLike(jobTree);  // close IS the kill (contract b)

    const auto Tail = [](const std::string& s) {
        return s.size() > 400 ? s.substr(s.size() - 400) : s;
    };

    if (timedOut) {
        err = "구두 타임아웃 (600000ms — TerminateJobTree) stderr: " +
              Tail(stderrBuf);
        return false;
    }
    // rc 판정 — 259(kStillActiveExit)만 관측 미수확(수확 만료)으로 통과시키고
    // 실 실패 rc는 정직 거부한다(stdout이 있어도 rc≠0 = 스폰 계약 실패).
    if (exitCode != 0 && exitCode != jk::process::kStillActiveExit) {
        err = "구두 rc=" + std::to_string(exitCode) + " stderr: " +
              Tail(stderrBuf);
        return false;
    }

    // 5) 절단 — transcript::ParseLastText(순수 부품 재용 — 파서 복제 금지).
    out = transcript::ParseLastText(stdoutBuf);
    if (out.empty()) {
        // 259(수확 만료)·rc 0 등에서 stdout 결실 0 = 정직 실패 — stderr 꼬리
        // 진단(ollama-direct leg "빈 응답" 원존 관측 승계).
        err = stderrBuf.empty()
                  ? std::string("구두 빈 전사 (whisper-cli stdout 결실 0)")
                  : "구두 빈 전사 stderr: " + Tail(stderrBuf);
        out.clear();
        return false;
    }
    return true;
}

}  // namespace run
}  // namespace voice
}  // namespace jk