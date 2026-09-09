// trig_idle — 장시간 미사용 시 레이아웃 자동 저장 (docs/32). 활동 이벤트
// (window.focused|created|destroyed)가 마지막으로 온 지점부터 idle 시간을
// 재고, 임계값을 넘으면 한 번만 저장한다. 임계값은 state/idle_minutes 파일
// (exeDir 기준)로 오버라이드 — 프로브가 0으로 세팅해 즉시 발화를 검증한다.
var thresholdMin = 30;
try {
  var v = parseInt(desktop.readFile("state/idle_minutes"), 10);
  if (!isNaN(v)) thresholdMin = v;   // 0도 유효한 임계값 (프로브 즉시 발화)
} catch (e) {}   // 파일 없음 = 기본값
var lastActivity = Date.now(), saved = false;
on("window.focused", {}, function () { lastActivity = Date.now(); saved = false; });
on("window.created", {}, function () { lastActivity = Date.now(); saved = false; });
on("window.destroyed", {}, function () { lastActivity = Date.now(); saved = false; });
setInterval(function () {
  var idleMs = Date.now() - lastActivity;
  if (!saved && idleMs >= thresholdMin * 60000) {
    saved = true;
    desktop.saveLayout("auto_idle");
    desktop.notify("자동 저장", "idle " + thresholdMin + "분 — 레이아웃 auto_idle 저장");
  }
}, 10000);
