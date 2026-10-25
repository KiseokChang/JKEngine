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

#include <algorithm>  // std::sort — 모델 dir 스캔의 결정론 선두 핀
#include <chrono>
#include <cstdlib>  // getenv — WHISPER_BIN·STT_MODEL 지역 오버라이드
#include <filesystem>
#include <fstream>  // 캡처 WAV 재독 — turn (2) 헤더 판정부 입력
#include <iterator>  // istreambuf_iterator — WAV 재독 절단
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

// 라우터 어휘 힌트 (EYESfix Step A — 2026-10-25 사용자 폰 EYES 판정
// "음성인식 엉망" — "지뢰찾기 실행" 발화 → 전사 "ok" — 수형): whisper-cli
// --prompt(초기 프롬프트 — cli.cpp 원문 "max n_text_ctx/2 tokens" 상한)에
// 명령 도메인 어휘를 부여해 제한 발화 도메인의 전사 품질 기대를 상향한다.
// 어휘 원문 = ChatRouter.cpp kRouteTable(+kAliasTable 한국어 별명) — 단일
// 진실원(§2.1)이나 익명 네임스페이스라 직순회 불가 → 문자 리터럴 수형 +
// 표와의 정합은 리포트 원문 계약(EYESfix-r1). 한국어 어휘만 두는 이유:
// 전사 언어 핀 = -l ko(위 kVoiceLanguage)이고 prompt 토큰은 디코딩
// 개시 시퀀스에 직접 들어가므로 영문 카탈로그 키(minesweeper) 혼입은 tiny류
// 의 영문 전사 유입 리스크 — 앱 이름은 한국어 별명(kAliasTable) 소관.
// **risk 원장행**: prompt는 디코딩 편향이므로 무발화/환각("click" 원존 —
// T5 §6)이 prompt 어휘 사이의 "앱명+트리거" 조합형 환각으로 이어져
// launch 미들 트리거가 성립할 수 있다 — v1 VAD 없이 완화 없음, 실발화
// 재판정은 사용자 EYES 몫(정직 기록).
constexpr char kVoicePrompt[] =
    "지뢰찾기 테트리스 실행해줘 실행 열어줘 켜줘 닫아줘 꺼줘 앞으로 가져와 "
    "포커스 창 목록 창목록 뭐 떠 있어";

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
// T3에서 공개 승계(헤더 계약 — verdict 게이트가 같은 진실원을 소비).
std::string ResolveCliPath(bool* found) {
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

// ggml 모델 리졸브(#112 T3 — 헤더 계약). exe 형제 dir의 stt-models/ — 조달기
// install_stt_models.sh의 축별 dir(Win build/stt-models·WSL buildwsl/stt-
// models·폰 buildterm/stt-models)가 전부 exe 형제 dir 모양으로 수렴하는
// 진실원. 파일명 편차(ggml-small.bin/ggml-tiny-ko.bin)는 dir 스캔으로 흡수 —
// .bin만 모아 문자열 정렬하고 선두 1개(결정론 — 조달기가 축당 1개를 내려놓는
// 계약의 보조 정합). STT_MODEL env = 완전 경로 지역 오버라이드(WHISPER_BIN과
// 같은 probe/현장 대응 몫 — 오버라이드가 우선). 스폰 0 — 존재 판정만.
std::string ResolveModelPath() {
    if (const char* envModel = std::getenv("STT_MODEL")) {
        if (*envModel) return envModel;
    }
    const std::string exe = jk::fs::GetExecutablePath();
    if (exe.empty()) return "";
    const std::filesystem::path dir =
        std::filesystem::path(exe).parent_path() / "stt-models";
    std::error_code ec;
    if (!std::filesystem::is_directory(dir, ec) || ec) return "";
    std::vector<std::string> names;
    for (const std::filesystem::directory_entry& e :
         std::filesystem::directory_iterator(dir, ec)) {
        if (e.path().extension() == ".bin") names.push_back(e.path().string());
    }
    if (ec || names.empty()) return "";  // 부재 = 빈 — verdict "whisper 모델 부재"
    std::sort(names.begin(), names.end());
    return names.front();
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
        QuoteArg(wavPath) + " -l " + kVoiceLanguage + " --prompt " +
        QuoteArg(kVoicePrompt);
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

// ---- turn — STT 전사 문자 → ChatRouteTurn 접점 (#112 T3 — 스펙 §3 ③ 안 A) ----
// 순수 접점 — SDL 무접촉(발화 수집 캡처 본체는 VoiceCaptureSDL.cpp의 별도
// 장치 사이 — §3 ⑤ 무음 계약: 재생 계통(vplayer 조그 무음 원존)과 접점 0).
namespace turn {

// ChatAction → (도구, 인자 JSON) — ClientChatApp::Submit(CaseF.cpp 146-179
// 스위치)이 이미 먹는 창 서버 도구 양식의 원문 수형(재용 — 새 파서 0·질의
// JSON 조립의 단일화). 무앱 Close/Focus = argless "{}"(포커스 창 위임 —
// 라우터 1n-7·2u-⑥ 원존 규약)·인용은 EscapeJson 쌍둥이 원문(MusicModel.h
// 선례 — "쌍둥이 원문 수형" 복제 계약).
bool ChatActionQuery(const ChatAction& a, std::string& tool,
                     std::string& args) {
    switch (a.kind) {
        case ChatAction::Launch: {
            if (a.app.empty()) return false;  // launch 무app — 지시 불성립(1n-6)
            tool = "launch_app";
            args = "{\"app\":\"" + JsonEscapeStr(a.app) +
                   "\"}";
            return true;
        }
        case ChatAction::Close:
            tool = "close_window";
            args = a.app.empty()
                       ? "{}"
                       : "{\"app\":\"" +
                             JsonEscapeStr(a.app) +
                             "\"}";
            return true;
        case ChatAction::Focus:
            tool = "focus_window";
            args = a.app.empty()
                       ? "{}"
                       : "{\"app\":\"" +
                             JsonEscapeStr(a.app) +
                             "\"}";
            return true;
        case ChatAction::ListWindows:
            tool = "list_windows";
            args = "{}";
            return true;
        case ChatAction::Info:
            return false;  // 도구 지시 없음 — 앱이 질의를 보내지 않는다
    }
    return false;
}

// 사슬 본체 — 헤더 계약 (1)게이트 (2)wav 판정 (3)전사 (4)접점 (5)도구 지시.
bool RunVoiceTurn(const std::string& wavPath, const agent::ChatConfig& cfg,
                  const ToolDispatcher& dispatch, VoiceTurnResult& out) {
    out = VoiceTurnResult{};

    // (1) fail-closed 게이트 — T1 판정부 verdict 원문 그대로(표면이 그대로
    //     인쇄한다 — 2v-a reason 4행 계약). 리졸브 2종 = run 원존 재용.
    bool binFound = false;
    std::string bin = run::ResolveCliPath(&binFound);
    std::string model = run::ResolveModelPath();
    const jk::stt::VoiceVerdict v = jk::stt::VoiceLegVerdict(
        jk::stt::VoiceWhisperRootSet(), binFound, !model.empty());
    if (!v.enabled) {
        out.err = v.reason;
        return false;
    }
    (void)bin;  // bin은 found 판정 소관 — 경로 원문은 run leg가 err에 실는다

    // (2) wav 헤더 판정부 — 캡처 결과의 정직 재독(SDL open이 allowed_changes=0
    //     로 16k 모노 s16le를 강제했으므로 통과가 규약 — 비적합 = 재합성 없이
    //     정직 거부(2v-⑥/⑦ 거부 규약의 런타임 계약)).
    std::ifstream wavFile(wavPath, std::ios::binary);
    std::vector<unsigned char> wavBytes;
    if (wavFile) {
        wavBytes.assign(std::istreambuf_iterator<char>(wavFile),
                        std::istreambuf_iterator<char>());
    }
    wavFile.close();
    if (!wav::Validate16kMono(wavBytes.data(), wavBytes.size())) {
        out.err = "구두 캡처 wav 부적합 (16 kHz 모노 PCM 아님 — 정규화 사전 계약)";
        return false;
    }

    // (3) 전사 — run leg(T2 스폰 원문 — err 원문 그대로 승계 — T4 표기 계약).
    std::string stt, trErr;
    if (!run::WhisperTranscribe(wavPath, model, stt, trErr)) {
        out.err = trErr;
        return false;
    }
    out.sttText = stt;  // 전사 원문 — §3 ④ 정직 기록 계약(오차의 표기 문화)

    // (4) 접점 — in-process ChatRouteTurn 직호출(안 A — "STT 텍스트 ==
    //     타이핑 텍스트" — §2.1). chat.json 미구성 cfg = 라우터 즉발+stub
    //     폴백 원존 — "LLM이 아니어도 구두는 조작된다". 정규화는 라우터
    //     NormalizeText 소관 — 여기서 재정규화(파서 복제)하지 않는다.
    jk::ChatAction action;
    out.guide = jk::ChatRouteTurn(out.sttText, action, cfg, &out.usedLlm);
    out.ok = true;

    // (5) 도구 지시 — Info가 아니면 기존 도구 경로로 전달(launch_app/…
    //     ClientChatApp SendQuery·agentctl과 같은 질의 JSON). dispatch 미지정
    //     = 접점 단정까지(질의 전달 생략 — 2v-d가 쓰는 관측 모양).
    if (action.kind == ChatAction::Info) return true;
    std::string tool, args;
    if (!ChatActionQuery(action, tool, args)) return true;  // 도달 없음
    if (dispatch && dispatch(tool, args)) return true;
    // T3 리뷰 Minor ③ 접선(T4 병행 — guide 유지+err 병기): 전달 실패여도
    // 접점이 결정한 안내문을 지우지 않는다 — 지시의 의도(guide)와 결과(err)를
    // 둘 다 표면이 정직 기록한다(ClientChatApp T4 소비 계약과 정합).
    out.err = "구두 도구 지시 전달 실패 (" + tool + ")";
    return false;
}

}  // namespace turn
}  // namespace voice
}  // namespace jk
