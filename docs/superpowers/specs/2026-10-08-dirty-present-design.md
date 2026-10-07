# 스펙: 더티프레젠트 — 렌더→부분 blit→부분 X11 업로드 (2026-10-08)

발단(docs/78 §5.7 [compst] 실측 — 봉합 4종 착지 후 남은 본질): 폰 서버 합성
1회 = 레이어 blit 0.5ms vs **present ~70ms** — 93%가 SDL SW 렌더러
SDL_RenderPresent의 X11 **전체** 프레임 업로드. 커서 블링크 같은 8×16px
변화 1건에 1920×1080 전체를 올린다. 폰 idle 34.4%(합성 3/s=진짜 변화 뿐
이라 촉발 자체는 이미 소각됨)의 나머지 몫이 present.

코드 원문 발견(스펙 착수 실측 2026-10-08): **더티 rect가 이미 와이어에
흐른다** — `ipc::CommitSurfaceHeader.dirtyCount + DirtyRect[]`
(engine/include/ipc/JKWireProtocol.h:102-112). 클라는 실제 dirtyRects_를
보내는데 서버 JKWindowServer.cpp:2413-2425는 그 개수만 보고 폐기한다
(MarkDirty 플래그만). **신규 와이어 완전 불요** — 원료는 이미 도착.

## 목표

폰(서버 합성 SW 렌더러)에서 프레임 업로드가 **더 dirty rect에 비례**한다.
커서 블링크 1건의 upload가 전체 프레임(~70ms) → rect 사이즈(≈0.1ms 근처).
폰 idle 34.4% → 한 자릿수 목표. WSL idle 6.7% 기준선에 동형 이득.

## 설계 결정 (컨트롤러 — 2026-10-08 사용자 승인)

1. **범위 = 서버 합성기 단일 present 지점** — `JKCompositor::Composite`
   끝의 `SDL_RenderPresent(renderer_)`(engine/src/server/JKCompositor.cpp:331).
   클라 단독 모드(JKSDLRenderBackend::Present — Windows GPU) 접촉 0:
   GPU present는 더티 개념이 필요 없다. docs/78 §5.7 ④의 원칙
   ("Windows·WSL은 ACCELERATED 유지") 승계.
2. **더티 rect 출처 = 와이어 원용** — CommitSurface의 DirtyRect[]를 서버가
   수집(표면 좌표)→ dst rect와 layer scale로 **물리 화면 좌표**에 매핑.
   커밋 rect가 비어 있으면(레이어 플래그만 dirty) 레이어 dst rect 전체.
   레이어 위치/크기/표시/alpha/최대화 글리프 변화 = 이전∪새 dst rect.
   포커스 재정렬(SortLayers 결과 변화)·오버레이 훅 = **보수적: 전체
   프레젠트** — 부분 rect가 부채질하는 시각적 회귀를 막는 안전망.
   그램파 수준은 v1 백로그(단일 표=계측으로 실측 후 결정 — YAGNI).
3. **부분 업로드 경로 = SW 렌더러 전용 + fallback 사다리**:
   - SW 렌더러: 더 dirty rect마다 `SDL_RenderReadPixels`(백버퍼→버퍼) →
     창 표면(SDL_GetWindowSurface)에 행 복사 → `SDL_UpdateWindowSurfaceRects`.
   - GL/기타 또는 서피스 획득 실패 → **기존 SDL_RenderPresent** (기본 회귀
     없음 — fail-safe).
   - `JK_PRESENT_FORCE_FULL=1` 환경변수 = 구판 전체 프레젠트 강제
     (A/B 실측+진단 회귀 사다리).
   - **안전 근거**: 컴포지터는 합성마다 콜러가 배경을 비우고 전 레이어를
     다시 그리므로 백버퍼가 항상 "완전한 새 프레임"이다 — 제시 안 한
     부분은 마지막 제시 시점 내용과 결합해도 idempotent하다. 더티 rect의
     백버퍼 판독은 언제나 유효(백버퍼/표면 분리 재구성 불요).
4. **더티 rect 계산은 순수 함수화** — SDL 렌더러 없이 단정 가능한
   합집합/매핑/역치 로직(selftest 케이스 대상)과 SDL API 호출 본체를 분리.
5. **측정 계약** — [compst] 라인을 `present=full|dirty(N)`로 확장(전체 vs
   부분, rect 개수 표기, 소요 ms). A/B는 JK_PRESENT_FORCE_FULL 실측.
   폰 재배포(9-10분 빌드) 후 cpustat idle 전후 실측.

## 검증 계약

- selftest: 더티 계산 순수 함수 단정 — 표면rect→화면rect 매핑(스케일
  포함)·합집합 병합·역치(전체 전환)·빈 더티=제시 스킵·포커스/오버레이
  전체 케이스. 캐논 계보 기록(현 495/472/203·폰 472).
- WSL 서버 실측: JK_CPU_TRACE cpustat idle 전후(6.7% 기준선), [compst]
  present 수치 A/B.
- 폰 실측: cpustat idle 전후(34.4% 기준선) + 블링크/앱 launch 프레임
  [compst] 원문 — **사용자 육안 결제가 최종 관문**(가짜 결제 금지).
- Windows 무회귀: selftest 캐논 유지(서버 합성 SW 경로에 접촉 없음 확인).

## 범위 밖

- 클라 단독 모드(Windows) 렌더 경로·JKSDLRenderBackend 접촉 없음.
- 쓰리픽스(GL) 서버 경로의 partial present(GL texture 업로드는 전체가
  값싼 구조) — SW 전용 설계에 흡수.
- 커서 그리기·블링크 rect 수준 최적화(레이어 dst보다 촘촘한 단위) — v1
  백로그, 실측 결과가 요구할 때만.
- X11 직접 경로(XShm 등) — SDL 계약 위 경로에서 해소됨(UpdateWindowSurfaceRects가
  X11 뷰에 그친다 — 실측으로 검증).