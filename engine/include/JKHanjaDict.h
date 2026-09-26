#ifndef JKHANJADICT_H
#define JKHANJADICT_H

#include <cstdint>
#include <vector>

namespace jk::hanja {

// 한자 사전 공급자 계약 (docs/66 — O1 한자 변환).
//
// kssmSyllable: 변환 대상 조합형(KSSM) 완성 음절 1자 — 2바이트 쌍을
//   (first << 8) | second 로 압축한 값(JKEdit/TerminalView 버퍼가 KSSM
//   2바이트 저장을 쓴다).
// *out: 한자 후보들 — 같은 KSSM 쌍 인코딩의 1자 목록. 사전 순서를 유지한다
//   (MS IME 관례의 빈도순 — HanjaDic COM이 돌려주는 순서).
//
// 실패=false + *out 클리어. 호출자는 후보 창을 열지 않고 조용히 무시한다.
// UI 스레드 전용: 시스템 채널은 COM STA + 사전 캐시를 쓴다(docs/66 §3.5).
using Provider = bool (*)(uint16_t kssmSyllable, std::vector<uint16_t>* out);

// 직전 음절의 한자 후보 조회. 시스템 채널(HanjaDic COM, docs/66 §3.5) 또는
// SetProviderForTest로 주입된 프로바이더 경유. 음절별 전체 후보를 캐시한다.
bool Candidates(uint16_t kssmSyllable, std::vector<uint16_t>* out);

// 사전 채널 존재 여부. 최초 호출에서 초기화(COM 생성+OpenMainDic+'한' 채질).
// false면 전 경로 no-op — 비한글 환경 graceful(docs/66 위험 6).
bool Available();

// 유닛 프로브 주입. nullptr=시스템 사전 복귀. 주입 중 Available()=true —
// 모드 로직 테스트는 실제 사전 API와 무관하게 결정론적으로 돈다(docs/65 O2
// 양성 대조군 철학).
void SetProviderForTest(Provider p);

} // namespace jk::hanja

#endif // JKHANJADICT_H