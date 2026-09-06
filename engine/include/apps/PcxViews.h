#ifndef APPS_PCXVIEWS_H
#define APPS_PCXVIEWS_H

#include <memory>
#include <string>

namespace jk {

class JKWindow;

// PCX 뷰어 UI 빌더. 단일 프로세스 PcxApp과 클라이언트 모듈(ClientPcxApp)이
// 같은 창 구성 코드를 공유한다(TetrisGameWindow::Build와 동일한 재사용 패턴).
//
// filePath가 비어 있거나 로드에 실패하면 "Failed to load PCX file."
// 플레이스홀더를 보여준다. 앱 모듈은 CLI 인수를 받지 못하므로 클라이언트
// 래퍼는 빈 경로로 호출한다(단일 프로세스 `pcx` 무인수 실행과 동일 상태).
//
// 반환된 윈도우(1280x680, 제목 "PCX Viewer - SDL2 Port")는 호출자가
// 소유한다 — SetMainWindow로 루트 윈도우로 지정한다.
std::unique_ptr<JKWindow> CreatePcxViewerWindow(const std::string& filePath);

} // namespace jk

#endif // APPS_PCXVIEWS_H