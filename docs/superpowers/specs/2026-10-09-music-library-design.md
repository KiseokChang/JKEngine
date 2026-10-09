# 스펙: 뮤직 라이브러리 — 음악 모아보기 허브 (#90, 2026-10-09)

> **상태: 확정 — 설계 승인(2026-10-09 "물론이죠")+사용자 결정 3건.**
> 발단: 사용자 요청 — 갤러리(docs/87)와 같은 형태의 음악 모아보기 앱.
> 완성 후 2단계: `I:\progwork\spatial-player` 코어 라이브러리 연동(§6).

## 결정 원장 (사용자 확정 — 기각 시 즉시 수리)

- **D1 재생 = vplayer 위임**(3안 중 사용자 선택) — 뮤직 라이브러리는
  모아보기 허브(스캔·목록·필터)만 만들고, 트랙 재생은 vplayer가 한다.
  새 오디오 디코더 제작 없음. 재생 코스토(libavcodec)가 이미 있아
  중복 제작 절감. **spatial-player 연동 지점도 여기로 귀결** — 위임
  대상 교체만이 연동이다.
- **D2 스캔 = 파일명 스캔** — v1 메타데이터는 파일명·크기·수정시각만.
  ID3/태그·앨범 커버는 백로그(별도 태스크, 이 라인 밖).
- **D3 스캐닝 = 서브디렉터리 재귀 포함**(사용자 "물론이죠") —
  music.dirs의 각 루트 아래 트랙을 한 목록에 모은다.

## 1. 구조 — 갤러리 쌍둥이 (docs/87 원문 승계)

- **신설 모듈:** `engine/src/apps/ClientMusicApp.cpp`(ImGui 클라,
  어두운 루트창·16ms 타이머·OnThemeChanged·한글 데스크톱 폰트 —
  gallery 선례 "shot 구조 계승" 동형) + `engine/src/apps/
  JKAppModule_music.cpp`(meta: `music`/`Music`/560x520).
- **순수 부품 헤더:** `engine/include/apps/MusicModel.h` — `jk::music`
  네임스페이스(갤러리 `jk::gallery` 쌍둥이): **MusicDirList**, **NormalizeDirs**
  (기본 폴더 앞+유저 dirs 뒤·중복 제거·빈 성분 제거·settings 부재/파손
  =기본 1건 fail-safe), **ListAudioFiles**(재귀+확장자 필터+mtime
  desc 정렬·ec 중립형), **MatchFilter**(이름 부분일치, 대소문자 무시),
  **AudioDirFallback**({exeDir}/state/music — 기본 폴더 규약).
  갤러리의 캐시 키(Fnv1a)·FitThumb는 오디오엔 불요 — YAGNI.
- **빌드:** `jkapp_music` SHARED(win/posix 양축 — WIN32 게이트 없음)
  + `music.jkx` pack + jkx_packages 등록 — 갤러리 T1 CMake 원문 승계.

## 2. 스캔 (docs/87 문법 승계)

- **소스:** settings `music.dirs`(gallery.dirs와 같은 구조·같은 정규화).
- **확장자:** mp3/flac/wav/ogg/m4a/aac/wma (대소문자 무시).
- **재귀:** 각 루트에서 전 트리(하위폴더 포함) — D3. 목록 행에는
  파일명과 함께 루트 기준 상대경로를 표기한다 — 재귀 모음에서
  "어느 폴더의 파일인가"가 파일명만으론 안 보인다.
- **대용량 폴더:** 스캔은 **비동기 워커**(각 dir당 1회 풀 스캔 후
  캡슐 인가) = 갤러리 썸네일 비동기 도착 선례(idle 게이트 계약 준수 —
  도착 시에만 더티·#89 T2 계약).
- **스캔 실패:** 각 dir 독립 — 폴백 0건이어도 목록(빈 상태 안내)은 그린다
  (갤러리 셀프테스트 원문 계열).

## 3. UI — 목록 중심 (격자 없음)

- **상단 디렉토리 스트립**(갤러리 동형) + **트랙 리스트**:
  열 = [파일명(상대경로 토큰)] [크기] [수정시각].
- **이름 필터** 1행(MatchFilter) — 실시간 부분일치, 정렬은 mtime desc
  고정(v1).
- **더블클릭 = vplayer 재생**(§4) — 재생 중 표시·플레이리스트 연속 재생은
  v1 스코프 밖(vplayer가 자체 창과 상태를 갖는 유의미한 귀결).
- 컨트롤 클러스터는 갤러리 헤더 구성과 동일한 밴드 산식
  (ComputeChromeTitleBarHeight 문법 — docs/86 승격 원장).

## 4. 재생 위임 (D1)

- **런치:** 클라에서 `SendQuery("launch_app", {app:"vplayer"})` —
  라이브러리 앱 선례(ClientLibraryApp.cpp:246-255) 원문 재용.
- **경로 전달:** 런치된 vplayer 창에 기존 앱 도구 `open`(path, 비동기 —
  `ClientVPlayerApp.cpp:2613` 원문) 1콜. 경로 전달 배선은 **기존 vplayer
  모듈 무변경**이 원칙 — 도구가 있고 relay 경로(앱 도구 허브 —
  JKWindowServer appTool*)가 있어 신선 부품 추가 불요. 만약 클라→클라
  relay가 실측에서 안 되면 **vplayer 인자 지원 1행 추가**가 폴백(플랜 T에서
  실증 후 선정 — 신규 부품보다 원존 선호).
- **재선택:** 이미 vplayer 창이 열려 있으면 새 창을 만들지 않고 open 1콜
  (vplayer는 단일 클립 교차 — 계약상 자연 귀결). 창 핸들 취득은
  서버 창 목록 쿼리(실측 가능한 기존 챈넬) — 플랜 T에서 실증.

## 5. 테스트 — 갤러리 선례 전부 승계

- **selftest 2m 계열(main.cpp):** MusicDirList(기본+유저·부재/파손
  fail-safe)·ListAudioFiles(재귀+확장자+mtime 정렬)·MatchFilter(부분일치·
  대소문자)·NormalizeDirs — 갤러리 2g 쌍둥이 케이스 구성.
- **WSL probe:** 부팅→music 앱 창 존재+트랙 리스트 렌더 실측
  (wsl_gallery.sh 선례).
- **폰 probe:** phone_gallery.sh 선례 — 폰 selftest 캐논 흡수(2m 케이스
  전량)·모양 캡처는 EYES.
- **캐논:** 현재 Win 588/WSL 565/(posix 296)/폰 565 — **2m 케이스 신설
  케이던스로 계보 표기 의무**.

## 6. 2단계 — spatial-player 연동 (별도 라인·별도 스펙)

뮤직 라이브러리 v1 착지 후: spatial-player(`I:\progwork\spatial-player`
— OpenAL HRTF 3D 위치 사운드, 16-bit PCM WAV)의 재생부(spatial.cpp)를
코어 라이브러리로 봉인 → 뮤직 앱의 재생 leg 실험(vplayer 위임과 병행
또는 교체). 현재 WAV 전용이므로 우선 실측 대상은 WAV 재생 leg —
확장자 파이프라인(mp3 등)은 spatial-player 쪽 과제. **이 스펙의
약속은 지점만 남긴다(§4 위임 대상 교체) — 배선은 2단계 스펙이 안다.**

## 7. 성공 판정

- 설정한 dirs의 트랙이(재귀) 목록에 뜬다 — WSL 실측+폰 실측.
- 더블클릭 → vplayer 창이 열리고 클립이 재생된다(폰/WSL 실측 —
  EYES 최종). 캐논 전 축 신설 케이스 흡수 후 등가 성립.
- 클라 idle 스핀 계약(#89 docs/88) 준수 — 스캔 도중에만 더티,
  idle 0%.

## 8. 비-목표

- ID3/태그·앨범 커버·플레이리스트(m3u)·재생 중 연속 재생·레이트
  케이던스 — 전부 백로그.
- vplayer 자체 변경(도구/재생 코어) — 경로 전달 폴백(1행)만 예외.
- spatial-player 코어 라이브러리화 — 2단계 라인.