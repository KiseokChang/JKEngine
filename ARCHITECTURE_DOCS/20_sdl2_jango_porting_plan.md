# SDL2 JKWindow 기반 원본 비즈니스 앱 포팅 계획

> WINDBASE/JANGO, WINDBASE/2CAOCC에 남아 있는 원본 JKENGINE 애플리케이션을 `prototype/sdl2_jkwindow` 위로 포팅하는 단계별 계획입니다.  
> 원본 파일은 그대로 두고, SDL2 프로토타입 API에 맞춰 **새로운 앱 클래스**를 작성합니다.

---

## 1. 원본 최상위 앱 진입점

| 앱 | 진입 파일 | 핵심 클래스 | 특징 |
|---|---|---|---|
| **JANGO 메뉴/런처** | `WINDBASE/JANGO/JANGO.CPP` | `JangoApplication` | 6개 메뉴 버튼, 비밀번호/부대변경/파일선택 다이얼로그, 하위 업무 윈도우 실행 |
| **2.4G 장비 관리** | `WINDBASE/JANGO/EQP24.CPP` | `Equip24Application`/`Equip24Window` | 마스터-디테일 CRUD, 다량의 커스텀 리스트/다이얼로그 |
| **구 장비 관리** | `WINDBASE/JANGO/EQUIP.CPP` | `EquipWindow` | 독립 실행형, `main()` 직접 초기화 |
| **2CAOCC C2 앱** | `WINDBASE/2CAOCC/OCCMAIN.CPP` | `OCCApplication`/`OCCWindow` | 메뉴바, 지도/좌표, 선택 다이얼로그 |

`ADDEQP24.CPP`는 `EQP24.CPP`와 동일한 `Equip24Application`을 생성하므로, 두 파일은 하나의 포팅 대상으로 취급합니다.

---

## 2. 포팅 우선순위

1. **JANGO 메뉴/런처** — 상대적으로 단순한 메인 메뉴 + 모달 다이얼로그 흐름. `JKApplication`, `JKDialog`, `JKFileDialog`, 메시지 박스를 검증하는 최적의 진입점.
2. **JANGO 2.4G 장비 관리** — 핵심 업무 CRUD. `JKListBox`, `JKEdit`, `JKComboBox`, 데이터 매니저를 검증.
3. **구 장비 관리 (`EquipWindow`)** — `EQUIP.CPP`의 독립 실행 구조를 SDL2 `main()` 형태로 변환.
4. **2CAOCC** — 메뉴바, 지도/좌표, 선택 다이얼로그가 많아 마지막에 집중.

---

## 3. JANGO 런처 UI/흐름

### 3.1 메인 메뉴 버튼

| 버튼 | ID | 동작 |
|---|---|---|
| 인    사 | `ID_BUTTON+1` | 비밀번호 다이얼로그 → `InsaWindow` |
| 2.4G 장비 | `ID_BUTTON+2` | `Equip24Window` |
| 장    비 | `ID_BUTTON+3` | `EquipWindow` |
| 파일 선택 | `ID_BUTTON+4` | 파일 선택 다이얼로그 |
| 부대 변경 | `ID_BUTTON+5` | 부대 선택 다이얼로그 + 확인 메시지 |
| 종    료 | `ID_BUTTON+6` | 앱 종료 |

### 3.2 필요한 SDL2 컴포넌트

- `JKApplication` 상속 + `OnInit()` 재정의
- `JKWindow` 메인 윈도우 (1024×768 논리 좌표)
- `JKButton` 6개
- `JKStatic` 텍스트/About 영역
- `JKDialog` 기반 `PasswordDialog`, `BudaeDialog`
- `JKFileDialog` (기존)
- `JKMessageBox` (기존)

### 3.3 하위 윈도우 스텁

`InsaWindow`, `Equip24Window`, `EquipWindow`는 초기 단계에서 `JKMessageBox` 스텁으로 대체합니다.  
이후 각 윈도우를 독립 Phase로 포팅합니다.

---

## 4. 공통 호환 레이어 추가

| 항목 | 원본 | SDL2 프로토타입 | 조치 |
|---|---|---|---|
| 색상 상수 | 팔레트 인덱스 (`LtBlue`, `Blue` 등) | RGB `SetBackColor(r,g,b)` | 포팅 시 상수를 RGB 매크로로 교체 |
| 좌표 체계 | `left/top/right/bottom` | `x/y/w/h` | `JKRect`에 `SetLeftTopBy`, `Expand`, `ExpandBy` 헬퍼 추가 |
| Rect 생성 | `JKRect(l,t,r,b)` | `JKRect{x,y,w,h}` | `MakeRect(l,t,r,b)` 헬퍼 사용 |
| 모달 루프 | `JKDialog::Run()` 반환값 | 비동기 `Show()` + 콜백 | `JKDialog` 베이스 클래스 추가, 필요 시 콜백 기반으로 재작성 |
| 컨트롤 ID | `ID_BUTTON`, `ID_STATIC`, `ID_OK`, `ID_CANCEL` | 미정의 | `legacy_compat.h` 또는 각 앱에 상수 정의 |

---

## 5. 파일 추가/변경 예정

```
prototype/sdl2_jkwindow/
├── include/
│   ├── JKTypes.h              # JKRect 헬퍼 확장
│   ├── JKButton.h             # SetDepth() 추가
│   └── JKDialog.h             # 새 모달 다이얼로그 베이스
│   └── apps/
│       └── JangoApp.h         # JANGO 런처 선언
├── src/
│   ├── JKDialog.cpp
│   ├── JKButton.cpp           # SetDepth()
│   └── apps/
│       └── JangoApp.cpp       # JANGO 런처 구현
├── src/main.cpp               # --app=jango 분기
└── CMakeLists.txt             # 신규 소스 등록
```

---

## 6. 검증 기준

- [ ] `build_sdl2_jkwindow.bat`로 빌드 성공
- [ ] `jkproto_sdl2_jkwindow.exe jango` 실행 시 JANGO 메인 메뉴 표시
- [ ] 6개 버튼 클릭 시 각각 비밀번호/스텁/파일선택/부대변경/종료 동작
- [ ] 모달 다이얼로그가 메인 윈도우 위에 표시되고, 닫기 전 메인 윈도우 입력 차단
- [ ] 종료 버튼 또는 창 닫기로 앱 종료

---

## 7. 다음 단계

1. 본 문서에 기술한 호환 레이어 및 JANGO 런처 구현.
2. `Equip24Window` 포팅 시작 (데이터 매니저, 리스트 박스, 입력 다이얼로그).
3. `EquipWindow`, `OCCApplication` 순으로 확장.
