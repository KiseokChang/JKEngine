# 뮤직 라이브러리 (#90) 구현 플랜

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:subagent-driven-development.

**Goal:** 갤러리 쌍둥이 music 모듈 — `music.dirs` 재귀 스캔·파일명 목록·이름 필터·더블클릭 vplayer 위임 재생.

**Architecture:** 갤러리 선례(gallery.jkx·ClientGalleryApp·GalleryModel.h·2g selftest·probe) 그대로의 쌍둥이 — 새 오디오 코어 제작 없음, 재생은 기존 vplayer 앱 도구 `open` 위임.

**Spec:** docs/superpowers/specs/2026-10-09-music-library-design.md

## Global Constraints

- 갤러리 line 원문 승계: `ClientGalleryApp.cpp`·`GalleryModel.h`·`main.cpp` 2g(selftest :4944-)·wsl_gallery.sh·phone_gallery.sh — **문법=승계, 이름=music**.
- #89 idle 계약(docs/88) 준수 — 스캔 결과 도착·입력·필터 변화 시에만 더티; 타이머 틱 자체는 더티 아님.
- 캐논(현 Win 588/WSL 565/posix 296/폰 565) — 신설 `2m` 케이스 계보 표기 의무. 캐논 불변 전 단계 검증.
- 커밋: git add 명시 경로만+`git diff --cached --stat` 검증+rev-parse 실측+origin main 직행+amend 금지+Co-Authored-By 트레일러.
- IP 게이트(dotted-quad 0건)·폰 ssh 수속(known_hosts self-resolve·파일 기록 금지·$TMPDIR·pkill 브래킷·`< /dev/null` 금지·jkweb 절사 금지·서버 존중)+폰 캡처 ffmpeg x11grab.
- Windows 빌드 표준 2줄(`C:/msys64/ucrt64/bin` PATH 선행)+`-j3`; posix 풀 로그 리다이렉트; WSL selftest WSL 내부 리다이렉트.
- vplayer 테스트 미디어: i:\@keep 금지 — 엔진 소유 합성 클립만.
- 서브에이전트 스폰 금지(구현자).

---

### Task 1: 모듈 골격 — MusicModel.h 순수 부품+2m selftest+music.jkx

**Files:**
- Create: `engine/include/apps/MusicModel.h` — `jk::music` 네임스페이스:
  - `std::vector<std::string> MusicDirList(const std::string& exeDir, const std::string& settingsJson);` — 기본 폴더(`{exeDir}/state/music`) 앞+`music.dirs` 배열 뒤·중복 제거·빈 성분 제거·settings 부재/파손=기본 1건 fail-safe. 구조=갤러리 `jk::gallery::GalleryDirList` 쌍둥이(갤러리 원문을 읽고 이름만 바꿔 재현 — 로직 동형 복제 허가: 라인 쌍둥이 원리).
  - `std::vector<std::string> NormalizeDirs(const std::vector<std::string>& dirs);`
  - `struct Track { std::string full; std::string rel; std::string name; long long size; long long mtime; };`
  - `std::vector<Track> ListAudioFiles(const std::string& root);` — 재귀 전 트리+확장자 스캔(mp3/flac/wav/ogg/m4a/aac/wma, 대소문자 무시)+mtime desc 정렬+ec 중립형(개별 stat 실패=해당 항목 스킵). `rel`=루트 기준 슬래시 경로(상대경로 표기 계약).
  - `bool MatchFilter(const std::string& name, const std::string& filter);` — filter 빈 문자열=전부 참·대소문자 무시 부분일치(ASCII fold만 — 한글은 이진 비교, v1).
  - `struct AudioDirFallback` — `{exeDir}/state/music` 경로 조립(갤러리 `AppDirFallback` 쌍둥이).
- Test: `engine/src/main.cpp` — 2g(:4944-) 아래 2m: ①MusicDirList 2건(settings `{"music":{"dirs":["P:/sounds"]}}`→기본+P:/sounds·부재=기본·파손("dirs" 문자열)=기본 fail-safe) ②NormalizeDirs 중복 제거 2건 ③ListAudioFiles — 실제 임시 디톤더리에 하위폴더 1개+파일 4파일(mp3 재생확대·MP3 대문자·txt 제외·하위 wav) 트리 구성 후: 재귀 3건 수취·txt 제외·mtime desc·rel에 하위폴더 슬래시 포함 단정 ④MatchFilter 3건(빈 필터·부분일치·대소문자)
- Modify: `engine/src/main.cpp`만 — 이 태스크는 순수 부품+테스트만(모듈·CMake· 앱은 T2 — T1 커밋이 빌드를 깨지 않는다).
- [ ] **Step 1:** selftest 2m 원문 작성(main.cpp 2g 블록 아래)
- [ ] **Step 2:** MusicModel.h 구현
- [ ] **Step 3:** 빌드 3축 — Windows 2줄 표준+WSL
- [ ] **Step 4:** selftest — 캐논 계보 표기: Win 588→(2m n·2i 등호) / WSL 565→(2m n) 실측 원문 리포트
- [ ] **Step 5:** 커밋 `feat(apps): music 순수 부품+2m selftest (T1)`

### Task 2: music 모듈+ClientMusicApp — 비동기 스캔+리스트 UI

**Files:**
- Create: `engine/src/apps/ClientMusicApp.cpp` + `engine/include/apps/ClientMusicApp.h` — 갤러리 쌍둥이(어두운 루트창·16ms 타이머·OnThemeChanged·한글 데스크톱 폰트·AppContentTopOffset 밴드).
- Create: `engine/src/apps/JKAppModule_music.cpp` — JKAppModule_vplayer.cpp 원문 형태(meta `music`/`Music`/560x520+`jk_app_run_client`).
- Modify: `engine/CMakeLists.txt` — jkapp_gallery 행(갤러리 T1 원문: SHARED win/posix 양축·`music.jkx` pack+jkx_packages 1행) 옆에 jkapp_music 신설 — 갤러리 T1 커밋(56f26d9)의 CMake hunk 원문을 읽고 동형으로.
- **구성:**
  1. **디렉토리 스트립**(갤러리 동형) — MusicDirList 결과 탭, 선택=현재 루트.
  2. **비동기 스캔** — `std::thread` 워커(갤러리 썸네일 워커 원문 계약: 생성 주기·파괴 join·`std::atomic<bool> scanDone_`·도착 시 1회 `frameDirty_`=true — **도착 외 더티 금지(#89 계약)**). 루트 전환마다 다시 스캔.
  3. **트랙 리스트** — ImGui 표(열: [rel(상대경로)] [크기(bytes 사람단위 KB/MB 1자리)] [mtime(YYYY-MM-DD HH:MM)])+**이름 필터 Edit**(MatchFilter) — 필터 타이핑 시만 더티(입력 이벤트 — 자연 귀결).
  4. **빈 상태** — 스캔 0건: "트랙 없음 — settings music.dirs에 폴더를 추가하세요" 안내 행.
- Test: selftest `2m-e` 신설 — "스캔 워커 쌍둥이 케이스": ListAudioFiles 결과가 앱 표기 데이터 동형(정렬·rel) 단정으로 흡수 가능 — 신설 어설션 1건(모바일 불변 파킹 아님).
- [ ] **Step 1:** ClientMusicApp 골격+UI(원문 갤러리 쌍둥이)
- [ ] **Step 2:** 빌드 3축+selftest 캐논(2m n 갱신 표기)
- [ ] **Step 3:** 커밋 `feat(apps): music 모듈+클라 앱 — 비동기 스캔+리스트+필터 (T2)`

### Task 3: 재생 위임 배선 — launch_app+app_tool open

**Files:**
- Modify: `engine/src/apps/ClientMusicApp.cpp` — 더블클릭 핸들러.
- **계약(스펙 §4 원문):**
  1. 더블클릭 → `SendQuery("launch_app", "{\"app\":\"vplayer\"}")` (라이브러리 원문 ClientLibraryApp.cpp:246-255 쌍둥이 — queryId 적립+reply 흡수).
  2. 이어서 `SendQuery("app_tool", {"app":"vplayer","tool":"open","args":{"path":"<트랙 full>"}})` — windowId 미지정(단일 후보=직행 — JKWindowServer.cpp:3858-3875 후보 수집 계약). 실측: vplayer 등록 도구 허브(ClientVPlayerApp.h:58)에 `open` 존재.
  3. **폴백 원존 원칙:** 미지정+복수 후보(ambiguous) 등 실측 봉차면 — vplayer jkx 인자 지원 1행(`--file <path>` 승격)으로 교정 — **v1은 서버 신선 부품 추가 금지.** 실측 근거가 리포트에 없으면 안 됨.
- Test: selftest `2m-f` — 더블클릭 핸들이 open 경로 인자로 Track.full을 전달하는 형식 검증(순수 부품으로 핸들 로직 흡수 — `jk::music::OpenRequestJson(const Track&)` 신설: `{"app":"vplayer","tool":"open","args":{"path":"..."}}` 조립 — selftest 3케이스: 일반 경로·역슬래시 경로(슬래시 정규화)·공백 경로 에스케이프).
- [ ] **Step 1:** OpenRequestJson 순수 부품+2m-f selftest 원문
- [ ] **Step 2:** 더블클릭 배선(SendQuery 2곳 — 순수 부품 소비)
- [ ] **Step 3:** 빌드 3축+selftest 캐논
- [ ] **Step 4:** 커밋 `feat(apps): music 더블클릭 → vplayer 위임 배선 (T3)`

### Task 4: WSL probe — wsl_music.sh

**Files:**
- Create: `engine/tools/probes/wsl_music.sh` — 갤러리 wsl_gallery.sh 원문 계약 승계: ①부팅(taskbar 자동 스폰) ②music 앱 런치(launch_app)→window 목록 실측 ③리스트 렌더 실측(스크린샷 1장+stderr 진단 1행) ④**더블클릭 재생 위임 실측** — ②의 창에 app_tool open(합성 wav 경로)→vplayer 창 존재+get_status 재생 수치 1행.
- [ ] **Step 1:** probe 작성+실행 풀 로그
- [ ] **Step 2:** 커밋 `test(probes): music WSL probe — 위임 재생 실측 (T4)`

### Task 5: 폰 probe — phone_music.sh

**Files:**
- Create: `engine/tools/probes/phone_music.sh` — phone_gallery.sh 원문 계약 승계(폰 selftest 캐논 흡수 2m·모양 캡처 ffmpeg x11grab·REMNANT 소각·서버 존중).
- **폰 selftest:** 캐논 565→(2m n) — 계보 표.
- [ ] **Step 1:** probe 작성+폰 실측(재배포 포함 — SWEEP-DIFF=0 전수 sweep canary 계약)
- [ ] **Step 2:** 커밋 `test(probes): music 폰 probe — 실기기 실측 (T5)`

### Task 6: as-built docs/89

- Create: `docs/89_music_library_asbuilt.md`(docs/88 문체) — §1 배선 원장(rev-parse)·§2 캐논+위임 실측 표·§3 함정·§4 deferred·§5 EYES 게이트. Modify: docs/87 §4 형제 라인 포인터.
- [ ] **Step 1:** docs/89 작성
- [ ] **Step 2:** 커밋 `docs(apps): 뮤직 라이브러리 as-built (T6)`

## 사용자 게이트 요약 (플랜 밖)

1. 폰 모양+위임 재생 육안 = 사용자 선언만 결제(EYES-PENDING).
2. 결정 3건(D1/D2/D3) 스펙 확정 — 기각 시 즉시 수리.
3. spatial-player 연동(2단계) — 별도 스펙 착수는 사용자 선언.