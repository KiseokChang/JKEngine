# 스펙: opus/m4a 태그+커버아트 지원 (#115, 2026-10-26)

> **상태: 확정 — 백로그 등재 원장("opus/m4a"·갱신 58 이후 지시 대기)·사용자
> "최대한 구현 진행" 지시(2026-10-26 출근). 재량 = 기각 시 즉시 수리.**

## 목적

music 라이브러리에서 **opus/m4a 파일이 "-" 폴백 행**(#99 읽기 4형식의 밖) —
이 라인 = 두 형식의 **태그 읽기+쓰기+커버아트**를 기존 사슬에 병합. **재생
디코드(m4a AAC/opus)는 v1 비-목표**(spatial-player decoder 확장 = 별도
승인 라인 — 조달 무거움 — 원장행으로 봉인).

## 재량 결정 원장 (기각 시 즉시 수리)

- **D1 조달 0**: 두 형식 전부 **own 순수 파서** — m4a = MP4 컨테이너 box
  보행(moov/udta/meta/ilst의 ©nam/©ART/©alb·covr atom)·opus = Ogg 컨테이너
 (OpusHead/OpusTags — 기존 Ogg 쓰기 사슬의 응용·VorbisComment 재용)·쓰기는
  #112 WriteTag 사슬의 두 분기 신설.
- **D2 m4a 쓰기**: ilst atom 재작성 — moov 밖 mdat(오디오 raw)는 원문 바이트
  이동 없음(moov/metadata box 재조립만)·free atom 흡수 원존 계약.
- **D3 opus 쓰기**: 기존 Ogg 재조립 계약(D10 — granule 불변) 재용·
  OpusTags는 vorbis 코멘트 패킷과 동일 구조(벤더/엔트리) — MBP 필드 재용.
- **D4 커버아트**: m4a = covr atom(png/jpeg — mime 포함 flag)·opus =
  METADATA_BLOCK_PICTURE 재용 — ReadCover/WriteTag 사슬 병합.
- **D5 UI/도구 무변경**: 5→7열(커버+편진 열)이 이미 존재 — opus/m4a 행이
  "-" 대신 실값 표기로 전환되는 것뿐·music_tag_write 도그도 무변경(사슬
  확장은 WriteTag 내부 몫)·확장자 스니프(.opus/.m4a/.mp4/.m4b) 정합.
- **D6 재생 비-목표 원장행**: opus/m4a 재생 = spatial-player/decoder 조달
 라인(별도 합의·플레이버 밴드 "-" 표기 유지·"태그만 지원" 표기 관행 원존
 [#110 폰 probe 원文]).

## 1. 구현 단위

`engine/include/apps/MusicModel.h` tags namespace 확장(기존과 동형):

- 읽기: `ParseM4aTags`/`ParseOpusTags`(ParseFlacTags 계열 승계)·ParseTag
  스니프 분기 확장(ftyp/moov — OggS의 OpusHead 매직).
- 쓰기: `WriteM4aTag`(box 재조립)·`WriteOpusTag`(Ogg 재조립 재용)·
  ReadCover 분기 2건(covr/MBP 재용).
- selftest **2x 신설**: ①합성 m4a 무태그→재작성 round-trip ②covr
 round-trip(mime) ③m4a mdat 원문 무이동 등호 ④opus 무태그→재작성 ⑤opus
 MBP round-trip ⑥한글 round-trip(2형식) ⑦실패 계약(잘린 box 정직 거부)⑧
 — 10-12건 목표.

## 2. 테스트·게이트

- selftest 2x 전축+캐논 승계(4축 증분 등호)·WSL probe 세그먼트(합성
 지시 쓰기→재독 표기 원문)·폰 probe 동형(접촉 런).
- EYES: 폰/Win 라이브러리에서 .m4a/.opus 파일 표기+커버아트 체감.

## 3. 비-목표

- 재생 디코드(D6)·AAC 디코더 조달·m4b 챕터/가사/년도 필드·iTunes 확장
 외 표기·폰 라이브러리 대형 스캔 성능 특화.