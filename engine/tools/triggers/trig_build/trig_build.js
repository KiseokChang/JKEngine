// trig_build — 빌드 실패 감지 (docs/32): terminal.output 스트림에서 MSVC/GCC
// 에러 패턴을 찾아 agent.notify 방송. 청크 단위로 들어오므로 60초 디바운스로
// 한 번의 실패가 여러 알림으로 증폭되지 않게 한다.
var lastNotify = 0;
on("terminal.output", { match: /error C\d+|fatal error|error:/i }, function (e) {
  var now = Date.now();
  if (now - lastNotify < 60000) return;   // debounce 60s
  lastNotify = now;
  var snippet = (e.text || "").slice(0, 200);
  desktop.notify("빌드 실패 감지", snippet);
  desktop.log("trigger: build fail -> notify");
});
