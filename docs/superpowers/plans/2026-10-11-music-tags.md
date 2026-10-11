# music 태그 읽기 (#99) 구현 플랜

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:subagent-driven-development.

**Goal:** music 목록에 곡명/아티스트/앨범 — ID3v2+v1(mp3)·dr_flac/stb_vorbis 메타(FLAC/Ogg)의 순수 파서가 스캔 1패스에 접첨.

**Architecture:** `jk::music::tags` 순수 부품(2s selftest — 합성 원문)+Track 확장(title/artist/album·파일명 폴백·UTF-8 규약)+UI 열 재편+MatchFilter 확장. 정렬 v1 고정(mtime desc).

**Spec:** docs/superpowers/specs/2026-10-11-music-tags-design.md

## Global Constraints

- 조달 0 — own 파서+조달 third_party(dr_*/stb_vorbis)의 메타 API 재용·spatial-player 무변경·저장소 무변조.
- idle 계약(#89)·UTF-8 규약(#94)·도구 6종 원존·P1/CP949/T2 가드 원존 무변조.
- 캐논(Win 671/WSL 648/posix 304/폰 648) — 신설 `2s` 계보 표기.
- 커밋 계약(명시 경로·IP 0건(-F)·push·amend 금지·트레일러). 빌드 ninja 3축 표준. 서브에이전트 스폰 금지. 테스트 음성 = 합성만(i:\@keep 금지).

---

### Task 1: tags 파싱 원문 실측+순수 부품+2s

**Files:**
- Modify: `engine/include/apps/MusicModel.h` — `jk::music::tags`:
  - `struct TagSet { std::string title, artist, album; };`
  - `TagSet ParseId3v2(const unsigned char* p, size_t n);` — ID3v2.3/2.4 헤더 10B+프레임(TIT2/TPE1/TALB/TDRC/TRCK)·인코딩 0=Latin-1(UTF-8 승격 — 바이트<0x80 등가·0x80-0xFF는 3byte 승격 규약 원문)·1/2=UTF-16(LE/BOM)·3=UTF-8 pass·syncsafe size·padding/footer 스킵.
  - `TagSet ParseId3v1(const unsigned char* p, size_t n);` — 말미 128B("TAG").
  - `TagSet ParseTag(const std::string& path, std::string& err);` — 파일 열어 앞부분/끝부분만(필요 범위만 read)·포맷 분기: mp3=v2→v1·flac=dr_flac 메타 API(실측 — TIT2 매핑)·ogg=stb_vorbis 코멘트(실측)·wav/dr_audio=이용 가능 여부 실측. 전부 UTF-8 정규화·실패=빈 TagSet(폴백 계약).
- Test: `engine/src/main.cpp` **2s**(2r 아래) — 합성 원문 블롭(inline byte 배열 — 리포트 원문) ①v2.4 TIT2/TPE1/TALB(UTF-8) ②v2.3 ISO-8859-1(승격 원문) ③UTF-16LE(BOM) ④v1 폴백 ⑤syncsafe 2^28 경계 ⑥padding/footer 무시 ⑦FLAC 합성 1건(T1에서 dr_flac API 실측·합성 불가면 own vorbis 코멘트 부분 파서 최소 신설 — 근거 리포트) ⑧Ogg 합성 ⑨필터 확장(제목/아티스트) ⑩fallback 규약 2건 — **2s ≥12건**.
- [ ] **Step 1:** dr_third_party 메타 API 실측 원문(쓰는 수형/갭 — 리포트)
- [ ] **Step 2:** 2s 원문+구현 — 빌드 Win+WSL·selftest 계보(671→/648→ +2s n·posix 304 사유)
- [ ] **Step 3:** 커밋 `feat(apps): music 태그 순수 파서+2s (T1)`

### Task 2: 스캔 통합+Track 확장

**Files:**
- Modify: `engine/include/apps/MusicModel.h` + `engine/src/apps/ClientMusicApp.cpp`(+h) — Track 확장(D3·폴백 규약)+ListAudioFiles 내 태그 파싱 접합(스캔 워커 1패스·헤더 범위만)·UI 열: [곡명(아티스트)] [앨범] [크기] [수정]·행 클릭/도구 원존 무변조.
- Test: selftest — 어설션 없음(2s 소유) — 캐논 등호 원문.
- [ ] **Step 1:** 통합+UI 2. 빌드 3축 등호(계보) 3. 커밋 `feat(apps): music 태그 스캔 통합+행 라벨 (T2)`

### Task 3: WSL probe — 태그 목록 실측

**Files:**
- Modify: `engine/tools/probes/wsl_music.sh` — 합성 태그 트랙(탐색 — 브라우저 도착 원문 수형) → **목록 열 표기 원문(곡명/아티스트/앨범·fallback 행)+필터(곡명 매치 원문) 캡처**·기존 세그먼트 원문 유지.
- [ ] **Step 1:** probe 확장+런 2. 커밋 `test(probes): music WSL probe — 태그 표기 실측 (T3)`

### Task 4: 폰 probe — 캐논 흡수+동형

**Files:**
- Modify: `engine/tools/probes/phone_music.sh` — 동형 세그먼트(XTEST 주입기)·폰 selftest 648→+2s n.
- [ ] **Step 1:** probe 확장+폰 실측 2. 커밋 `test(probes): music 폰 probe — 태그 동형 세그먼트 (T4)`

### Task 5: as-built — docs/90 §7.9 or docs/91

- docs/90 §7.9 신설(체인·원리·캐논·함정·EYES)+§4.1 추가만.
- [ ] **Step 1:** 작성 2. 커밋 `docs(apps): music 태그 as-built (T5)`

## 사용자 게이트

1. 태그 목록 모양/필터 체감 = EYES. 2. 결정 D1-D6 = 재량 — 기각 시 즉시 수리.