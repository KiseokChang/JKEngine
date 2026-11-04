#ifndef JKLLMTURNHTTP_H
#define JKLLMTURNHTTP_H

// T7 — LLM HTTP 어댑터 (플랜 docs/superpowers/plans/2026-11-04-jkapk-t7-llm-http.md
// — 스펙 2026-11-01-jkdesktop-apk-unified-design §5 "LLM 계약 통일"의 Task 7).
// 전제 검산 원존: jk::net = 리슨/수락만 있는 어댑터(ConnectTcp·HTTP 클라·TLS
// 부재) → (a) 로컬 ollama = 평문 HTTP 순수 native, (b) cloud(https://ollama.com)
// = 폰 플랫폼 TLS JNI 브리지(JkVoiceService env 선례 — 조달 0)의 이원화.
// Win/WSL의 cloud = fail-loud 정직 거부(빈 응답 = 소비자 stub 폴백 계약과 동형).
#include <cstdint>
#include <string>

namespace jk {
namespace agent {

// chat.json "llm_mode"의 판정(순수·결정 — selftest 등호 원존). 인정 값:
//   "off"(기존 엔진 계약 100% 원존 — 캐논 무영향) | "local" | "cloud".
// trim+ASCII 소문화 후 정확 등호만 인정; 위배·빈 값 = "off" (fail-closed).
std::string NormalizeOllamaMode(const std::string& raw);

// ollama POST /api/chat 요청 본문 조립(순수). stream:false — v1 = 버퍼드
// 단발(delta 콜백 미제공; 턴 진실원 = 응답의 message.content). JSON 이스
// 케이프는 JKAgentJson의 EscapeJson 재용(복제 금지 계약).
std::string BuildOllamaChatJson(const std::string& model,
                                const std::string& systemPrompt,
                                const std::string& prompt);

// POST /api/chat 응답 파스(순수): json.message.content를 꺼낸다. 본문이
// 스트리밍 NDJSON("stream":true 류)이면 마지막 개체의 content 승계(방어 —
// 데몬 버전 편차). 파스 실패 = false(정직 — 소비자 폴백 계약 그대로).
bool ParseOllamaChatReply(const std::string& body, std::string& content);

// HTTP 응답 결과. ok = 상태 200 + 본문 수취. body = 응답 본문(정직 전달 —
// 파스는 소비자 소관).
struct LlmHttpPostResult {
    bool ok = false;
    std::string body;
    std::string status;  // "200"/"401"/.. — 진단 원문(서예외 서술 방지)
};

// 평문(로컬) HTTP/1.1 POST — jk::net::ConnectTcp 재용. host는 NormalizeOllama
// Host 통과분(스키마 제거·포트 포함 상대 — 호출부 소관 분리). 본문 = JSON
// 원문. 실패(연결·전송·응답 형) = ok=false + status에 원인 어휘.
LlmHttpPostResult HttpPostPlain(const std::string& hostNorm, uint16_t fallbackPort,
                                const std::string& path,
                                const std::string& bodyJson, int timeoutMs);

// cloud 트랜스포트 주입점 — Android의 JNI TLS 브리지(JkHttp.java)가 심는다.
// 주입 부재 = 정직 실패("cloud transport not wired" — stub 폴백 계약과 동형;
// Win/WSL은 주입 영구 부재). 스턬 transport로 selftest가 단정한다.
using HttpsPostFn = LlmHttpPostResult (*)(const std::string& url,
                                          const std::string& bodyJson,
                                          const std::string& bearer,
                                          int timeoutMs);
void SetHttpsPostTransport(HttpsPostFn fn);
HttpsPostFn GetHttpsPostTransport();

// 폰 클라우드 턴 — https://ollama.com/api/chat + Bearer. 주입 transport 부재 =
// 정직 실패. 기존 TurnJob의 Finish 계약과 재용되는 게 턴 스레드 쪽(배선 T4).
inline constexpr char kOllamaCloudUrl[] = "https://ollama.com/api/chat";

}  // namespace agent
}  // namespace jk

#endif  // JKLLMTURNHTTP_H