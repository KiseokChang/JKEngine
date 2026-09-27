// Workshop 단 1 유닛 프로브 (docs/67 단 1). 빌드 레시피는 docs/61:708 선례 —
// 프로브 소유 temp dir만 쓴다(사용자 state/scripts 불접촉). 콘솔 프로브 —
// 창 서버 없음(jkedit_probe 선례).
//   (a) 스토어 단정 — 이름 검증·ListSlots 정렬·세대 사다리+20 프룬·영속 트림
//   (b) 위젯 스냅샷/복원 — 한글 포함 텍스트 동일성+입력모드+불일치 접두
//   (c) JS 훅 왕복 — onSaveState/onRestoreState+pending 소비
//   (d) WorkshopScriptApp 서브클래스 도구 루프 — 슬롯 CRUD·자동 전환·이력·복원
//   (e) 상태 동일성 e2e (docs/67 §6 필수 게이트) — 라벨만 바꾼 set_script 후
//       에디트 텍스트 생존 단정 + 슬롯 왕복 생존
#include <JKWindow.h>
#include <apps/ClientScriptApp.h>
#include <script/JKWorkshopStore.h>

#ifdef main
#undef main  // SDL.h defines main to SDL_main; we use plain main()
#endif

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

using namespace jk;

static int g_fail = 0;
static void Check(const char* name, bool ok, const std::string& detail = {}) {
    if (ok) std::printf("PASS: %s\n", name);
    else {
        ++g_fail;
        std::printf("FAIL: %s -- %s\n", name, detail.c_str());
    }
}

namespace fs = std::filesystem;

static void WriteFile(const fs::path& p, const std::string& data) {
    std::FILE* f = nullptr;
    fopen_s(&f, p.string().c_str(), "wb");
    if (f) {
        std::fwrite(data.data(), 1, data.size(), f);
        std::fclose(f);
    }
}

static std::string ReadFile(const fs::path& p) {
    std::FILE* f = nullptr;
    fopen_s(&f, p.string().c_str(), "rb");
    if (!f) return {};
    std::string out;
    char buf[4096];
    size_t n = 0;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) out.append(buf, n);
    std::fclose(f);
    return out;
}

// 스크립트 원문 → JSON 문자열 리터럴 이스케이프(도구 args 조립용).
static std::string JsonEscHelper(const std::string& s) {
    std::string r;
    for (char c : s) {
        switch (c) {
        case '"': r += "\\\""; break;
        case '\\': r += "\\\\"; break;
        case '\n': r += "\\n"; break;
        default: r += c;
        }
    }
    return r;
}

// 스냅샷 JSON에서 편집창 i번째의 "m" 값을 바꾼 복제본.
static std::string EditModeTo(const std::string& snap, size_t nth,
                              int mode) {
    // nth째의 "m":N 위치를 찾는다 — "t"와 "m"은 항상 이 순서로 한 쌍이다.
    size_t pos = 0;
    for (size_t k = 0; k <= nth; ++k) {
        pos = snap.find("\"m\":", pos + (k == 0 ? 0 : 1));
        if (pos == std::string::npos) return snap;
    }
    std::string out = snap;
    const size_t digit = pos + 4;  // "m": 다음 한 글자
    out[digit] = static_cast<char>('0' + mode);
    return out;
}

// --- (a) 스토어 --------------------------------------------------------------

static void TestStore(const fs::path& dir) {
    namespace w = jk::workshop;

    Check("a1-name-ok", w::IsValidSlotName("myapp") &&
                            w::IsValidSlotName("second-slot") &&
                            w::IsValidSlotName("a-B_9"));
    Check("a1-name-bad", !w::IsValidSlotName("") &&
                             !w::IsValidSlotName("bad.name") &&
                             !w::IsValidSlotName("..") &&
                             !w::IsValidSlotName("has space") &&
                             !w::IsValidSlotName("한글") &&
                             !w::IsValidSlotName(std::string(33, 'a')));

    fs::create_directories(dir);
    WriteFile(dir / "b.js", "// b\n");
    WriteFile(dir / "a.js", "// a\n");
    WriteFile(dir / "myapp.js", "// myapp\n");
    WriteFile(dir / "bad.name.js", "// bad\n");
    WriteFile(dir / ".current_workshop", "myapp");
    fs::create_directories(dir / ".history" / "myapp");

    std::vector<std::string> slots;
    const bool listed = w::ListSlots(dir.string(), slots);
    Check("a2-listslots", listed && slots.size() == 3 && slots[0] == "a" &&
                              slots[1] == "b" && slots[2] == "myapp",
          "n=" + std::to_string(slots.size()));
    Check("a3-listslots-dots-invisible",
          std::find(slots.begin(), slots.end(), "bad.name") == slots.end());

    // 세대 사다리: prev 원문 보관 + gen 승순.
    const int g1 = w::AppendSnapshot(dir.string(), "myapp", "gen-one");
    const int g2 = w::AppendSnapshot(dir.string(), "myapp", "gen-two");
    Check("a4-gens-ascending", g1 == 1 && g2 == 2,
          "g1=" + std::to_string(g1) + " g2=" + std::to_string(g2));
    std::string gen1;
    Check("a5-readgen-roundtrip",
          w::ReadGen(dir.string(), "myapp", 1, gen1) && gen1 == "gen-one");
    Check("a5b-readgen-missing",
          !w::ReadGen(dir.string(), "myapp", 9, gen1));
    Check("a6-first-write-no-snapshot",
          w::AppendSnapshot(dir.string(), "ghost", "") == 0);

    // 20세대 캡 — a4까지 2세대 + 22회 = 24 → 최오소 4세대 프룬, 5..24 유지.
    for (int i = 0; i < 22; ++i)
        w::AppendSnapshot(dir.string(), "myapp", "cap-" + std::to_string(i));
    std::vector<w::HistoryEntry> gens;
    w::ListHistory(dir.string(), "myapp", gens);
    Check("a7-cap-20", gens.size() == 20 && gens[0].gen == 5 &&
                           gens.back().gen == 24,
          "n=" + std::to_string(gens.size()) + " first=" +
              std::to_string(gens.empty() ? 0 : gens[0].gen));

    // 영속 파일 — 스토어 함수로 두 번 쓴다(덮어쓰기). UCRT rename은 대상
    // 존재 시 실패(2026-09-26 라이브 게이트 c7 실측) — WriteFileAll의
    // remove+rename이 이 경로를 지휘한다. 세 번째 쓰기는 메모장 트림 형태.
    std::string cur;
    Check("a8-current-overwrite",
          w::WriteCurrentSlotFile(dir.string(), "workshop", "second-slot") &&
              w::WriteCurrentSlotFile(dir.string(), "workshop", "myapp") &&
              w::ReadCurrentSlotFile(dir.string(), "workshop", cur) &&
              cur == "myapp");
    WriteFile(dir / ".current_workshop", "  second-slot \r\n");
    Check("a8b-current-trim",
          w::ReadCurrentSlotFile(dir.string(), "workshop", cur) &&
              cur == "second-slot");

    Check("a9-historydir",
          w::HistoryDir(dir.string(), "myapp") ==
              (dir / ".history" / "myapp").string());
    Check("a9b-dirof", w::DirOf("a\\b\\c.js") == "a\\b" &&
                           w::DirOf("plain.js") == ".");
}

// --- (b)(c) 호스트 스냅샷·복원·훅 ---------------------------------------------

static void TestHostState(const fs::path& dir) {
    const std::string scriptA =
        "var a = createEdit({x:0,y:0,w:120,h:20}, '한글A');\n"
        "var b = createEdit({x:0,y:30,w:120,h:20}, 'BB');\n"
        "setText(b, '한글B');\n";
    WriteFile(dir / "state_a.js", scriptA);

    JKWindow win("probe");
    JKScriptHost host;
    host.Attach(&win);
    JKScriptTimerServices noop;
    noop.start = [](uint32_t, uint32_t) -> uint64_t { return 1; };
    noop.stop = [](uint64_t) {};
    host.SetTimerServices(noop);

    Check("b1-start", host.Start((dir / "state_a.js").string()),
          host.LastError());
    std::string snap;
    Check("b2-capture", host.CaptureWidgetState(snap));
    Check("b2b-capture-shape",
          snap.find("\"v\":1") != std::string::npos &&
              snap.find("edits") != std::string::npos &&
              snap.find("\"t\":\"") != std::string::npos,
          snap);
    // UTF-8 경계: 위젯 저장 텍스트는 KSSM, 스냅샷은 UTF-8(한글 그대로).
    Check("b2c-capture-korean",
          snap.find("한글A") != std::string::npos &&
              snap.find("한글B") != std::string::npos,
          snap);
    Check("b2d-capture-order",
          snap.find("한글A") < snap.find("한글B"));

    // 입력모드: 첫 편집창의 m을 1(InternalHangul)로 바꾼 스냅샷 → 복원 →
    // 재캡처로 m==1 단정(입력모드가 스냅샷을 타고 살아남는다).
    const std::string modeSnap = EditModeTo(snap, 0, 1);
    Check("b3-mode-restore", host.RestoreWidgetState(modeSnap));
    std::string snap2;
    Check("b3b-mode-roundtrip", host.CaptureWidgetState(snap2) &&
                                    snap2.find("\"m\":1") != std::string::npos,
          snap2);
    // 복원이 텍스트를 건드리지 않는다(같은 스냅샷의 텍스트 부분은 유지).
    Check("b3c-mode-restore-keeps-text",
          snap2.find("한글A") != std::string::npos &&
              snap2.find("한글B") != std::string::npos);

    // 불일치 접두: 1편집창짜리 새 스크립트에 2편집창 스냅샷 — j번째 편집창이
    // edits[j]를 받는다(생성순서 접두 매칭 — 컨트롤 id는 Start마다 리셋).
    const std::string scriptB =
        "var c = createEdit({x:0,y:0,w:120,h:20}, '새것');\n";
    WriteFile(dir / "state_b.js", scriptB);
    host.Stop();
    Check("b4-restart-1-edit", host.Start((dir / "state_b.js").string()));
    Check("b4b-prefix-restore", host.RestoreWidgetState(snap));
    std::string snap3;
    Check("b4c-prefix-matched-first",
          host.CaptureWidgetState(snap3) &&
              snap3.find("한글A") != std::string::npos,  // edits[0]가 0번째로
          snap3);

    // (c) JS 훅 왕복.
    const std::string scriptC =
        "var e = createEdit({x:0,y:0,w:120,h:20}, '');\n"
        "function onSaveState(){ return \"saved42\"; }\n"
        "function onRestoreState(s){ setText(e, \"got:\" + s); }\n";
    WriteFile(dir / "state_c.js", scriptC);
    host.Stop();
    Check("c1-start-hooks", host.Start((dir / "state_c.js").string()));
    std::string jsState;
    Check("c2-savestate", host.DispatchSaveState(jsState) && jsState == "saved42", jsState);
    // pending 소비: Reload → Start 후반(onCreate 직후) onRestoreState가
    // 편집창에 기록한다 — 위젯 캡처로 간접 단정.
    host.SetPendingRestoreState(jsState);
    Check("c3-reload", host.Reload());
    std::string after;
    Check("c4-pending-consumed", host.CaptureWidgetState(after) &&
                                     after.find("got:saved42") !=
                                         std::string::npos,
          after);
    // pending은 소비 후 클리어 — 또 리로드하면 onRestoreState가 무해 통과.
    Check("c5-reload-again", host.Reload());
    std::string after2;
    Check("c5b-pending-cleared", host.CaptureWidgetState(after2) &&
                                     after2.find("got:") ==
                                         std::string::npos,
          after2);

    // 훅 부재 — 조용한 무상태(빈 결과).
    const std::string scriptD =
        "createEdit({x:0,y:0,w:120,h:20}, 'nohook');\n";
    WriteFile(dir / "state_d.js", scriptD);
    host.Stop();
    Check("c6-start-nohook", host.Start((dir / "state_d.js").string()));
    std::string none;
    Check("c7-no-hook-no-state", !host.DispatchSaveState(none) && none.empty());
    Check("c8-restorestate-absent-safe", host.DispatchRestoreState("{}"));
}

// --- (d)(e) 워크숍 앱 도구 루프 + 상태 동일성 ---------------------------------

class ProbeWorkshopApp : public WorkshopScriptApp {
public:
    bool Call(const std::string& tool, const std::string& args,
              std::string& out) {
        return OnAgentToolCall(tool, args, out);
    }
    JKScriptHost* Host() { return host_.get(); }
    const std::string& Path() const { return scriptPath_; }
};

static void TestAppTools(const fs::path& dir) {
    const std::string v1 =
        "var e = createEdit({x:0,y:0,w:120,h:20}, '라이브상태');\n"
        "var l = createLabel({x:0,y:40,w:200,h:20}, 'v1라벨');\n";
    const std::string v2 =
        "var e = createEdit({x:0,y:0,w:120,h:20}, '라이브상태');\n"
        "var l = createLabel({x:0,y:40,w:200,h:20}, 'v2라벨');\n";
    const std::string v2s =
        "createEdit({x:0,y:0,w:120,h:20}, '두번째슬롯');\n";

    ProbeWorkshopApp app;
    app.SetScriptInfo("probe-workshop", (dir / "myapp.js").string());
    auto win = std::make_unique<JKWindow>("probe");
    win->SetWindowRect(JKRect{ 0, 0, 320, 240 });
    app.SetMainWindow(std::move(win));

    std::string out;
    bool ok = app.Call("set_script", "{\"source\":\"" + JsonEscHelper(v1) +
                                       "\"}", out);
    Check("d1-set1", ok && out.find("\"ok\":true") != std::string::npos &&
                         out.find("\"gen\":0") != std::string::npos,
          out);

    // (e) 준비: 리로드 전 위젯 캡처.
    std::string before;
    Check("e1-capture-before", app.Host()->CaptureWidgetState(before), before);

    ok = app.Call("set_script", "{\"source\":\"" + JsonEscHelper(v2) +
                                    "\"}", out);
    Check("d2-set2-gen1", ok && out.find("\"gen\":1") != std::string::npos, out);

    // (e) 상태 동일성 게이트 (docs/67 §6): 같은 편집창이 살아남은 set_script
    // 후 에디트 텍스트 생존 — 라벨은 새 스크립트가 다시 그린다(복원 금지).
    std::string after;
    Check("e2-edit-survives-reload", app.Host()->CaptureWidgetState(after) &&
                                         after.find("라이브상태") !=
                                             std::string::npos,
          after);
    Check("e3-label-not-restored",
          after.find("v2라벨") == std::string::npos, after);

    // 다른 슬롯 쓰기 + 자동 전환.
    ok = app.Call("set_script",
                  "{\"source\":\"" + JsonEscHelper(v2s) +
                      "\",\"slot\":\"second\"}", out);
    Check("d3-set-slot-autoswitch",
          ok && out.find("\"slot\":\"second\"") != std::string::npos, out);
    ok = app.Call("list_slots", "{}", out);
    Check("d4-list-current-second",
          ok && out.find("\"current\":\"second\"") != std::string::npos, out);
    ok = app.Call("get_script", "{}", out);
    Check("d4b-get-new-slot-source",
          ok && out.find("두번째슬롯") != std::string::npos, out);

    // 복귀 — stateByPath_가 무료로 보존(위젯 캡처로 단정).
    ok = app.Call("use_slot", "{\"slot\":\"myapp\"}", out);
    Check("d5-use-slot-back", ok, out);
    std::string back;
    Check("e4-slot-roundtrip-survives",
          app.Host()->CaptureWidgetState(back) &&
              back.find("라이브상태") != std::string::npos,
          back);

    // 이력 — 승순.
    ok = app.Call("script_history", "{}", out);
    Check("d6-history-ascending",
          ok && out.find("\"gen\":1") != std::string::npos, out);

    // 복원 — 직전 원문이 새 세대(snapshotGen)가 된다.
    ok = app.Call("restore_script", "{\"gen\":1}", out);
    Check("d7-restore-ok",
          ok && out.find("\"snapshotGen\":") != std::string::npos, out);
    ok = app.Call("get_script", "{}", out);
    Check("d7b-restored-source", ok && out.find("v1라벨") != std::string::npos,
          out);

    // 오류 경로 — 폐곡선 규약(도구 응답으로 표면화).
    ok = app.Call("set_script",
                  "{\"source\":\"x\",\"slot\":\"bad.name\"}", out);
    Check("d8-bad-slot", !ok && out.find("bad_slot") != std::string::npos, out);
    ok = app.Call("use_slot", "{\"slot\":\"ghost\"}", out);
    Check("d8b-no-such-slot", !ok && out.find("no_such_slot") != std::string::npos,
          out);
    ok = app.Call("restore_script", "{\"gen\":99}", out);
    Check("d8c-no-such-gen", !ok && out.find("no_such_gen") != std::string::npos,
          out);
    ok = app.Call("restore_script", "{}", out);
    Check("d8d-bad-args", !ok && out.find("bad_args") != std::string::npos, out);
}

// --- (f) 스트립 z-order 게이트 (2026-09-26 라이브 결함 — "슬롯 선택 먹통") ----
// children_ 순서가 곧 z-order(페인트·HitTest 모두 후순 우선) — HitTest의
// 스크린 좌표 매핑은 헤드리스 프로브가 신뢰할 수 없어 순서를 직접 단정한다.

static void TestZOrder() {
    JKWindow win("probe");
    win.SetWindowRect(JKRect{ 0, 0, 320, 240 });
    auto* a = new JKStatic(JKRect{ 0, 0, 50, 20 }, 0);    // 스트립 콤보 자리
    win.AddControl(std::unique_ptr<JKControl>(a));
    auto* c = new JKStatic(JKRect{ 0, 0, 320, 240 }, 0);  // 리로드 패널(DOCK_FILL 자리)
    win.AddControl(std::unique_ptr<JKControl>(c));
    // 패널이 나중에 얹혀 스트립이 가려진 상태 — 라이브 증상의 구조.
    Check("f1-covered", win.GetChildren().back().get() == c);
    win.MoveChildToTop(a);
    Check("f2-raised", win.GetChildren().back().get() == a);
    Check("f3-still-own", a->GetParent() == &win &&
                              win.GetChildren().size() == 2);
    // 이미 맨 뒤(위)면 no-op — 소유권 이동 없이 안전.
    win.MoveChildToTop(a);
    Check("f4-noop", win.GetChildren().back().get() == a &&
                         win.GetChildren().size() == 2);
}

// --- (라이브 패치 — 컨텍스트 생존 정의 재평가, 스펙 §6 게이트 ①) ---
// 생존 단위: 위젯(native)·글로벌 프로퍼티·의미 커서 선언. 교체: top-level
// function 정의. 차단: 패치 평가 중 생성류(bad_patch). 미호출: onCreate/onExit.
// 텍스트 판독은 CaptureWidgetState(편집창 텍스트만 수록 — 라벨·버튼은 스크립트
// 소유 파생 출력)로 한다 — 기존 b2/b2b 체크와 같은 판독 경로.
static void TestLivePatch(const fs::path& dir) {
    JKWindow win("livepatch-probe");
    JKScriptHost host;
    host.Attach(&win);
    JKScriptTimerServices noop;
    noop.start = [](uint32_t, uint32_t) -> uint64_t { return 1; };
    noop.stop = [](uint64_t) {};
    host.SetTimerServices(noop);

    // S를 어휘(let/const) 바인딩으로 선언해야 f16의 const 재선언이
    // SyntaxError가 된다 — globalThis.S(설정 가능 프로퍼티)면 const 재선언이
    // ES 규약상 합법이라 성공해 버린다(2026-09-27 유닛 실측).
    WriteFile(dir / "live_a.js",
        "const S = {count: 1};\n"
        "var btn = createButton({x:0,y:0,w:120,h:24}, 'v1');\n"
        "var ed = createEdit({x:0,y:30,w:160,h:22}, '');\n"
        "function onClick(id){ S.count = S.count + 1; setText(ed, 'hit' + S.count); }\n");
    Check("f1-start", host.Start((dir / "live_a.js").string()),
          host.LastError());

    // 구 정의 클릭 — 상태 시드: count 1→2, 편집창 "hit2".
    // (controls_는 private — 창 children이 스크립트 생성 컨트롤과 1:1이다:
    // 이 창은 빈 채로 시작하므로 children이 곧 카운터. TestZOrder 접근 경로.)
    const uint16_t btnId = win.GetChildren().front()->GetControlId();
    host.DispatchClick(btnId);
    std::string snap;
    Check("f2-seed-click", host.CaptureWidgetState(snap) &&
                               snap.find("hit2") != std::string::npos, snap);

    // 컴파일 게이트: 통과는 무부작용, 실패는 컨텍스트 무손상(스펙 §4 흐름 1).
    // 베이스라인 위젯 = 버튼+편집창 = 2 — 게이트가 위젯을 만들지 않는다.
    Check("f3-gate-ok", host.CompileGate(
        "function onClick(id){ S.count = S.count + 10; "
        "setText(ed, 'patched' + S.count); }\n"));
    Check("f4-gate-noop", host.IsRunning() && win.GetChildren().size() == 2);
    Check("f5-gate-syntax-fail", !host.CompileGate(
        "function onClick(id { setText(id, 'x'); }\n"),
        host.LastError());
    Check("f6-gate-fail-intact", host.IsRunning() &&
                                     win.GetChildren().size() == 2);

    // 재평가 성공 — 위젯 수 불변, 새 정의가 전역 조회로 해석된다.
    Check("f7-patch-ok", host.PatchEval(
        "function onClick(id){ S.count = S.count + 10; "
        "setText(ed, 'patched' + S.count); }\n"), host.LastError());
    Check("f8-patch-alive", host.IsRunning() && win.GetChildren().size() == 2);
    host.DispatchClick(btnId);
    std::string snap2;
    Check("f9-state-survives", host.CaptureWidgetState(snap2) &&
                                   snap2.find("patched12") != std::string::npos,
          snap2);  // 구 상태(2) 생존 + 새 정의(+10)

    // 재평가 중 생성류 차단 — bad_patch 에러가 규약을 교육한다(폐곡선).
    Check("f10-patch-create-blocked", !host.PatchEval(
        "createButton({x:0,y:60,w:20,h:20}, 'dup');\n"), host.LastError());
    Check("f11-block-error-text",
          host.LastError().find("live patch") != std::string::npos,
          host.LastError());
    Check("f12-no-dup-widget", win.GetChildren().size() == 2);

    // 패치 중 문법 실패 — 컨텍스트 무손상, 구 정의가 그대로 바인딩.
    Check("f13-patch-syntax-fail", !host.PatchEval(
        "function onClick(id { setText(id, 'x'); }\n"), host.LastError());
    Check("f14-syntax-fail-alive", host.IsRunning());
    host.DispatchClick(btnId);
    std::string snap3;
    // 구 정의(=f7 패치의 +10 정의)가 그대로 바인딩 — 실패 패치가 정의를
    // 갈아치웠다면 'patched' 접두가 아닌 'x'가 찍힌다. 12+10=22.
    Check("f15-old-def-still-bound", host.CaptureWidgetState(snap3) &&
                                         snap3.find("patched22") !=
                                             std::string::npos, snap3);

    // top-level const 재선언 — 재평가 시 SyntaxError → 호출자가 낙하 판단.
    // 호스트는 죽지 않고(스펙 §4 유수정) 구 정의가 살아 남는다.
    Check("f16-const-redecl-fails", !host.PatchEval("const S = {count: 0};\n"),
          host.LastError());
    Check("f17-const-fail-alive", host.IsRunning());
    host.DispatchClick(btnId);
    std::string snap4;
    // S 무손상(22) + 구 정의(+10) → 32.
    Check("f18-old-def-after-const", host.CaptureWidgetState(snap4) &&
                                         snap4.find("patched32") !=
                                             std::string::npos, snap4);

    // 커서 선언 보존 — 패치는 cursorDeclJson_을 클리어하지 않는다(스펙 §3).
    WriteFile(dir / "live_cursor.js",
        "declareCursor({origin:{x:10,y:10}, cellW:30, cellH:25, rows:8, "
        "cols:10, kinds:['paint']});\n"
        "function onAgentAct(kind,row,col){ return '{\\\"ok\\\":true}'; }\n");
    Check("f19-cursor-start", host.Start((dir / "live_cursor.js").string()),
          host.LastError());
    const std::string decl0 = host.DeclaredCursorJson();
    Check("f20-decl-seeded", !decl0.empty(), decl0);
    Check("f21-patch-act-fn", host.PatchEval(
        "function onAgentAct(kind,row,col){ "
        "return '{\\\"ok\\\":true,\\\"patched\\\":true}'; }\n"),
        host.LastError());
    Check("f22-decl-preserved", host.DeclaredCursorJson() == decl0,
          host.DeclaredCursorJson());
    Check("f23-patch-redeclare", host.PatchEval(
        "declareCursor({origin:{x:10,y:10}, cellW:30, cellH:25, rows:8, "
        "cols:10, kinds:['paint','chord']});\n"), host.LastError());
    Check("f24-decl-updated",
          host.DeclaredCursorJson().find("chord") != std::string::npos,
          host.DeclaredCursorJson());

    // 풀 리로드 회귀 — Stop+Start 경로는 무수정(라이브 경로가 옆길일 뿐).
    // 리로드 후 커서 선언은 파일의 것으로 리셋 — 패치로 추가한 'chord'는
    // Start마다 리셋 계약(:1134)에 따라 소멸한다.
    Check("f25-full-reload-works", host.Reload());
    Check("f26-reload-fresh-cursor",
          host.DeclaredCursorJson().find("paint") != std::string::npos &&
              host.DeclaredCursorJson().find("chord") == std::string::npos,
          host.DeclaredCursorJson());
}

int main() {
    const fs::path base = fs::temp_directory_path() / "jk_workshop_slot_probe";
    fs::remove_all(base);
    const fs::path storeDir = base / "store";
    const fs::path hostDir = base / "host";
    const fs::path appDir = base / "app";
    try {
        TestStore(storeDir);
        fs::create_directories(hostDir);
        TestHostState(hostDir);
        {
            const fs::path lp = hostDir / "livepatch";
            fs::create_directories(lp);
            TestLivePatch(lp);
        }
        fs::create_directories(appDir);
        TestAppTools(appDir);
        TestZOrder();
    } catch (const std::exception& e) {
        std::printf("FAIL: exception -- %s\n", e.what());
        ++g_fail;
    }
    fs::remove_all(base);
    std::printf("RESULT: %s (%d fail)\n", g_fail ? "FAIL" : "ALL PASS",
                g_fail);
    return g_fail ? 1 : 0;
}