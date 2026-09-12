// rate_probe — docs/38 rate-limit acceptance bundle (docs/32 Task 3 probe).
// Phases are driven by the probe publishing kick events (probe_agent_
// ratelimit.ps1). Caps are consume-on-allow on both sides:
//   - server connection cap: 60 publishes/10s (dropped events are answered
//     {"ok":true,"dropped":1} and never reach the bus / events_list stats)
//   - local handler cap: 60 fires/60s per source (= container name, shared
//     by every handler in this bundle) — the LOCAL cap binds first for the
//     self-loop, which is the deterministic notify/stop point.
// 1. kick_burst -> 70 publishes of ratelimit.server: 60 broadcast + 10 drops.
// 2. kick_loop  -> self-feeding ratelimit.ping loop, stops at the local cap
//    (one "rate limit" log + one notify per window).
// 3. kick_other -> isolation: fires in a fresh budget window.
// 4. kick_timers-> timer cap: 64 slots (one held by trig_idle's interval)
//    -> 7 of the 70 timers are dropped.
on("ratelimit.kick_burst", null, function (e) {
  for (var i = 0; i < 70; i++) desktop.publish("ratelimit.server", { n: i });
  desktop.log("burst done");
});
on("ratelimit.kick_loop", null, function (e) {
  desktop.publish("ratelimit.ping", { n: 0 });
});
on("ratelimit.ping", null, function (e) {
  var n = (e.data && e.data.n) || 0;
  desktop.log("ping:" + n);
  desktop.publish("ratelimit.ping", { n: n + 1 });
});
on("ratelimit.other", null, function (e) { desktop.log("other fired"); });
on("ratelimit.kick_timers", null, function (e) {
  for (var i = 0; i < 70; i++) setTimeout(function () {}, 1000);
});