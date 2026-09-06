# 터미널 기능 로드맵 — 구현 상태 감사 + 단계별 계획

> 2026-09-06. "터미널 에뮬레이터에 필요한 기능" 브레인스토밍(외부 LLM 대화)을
> 우리 코드베이스 실제 상태와 대조한 감사 문서. 기준 시점: 스크롤백 구현 직후
> (docs/22 Phase 1 완료 + 스크롤백 1차). 상위 설계는 `22_sdl2_terminal_conpty.md`.

---

## 1. 용어 → 우리 코드 대응표

외부 설명에서 나온 개념이 우리 코드에서 어디에 해당하는지. 이 대응표를 알면
외부에서 본 터미널 구현 글/코드를 우리 코드베이스로 번역할 수 있다.

| 외부 용어 | 우리 구현 | 파일 |
|---|---|---|
| **ConPTY** (`CreatePseudoConsole`) — 파워셸을 검은 창 대신 내 프로그램에 연결 | `JKConPtyBridge`: 파이프 2쌍 생성 → 가상 콘솔 생성 → 셸 프로세스 기동. 출력은 전용 리더 스레드가 버퍼에 적체, 입력은 `WriteFile` | `src/terminal/JKConPtyBridge.cpp` |
| **VT/ANSI 스트림 파싱** — `\x1b[31m` 같은 숨은 암호 해독 | `JKVtParser`: 손수 만든 상태 머신(Ground/Esc/Csi/Osc). 색(SGR), 커서 이동, 화면 지우기, 스크롤 영역, 대체 화면, UTF-8 해독 | `src/terminal/JKVtParser.cpp` |
| **텍스트 그리드** — 바둑판 셀 모델 | `JKTerminalGrid` + `JKTermCell{cp, fg, bg, attrs, width}`. 커서/스크롤 영역/alt 화면/dirty 비트/스크롤백 | `src/terminal/JKTerminalGrid.cpp` |
| **오프스크린 surface** — 안 보이는 도화지에 다 그린 뒤 한 번에 제출 | 우리는 더 나아가 **클라이언트 프로세스**가 숨은 SDL 렌더러 타깃 텍스처에 씬을 그리고 `SDL_RenderReadPixels`로 픽셀을 읽어 **공유 메모리**에 복사 → 서버가 합성. 외부 설명의 "더블 버퍼링"의 분리 프로세스 버전 | `src/client/JKClientApplication.cpp` |
| **폰트 렌더링 API** (Gemini 설명은 DirectWrite) | `JKGlyphAtlas`: stb_truetype으로 글자를 비트맵으로 굽고 `(글자,색,굵기)` 키로 GPU 텍스처에 캐싱 → 셀마다 블립 | `src/terminal/JKGlyphAtlas.cpp` |
| **dll/jkx 포장** | `jkapp_terminal.dll`(C ABI 모듈) + `terminal.jkx` 단일 파일 패키지, 런처/서버 스폰 연동 | `src/apps/`, CMake `jkx-pack` |
| **리사이즈 시 PTY 알림** (리눅스 SIGWINCH) | 서버가 창 크기 변경 → `ResizeSurface` 메시지 → 클라 remap → `ResizePseudoConsole` 호출 | `JKWindowServer.cpp`, `JKConPtyBridge::Resize` |

**주의**: 외부 설명(특히 Gemini 답변)은 Win32 단일 창(WM_CHAR/Direct2D) 가정이라
코드를 그대로 이식할 수 없다. 개념만 가져오고 SDL 이벤트 + 서버/클라 분리 +
파이프 IPC 구조로 번역해야 한다.

---

## 2. 구현 상태 감사

### 2.1 구현 완료

| 기능 | 비고 |
|---|---|
| ConPTY 브릿지 전체 | 생성/입력/출력/리사이즈/셸 종료 감지, GetProcAddress 폴백 |
| VT 파서 Phase 1 스코프 | SGR(16/256/truecolor), CUP, ED/EL, DECSTBM, alt 화면 1049, OSC 0 타이틀, DSR 6 응답, UTF-8, 지연 랩 |
| 그리드 + alt 화면 + 커서 저장/복원 | 셀프테스트 골든 시나리오로 검증 |
| 입력 매핑 | 화살표/F1–F12/Home/End/PgUp/PgDn/Ins/Del/Tab·Shift-Tab/Esc/Enter/BS(DEL)/Ctrl+문자/Alt 접두. 수정자 상태는 `ev.option`으로 서버→클라 전달 |
| 스크롤백 1차 | top-margin 스크롤 행 스냅샷 1000줄, 휠 노치당 3행, 키 입력 시 live 복귀, alt/마진 스크롤 제외, RIS 해제 |
| 프레임 게이트 | 출력 없으면 렌더 스킵(대기 CPU ≈ 0) |
| 수명주기 | 지연 pty 기동(레이아웃 확정 후), 셸 종료→창 종료, 서버 닫기→정리 |
| 패키징/서버 연동 | `.jkx` 자동 리팩, 런처 스폰, 서버 주도 리사이즈 → 셸 재출력 |

### 2.2 부분 구현 (미진)

| 기능 | 현재 상태 | 빠진 것 |
|---|---|---|
| 스크롤백 | 스냅샷 폭 고정 | 리플로우(줄 재조립) 없음, 스크롤 중 신규 출력 시 앵커 미고정(뷰가 따라감), 스크롤바 표시 없음, alt 화면에서 휠 무반응 |
| 더티 렌더 | 행 단위 dirty 비트 존재, 프레임 게이트 동작 | 뷰가 프레임마다 **전체 셀**을 다 그림(행 스킵 미사용). 스크롤 비트블릿 없음 |
| IME | 확정 `Char`만 소비해 pty로 전송 | 조합 중(`TextEditing`) 미표시 → 조합 글자는 SDL 기본 부유창에만 보임. 커서 위치 인라인 오버레이 없음 |
| 브래킷 붙여넣기 2004 | 파서가 플래그만 추적 | 붙여넣기 시 `\x1b[200~` 감싸기 미구현(붙여넣기 자체도 미구현이라 대기) |
| 커서 표현 | 블록 외곽 + 530ms 깜빡임, DECSET 25 | 모양 전환(DECSCUSR: 블록/밑줄/세로바), DECSET 12 깜빡임 토글 |

### 2.3 미구현

| 기능 | 격차 크기 | 메모 |
|---|---|---|
| **CJK 전각 + 한글 렌더** | ★★★ 최대 | 아틀라스 범위가 ASCII(0x20–0x7E)+박스그리기(0x2500–0x259F)뿐 → **한글이 전부 플레이스홀더 막대로 보임**. `width` 필드는 예약돼 있으나 항상 1, 더미 셀 없음, East Asian Width 판별 없음 |
| 텍스트 선택 + 클립보드 | ★★★ | 드래그 하이라이트/추출/Ctrl+Shift+C·V/더블·트리플 클릭 전부 없음 |
| 마우스 보고 | ★★ | DECSET 1000/1002/1006 추적 없음 → htop 등에서 마우스 불가 |
| 텍스트 리플로우 | ★★ | `isWrapped` 추적 없음, unwrap→rewrap 없음 |
| 조합키 특수 시퀀스 | ★★ | Ctrl+방향키(`\x1b[1;5C`) 등 수식어 인코딩 없음(Alt 접두만 있음) |
| 어플리케이션 커서 키 모드 | ★ | DECSET 1 미추적 → vim에서 화살표가 `\x1bOA` 필요해도 `\x1b[A` 발송(현실: ConPTY/vim은 보정되어 동작하나 표준 준수 필요) |
| 폰트 다국화 | ★★ | Consolas에 한글 글리프 없음 → 맑은 고딕(malgun.ttf) 등 2차 폰트 필요 |
| URL/하이퍼링크 | ★ | 정규식 감지 + OSC 8 둘 다 없음 |
| 설정 파일 | ★ | shell/폰트/테마/스크롤 크기 하드코딩 |
| OSC 타이틀 → 서버 동기화 | ★ | 클라 타이틀바만 갱신, 서버 레이어 타이틀은 불변(와이어 메시지 필요) |
| 입력 쓰기 큐(스레드) | ☆ | 동기 WriteFile 유지 — 대용량 붙여넣기 전까지 관찰 과제 |

---

## 3. 단계별 구현 계획

의존성 순서. 각 단계는 독립적으로 배포 가능하고 셀프테스트/스모크로 검증한다.

### 단계 0 — 소소한 즉효 (반 세션)

| 작업 | 파일 | 비고 |
|---|---|---|
| 행 스킵 렌더: `OnPaintClient`에서 `IsRowDirty(row)`인 행만 다시 칠하기 | `TerminalView.cpp` | 프레임당 비용 감소. 단, **스크롤/커서 깜빡임/선택은 전체 행 dirty를 유발하므로 MarkAllDirty 유지** |
| 스크롤 중 신규 출력 시 앵커 유지 | `TerminalView.cpp` | offset을 "가장 오래된 히스토리 행" 기준으로 재계산하거나, offset+hist가 커진 만큼 offset을 밀어 시야 고정 |
| 커서 깜빡임 리셋 | `TerminalView.cpp` | 입력 직후 blinkOn_=true로 강제해 타이핑 중 사라짐 방지(현재 토글만) |

### 단계 1 — CJK 전각 + 한글 렌더 (핵심 격차, 1–2세션)

> **✅ 구현 완료 (2026-09-06).** 폭 판별은 `JKTermCharWidth`(`JKTerminalGrid.h`,
> conhost 호환: 호환 자모 0x3130–0x318F는 반각), 더미 셀은 `JKTerminalGrid::PutChar`
> (width=2 기록 + width=0 추종 셀, 끝 열 wrap), 커서/BS 더미 스킵은 그리드+파서,
> 렌더는 `TerminalView::PaintCell`(width==2 → 2셀 블립), 아틀라스는
> `JKGlyphAtlas::InitFallback`(malgun.ttf, 512글자 청크 페이지 지연 래스터,
> `termglyphfb_*` 키)로 착수. 셀프테스트: 한글 더미 배치 / a가b 혼용 / BS 더미
> 건너뜀 / 끝 열 전각 wrap — 0 failures.

한글이 막대로 보이는 문제를 해소하는 최우선 단계.

1. **폭 판별**: `JKVtParser`가 코드포인트 → 폭(1/2) 판별. 간이 East Asian Width
   테이블(전각 구간: 0x1100–0x115F, 0x2E80–0xA4CF, 0xAC00–0xD7A3,
   0xF900–0xFAFF, 0xFE30–0xFE4F, 0xFF00–0xFF60, 0xFFE0–0xFFE6 + 이모지 일부).
2. **더미 셀 규칙**: `PutChar`에서 width=2 기록 + 다음 셀에 더미(width=0) 마킹.
   라인 끝에서 wrap 시 더미 포함 처리. 커서 이동/BS/지우기는 더미를 건너뛰거나
   원본+더미를 함께 처리.
3. **렌더**: width==0 스킵, width==2는 2셀 폭 블립.
4. **아틀라스**: CJK 범위를 **2차 폰트**(malgun.ttf)에서 래스터. `(cp, fg, bold)`
   페이지 키 유지 — 한글은 조합형이라 글자 수가 많으므로 LRU 페이지 상한 확인.
   Consolas에 한글이 없어 반드시 폴백 폰트가 필요.
5. 검증: powershell에서 `Write-Host "한글 테스트"`, `Get-ChildItem`(한국어 파일명),
   셀프테스트에 전각 시나리오 추가(`"a가b"` → 셀 폭 검사).

### 단계 2 — 선택 + 클립보드 (1–2세션)

1. **선택 모델**: 뷰에 `selAnchor_/selFocus_`(결합 히스토리+그리드 좌표), 드래그는
   서버의 마우스 캡처 경로 재사용. 렌더에서 선택 영역 역상(fg/bg 스왑).
2. **좌표 매핑**: 픽셀 → (행, 열) = `(y-client.y)/kTermCellH`, `(x)/kTermCellW` +
   스크롤 오프셋 보정(히스토리 인덱스로 환산).
3. **추출**: 정렬(anchor/focus) 후 행 순회 — width==0 스킵, 행 끝 `\r\n`.
4. **클립보드 PAL**: `JKPlatform`에 `GetClipboardText/PutClipboardText`(Win32
   CF_UNICODETEXT, UTF-8 인터페이스). Ctrl+Shift+C/V 바인딩.
5. **붙여넣기**: UTF-8로 변환해 pty Write. `bracketedPaste_` 플래그 활용해
   `\x1b[200~ ... \x1b[201~` 감싸기.
6. 더블클릭 단어(구분자 집합 탐색)/트리플클릭 행 선택.
7. 검증: 드래그→Ctrl+Shift+C→메모장 붙여넣기, 역방향 드래그, `ls` 출력 선택.

### 단계 3 — 마우스 보고 + 고급 키 모드 (1세션)

1. DECSET 1000/1002/1006 추적(파서 상태), 마우스 다운/업/무브 → SGR 시퀀스
   (`\x1b[<b;x;yM/m`) 인코딩 → pty 전송. 버튼/좌표는 이미 와이어에 있음.
2. alt 화면 휠 → 화살표/PgUp 변환(vim 스크롤).
3. DECSET 1(어플리케이션 커서 키) → 화살표 `\x1bOA` 계열 전환.
4. 수식어 조합: `CSI 1;<m><char>` (Shift=2, Alt=3, Ctrl=5).
5. DECSCUSR(커서 모양) + DECSET 12.
6. 검증: `wsl htop`(마우스), vim에서 Ctrl+방향키 단어 이동.

### 단계 4 — 리플로우 + 스크롤 완성도 (2세션, 난이도 최고)

1. 그리드 행 + 스크롤백 스냅샷에 `isWrapped` 플래그(PutChar 지연 랩 지점에서 기록).
2. 리사이즈 시: 히스토리+화면을 논리 줄로 언랩 → 새 폭으로 리랩 → **새 버퍼에
   조립 후 스왑**(기존 버퍼 위에서 직접 수정 금지).
3. 앵커: 리플로우 기준점(보통 화면 하단/커서 행) 추적 후 새 위치로 스크롤 복원.
4. 스크롤바 표시(우측 얇은 트랙: 스크롤백 비율 + 위치).
5. 검증: vim으로 긴 파일 열고 폭 리사이즈 반복 → 텍스트 찰흙 재배치 + 커서 점프 없음.

### 단계 5 — 부가 완성도 (각각 독립)

| 작업 | 파일 | 비고 |
|---|---|---|
| URL 감지: 뷰포트 정규식 + OSC 8 파싱, Ctrl+클릭 → 셸 열기 | `TerminalView.cpp`, `JKVtParser.cpp` | OSC 8은 셀에 링크 메타데이터 필요(별도 벡터 캐시 권장) |
| 설정 파일 `terminal.ini` | 신규 `JKTerminalConfig` | shell/폰트/테마/스크롤 크기 → 브릿지/아틀라스 주입 |
| OSC 0 타이틀 → 서버 레이어 타이틀 | `JKWireProtocol.h`, `JKClientConnection` | `SetTitle` S→C 신규 메시지 |
| 입력 쓰기 큐(스레드) | `JKConPtyBridge.cpp` | 대용량 붙여넣기 시 파이프 블록 방지. 현재 동기 유지 |
| 스크롤 비트블릿 | 렌더 경로 | 행 스킵으로 부족할 때만 |

---

## 4. 리스크 / 비고

- **전각+리플로우 상호작용**: 단계 1의 더미 셀 규칙을 단계 4 리플로우가 전제로
  하므로, 단계 1에서 "더미는 항상 원본 오른쪽에, wrap 경계에서 원본만 따라감"
  규칙을 미리 문서화해 둘 것.
- **한글 아틀라스 메모리**: 페이지 키가 (fg, bold)라 16색 전경 × 2355자 조합이
  순간적으로 몰리면 페이지 폭발 가능 — LRU 상한(page 개수) 확인 필요.
- **마우스 이중 소유**: 서버가 크롬(닫기/리사이즈/타이틀)을 가로채므로, 마우스
  보고 모드에서는 서버가 캡처 전에 콘텐츠로 넘기는 기존 규칙(§7) 그대로 유지.
- 외부 레퍼런스 코드(Win32 단일창 가정)는 이식 불가 — 위 §1 대응표로 번역.