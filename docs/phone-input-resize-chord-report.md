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