// trig_crash — 앱 비정상 종료 배지 (docs/32). 서버가 CleanupDisconnectedClients에서
// 감지한 app.crashed를 받아 agent.notify로 방송한다. 봉투의 title/pid는 최상위
// 필드 (PushAgentEvent 규약).
on("app.crashed", {}, function (e) {
  desktop.notify("앱 비정상 종료", (e.title || "?") + " (pid " + e.pid + ")");
});
