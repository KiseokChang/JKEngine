# jkdesktop 단일 APK — 완전 대체 축 v1 설계 (스펙)

> 일자: 2026-11-01 · 승인: 사용자(대면 결정 3건+설계 제시 승인 "넵")
> 근거: docs/101(조사)·docs/103(스파이크-B 리포트 — 4질문 전원 폐곡)·본 세션
> 실측 원문 전수. 스파이크 코드(engine/android/spike-b)는 폐기 라벨 유지 —
> 이 스펙은 개념·수형 원문만 승계한다.

## 1. 목표와 성공 판정

- **최종 그림**(DeX+voice vision 원존): 아이콘 하나 → DeX 꽉찬 jkdesktop(나만의
  OS) → 구두로 조작. Termux 의존 폐기(개발 절차용 의존만 잔류 허용 — §6).
- **성공 판정(v1)**: 폰 재부팅 후 아이콘 1회 탭 → DeX에서 데스크탑(배경+태스크바
  +런처 그리드) 기동 → 지뢰찾기(마우스+챠드)·뮤직(재생·태그)·터미널(sh)·채팅
  ([구두]=SpeechRecognizer)·갤러리·파일·설정 전부 jkdesktop 창으로 동작.
  sshd/X11/jkweb 수동 기동 불요.
- **성공 판정(v2·별도 라인)**: vplayer(MediaCodec+자체 MP4 디먹스 — 사용자
  결정 — "길 2"·ffmpeg 조달 0).

## 2. 구조 (docs/103 실측 승계)

- APK 하나(packageId 제안 `chang.jkdesktop`) — SDLActivity = 서 본체:
  - **서버** = SDLMain 스레드에서 엔진 `main --server`(SDLActivity 유일 화면 —
    SDL 창 1개 제한 원존). 렌더 = GL(accelerated, android 드라이버 — SW 힌트
    제외 수형 채용).
  - **클라/모듈** = 진짜 PIE 실행파일을 `lib*.so`로 포장(nativeLibraryDir exec —
    W^X 우회 실측 X_OK=1) + `lib*.so` 모듈 dlopen(r13/lib 접두 수형 채용).
  - **클라 렌더** = SW 렌더러를 **표면 직결**(SDL_CreateSoftwareRenderer —
    docs/103 핵심 실측: 창 경유 SDL_CreateRenderer는 원시 미착지 결함·표면
    직결은 정상) → 그림=표면 일직선 → 공유메모리 커밋 → 서버 합성. 타깃
    텍스처 경로 = NOP(스파이크 r31 수형 채용).
- **프로세스 격리 유지**(창 서버 계약 원존 — 앱 크래시 = 창 소멸만).
- **공유메모리/소켓/TempDir** = `$TMPDIR`(앱 캐시/파일 디렉 — SDLMain 심 시딩
  채용)·shm 이름화 폴백 원존·IPC AF_UNIX 원존.

## 3. 창 수명·DeX 전환 계약 (사용자 관측 절 신설)

- **DeX 창 모드**: 기본 = SDL 창 풀스크린 요청(시스템 타이틀바 소멸 → DeX
  화면 완전 점유 — "나만의 OS" 모양). 자유창 모드 = 설정 토글(옵트인).
- **이행 계약**: 폰↔DeX 전환·표면 재구성 = fit-to-display 재산정(기존 흐름 유지
  — 스파이크 원문 `desktop size changed to 1920x988` 흐름) + 폰 표면 = 풀
 스크린.
- **재진입**: 닫았다 다시 열기 = 가드 우회+endpoint unlink 재바인드(수형 채용)
  + 재진입 zombie(acceptor leak) 정리 — 본판에서 뒷정리 루틴 신설(원장행 승계).
- **수명**: 포그라운드 서비스(서버·음성 세션 소멸 방지)·Back 키 = 창 유지.
  BOOT 자동 기동 = 옵션(설정, v1 기본 OFF — 사용자 opt-in 계약 원존).

## 4. 입력·한글 계약

- 터치→마우스(엔진 흡수 — SDL_FINGER→Mouse 수형 채용), DeX 트랙패드=마우스
  원존, 좌표=창 정규화×표면 크기.
- **MouseChord 신설**: 왼+오른 보유 판별(시간 창) → 앱에 `MouseChord` 이벤트
  배달(기존 MouseDown/Up 병행 전달 — 호환). 첫 소비자 = 지뢰찾기(둘레 열기).
- IME/한글: 폰 소프트 키보드 → SDL 텍스트 이벤트 → 기존 한글자판(자동 조합)
  계승. SDLActivity의 텍스트박스(software keyboard) 훅 검증필.

## 5. 음성·LLM·네트워크 (사용자 결정 3건)

- **STT = Android SpeechRecognizer 단일 축**(사용자 결정 "3") — 마이크 캡처 코드
  ·ffmpeg 16k 정규화 leg = v1 제외. RECORD_AUDIO 권한 요청 흐름 봉입.
  Whisper 사슬(480MB 모델·vendor·GPU 스파이크 원장) = 코드·docs 보존, 미봉.
  추후 솔루션(예: ollama cloud 전사 엔드포인트)은 라인으로 승격(원장 표기).
- **TTS = 안드로이드 시스템 TTS API**(JNI 브리지 — termux-tts-speak 폐기) + 기존
  Win 축(SAPI) 유지.
- **LLM 계약 통일**: ChatRouter 근사 매칭(오프라인 기본·0.02s 실측 원존) →
  LLM 지정 시 HTTP 어댑터 1종: (a) 로컬 ollama(`ollama_host` 설정 — 원격 호스트
  계약 f1d57e6 재용) (b) **ollama cloud**(`https://ollama.com/api/chat` + Bearer
  API키 — 설정 저장). 폰 내장 모델 조달 0.
- **jkweb 폐기 → ChatRouter 스레드 in-process**: 동형 HTTP :8090 표면 유지
  (원격 시험·APK 갈아타기 안정화 채널로도 유용 — /talk 계약 유지).
- 3레그 배포 사슬은 "개발 절차"로만 잔류(§6) — 운영 의존에서 제외.

## 6. 저장·뮤직

- **저장권한 = MANAGE_EXTERNAL_STORAGE**(사용자 결정 "1") — 폰 Settings "모든
  파일 접근" 토글 1회 → 사용자 폴더(model·@음악 등) 그대로. 표준 저장소 프레임
  의존 0. 스토어 심사 고민 = 미로 전환 라인(사용자 표기 원존).
- **뮤직 v1 봉입**: spatial-player의 단일헤더 디코더 4종(dr_wav/dr_mp3/dr_flac/
  stb_vorbis — 원문: decoder.cpp) → NDK 크로스빌드 + **OpenAL(OpenSL ES 백엔드
  NDK 빌드)** + 태그/커버아트(엔진 own 사슬 승계).
- v1 런처 축: 지뢰찾기·테트리스·갤러리·파일·설정·메모·에이전트 관리자·채팅·
  뮤직·터미널(/system/bin/sh=mksh 셸)·워크숍·스크립트 .jkx·아이콘 편집·기타
  순수 앱. **vplayer = v2**(그리드 비로퇴 또는 "v2 준비중" 표기 — 구현 시 판정).
- lf/helix: 조달기(install_lf_helix_posix.sh)의 static 바이너리를 lib*.so 포장
  배포(실행 조달 계약 유지).

## 7. 개발·이동 절차

- **배포 = adb 무선**(본 세션 페어링 성립 — platform-tools 37·`install -r`·
  `am start` 원격 — 설치 탭 절사 확정).
- 셀프테스트/probe: 서버·클라 사슬을 ssh(Termux 개발용 잔존)+srv.err 로그
  채널(스파이크 채용 승계)로 수확. stdout→logcat의 타앱 필터 함정 원존.
- **Termux 스택**: APK 판정 완료까지 병행 유지(회귀 백업판) → 판정 후 사용자가
  폐기 결정.

## 8. 판정·잔여 원장행

- 캐논 축: Win/WSL/posix/폰(4축 유지) — APK 라인의 셀프테스트 게이트 병행.
- 파일 스코프: jkcore/jkserver/jkclient/shell+앱 모듈 전부 기존 트리 — APK
  계층(Java·JNI·gradle·조달) 신설.
- 잔여 원장행(docs/103 §4 승계): text.font_path 설정 봉질·프레임 정밀 측정·
  DeX fit-scale 재판정·재진입 zombie 정리·터미널 스폰의 apps-bin leg(lf/helix
  패키징)·오디오 금형(mixer 심 → 실물 SDL_mixer/OpenAL).

## 9. v1-밖 명시 (YAGNI)

- BOOT 자동 기동(옵트인 설정만 준비)·vplayer(길 2 — 별도 라인)·Whisper 재봉
  ·저장소 프레임(SAF)·스토어 서명/심사 절차·폰 내장 LLM.