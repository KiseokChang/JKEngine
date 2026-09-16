# vplayer 전체화면 + 하단 호버 OSD 설계 (2026-09-17)

## 0. 요원 출처와 전제

사용자 요청(2026-09-17, 확정): "vplayer에 전체 화면 재생이 있었으면 해요. 그리고
seek라던지 영상위에 반투명 제어창이 있으면 좋겠어요. 마우스가 아래쪽 호버하면
나타나고 마우스가 안움직이고 좀 있거나 딴데로 빠지면 사라지면 좋겠구요."

- 사용자 출근 중 자율 진행 지시("재량껏 진행해 주세요. 끝나면 다음다음으로").
- 스펙 단독 작성 확정: 요구사항이 구체적이고 docs/50의 vplayer UX 선례(v2
  역방향 재생)처럼 사용자 전제 확정 시 컨트롤러가 결정을 기록하며 진행.

## 1. 문제 정의

vplayer는 항상 창 모드(경로/전송행 + 비디오 영역)로만 동작한다. 영화 감상처럼
큰 화면을 쓸 때 (a) 전체화면이 없고 (b) 제어행이 화면 위에 상시 붙어 있다.
요구: 전체화면 재생 + 비디오 위 반투명 하단 제어 스트립(OSD) — 하단 호버 시
표시, 무동작 잠시/커서 이탈 시 소멸.

## 2. 아키텍처 결정

### 2.1 전체화면 = 서버 레이어 상태 (maximize 계열)

서버가 레이어 단위 fullscreen 상태를 소유한다(docs/39 maximize의 형제):

- **preFsRects_** 상태 맵(presence = fullscreen) — preMaxRects_와 별개 맵.
  maximize/restore 맵에 섞으면 최대화 글리프·복원·데스크탑 리사이즈 재발행
  경로가 전부 얽힌다(M2a 이후의 교훈: 상태는 단일 소유, 형제 상태는 별도
  맵). 저장 값은 MaxState 동일(x/y/surfW/surfH/dispW/dispH).
- ToggleFullscreen: (a) 최대화 중이면 먼저 RestoreFromMaximize(Windows 관례 —
  최대화→전체화면 전환은 정상 크기에서 출발). (b) 전체 출력 크기로
  CommitChromeResize(작업 영역 예약 없음 — 전체화면은 작업표시줄을 덮는다,
  표준). (c) layer position (0,0) + 연결측 SetPosition(0,0). (d)
  layer.SetFullscreen(true). (e) window.fullscreen 이벤트.
  복원은 RestoreFromMaximize의 대응물(FullscreenRestore): 저장 rect 복원 +
  window.fullscreen_exit 이벤트. .bak 없음 — 화면 상태라 저장소 없음.
- **크롬 스킵**: fullscreen 레이어는 (i) TryChromeGrab 즉시 false(상단 24pt를
  포함한 모든 클릭이 앱에 도달 — vplayer 요구의 본질), (ii)
  DrawCloseOverlay/DrawMaximizeButton 스킵, (iii) UpdateChromeHoverCursor
  리사이즈 커서 스킵. 클릭 포커스는 1241행의 일반 경로라 살아 있음(전체화면
  비디오 클릭 = 일시정지 + 포커스).
- **데스크탑 리사이즈 재발행**: UpdateOutputBounds의 재발행 루프에 fullscreen
  레이어 추가 — CommitChromeResize(ww, wh)로 새 출력에 맞춤(예약 없음), 이벤트
  없음(maximize 재발행과 동일 규약).
- **단절 정리**: CleanupDisconnectedClients가 fullscreen 맵 항목을 erase
  (maximize와 동일 — 죽은 레이어의 저장 rect는 무의미).

### 2.2 토글 경로 3종 + 서버 도구 1종

- **서버 도구 `window_fullscreen {id?, on?}`** — kPermMatrix 행
  `("window_fullscreen","none","allow")`(focus_window와 동일 분류: 화면 상태
  변경일 뿐 승인 행위가 아님, 값 무력). id 생략 = 호출자 자기 창(창 클라이언트
  — vplayer가 쓰는 경로), 명시 id = 임의 창(스크립트/프로브 경로). control-only
  호출자의 생략형 = bad_request("no_window"). on 생략 = 현재 상태 반전.
- **vplayer 자체 트리거**: (1) F11 키(PreProcessMessage에서 Key 이벤트 —
  keyCode 확인 후 도구 호출), (2) 비디오 위 더블클릭(detail==2 — payload.detail
  로 클릭 수 전달 확인됨, windowed 모드 더블클릭은 무시), (3) OSD 버튼.
  vplayer는 항상 자기 창(id 생략)으로 호출 — 응답이
  `{"ok":true,"fullscreen":true|false}` 로 새 상태를 실어 오므로 클라 플래그가
  항상 최신(외부에서 명시 id로 바꾼 경우까지 커버: 응답이 실제 결과를 반영).
- **이벤트 카탈로그**: window.fullscreen / window.fullscreen_exit —
  docs/32 §7.5 카탈로그에 추가(스크립트 관찰용). PushMaximizeEvent 재사용.

### 2.3 vplayer 극장 모드 + OSD

- **fullscreenUi_ 플래그**(클라): 도구 응답으로 설정/해제. BuildUi 분기:
  - 극장 모드: 상단 경로/전송행 전부 숨김, 비디오를 표면 전체로 aspect-fit
    중앙(기존 코드의 avail가 표면 전체가 되는 것뿐 — 레터박스). 조그 노브는
    유지(기존 임계 통과). 루트의 어두운 클리어가 배경(레슨: 검은 막대).
  - OSD 스트립: 하단 고정 반투명 바(ImGui Begin NoDecoration + 전역 알파로
    페이드 — 창 단위 PushStyleColor alpha). 내용: Play/Pause, 시크 슬라이더
    (커밋-on-release, 기존 계약 동일), 시간/전체, 볼륨, 전체화면 해제 버튼.
    A/V 오프셋·렌더 게이지·열기 버튼은 창 모드 전용(극장 모드는 필수 제어만).
  - seekError/audioDeviceFailed 등 상태 문구는 OSD 위 1행으로 승격(에러는
    사라지면 안 되니까 — OSD 히든과 무관히 항상 렌더).
- **OSD 가시 규칙** (사용자 요구의 직역):
  - `visible = (커서가 하단 밴드 안) && (마지막 마우스 활동 후 < 2.5s)`
    - 하단 밴드 = 표면 높이 하단 22% + OSD 스트립 자체.
    - 활동 = MouseMove/클릭/휠 아무거나(ImGui io.MouseDelta != 0, WantCapture
      클릭, MouseWheel).
  - 커서가 밴드 밖으로 빠지면 즉시 소멸("딴데로 빠지면") — 아이들 타이머는
    밴드 안 정지 시만 유효.
  - 페이드: 200ms 선형 알파 보간(불쾌한 팝 방지). 히든 상태에서는 스트립이
    입력을 흡수하지 않게 Begin 조건부(알파 0 미만이면 스킵).
  - 슬라이더 드래그 중에는 항상 표시(드래그=활동이므로 자동 충족).
- **창 모드는 전혀 변하지 않는다** — 기존 행/전송/슬라이더 그대로. OSD는
  극장 모드 전용.

### 2.4 접근 경로 요약

| 표면 | 경로 |
|---|---|
| 사용자(전체화면 진입) | F11 / 비디오 더블클릭 |
| 사용자(전체화면 해제) | F11 / 더블클릭 / OSD 버튼 / Esc |
| vplayer 앱 | window_fullscreen 생략형(자기 창) |
| 스크립트/프로브 | window_fullscreen 명시 id + on 진위 |
| MCP 에이전트 | window_fullscreen(none gate, broker 통과) |

## 3. 비목표 (YAGNI)

- OS 진짜 전체화면(SDL_SetWindowFullscreen, 데스크탑 창 자체 전환) — 서버가
  모든 창의 컴포지터인 이 엔진에서는 레이어 전체화면이 같은 효과를 내며
  데스크탑 셸과의 정합(복원·이벤트·입력 매핑)을 유지한다. OS 전환은
  컴포지터 상태와 어긋나는 부수 결함을 만든다(문서화).
- 극장 모드에서의 재생목록/자막 — 요구 없음.
- 창 모드 OSD — 창 모드는 이미 제어행이 있다.
- OSD 페이드 애니메이션의 정밀 보간 — 200ms 선형이면 충분.

## 4. 리스크와 방어

- **시크/조그 상태와의 상호작용**: 극장 모드 OSD 슬라이더는 기존 커밋-on-release
  경로 재사용 — reverseActive_ 가드(docs/50 §10)도 동일 적용. 조그 노브와
  OSD가 겹치면(비디오 하단 우측) OSD가 위(스트립이 마지막 Begin) — 노브 드래그
  중 활동 타이머가 리셋되어 OSD가 유지되고, 두 컨트롤의 히트 영역이 겹치는
  충돌은 ImGui의 후술 우선으로 OSD가 이긴다(문서화된 수용).
- **전체화면 중 창 모드 복원 실패**: CommitChromeResize 실패 시 저장 rect 그대로
  유지되므로 재시도 없이 이벤트만 정직 발행 — maximize와 동일 운명 공유.
- **F11이 다른 앱에서도 눌리는 문제**: F11은 vplayer 포커스 시에만 도달(서버가
  포커스 레이어로 키 라우팅) — 전역 단축키 아님.
- **프로브 플레이크**: SendKeys F11 대신 도구 경로(window_fullscreen 명시 id)
  로 서버 상태를 구동 — vpt 계열의 SendKeys 플레이크 교훈(roadmap) 준수.
  OSD 시각 확인은 capture_window 스크린샷 + 픽셀 판정, 커서 이동은 SendInput
  절대좌표(기존 vpt10 패턴).

## 5. 검증 계획

- **vpt12 (신규, PS5.1 ASCII)**: vplayer 스폰 → agentctl로
  window_fullscreen(명시 id) on → list_windows에서 0,0 + 전체 크기 확인 +
  window.fullscreen 이벤트 수신 → 극장 모드 전환 확인(스크린샷: 상단 행 소멸) →
  커서 하단 이동 + 활동 → OSD 픽셀 변화 확인 → 2.5s 방치 후 OSD 소멸 확인 →
  on:false 복원 → 원 rect 복원 + window.fullscreen_exit 이벤트.
- **회귀**: vpt9(시크)/vpt4(재생 e2e)/vpt5(게이트)/vpt11(페이싱)/selftest +
  probe_agentmgr 등 도구 회귀(window_fullscreen이 kPermMatrix/jkagentd에
  추가되므로 jkagentd selftest 포함) + jkdesktop test.
- **게이트**: mtime(레슨 37) + 클린바이너리 공식런(레슨 61).

## 6. as-built

- 커밋: (실행 시 기입)
- docs: (실행 시 기입 — docs/50 §11)