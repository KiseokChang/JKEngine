# JKENGINE 아키텍처 문서 모음

> JKENGINE(1995년경, Borland C++ 4.5, DOS 32-bit)의 전체 구조와 각 모듈별 아키텍처, 핵심 기술을 정리한 문서입니다. 이 문서는 추후 웹에서 구동 가능한 코드로 재구현(re-implementation)하기 위한 참고 자료로 사용됩니다.

---

## 문서 목록

| 파일 | 한 줄 요약 | 우선순위 | 길이(줄) |
|------|-----------|---------|---------|
| `01_overview.md` | JKENGINE 전체 구조, 레이어, 빌드 환경, 핵심 패턴 | ★★★ | 141 |
| `02_jkdbase.md` | JKDBASE 데이터 관리 레이어 상세 | ★★★ | 306 |
| `03_jkwindow.md` | JKWINDOW GUI/그래픽/이벤트 프레임워크 상세 | ★★★ | 449 |
| `04_windbase_jango.md` | WINDBASE/JANGO 애플리케이션 프레임워크 | ★★☆ | 295 |
| `05_2caocc_app.md` | WINDBASE/2CAOCC 애플리케이션 아키텍처 | ★★☆ | 235 |
| `06_web_port_roadmap.md` | 웹 포팅 로드맵 및 기술 스택 제안 | ★★★ | 217 |
| `10_sdl2_windows_setup.md` | Windows + VS Code + MSYS2 + SDL2 개발 환경 세팅 | ★★★ | 269 |
| `11_jkwindow_sdl_mapping.md` | JKWINDOW → SDL2 클래스 매핑 설계 | ★★★ | 321 |
| `12_sdl2_prototype_roadmap.md` | SDL2 JKWindow 프로토타입 로드맵 | ★★★ | 195 |
| `13_sdl2_top_level_apps.md` | SDL2 기반 최상위 앱/기능 후보 | ★★☆ | 124 |
| `14_sdl2_window_dpi.md` | SDL2 윈도우 배치·DPI/좌표계/렌더링 파이프라인 | ★★☆ | 362 |
| `15_verification_playbook.md` | 창/DPI 화면 검증 방법론·14항목 자동 검증 플레이북 | ★★☆ | 187 |
| `99_file_inventory.md` | 주요 소스/헤더/리소스 파일 인벤토리 | ★★☆ | 265 |
| `README.md` | 본 문서 | ★★★ | 93 |

### 빠른 참조: 어떤 문서를 먼저 볼까

- **처음 접할 때**: `01_overview.md` → `03_jkwindow.md` → `02_jkdbase.md`
- **SDL2 프로토타입 작업 중**: `.claude/PROJECT.md` → `11_jkwindow_sdl_mapping.md` → `14_sdl2_window_dpi.md` → `15_verification_playbook.md`
- **화면 좌표/배율 버그**: `14_sdl2_window_dpi.md` §11-§12
- **검증 방법/스크립트**: `15_verification_playbook.md`
- **포팅 우선순위/로드맵**: `06_web_port_roadmap.md`, `12_sdl2_prototype_roadmap.md`

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
- [x] SDL2 JKWindow 프로토타입 로드맵 작성 (`12_sdl2_prototype_roadmap.md`)
- [x] Phase 2~4 (Layout/Rendering/Resource) 브랜치 구현 확인 (`phase2-full-stack`)
- [x] SDL2 기반 최상위 앱/기능 후보 검토 (`13_sdl2_top_level_apps.md`)
- [x] 창 상단 잘림(DPI 스케일링) 수정 — 프레임 중앙 배치 + 클라이언트 원점 보정 (`phase2-full-stack` 작업 트리)
- [x] 윈도우 배치/DPI/좌표계 아키텍처 문서화 (`14_sdl2_window_dpi.md`)
- [x] 화면 검증 방법론 정리 + 검증 스크립트 레포 편입 (`15_verification_playbook.md`, `tools/verify_fixwin3.ps1`)
- [ ] Phase 1 Input/Focus System 마무리 — `phase1_input_focus.md` 기준 진행 중
- [x] MSYS2 설치 및 SDL2 툴체인 구축 (UCRT64 + CMake/Ninja — `build_sdl2_jkwindow.bat` 동작)
- [x] Prototype 빌드/실행 검증 (2026-08 창 수정 작업 시 메인/jango/occ/test 모드 실행·셀프테스트 통과)
- [ ] 2CAOCC 세부 모듈(Coord, POS, Meta, BATT 등) 심층 분석 — 추가 요청 시 진행
- [ ] 기존 `.dat`/`.tbl`/`.res` 파일 해독 및 데이터 추출 — 추가 요청 시 진행

---

## 참고

- 모든 문서는 Markdown 형식입니다.
- 절대 경로 기준: `i:\progwork\JKENGINE\ARCHITECTURE_DOCS\`
- 원본 코드: `i:\progwork\JKENGINE\`
