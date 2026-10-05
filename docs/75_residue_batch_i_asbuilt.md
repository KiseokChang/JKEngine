# docs/75 — 플랜 I as-built: 소액 잔여 전량 소각 + 지뢰찾기 수리 + 트러스트 메모 (2026-10-05)

세션 원장: a5ad560(docs/73 후속) → 36c7de2, 커밋 8건. 사용자 지시 "순서댈 다
처리해줘요"(추천 순서 ①트러스트 메모 ②MANI 수리 ③O6 ④소박 묶음) + 중간
버그 보고(지뢰찾기) 즉시 루프. 플랜: docs/superpowers/plans/
2026-10-05-residue-batch-i.md.

## 1. 완결 매핑

| 항목 | 커밋 | 실측 |
|---|---|---|
| **트러스트 문 결정 메모** (docs/74) — 3 후보 비교, 능력 선언형(A)+fail-closed 추천, 결정 3항목 | caad290 | 리뷰어 정직성 판정 합격(선례 :3986-4055/:7044-7062 실측; capabilities= 는 "결정 전 비구속 설계 스케치"로만 기술) |
| ★**지뢰찾기 난이도 전환 그리드 창 벗어남** (사용자 보고) — 클라 배치에 C→S 자기 리사이즈 채널 부재가 근원 | 128f409 | 라이브 probe: 레지스트리 320×380 정지(버그) → 408×466→744×466( 산식 24+cols·24 / 82+rows·24 정확 일치); **사용자 눈확인 완료(설계 의도대로)** |
| **I2 MANI 필드 보존** — `jk::JkxManifestMerge(authored, regenerated)` 순수 함수(2-pass 행 재조립) + RunJkxPack 스크립트 분기 배선 | b7a76a3 | 셀프테스트 13 check; gate⑤ 실측: `jkx-pack workshop` 재판 → MANI에 `scriptfile=`/`watch=` 잔존(grep 1/1) |
| **I3 O6 스크롤백 잔여** — 선택 좌표 전체 행 공간 전환(`ViewportRowToFull`/`NormalizeSelFull`+`CellFromPointFull`+스크롤백 paint 하이라이트+복사 접근자) + 비SGR 휠 클래식 신고(`EncodeWheelX10` Cb 64/65) | 6b41739 | 신설 check 6 PASS; 리포트 경로(뷰포트 좌표)와 선택 매핑 공간 분리 검증 |
| **I4 소박 6건** — ①closeHover_ 호버 페인트 ②[vpt12] fprintf 소각 ③2KB 병합 보류 stderr 가시화 ④WideToUtf8 공용화(신설 아님 — 기존 `jk::text::Utf16ToUtf8`/`Utf8ToUtf16`로 2복각+jkchat 16콜사이트 흡수) ⑤폴백 글리프 18 cp(더블라인 11+반쪽 4+셰이드 3) ⑥클래식 모션 Cb btn+32(호버 35) 봉합 | 36c7de2 | 접촉 TU 7종 오브젝트 RC=0; 승계 어설션 `classic motion = Cb btn+32` PASS |
| 니트 소화 (컨트롤러 직접) | 9421fa4, 609d98b, rider 커밋(본 문서) | 아이콘 이중등록 주석·overflow 밀림/리플로우 근사 주석·docs/74 시제/행번호 |

## 2. 설계 원리 (why)

- **지뢰찾기**: 윈도 서버가 표면 지오메트리의 진실원. 클라 로컬
  `SetWindowRect`는 표면 밖 오버플로우일 뿐(사용자 보고 = 정직한 관측).
  신규 와이어 메시지 대신 기존 서버 툴 `window_resize`(id 생략=자기 창)를
  `SendAgentQuery`로 재용(1-in-flight; 다음 클릭이 재요청 — 최종상태 정합).
  콜백 미설정(싱글 프로세스)이면 기존 로컬 경로 — 무영향.
- **I2**: docs/67 단 2 룰링 "화이트리스트 → 필드 보존". authored 행 순서
  원문 유지 + canonical 키 행 치환 + reg 미보유 키 꼬리 추가. 컨트롤러
  승인 편차: dup authored 행은 각각 치환(계약 면 정확), replace-루프 대신
  2-pass 재조립(계획서 스스로 지시한 오프셋 어긋남 회피).
- **I3**: 선택은 전체 행 공간(스크롤백 deque 인덱스와 라이브 행의 합산
  인덱스), 리포트는 뷰포트 그대로 — 두 좌표계의 목적 분리가 혼용을 막는다.
  off=0에서 구 live 선택과 동치(기존 검증 유지).
- **I4**: 공용화는 "신설"이 아니라 "기존 계약으로의 흡수" — `WideToUtf8`
  복각은 WCTM(CP_UTF8,0)이므로 `jk::text::Utf16ToUtf8` 바이트 동일 계약.
  jkchat의 Utf8ToWide는 MB_ERR_INVALID_CHARS fail-closed로 이전보다 엄격
  (입력이 전부 엔진 산출 유효 UTF-8 — 파일 헤더에 관측 차이 없음 기록).

## 3. 최종 리뷰 (오퍼스)

**APPROVE WITH NITS — push 가능.** 횡단 검증: I3×I4-6×I4-5 동일 파일 3중
접촉 봉쇄(리포트/선택 게이트 상호배타 :703, 휠 분기 쌍둥이 구조, 더블
무효화 0). 승인 편차 6건(위 원리에 흡수). 라이더 소화: docs/74 행번호
(:7044-7062)+시제 갱신, EOF 개행 3건, `engine/tmp/i2_merge_selftest.cpp`
소각(하니스 역할 완료 — 셀프테스트가 계약의 소유자).

### 니트 원장(눈확인/이후 작업)
- **지뢰찾기 화면 가장자리**: 리사이즈는 위치 불변(서버 정책, window_move와
  일관) — B→E 성장 시 창이 우하단 가장자리에 붙으면 일부 화면 밖.
  수동 리사이즈와 동일한 기존 정책. 원장 기록만(정책 변경 아님).
- **E→B 축소 방향** 미실측(같은 CommitChromeResize 기구라 리스크 낮음) —
  눈확인 1줄 요청 예정.
- **closeHover**: 표면 이탈 시 OUT 전이 지연(클라 서브 모드, MouseLeave
  이벤트 부재 — 라우팅 한계, 문서화됨); 컴포지터 오버레이 복제 = I4
  원장(서버 측 상태).
- **셰이드 글리프** 폴백 경로 1px FillRect ~96회/셀 — 폴백 전용(아틀라스
  없을 때만)이라 v1 수용.
- **코너 교차 틱**: 폴백 팔-시작 규약의 표준 근사 — 눈확인 목록.
- **1-in-flight 가드 무타임아웃**: 응답 전부 도착하는 실측(에러 경로 포함)
  + 64 캡 큐 — 재접속 유실 시에도 다음 클릭 재요청이 회복.
- **2KB 스키마 한도 상향**: 정책 결정 유보(스펙 docs/58 유지) — 사용자 몫.
- **vpt12_fullscreen.ps1:253 [vpt12] grep** 이제 빈 출력(fprintf 소각) —
  판정은 픽셀 기반이라 무영향, 프로브 소유 정리 항목.
- **스크롤백 재앙커링**(overflow 밀림 정밀화) — 코드 주석 "이후 작업 후보".

## 4. 게이트 원장

- 빌드 사이클: 라이브 스택 정지 → ninja RC=0 → `jkdesktop.exe test`
  0 failure(s) → 복원 → ping GREEN ×3 사이클(minefix/I2/I3+I4 통합).
- 실측 도구: `diag_mine_diff.ps1`(read-only+agent 도구 추동, 스택/권한 무접촉).
- 렛슨 재확인: PowerShell은 JSON 이중 따옴표를 떨어뜨린다 — 사용자 커맨드는
  `{\"k\":..}` 이중 이스케이프 표기로만 안내(이번 세션 사용자 bad_request 사례).

## 5. 다음 (사용자 몫)

1. **docs/74 트러스트 결정** (1-3항목) — 단 2 출하 라인 개통의 문.
2. 눈확인 대기: 터미널 글리프 18 cp, close 버튼 호버, 지뢰찾기 E→B 축소.
3. 결정 시 → docs/74 §5 경로(설계 스펙 → 플랜 → 능력 게이트+배지).
## 3b. 라이더 — 서버 컴포지터 X 오버레이 호버 (f46f27b, 2026-10-05)

사용자 눈확인 보고("X 에 호버 했는데 아무 변화 없는데요?") → 라이브 진단+수리.

- **원인**: I4-1에서 closeHover_를 클라 페인트(JKWindow.cpp)에만 심고, 서버
  모드(split)에서 보이는 X는 컴포지터 DrawCloseOverlay — 상태 없이 그려
  호버 무변화. 본래 I4 원장의 "컴포지터 오버레이 복제 out-of-scope" 항목.
- **수리**: 상태 판정은 서버(JKWindowServer::UpdateCloseHover — HitTest·
  면제 검사·존 산식 전부 TryChromeGrab 존 1과 동일), 컴포지터는
  SetCloseHoverLayer(id) 미러 그리기(docs/39 maximize 플래그 동일 방향).
  전이 시에만 Composite() 1회. SDL_WINDOWEVENT_LEAVE에서 확정 소거
  (서버 창 밖 이탈 = 모션 부재 프리즈 소각).
- **실측**(diag_ch2.ps1, 마우스 모션 실전 주입): OUT→ON 픽셀 diff 108/169,
  ON→OFF 108/169 — ON/OFF 대칭, 정상. 단일 프로세스 모드는 클라 closeHover_
  (I4-1) 그대로.
- **진단 트랩 원장(실측 프로브 렛슨 — 다음 대기 실측에 그대로 적용)**:
  1. `capture_window`는 클라 shm 리드백 — 서버 그림 오버레이는 안 나온다.
     서버 측 시각 검증은 실물 화면 캡처(shot.ps1 패턴)만 유효.
  2. CopyFromScreen 캡처의 원점은 **윈도 rect** 좌상단(비클라 영역 포함,
     125% 데스크톱에서 +9,+50) — 화면 좌표로 샘플하면 수십 px 어긋나
     "변화 없음" 오판. image = screen - GetWindowRect 좌상단 수학 필수.
  3. `-WindowStyle Hidden`으로 서버 기동하면 SDL 창까지 SW_HIDE 되어
     GetClientRect 0x0 — 리다이렉트 캡처가 필요하면 **보통 창 +
     -RedirectStandardError/StandardOutput**으로 기동. (로그는 stdout,
     fprintf(stderr)는 .err 파일.)
  4. SetCursorPos가 커서를 점프시킨 뒤 모션 이벤트는 1회(연속 유지 모션
     없음) — 호버는 전이 1회 발화 후 프리즈가 정상 동작. 또한 이전 프로브
     실행이 커서를 X 위에 남겨두면 다음 실행의 before 샷이 이미 호버 상태
     (스폰 좌표 고정이라 동일 위치) — 비교 기준 샷은 "커서를 보드 중앙으로
     옮긴 뒤" 찍는다.
