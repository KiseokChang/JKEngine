# 스펙: music 태그 쓰기+커버아트 (#111, 2026-10-25)

> **상태: 확정 — 사용자 승인(2026-10-25 chat 설계 승인 "진행") + 결정 3건
> 사용자 선택(커버아트=표시+삽입 전체·4형식 전부·행 [편집] 버튼+패널).
> 재량 항목 = 기각 시 즉시 수리.**

## 목적

music 목록 곡의 태그를 **파일에 직접 쓰고**(제목/아티스트/앨범 편집),
파일에 삽입된 **커버아트를 보고** 새 아트로 **교체할 수 있다** — 읽기
라인(#99)의 역방향. 파일이 곡명을 갖고 커버를 갖는다는 것이 음악의
정의 이외의 이유 없음(DRY 최소).

## 사용자 결정 원장 (2026-10-25)

- **D1 커버아트 = 표시+삽입 전체** (읽기 표시+이미지 파일 삽입 쓰기).
- **D2 쓰기 형식 = 4형식 전부** (mp3/FLAC/Ogg/WAV — 읽기와 등폐).
- **D3 UI = 행 [편집] 버튼+패널** (music 창 내 전환·폰 동일).

## 결정 원장 (컨트롤러 재량 — 기각 시 즉시 수리)

- **D4 조달 0**: third_party 조달 금지 — own 쓰기 부품(`jk::music::tags`
  확장, 읽기 파서의 역방향). 이미지 디코드/인코딩은 조달 third_party(
  stb_image/stb_image_write) 재용.
- **D5 v2.3 정규화**: mp3 쓰기는 ID3v2.3으로 통일(폰 호환성 — 일부
  폰 플레이어 v2.4 미지원). 기존 태그 유지 계약: v2 존재=정규화 재작성(
  TIT2/TPE1/TALB/APIC 교체+TCON 복제(장르 보존)+그 외 기존 프레임은
  원문 바이트 그대로 복사 보존 — 해석 불가 프레임 drop 금지)·v2 부재=
  신설(패딩 1KB)·v1 존재=v1 유지.
- **D6 원자적 쓰기**: 동일 dir `.pid.tagtmp` 파일 쓰기→fsync→rename
  (MusicDirStore #92 선례 승계) — 실패 시 원본 무손상. 백업 사본 미생성
  (백업은 사용자 몫).
- **D7 커버아트 읽기 = 지연**: 스캔 1패스에 섞지 않는다(#99 D2 원존
  무변조) — 편집 패널 열림 시 해당 곡만 ReadCover. 썸네일 캐시=메모리
  LRU 96(갤러리 선례 승계)·목록 행 썸네일 32px.
- **D8 커버아트 삽입 = 완전 교체**: 기존 아트 있으면 새 아트로 교체
  (FLAC PICTURE 블록 교체포함 — 복수 PICTURE는 첫 유효만 쓰고 보유
  블록 소각).
- **D9 FLAC 재조립**: STREAMINFO 보존(필수 첫 블록)·유효하지 않음
  판정된 텍스트 메타 블록 외 SEEKTABLE은 보존 시도(실패 시 원문 거부).
- **D10 Ogg 재번호**: 코멘트 페이지 교체 후 이후 오디오 페이지 시퀀스
  재작성(granule position 불변·체인 계약).

## 1. 사슬 설계 — `jk::music::tags` 쓰기 부품 (`engine/include/apps/MusicModel.h`)

`tags` namespace에 추가(읽기 ParseTag와 같은 파일·같은 스타일):

- `struct CoverArt { std::string mime; std::vector<unsigned char> bytes; };`
- `bool ReadCover(const std::string& path, CoverArt& out, std::string& err);`
  — mp3: APIC 프레임(인코딩 0 Latin-1 mime+desc) / FLAC: PICTURE 블록
  (mime·데이터) / Ogg: METADATA_BLOCK_PICTURE(베이스64 — vorbis 코멘트
  필드) / WAV: "id3" 청크의 APIC. 실패=정직 거부(빈 out·err).
- `bool WriteTag(const std::string& path, const TagSet& t, const CoverArt*
  cover /* null=title/artist/album만·cover는 교체, 미지정=기존 유지 */,
  std::string& err);` — 확장자(+헤더 스니프)로 형식 분기:
  - **mp3**: WriteId3v2.3 — 기존 v2 크기 산출→새 태그 크기 계산→패딩
    조절→동영상 데이터 재배치(전체 파일 rewrite·D6 원자적). v2.3
    프레임 10B 헤더+32bit big-endian size·텍스트 인코딩 1=UTF-16LE
    (#94 역변환 — UTF-8→UTF-16LE·BOM)+APIC(인코딩 0·mime"image/jpeg|png")
    ·TCON 유지(v2.3 프레임 복제).
  - **FLAC**: METADATA 블록 연쇄 재조립(§D9·BLOCK_TYPE 4=VORBIS_COMMENT
    교체·6=PICTURE 신설/교체·그 외 보존)·is-last 마킹 재계산.
  - **Ogg**: 첫 vorbis 코멘트 페이지 교체+이후 페이지 재번호(§D10·
    granule 불변).
  - **WAV**: RIFF 청크 연쇄에서 "id3" 청크 교체/신설(RIFF 크기 재계산).
- 실패 계약: 확장자/스트럭쳐 스니프 불일치·비지원 형식·읽기 실패=정직
  거부+원본 무변경.

## 2. 커버아트 표시 (`engine/src/apps/ClientMusicApp.cpp`)

- 목록 행 좌측 32px 썸네일(트랙 캐시 LRU 96·갤러리 썸네일 캐시 선례
  승계)·아트 부재 = 기본 자리표시(회색 사각 — 기존 행 라벨 변조 없음).
- 디코드: stb_image 조달품 재용(트리 존재)·RGBA 텍스처 캐시.

## 3. UI — [편집] 패널 (`ClientMusicApp.cpp`)

- 목록 행 우측 **[편집] 버튼** 신설(기존 도구/더블클릭/행 클릭 배선
  원존 무변조) → 태그 편집 패널([폴더 관리] 패널 동형 전환 패턴).
- 패널: 제목/아티스트/앨범 InputText(현값 프리필)+커버아트 큰 미리보기
  +[커버 선택] 버튼(#97 브라우저 부품 재용 — 폴더 브라우저 동형 폴더/파
  일 탐색, 타이핑 회피 계약)+[저장](WriteTag+재스캔 재청구)/[취소].
- 재생 화면(vplayer/spatial) 무접촉. 읽기전용/잠금 파일=거부 원문 표기
  (#94 규약 — 파일 터치=네이티브 경로).

## 4. 도구

- `music_tag_write` 1종 신설(agent tool hub — music 앱 등록 3종에
  병합되는 4종째): args {path, title, artist, album, cover(path|null)}
  — UI 사슬 재용(같은 WriteTag). 도구 6종(서버)+3종(music)+1종=원존
  무변조, music_dir_list 불변.
- 폰 probe 동형 세그먼트(XTEST/send_input 폴백 계약 — #110 dd96501
  선례 승계).

## 5. 안전성·오류

- 원자적 쓰기(D6)·재스캔 갱신(편집 후 자동 재청구 — 취소 계약 #93 원존)
- 크기 0/스니프 실패/비지원 = 정직 거부(수정 없음)·err 원문 표기
- CP949/한글 파일명: **파일 콘텐츠 UTF-8 규약(#94)**·파일명 터치=
  네이티브 경로(fs 게이트) — Win ACP 변환 승계.

## 6. 테스트

- **selftest 2u 신설**(2s 계보 승계 — 합성 원문 블롭 inline): ①합성
  mp3 v2.3 재작성→ParseTag round-trip ②태그 없는 mp3 v2 신설 ③v1
  유지+v2 병행 ④FLAC 블록 재조립(STREAMINFO 등호+재독) ⑤FLAC
  PICTURE 신설/교체 ⑥Ogg 코멘트 교체+재독·페이지 시퀀스 연속 ⑦WAV
  id3 청크 교체+RIFF 크기 등호 ⑧APIC 삽입→ReadCover round-trip(mime)
  ⑨한글 UTF-8→v2.3 UTF-16LE round-trip ⑩원자적 쓰기 실패=원본 바이트
  등호 ⑪커버 교체(D8) ⑫미지정 cover=null=기존 APIC 유지 ⑬빈 파일
  정직 거부 ⑭ReadCover 부재=정직 거부 ⑮WAV 신설 청크 ⑯폰 캐논 계보
  — **2u≥16건 목표**.
- 4축 캐논 승계(폰 668 유지 — 쓰기는 스캔 경로 비접촉·신설 어셜션 0.
  selftest 계보 표기 668→+2u n).
- WSL probe: 합성 파일 지시 쓰기→재독 표기 원문+행 썸네일 캡처·폰
  probe: 동형 세그먼트(캐논 흡수+표기 원문).

## 7. 성공 판정

- selftest 2u 전축+WSL 실측+폰 캐논 등호 — 사용자 육안(편집 체감·
  커버아트 모양+체감·폰 체감) = EYES.
- 캐논 계보 표기(2u n — 4축 등호 유지).

## 8. 비-목표

- 태그 쓰기 장르/년도 필드 편집 UI(v1 — TCON 복제는 유지, 편집 필드는
  3개만)·커버아트 자동 fetch/스크래핑(외부 네트워크 조달=비목표)·
  m3u/정렬 변경·스캔 도중 커버아트 일괄 로드(성능 원존)·vplayer 재생
  화면 커버 표시(별도 합의)·백업 사본 자동 생성.