# music 폴더 브라우저 (#97) 구현 플랜

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:subagent-driven-development.

**Goal:** [폴더 관리]에서 폴더를 타이핑 없이 선택 — 내장 미니 브라우저(폴더 목록+상위+이 폴더 추가).

**Architecture:** 순수 부품 DirBrowserModel(cwd 스캔·dirs-first·상위 이동·결정론)+UI 브라우즈 패널(탭 전용 — 폰 최적화). 별도 대화상자 창/AppTool 릴레이 없음(키보드 불요 — 폰 입력 계약 회피).

**Spec:** #92 라인(2026-10-10-music-dirs-ui) 계약 승계+본 플랜 결정 원장. 발단: 사용자 "파일 다이알로그로 폴더 선택해야 하지 않아요?"(2026-10-11 — 브라우즈 백로그의 결제=내장 방식 정산).

## Global Constraints

- **결정(재량·기각 시 즉시 수리):** ①내장 미니 브라우저 본채택(별 대화상자 앱 아님 — 폰에서 별도 창+포커스+키보드 회피가 직접 동기) ②브라우저는 **폴더 전용**(파일 목록 숨김 — 확장자 필터와 혼동 방지) ③[이 폴더 추가] = Store.AddDir+기존 재스캔 계약 — 성공=브라우즈 종료+목록 반영 ④숨김 dir 스킵(도트 런 — LibraryCatalog 수형)+ ec 중립형 ⑤경로 표기 = UTF-8 규약(#94).
- 캐논(Win 661/WSL 638/posix 296/폰 638) — 신설 `2r` 계보 표기.
- idle 계약·도구 6종·Store 원존·P1 수형·T1 fix r1 CP949 스캐너 전부 무변조.
- 커밋 계약 전부(명시 경로·IP 0건(-F)·push·amend 금지). 빌드 ninja 표준+WSL. 서브에이전트 스폰 금지.

---

### Task 1: DirBrowserModel(순수)+2r

**Files:**
- Modify: `engine/include/apps/MusicModel.h` — `jk::music::browse`:
  - `struct DirPage { std::vector<std::string> dirs; };`
  - `DirPage ListSubdirs(const std::string& cwd);` — cwd의 폴더만(ec 중립·숨김 `.도트` 스킵·파일과 상위 ".." 는 UI 측 상수 행 — 모델은 subdir 원문만·이름 오름차순 결정론).
  - `std::string JoinDir(const std::string& cwd, const std::string& name);` — '/' 규약(#94·Slashize 재용).
  - `std::string Parent(const std::string& cwd);` — 루트에서 상위=빈 문자열(또는 원문 그대로 — 최상위 부모가 자기 반환 금지 — 어샥션).
- Test: `engine/src/main.cpp` **2r** (2q 아래): ①ListSubdirs — 임시 트리(폴더 3+파일 2+숨김 폴더 1): 폴더 3 오름차순+파일 0 ②JoinDir 슬래시(기존 슬래시·끝 슬래시) ③Parent 3케이스(중간/루트/루트 부모=부자기 금지) ④존재 하지 않는 cwd=빈 페이지(ec) — **2r ~8건**.
- [ ] **Step 1:** 2r 원문/2. 구현/3. 빌드 Win+WSL+selftest 계보(661→/638→ +2r n)/4. 커밋 `feat(apps): music 폴더 브라우저 순수 부품+2r (T1)`

### Task 2: UI 브라우즈 패널

**Files:**
- Modify: `engine/src/apps/ClientMusicApp.cpp`(+h) — [폴더 관리] 패널에 **[찾아보기] 토글**: cwd 경로 표기 1행+하위 폴더 목록(클릭=JoinDir 진입·".." 행=Parent)+**[이 폴더 추가]**(현재 cwd → Store.AddDir — 성공=브라우즈 종료+재스캔·실패=status 정직 1행)+[닫기]. cwd = dirs_[dirIndex_](등록 목록의 현재 탭 루트 — 시작점 자연). idle 계약(변화 시만 더디).
- [ ] **Step 1:** UI/2. 빌드 등호 3축 원문/3. 커밋 `feat(apps): music 폴더 브라우저 패널 — 타이핑 없는 선택 (T2)`

### Task 3: WSL probe — 브라우저 실측

- Modify: `engine/tools/probes/wsl_music.sh` — ①music_dir 도구 대조군 유지 ②브라우징 실측: (도구 경유 불가 — UI 탭 실측) — **send_input이 WSL에서 가능(기존 세그먼트 원문 수형 — 클릭 브래킷) → [폴더 관리]→[찾아보기]→cwd 표기 원문 캡처+하위 목록 캡처·2종** — 불가 세션이면 원장행.
- [ ] **Step 1:** probe 확장+런/2. 커밋 `test(probes): music WSL probe — 브라우저 실측 세그먼트 (T3)`

### Task 4: 폰 probe — 브라우저 동형(최소)

- Modify: `engine/tools/probes/phone_music.sh` — 폰 동형 최소: 브라우저 탭 전환 수단=XTEST(#96 원장 수형 — xtap 재조립 — 계약상 삭제 금지 유지)·cwd 표기/목록 캡처·폰 selftest 캐논(638→+2r).
- [ ] **Step 1:** probe 확장+폰 실측(SWEEP-DIFF=0·sync 선행)/2. 커밋 `test(probes): music 폰 probe — 브라우저 동형 세그먼트 (T4)`

### Task 5: docs/90 §7.8 갱신

- [ ] **Step 1:** §7.8 신설(체인·원리·캐논·함정·EYES)+§4.1 추가만/2. 커밋 `docs(apps): music 폴더 브라우저 as-built 갱신 (T5)`

## 사용자 게이트

1. 폰/Win 브라우저 체감(+추가→스캔 전 사이클) — 사용자 선언(EYES).
2. 결정 5건 = 재량.