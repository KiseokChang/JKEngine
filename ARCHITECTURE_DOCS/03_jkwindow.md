# JKWINDOW: GUI/그래픽 프레임워크 아키텍처

JKWINDOW는 JKENGINE의 **자체 GUI 프레임워크**로, 1990년대 OWL(ObjectWindows Library)과 Windows API의 영향을 받아 설계되었습니다. DOS 32-bit 환경에서 직접 VESA 그래픽 모드를 제어하고, 메시지 기반 이벤트 루프를 구현합니다.

## TL;DR

- **핵심**: `JKApplication` → `EventHandler`/`MessageQue` → `JKWindow` → `JKControl` 트리.
- **그리기**: `JKDC`(Device Context)가 Screen/Memory/Meta/Printer 4가지 백엔드를 추상화.
- **SDL2 프로토타입에서 살아남은 부분**: 컨트롤 트리, 메시지 라우팅, DC 그리기 명령. VESA256/프린터/메타파일은 제거.
- **빠른 찾기**: 파일 구성 §1, 클래스 계층 §2, 메시지 루프 §3, JKDC §4, 컨트롤 §5.

---

## 1. JKWINDOW 파일 구성

```
JKWINDOW/
├── JKAPP.H/.CPP           # JKApplication (최상위 앱)
├── JKMAIN.H/.CPP          # main() 진입점
├── JKINIT.H/.CPP          # 전역 초기화/종료
├── JKMODULE.H             # MainApplication 전역
├── JKWINDOW.H/.CPP        # JKWindow (창)
├── JKDIALOG.H/.CPP        # JKDialog (모달/모달리스 대화상자)
├── CONTROL.H/.CPP         # JKControl (모든 UI의 기반)
├── EVNTHAND.H/.CPP        # EventHandler (메시지 루프)
├── MSGQUE.H/.CPP          # 메시지 큐
├── MSGFILT.H/.CPP         # 메시지 필터/정규화
├── MSGCREAT.H/.CPP        # 메시지 생성자 기반 클래스
├── JKDC.H/.CPP            # Device Context (그리기 추상화)
├── JKSCRNDC.H/.CPP        # 화면 DC
├── MEMDC.H/.CPP           # 메모리 DC
├── METADC.H/.CPP          # 메타파일 DC
├── JKPRNDC.H/.CPP         # 프린터 DC
├── KGRAPHIC.H/.CPP        # 그래픽 함수 래퍼
├── GRBASE.H/.CPP          # 그래픽 기반
├── GRMAIN.H/.CPP          # 그래픽 메인
├── GRAPH_PM.H/.CPP        # 그래픽 포트/매니저
├── VESA256.H/.CPP         # VESA 256색 모드 직접 제어
├── RESMAN.H/.CPP          # 리소스 관리자
├── HANMAN.H/.CPP          # 한글 입출력 관리자
├── VFONTMAN.H/.CPP        # 벡터 폰트 관리자
├── MOUSEMAN.H/.CPP        # 마우스 이벤트 생성
├── KEYBDMAN.H/.CPP        # 키보드 이벤트 생성
├── TIMERMAN.H/.CPP        # 타이머 이벤트 생성
├── PRINTMAN.H/.CPP        # 프린터 관리
├── JKBUTTON.H/.CPP        # 버튼 컨트롤
├── JKEDIT.H/.CPP          # 에디트 컨트롤
├── LISTBOX.H/.CPP         # 리스트박스
├── COMBOBOX.H/.CPP        # 콤보 박스
├── SCROLBAR.H/.CPP        # 스크롤 바
├── CHECKBOX.H/.CPP        # 체크 박스
├── SPIN.H/.CPP            # 스핀 컨트롤
├── POPUPSTC.H/.CPP        # 팝업 정적 텍스트
├── OKDIALOG.H/.CPP        # OK 대화상자
├── YESNODLG.H/.CPP        # Yes/No 대화상자
├── FILEDLG.H/.CPP         # 파일 선택 대화상자
└── ... (기타 테스트/특수 컨트롤)
```

---

## 2. 클래스 계층 구조

```
DLLFrame<JKControl> (템플릿 리스트 기반)
    └── JKControl
        ├── JKApplication    : EventHandler를 상속, 전체 앱 윈도우
        ├── JKWindow         : 일반 윈도우
        │   └── JKDialog     : 대화상자(모달 루프 포함)
        ├── JKButton
        ├── JKStatic
        ├── JKEdit
        ├── JKListBox
        ├── JKComboBox
        ├── JKScrollBar
        ├── JKCheckBox
        └── ... 사용자 정의 컨트롤
```

- 모든 시각적 요소는 `JKControl`을 상속합니다.
- `JKControl`은 `DLLFrame<JKControl>`를 통해 이중 연결 리스트(prev/next)를 가집니다.
- 부모-자식 관계는 `Parent` 포인터 + `AddControl()`/`GetControl()`으로 구성됩니다.

---

## 3. JKControl: 모든 UI의 기반

```cpp
class JKControl : public DLLFrame<JKControl> {
    WORD    ControlID;       // 개발자가 지정한 ID
    DWORD   WinID;           // 런타임에서 할당한 고유 윈도우 ID
    JKControl *Parent;       // 부모 윈도우
    JKControl *prev, *next;  // 형제 리스트
    JKRect  WinRect;         // 부모 기준 좌표
    JKRect  ClientRect;      // 클라이언트 영역
    JKPoint ClientOrgPos;    // 클라이언트 원점 오프셋
    WORD    IsShow;          // 표시 상태
    WORD    WinAttr;         // 윈도우 속성 플래그
    RectQue ShowRegion;      // 실제로 보이는 영역(클리핑용)
    char*   WinText;         // 텍스트
    ...
};
```

### 3.1 핵심 가상 함수

| 함수 | 역할 |
|------|------|
| `InitWindow()` | 초기화(리소스 로드, 자식 생성) |
| `SetupWindow()` | 레이아웃 구성(주로 재정의) |
| `OpenWindow()` | 윈도우 열기 |
| `CloseWindow()` | 윈도우 닫기 |
| `ShowWindow()` / `HideWindow()` | 표시/숨김 |
| `PaintWindow(dc, rect)` | 비클라이언트 영역(테두리, 타이틀) 그리기 |
| `PaintClient(dc, rect)` | 클라이언트 영역 그리기 |
| `RespondMessage(msg)` | 메시지 처리 |
| `ProcessMessage(msg)` | 메시지 라우팅/자식에게 전달 |

### 3.2 DC 생성 함수

```cpp
JKDC* CreateScreenDC(JKDC& dc=*::GlobalDC);
JKDC* CreateWindowDC(JKRect rect, RectQue* minus=0, JKDC& dc=*::GlobalDC);
JKDC* CreateClientDC(JKRect rect, RectQue* minus=0, JKDC& dc=*::GlobalDC);
```

- 화면 DC, 윈도우 DC, 클라이언트 DC를 생성해 클리핑과 좌표 변환을 자동으로 처리합니다.

### 3.3 메시지 전송 함수

```cpp

---

## 5. 메시지 시스템

### 5.1 JKMSG

```cpp
class JKMSG {
    WORD  main;    // 메시지 종류
    DWORD sub;     // 보통 좌표 또는 컨트롤 ID
    DWORD detail;  // 추가 데이터
    DWORD winid;   // 대상 윈도우 ID
    WORD  option;  // 옵션/수정자
};
```

### 5.2 주요 메시지 분류

| 범위 | 의미 | 예 |
|------|------|-----|
| `NOEVENT` ~ `DEADKEYDOWN` | 키보드/문자 입력 | `CHARDOWN`, `KEYDOWN` |
| `ONLYMOVE` ~ `RIGHTDRAGGING` | 마우스(클라이언트) | `LEFTCLICK`, `LEFTDRAGGING` |
| `NCONLYMOVE` ~ `NCRIGHTDRAGGING` | 마우스(논클라이언트) | `NCLEFTCLICK` |
| `ELONLYMOVE` ~ `ELRIGHTDRAGGING` | 마우스(윈도우 요소) | `ELLEFTCLICK` |
| `TIMERMESSAGE` ~ | 타이머 | `TIMERACTIVE`, `TIMERHAPPEN` |
| `CLOSEWINDOW` ~ `REDRAWCLIENT` | 윈도우 관리 | `SHOWWINDOW`, `REPAINT` |
| `COMMAND` | 커맨드 메시지 | `COMMAND + cmdid` |
| `SETFOCUS` ~ `APPEXIT` | 포커스/커서/앱 종료 | `SETFOCUS`, `HIDECURSOR` |
| `CONTROLMESSAGE` ~ | 컨트롤별 메시지 | 버튼, 리스트박스, 에디트 등 |
| `DIALOGMESSAGE` | 다이얼로그 결과 | `ID_OK`, `ID_CANCEL`, `ID_YES`, `ID_NO` |
| `USERMESSAGE` | 사용자 정의 메시지 시작 | `USERMESSAGE`, `USERMESSAGE+1`... |

### 5.3 컨트롤 ID 범위

```cpp
#define ID_CHECKBOX     0x1000
#define ID_STATIC       0x2000
#define ID_BUTTON       0x3000
#define ID_EDIT         0x4000
#define ID_POPUP        0x5000
#define ID_LISTBOX      0x6000
#define ID_SCROLLBAR    0x7000
#define ID_SPIN         0x8000
#define ID_SUBWINDOW    0x9000
#define ID_USERCONTROL  0xa000
```

---

## 6. JKDC: 그리기 추상화

### 6.1 클래스 계층

```
JKDC (순수 가상 그리기 함수)
    ├── JKScreenDC  : VESA 실제 화면에 직접 그림
    ├── JKMemDC     : 메모리 버퍼(더블 버퍼링, 오프스크린)
    ├── JKMetaDC    : 메타파일(그리기 명령 기록)
    └── JKPrinterDC : 프린터 출력
```

### 6.2 JKDC 핵심 기능

```cpp
class JKDC {
    JKPen   CurrentPen;
    JKBrush CurrentBrush;
    BYTE    TextColor, BackColor;
    BOOL    WriteMode;
    JKPoint CurrentPos;
    JKPoint TempOrgPos;
    RectQue ClipRegion;

    virtual void PutPixel(...);
    virtual void Line(...);
    virtual void SolidBar(...);
    virtual void Rectangle(...);
    virtual void Circle(...);
    virtual void Arc(...);
    virtual void DrawPolygon(...);
    virtual void FillPolygon(...);
    virtual void PutCh(...);
    virtual void TextOut(...);
    virtual void TextOutX(...);

---

## 10. 한글/폰트 지원

- `HanMan`(`HangulManager`): 한글 조합/완성형 처리 및 출력
- `VectorFontManager`: 벡터 폰트 로드/출력
- `JKDC::PutCh()`, `JKDC::HanPutCh()`: 한글/영문 출력

---

## 11. 핵심 기술적 특징

1. **DOS/VESA 직접 제어**: 운영체제 GUI 없이 직접 그래픽 하드웨어를 다룹니다.
2. **OWL 스타일 메시지 루프**: `Run()`, `RespondMessage()`, `EvXxx` 가상 함수 체계.
3. **이중 연결 리스트 기반 윈도우 트리**: 모든 컨트롤이 prev/next로 연결됩니다.
4. **DC 추상화**: 화면/메모리/프린터를 동일한 인터페이스로 그립니다.
5. **리페인트 클리핑**: `ShowRegion`, `RectQue`를 이용해 무효 영역만 다시 그립니다.
6. **팩 파일 리소스**: 여러 이미지/팔레트를 하나의 `.dat` 파일에 패킹해 관리합니다.

---

## 12. 웹 포팅 시 JKWINDOW 대체 전략

| 원본 JKWINDOW | 웹 대안 | 비고 |
|---------------|--------|------|
| `JKApplication` | React/Vue/Svelte 최상위 App 컴포넌트 | 상태 관리로 대체 |
| `JKWindow`/`JKDialog` | DIV 기반 모달/윈도우 + CSS | z-index, position absolute |
| `JKControl` | 컴포넌트 클래스 | props/state 이벤트 핸들러 |
| `JKMSG`/`MessageQue` | JS 이벤트 루프 + 커스텀 이벤트 | 브라우저 이벤트로 대체 |
| `JKDC` | HTML5 Canvas 2D Context | 직접 그리기 필요 시 |
| `JKButton`/`JKEdit` 등 | HTML `<button>`, `<input>`, `<select>` | 기본 폼 요소 사용 권장 |
| VESA 256 | Canvas/CSS 또는 DOM | 색상 제한은 의도적으로 제거해도 무방 |
| `ResourceManager` | Webpack/Vite asset + IndexedDB | 정적 리소스는 번들링 |
| `HanMan` | 브라우저 기본 한글 입력 | 한글 코드 변환은 유지 필요 시 별도 모듈 |
| `TimerMan` | `setInterval`/`setTimeout` | JS 타이머로 대체 |
| `MouseEvents`/`KeyboardEvents` | DOM mouse/keyboard events | `addEventListener` |

---

## 13. JKWINDOW 초기화 순서

```cpp
BOOL InitJKProgram() {
    InitGraph();          // 1. VESA 모드 설정
    ResMan = new ResourceManager();
    HanMan = new HangulManager();
    GlobalDC = new JKScreenDC();
    GlobalVFontMan = new VectorFontManager(".");
    SystemMsgQue = new MessageQue();
    MsgQue = new MessageQue();
    TimerMan = new TimerManager();
    MouseEvents = new MouseManager();
    KeyboardEvents = new KeyboardManager();
    PrintMan = new PrintManager();
    MsgFilter = new MessageFilter();
    return TRUE;
}
```

- 위 순서대로 전역 객체를 생성하므로, 웹 포팅 시에도 동일한 초기화 단계를 명시적으로 재현하는 것이 좋습니다.

    virtual void _PutImage(...);
    virtual void _GetImage(...);
};
```

### 6.3 펜/브러시

```cpp
class JKPen {
    WORD Pen;       // 비트 패턴
    WORD Thick;     // 두께
};
class JKBrush {
    int8 Brush[8];  // 8x8 비트 패턴
};
```

- 표준 펜/브러시가 미리 정의되어 있습니다.

---

## 7. VESA 256 그래픽

```cpp
#define VESA_640X480X256    0x101
#define VESA_800X600X256    0x103
#define VESA_1024X768X256   0x105

void SetMode(INT mode);
void EndMode();
void PutPixel(INT x, INT y, INT color);
INT  GetPixel(INT x, INT y);
```

- VESA BIOS 확장을 통해 256색 모드로 전환합니다.
- 팔레트는 768바이트(RGB x 256색)로 직접 제어합니다.
- `JKScreenDC`는 이 저수준 함수들을 호출해 화면에 출력합니다.

---

## 8. 리소스 관리: ResourceManager

```cpp
#define RES_IMAGE    0x0000
#define RES_STRING   0x0001
#define RES_CURSOR   0x0002
#define RES_PALETTE  0x0003

#define RT_DYNAMIC   0x01
#define RT_FILE      0x02
#define RT_PACKFILE  0x04
```

```cpp
class ResourceBase {
    WORD     ResourceID;
    uint16   ResourceKind;
    BYTE*    RealData;
    BYTE     ResourceType;
    uint16   ResourceIndex;
};

class ResourceManager : public DLLFrame<ResourceBase> {
    ResourceBase* GetResource(WORD resid, uint16 kind=RES_IMAGE);
    BOOL AddResource(ResourceBase* res);
    BOOL DeleteResource(WORD resid, uint16 kind=RES_IMAGE);
};
```

- **팩 파일**: `jkimage.dat`, `jkpalett.dat`, `insaimg.dat` 등 패키지 파일에서 인덱스로 접근
- **개별 파일**: `funcres.res`, `iconres.res`, `moonres.res` 등 독립 리소스 파일
- **동적**: 런타임에 생성된 데이터

---

## 9. 입력 장치 추상화

### 9.1 MouseManager

```cpp
class MouseManager : public MessageCreator {
    JKPoint LastDrawPos, LastCheckPos;
    int16   CursorShowFlag;
    JKRect  ActiveRegion;
    WORD    CursorID;

    BOOL CreateMessage(MessageQue* msgque=::SystemMsgQue);
    void ShowCursor();
    void HideCursor();
    void SetCursorPos(JKPoint pos);
    JKPoint GetCursorPos();
    BOOL SetCursorID(WORD resid);
};
```

- DOS 인터럽트 `0x33`을 통해 마우스 상태를 읽어 메시지를 생성합니다.
- 내부 커서 이미지 10종이 내장되어 있습니다.

### 9.2 KeyboardManager

```cpp
class KeyboardManager : public MessageCreator {
    uint16 CharCode;
    uint16 KeyKind;
    uint16 Modifier;

    BOOL CreateMessage(MessageQue* msgque=::SystemMsgQue);
};
```

- BIOS 키보드 상태를 읽어 `CHARDOWN`/`KEYDOWN` 메시지를 생성합니다.

### 9.3 TimerManager

```cpp
class TimerManager : public MessageCreator, public DLLFrame<TimerMember> {
    BOOL SetTimer(DWORD winid, DWORD interval);
    void KillTimer(DWORD winid);
    BOOL CreateMessage(MessageQue* msgque=::SystemMsgQue);
};
```

- DOS 타이머 인터럽트(`0x08`)를 후킹해 주기적 메시지를 생성합니다.

void SendMessage(WORD main, DWORD sub=0, DWORD detail=0, WORD option=0);
void ExecuteMessage(WORD main, DWORD sub=0, DWORD detail=0, WORD option=0);
void SendCommand(WORD cmdid, WORD data=0, DWORD detail=0, WORD option=0);
...
```

- `Send`: 메시지 큐에 추가 (비동기)
- `Execute`: 즉시 처리 (동기)

---

## 4. JKApplication / EventHandler: 앱과 메시지 루프

### 4.1 JKApplication

```cpp
class JKApplication : public EventHandler, public JKControl {
    BOOL IsInitApp;
    BYTE OldPalette[768];

    void InitApplication();   // 팔레트/리소스 초기화
    void CloseApplication();    // 팔레트 복원
    WORD Run();                 // 메시지 루프 실행
    void InitResourceMan();     // 내부 리소스 등록
};
```

- `JKApplication`은 `EventHandler`와 `JKControl`을 다중 상속합니다.
- 생성자에서 `JKRect(0, 0, getmaxx(), getmaxy())`로 전체 화면 크기의 컨트롤을 만듭니다.

### 4.2 EventHandler 메시지 루프

```cpp
WORD EventHandler::Run() {
    while(1) {
        if(MsgFilter->PatchMessage()) {
            MsgFilter->TranslateMessage();
        }
        while(MsgQue->GetMessage(msg)) {
            if(!PreProcessMessage(msg)) { LoopBreak=TRUE; break; }
            MsgFilter->MessageNormalize(*MsgQue);
            if(LoopBreak) break;
        }
        if(LoopBreak) break;
    }
}
```

1. `PatchMessage()`: 하드웨어 이벤트(마우스/키보드/타이머)를 `SystemMsgQue`에서 수집
2. `TranslateMessage()`: 좌표/포커스 변환
3. `GetMessage()`: 애플리케이션 큐에서 메시지 꺼냄
4. `PreProcessMessage()`: 전역 메시지(APPEXIT 등) 처리
5. `ProcessMessage()`: 대상 윈도우/컨트롤로 라우팅
6. `MessageNormalize()`: 리페인트 등 후처리
