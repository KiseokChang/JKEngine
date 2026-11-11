# 109 — 텍스트 스케일 통일 + 크롬 메트릭 단일원 (2026-11-12)

docs/106 §3의 남은 두 항목(텍스트 스케일 3갈래 / 크롬 기하 상수 3곳 중복)의
설계. 착수 전 판정 근거·순서·게이트만 확정하고 구현은 별도 커밋.

## 1. 원존 (docs/106 §3 원문 요약)

| 경로 | 현재 | 증상 |
|---|---|---|
| 런처 라벨(셸) | 텍스처 = 셀 메트릭, 블릿 = **1×** | HiDPI(s=2)에서 라벨만 절반 크기 |
| 툴팁(셸) | 텍스처 = 셀 메트릭, 블릿 = **×s** | 주변 UI와 크기 일치(정상 기준) |
| 시계(셸) | 라벨 텍스처 **×4 고정** | 의도된 확대 — 토큰 아님 |
| 태스크바 라벨 | 미니 아틀라스(자체 메트릭) | 셸과 래스터 경로 이중 |

원칙 판정: **논리 좌표 = 셀 메트릭 1벌**, 물리 = `×outputScale` 블릿.
텍스처는 항상 셀 메트릭 해상도로 굽고, 그리는 쪽이 `s`를 곱한다(툴팁이
이미 그 계약 — 라벨을 그쪽으로 맞춘다).

## 2. 수형

### 2.1 라벨 블릿 = 툴팁 산식 (1줄 계열 수정)
`Draw()`의 라벨 dst를 `labelTexW*s`, `labelTexH*s`로. 셀 폭 클램프(458e43c)는
물리 폭 `icon.rect.w*s` 기준으로 유지 — 순서: `×s` → 클램프 → 중앙 정렬.

### 2.2 시계 = 토큰화된 배율
`const int scale = 4;` → `kClockScale` 상수 + 주석(의도 명시). 텍스처는
`(font_scale × outputScale)`로 굽는 대신 지금처럼 라벨 텍스처 재사용 + 블릿
배율 유지(굽기 해상도 이득 없음 — 벡터 아틀라스가 이미 스케일 독립).

### 2.3 텍스처 팩토리 (후속 — 이번 범위 밖)
셸 `TextTextureEx`와 태스크바 `MiniTextTexture`의 이중 래스터는 `jk::text::
TextureFactory`로 수렴(docs/106 §3). 이번 커밋은 계약 정렬까지만.

## 3. 크롬 메트릭 단일원 (2-6)

현재 3곳: `JKWindow.cpp`(kBorder=2·kTitle), 컴포지터 페인트, 서버 히트테스트.
수형 = `include/JKChromeMetrics.h`:

```cpp
namespace jk::chrome {
inline constexpr int kBorder = 2;
// kTitle = 셀 메트릭 연동(플랫폼값 베이크 아님 — JKWindow 주석 원존).
int TitleHeight();   // GetCellMetrics 기반
inline JKRect ClientRect(const JKRect& win);
}
```
셀프테스트: `JKWindow::GetClientRect()` == `chrome::ClientRect()` 등호(승격).

## 4. 순서·게이트

1. 라벨 블릿 ×s + 클램프 순서 정리 → PC 셀프테스트(993+) + s=2 눈확인.
2. 시계 kClockScale 명명 → 무동작(빌드만).
3. JKChromeMetrics.h + 등호 단정 → 1커밋.
4. 안드로이드 APK 1회 배포(DeX 표면 = s>1 실측 지점).

금지: 3과 1을 한 커밋에 섞지 말 것(원인 격리 — docs/108 §3 게이트 승계).
