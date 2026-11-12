# 110 — 자율 세션 정산 (2026-11-12 새벽)

사용자 출근 중 자율 진행분. 모두 커밋 완료(푸시 전 로컬).

## 1. docs/108 서버 TU 추출 — 1~4단계 폐곡

| 단계 | 커밋 | 내용 |
|---|---|---|
| 1 | 156a748 | JKServerState.{h,cpp} — 경로·지문 헬퍼 5종 |
| 2 | 4d9ccc5 | IWindowServerServices + AgentWindowSnapshot |
| 3 | 78e0029 | **JKAgentHub.cpp — HandleAgentQuery 3,369줄 이동** (서버 9,649→6,192줄) |
| 3v | d3e425b | 실기기 검증 원문 |
| 4 | 9de0100 | 권한 매트릭스 불변식 3단정(셀프테스트) |

- 공유 헤더 `server/JKAgentPermissions.h`(42행) — 서버·허브 단일 진실원.
- 실기기: APK 설치 → `BUILD_SHA=9de0100` → `POST /talk {"text":"창 목록 보여줘"}`
  → `{"ok":true,...,"kind":"ListWindows","server":"{\"ok\":true,\"windows\":[]}"}`.
- 함정 원존 3종 기록(헤더 namespace 중첩·불완전 배열형 range-for·익명
  네임스페이스 헬퍼 링크 파산).

## 2. docs/106 UI 토큰·런처 봉합

- 1cf9f99: 테마 토큰 5종(라벨/툴팁/시계) + `theme::Generation()` 세대 캐시
  무효화 + 셀프테스트 106-3a~3d(대비 3:1). **kLight에서 런처 라벨이 사라지던
  결함** 봉합.
- 458e43c: 런처 그리드 리사이즈 재계산 · cols=0 UB · 라벨 셀 폭 클램프.
- f14ac64(docs/109): 텍스트 스케일 통일·크롬 메트릭 단일원 설계(다음 구현).

## 3. 상태·남은 일

- 셀프테스트 1,000 PASS 0 FAIL(PC). 커밋마다 게이트 통과.
- 안드로이드 APK = 테마/런처 반영 재빌드 진행(완료 시 설치·BUILD_SHA 기록).
- 다음 후보: ① 라벨 블릿 ×outputScale(HiDPI) ② JKChromeMetrics.h 배선+등호
  단정(헤더 초안 작성됨) ③ JNI 훅 단일 ABI 헤더 ④ JSON 이스케이프 21벌 통합
  ⑤ 하루 친구 데이터 원천(폰 실측 필요) ⑥ ruflo 샌드박스 파일럿(보고서 docs/107).
- 눈확인 대기(사용자): light 프리셋 런처 라벨 · DeX 창 모드 리사이즈 그리드 ·
  TV/폰 실기기 회귀.

## 4. Ruflo 세션 보고(CLAUDE.md 지침)

- 이번 세션 ruflo MCP 미가용: `jkdesktop` MCP 서버 연결 실패(세션 시작 시
  툴 스키마 미로드) — ruflo 도구로 오케스트레이션하지 못했다. 대체 = 내장
  Workflow/서브에이전트. ruflo 조사 보고서는 docs/107(원칙·동작·특성·파일럿
  제안) 참조.

## 5. 추가 커밋 (같은 세션 이어서)

| 커밋 | 내용 |
|---|---|
| 0599cd1 | 라벨 블릿 ×outputScale(HiDPI) · 시계 배율 명명 · `JKChromeMetrics.h` + 등호 단정(109-3) |
| 16a2541 | 태스크바 네비 셀 그림 분기 = 표시 문자열 → `NavGlyph` 역할 enum |
| ce63cf9 | 시계 위젯 onDynamicDraw 매 프레임 → 자리 이동/분 교차만 |
| 149eedd | 하루 정리 리더 순수 TU 승격(`apps/DaySummaryPrompt`) + 셀프테스트 day-1~3 |

- 최종 셀프테스트: **1,004 PASS 0 FAIL**.
- 지연: 테마/런처 반영 APK 재빌드는 메모리 부족으로 시스템이 중단(재시작은
  사용자 요청 시). 현 시점 폰 설치본 = BUILD_SHA `9de0100`(스텝 4까지).
- 남은 후보: JSON 이스케이프 21벌 통합(docs/106 §2-5) · JNI 훅 단일 헤더 ·
  태스크바/셸 텍스처 팩토리 · 하루 친구 폰 실측(사용자) · ruflo 파일럿(사용자 판정).

## 6. 2026-11-13 이어서 (배포·아이콘·통합)

| 커밋 | 내용 |
|---|---|
| 7d62e77 | 런처 아이콘 12종(신규 5·갱신 7) + 생성기 + APK 자산 동기화 |
| 710690a | 자산 스탬프(lastUpdateTime) + 재추출 전체 덮어쓰기 |
| 32b14b3 | lf/hx 셀 이중 등록 소각 · 내부 모듈(taskbar/filedlg) 셀 제외 · appName 중복 제거 |
| (동일 커밋) | 로그 수출 = Downloads 고정 파일 직접 쓰기(MediaStore 충돌 소각) |
| f5ddd7b | JSON 이스케이프 21벌 → json/JKJson 4계약 + 셀프테스트 2-5a~e |
| 941b2b7 | JNI 훅 선언 3복제 → include/jkhooks_android.h |

실기기: BUILD_SHA=941b2b7, /talk ListWindows·따옴표 포함 발화 왕복 정상,
icons 74장 추출, jk_srv.err.log 생성(로그 수출 복구). 셀프테스트 1,009 PASS.
