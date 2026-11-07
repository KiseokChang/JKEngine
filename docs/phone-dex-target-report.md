# DeX 타깃 개봉 leg 실측 보고 (2026-11-07)

컨트롤 앱 "START DESKTOP (DEX)"이 "desktop launched on display 12"를 보고하지만
TV에 jkdesktop이 안 뜬 결함 라인 — 원인 2건(크래시 레이스 + DeX 표면 위계) 전부
실측으로 확정하고 정리.

## 결론 (한 장 요약)

| 항목 | 실측 결과 |
|---|---|
| TV DeX 표면 = logical display **2** (가상 Desktop, owner android uid 1000) | `dumpsys activity`: DeX 홈 런처(com.sec.android.app.desktoplauncher t269) 상주, 앱 소스 개봉 최종 성공 표면 |
| 컨트롤 앱이 보는 표면 = **[0, 12]** 뿐 (jkctl enum 로그) | `name='HDMI 화면' size=1920x1080`의 id 12 = 물리 RTK HDMI 계층 — **DeX 합성 아래 가려진다** |
| 창이 12에 프레젠트돼도 TV는 런처를 보여줌 | SurfaceFlinger 원문 `first frame available #18083` 그런데 TV 캡처=런처(평균 57.07 동일) |
| 셸 소스 `am start --display 2` | **수용** — 태스크가 2로 이동, TV에 jkdesktop 풀스크린 확인(평균 98.3, 아이콘 그리드 캡처) |
| 앱 소스 `am start --display 2` (Runtime.exec) | **무시 — Starting: Intent만 인쇄, 프로세스 미성립**(pidof-ok=false 실측, 크래시 0) |
| 앱 소스 ActivityOptions.setLaunchDisplayId(12) | 수용(태스크가 12로 이동)하지만 TV에서는 안 보인다 |

→ **DeX 진짜 표면(2)은 앱에 비가시** — 폰 단축 leg로는 12까지가 한계.
DeX 상시 사용 엔트리는 **DeX 홈의 JK 아이콘** (항상 display 2로 개봉 —
본 세션 이전의 모든 DeX 실전 세션이 실은 이 경로). 셸/adb 경로(--display 2)는
jkbridge가 소화 가능(차후 폰 게이트웨이 leg 후보).

## 결함 1 — 냉재기동 음성 콜백 크래시 (픽스 커밋 본 건)

force-stop→재기동 냉시작에서 PhoneControlActivity(폰)와 MainActivity(DeX)가
**병렬 부팅** → PhoneControlActivity.onCreate 즉시 JkVoice.begin →
SpeechRecognizer가 서비스 재기동 직전 상태에서 빠른 onError 발사 → SDL 부하
경로의 loadLibrary보다 먼저 `nVoiceError` 호출 → `UnsatisfiedLinkError` →
프로세스 전체 사망 → DeX 태스크 소멸 (crash buffer PID 19149 원문).

이전 부팅은 Main→SDL→lib 로드가 먼저 끝나 이 레이스가 안 드러난 것.

**픽스** (JkVoice.java):
1. `begin()`이 리스너 등록 전에 `System.loadLibrary("jkdesk")` (멱등).
2. onError/onResults 콜백 Throwable 가드 — 음성 실패가 프로세스를 죽이지 않음.

재발검증: 동일 냉시작 병렬 흐름 3회 반복 — FATAL 0, MainActivity 태스크 생존
(pid 24203/26167/268xx), engine boot 원문 정상.

## 결함 2 — 12 vs 2 표면 위계 (구조 파악, 앱 쪽 대응 불가)

- 컨트롤 leg를 `am --display 2 시도 → pidof 검증 → 실패 시 visible-HDMI(12)
  setLaunchDisplayId → 최종 폴백 기본 개봉` 순으로 정리. 앱 소스 am은 무시되므로
  실질 경로 = 12(폰 기준 외장) or 기본(폰 화면 0).
- 12는 DeX 꺼짐 상태에서는 일반 외장 HDMI → **DeX off일 때 컨트롤 leg가
  TV 풀스크린을 만든다** (DeX on일 때만 가려짐).

## 폐곡 상태

- TV에 jkdesktop 풀스크린(아이콘 그리드+벽지) — `tv_late.png` 실측.
- 컨트롤 앱: force-stop+재기동+폴백 정리 상태로 재설치 완료.
- 크래시 재발 0.

## 후보 다음 leg — 판정 정산 (2026-11-08)

1. jkbridge adb-wireless 셸 leg(무인복구) → **기각(사용자 판정 원존)**:
   실사용 환경(거실 TV+폰)에 **PC가 없다** — PC 의존 확대 leg 자체가
   구도 위반이라 폐쇄. 향후 재제안 금지(재발 방지 원존).
2. 엔진 자가 이동(가시성 실측) → 접어둠(부가 이득 미미 — 아래 3의 UX가
   재귀를 커버).
3. **DeX 복구의 설계 진실 = 성립**: jkdesktop이 죽어도 TV에는 DeX 환이
   남는다(위계 원존 — DeX 홈=표면 2 상재) → DeX 홈의 **JK 아이콘 한 탭 =
   정식 복구 경로**(본 세션 이전 모든 DeX 실전의 실제 진입점).
   구도 = **부팅은 TV 한 탭, 제어는 폰 구두** — 폰 단독 원칙 유지.
