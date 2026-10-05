#ifndef JK_WIRE_ENDPOINTS_H
#define JK_WIRE_ENDPOINTS_H

// Wire endpoint names shared by every process on the bus (server host,
// client host, agent clients). The server's acceptor listens on the same
// name all clients connect to — one constant, no per-file literals.
namespace jk {
namespace ipc {

inline constexpr char kWindowServerPipeName[] = "\\\\.\\pipe\\JKWindowServerPipe";

// posix에서 pipe-name 상수가 전송 어댑터의 fold 규칙(JKPipeTransport_posix
// MapEndpointName: pipe-접두 박탈+'/'→'_'+.sock)을 거치면 결정론 결과가
// /tmp/JKWindowServerPipe.sock — 셸 스폰 대기 루프 등 "파일 존재" 관측자가
// 그 결과 경로를 알아야 할 때 쓴다(플랜 G3). win32는 파이프 이름 그대로.
// 주의: 이 규칙은 JKPipeTransport_posix.cpp의 MapEndpointName이 유일한 진실
// 원본이다 — fold 규칙이 바뀌면 이곳의 결정론 결과 문자열을 같이 고쳐야 한다
// (파일 경로를 만드는 것이 아니라 fold 결과를 재서술한 상수일 뿐).
inline const char* DefaultServerEndpointPath() {
#ifdef _WIN32
    return kWindowServerPipeName;
#else
    return "/tmp/JKWindowServerPipe.sock";
#endif
}

} // namespace ipc
} // namespace jk

#endif // JK_WIRE_ENDPOINTS_H