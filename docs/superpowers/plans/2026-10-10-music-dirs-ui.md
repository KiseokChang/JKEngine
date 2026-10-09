# music 폴더 관리 UI+도구 3종 (#92) 구현 플랜

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:subagent-driven-development.

**Goal:** music 앱 내에서 `music.dirs`를 추가/제거하는 UI+앱 도구 3종 — settings.json 수기 편집 소거.

**Architecture:** MusicDirStore(읽기-수정-쓰기 원자적 — 임시+rename) 순수 부품+UI 패널(목록·추가 Edit·행 제거)+도구 3종 등록. 파일을 쓰는 앱은 이번이 첫 자율 UI(정직 문화 — 실패 표기 필수).

**Spec:** docs/superpowers/specs/2026-10-10-music-spatial-leg-design.md §2(확장·D4 정직 문화 승계)+플랜에 명시된 결정 원장(본 문).

## Global Constraints

- **신설 결정(컨트롤러 재량·기각 시 즉시 수리):** ①settings.json 읽기-수정-쓰기 원자적(임시파일+rename) ②기존 키(audio/retention/gallery 등) 무손상 파싱 유지 ③경로 직접 입력 Edit만(브라우즈 대화=백로그) ④쓰기 실패=정직 표기(무음 금지) ⑤쓰기 성공=즉시 재스캔(등록-스캔-표기 1패스).
- #92 계약 계승: idle 계약(docs/88)·정직 문화(D4)·도구 허브 계약(T2 수형)·폰 leg 배제(D5 — UI만 폰에도 뜬다, leg 무관).
- 캐논(현 Win 631/WSL 608/posix 296/폰 608) — 신설 `2o` 계보 표기 의무.
- 커밋: 명시 경로만+`--stat`+rev-parse+origin main 직행+amend 금지+트레일러. IP+호스트 경로 0건(-F).
- 빌드 `ninja -C engine/build -j3`+WSL; env-set 빌드 불요(leg 무접촉 — 2변형 재실측 불요, 등호만).
- Windows/WSL selftest 표준(WSL은 내부 리다이렉트). 서브에이전트 스폰 금지.

---

### Task 1: MusicDirStore — settings.json 원자적 쓰기(순수 부품)+2o

**Files:**
- Create: `engine/include/apps/MusicDirStore.h` — `jk::music::store`:
  - `struct DirWriteResult { bool ok=false; std::string err; };`
  - `DirWriteResult AddDir(const std::string& exeDir, const std::string& path);` — settings.json 원문 부재=신설 JSON 1건(music.dirs만·기본형), 기존 원문 파싱(부적합 JSON=err — **원본 보존·오염 금지**), `music.dirs`에 중복(정규화 후) 있으면 no-op ok, 쓰기=같은 폴더 settings.json.tmp+fs::rename(원자적 — 실패 시 원본 유지+err).
  - `DirWriteResult RemoveDir(const std::string& exeDir, const std::string& path);` — 미존재=no-op ok(정직), 기본 폴더(state/music)는 대상 아님(리포트 명시 — removable=false).
  - `std::vector<std::string> UserDirs(const std::string& settingsJson);` — 기존(파싱 실패=빈 목록 — 뷰 전용·오류 비표기). AddDir 경로의 역슬래시는 슬래시 정규화(2m-g 원문 수형·기본 폴더와 동일 규약).
- Test: `engine/src/main.cpp` 2n 아래 **2o** — ①부재 파일 신설 원문 2건(dirs 1건+기본 키 유지) ②기존 settings(audio/retention) 보존 원문 2건·music.dirs 합성 ③중복 Add no-op·Remove 미존재 no-op(정직) ④부적합 JSON 원본 보존+err 1행 ⑤UserDirs 파싱 2건 — **2o 10건**.
- [ ] **Step 1:** 2o selftest 원문(2n 아래)
- [ ] **Step 2:** MusicDirStore 구현
- [ ] **Step 3:** 빌드 Win+WSL·selftest — 캐논 Win 631→(2o n)/WSL 608→(2o n) 계보 표기
- [ ] **Step 4:** 커밋 `feat(apps): music 폴더 저장소 — settings.json 원자적 쓰기 (T1)`

### Task 2: UI 패널+도구 3종 등록

**Files:**
- Modify: `engine/src/apps/ClientMusicApp.cpp`(+h) — ①디렉토리 스트립 옆 [폴더 관리] 토글 → 패널: UserDirs 목록(행별 [제거])+경로 Edit+[추가] 버튼 — Store 소비(성공=재스캔·실패=status 1행 정직 표기·무음 금지) ②도구 3종 등록: `music_dir_add{path}·music_dir_remove{path}·music_dir_list` — Store 원문 소비(등록 수형=T2 도구 3종 원문·T2 계약 무보존) ③**leg 무접촉**(기존 계약) — env 미설정 빌드에서 UI+도구 동작(RC=0·순수부품만).
- Test: selftest — 신설 어설션 없음(T1 소유) — 캐논 등호·env 미설정 RC=0.
- [ ] **Step 1:** 패널+도구 구현
- [ ] **Step 2:** 빌드 Win+WSL(env 미설정)+selftest 계보
- [ ] **Step 3:** 커밋 `feat(apps): music 폴더 관리 UI+도구 3종 (T2)`

### Task 3: WSL probe 확장 — dirs UI/도구 실측

**Files:**
- Modify: `engine/tools/probes/wsl_music.sh` — 세그먼트 신설: music_dir_add(합성 폴더 — $TMPDIR)→music_dir_list 원문 1행(등록 확인)→music_dir_remove→list 원문(제거 확인)·UI [폴더 관리] 패널(모양 1캡처 — engine/tmp)·무음 실패 금지 검증(부적합 주입은 probe에서 안 함 — EYES).
- [ ] **Step 1:** probe 확장+실행(원장 준수: 시차 배치·세척 후 grep) 2) 커밋 `test(probes): music 폴더 dirs 도구 실측 세그먼트 (T3)`

### Task 4: 폰 probe 확장 — 캐논 흡수+동형 원문

**Files:**
- Modify: `engine/tools/probes/phone_music.sh` — dirs 도구 3종 동형 세그먼트(폰 $TMPDIR·원복 원문)+캐논 흡수(608→+2o)+모양 캡처 1종.
- [ ] **Step 1:** probe 확장+폰 실측(SWEEP-DIFF=0) 2) 커밋 `test(probes): music 폰 probe — dirs 도구 동형 세그먼트 (T4)`

### Task 5: as-built 갱신 — docs/90 §7

- Modify: `docs/90_music_spatial_leg_asbuilt.md` — §7 신설(폴더 관리 UI 원장 — 첫 settings.json 쓰기 앱·원자적 수형·park 승계)+§4.1 park 표 갱신.
- [ ] **Step 1:** docs/90 §7 작성 2) 커밋 `docs(apps): music 폴더 관리 as-built 갱신 (T5)`

## 사용자 게이트 요약 (플랜 밖)

1. 폰/Win 모양 육안+폴더 추가 수동 동작 = 사용자 선언만 결제(EYES).
2. 결정 원장 5건 = 재량 — 기각 시 즉시 수리.