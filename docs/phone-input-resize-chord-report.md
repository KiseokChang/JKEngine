# APK 입력 라인 as-built — DeX 내부 창 리사이즈 2차 진원 + 챠드 게이트 (2026-11-04)

세션: jkengine 메인(2026-11-04 오전~낮). 시작 = 사용자 보고 "dex 내부 리사이징 문제요" →
수형 두 건 폐곡+결제("잘 되는듯", "더블 클릭 잘되네요").

## 1. DeX 내부 창 리사이즈 2차 진원 수형 — 063d029

**1차 원장 정정**: "핸드셰이크 미발사"는 부정확 — 어제 세션 srv.err 직독에서
지뢰창 `renderer re-bound` 4건 존재(320x568·732x568·816x380·816x567).
재결합(8b81569)·shm 리맵 전부 작동했고 파손 클라도 살아서 파손 이미지를
shm에 쓰고 있었다(유령 창 아님 — 프로세스 맵+shm 이름으로 클라 매칭).

**진원**: r30 우회 수형(SW 렌더러가 숨은 SDL 창 표면에 직결 그림 — 타깃
더미 핸들 NO-OP, JKSDLRenderBackend.cpp)과 리사이즈의 충돌. SizeChanged를
받아도 `SDL_SetWindowSize`가 0건 → 창 표면 = 기동 크기(예 320x568) 피치
고정:

1. 그림 = 신규 816좌표가 320폭 표면에 클립(좌측만 착지).
2. `RenderReadPixels(null)` = SW 렌더러 출력(=창 표면) 크기까지만 읽음,
   dst는 신규 w*4 → pixelBuffer 잔재가 구 피치로 재해석 = **격자 타일
   무늬**(관측 원문 = 구 320x568 이미지 486,400B/3264 ≈ 150행의 휘발
   밴드)+0(투명 → 배경 비침).
3. RenderAndCommit은 shm에 전체 memcpy → 파손 이미지 완전 서거.
4. pixelBuffer 초과 쓰기 구조 = 클라 Segmentation fault(srv.err 원문)
   와 같은 축.

**수형**: SizeChanged 훅(8b81569 존속)의 DestroyRenderer 앞
`SDL_SetWindowSize(hiddenWindow_, 신규 w, h)` 1행. 더미 드라이버 창
표면이 `SDL_OnWindowResized`의 surface_valid=FALSE 계약으로 신규 크기
프레임버퍼 재생성(SDL 소스 SDL_video.c:2779/:3162 검증; 부팅
GetWindowSurface 실측으로 드라이버 능력 확인済).

**폰 실측 폐곡**: 드래그 리사이즈 6연속 re-bound(821x380..1184x694, 새
세대 shm 3_7까지 리맵)·SEGV 0·shm 3_7 풀오파크(알파0 **0.0%**)·DeX
스크린캡처 정상 격자. **사용자 결제 "잘 되는듯"**.

## 2. 챠드 게이트 — 더블클릭 챠드 신설 (게이트: IsDoubleTapChordGesture)

사용자 보고: "깃발 되는데 좌우동시클릭은 안먹음" (DeX 트랙패드).

**[mouse] 계측 원문**(T3 진단 빌드·[ime] 훅 무조건 선례 — 폐곡 후
s_inputGateTrace 게이트로 접음): 깃발 = `button=3 clicks=1` 순도착 ✓.
챠드 시도 구간 = `button=3` **0건**, 대신 `button=1 clicks=1,2,3,4`
연속 left down/up — **DeX 트랙패드는 좌우 동시 버튼 상태를 만들어주지
못한다**(두 번째 누름이 또 left로 분류). 구조: 트랙패드는 물리 버튼이
없어 OS 합성기가 접촉 집합을 제스처 단위로 하나의 분류로만 골라 배달
(2 손가락 탭 = right, 그 외 = left/드래그) — 실물 마우스의 HID 2버튼
동시 비트는 표현 불가.

**채택**: Windows 지뢰찾기 정통 **"안전 더블클릭 챠드"** — 열린 숫자
셀에서 `clicks>=2` 좌클릭 = 둘레 열기(`TryChordAt` 재용, 불성립(닫힌
셀 등 ChordReveal 거부)면 일반 개방 통과). 기존 계약 무변조:

- 실물 마우스 좌+우 챠드(앱 로직 leftDown_/rightDown_) 원존 유지
- 폰 화면 두 손가락 터치 챠드(spike 수형 — SDL_FINGER held==2 → right
  합성, "[jkdesk] touch chord synth (right)") 원존 유지
- 클릭 수 신규 실계: `TranslateSDLEvent` MouseDown/Up이
  `ev.keyCode = sdl.button.clicks`를 실게 한다(마우스 이벤트의 keyCode
  는 이 게이트가 첫 소비자 — 와이어 client 매핑(detail=버튼·
  keyCode=클릭 수)도 동일 규약 — JKClientSurface.cpp 원존)

**셀프테스트**: 등호 케이스 4건(좌2/좌4=발화·좌1=개방·우2=깃발 유지) —
Win축 960 → **965 PASS 0 failure**.

## 3. 기존 결함 수리 — 1n-f8 (구식 셀프테스트 견본)

HEAD(960)에서도 FAIL — pre-existing 확정(스톤 루프 제어 실험). 원인:
#124 r6/r9에서 스키마 라인이 browser·btype으로 확장됐는데 1n-f8 견본이
구형 `launch / close / focus / list / talk`로 얼어파산. 실착지 =
ChatRouter.cpp ActionSchemaLine(). 진짜 계약(무따옴표·무파이프 — ollama
재인용 수리)은 현재 스키마가 여전히 충족 — 견본만 현행으로 갱신.

## 4. 캐논·렛슨

- Win 셀프테스트 **965 PASS / 0 failure** (960 베이스 + 챠드 4 + 1n-f8
  수리 1). 캐논 이동 = 계획된 신설(+5). WSL/posix/폰 축 재측정은 다음
  접촉 원장행(같은 main.cpp 공유 — 등호 상존 예상).
- 렛슨: 이 세션의 APK 진단 루프 = srv.err(run-as cat)이 유일한 엔진
  원문 채널(logcat 아님 — 로그 리네임해도 fd로 기록 계속)·apk `input`
  주입 `-d`/`wm size -d` 전부 이 펌웨어 차단 → DeX 표시 입력은 사용자
  실측 협조(드래그·더블클릭 원문 결제 루프)가 정석.
- 명령 함정 재발 2건: MinGW 콘솔 한글 → CP949(curl --data-binary
  @file 회피)·`jkdesktop.exe selftest`는 무효 — 서브커맨드 `test`.
## 5. 2차 배치(2026-11-04 오후) — 최대화 후 깃발 소실 + DeX 캡션 가림 (6cdbbb3 이후)

사용자 보고: ①"최대화 한 다음 오른쪽 버튼(깃발) 안됨" ②"JKENGINE Window Server
타이틀바에 지뢰찾기 앱 상단 가림" ③깃발은 더블클릭 빌드에서 성립.

**진단 원문(계측 빌드)**: [mine] 앱 측 추적 — 우클릭(btn=3)은 도착·inside=1·
셀 판정 정상 — 그러나 shm에 깃발/물음표 **0건**(배경 적표 스크린 대조 포함).

**진원 ①(깃발 소실 = 캐시 텍스처 UAF)**: 재결합 훅(8b81569)의
SDL_DestroyRenderer가 **렌더러에 얹힌 모든 SDL 텍스처를 통째로 파괴**하는데
JKResourceCache::FlushUploads는 업로드 성립 시 **소스(SDL_Surface/rgba)를
자유시켜** 버렸다(:146-148) — 재결합 뒤 캐시 핸들 = 죽은 지문; 프리미티브
(칸·숫자 텍스트)는 살아있고 아이콘(깃발·물음표·지뢰)만 소실 — 어쩔 땐
작동(초반 깃발 성립 원존)·어쩔 땐 크래시(어제 클라 SEGV) = UAF 시그니처.

수형(JKResourceCache): **소스 보존 계약** — 업로드 성립분을 retained_로
이동(자유 철수)·신설 **ReinstateImages()**(클라 재결합 훅이 호출): 구
렌더러 핸들은 이미 소거 때 죽었으므로 pendingDestroys_ 소각(dangling
파괴 방지)+retained_ → pending_ 재큐 → 다음 flush가 재업로드. Unload /
소멸자 사거리 보존 분 소유 정리 포함.

**진원 ②(DeX 캡션 가림 = 드라이버 게이트 1자)**: [dex-diag] 적표 실험
(표면 최상·하단 8행 적표 — 하단이 화면 1036..1042에 착지 = 표면 실위치
y≈54, **DeX 캡션이 표면 상단을 덮음 원문 확정**) + **부트 로그에
"[jkdesk] Dex fullscreen" 행 부재** = 풀스크린 leg가 미발사인 것 발견
→ **SDL 드라이버 명 실측 = 'Android'(대문자)** — 'android' 소문자만
비교한 게이트 3곳(풀스크린·터치 챠드 합성·소프트웨어 렌더러 힌트)이
전부 미발사. 수형 = SDL_strcasecmp 정규화(3곳).

폰 실측 폐곡: **Dex fullscreen requested**(부트 원문)·fit 1920x1080
등호·캡션 스트립 화면 소멸(스크린캡처)·최대화 상단 온전
·깃발 성립 — **사용자 결제 "자되요"(2026-11-04)**. 임시 계측
([mine]/[dex-diag] 적표는 철수, [mouse]는 [input] 게이트 잔존.

**렛슨**: SDL android 드라이버 명 = 'Android' 대문자 사건 — 드라이버
명 비교는 strcasecmp 원칙. DeX 창모드의 캡션 = SDL이 보더를 모르는
inset(적표 실험 원문) — 풀스크린(스펙 §3)이 정답, 자유창 모드는 별도
라인(SDL3 승인 축과 병행 재판).
