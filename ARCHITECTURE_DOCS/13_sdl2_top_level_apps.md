# SDL2 JKWindow 기반 최상위 애플리케이션/기능 후보

> JKENGINE의 원래 아키텍처는 **JKDBASE → JKWINDOW → WINDBASE(JANGO/2CAOCC)** 3계층 구조입니다. SDL2 JKWindow 프로토타입(Phase 0~5)이 안정화되면, 그 위에 올릴 수 있는 실제 애플리케이션(최상위 앱)과 실용 기능 후보를 검토합니다.

---

## 1. 검토 기준

| 기준 | 설명 |
|------|------|
| **계승성** | 원본 JKENGINE(WINDBASE/JANGO/2CAOCC)의 업무 가치나 구조를 얼마나 살리는가 |
| **기술적 준비도** | Phase 0~5에서 완성해야 할 JKWindow/JKControl/JKDBASE 기능 의존도 |
| **실현 난이도** | 단기(1~2주), 중기(1~2개월), 장기(3개월+) 기준 |
| **데모/검증 가치** | SDL2 GUI 프레임워크를 증명하는 데 얼마나 효과적인가 |

---

## 2. 후보 범주

### 2.1 원본 JKENGINE 애플리케이션 복원

원본 C++ 코드의 업무 로직과 데이터 모델을 SDL2/C++로 재구현합니다. JKENGINE의 궁극적 목표와 가장 잘 맞습니다.

| 앱/기능 | 원본 모듈 | 핵심 내용 | 난이도 | 의존 기능 |
|---------|-----------|-----------|--------|-----------|
| **JANGO 인사 관리** | `JANGO/insa*`, `inwon*`, `persn*` | 인사/인원/출생/전역 등록, 수정, 조회 | 중기 | Dialog, Edit, ListBox, ComboBox, 데이터 바인딩 |
| **JANGO 2.4G 장비 관리** | `JANGO/eqp24*`, `EQ24DMAN`, `EQP24DEF` | 장비명/종류 마스터-디테일, 수량 누적, 검색 | 중기 | JKDBASE Table/Record, Dialog, Button, Edit |
| **JANGO 부대 변경/비밀번호** | `passdlg.cpp`, `budaedlg.cpp` | 비밀번호 다이얼로그, 부대 선택 | 단기 | Modal, Focus, MessageBox, 파일 선택 |
| **2CAOCC 메뉴/포대/항공/사격/진지** | `OCCAPP`, `BATT*`, `AIR*`, `FIRE*`, `PLNTG*` | 군용 좌표/지도 기반 C2 애플리케이션 | 장기 | Menu, 지도 캔버스, 좌표 변환, 복합 다이얼로그 |
| **2CAOCC 좌표 변환/지도 뷰어** | `real2pix.cpp`, `calccord.cpp`, `cordview.cpp` | 실제 좌표↔화면 픽셀, 거리/방위 계산, 기호 표시 | 중기 | Canvas/DC, Scroll, Zoom, 심볼 리소스 |

**권장 진입점**: JANGO 2.4G 장비 관리가 전형적인 CRUD + 마스터-디테일 패턴이며, `06_web_port_roadmap.md`에서 웹 포팅 MVP 1순위로 권장됩니다.

---

### 2.2 JKDBASE 데이터 도구

원본 바이너리 `.dat`/`.tbl` 자산을 해독하고, 현대에서도 쓸 수 있는 데이터 도구를 만듭니다.

| 앱/기능 | 내용 | 난이도 | 의존 기능 |
|---------|------|--------|-----------|
| **JKDBASE 데이터 뷰어** | `.dat`/`.tbl` 파일을 열어 레코드/필드/팔레트 구조를 표시 | 단기 | JKDBASE 엔진, Table, ListBox, Hex/text 뷰 |
| **JKDBASE 레코드 편집기** | 레코드 추가/삭제/검색/정렬, Entry 타입별 입력 UI | 중기 | EntryBase 파생, Edit, ComboBox, Validation |
| **DataFileManager 뷰어** | `DFILEMN` 기반 파일 매니저 재구현 | 단기 | FileDialog, ListBox, 메타데이터 표시 |
| **CSV/JSON ↔ JKDBASE 변환기** | 레거시 데이터를 현대 포맷으로 수출/수입 | 중기 | JKDBASE 직렬화, 파일 I/O, 진행률 UI |
| **리소스 팩(.res) 추출기** | 이미지/아이콘/팔레트를 PNG/JSON으로 보내기 | 중기 | Resource Manager, Image decode, FileDialog |

**가치**: 모든 상위 앱의 기반이 되는 데이터 마이그레이션을 가능하게 하며, `06_web_port_roadmap.md`의 Phase 0과 직결됩니다.

---

### 2.3 GUI 개발 도구

SDL2 JKWindow 자체를 개발/확장할 때 필요한 도구입니다.

| 앱/기능 | 내용 | 난이도 | 의존 기능 |
|---------|------|--------|-----------|
| **JKWindow 폼 빌더** | Button/Edit/ListBox 등을 드래그앤드롭으로 배치하고 `.frm`/`.cpp` 코드 생성 | 중기 | 모든 컨트롤, Mouse drag, Canvas, 파일 저장 |
| **리소스/아이콘 에디터** | 256색 팔레트 이미지, 아이콘, 심볼을 편집하고 `.res` 팩에 저장 | 중기 | OffscreenSurface, Palette, File I/O, Undo |
| **폰트 뷰어/에디터** | JKENGINE 한글/벡터 폰트를 시각화하고 수정 | 중기 | HangulManager, JKEdit, 폰트 리소스 포맷 |
| **테마/스킨 에디터** | 색상, 테두리, 폰트 스타일을 바꿔 GUI 테마를 저장/적용 | 단기 | ResourceCache, Style properties, 색상 선택 Dialog |
| **컨트롤 데모 쇼케이스** | Button, Edit, CheckBox, ComboBox, Menu, Dialog 등 모든 기능을 한 화면에서 시연 | 단기 | 전체 컨트롤, Focus, Tab, Modal |

**가치**: 폼 빌더는 나중에 JANGO/2CAOCC 화면을 재구현할 때도 생산성을 크게 높입니다.

---

### 2.4 현대적 확장 및 부가 앱

SDL2/CMake/C++ 기반 데스크톱 앱으로서 새로운 가능성을 보여줍니다.

| 앱/기능 | 내용 | 난이도 | 의존 기능 |
|---------|------|--------|-----------|
| **SDL2 한글 메모장** | 한글 입력(JKEdit + JHK)을 활용한 텍스트 편집/저장 | 단기 | JKEdit, Hangul, FileDialog, Menu |
| **CSV/JSON 테이블 뷰어** | 현대 데이터 파일을 JKDBASE-like 그리드로 조회/필터 | 중기 | ListBox/Grid, Sort, Search, File I/O |
| **SQLite 기반 데이터 앱** | JKDBASE 대신 SQLite를 백엔드로 쓰는 CRUD 앱 | 중기 | JKDBASE 추상화, SQL wrapper, Dialog |
| **차트/통계 뷰어** | JANGO의 `bargraph/piechart`를 SDL2 Canvas로 재구현 | 중기 | DC/Canvas, 데이터 집계, 범례/축 UI |
| **단순 2D 게임/시뮬레이션** | SDL2에 적합한 예: 지도 위 항적 시뮬레이션, 전술 시뮬레이션 | 중기 | Timer, Animation, DC, 키보드/마우스 입력 |
| **JKENGINE 용어/코드북 사전** | JKENGINE/JANGO/2CAOCC 구조체/명령어/약어 정리 도구 | 단기 | Static, ListBox, Search, HTML/markdown 뷰 |

---

## 3. 우선순위/의존성 매트릭스

```
Phase 0~5   Phase 6A        Phase 6B        Phase 7         Phase 8
(프레임워크) (데이터/데모)   (원본 MVP)      (개발 도구)     (고급 앱)
│           │               │               │               │
│           ├── 컨트롤 데모   ├── JANGO 2.4G  ├── 폼 빌더    ├── 2CAOCC 좌표/지도
│           ├── 데이터 뷰어 ├── 메모장     ├── 리소스 에디터 ├── SQLite CRUD 앱
│           ├── CSV/JSON     ├── DFM 뷰어   ├── 폰트/테마   ├── 게임/시뮬레이션
│              변환기         └── 비밀번호/부대 변경
└───────────────────────────────────────────────────────────────────────▶
```

---

## 4. 권장 최상위 앱 선택

| 목표 | 추천 앱 | 이유 |
|------|---------|------|
| **JKENGINE 가치 증명** | JANGO 2.4G 장비 관리 | CRUD+마스터-디테일 원본 업무를 재현 |
| **GUI 프레임워크 검증** | 컨트롤 데모 쇼케이스 | 모든 Phase 0~5 기능을 한눈에 보여줌 |
| **데이터 마이그레이션** | JKDBASE 데이터 뷰어/편집기 | 웹 포팅 및 분석의 핵심 인프라 |
| **개발 생산성** | 폼 빌더 + 리소스 에디터 | 상위 앱 UI 재구현 속도를 높임 |
| **SDL2 강점 활용** | 지도/좌표 뷰어 또는 2D 시뮬레이션 | Canvas/Timer/입력을 극대화 |

---

## 5. 다음 단계

1. Phase 0~5(Input/Focus/Layout/Rendering/Resource)가 완료되면, **컨트롤 데모 쇼케이스**를 먼저 제작해 GUI 프레임워크를 검증합니다.
2. 동시에 **JKDBASE 데이터 뷰어**를 병행하여 원본 `.dat`/`.tbl` 해독 기반을 확보합니다.
3. 이 두 가지가 준비되면 **JANGO 2.4G 장비 관리**를 SDL2 버전의 첫 번째 실제 업무 앱으로 삼습니다.
4. 2CAOCC, 폼 빌더, SQLite 연동 등은 그 이후 단계별로 확장합니다.

---

## 6. 참고 문서

- `04_windbase_jango.md` — JANGO/WINDBASE 애플리케이션 구조
- `05_2caocc_app.md` — 2CAOCC 애플리케이션 구조
- `06_web_port_roadmap.md` — 웹 포팅 시 권장 MVP 순서
- `12_sdl2_prototype_roadmap.md` — SDL2 JKWindow 프로토타입 단계
