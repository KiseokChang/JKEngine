# music 태그 쓰기+커버아트 (#111) 플랜

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:subagent-driven-development.

**Goal:** music 목록 곡의 제목/아티스트/앨범을 파일에 직접 쓰고(4형식), 커버아트를 읽어 행/패널에 표시하며 이미지 파일을 커버로 교체.

**Architecture:** `jk::music::tags`에 쓰기 사슬 확장(ReadCover+WriteTag — 형식별 역방향 재조립)·UI는 행 [편집] 버튼+패널([폴더 관리] 패턴+커버 선택=#97 브라우저 재용)·재스캔 갱신·원자적 쓰기(tmp+rename).

**Spec:** docs/superpowers/specs/2026-10-25-music-tagwrite-design.md (D1-D10)

## Global Constraints

- 조달 0 — own 쓰기 부품+조달 third_party(stb_image/jpeg/png 재용)의 디코드만. **spatial-player·vplayer 재생 화면 무접촉**·스캔 경로(ScanAudioTree/ParseTag 튜닝) 무변조.
- UTF-8 규약(#94): 파일 콘텐츠=파서 정규화 UTF-8·파일 터치(쓰기)=네이티브 경로(fs 게이트 — Win ACP 승계).
- idle 계약(#89)·도구 원존 무변조(music_dir_list 불변)·취소 계약(#93) 원존.
- 캐논(Win 691/WSL 668/posix 304/폰 668) — 신설 selftest 계보 `2u`만 증분·기존 계보 등호 유지.
- 커밋 계약 전부(명시 경로·IP 0건(-F)·push·amend 금지·트레일러)·빌드 ninja 3축 표준·서브에이전트 스폰 금지·테스트 음성/이미지 = 합성만(i:\@keep 금지).
- 폰 probe: XTEST 좌표 유계 = send_input 폴백 계약(#110 dd96501 선례 — XTAP_UP 게이트 원존).

---

### Task 1: tags 쓰기 부품 mp3 leg — WriteId3v2.3+ReadCover APIC

**Files:**
- Modify: `engine/include/apps/MusicModel.h` — tags namespace:
  - `struct CoverArt { std::string mime; std::vector<unsigned char> bytes; };`
  - `bool ReadCover(const std::string& path, CoverArt& out, std::string& err);` — mp3: APIC 프레임(첫 유효·인코딩 0/1 모두 지원 — mime Latin-1+desc skip·picture 데이터)·FLAC/Ogg/WAV leg는 T2 몫(이번 Task에서는 mp3만, 다른 형식은 정직 거부 err="unsupported_format")
  - `bool WriteTag(const std::string& path, const TagSet& t, const CoverArt* cover, std::string& err);` — 확장자+스니프 분기·이번 Task는 mp3 leg만(다른 형식=정직 거부)·spec §1 mp3 사슬(D5·D6)·APIC 삽입(cover null=기존 APIC 유지 — ReadCover 재용)
- Test: `engine/src/main.cpp` selftest **2u** 신설(기존 2a..2t와 충돌 확인 — 있으면 다음 문자) — 계보 케이스: ①합성 mp3(무태그)에 v2.3 신설(TIT2/TPE1/TALB+패딩 1KB)→ParseTag round-trip ②기존 v2.4 파일→v2.3 정규화 재작성→재독 ③기존 v1만 파일→v2 신설+v1 값 유지(재독=v2 우선 규약 원존) ④한글 UTF-8 입력→v2.3 UTF-16LE(BOM 0xFF 0xFE) 인코딩 1 round-trip ⑤APIC(합성 jpeg 블롭 512B "image/jpeg") 삽입→ReadCover round-trip(mime 등호) ⑥cover=null 재작성→기존 APIC 보존 ⑦실패 계약(0바이트 정직 거부+원본 등호)

**Interfaces:**
- Consumes: `tags::ParseId3v2`(재독 검증)·`tags::ParseTag`·Utf8ToAnsi/AnsiToUtf8(#94 — MusicModel.h 기존)
- Produces: `CoverArt`·`ReadCover`·`WriteTag` 서명(이후 태스크 전부의 계약)

- [ ] **Step 1:** 2u 케이스 ①-⑦ 먼저 작성(fail 확인 — 정의 부재 컴파일 에러)
- [ ] **Step 2:** mp3 사슬 구현(v2.3 재작성 — 10B 헤더+32bit BE size·syncsafe 아님 주석·v2 크기 산출→패딩 조절→오디오 데이터 재배치·D6 원자적 tmp+rename+fsync)
- [ ] **Step 3:** 빌드 Win+WSL 등호(기존 계보 등호+2u 7건 PASS)
- [ ] **Step 4:** 커밋 `feat(apps): music 태그 쓰기 mp3 leg — ID3v2.3+APIC (T1)`

### Task 2: FLAC/Ogg/WAV write legs+ReadCover 3형식

**Files:**
- Modify: `engine/include/apps/MusicModel.h` — WriteTag/ReadCover 분기 확장:
  - **FLAC**: METADATA 블록 연쇄 재조립 — STREAMINFO(첫 블록·데이터 보존)·VORBIS_COMMENT(4) 교체·PICTURE(6) 신설/교체(D8)·SEEKTABLE 등 나머지 보존(D9)·is-last 재마킹·VorbisComments 벤더 문자열 유지(기존 것 재용 — ParseFlacTags 계열 스니프)
  - **Ogg**: 첫 vorbis 코멘트 페이지 교체+코멘트 뒤 모든페이지 시퀀스 재작성(D10 — granule position·첫 페이지의 오디오 스트림 시작 불변)·METADATA_BLOCK_PICTURE base64 필드(D8 교체)
  - **WAV**: "id3" 청크 교체/신설+RIFF 크기 재계산·기존 청크 순서 보존
  - ReadCover 확장: FLAC PICTURE 블록(4=PICTURE mime·data)·Ogg base64Picture(디코더 inline — 표준 base64 20행)·WAV id3 청크 APIC 재용(T1 mp3 APIC 파싱 재용)
- Test: `engine/src/main.cpp` 2u 이어서 — ⑧합성 FLAC(무태그·STREAMINFO+한 개 오디오 프레임 스니프 픽스처)→VORBIS_COMMENT 신설→ParseTag round-trip ⑨FLAC PICTURE 신설→ReadCover(mime) ⑩FLAC 기존 PICTURE 교체(D8 — 2차 아트 등호) ⑪합성 Ogg(2페이지)→코멘트 교체→ParseTag round-trip+페이지 시퀀스 연속성 재독 ⑫Ogg METADATA_BLOCK_PICTURE round-trip ⑬합성 WAV→id3 청크 신설→ParseTag round-trip+RIFF 크기 등호 ⑭WAV id3 APIC→ReadCover
- [ ] **Step 1:** 2u ⑧-⑭ 작성(fail 확인)
- [ ] **Step 2:** 3형식 구현 — 빌드 3축 등호(기존 계보 등호+2u 누적)
- [ ] **Step 3:** 커밋 `feat(apps): music 태그 쓰기 FLAC/Ogg/WAV leg+ReadCover 전형식 (T2)`

### Task 3: UI — 행 [편집] 버튼+태그 편집 패널+커버아트 표시

**Files:**
- Modify: `engine/src/apps/ClientMusicApp.cpp` (+`engine/include/apps/` 헤더 — 필요분): 
  - 행 셀에 **[편집] 버튼**(기존 [spatial] 버튼 옆 — 버튼 클릭/행 클릭/도구 배선 원존 무변조) → **태그 편집 패널**([폴더 관리] 패널 동형 전환 패턴 — 같은 창·탭 유지)
  - 패널: InputText 3개(제목/아티스트/앨범 — 현값 프리필·파일명 폴백 규약 승계)+커버아트 미리보기(stb_image 디코드 — Texture LRU 96·갤러리 선례)+[커버 선택]([찾아보기] 브라우저 부품 재용 — 이미지 필터 jpg/png만·타이핑 회피 계약)+[저장](WriteTag(cover 지정 시 삽입)→성공 시 재스캔 재청구+패널 닫힘·실패 시 err 표기)/[취소]
  - 목록 행 좌측 **32px 썸네일**: ReadCover 지연 읽기(D7 — 스캔 무접촉)·아트 부재=회색 자리표시·LRU 96 캐시
- Test: selftest 어셜션 없음(UI — 캐논 등호 원문)·빌드 3축 등호
- [ ] **Step 1:** 패널+행 버튼 구현(기존 패턴 파일 참조 — 폴더 관리 패널·브라우저)
- [ ] **Step 2:** 빌드 3축 등호+selftest 계보 재실측
- [ ] **Step 3:** 커밋 `feat(apps): music 태그 편집 패널+커버아트 표시 (T3)`

### Task 4: music_tag_write 도구 신설

**Files:**
- Modify: `engine/src/apps/JKAppModule_music.cpp`(또는 ClientMusicApp 도구 등록부 — 기존 trio 등록 위치 참조): `music_tag_write` 4종째 — args `{path, title, artist, album, cover(path|null)}` — cover path → 파일 읽어 CoverArt(mime을 마법수로 판정 jpeg=FFD8/png=8950)→`tags::WriteTag` 재용(같은 사슬)·성공=재스캔 재청구+`{"ok":true}`·`music_dir_list 등 기존 도구 8종 원존 무변조`
- Test: selftest 케이스 2u ⑮(도구 경로 합성 파일 지시 쿼리 — 재독 표기 원문)
- [ ] **Step 1:** 도구 등록+2u ⑮ — 빌드 3축
- [ ] **Step 2:** 커밋 `feat(apps): music_tag_write 도구 — agent 쓰기 경로 (T4)`

### Task 5: WSL probe — 쓰기 round-trip 실측

**Files:**
- Modify: `engine/tools/probes/wsl_music.sh` — 세그먼트 신설: 합성 파일(무태그 mp3+FLAC) 도구/직접 쓰기→**music_tag_write relay 원문**(accepted/재독 표기 — 목록 행 [Phone Tag alpha...]-동형 원문)+행 썸네일 캡처 1종+기존 태그 세그먼트 원존·2u 계보 라인 재확인
- [ ] **Step 1:** probe 확장+런 2회(원문 수취)
- [ ] **Step 2:** 커밋 `test(probes): music WSL probe — 태그 쓰기 원문 (T5)`

### Task 6: 폰 probe 동형+폰 캐논 재실측

**Files:**
- Modify: `engine/tools/probes/phone_music.sh` — 동형 세그먼트(합성 파일 쓰기→재독 표기+행 썸네일 캡처 — XTAP send_input 폴백 계약 원존 — XTAP_UP 게이트)·폰 selftest 계보 668→+2u n 재실측(폰 접촉 런 — 192.168.11.0/24 서브넷 정찰+PHONE_HOST 1줄 — 주소 인쇄 금지 봉인)
- [ ] **Step 1:** probe 확장+폰 런(캐논 승계 원문)
- [ ] **Step 2:** 커밋 `test(probes): music 폰 probe — 태그 쓰기 동형+캐논 (T6)`

### Task 7: as-built docs

**Files:**
- Modify: `docs/90.md` §7.10 신설(사슬·커버아트 지연 읽기·원자적 쓰기·캐논·함정·EYES)+§4.1 부기 — docs/superpowers/specs/2026-10-25 뒤로
- [ ] **Step 1:** 작성
- [ ] **Step 2:** 커밋 `docs(apps): music 태그 쓰기 as-built (T7)`

## 사용자 게이트

1. [편집] 패널 체감·커버아트 모양/체감·폰 체감 = 사용자 선언(EYES).
2. 결정 D4-D10 = 재량 — 기각 시 즉시 수리.