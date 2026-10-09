# 스펙: 뮤직 앱 spatial 재생 leg — audio_core 연동 (2단계, 2026-10-10)

> **상태: 확정 — 배선 옵션 1 사용자 선택("추천대로 갈게요", 2026-10-10).**
> 발단: #90 뮤직 라이브러리 착지 후 사용자 지시 — "완성 후
> I:\progwork\spatial-player\에서 만들 코어 라이브러리랑 연동도 해보자".

## 산전 전제 (spatial-player 실측 원문 — 스펙 작성 전 재실측)

`I:\progwork\spatial-player`는 이미 v1.0: **audio_core** 정적 라이브러리
(wav+spatial+decoder+stream_player·OpenAL Soft 1.23.1 소스 정적 링크) —
**mp3/flac/ogg/wav 스트리밍 디코드**(third_party dr_*/stb_vorbis)+버퍼큐
재생(**STEREO/POSITIONAL 모드 토글·seek·갭리스 메타**). API:
`Decoder::open(path,err)`(포맷 판별)·`BufferQueueStreamPlayer`(pump 폴링
계약)·`SpatialEngine`(AL 컨텍스트 래퍼/ 위치 제어). **spatial-player 저장소는
무변경(소비자 입장) — 트러스트 계약.**

## 결정 원장 (사용자 확정 — 기각 시 즉시 수리)

- **D1 재생 leg = 내장**(옵션 1) — jkapp_music DLL이 audio_core를 링크,
  music 앱 안에서 직접 재생. 위치 조작(방위각/거리/고도 슬라이더)도 앱 안.
- **D2 배선 = fail-closed** — CMake는 `SPATIAL_PLAYER_ROOT` 환경변수로
  spatial-player/audio_core를 add_subdirectory; **미설정이면 spatial leg가
  빌드 생략**(컴파일 단계부터 건너뜀) — music은 vplayer 위임만(v1 동작 무손상),
  리포트에 원장 표기. spatial-player 경로는 소스/커밋에 기록하지 않는다(로컬
  repo 규약 — env만).
- **D3 원존(공존 계약): 더블클릭=vplayer 위임 유지** — #90 D1 계약 무변조.
  spatial leg는 **행의 [spatial] 버튼**으로 발사(2채널 공존 — 실험 leg라
  기본 재생이 아니라 접근성 낮은 배선). v1 종착지가 아니라 leg 실험.
- **D4 지원 포맷 표기 = 정직 문화(배지 계약 승계):** audio_core 지원
  (mp3/flac/ogg/vorbis/wav)은 [spatial] 활성, 미지원(m4a/aac/wma)은
  회색+툴팁 "미지원 포맷".
- **D5 폰 스코프 = spatial leg 배제** — 폰은 vplayer 위임 유지(레퍼런스
  없는 최소 배선 — OpenAL/디바이스 백엔드 재판정은 EYES 후속). WSL은 leg
  실측 대상(ALC 디바이스 실패 → fail-closed로 위임 폴백+status 표기).

## 1. 빌드 배선

- JKEngine CMakeLists: `SPATIAL_PLAYER_ROOT` set일 때만
  `add_subdirectory(${SPATIAL_PLAYER_ROOT} spatial-player-build)` +
  jkapp_music `target_link_libraries(... audio_core)` — Win(MinGW)·WSL 양축.
- audio_core가 jkapp_music SHARED에 **정적 링크** — OpenAL Soft도 정적 —
  첫 설정 FetchContent(네트워크 소요) — 리포트 원장 표기.
- 폰: leg 빌드 비활성(phone 축 jkapp_music은 audio_core 미링크 —
  D5 — 조건부 소스 목록).

## 2. 재생 leg 구조 (music 앱 내)

- **상태 머신:** `Idles→Playing→Stopped` — StreamPlayer pump()는
  **OnIdle에서 폴링**(16ms 타이머 — idle 계약: 재생 중=진행 중 필요(진행
  표기 갱신)이면 더티 — vplayer 재생 유지 수형 동형, 무재생 pump 무더티).
- **배선:** 트랙 [spatial] 클릭 → `BufferQueueStreamPlayer.start(path)` —
  POSITIONAL 기본(모노 다운믹스+SpatialEngine 위치)·모드 토글(STEREO).
- **위치 UI:** 방위각(-180..180)·거리(0.5..20m)·고도(-45..45) 슬라이더
  3조(재생 중만 활성) — 드래그 변경 시만 더티(입력 이벤트 자연 귀결).
- **정직 귀속:** 재생 중 상태 표기 행(pos·모드·디바이스 명)+ALC 실패 시
  폴백 안내("spatial leg 불가 — vplayer 위임 이용").
- **app 도구 등록 3종**(앱 도구 허브 — #90 T3 같은 계약):
  `spatial_play{path}·spatial_stop·spatial_status` — probe/구두 조작(DeX
  비전의 구두 파이프라인 재료)을 위한 관측/조작 표면.
- **충돌 원장:** OpenAL 디바이스와 vplayer(SDL 오디오)는 프로세스 소유 다름
  — OS 믹서가 합산(동시 재생=혼합, 결함 아님 — 원장 표기).

## 3. 테스트

- **selftest 2n:** ①지원/미지원 포맷 판정 표(확장자→활성/회색) ②OpenRequest
  경로 계열 재용(공백·역슬래시) ③leg 상태 머신 전이 순수 검증 ④app 도구
  원문(3종 JSON) — 계보 표기(캐논 614/591/296/폰 591 → +n).
- **WSL probe:** wsl_music.sh 확장 — spatial_play 실측(get_status 재생
  수치 원문+정지)·leg fail-closed 1행(ALC 실패 환경 시뮬레이션은
  불요 — 실패 시 표기 원문만 실측).
- **폰 probe:** 캐논 흡수(2n n건·**폰은 D5로 spatial 버튼 미활성 표기**
  — 정직 원문).

## 4. 성공 판정

- WSL: music 앱 [spatial] 재생 → 상태 표기(pos 진행)+정지 — probe rc=0
  원문. vplayer 위임 회귀 없음(더블클릭 그대로). 캐논 전축(2n 흡수).
- 사용자 청안(육안의 청각판): WSL 또는 Windows에서 mp3/flac의 HRTF 위치
  변화 조작 — 사용자 선언만 결제(EYES).
- 폰: 캐논 등호+spatial 미활성 표기 원문.

## 5. 비-목표

- spatial-player 저장소 변경(코어 라이브러리는 이미 v1.0 — 무변경).
- m4a/aac/wma 디코더(조달/라이선스 — 별도).
- 플레이리스트/연속 재생/앨범 아트 — 백로그(스펙 2026-10-09-music §8 승계).
- 폰 OpenAL/디바이스 백엔드 재판정 — EYES 후속.