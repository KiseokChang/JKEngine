# JKENGINE 아키텍처 문서 모음

> JKENGINE(1995년경, Borland C++ 4.5, DOS 32-bit)의 전체 구조와 각 모듈별 아키텍처, 핵심 기술을 정리한 문서입니다. 이 문서는 추후 웹에서 구동 가능한 코드로 재구현(re-implementation)하기 위한 참고 자료로 사용됩니다.

---

## 문서 목록

| 파일 | 내용 | 우선순위 |
|------|------|---------|
| `01_overview.md` | JKENGINE 전체 구조, 레이어, 빌드 환경, 핵심 패턴 | ★★★ |
| `02_jkdbase.md` | JKDBASE 데이터 관리 레이어 상세 | ★★★ |
| `03_jkwindow.md` | JKWINDOW GUI/그래픽/이벤트 프레임워크 상세 | ★★★ |
| `04_windbase_jango.md` | WINDBASE/JANGO 애플리케이션 프레임워크 | ★★☆ |
| `05_2caocc_app.md` | WINDBASE/2CAOCC 애플리케이션 아키텍처 | ★★☆ |
| `06_web_port_roadmap.md` | 웹 포팅 로드맵 및 기술 스택 제안 | ★★★ |
| `10_sdl2_windows_setup.md` | Windows + VS Code + MSYS2 + SDL2 개발 환경 세팅 | ★★★ |
| `11_jkwindow_sdl_mapping.md` | JKWINDOW → SDL2 클래스 매핑 설계 | ★★★ |
| `99_file_inventory.md` | 주요 소스/헤더/리소스 파일 인벤토리 | ★★☆ |
| `README.md` | 본 문서 | ★★★ |

---

## JKENGINE 전체 아키텍처 요약

```
┌─────────────────────────────────────────────────────────────┐
│  Application Layer                                          │
│  - WINDBASE/JANGO      (인사/장비/2.4G 장비 관리)           │
│  - WINDBASE/2CAOCC     (포대/항공/사격/진지/좌표)           │
├─────────────────────────────────────────────────────────────┤
│  GUI Framework Layer                                        │
│  JKWINDOW                                                   │
│  - JKApplication, JKWindow, JKDialog, JKControl             │
│  - JKDC (Screen/Memory/Meta/Printer)                        │
│  - MessageQue, EventHandler, Mouse/Keyboard/Timer           │
│  - VESA256, ResourceManager, Hangul/VectorFont              │
├─────────────────────────────────────────────────────────────┤
│  Data Management Layer                                      │
│  JKDBASE                                                    │
│  - FILEHEADER, RecordManager(.dat), TableManager(.tbl)      │
│  - DataManager (CRUD/Search/Sort)                           │
│  - EntryBase/RecordBase, Number/Real/Date/Time/Boolean      │
├─────────────────────────────────────────────────────────────┤
│  DOS/VESA Platform Layer                                    │
│  - VESA BIOS, 256-color palette, interrupt-driven I/O      │
└─────────────────────────────────────────────────────────────┘
```

---

## 웹 포팅 시 권장 MVP 순서

1. **JKDBASE 데이터 마이그레이션** — 모든 업무의 기초
2. **JANGO 2.4G 장비 관리** — 전형적인 CRUD + 마스터-디테일 패턴
3. **JANGO 인사 관리** — 복잡한 데이터 바인딩
4. **2CAOCC 지도/좌표** — 고급 GUI/지도 기능

---

## 분석 진행 상태

- [x] JKDBASE 핵심 클래스 및 파일 형식 분석
- [x] JKWINDOW 메시지 루프/DC/컨트롤 계층 분석
- [x] JANGO 메인 앱 및 2.4G 장비 데이터 흐름 분석
- [x] 2CAOCC 파일 구조 및 도메인 추정
- [x] SDL2 기반 Windows 개발 환경 설계 (MSYS2/UCRT64 + CMake)
- [x] JKWINDOW → SDL2 클래스 매핑 설계
- [x] SDL2 JKWINDOW 프로토타입 코드 작성 (`prototype/sdl2_jkwindow/`)
- [ ] MSYS2 설치 및 SDL2 툴체인 구축 — 환경 준비 후 진행
- [ ] Prototype 빌드/실행 검증 — MSYS2 설치 후 진행
- [ ] 2CAOCC 세부 모듈(Coord, POS, Meta, BATT 등) 심층 분석 — 추가 요청 시 진행
- [ ] 기존 `.dat`/`.tbl`/`.res` 파일 해독 및 데이터 추출 — 추가 요청 시 진행

---

## 참고

- 모든 문서는 Markdown 형식입니다.
- 절대 경로 기준: `i:\progwork\JKENGINE\ARCHITECTURE_DOCS\`
- 원본 코드: `i:\progwork\JKENGINE\`
