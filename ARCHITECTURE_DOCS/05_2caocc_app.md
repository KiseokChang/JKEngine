# WINDBASE/2CAOCC: 2CA OCC 애플리케이션 아키텍처

`WINDBASE/2CAOCC`는 JKENGINE 내의 또 다른 업무 애플리케이션으로, **통신/포대/지점/좌표/계산** 등의 기능을 다루는 것으로 보입니다. JANGO와는 별개의 애플리케이션이며, 동일한 JKWINDOW 프레임워크 위에 구축되어 있습니다.

---

## 1. 2CAOCC 파일 구성

```
WINDBASE/2CAOCC/
├── OCCAPP.H/.CPP        # 최상위 OCCApplication
├── OCCWIN.H/.CPP         # 메인 윈도우
├── OCCMAIN.H/.CPP        # 진입점/전역
├── OCGLOBAL.H/.CPP       # 2CAOCC 전역 상태
├── MENUBAR.CPP           # 메뉴 바
├── METABANK.CPP          # 메타 데이터 뱅크
├── METAELEM.CPP          # 메타 요소
├── POSDTMAN.CPP          # 위치 데이터 관리자
├── POSREC.CPP            # 위치 레코드
├── DATAOS.CPP            # 데이터 OS/공유
├── DTMANSET.CPP          # 데이터 관리자 설정
├── COORDENT.CPP          # 좌표 입력
├── CORDVIEW.CPP          # 좌표 뷰
├── REALPOS.CPP           # 실제 위치
├── REAL2PIX.CPP          # 실제→픽셀 변환
├── CALCCORD.CPP          # 좌표 계산
├── GETWIDTH.CPP          # 폭 계산
├── SHOWCUR.CPP           # 현재 상태 표시
├── SHOWPRFX.CPP          # 접두어 표시
├── CMDMSGBX.CPP          # 명령 메시지 박스
├── LOGODLG.CPP           # 로고 다이얼로그
├── PASSDLG.CPP           # 비밀번호 다이얼로그
├── SELCOLOR.CPP          # 색상 선택
├── SELFUNC.CPP           # 기능 선택
├── SEL1OF2.CPP           # 2중 선택
├── SELATTR.CPP           # 속성 선택
├── SELBATT.CPP           # 포대 선택
├── SELDAND.CPP           # 단대 선택
├── SELTGDLG.CPP          # 타겟 선택 다이얼로그
├── AIRDLG.CPP            # 항공 다이얼로그
├── AIRMAN.CPP            # 항공 관리
├── AIRREC.CPP            # 항공 레코드
├── AIRMNDLG.CPP          # 항공 메인 다이얼로그
├── BATTDLG.CPP           # 포대 다이얼로그
├── BATTLST.CPP           # 포대 목록
├── BATTMAN.CPP           # 포대 관리
├── BATTREC.CPP           # 포대 레코드
├── BATTSTAT.CPP          # 포대 상태
├── FIREDLG.CPP           # 사격 다이얼로그
├── FIRELST.CPP           # 사격 목록
├── FIREMAN.CPP           # 사격 관리
├── FIREREC.CPP           # 사격 레코드
├── PLNTGDLG.CPP          # 진지 다이얼로그
├── PLNTGLST.CPP          # 진지 목록
├── PLNTGMAN.CPP          # 진지 관리
├── PLNTGREC.CPP          # 진지 레코드
├── HOPEDLG.CPP           # 희망 다이얼로그
├── HOWIZDLG.CPP          // 유도/마법사 다이얼로그
├── HOWIZMAN.CPP          // 유도 관리
├── HOWIZREC.CPP          // 유도 레코드
├── HPTDLG.CPP            # HPT 다이얼로그
├── HPTMAN.CPP            # HPT 관리
├── HPTMNDLG.CPP          # HPT 메인 다이얼로그
├── HPTREC.CPP            # HPT 레코드
├── DANDDLG.CPP           # 단대 다이얼로그
├── DANDENT.CPP           # 단대 입력
├── DANDCTRL.CPP          # 단대 컨트롤
├── EDANDDLG.CPP          # 단대 편집
├── ROADDLG.CPP           # 도로 다이얼로그
├── CIRCLDLG.CPP          # 원 다이얼로그
├── MOONCTRL.CPP          # 달/위성 컨트롤
├── INPBALSU.CPP          # 입력 보충
├── GMETTBL.CPP           # 기상 테이블
├── MT_CHAR.CPP           # 문자 메타
├── MT_PLLN.CPP           // 선 메타
├── HSHOWBLK.CPP          # 블록 표시
└── ... (기타)
```

---

## 2. OCCApplication / OCCWindow

### 2.1 OCCApplication

```cpp
class OCCApplication : public JKApplication {
    void SetupWindow();      // 메인 OCCWindow 추가
    void InitResourceMan();  // 2CAOCC 전용 이미지 리소스 등록
};
```

### 2.2 리소스 등록

```cpp
void OCCApplication::InitResourceMan() {
    JKApplication::InitResourceMan();
    if(ResMan) {
        for(uint16 i=0; i<3; i++) {
            ResMan->AddResource(new ResourceBase(0x400+i, RES_IMAGE,
                (BYTE*)"funcres.res", RT_FILE|RT_PACKFILE, i*2));
            ResMan->AddResource(new ResourceBase(0x1000+0x400+i, RES_IMAGE,
                (BYTE*)"funcres.res", RT_FILE|RT_PACKFILE, i*2+1));
        }
        for(uint16 i=0; i<50; i++)
            ResMan->AddResource(new ResourceBase(0x4000+i, RES_IMAGE, TRUE, "iconres.res", i));
        for(uint16 i=0; i<30; i++)
            ResMan->AddResource(new ResourceBase(0x4000+0x100+i, RES_IMAGE,
                (BYTE*)"moonres.res", RT_FILE|RT_PACKFILE, i));
        for(uint16 i=0; i<61; i++)
            ResMan->AddResource(new ResourceBase(0x4000+0x200+i, RES_IMAGE, TRUE, "dandaeho.res", i));
        for(uint16 i=0; i<80; i++)
            ResMan->AddResource(new ResourceBase(0x4000+0x300+i, RES_IMAGE, TRUE, "edandae.res", i));
    }
}
```

- **리소스 집중도가 높음**: 아이콘/기능/지도 기호 등 다량의 이미지 팩 사용.
- `funcres.res`, `iconres.res`, `moonres.res`, `dandaeho.res`, `edandae.res` 등이 핵심 리소스 파일.

---

## 3. 업무 도메인 추정

파일명과 클래스명을 분석하면 다음과 같은 업무 도메인이 추정됩니다.

| 도메인 | 파일 | 설명 |
|--------|------|------|
| **포대(Battery, BATT)** | `batt*.cpp` | 포대 관리, 상태, 목록 |

---

## 5. POSDTMAN: 위치 데이터 관리자

```cpp
// POSDTMAN.CPP / 관련 헤더 추정
class PosDataManager {
    // 위치(Position) 데이터의 CRUD, 검색, 정렬
};
```

- `POSREC.CPP`에서 정의된 위치 레코드를 관리하는 것으로 보입니다.
- JKDBASE나 DataFileManager 중 하나를 사용할 가능성이 높습니다.

---

## 6. METABANK / METAELEM

```cpp
// METABANK.CPP, METAELEM.CPP
class MetaBank;
class MetaElement;
```

- 메타데이터(예: 부호/기호/속성)를 중앙에서 관리하는 저장소로 추정됩니다.
- 2CAOCC의 다양한 기호(포대, 항공, 진지 등)의 메타 정보를 담고 있을 가능성이 있습니다.

---

## 7. 핵심 기술적 특징

1. **지도/좌표 기반 UI**: 실제 좌표를 화면에 매핑하고, 아이콘/도형으로 표시.
2. **다양한 선택 다이얼로그**: `SEL*` 파일들은 복잡한 업무 선택 UI를 담당.
3. **리소스 중심**: 다량의 `.res` 팩 파일을 통해 기호/아이콘을 관리.
4. **데이터-뷰 분리**: JANGO와 마찬가지로 JKWINDOW의 `JKControl`과 데이터 레이어가 분리되어 있음.
5. **복합 다이얼로그 구조**: `*DLG`, `*MNDLG`, `*LST` 등으로 다이얼로그 계층이 명확.

---

## 8. 웹 포팅 시 2CAOCC 대체 전략

| 원본 2CAOCC | 웹 대안 | 비고 |
|-------------|--------|------|
| OCCApplication | SPA 메인 App | 메뉴바 + 지도/대시보드 |
| OCCWindow | 메인 대시보드 컴포넌트 | 좌측 메뉴 + 중앙 캔버스 |
| 좌표 변환(`real2pix`) | 지도 라이브러리 + 투영 변환 | Leaflet/OpenLayers/Proj4js |
| 기호/아이콘 `.res` | SVG/PNG 아이콘 세트 | 메타 정보는 JSON으로 변환 |
| 포대/항공/사격/진지 데이터 | IndexedDB / REST API | 도메인별 스키마 설계 |
| 선택 다이얼로그 | React/Vue 모달 컴포넌트 | 동적 폼 생성 |
| 메뉴바 | 사이드바/탭 메뉴 | 라우터 연동 |
| 좌표 입력 | 폼 + 지도 클릭 입력 | 양방향 바인딩 |
| 프린트/보고서 | HTML 인쇄 / PDF 생성 | jsPDF, html2canvas |

---

## 9. JANGO와 2CAOCC의 공통점과 차이점

| 항목 | JANGO | 2CAOCC |
|------|-------|--------|
| 진입 클래스 | `JangoApplication` | `OCCApplication` |
| 주요 업무 | 인사, 2.4G 장비 관리 | 포대/항공/사격/진지/좌표 |
| 데이터 관리 | `DataFileManager` + 커스텀 클래스 | JKDBASE 또는 `DataFileManager` 사용 추정 |
| UI 스타일 | 버튼 메뉴 + 다이얼로그 | 메뉴바 + 지도/좌표 + 다이얼로그 |
| 리소스 | `insaimg.dat`, `jkimage.dat` 등 | `funcres.res`, `iconres.res`, `moonres.res`, `dandaeho.res`, `edandae.res` |
| 핵심 기술 | 데이터 바인딩, 마스터-디테일 | 좌표 변환, 지도 심볼, 메타데이터 |

---

## 10. 2CAOCC 분석 시 추가로 필요한 작업

현재 2CAOCC는 파일 목록과 `OCCApplication`/`OCCWIN`만 일부 분석되었습니다. 더 정확한 아키텍처를 위해서는 다음 파일들의 세부 분석이 필요합니다.

- `POSDTMAN.CPP` / `POSREC.CPP` / `DATAOS.CPP` / `DTMANSET.CPP`
- `METABANK.CPP` / `METAELEM.CPP` / `MT_CHAR.CPP` / `MT_PLLN.CPP`
- `OCCWIN.CPP` / `OCGLOBAL.CPP`
- `COORDENT.CPP` / `CORDVIEW.CPP` / `REALPOS.CPP` / `REAL2PIX.CPP` / `CALCCORD.CPP`
- `BATTMAN.CPP` / `AIRMAN.CPP` / `FIREMAN.CPP` / `PLNTGMAN.CPP`

| **항공(Air, AIR)** | `air*.cpp` | 항공기/목표 관련 |
| **사격(Fire, FIRE)** | `fire*.cpp` | 사격 데이터, 사격 명령 |
| **진지(Planting, PLNTG)** | `plntg*.cpp` | 진지/배치 정보 |
| **단대(Dandae, DAND)** | `dand*.cpp`, `edandae.*` | 단대(대대/중대 수준) 관리 |
| **좌표/위치(Coord, POS)** | `coordent.cpp`, `cordview.cpp`, `realpos.cpp`, `real2pix.cpp`, `posdtman.cpp`, `posrec.cpp` | 좌표 입력, 변환, 데이터 관리 |
| **기상(GMET)** | `gmettbl.cpp` | 기상 정보 테이블 |
| **HPT** | `hpt*.cpp` | HPT 관리 |
| **유도/마법사(Wizard)** | `howiz*.cpp` | 유도/마법사 |
| **색상/선택** | `selcolor.cpp`, `selfunc.cpp`, `sel1of2.cpp`, `selattr.cpp` | 다양한 선택 UI |
| **메뉴/메타** | `menubar.cpp`, `metabank.cpp`, `metaelem.cpp`, `mt_char.cpp`, `mt_pllN.cpp` | 메뉴 및 메타 데이터 |

---

## 4. 좌표 및 단위 변환

2CAOCC의 핵심 기술 중 하나는 **실제 좌표 ↔ 화면 픽셀 좌표 변환**입니다.

| 파일 | 기능 |
|------|------|
| `realpos.cpp` | 실제 좌표(예: 위도/경도 또는 군용 좌표) 정의 |
| `real2pix.cpp` | 실제 좌표를 화면 픽셀 좌표로 변환 |
| `calccord.cpp` | 좌표 계산(거리, 방위각 등) |
| `getwidth.cpp` | 폭/스케일 계산 |
| `coordent.cpp` | 좌표 입력 컨트롤 |
| `cordview.cpp` | 좌표 표시 뷰 |

- 웹 포팅 시에는 **Canvas 좌표 변환** 또는 **지도 라이브러리(Leaflet, OpenLayers, Google Maps)** 활용을 검토해야 합니다.
