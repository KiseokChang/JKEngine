// config.js — v4 readConfig (docs/27 단계 4). The runner starts the scenario
// from scripts/tests/, so "config.json" resolves next to this file. The
// terminal's terminal.json consumes the same semantics through its own C++
// reader (self-test block 10).
var cfg = readConfig("config.json");

function onCreate() {
    assert(cfg !== null && typeof cfg === "object", "readConfig returns the parsed object");
    assertEq(cfg.shell, "cmd.exe /k echo configured", "string key read");
    assertEq(cfg.scrollback, 2500, "number key read");
    assertEq(cfg.themeBg, "#112233", "#RRGGBB color key read");
    assertEq(cfg.themeFg, 16766976, "numeric color key read");

    // Failure semantics: null + a log line — the script supplies defaults.
    assert(readConfig("does_not_exist.json") === null, "missing file returns null");
    assert(readConfig("../jk.d.ts") === null, "path traversal rejected");
    assert(readConfig("C:\\Windows\\win.ini") === null, "absolute path rejected");
}