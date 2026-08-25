# JKENGINE 전체 구조 개요

> 작성 목적: 1995년경 개발된 JKENGINE/JKDBASE/JKWINDOW/WINDBASE/JANGO 소스를 분석하고, 향후 웹에서 구동 가능한 코드로 재구현(re-implementation)할 수 있도록 핵심 아키텍처와 데이터 흐름을 정리합니다.

---

## 1. 프로젝트 개요

| 항목 | 내용 |
|------|------|
| 프로젝트명 | JKENGINE |
| 시대 | 1995년경 (소스/빌드 환경으로 추정) |
| 언어 | C++ (Borland C++ 4.5 문법 기반) |
| 플랫폼 | MS-DOS, 32-bit DOS extender 기반 GUI 애플리케이션 |
| 빌드 도구 | `JKENGIN.IDE`, `JKENGIN.DSW` (Borland IDE/Workshop 프로젝트 파일) |
| GUI 스타일 | OWL(ObjectWindows Library)과 유사한 자체 GUI 프레임워크 |
| 그래픽 | VESA 256색 모드(640x480/800x600/1024x768) 직접 제어 |

---

## 2. 최상위 디렉터리 구조

```
i:\progwork\JKENGINE
├── ARCHITECTURE_DOCS        # 본 문서 폴더
├── JKDBASE                  # 데이터 관리 라이브러리
├── JKWINDOW                 # GUI/그래픽/이벤트 프레임워크
├── RESOUCES                 # 리소스(아이콘 등, 폴더명 오타로 보임)
└── WINDBASE                 # 업무 애플리케이션 레이어
    ├── 2CAOCC               # 2CA OCC(?) 애플리케이션
    └── JANGO                # JANGO GWES 애플리케이션
```

---

## 3. 레이어드 아키텍처

```
┌─────────────────────────────────────────────────────────────┐
│  Application Layer                                          │
│  WINDBASE/JANGO, WINDBASE/2CAOCC                            │
│  - 업무 로직, 폼/다이얼로그, 리포트, 데이터 바인딩            │
├─────────────────────────────────────────────────────────────┤
│  GUI Framework Layer                                        │
│  JKWINDOW                                                   │
│  - JKApplication, JKWindow, JKDialog, JKControl             │
│  - 메시지 루프, 이벤트 처리, DC/그래픽, 리소스 관리          │
├─────────────────────────────────────────────────────────────┤
│  Data Management Layer                                      │
│  JKDBASE                                                    │
│  - 파일 기반 레코드/테이블 관리, Entry/Record 스키마         │
├─────────────────────────────────────────────────────────────┤
│  DOS/VESA Platform Layer                                    │
│  - VESA 256, 인터럽트 기반 마우스/키보드/타이머              │
└─────────────────────────────────────────────────────────────┘
```

---

## 4. 핵심 설계 패턴

| 패턴 | 적용 위치 | 설명 |
|------|----------|------|
| **이중 연결 리스트 템플릿** | `tmpltdll.h` (`DLLFrame<T>`) | 거의 모든 클래스의 컬렉션 기반. `JKControl`, `EntryBase`, `RecordBase`, `ResourceBase`, `MSGContainer`, `TimerMember` 등이 상속 또는 포함하여 사용 |
| **문서/뷰 유사 구조** | `JKControl` + `RecordViewBase` | OWL의 TWindow/TDoc/TView 개념을 단순화. 제어(Control)가 자신의 영역을 직접 그리고 메시지를 처리 |
| **DC(Device Context)** | `JKDC` 및 파생 클래스 | GDI의 DC를 모방. 화면(`JKScreenDC`), 메모리(`JKMemDC`), 메타파일(`JKMetaDC`) 등으로 분리 |
| **메시지 기반 이벤트** | `JKMSG`, `MessageQue`, `EventHandler` | Windows API의 MSG/PeekMessage/DispatchMessage를 모방한 자체 메시지 큐 |
| **스키마-레코드 분리** | `EntryBase`/`RecordBase` | 데이터 필드(Entry)를 조합해 레코드(Record)를 구성. DB의 테이블 행 개념 |
| **팩토리/등록식 리소스** | `ResourceManager` | 파일 또는 메모리 기반 팔레트/이미지/커서/문자열을 ID로 관리 |

---

## 5. 주요 글로벌 싱글턴

`JKWINDOW` 초기화(`InitJKProgram`) 시 다음 전역 객체가 생성됩니다.

| 전역 객체 | 파일 | 역할 |
|-----------|------|------|
| `ResMan` | `resman.h/cpp` | 이미지/팔레트/문자열/커서 리소스 관리 |
| `HanMan` | `hanman.h/cpp` | 한글 출력/입력 관리 |
| `GlobalDC` | `jkscrndc.h/cpp` | 화면 전체 Device Context |
| `GlobalVFontMan` | `vfontman.h/cpp` | 벡터 폰트 관리 |
| `SystemMsgQue` | `msgque.h/cpp` | 시스템 메시지 큐(하드웨어 이벤트 수신) |
| `MsgQue` | `msgque.h/cpp` | 애플리케이션 메시지 큐 |
| `TimerMan` | `timerman.h/cpp` | 타이머/주기적 메시지 생성 |
| `MouseEvents` | `mouseman.h/cpp` | 마우스 상태 감지 및 메시지 생성 |
| `KeyboardEvents` | `keybdman.h/cpp` | 키보드 상태 감지 및 메시지 생성 |
| `PrintMan` | `printman.h/cpp` | 프린터 출력 관리 |
| `MsgFilter` | `msgfilt.h/cpp` | 메시지 정규화, 포커스, 캡처, 리페인트 영역 계산 |

---

## 6. 진입점 흐름

```cpp
// JKMAIN.CPP
void main() {
    if(InitJKProgram()) {                 // 1. 그래픽/메시지/입력 초기화
        MainApplication = CreateApplication(); // 2. 앱 객체 생성 (링크 시 외부 정의)
        if(MainApplication) {
            MainApplication->Run();       // 3. 메시지 루프 실행
            delete MainApplication;
        }
        CloseJKProgram();                 // 4. 종료 및 자원 해제
    }
}
```

`CreateApplication()`은 프로젝트별로 다르게 링크됩니다.
- `JANGO`: `JangoApplication` 생성
- `2CAOCC`: `OCCApplication` 생성

---

## 7. 웹 포팅 시 핵심 고려사항(요약)

| 원본 기술 | 웹 대체안 | 난이도 |
|-----------|----------|--------|
| VESA 256 그래픽 | HTML5 Canvas / CSS / DOM | 중간 |
| DOS 인터럽트 마우스/키보드 | DOM 이벤트(mouse/keyboard) | 낮음 |
| 파일 I/O (fopen/open/read/write) | IndexedDB / File API / 서버 DB | 중간~높음 |
| 8.3 파일명/고정 레코드 파일 | JSON/IndexedDB/SQLite(WASM) | 중간 |
| 한글 입출력 | 브라우저 기본 한글 입력 | 낮음 |
| OWL 메시지 루프 | JS 이벤트 루프 + 상태 관리 | 중간 |
| 프린터 출력 | 브라우저 인쇄 / PDF 생성 | 낮음~중간 |

> 자세한 포팅 로드맵은 `06_web_port_roadmap.md`를 참고하세요.

---

## 8. 문서 구성

| 문서 | 내용 |
|------|------|
| `01_overview.md` | 전체 구조 (본 문서) |
| `02_jkdbase.md` | 데이터 관리 레이어 상세 |
| `03_jkwindow.md` | GUI/그래픽 프레임워크 상세 |
| `04_windbase_jango.md` | WINDBASE/JANGO 앱 프레임워크 상세 |
| `05_2caocc_app.md` | 2CAOCC 애플리케이션 상세 |
| `06_web_port_roadmap.md` | 웹 포팅 로드맵 |
| `99_file_inventory.md` | 소스 파일 목록 |
