# Dear ImGui 백엔드 (`imgui_impl_jkwindow`) 계획

> `engine` 윈도우 서버 환경에 **Dear ImGui**를 통합하는 계획 문서.
> 2026-09-05 기획 (구현 전). 터미널 트랙(별도 문서)에 이은 제2 트랙으로, "터미널 우선,
> 이후 Dear ImGui" 순서가 사용자 승인되어 있다. 구현은 터미널 이후에 시작한다.
> 2026-09-06 업데이트: 터미널 트랙 Phase 1 완료(docs/22). 사용자 리서치
> (`temp/imgui 적용 검토.md`) 검토 결정을 **§11**에 기록 — §9 Phase 2 이후
> 로드맵은 §11 기준으로 재편된다.
> 기반이 되는 문서: `19_sdl2_window_server.md`(서버 모델), `21_jkx_container.md`(패키징),
> `16_sdl2_jkwindow_ime.md`(IME), `20_png_assets.md`(아이콘 규약).

## TL;DR

- **목표**: Dear ImGui 생태계의 OSS 도구(노드 에디터, 파일 매니저, 헥사 에디터, 신스 UI 등)를
  jkwindow 클라이언트 앱으로 최소 수정 포팅할 수 있게 한다. 핵심은 ImGui 코어 벤더링 +
  커스텀 백엔드 `imgui_impl_jkwindow`(이벤트 입력 + 클라이언트 offscreen 텍스처로의 렌더) +
  데모 앱 `jkapp_imguidemo`.
- **클라이언트는 이미 GPU 렌더러를 갖고 있다**: 클라이언트 프로세스는 HIDDEN SDL 창에
  `SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE` 렌더러를 만들고
  (`JKClientApplication.cpp:55-65`), 씬을 **렌더 타깃 텍스처**에 그린 뒤
  `SDL_RenderReadPixels`로 읽어 공유 메모리에 커밋한다(`JKClientApplication.cpp:530-564`).
  따라서 래스터라이저는 **커스텀 소프트웨어 래스터라이저가 아니라 `SDL_RenderGeometry` 경로**
  (공식 `imgui_impl_sdlrenderer2` 포팅)를 채택한다 — 근거는 §4.2.
- **이벤트**: 서버가 전달하는 `JKEvent`를 백엔드가 `ImGuiIO`로 변환한다(§4.1 표).
  모디파이어는 **반드시 이벤트에 실려온 `option`**(서버가 `keysym.mod`를 채움,
  `JKWindowServer.cpp:731`)을 쓴다. 클라이언트 프로세스의 `SDL_GetModState()`는 실제 OS
  이벤트를 받지 못해(보이는 창은 서버 소유, doc 19 §2) 신뢰할 수 없다.
- **루프 성질(실측)**: `JKClientApplication::Run`은 dirty-driven이 아니라 **매 반복 무조건
  렌더 + CommitFull**을 하고 `SDL_Delay(1)`로 페이싱된다(`JKClientApplication.cpp:183-197`).
  따라서 ImGui 로직은 타이머(60Hz)로만 돌리고, 루프의 매 반복에서는 **직전 `ImDrawData`를
  타깃 텍스처에 재발행**한다(§5).
- **ImGui 컨텍스트는 프로세스당 1개로 끝난다**: 클라이언트 앱 = 자기 프로세스(서버가
  `CreateProcess`로 스폰, doc 19 §2·§8)이므로 모듈 DLL 정적 임베딩 규칙과 충돌 없음(§3).
- **버전 핀**: **v1.92.9b (2026-07-31)**. 1.92부터 동적 폰트/텍스처 프로토콜
  (`ImGuiBackendFlags_RendererHasTextures`)이 기본이므로 백엔드는 공식 백엔드의 현재 구조를
  따라 `ImTextureData` 생성/부분 갱신을 구현한다(§4.3). MIT 라이선스.
- **멀티뷰포트는 영구 제외** — 창 소유는 서버가 갖는다(doc 19 §1). ImGui 팝업/툴팁은
  surface 내부 렌더라 문제없다.
- **빠른 찾기**: 구조 §2, 벤더링·빌드 §3, 백엔드 설계(이벤트 표·래스터 결정) §4,
  프레임 수명주기 §5, 데모 앱 §6, CMake·패키징 §7, 리스크 §8, 구현 단계 §9, 검증 §10.

---

## 2. 전체 구조

```
[이벤트 흐름]
서버 SDL 이벤트 (물리 px)
  → JKWindowServer 입력 해석: surface 로컬 좌표 역변환 (JKWindowServer.cpp:688-693)
  → InputEventPayload (named pipe)
  → JKClientSurface::ReadLoop → JKEvent 역직렬화 (JKClientSurface.cpp:186-231)
  → Run 루프 DrainInputChannel → ProcessOneEvent (JKClientApplication.cpp:212-225, 227-282)
  → [앱 오버라이드] PreProcessMessage: ① ImGui_ImplJKWindow_ProcessJKEvent(ev)
     ② 기존 라우팅 그대로 통과 (빈 컨트롤 트리라 무해)
  → 다음 ImGui 프레임(NewFrame)이 ImGuiIO 큐를 소비

[그리기 흐름]
앱 타이머 60Hz (SetTimerInterval, ClientTetrisApp.cpp:38-39 선례)
  → ImGui_ImplJKWindow_NewFrame(dt, w, h) → 앱 위젯 코드 → ImGui::Render()
     (ImDrawData는 다음 NewFrame까지 유효 — 재발행 가능)

클라 Run 루프 매 반복 (~1kHz, SDL_Delay(1) 페이싱)
  → RenderAndCommit:
     ComposeScene: 컨트롤 트리 → JKRenderCommandList 직렬화 (JKClientApplication.cpp:472-499)
     → target 텍스처 replay = 배경 씬                    (:530-539)
     → [신설 훅] RenderOverlay: ImGui_ImplJKWindow_RenderDrawData
        = SDL_RenderGeometry로 ImDrawData를 타깃 텍스처 위에 알파 블렌드   ← 유일한 코어 변경
     → SDL_RenderReadPixels → pixelBuffer                (:548)
     → memcpy → 공유메모리 shm → CommitFull              (:560-564)
  → 서버 컴포지터: dirty 텍스처 업로드 → fit 스케일 렌더 (doc 19 §4.3, §7.3)
```

핵심 통찰: 클라이언트의 최종 픽셀은 **가속 렌더러의 렌더 타깃 텍스처**에서 나온다. ImGui는
공유 메모리 RGBA surface에 직접 쓰지 않고, 그 텍스처에 합성된다. 커밋/dirty/리사이즈 흐름은
기존 코드를 전혀 건드리지 않는다.

---

## 3. 벤더링·빌드

### 3.1 벤더링

- 대상: **Dear ImGui v1.92.9b** (2026-07-31, 이 문서 작성 시점 최신 — v1.92.9의
  DragFloat/테이블 픽셀 회귀를 고친 핫픽스). 정확한 커밋을 핀으로 기록한다.
- 위치: `third_party/imgui/` — 코어 5개(`imgui.cpp`, `imgui_draw.cpp`, `imgui_tables.cpp`,
  `imgui_widgets.cpp`, `imgui_demo.cpp`) + 헤더(`imgui.h`, `imgui_internal.h`,
  `imconfig.h`, `imstb_rectpack.h`, `imstb_textedit.h`, `imstb_truetype.h`) + `LICENSE.txt`.
- `imstb_*`는 ImGui 저장소가 자체 사본으로 포함한다. 기존 `third_party/stb`는
  `stb_image.h`만 있어(imfont 용도 아님) 추가 벤더링 불필요.
- 라이선스: MIT — `LICENSE.txt`를 그대로 둔다. 서드파티 고지는 차후 단일 목록으로.
- **v1.92 주의**: 1.92.0(2025-06)은 동적 폰트 시스템 + `ImGuiBackendFlags_RendererHasTextures`
  텍스처 프로토콜로의 대대적 전환(2015년 이후 최대 브레이킹)이었다. 우리 백엔드는 구식
  `GetTexDataAsRGBA32` 1회 빌드 경로가 아니라 **공식 `imgui_impl_sdlrenderer2`의 현재 구현을
  포팅**해 `ImTextureData`의 WantCreate/WantUpdates/WantDestroy를 처리한다(§4.3).

### 3.2 빌드 구조와 모듈 경계

- `imgui` **별도 static 타깃**(CMake §7). `jkcore`에 넣지 않는다 — jkcore는 모든 모듈
  DLL(현재 11종)에 정적으로 임베드되므로, ImGui(~1-2MB 목적 코드)를 얹으면 ImGui를 안 쓰는
  모든 모듈이 그 비용을 진다. ImGui 앱만 `imgui`를 링크한다.
- **컨텍스트 수명**: 각 클라이언트 앱은 자기 프로세스로 스폰되고(doc 19 §2),
  모듈 DLL은 자기 jkcore 사본 + 자기 CRT를 가진다(doc 19 §2, `JKAppModule.h:6-10` 주석).
  ImGui 전역 컨텍스트(`GImGui`)는 DLL/프로세스당 1개 — "프로세스당 ImGui 앱 1개" 모델이라
  충돌 여지가 없다. FreeLibrary 금지 규칙(doc 21 §3)도 그대로 적용된다.
- 백엔드 소스(`imgui_impl_jkwindow.cpp`)는 `imgui` 타깃에 함께 넣는다. 백엔드가 의존하는 것은
  ImGui 자체 + SDL2 + `JKEvent.h`(jkcore 헤더, include path만 사용)뿐이다.

---

## 4. 백엔드 설계 (`imgui_impl_jkwindow`)

`imgui_impl_sdl2`(이벤트) + `imgui_impl_sdlrenderer2`(렌더)를 합친 형태의 소프트웨어 파이프라인
백엔드. 공개 API(구현 파일에서 제공):

```
ImGui_ImplJKWindow_Init(SDL_Renderer*)            // io 백엔드 플래그, 클립보드 함수 등록
ImGui_ImplJKWindow_Shutdown()
ImGui_ImplJKWindow_ProcessJKEvent(const JKEvent&) // ImGuiIO Add* 큐잉
ImGui_ImplJKWindow_NewFrame(float dt, int w, int h)
ImGui_ImplJKWindow_RenderDrawData(ImDrawData*, SDL_Renderer*)
```

### 4.1 JKEvent → ImGuiIO 매핑 표

서버가 채우는 필드 값(`JKWindowServer.cpp:688-750`)과 클라 역직렬화
(`JKClientSurface.cpp:186-231`)를 기준으로 한다.

| JKEvent | 서버가 채우는 값 | ImGuiIO 호출 | 비고 |
|---------|-----------------|--------------|------|
| `MouseMove` | `x,y` = surface 로컬 px(레이어 스케일 역변환 완료), `dx,dy` | `AddMousePosEvent((float)x, (float)y)` | 서버 캡처 중엔 표면 밖 좌표(음수/초과)가 온다 — 그대로 전달, ImGui 허용(§8) |
| `MouseDown` | `detail` = SDL 버튼(1~5), `keyCode` = 클릭 수(클라에서 스왑됨, `JKClientSurface.cpp:217-218`) | `AddMouseButtonEvent(map(detail), true)` | 1→Left, 2→Right, 3→Middle, 4/5→X1/X2. **주의**: SDL 규약상 2=middle이지만 jk 이벤트는 SDL 버튼값을 그대로 믿는다 |
| `MouseUp` | 동일 | `AddMouseButtonEvent(map(detail), false)` | |
| `MouseWheel` | `dx,dy` = SDL wheel x/y 원값(`JKWindowServer.cpp:716-721`) | `AddMouseWheelEvent((float)-dx, (float)dy)` | imgui_impl_sdl2와 동일: x 부호 반전, y 그대로 |
| `KeyDown` | `keyCode` = `keysym.sym`, `detail` = repeat, `option` = `keysym.mod`(`JKWindowServer.cpp:729-731`) | SDL sym→ImGuiKey 표로 `AddKeyEvent(key, true)` + 4개 `AddKeyEvent(ImGuiMod_*, up/down)` | repeat 이벤트도 그대로 전달(ImGui 입력 큐 규약). 모드 갱신은 이벤트의 `option`만 사용 — 클라 `SDL_GetModState()`는 죽은 값 |
| `KeyUp` | 동일 | `AddKeyEvent(key, false)` + 모드 up | Ctrl+C 등 조합 감지에 KeyUp 필수 |
| `Char` | `text` = UTF-8(`SDL_TEXTINPUT`, `JKWindowServer.cpp:733-740`) | `AddInputCharactersUTF8(ev.text)` | `JKEvent::text` 64바이트(`JKEvent.h:42`) |
| `TextEditing` | `text` + `editStart=detail`, `editLength=option`(`JKWindowServer.cpp:741-750`) | **Phase 1 미매핑(보류)** | ImGui는 pre-edit 문자열을 자체 렌더하지 않는다. §8·§9 Phase 2 참조 |
| `SizeChanged` | `x,y` = 새 surface 크기(`JKClientApplication.cpp:232-251`) | NewFrame 시 `io.DisplaySize` 갱신으로 흡수 | 별도 Add* 없음 |

- **SDL sym → ImGuiKey 표**: `imgui_impl_sdl2`의 `ImGui_ImplSDL2_KeycodeToImGuiKey` 접근을
  그대로 빌린다. 서버가 `keysym.sym`을 무변환 전달하므로(§4.1 KeyDown 행) 표는 공식 백엔드와
  동일하게 동작한다 — 출력 가능 ASCII(`'a'.. 'z'`, `'0'..'9'`, `SDLK_RETURN/ESCAPE/...`)는
  리터럴 매칭, 화살표/F키/모디파이어는 `SDLK_UP = 0x40000000|scancode` 계열 상수 매칭.
- **백엔드 플래그**: `io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures |
  RendererHasVtxOffset`. `HasMouseCursors`는 **설정하지 않는다**(§8 커서).
- **클립보드**: `io.SetClipboardTextFn/GetClipboardTextFn` → 클라 프로세스에서
  `SDL_SetClipboardText/SDL_GetClipboardText` 직접 호출. 클라는 `SDL_Init(SDL_INIT_VIDEO)`를
  하므로(`JKClientApplication.cpp:36`) OS 클립보드에 접근 가능하다 — 선례:
  `JKEdit.cpp:713-728`. **서버 측 클립보드 브릿지는 존재하지 않으며, 필요도 없다**(조사
  결과 — doc 19/16에 브릿지 없음, 클라 로컬 SDL 클립보드가 유일한 경로). Phase 1에 포함.

### 4.2 래스터라이저 결정과 근거

**결정: (b) `SDL_RenderGeometry` — 클라이언트가 이미 소유한 가속 렌더러로, 기존 렌더 타깃
텍스처에 그린다.** 커스텀 소프트웨어 래스터라이저(안 (a))는 채택하지 않는다.

근거 (코드에서 확인한 사실):

1. 클라이언트 렌더 파이프라인은 이미 GPU다. `JKClientApplication`은 HIDDEN 창에
   `SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE`를 요청하고(`JKClientApplication.cpp:55-56`)
   씬을 렌더 타깃 텍스처에 그린 뒤 readback한다(`:530-548`). "소프트웨어 RGBA surface"는
   **전송 매체**일 뿐 래스터 지점이 아니다. 프롬프트상 가정("클라가 소프트웨어 렌더러를
   감싸는가 / raw pixel 쓰기인가")에 대한 정정: `JKSDLRenderBackend`는 가속 SDL 렌더러를
   감싸며(`JKSDLRenderBackend.h:9-14`), `JKOffscreenSurface`(raw 픽셀)는 클라 경로에서
   사용되지 않는다(헤더만 include됨, `JKClientApplication.cpp:6`).
2. 공식 `imgui_impl_sdlrenderer2`가 정확히 이 모양이다: `SDL_RenderGeometryRaw` + 커맨드마다
   `SDL_RenderSetClipRect`(프레임버퍼 클램프 + 화면 밖 커맨드 skip) + `SDL_COLOR` 재해석
   (endian 일치 RGBA32) + `SDL_BLENDMODE_BLEND`. 이를 포팅하면 신규 코드는 수백 줄 수준.
   자작 소프트웨어 래스터라이저는 삼각 래스터·텍스처 샘플링·블렌딩·AA 전부를 새로 검증해야
   하고(천 줄 이상), 얻는 것은 없다.
3. 블렌딩/클립이 SDL에 위임된다: ImDrawCol은 straight-alpha이고 SDL 블렌드
   (`SRC_ALPHA, ONE_MINUS_SRC_ALPHA`)와 정합 — 공식 백엔드가 검증한 조합. 클립은
   `SDL_RenderSetClipRect`가 **현재 렌더 타깃**에 적용되므로 우리의 타깃 텍스처 상황에서도
   동작한다. `RenderAndCommit`은 replay 전에 `SetScale(1.0,1.0)`으로 고정하므로
   (`JKClientApplication.cpp:531`) 클립/정점 좌표 == 타깃 픽셀 좌표, `FramebufferScale=1`.
4. 커밋 흐름과 자연 결합: ImGui 출력은 씬 replay 직후 같은 타깃에 합성되고, 기존
   `SDL_RenderReadPixels → shm → CommitFull` 경로가 그대로 실어 보낸다. dirty rect 로직,
   리사이즈 세대 교체(doc 19 §7.2), fit 스케일 모두 무수정.
5. 성능: 래스터는 GPU가 담당하고, 매 프레임 지불하던 readback 비용(1280×720 RGBA ≈ 3.5MB)은
   기존과 동일. 안 (a)은 CPU 래스터 비용을 추가로 얹는 셈.

단서(한계): SDL_RenderGeometry는 드로우 중 샘플 모드 전환 콜백을 지원하지 않는다(공식
백엔드도 미지원) — 폰트/이미지 텍스처는 생성 시 `SDL_ScaleModeLinear`로 통일한다.
또한 JKDC 명령 리스트에는 텍스처 삼각형 명령이 없으므로(`JKRenderBackend.h:38-51` —
BlitTexture는 사각 blit뿐) **ImGui를 JKRenderCommandList로 직렬화하는 것은 불가능**하며,
반드시 SDL_Renderer 수준 훅이 필요하다. 이것이 §5의 `RenderOverlay` 훅인다.

### 4.3 폰트 아틀라스·텍스처 (ImTextureID)

- 1.92 동적 폰트 프로토콜: 백엔드는 `RenderDrawData` 앞단에서 `draw_data->Textures`의
  `ImTextureData`를 처리한다 — `WantCreate` → `SDL_CreateTexture(RGBA32, STATIC)` +
  `SDL_UpdateTexture` + `SetTexID((ImTextureID)(intptr_t)tex)`, `WantUpdates` → 영역 단위
  `SDL_UpdateTexture(tex->GetPixelsAt...)`, `WantDestroy` → `SDL_DestroyTexture`.
  (공식 imgui_impl_sdlrenderer2 포팅, 텍스처마다 `SDL_ScaleModeLinear` +
  `SDL_BLENDMODE_BLEND`.)
- 앱 소유 이미지: `ImTextureID = SDL_Texture*` 규약. 클라 프로세스의 백엔드 핸들도
  `SDL_Texture*`이므로(`JKSDLRenderBackend.cpp:98-107`) Phase 2에서 `JKResourceCache` 텍스처를
  ImGui 이미지로 노출하는 브릿지가 가능하다(문서화만, 구현은 후순위).
- 기본 폰트: ImGui 기본 빌트인(1.92.6+ 기본 벡터 폰트) — 초기화 비용 없음.
- **한글(Phase 2)**: 동적 폰트 시스템이라 구식 `GetGlyphRangesKorean`(아틀라스 선불 비용) 대신
  한글 글리프를 가진 TTF를 `ImFontConfig`로 로드하면 on-demand 래스터된다. TTF 경로는 이미
  `HangulManager`가 보유(`JKClientApplication.cpp:116-123`) — 경로 재사용. 아틀라스 크기
  비용 문제는 1.92에서 사실상 소멸.

---

## 5. 프레임 수명주기

### 5.1 루프 성질 (실측 — 계획의 전제)

- `Run()`은 `DrainTimerChannel → DrainInputChannel → RemoveClosedChildren → RenderAndCommit →
  SDL_Delay(1)`을 돌며 **매 반복 무조건 렌더 + CommitFull**이다
  (`JKClientApplication.cpp:183-197`, `:501-565`). dirty-driven이 아니다. 유효 프레임 레이트는
  `SDL_Delay(1)` + 렌더/ readback/IPC 비용에 지배되며 수백 fps 수준.
- 타이머는 별도 `JKTimerThread`가 메시지 버스 Timer 채널로 이벤트를 던진다
  (`JKTimerThread.h:13-15`), `SetTimerInterval`은 legacy 타이머 등록이다
  (`JKClientApplication.cpp:372-382`). 게임 앱 선례: Tetris가 `PreProcessMessage`에서 Timer
  이벤트를 받아 로직을 돌린다(`ClientTetrisApp.cpp:43-50`).

### 5.2 수명주기 (따라서 이렇게 간다)

1. **Init** (`ClientImGuiDemoApp::OnInit`): 크롬리스 루트 구성(§6) → `ImGui_ImplJKWindow_Init`
   → `SetTimerInterval(16)` → `io.IniFilename = nullptr`(멀티 클라이언트의 imgui.ini 동시
   쓰기 충돌 회피; 설정 저장은 후순위).
2. **이벤트**: `PreProcessMessage` 오버라이드에서 **모든** 이벤트를
   `ImGui_ImplJKWindow_ProcessJKEvent`로 먼저 흘린 뒤 `return true`. 주의:
   `PreProcessMessage`의 반환값은 "이벤트 소비 여부"가 아니라 **"계속 실행"**이다
   (`ProcessOneEvent`가 false를 받으면 `running_=false`로 앱 종료, `JKClientApplication.cpp:263`,
   `:220`) — 이벤트를 삼키는 데 이 함수를 쓸 수 없다(프롬프트 가정에 대한 정정). 삼킘이
   필요해지면 `RouteMessage` 오버라이드로 흡수한다(Phase 1은 불필요 — 컨트롤 트리가 비어
   라우팅이 no-op).
3. **ImGui 프레임** (Timer 이벤트마다 ≈60Hz): `NewFrame(dt)` → 앱 위젯 코드 → `ImGui::Render()`.
   `dt`는 타이머 명목값이 아니라 `SDL_GetPerformanceCounter` 실측. NewFrame→Render 짝은
   이 콜백 안에서 완결되므로 ImGui의 NewFrame/EndFrame 순서 assert와 루프의 dirty 여부가
   충돌하지 않는다. **첫 프레임 전(RenderOverlay가 DrawData를 보기 전) 상태에서는 훅이
   skip된다**(프레임 카운터 가드).
4. **오버레이 합성** (루프 매 반복): `ComposeScene`이 매번 타깃을 `Clear`+재도장하므로
   (`JKClientApplication.cpp:481-485`) ImGui 픽셀은 매 반복 다시 그려야 한다. `RenderAndCommit`
   안의 신설 가상 훅 `RenderOverlay(SDL_Renderer*, int w, int h)`(기본 no-op)이 scene replay
   (`:534-539`)와 `SDL_RenderReadPixels`(`:548`) **사이**에서
   `ImGui_ImplJKWindow_RenderDrawData(ImGui::GetDrawData(), renderer)`를 호출한다.
   DrawData는 다음 NewFrame까지 유효하므로(ImGui 규약) 60Hz 갱신 + 수백 fps 재발행이 안전.
   재발행 비용이 무거우면 §8의 가드 최적화로 이어진다.
5. **리사이즈**: `SizeChanged` → `ApplyPendingResize` + logical 크기 갱신 + `Invalidate`
   (`JKClientApplication.cpp:232-251`) → 다음 NewFrame이 `io.DisplaySize`를 새 surface 설계
   크기로 갱신, 기존 코드가 타깃 텍스처를 재생성(`:516-528`). **`io.DisplaySize`는 서버
   디스크톱(1280×720)이 아니라 클라 surface 설계 크기다** — 표시 fit 스케일은 서버가 전담하며
   클라는 설계 px로 그린다(doc 19 §7.3). 프롬프트 지시대로 문서화 명시.

---

## 6. 데모 앱 (`jkapp_imguidemo`)

- 메타: `{ name:"imguidemo", title:"ImGui Demo", width:1280, height:720 }` — 데스크톱 논리
  1280×720에 1:1 fit. 캐스케이드 오프셋(+20)으로 닫기 버튼이 밀려나는 문제는 서버의 스폰
  clamp가 처리한다(doc 19 §7.1).
- 구조(게임 앱 선례 그대로, `ClientTetrisApp.cpp:11-41`):
  - 루트 `JKWindow` + 내부 풀사이즈 창을 `WA_CHROMELESS` + `DOCK_FILL`로 구성 — 창 관리는
    전부 서버 크롬의 역할(doc 19 §7). **ImGui는 컨트롤 트리를 우회한다**: 컨트롤 트리는
    배경(단색 클리어, `ComposeScene`의 기본 192,192,192 클리어)만 그리고 모든 UI는
    `RenderOverlay` 훅의 ImDrawData가 위에 얹는다. "컨트롤 트리 우회 패턴"의 참조 구현.
  - `PreProcessMessage`로 전 이벤트를 ImGui에 피드(§5.2).
- 창 내용: `ImGui::ShowDemoWindow()`(전 기능 스모크) + `ShowStyleEditor()` + 빌트인
  `ImGui::PlotLines` 데모 패널. 이벤트 검증용으로 키/마우스 상태를 표시하는 소형
  디버그 패널(ImGuiIO 덤프)을 추가한다 — §10 검증의 1차 도구.
- 모듈 보일러플레이트는 tetris와 동일(`JKAppModule_tetris.cpp:7-19`):
  `jk_app_meta` → 정적 메타 반환, `jk_app_run_client(pipeName)` → 앱 생성/Init/Run.
- 런처 아이콘: `assets/icons/launcher_imgui@1x.png` / `@2x.png` — .jkx 컨테이너가 자체
  아이콘을 실어 규약을 따른다(doc 21 §1, `JKWindowServer.cpp:1021-1023`). 내장 폴백 경로
  규약(`launcher_<pfx>`, `JKWindowServer.cpp:976-978`)과 동일한 접미 관행.

---

## 7. CMake·모듈 패키징

`CMakeLists.txt` 변경점:

1. **`imgui` static 타깃** (신설):
   - 소스: `third_party/imgui/imgui*.cpp` 5개 + `src/imgui_impl_jkwindow.cpp`.
   - include: `third_party/imgui`(PUBLIC) + `include`(jkcore 헤더, `JKEvent.h`용) + SDL2.
   - `target_link_libraries(imgui PUBLIC SDL2::SDL2)` — 백엔드가 SDL 심볼을 쓴다.
   - `jkcore`에 포함하지 않는다(§3.2).
2. **`jkapp_imguidemo` SHARED** (신설): `src/apps/JKAppModule_imguidemo.cpp` +
   `src/apps/ClientImGuiDemoApp.cpp`. `JKAPP_MODULE_BUILD` 정의, `PREFIX ""`,
   `jkcore` + `imgui` 링크(둘 다 정적 임베드 — 모듈 경계 규칙 유지).
3. `-static-libstdc++ -static-libgcc` foreach 목록에 `jkapp_imguidemo` 추가
   (기존 11종 목록, `CMakeLists.txt:261-269`).
4. **jkx repack** (신설): `add_custom_command`로 `jkx-pack imguidemo` +
   `jkx_packages(ALL)` 의존 추가 — 기존 11개 앱과 동일 패턴(`CMakeLists.txt:286-341`).
   패커는 모듈의 `jk_app_meta()`를 단일 출처로 읽고 아이콘은
   `assets/icons/launcher_imgui@{1,2}x.png`를 찾는다(main.cpp `RunJkxPack`, doc 21 §2).
5. 신규 파일 목록: §9 참조.

---

## 8. 리스크

| # | 리스크 | 내용 | 완화 |
|---|--------|------|------|
| 1 | **루프 × ImGui 재발행 비용** | 루프가 수백 fps로 매 반복 전면 재렌더+readback+CommitFull(§5.1)이므로 ImGui 오버레이도 매 반복 재발행된다. ShowDemoWindow급 드로우리스트(수만 정점) × 수백 fps는 GPU 업로드 부담 | ① ImGui 로직은 60Hz 타이머로만(NewFrame 비용 절반 이상 절감) ② 실측: `SDL_GetRenderScale`·프레임 시간 로그 ③ 필요 시 Phase 1.5 최적화: "씬 미변경 && DrawData 프레임 번호 미변경이면 replay+재발행+readback skip" 가드를 `RenderAndCommit`에 추가(코어 소폭 변경, 측정 근거가 있을 때만) |
| 2 | **블렌딩 정합성** | straight-alpha ImDrawCol × SDL blend, endian 의존 `SDL_Color` 재해석 | 공식 imgui_impl_sdlrenderer2의 검증된 조합을 그대로 포팅(§4.2). 반투명 창/그라디언트 스크린샷으로 시각 검증(§10) |
| 3 | **NewFrame assert vs 루프** | 클라 루프가 dirty-driven이 아니므로 assert 리스크는 오히려 낮지만, "Timer가 안 오면 DrawData가 없는" 초기 상태 존재 | NewFrame→Render를 Timer 콜백 내 완결 + RenderOverlay 프레임 카운터 가드(§5.2-3) |
| 4 | **키 리피트·모디파이어** | 클라 프로세스에 실제 OS 키 이벤트가 없어 로컬 SDL 상태(`SDL_GetModState`)가 죽어 있음. 기존 TAB 핸들러도 이 값을 쓴다(`JKClientApplication.cpp:272` — 잠재 이슈, 본 계획 범위 밖) | 백엔드는 **이벤트 실려온 `option`(`keysym.mod`)** 만 사용(§4.1). KeyUp까지 전달해 모드 up/down 추적. 리피트는 `detail` 플래그 그대로 전달 |
| 5 | **서버 캡처 중 좌표** | MouseDown 캡처 중 서버는 surface 밖 좌표를 음수/초과로 그대로 보낸다(doc 19 §8) | ImGui는 화면 밖 마우스 좌표를 허용 — 그대로 전달. 슬라이더 드래그가 surface 밖으로 나가는 동작이 오히려 자연스러워짐 |
| 6 | **OS 커서 없음** | 서버가 커서를 그리며(doc 19 §10 "커서 모양 변경 없음") ImGui 커서 모양(I-beam, 리사이즈) 미반영 | `io.MouseDrawCursor` 미사용, `HasMouseCursors` 플래그 미설정. 커서 모양 동기화는 Phase 2+ (서버에 커서 모양 힌트 메시지 필요 — 별도 설계) |
| 7 | **IME/pre-edit** | `TextEditing`(조합 중 문자열)을 ImGui가 자체 렌더하지 않음 — 한국어 IME로 InputText 사용 시 조합 문자열이 안 보임(doc 16 §3 모델과 동일 제약) | Phase 1: 라틴 입력만 지원, TextEditing 미매핑. Phase 2: (a) 1.92 InputText/IME 개선 사항 확인 후 매핑, 또는 (b) 내장 2벌식 오토마타(`JKHangulAutomata`)를 ImGui 입력 경로에 적용. 한글 폰트와 함께 Phase 2 과제 |
| 8 | **모듈 CRT/크기** | imgui 정적 링크로 모듈 DLL 수백 KB~MB 증가, MinGW static 규칙과 상호작용 | 기존 `-static-libstdc++` 규칙만 추가 적용. jkcore 미포함으로 타 모듈 영향 없음(§3.2) |
| 9 | **fit 스케일과 폰트** | FHD 설계 크기의 ImGui 앱은 서버가 축소 표시 — 비트맵 폰트 흐림 | `imguidemo`는 1280×720(1:1)으로 회피. FHD ImGui 앱이 필요해지면 설계 크기/폰트 크기 정책을 그때 결정 |
| 10 | **imgui.ini 충돌** | 기본 설정이 CWD(exe dir)에 `imgui.ini` 쓰기 — 여러 클라가 동시 기록 | Phase 1에서 `io.IniFilename = nullptr` |

---

## 9. 구현 단계 (file-level)

### Phase 1 — 벤더링 + 백엔드 + 데모 (이벤트/렌더 실증)

1. `third_party/imgui/` — v1.92.9b 벤더링(§3.1, 코어 5 cpp + 헤더 + imstb_* + LICENSE).
2. `CMakeLists.txt` — `imgui` static 타깃 추가(§7-1).
3. `include/imgui_impl_jkwindow.h` + `src/imgui_impl_jkwindow.cpp` — 백엔드 신설:
   이벤트 피더(§4.1 표 전체), `ImTextureData` 업데이트(§4.3), `SDL_RenderGeometry`
   렌더(§4.2), 클립보드 콜백(§4.1), NewFrame(dt 실측, DisplaySize 인자).
4. `include/client/JKClientApplication.h` / `src/client/JKClientApplication.cpp` —
   **유일한 코어 변경**: protected 가상 `RenderOverlay(SDL_Renderer*, int w, int h)`(기본
   no-op) 추가, `RenderAndCommit`의 scene replay와 `SDL_RenderReadPixels` 사이 호출
   (`JKClientApplication.cpp:539-548` 사이 지점).
5. `include/apps/ClientImGuiDemoApp.h` + `src/apps/ClientImGuiDemoApp.cpp` — 데모 앱
   (§6): 크롬리스 루트, 전 이벤트 피드, 60Hz 타이머 프레임, DemoWindow/StyleEditor/
   PlotLines/IO 덤프 패널.
6. `src/apps/JKAppModule_imguidemo.cpp` — C ABI 모듈(§6).
7. `assets/icons/launcher_imgui@1x.png`, `@2x.png` — 런처 아이콘(doc 20 규약).
8. `CMakeLists.txt` — `jkapp_imguidemo` 타깃 + static 링크 옵션 + `jkx-pack` repack
   커맨드(§7).
9. 검증(§10).

### Phase 2 — 실전 포팅 + 한글 + 고도화

> (2026-09-06 §11에서 재편: Phase 2의 쇼케이스는 **taskmgr** 우선. 아래 항목들은
> §11 로드맵의 Phase 3로 이동.)

1. **실전 OSS 도구 1종 포팅**: 1차 후보 **ocornut/imgui_club의 메모리 에디터
   (`imgui_memory_editor.h`)** — 단일 헤더, MIT, 외부 의존 0, 데이터 집약 UI로 래스터
   성능 실증에 최적. 차후 후보: AirGuanZ/imgui-filebrowser(파일 매니저 트랙).
   포팅물은 `src/apps/`에 새 모듈(`jkapp_<tool>`)로.
2. **한글**: `HangulManager` TTF 로드(§4.3) + IME pre-edit 처리(§8-7).
3. **`JKResourceCache` ↔ ImTextureID 브릿지**: 기존 리소스 텍스처를 ImGui 이미지로
   (§4.3) — 이미지 뷰어류 포팅의 전제.
4. **ImPlot 등 추가 벤더링 판단** — 데모/도구 수요에 따라.
5. **명시적 제외(참고)**: 멀티뷰포트 — 창 소유가 서버에 있으므로 지원하지 않는다.
   ImGui 팝업/툴팁/콤보 등 surface 내부 플로팅은 모두 정상 동작한다.
6. (측정 근거 시) §8-1의 루프 가드 최적화.

---

## 10. 검증 방법

1. **빌드**: `jkapp_imguidemo.dll` + `apps/imguidemo.jkx` 생성, 서버 런처에 아이콘 셀 표시.
2. **렌더 스모크**: `--server` → imguidemo 스폰 → 데모 창이 1:1로 표시(1280×720), 서버
   크롬(타이틀 드래그/닫기)이 정상 동작 — ImGui surface 위에서도.
3. **이벤트 피드 표 검증** (§4.1 — IO 덤프 패널로 눈으로 확인):
   - 마우스: 호버 하이라이트, 클릭/드래그가 보이는 위치와 정확히 일치(혼합 배율 모니터 포함
     — doc 19 §9-3 재사용).
   - 휠: DemoWindow의 스크롤 방향/속도.
   - 키: InputText에 타이핑, Ctrl+C/V(클립보드), Shift+화살표 선택, KeyUp 후 모드 해제.
   - 리피트: 키 홀딩으로 반복 입력 확인.
4. **캡처 경계**: 슬라이더를 surface 밖까지 드래그 → 값이 계속 변함(§8-5).
5. **성능 측정**: 데모 창 + StyleEditor + Plot 다중 표시 상태에서 클라 루프 1회 시간
   (render+readback)과 서버 CPU를 기록 — §8-1의 판단 근거로 남긴다.
6. **리사이즈**: 테두리 드래그로 surface 리사이즈 → ImGui UI가 새 DisplaySize로 재배치,
   1프레임 빈 화면 artifact(doc 19 §7.2) 이외 이상 없음.
7. **멀티 클라이언트**: imguidemo + tetris 동시 스폰 → 포커스 라우팅(키는 포커스 클라로만 —
   doc 19 §8), imgui.ini 미생성 확인(§8-10).
8. **종료**: 닫기 버튼 → 클라 exit=0 → 레이어 제거 → 재스폰(acceptor 재무장, doc 19 §9-7
   절차 준용).

---

## 11. 리서치 검토 결정 (2026-09-06)

사용자 리서치(`temp/imgui 적용 검토.md` — 외부 AI 대화 로그)를 코드베이스 실정과
대조해 본 계획에 반영한 결과. §1-§10의 기술 설계는 유지되고, 아래 결정이 추가된다.

### 11.1 원칙 — 두 트랙 전략

- **jkwindow 네이티브 UI**: 셸/작업표시줄처럼 포커스·크롬·최상위 특권이 필요한
  시스템 UI 전용 (docs/28 = 작업표시줄 B안이 이 트랙).
- **ImGui 트랙**(본 문서): 테스크매니저, 뷰어, 도구 등 유틸리티 앱 — 생산성이
  결정 요건인 UI. 두 트랙의 경계는 "창 관리 특권의 필요 여부".

### 11.2 로드맵 재편 (§9 Phase 2 대체)

- **Phase 2 — taskmgr 쇼케이스**: 셸 프로토콜(docs/28) 재사용.
  - 프로토콜 v2: `ShellWindowEntry`에 `pid` 추가. 테스크매니저 클라는 pid로
    직접 `GetProcessMemoryInfo`/`GetProcessTimes` 조회 (서버 부담 0).
    (구현에서 정정: pid는 서버 기록이 아니라 Hello v2 클라 자기 보고 — §11.6)
  - UI: `ImGuiTable`(정렬 `ImGuiTableSortSpecs`) + `PlotLines`(CPU 이력) +
    IO 덤프 패널은 Phase 1 데모에서 계승.
  - 창 활성화/최소화: 기존 `WindowActivate`/`WindowMinimizeToggle` 재사용 —
    작업표시줄보다 적은 노력으로 셸 기능을 확장하는 구조.
- **Phase 3 — 생태계**: `imgui_memory_editor`(단일 헤더) → ImPlot →
  (장기) CEF/Ultralight OSR, FFmpeg 비디오 플레이어. 리서치 문서의 OSR
  5단계 파이프라인(CreateView → Update/Render → lockPixels →
  SDL_UpdateTexture → ImGui::Image + 입력 주입)과 A/V 싱크(오디오 마스터
  클럭, PTS 기반 프레임 드롭) 분석은 착수 시 재활용.
- **한글 폰트**: §4.3 유지 — v1.92 동적 폰트라 TTF 로드만으로 on-demand
  래스터(`HangulManager` TTF 경로 재사용). 리서치의
  `GetGlyphRangesKorean`(선불 아틀라스)은 1.92 이전 방식이라 미채택.

### 11.3 리서치 대비 정정 (코드베이스 실정)

| 리서치 가정 | 실정 (근거) |
|---|---|
| 창 목록 동기화용 FAM 가변배열 + ADD/REMOVE/UPDATE 델타 이벤트 신설 | 불필요 — `WindowList` 고정 32개 스냅샷 전체 교체(push-on-change, idempotent 자가복구)가 이미 존재 (JKWireProtocol.h:113, docs/28) |
| JKDC 백도어(`GetRawRenderer`)로 raw SDL_Renderer 노출 | protected 가상 `RenderOverlay` 훅 1개(§5.2-4)로 충분. "JKRenderCommandList에 삼각형 명령이 없어 JKDC 직렬화는 불가능"한 것은 동일 판단(§4.2 단서) |
| 클라가 소프트웨어 렌더러/raw 픽셀을 다뤄 커스텀 래스터라이저 필요 | 클라는 이미 가속 렌더러 + 렌더 타깃 텍스처 → readback → shm 경로(§4.2 근거 1) — 공식 `imgui_impl_sdlrenderer2` 포팅이 정답 |
| 알파 블렌딩/리사이즈 핸드셰이크/shm 이중버퍼 신설 | 기존 커밋 경로가 전부 제공 — 신규 작업 0 |
| 단일 프로세스 모드도 ImGui 지원 | 미포함 — JKRenderThread 명령 리스트에 삼각형이 없어 별도 설계 필요(§4.2 단서). Phase 1은 클라 경로만 |

### 11.4 유지 결정

- **멀티뷰포트 영구 제외** — 창 소유는 서버에 있다(§2, §9-2-5와 동일).
- 버전 핀 §3.1(v1.92.x)과 `io.IniFilename = nullptr`(§8-10) 유지.

### 11.5 Phase 1 구현 기록 (2026-09-06 완료)

§9 Phase 1 착수 승인(2026-09-06) 후 커밋 단위로 구현·검증 완료.

| 커밋 | 내용 |
|---|---|
| fbfd0b6 | v1.92.9b 벤더링 + `imgui` 스태틱 타깃(§3) — jkcore 밖(§3.2) |
| 95c87c9 | `imgui_impl_jkwindow` 퓨즈드 백엔드(§4) — 이벤트/렌더/1.92 동적 텍스처 |
| 7d08bea | `JKClientApplication::RenderOverlay` 훅(§5.2-4) — jkcore 유일 변경 |
| eef3e7b | `jkapp_imguidemo` + `apps/imguidemo.jkx` + 런처 아이콘(§6) |
| a3b646e | 데모 프레임 게이트 수정 — 16ms 타이머가 프레임 클럭 |

**구현 확정 사항(계획 대비):**

- 백엔드는 플랫폼+렌더 데이터를 한 구조체(`ImGui_ImplJKWindow_Data`)로 합침 —
  클라는 단일 창/단일 렌더러 프로세스라 공식 백엔드의 분할 데이터가 무의미.
- punctuation 키는 scancode 폴백(JKEvent에 scancode 없음, §4.1) 대신
  ASCII 값 키코드 직접 매핑으로 해결.
- `RenderOverlay`는 렌더 타깃이 1:1 스케일로 바인딩된 scene-replay 직후에 호출 —
  ImGui 드로우가 그대로 커밋 표면에 합성됨.
- 데모 앱 프레임 클럭 = 16ms 레거시 타이머 + `IsFrameDirty` 게이트.
  **교훈**: 타이머 이벤트에서 dirty 플래그를 재설정해야 함 — RenderOverlay 끝의
  set + OnFrameCommitted의 clear만으로는 첫 프레임 이후 영구 정지(커밋 a3b646e).
- 이벤트 피딩은 `PreProcessMessage` 오버라이드 — TAB 포커스 사이클에 먹히는
  것보다 앞단이라 모든 이벤트가 ImGui에 도달.

**검증 결과(§10 스모크):**

- 셀프테스트 0 failures. 서버+imguidemo 스폰 → 데모 창/패널/PlotLines 렌더.
- 마우스: 패널 IO dump 체크박스 클릭 → 토글 + `MousePos`(표면 좌표) +
  `WantCaptureMouse=1` — 서버→클라→ImGui 전 경로.
- 셸 공존: 작업표시줄이 imguidemo 창을 버튼으로 추적, 버튼 클릭으로 활성화
  (WindowActivate 재사용 확인).
- 클라 FPS ~60(타이머 케이던스), 표면 1024×720.
- **캡처 교훈**: 서버 창 PrintWindow 캡처는 가상화 좌표/스테일 프레임으로 오인을
  낳음 — `SetProcessDPIAware` + `CopyFromScreen`이 신뢰됨(125% 모니터).

### 11.6 Phase 2 구현 기록 (2026-09-06 완료)

§11.2 Phase 2 착수 승인 후 커밋 단위로 구현·검증 완료. taskmgr은
**셸 프로토콜 위의 비셸 유틸리티 클라**로, 서버는 pid 전파만 하고 통계에
관여하지 않는다("서버는 어리석은 컴포지터" 원칙 유지).

| 커밋 | 내용 |
|---|---|
| 7f4a71d | 프로토콜 v2 — `HelloPayload{protocolVersion=2, pid}`, `ShellWindowEntry.pid`, `WindowListSubscribe`(16) |
| 0a73831 | `jkapp_taskmgr` — ImGuiTable 정렬 + 자체 프로세스 샘플링 + `taskmgr.jkx` + 런처 아이콘 |

**설계 정정 (계획 대비):**

- **pid는 Hello v2 자기 보고** — §11.2 원안의 "서버가 스폰 시 CreateProcess
  정보에서 기록"은 ①스폰→파이프 연결이 레이스(연결 전 기록 없음) ②수동 실행
  클라를 놓친다는 문제. 클라가 `GetCurrentProcessId()`를 Hello에 실어 보내는
  것이 순서 보장도 없고 수동 실행도 커버. v1 클라(4바이트 Hello)는 pid=0으로
  수용 — 기존 앱 무재빌드 호환.
- **`WindowListSubscribe`(16) 신설** — WindowList 발행은 셸 전용이었음.
  taskmgr은 셸 슬롯을 차지하면 안 되므로(활성 셸 1개 제약, docs/28) 비셸
  구독자 플래그를 추가하고 `PushWindowList`가 셸+구독자 모두에 발행.
  구독자는 자기 자신도 목록에 포함(Windows 작업관리자가 자신을 보여주듯).
  `WindowActivate`/`WindowMinimizeToggle`은 애초 전 클라에 개방 — 신규
  메시지 0으로 창 제어 완성.

**구현 확정 사항:**

- 클라 통계 경로: `OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION)` →
  `GetProcessTimes` Δ(kernel+user)/Δ(wall)/코어수 = Task-Manager 스타일
  머신 전체 대비 %, `GetProcessMemoryInfo` WorkingSetSize. ~2Hz 샘플.
- 정렬은 서버가 아니라 클라가 `ImGuiTableSortSpecs`로 로컬 rows_ 사본만
  재정렬 — 스냅샷은 항상 전체 교체이므로 서버 개입 불필요.
- `history_`(surfaceId 키)·`prev_`(pid 키)는 스냅샷마다 죽은 항목을
  제거 — 장시간 세션에서 모놀리식 surfaceId로 맵이 무한 커지지 않음.
- 첫 샘플은 프레임 1에 시드(`lastSample_`를 600ms 과거로) — 실제 수치가
  1초나 늦게 뜨지 않음.

**검증 결과(서버 모드 스모크):**

- taskmgr.jkx 런처 클릭 스폰 → WindowListSubscribe 즉시 초기 스냅샷 + pcx
  접속 시 push-on-change 1행→2행.
- pid/CPU%/Memory 라이브 갱신(자기 자신 1.3%/48.9MB, pcx 5.9%/52.8MB),
  State 컬럼 active/running/minimized 추적.
- Minimize 버튼 → pcx 숨김 + State=minimized + taskbar 버튼 딤 + 버튼
  라벨 Restore 전환 + 선택 행 CPU PlotLines 표시.
- Restore 버튼 → pcx 복귀 + State=running + 라벨 Minimize 복귀.
- 행 더블클릭 → WindowActivate로 pcx z-order 상승 + taskbar 하이라이트
  이전.
- pcx X 닫기 → 행·taskbar 버튼 즉시 소멸, 선택 해제로 Activate/Minimize
  버튼 disabled(BeginDisabled 경로).
- PID 컬럼 DefaultSort 화살표 렌더. 셀프테스트 0 failures 유지.
- **캡처 교훈(Phase 1 계승)**: 스몸 중 UI 좌표는 반드시 클라 영역 물리
  해상도 크롭에서 재측정 — 패널 리사이즈 후 옛 좌표 클릭은 무반응으로
  보인다(축소 렌더 눈측정 금지).
- 서버 kill → taskbar·taskmgr 클라 자발 종료(407af96 경로 재확인).

**Phase 3(생태계 — memory editor/ImPlot/OSR)**은 별도 승인 후 착수.

### 11.7 Phase 3 구현 기록 — 생태계 1·2단계 (2026-09-06 완료)

§11.2 Phase 3 착수 승인 후 생태계 트랙의 근거리 2종(memory editor, ImPlot)을
구현·검증 완료. 벤더링 원칙은 §3.2 계승 — imgui 타깃 위에 얹되 jkcore에는
절대 넣지 않는다.

| 커밋 | 내용 |
|---|---|
| 70bed80 | ImPlot(master 7eeb916, 2026-08-06) + imgui_club 메모리 에디터 벤더링, `implot` 스태틱 타깃, imguidemo에 편집 가능한 256바이트 스크래치 패널 |
| 1eb4a07 | taskmgr CPU 이력을 ImPlot으로 전환 — 첫 실사용 소비자 |

**구현 확정 사항:**

- **ImPlot 버전 선택**: v0.17은 1.92 텍스처 API(`ImTextureRef`,
  `ImDrawList::_SelectLineTexture`) 이전이라 컴파일이 안 된다. 1.92 지원
  첫 라인인 master 7eeb916(2026-08-06)을 채택. 이 마스터는 API가
  리팩터링되어 `ImPlotCond`/offset 인자가 사라지고 `ImPlotSpec` 하나로
  통합됐다 — 링버퍼 오프셋은
  `ImPlotSpec(ImPlotProp_Offset, offset)`으로 전달(구판 튜토리얼 그대로
  따라가면 컴파일 에러).
- ImPlot 컨텍스트 수명: `ImPlot::CreateContext()`는 ImGui 컨텍스트 직후,
  `DestroyContext()`는 **ImGui 컨텍스트 소멸 전**(현재 컨텍스트를 참조함).
- 메모리 에디터는 헤더온리라 include 경로만 `imgui` 타깃에 추가.
  `MemoryEditor::DrawWindow`는 호출마다 `Open=true`로 되돌리므로 토글은
  반드시 `if (Open) DrawWindow(...)` 가드로 — 패널 체크박스와 X 버튼이
  같은 플래그를 공유해 동기화된다.
- taskmgr 플롯은 `ImPlotFlags_CanvasOnly` + `NoDecorations` +
  `SetupAxesLimits(0,90,0,100, Always)` 스파크라인 구성.

**검증 결과(서버 모드 스모크):**

- imguidemo: 메모리 에디터에 시드 패턴(`00..FF` + "jkwindow memory
  editor") 렌더. **편집 경로 E2E 확인** — 첫 바이트 클릭 후 키보드 "41"
  입력 → 0x6A('j')가 0x41('A')로 쓰이고 ASCII 컬럼이 "Akwindow"로 갱신,
  커서 자동 진행. 서버 키 이벤트→클라→백엔드→위젯→버퍼 쓰기 전 경로.
- taskmgr: ImPlot 플롯 박스 + CPU 라인 렌더. **플롯은 버튼 아래에
  그려진다**(레이아웃 순서: 테이블→버튼→플롯→IO dump) — 위쪽만 캡처해서
  "플롯이 안 나온다"고 오독하지 않도록 주의. 저사용률 프로세스(0-2%)의
  라인은 0-100 축 하단 1-2px에 붙어 눈에 안 보일 수 있음.
- 셀프테스트 0 failures. imguidemo 60.7 FPS. 서버 kill → 클라 전부
  자발 종료(exit 0).

**미착수 (장기 트랙 — 별도 계획 필요):** CEF/Ultralight OSR과 FFmpeg
비디오 플레이어. 둘 다 수백 MB급 외부 바이너리 의존성 + 라이선스/빌드
결정(CEF 바이너리 배포판, FFmpeg LGPL/GPL)이 선행되므로 리서치 문서의
OSR 5단계 파이프라인·A/V 싱크 분석(§11.2)과 함께 착수 시 전용 플랜을
세운다.