# Residue Batch I Implementation Plan (I2 MANI 보존 + I3 O6 스크롤백 + I4 소박 6건)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development
> (recommended) or superpowers:executing-plans to implement this plan task-by-task.
> Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 워크숍 단 2 진행을 가로막은 소액 잔여와 수용-상태 결함(O6)을 전부
소각한다 — MANI 필드 보존(docs/67 단 2 룰링), 터미널 스크롤백 선택+클래식
휠 신고, 6건의 소박 결함 배치.

**Architecture:** I2는 순수 병합 함수(공용화→selftest에서 직접 검증) +
RunJkxPack가 그 함수를 쓰도록. I3는 선택 좌표를 "전체 행 공간"(스크롤백+화면
합산)으로 들어올려 ExtractSelectedText의 generic CellFn로 흡수 + 휠의
클래식(비SGR) 신고 신설. I4는 파일별 독립 소박 수리.

**Tech Stack:** C++17 / SDL2 / MinGW(ninja) / 기존 selftest(main.cpp "test"
모드).

**Spec:** docs/60_workshop.md §6 (MANI 재생성 결함), docs/67_workshop_vision.md
단 2 룰링("화이트리스트 설계 → 필드 보존"), docs/65_jkedit_defect_ledger.md
O6 수용 상태(잔여=선택+휠), 세션 소박 원장(I4 6건).

## Global Constraints

- 원칙(docs/67 단 2 룰링): MANI는 **필드 보존** — 화이트리스트 재생성 금지.
- permissions.json 전면 allow 런타임 파일은 변경 금지(프로브 소유 아님).
- posix selftest 케이스 다음 빈 자리 = 14(문서 원장 docs/73 §4) — 이 플랜은
  posix 자체 변경이 없으므로 케이스 추가 없음.
- 커밋 메시지: 한글 요지+실측 라인, `Co-Authored-By: Claude Code
  <noreply@anthropic.com>` 트레일러.
- push는 컨트롤러가 최종 리뷰 뒤 직접(사전 승인됨).
- 라이브 스택(jkdesktop --server+jkbridge) 링크 잠금 시 전체 빌드 전 정지→
  사후 복원(표준 절차, PowerShell Start-Process 2개).

---

### Task I2: jkx-pack MANI 필드 보존 (공용 병합 함수)

**Files:**
- Modify: `engine/include/JKJkxFile.h` (JkxManifest 옆에 선언) + `engine/src/JKJkxFile.cpp` (구현)
- Modify: `engine/src/main.cpp` RunJkxPack (스크립트 앱 분기 :696-714)
- Test: `engine/src/main.cpp` test 모드 jkx 셀프테스트 구역(:1594 부근 — 병합 함수
  가 직접 본다; main.cpp test는 이 TU라 static 필요 없이 자유 함수 호출 가능)

**Interfaces:**
- Produces: `namespace jk { std::string JkxManifestMerge(const std::string& authored, const std::string& regenerated); }` — 선언은 JKJkxFile.h.
- Consumes: 없음(신설). RunJkxPack와 셀프테스트가 소비.

**계약(정밀 규격):** authored 텍스트의 **행 순서·내용을 원문 보존**하되, regenerated에
키가 존재하는 authored 행은 regenerated의 첫 등장 값으로 치환(행 위치 유지).
authored에 없는 regenerated 키는 regenerated 순서대로 뒤에 추가. 주석(#)/빈
행은 그대로 통과. key 탐색은 `Trim`된 k=... 형태(파서와 동일 규칙:
첫 '=' 이전 trim). authored가 빈 문자열이면 regenerated를 그대로 돌려준다.
치환 후에도 authored 중복 키 행은 각각 치환(간단·멱등).

- [ ] **Step 1: 실패 케이스를 본다(셀프테스트 새 블록)** — main.cpp test 모드
  jkx 구역 뒤에 추가:

```cpp
// I2 — JkxManifestMerge: 필드 보존 (docs/67 단 2 룰링 — 재생성 화이트리스트
// 폐기를 필드 보존으로). 슬롯 authored manifest의 scriptfile=/watch=/미래
// capabilities= 가 재팩에 살아남는 게 이 함수의 유일 존재 이유.
{
    const std::string authored =
        "name=slot1\n"
        "title=슬롯1\n"
        "width=320\n"
        "height=240\n"
        "module=jkapp_script.dll\n"
        "scriptfile=../slots/slot1/app.js\n"
        "watch=1\n"
        "capabilities=agent,timer\n"
        "\n"            // 빈 행 통과
        "# comment\n";  // 주석 통과
    const std::string regenerated =
        "name=slot1\n"
        "title=slot1\n"
        "width=320\n"
        "height=240\n"
        "module=jkapp_script.dll\n"
        "script=app.js\n";
    const std::string merged = jk::JkxManifestMerge(authored, regenerated);
    check(merged.find("scriptfile=../slots/slot1/app.js\n") != std::string::npos,
          "JkxManifestMerge preserves scriptfile verbatim");
    check(merged.find("watch=1\n") != std::string::npos,
          "JkxManifestMerge preserves watch verbatim");
    check(merged.find("capabilities=agent,timer\n") != std::string::npos,
          "JkxManifestMerge preserves unknown keys (future capabilities)");
    check(merged.find("# comment\n") != std::string::npos,
          "JkxManifestMerge passes through comments");
    check(merged.find("title=slot1\n") != std::string::npos &&
          merged.find("title=슬롯1\n") == std::string::npos,
          "canonical key replaced in place by regenerated value");
    check(merged.find("script=app.js\n") != std::string::npos,
          "authored-only missing regenerated key appended");
    // 빈 authored → regenerated 원문
    check(jk::JkxManifestMerge("", regenerated) == regenerated,
          "empty authored -> regenerated verbatim");
}
```

- [ ] **Step 2: 실행 — FAIL** (`./jkdesktop.exe test` 콘솔? test 모드 exe는
  build 루트의 `jkdesktop.exe test` — "JkxManifestMerge" 관련 컴파일 에러/실패 확인)
- [ ] **Step 3: 구현** — JKJkxFile.cpp 구현(Trim은 이미 파일 내 존재 :30):

```cpp
// I2 — 재생성 MANI에 authored 필드를 보존한다(docs/67 단 2 룰링: 화이트리스트
// 설계 폐기). 규칙: authored 행 순서 유지, regenerated에 키가 있는 행은 그
// 값으로 치환, authored에 없는 regenerated 키는 뒤에 추가, 주석/빈 행 통과.
// (레슨: 미래 필드 — capabilities= — 도 "unknown"이 아니라 보존 대상이다.)
std::string JkxManifestMerge(const std::string& authored,
                             const std::string& regenerated) {
    if (authored.empty()) return regenerated;
    // regenerated의 key → 첫 등장 "key=value" 행 전체 (치환 소스).
    std::vector<std::pair<std::string, std::string>> regEntries;  // 순서 유지
    {
        size_t pos = 0;
        while (pos < regenerated.size()) {
            size_t eol = regenerated.find('\n', pos);
            if (eol == std::string::npos) eol = regenerated.size();
            const std::string line = regenerated.substr(pos, eol - pos);
            pos = eol + 1;
            const std::string t = Trim(line);
            if (t.empty() || t[0] == '#') continue;
            const size_t eq = t.find('=');
            if (eq == std::string::npos) continue;
            const std::string key = Trim(t.substr(0, eq));
            regEntries.emplace_back(key, t);   // value가 아니라 치환 행 전체
        }
    }
    std::string out = authored;
    if (!out.empty() && out.back() != '\n') out += '\n';
    std::string appended;
    // regEntries 순회하며: authored에 키가 이미 있으면 "첫 그 행을 값 행으로
    // 치환"(마킹), 없으면 appended에 추가. 두 번째 reg 등장(불법)은 무시.
    for (const auto& re : regEntries) {
        size_t pos = 0;
        bool replaced = false;
        bool appendedDone = false;
        while (pos < out.size()) {
            size_t eol = out.find('\n', pos);
            if (eol == std::string::npos) eol = out.size();
            const std::string line = out.substr(pos, eol - pos);
            const std::string t = Trim(line);
            const size_t eq = t.find('=');
            if (!t.empty() && t[0] != '#' && eq != std::string::npos &&
                Trim(t.substr(0, eq)) == re.first) {
                if (!replaced) {
                    out.replace(pos, line.size(), re.second);
                    replaced = true;
                } else if (!appendedDone) {
                    // authored 중복 키: 후속 키의 파서 무시를 믿고 원문 보존
                    appendedDone = true;
                }
            }
            pos = eol + 1;
        }
        if (!replaced) appended += re.second + "\n";
    }
    return out + appended;
}
```

  검토 포인트: `out.replace` 후 pos 계산은 치환 길이와 무관하게 `eol + 1`(치환
  행 크기가 바뀌어도 eol은 원본 좌표이므로 치환 길이가 eol보다 클 수 없음 —
  reg 행은 항상 유효 MANI 행). 실제로 replace 이후 eol 오프셋이 어긋나면
  중복 키 행 위치가 틀어질 수 있음 — **구현자는 아닌 방식**: 위 루프를 그대로
  두되, 치환 후 `eol`을 재계산하지 않는 치환 길이<행+1 검사를 추가한다
  (`line.size() < re.second.size()`여도 다음 탐색 pos는 eol+1이므로 원본
  eol이 이미 지나친 지점 → 다음 dup 키를 놓칠 수 있음. 안전한 구현: 행을
  쪼개서 새로 조립하는 2-pass — authored 행을 벡터로 들고 각 행을 reg 키에
  대조, 치환은 행 문자열 교체, 마지막 join. 위 replace-루프는 구현자가
  판단해 2-pass 조립으로 교체할 수 있고 테스트만 통과하면 룰링에 따른다).
- [ ] **Step 4: 실행 — PASS** (test 모드 전체 0 failure(s))
- [ ] **Step 5: RunJkxPack 배선** — main.cpp:696-714 스크립트 앱 분기:

```cpp
// (기존 개별 += 서술을 regenerated 텍스트 조립으로 전환)
std::string regenerated;
regenerated += "name=" +
    (authored.name.empty() ? std::string(appName) : authored.name) + "\n";
regenerated += "title=" +
    (authored.title.empty() ? std::string(appName) : authored.title) + "\n";
regenerated += "width=" +
    std::to_string(authored.width > 0 ? authored.width : 320) + "\n";
regenerated += "height=" +
    std::to_string(authored.height > 0 ? authored.height : 240) + "\n";
regenerated += "module=jkapp_script.dll\n";
regenerated += "script=app.js\n";
// I2 필드 보존: authored 원문(scriptfile=/watch=/미래 키)을 살리고
// canonical 키만 갱신 — docs/67 단 2 룰링.
manifestText = jk::JkxManifestMerge(text, regenerated);
```

  (text는 :697의 authored 원문 string — 함수 확장 유지.) 네이티브 분기는
  원문 manifest 원본이 없는 경로 — 변경 없음(원장에 기록).
- [ ] **Step 6: 전체 test 0 failure(s) + ninja RC=0** 재확인
- [ ] **Step 7: Commit** `feat(jkx): MANI 필드 보존 — JkxManifestMerge 공용 함수+RunJkxPack 배선 (I2)`

### Task I3: O6 스크롤백 잔여 — 스크롤백 선택 + 비SGR 휠 신고

**Files:**
- Modify: `engine/include/apps/JKTermSelection.h` (전체 행 공간 2헬퍼 신설)
- Modify: `engine/src/apps/TerminalView.cpp` (선택 좌표 재공간화 :157-165·:518-539·:541 이하 선택 분기·CopySelection :896-909; HandleWheel :79-132 비SGR 분기)
- Modify: `engine/include/apps/JKTermInput.h` (EncodeWheelX10 신설 + EncodeMouseX10 motion 봉합 — I4-6과 같은 파일, 충돌 주의 선후 배경 참조)
- Test: `engine/src/main.cpp` test 모드 터미널 셀프테스트 구역(:1991-2105 — ViewportRowToLive/NormalizeSel/Sanitize 블록 뒤)

**Interfaces:**
- Produces(C): `inline int ViewportRowToFull(int r, int off, int hist);` — 뷰포트 행 → 전체 행(0=스크롤백 최상단, hist+j = 화살 위 살아있는 j행). 값 = hist - off + r. 클램프 없음(호출자가 NormalizeSelFull로 클램프).
- Produces(F): `inline JKTermSelRect NormalizeSelFull(int ax, int ay, int bx, int by, int cols, int hist, int rows);` — 행 클램프 히 = hist+rows-1, 열 클램프 동일 NormalizeSel.
- Produces(I4-6): `inline std::string EncodeWheelX10(bool up, int mods, int x, int y);` — "\x1b[M" + (32+64/65+mods) + (32+x) + (32+y), 좌표 clamp 1..223.
- Consumes: ExtractSelectedText의 generic CellFn(변경 없음 — 셀 접근자만 새로).

**계약:**
- 선택 좌표는 이제 **전체 행 공간**(scrollback hist + live rows). 오프셋 없을 때
  (off=0) 전체 행 = hist + live 행 ↔ 기존 live 선택과 동치 — 기존 검증 유지.
- 마우스 리포트(앱 통신)는 기존처럼 **뷰포트 좌표 그대로** — 선택 매핑과 분리
  (CellFromPoint는 리포트 경로가 계속 씀 — 시만트 유지).
- 복사는 ExtractSelectedText 그대로: 셀 접근자가 L < hist → ScrollbackLine(L) 셀
  (넘으면 cp==0 빈 셀), else grid_->Cell(col, L-hist).
- 휠 비SGR(MouseMode 1000/1002/1003 중 1006 협상 없음)은 클래식 인코딩
  \x1b[M + Cb=64(up)/65(down)+mods 바이트 1개 — SGR과 동일 notches(≤3)·
  좌표(lastMouse가 클라안에 있을 때) 게이트.

- [ ] **Step 1: 실패 테스트** — main.cpp :2105 Sanitize 블록 뒤:

```cpp
// I3 — 전체 행 공간 헬퍼 (O6 스크롤백 선택, docs/65 수용→잔여 소각).
{
    check(jk::ViewportRowToFull(0, 0, 100) == 100 &&
          jk::ViewportRowToFull(3, 0, 100) == 103,
          "ViewportRowToFull: offset 0 -> live rows shifted by hist");
    check(jk::ViewportRowToFull(2, 4, 100) == 98,
          "ViewportRowToFull: scrolled-back viewport row lands in scrollback");
    const JKTermSelRect s = jk::NormalizeSelFull(5, 96, 1, 90, 20, 100, 6);
    check(s.x0 == 1 && s.y0 == 90 && s.x1 == 5 && s.y1 == 96 &&
          !s.Empty() && s.Contains(3, 93),
          "NormalizeSelFull: orders+clamps into full line space");
    check(jk::NormalizeSelFull(0, 0, 99, 299, 20, 100, 6)
              .y1 == 105,
          "NormalizeSelFull: rows clamp to hist+rows-1 (105)");
    // 클래식 휠 (비SGR 신고 — acknowledged gap 소각)
    check(jk::EncodeWheelX10(true, 0, 10, 5) ==
              std::string("\x1b[M") + char(32 + 64) + char(32 + 10) + char(32 + 5),
          "EncodeWheelX10: up = Cb 64 classic bytes");
    check(jk::EncodeWheelX10(false, 16, 99, 250) ==
              std::string("\x1b[M") + char(32 + 65 + 16) + char(32 + 99) + char(32 + 223),
          "EncodeWheelX10: down+ctrl clamps x to 223");
}
```

- [ ] **Step 2: FAIL** 확인 (`jkdesktop.exe test` — EncodeWheelX10 미정의)
- [ ] **Step 3: 구현** — JKTermSelection.h 2헬퍼(ViewportRowToLive 옆에, 주석
  같은 원칙) + JKTermInput.h EncodeWheelX10.
- [ ] **Step 4: TerminalView 재공간화** —
  ① OnPaintClient :161-165: `NormalizeSel` → `NormalizeSelFull(anchor/end는
  전체 좌표 저장본, cols, hist, rows)` — anchor/end도 전체 공간일 테니
  :157-160 주석 갱신(spec §5 v1 restriction 해제 기록).
  ② 선택 앵커 지정 지점들(HandleMouseEvent의 MouseDown/Move 분기, :541 이하):
  뷰포트 px → cx,cy (기존 clamp 산식 그대로, CellFromPoint의 live-remap
  없이) → **cy를 전체 행으로**: `ay = std::min(hist - scrollOffset_ + cy, hist+rows-1)` 후 NormalizeSelFull에서 단일 클램프 — 앵커 저장은
  전체 좌표.
  ③ CopySelection :896-909: NormalizeSelFull + 셀 접근자:

```cpp
const JKTerminalGrid* g = grid_;
const int hb = hist;
const std::string text = ExtractSelectedText(
    [g, hb](int c, int r) -> const JKTermCell& {
        static const JKTermCell emptyCell{};   // 스냅샷이 짧으면 빈 셀
        if (r < hb) {
            const auto& line = g->ScrollbackLine(r);
            return (c >= 0 && c < (int)line.cells.size())
                       ? line.cells[c] : emptyCell;
        }
        return g->Cell(c, r - hb);
    }, sel);
```

     static local thread-safety — static const JKTermCell는 C++11 init-safe지만
     터미널 창이 두 개면 공유되자만 read-only — 무해. 기피하려면 멤버 dummy
     셀도 가능(룰링 후 커밋).
  ④ 이전에 scrollOffset_=0으로 리셋하던 지점들(탕핑 :494/:500) — 선택이 전체
  공간이라 리셋 동작 변화 없음(선택 해제는 그대로 ClearSelection).
- [ ] **Step 5: HandleWheel 비SGR 분기** (:95 `if (parser_->MouseMode() != Off)` 블록 안 SGR 분기 뒤):

```cpp
if (!parser_->SgrMouse() && client.Contains(lastMouse_.x, lastMouse_.y)) {
    const JKPoint cell = CellFromPoint(lastMouse_.x, lastMouse_.y);
    const int notches = std::min(wheelY < 0 ? -wheelY : wheelY, 3);
    const SDL_Keymod mod = static_cast<SDL_Keymod>(option);
    const int mods =
        ((mod & KMOD_SHIFT) ? int(MouseMod::Shift) : 0) |
        ((mod & KMOD_ALT) ? int(MouseMod::Meta) : 0) |
        ((mod & KMOD_CTRL) ? int(MouseMod::Ctrl) : 0);
    std::string seq;
    for (int i = 0; i < notches; ++i)
        seq += EncodeWheelX10(wheelY > 0, mods, cell.x + 1, cell.y + 1);
    if (!seq.empty()) onInput_(seq.data(), seq.size());
}
return;  // (기존 유지 — 리포팅 ON은 로컬 스크롤 없음)
```

  :87-90의 "acknowledged gap" 주석 삭제(봉합 기록).
- [ ] **Step 6: 전체 test 0 failure(s) + ninja RC=0**
- [ ] **Step 7: Commit** `feat(terminal): O6 잔여 소각 — 스크롤백 선택+비SGR 휠 신고 (I3)`

### Task I4: 소박 6건 배치 (한 디스패처, 파일별 독립)

**Files / 변경 내역 (모두 확정 근거):**

1. **chromeCloseHover 소비** — `engine/src/JKWindow.cpp` 크롬 페인트 블록
   :313-327 + `engine/include/JKWindow.h` 멤버. `bool closeHover_ = false;`
   신설; RespondMessage MouseMove 처리부(:518-554)에서
   `GetCloseButtonRect().Contains(ev.x, ev.y)` 전이 감지 → 전이 시
   `AddDirtyRect(closeBtn가 screen 좌표 — GetScreenRect 기반 환산)`. 페인트:
   `closeHover_ ? t.chromeCloseHover : t.chromeButtonFace`로 FillRect(글리프
   X는 chromeButtonGlyph 유지 — Win11도 X는 유지). 컴포지터 오버레이
   (JKCompositor.cpp:326)은 서버 측 상태라 이 태스크 밖(원장 기록).
2. **vpt12 디버그 fprintf 소거** — `engine/src/apps/ClientVPlayerApp.cpp`
   :2399-2409 (`static osdDbgLast_` 스태틱+if 블록 전체) 제거. [vpt12]는
   프로브 소유 탐점 — 구조만 남기고 소각.
3. **2KB 스키마 병합 무음 보류 → 가시화** — `engine/src/server/JKWindowServer.cpp`
   :6675 `if (out.size() > 2*1024) return;` → 등록 한도(6816 schema_too_large
   2KB)에 맥락 대응 주석 + `std::fprintf(stderr, "[toolreg] kind merge skipped: merged schema %zu > 2048\n", out.size());`(무음 소각 — 원인 직접
   볼 수 있게). 한도 자체를 올리는 정책 변경은 원장 기록(스펙 docs/58 2KB 유지).
4. **WideToUtf8 공용화** — 현존 2복각: `engine/src/server/JKWindowServer.cpp`
   :8065-8082(#ifdef _WIN32 static) + `engine/tools/jkchat/main.cpp:45`.
   공용 홈: `JKTextConv` 계열 헤더 — 실측:
   `engine/include`에서 Utf8ToKssm 계열이 있는 헤더(구현자가 grep —
   `JKTextConv_win32.cpp`의 헤더)에 `#ifdef _WIN32 std::string
   WideToUtf8(const std::wstring&); #endif` 선언 + win32 TU에 구현 이사.
   두 복각 지점 삭제+호출. (와이드→UTF-8은 무손실 왕복 — 기존 주석 승계.)
5. **더블라인/셰이드 글리프** — `engine/src/apps/TerminalView.cpp` 글리프
   switch(:360-419 근처 — 0x252C/0x2534/0x253C/0x2588 존재). 신설 케이스:
   **0x2550 ═**(my-3·my+3 두 수평선) **0x2551 ║**(mx-3·mx+3), 모서리
   **0x2554 ╔ 0x2557 ╗ 0x255A ╚ 0x255D ╝**(팔 방향이며 각 팔은 이중선 —
   기존 싱글 코너 케이스의 두 배 버전), 티 **0x2560 ╠ 0x2563 ╣ 0x2566 ╦
   0x2569 ╩ 0x256C ╬**, 반쪽 블록 **0x2580 ▀**(상반 채움) **0x2584 ▄**
   **0x258C ▌** **0x2590 ▐**, 셰이드 **0x2591 ░**(2px 간격 도트 ~25%)
   **0x2592 ▒**(2px 체커 50%) **0x2593 ▓**(2px 체커 75% = ░ 보색).
   :413의 `placeholder` 주석에서 더블라인/셰이드는 제거해 남은 cp만 placeholder
   (0x2596-2599 분할 사분형 등). 자동화 테스트 없음 — 눈확인 표시.
6. **클래식 마우스 모션(Cb btn+32)** — `engine/include/apps/JKTermInput.h:68`
   EncodeMouseX10: Motion → `cb = std::clamp(btn, 0, 3) + 32;` + 바이트 출력
   :74의 `return out;` 제거(구현은 press/release와 동일 출력 경로).
   `engine/src/apps/TerminalView.cpp:680` `if (mode == Normal || !sgr) return;`
   → `if (mode == Normal) return;` — 1002는 기존 reportedButtons_ 게이트 유지,
   1003은 btn=3(호버) Cb 35. :691 주석+docs/41 §7 갭 기록 승계 갱신.
   **I3가 같은 파일을 건드리므로 I3가 먼저 커밋된 뒤 배치 디스패치**.

**Interfaces:**
- Produces: 없음(공용화 1건: `jk::WideToUtf8(const std::wstring&)` —
  JKTextConv 헤더(정확 경로는 구현이 grep로 확정해 기록), win32 전용).
- Consumes: Task I3의 EncodeMouseX10(동일 함수의 motion 분기 확장) —
  배치 내에서는 I3 커밋 이후 재빌드 기준으로 작업.

- [ ] **Step 1: 6건 각각 위 수거대로 수리(배치 구현, 항목 6은 I3 커밋 기반)**
- [ ] **Step 2: ninja RC=0 + test 0 failure(s) + app_tools ALL PASS 게이트**
- [ ] **Step 3: Commit** `fix(batch): 소박 6건 — closeHover/vpt12 소각/2KB 경고/WideToUtf8 공용화/글리프/모션 (I4)`

## 전체 통과 게이트 (컨트롤러)

- ① ninja RC=0 ② `jkdesktop.exe test` 0 failure(s) ③ app_tools ALL PASS
  ④ 라이브 스택 복원 후 list_windows/ping GREEN ⑤ I2 실측: 워크숍 슬롯
  jkx-pack 재실행 → jkx-list에서 scriptfile=/watch= 잔존 확인
- push + docs as-built + memory 갱신.

## Self-Review

1. Spec 커버: MANI 보존(I2 Step3/5)·O6 3부(선택+좌표+휠 I3)·소박 6건(I4 항목
   1-6) — docs/60 §6 + docs/67 룰링 + docs/65 O6 전체 대응. 소박 원장 6건
   목록과 1:1.
2. Placeholder 스캔: I4-4의 헤더 경로만 "grep로 확정해 기록" — 구현자 지시로
   수용 가능한 수준(헤더 후보는 단일 계열). 나머지 전부 구체 코드/라인.
3. 타입 일관: JkxManifestMerge(std::string,std::string) — 선언(JKJkxFile.h)
   =사용(main.cpp) 일치. ViewportRowToFull/NormalizeSelFull/EncodeWheelX10 —
   테스트·구현·사용 동일 서명.
