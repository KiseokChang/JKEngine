// 출하 시딩 원천 판정 (스펙 2026-10-05-slot-ship-tool §1-2 — docs/74 §5
// 단 2 출하 라인). 규약: 외부 파일(workshop 진실원)이 이미 있으면 아무것도
// 하지 않는다(외부가 이긴다 — 수신 기기 진실원 존중, 스펙 §1-3). 부재 시
// 출하 팩의 파묻힌 SCRI 원문으로 시딩하고, 그것도 없으면 템플릿(현행 회귀).
// 반환: 0=시딩 불요, 1=shipped 시딩, 2=template 시딩, -1=오류(error 채움).
#ifndef JK_CLIENT_APPS_JKWORKSHOPSEED_H
#define JK_CLIENT_APPS_JKWORKSHOPSEED_H

#include <string>

namespace jk {
int WorkshopSeedScript(const std::string& externalPath,
                       const std::string& shippedScriptPath,
                       const std::string& templateText,
                       std::string& error);
}

#endif  // JK_CLIENT_APPS_JKWORKSHOPSEED_H