// 단일 프로세스 터미널 (jkdesktop.exe terminal). 창 서버 없이 ConPTY 터미널을
// 띄운다 — 클라이언트 모드(ClientTerminalApp, docs/22)와 같은 grid/parser/
// atlas/pty 스택과 TerminalView를 쓰지만 JKApplication 위에 직접 얹는다.
// JKClientApplication의 OnIdle/IsFrameDirty 훅이 없으므로 PTY 펌프는 레거시
// 타이머 이벤트(30ms)에 올라타고, 종료는 PreProcessMessage에서 false 반환
// (단일 프로세스 종료 관용구 — VectorFontApp의 ALT+X 경로와 동일).
#include <apps/TerminalApp.h>
#include <apps/JKTerminalConfig.h>
#include <apps/TerminalView.h>

#include <cstdio>

namespace jk {

TerminalApp::~TerminalApp() = default;

void TerminalApp::OnInit() {
    // terminal.json: 모든 키는 기본값으로 폴백 — 파일이 없거나 깨져도 시작이
    // 실패하지 않는다 (ClientTerminalApp::OnInit와 동일).
    JKTerminalConfig cfg;
    if (!cfg.Load(JKTerminalConfig::DefaultPath())) {
        std::printf("[terminal] no terminal.json next to the exe — defaults\n");
        std::fflush(stdout);
    }

    grid_ = std::make_unique<JKTerminalGrid>();
    grid_->SetScrollbackMax(static_cast<size_t>(cfg.scrollback));
    parser_ = std::make_unique<JKVtParser>();
    parser_->Attach(grid_.get());
    atlas_ = std::make_unique<JKGlyphAtlas>();
    // Consolas는 Windows 기본 폰트; 없으면 뷰가 placeholder를 그린다.
    atlas_->Init(cfg.font.c_str(), kTermCellW, kTermCellH);
    // Malgun Gothic은 Consolas에 없는 한글/CJK 블록을 담당 (docs/26 단계 1);
    // 실패 시 넓은 글리프만 placeholder로 degrade된다.
    atlas_->InitFallback(cfg.fontFallback.c_str());
    pty_ = std::make_unique<JKConPtyBridge>();
    shell_ = cfg.shell;

    // TerminalView는 JKWindow 서브클래스이므로 그 자체가 메인 윈도우가 된다.
    // RespondMessage가 JKWindow로 위임하므로 크롬 입력 처리도 정상 동작한다.
    // 클라 모드와 달리 WA_CHROMELESS를 쓰지 않는다 — 단일 모드에서는
    // JKWindow가 그리는 타이틀바/테두리가 창의 크롬이다 (기존 단일 앱 관례).
    auto view = std::make_unique<TerminalView>(
        parser_.get(), grid_.get(), atlas_.get(), GetResourceCache());
    view->SetOnInput([this](const char* data, size_t len) { WriteToPty(data, len); });
    view->SetOnResize([this](int cols, int rows) { OnViewResized(cols, rows); });
    view->SetTheme(cfg.themeBg, cfg.themeFg);
    view->SetTitle("Terminal");
    view->SetWindowRect(JKRect{ 0, 0, 800, 500 });   // 클라 meta와 동일한 초기 크기
    view_ = view.get();

    SetMainWindow(std::move(view));
    // PTY 펌프 + 커서 깜빡임을 한 레거시 타이머로 구동한다. 30ms 틱이 출력
    // 지연 상한이자 펌프 주기; 깜빡임은 틱 카운터로 ~540ms (클라의 530ms와
    // 동등). 별도 AddTimer보다 winId 디스패치가 없는 쪽이 단순하다.
    SetTimerInterval(30);
}

void TerminalApp::OnClose() {
    if (pty_) {
        pty_->Stop();
    }
}

bool TerminalApp::PreProcessMessage(const JKEvent& ev) {
    // 휠 이벤트는 dx/dy만 있고 히트 타깃이 없어 라우팅 전에 가로챈다
    // (ClientTerminalApp와 동일).
    if (ev.type == JKEventType::MouseWheel && view_) {
        view_->HandleWheel(ev.dy);
        SyncRepaint();
    }
    if (ev.type == JKEventType::Timer) {
        if (!ptyStarted_ && !ptyStartFailed_) {
            // 뷰가 최종 레이아웃에 도달한 뒤 셸을 띄운다 — pty 셀 크기가
            // 그리드와 일치한다 (ClientTerminalApp::OnIdle 로직과 동일).
            if (view_ && view_->Cols() > 0 && view_->Rows() > 0) {
                ptyCols_ = view_->Cols();
                ptyRows_ = view_->Rows();
                ptyStarted_ = pty_->Start(shell_, ptyCols_, ptyRows_);
                if (!ptyStarted_) {
                    // ConPTY 생성/셸 스폰 실패는 일시적이지 않다 — 33Hz 재시도
                    // 루프 대비 한 번만 로그하고 재시도를 멈춘다.
                    ptyStartFailed_ = true;
                    std::printf("[terminal] pty start failed: %s\n", shell_.c_str());
                    std::fflush(stdout);
                    quitRequested_ = true;   // 셸 없이는 할 일이 없다
                }
            }
        } else if (ptyStarted_) {
            PumpPty();
            SyncRepaint();
        }
        // 커서 깜빡임: 30ms 틱 18번 ≈ 540ms.
        if (view_ && ++blinkCounter_ >= 18) {
            blinkCounter_ = 0;
            view_->TickBlink();
            SyncRepaint();
        }
    }
    if (quitRequested_) {
        return false;   // 셸 종료 → run loop 탈출 (단일 프로세스 종료 관용구)
    }
    return JKApplication::PreProcessMessage(ev);
}

void TerminalApp::PumpPty() {
    if (!pty_ || !pty_->IsValid()) return;

    std::string out;
    pty_->DrainOutput(out);
    if (!out.empty()) {
        parser_->Feed(reinterpret_cast<const uint8_t*>(out.data()), out.size());
    }
    const std::string replies = parser_->TakeReplies();
    if (!replies.empty()) {
        pty_->WriteInput(replies.data(), replies.size());
    }
    if (pty_->ShellExited()) {
        quitRequested_ = true;   // 셸 종료 → 앱 종료
    }
}

void TerminalApp::SyncRepaint() {
    // 클라의 IsFrameDirty/OnFrameCommitted 커밋-더티 최적화는 단일 모드에
    // 없다 — Invalidate가 곧 리페인트 예약이므로 이 순서로 충분하다.
    if (grid_ && grid_->IsDirty()) {
        if (view_) view_->Invalidate();
        grid_->ClearDirty();
    }
}

void TerminalApp::WriteToPty(const char* data, size_t len) {
    if (ptyStarted_ && pty_) {
        pty_->WriteInput(data, len);
    }
}

void TerminalApp::OnViewResized(int cols, int rows) {
    if (!ptyStarted_) return;   // 지연 시작이 뷰 크기를 그대로 쓴다
    if (cols == ptyCols_ && rows == ptyRows_) return;
    ptyCols_ = cols;
    ptyRows_ = rows;
    pty_->Resize(cols, rows);
}

} // namespace jk