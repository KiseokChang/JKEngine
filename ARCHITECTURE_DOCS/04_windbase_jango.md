# WINDBASE / JANGO: 애플리케이션 프레임워크 레이어

WINDBASE는 JKDBASE와 JKWINDOW 위에 구축된 **업무 애플리케이션 레이어**입니다. `JANGO`는 그 위의 **GWES(Graphic Windowing Environment System)** 스타일 애플리케이션 프레임워크이며, 주로 인사/장비 관리 업무를 다룹니다.

---

## 1. WINDBASE 디렉터리 구조

```
WINDBASE/
├── RECVIEW.H/.CPP       # RecordViewBase: 레코드-뷰 데이터 바인딩
├── ENTCTRL.H/.CPP       # EntryControl: 필드 편집 컨트롤 기반
├── SELCTRL.H/.CPP        # SelectEntryControl: 드롭다운/선택 필드
├── TYPECTRL.H/.CPP       # TypeEntryControl: 직접 입력 필드
├── CHKCTRL.H/.CPP        # CheckEntryControl: 체크박스 필드
└── JANGO/                # JANGO GWES 애플리케이션
    ├── JANGOAPP.H/.CPP   # 메인 애플리케이션
    ├── EQP24WIN.H/.CPP   # 2.4GHz 장비 관리 윈도우
    ├── EQ24DMAN.H/.CPP   # 2.4GHz 데이터 관리자
    ├── EQ24NAME.H/.CPP   # 장비명(마스터) 데이터 관리
    ├── EQ24KIND.H/.CPP   # 장비종류(상세) 데이터 관리
    ├── DFILEMN.H/.CPP    # DataFileManager (단순 파일 매니저)
    ├── EQP24DEF.H        # EQP24 데이터 구조체
    ├── EQP24GLB.H/.CPP   # 전역 상수/설정
    └── ... (다양한 다이얼로그/차트/인사 모듈)
```

---

## 2. 데이터 바인딩: RecordViewBase

```cpp
class RecordViewBase {
    RecordBase& RefRecord;
    JKControl&  RefControl;
    BOOL         IsChange;

    void EvCharDown(uint16 key, WORD);
    void RV_RespondMessage(JKMSG& msg);

    virtual void EvEntryCtrlQuery(WORD ctrlid);
    virtual void EvEntryCtrlEndModify(WORD ctrlid);
    virtual void EvEntryCtrlCancelModify(WORD ctrlid);
    virtual void EvEntryCtrlInvalidModify(WORD ctrlid);

    BOOL ModifyEntry(WORD ctrlid);
    void ReLoad();
};
```

- `RecordViewBase`는 **JKDBASE의 `RecordBase`와 JKWINDOW의 `JKControl`을 연결**하는 브리지 클래스입니다.
- 컨트롤에서 발생하는 `ENTRYCTRL_*` 메시지를 받아 `RefRecord`의 해당 Entry를 갱신하거나 되돌립니다.
- `IsChange` 플래그로 수정 여부를 추적합니다.

---

## 3. EntryControl 계열

```cpp
class EntryControl : public JKControl {
    EntryBase& RefEntry;

    virtual void ModifyEntry() = 0;  // 사용자 입력을 RefEntry에 반영
    virtual void ReLoad() = 0;       // RefEntry 값을 UI에 다시 표시
};
```

| 파생 클래스 | 파일 | 역할 |
|-------------|------|------|
| `TypeEntryControl` | `typectrl.cpp` | 키보드로 직접 값 입력 (Edit 기반) |
| `SelectEntryControl` | `selctrl.cpp` | 드롭다운/리스트에서 선택 |
| `CheckEntryControl` | `chkctrl.cpp` | 체크박스 형태 |

### 3.1 메시지 정의

```cpp
#define ENTRYCONTROLMESSAGE      (USERMESSAGE)
#define ENTRYCTRL_QUERY          (ENTRYCONTROLMESSAGE+1)
#define ENTRYCTRL_RELOAD         (ENTRYCONTROLMESSAGE+2)
#define ENTRYCTRL_BEGINMODIFY    (ENTRYCONTROLMESSAGE+3)
#define ENTRYCTRL_ENDMODIFY      (ENTRYCONTROLMESSAGE+4)
#define ENTRYCTRL_CANCELMODIFY   (ENTRYCONTROLMESSAGE+5)
#define ENTRYCTRL_INVALIDMODIFY  (ENTRYCONTROLMESSAGE+6)
```

---

## 4. JANGO 애플리케이션

### 4.1 JangoApplication

```cpp
class JangoApplication : public JKApplication {
    CtrlResManager CtrlResMan;

    void InitResourceMan();   // JK 리소스 + JANGO 전용 리소스 등록
    void SetupWindow();       // 메인 버튼 6개 + About 영역 배치
    BOOL CanClose();
    void EvButtonClick(WORD ctrlid);
    void EvCommand(WORD cmdid, WORD data, DWORD detail, WORD option);
};
```

### 4.2 메인 메뉴 버튼

```
[인    사]      → START_INSA      → PasswordDialog → InsaWindow
[2.4G 장비]     → START_EQUIP24   → Equip24Window
[장    비]      → START_EQUIP     → EquipWindow
[부대 변경]      → 파일 선택 다이얼로그
[종    료]      → CloseWindow
```

- 각 버튼 클릭 시 `SendCommand(START_*)`로 처리.
- `START_INSA`는 비밀번호 다이얼로그를 거칩니다.

---

## 5. DataFileManager

`JANGO`의 데이터 저장은 JKDBASE 대신 **더 단순한 `DataFileManager`**를 사용합니다.

```cpp
#define RPTR    int32
#define MAXFILS 11

typedef struct fhdr {
    RPTR   Delete_Record;   // 삭제 체인 헤드
    RPTR   Record_Count;    // 다음 사용 가능한 레코드 번호
    int16  Record_Length;   // 고정 레코드 길이
} FHEADER;

class DataFileManager {
    int16  Handle[MAXFILS];
    FHEADER FileHeader[MAXFILS];

    void  FileCreate(char* name, int16 len);
    int16 FileOpen(char* name);
    void  FileClose(int16 fp);
    RPTR  NewRecord(int16 fp, char* bf);
    int16 GetRecord(int16 fp, RPTR rcdno, char* bf, int16 length);
    int16 PutRecord(int16 fp, RPTR rcdno, char* bf, int16 length);
    int16 DeleteRecord(int16 fp, RPTR rcdno);
    BOOL  IsUseFul(int16 fp, RPTR rcdno);
};
```

### 5.1 파일 레이아웃

```
[FHEADER: sizeof(FHEADER)]
[UsefulFlag 1: 1바이트][Record 1: Record_Length 바이트]
[UsefulFlag 2: 1바이트][Record 2: Record_Length 바이트]
...
```

- **삭제 체인**: `Delete_Record`가 삭제된 레코드 번호를 가리키고, 삭제된 레코드의 `FHEADER.Record_Count` 필드에 다음 삭제된 레코드 번호가 저장되어 링크드 리스트 형태로 재사용됩니다.
- `FLOCATE(r,l) = sizeof(FHEADER) + (r-1)*(l+1) + r` 형태로 위치를 계산합니다.


---

## 6. Equip24 데이터 모델 예시

### 6.1 구조체

```cpp
// EQP24DEF.H

typedef struct eqp24name {
    char Division[5];     // 부문
    char Attached[7];     // 소속
    char Name[9];         // 장비명
    char Number[12];      // 번호
    RPTR FirstIndex;      // 첫 상세 인덱스
    RPTR LastIndex;       // 마지막 상세 인덱스
} NAME24;

typedef struct epq24kind {
    WORD  Order;          // 순서
    char  Name[15];       // 명칭
    char  Number[20];     // 번호
    WORD  Inga;           // 인가(?) 값
    WORD  A, B, C;        // 수량/분류값
    char  Date[11];       // 날짜
    RPTR  NameIndex;      // 소속 NAME24 인덱스
    RPTR  NextIndex;      // 다음 KIND24 인덱스
    RPTR  PrvIndex;       // 이전 KIND24 인덱스
} KIND24;

typedef struct epq24 {
    NAME24 Name24;
    KIND24 Kind24;
} EQP24;
```

### 6.2 Equip24Name / Equip24Kind

```cpp
class Equip24Name {
    NAME24 NameRecord;   // 현재 로드된 레코드
    NAME24 WorkRecord;   // 검색용 임시 레코드
    DataFileManager DFileMan;
    int16 FilePoint;

    RPTR Search(char* buf, RPTR index=0, WORD compkind=0x0002);
    RPTR PutRecord(NAME24& buf);
    RPTR DeleteRecord(RPTR index);
};

class Equip24Kind {
    KIND24 KindRecord;
    KIND24 WorkRecord;
    DataFileManager DFileMan;
    int16 FilePoint;

    RPTR Search(...);
    RPTR PutRecord(KIND24& buf);
    BOOL DeleteRecord(RPTR index);
};
```

- `Equip24Name`은 마스터(헤더) 테이블, `Equip24Kind`는 상세(라인) 테이블 역할.
- `NAME24.FirstIndex` / `LastIndex`로 `KIND24`의 연결 리스트를 관리.

### 6.3 Equip24DataManager

```cpp
class Equip24DataManager {
    Equip24Kind  Eqp24Kind;
    Equip24Name  Eqp24Name;

    RPTR GetFirstIndex(char* s, WORD compkind);
    RPTR GetFirstIndex(NAME24& name, WORD compkind);
    RPTR GetNameIndex(KIND24& name, WORD compkind);

    BOOL PutRecord(EQP24& eqp24, WORD compname=0x000F, WORD compkind=0x0003);
    BOOL AppendKind(KIND24& kind, RPTR index);
    BOOL DeleteRecord(RPTR index, int16 kind=2);
    BOOL DeleteName(RPTR index);
    BOOL DeleteKind(RPTR index);
};
```

#### PutRecord 흐름

1. `NAME24` 기준으로 동일한 이름 검색 (`GetFirstIndex`)
2. 없으면: 새 `NAME24` 추가 → 새 `KIND24` 추가 → 서로 인덱스 연결
3. 있으면: 같은 `NAME24`에 속한 `KIND24` 리스트를 순회
   - 동일 종류(`compkind` 비교)가 있으면 수량(`A`, `B`, `C`) 누적
   - 없으면 링크드 리스트 끝에 추가

#### DeleteKind 흐름

- `KIND24`의 `PrvIndex`/`NextIndex`를 재연결
- 만약 마지막 `KIND24`였으면 `NAME24`도 함께 삭제

---

## 7. JANGO의 다양한 서브 모듈

| 모듈 | 파일 예시 | 역할 추정 |
|------|----------|----------|
| 인사 관리 | `insa*.h/cpp`, `inwon*.h/cpp`, `persn*.h/cpp` | 인사/인원/출생/전역 관리 |
| 장비 관리 | `equip*.h/cpp`, `eqp24*.h/cpp`, `eq24*.h/cpp` | 장비, 2.4G 장비, 종류/명칭 관리 |
| 통계/차트 | `bargraph.cpp`, `piechart.cpp`, `bltchart.cpp` | 막대/파이/차트 출력 |
| 다이얼로그 | `passdlg.cpp`, `budaedlg.cpp`, `eqseldlg.cpp` | 비밀번호, 부대변경, 선택 다이얼로그 |
| 유틸리티 | `joappabt.cpp`, `sagoman.cpp`, `obsman.cpp` | About, 사고/장애, 관측 등 |

---

## 8. 핵심 기술적 특징

1. **수동 데이터 바인딩**: `RecordViewBase` + `EntryControl`로 MVC의 View-Controller를 직접 구현.
2. **양 파일 형태 저장**: `Equip24Name`/`Equip24Kind`처럼 마스터-디테일 관계를 파일 인덱스로 연결.
3. **링크드 리스트 기반 1:N**: JKDBASE의 `.tbl` 연결 리스트와 유사하게 `KIND24`가 `NextIndex`/`PrvIndex`로 연결됨.
4. **비밀번호/부대 개념**: `JANGO`는 `BudaeName`, `PasswordTable` 등 전역 상태를 가짐.
5. **리소스 의존성**: `insaimg.dat` 등 팩 파일과 `CtrlResMan`을 통해 UI 상태 연동.

---

## 9. 웹 포팅 시 WINDBASE/JANGO 대체 전략

| 원본 구성 | 웹 대안 | 비고 |
|-----------|--------|------|
| `RecordViewBase` | React/Vue 컴포넌트 + 상태 바인딩 | `v-model` / `useState` |
| `EntryControl` | 커스텀 Form 컴포넌트 | HTML 기본 입력 또는 커스텀 UI |
| `DataFileManager` | IndexedDB / SQLite WASM / REST API | 파일 핸들 인덱스를 DB 식별자로 변환 |
| `NAME24`/`KIND24` 구조체 | TypeScript Interface / Class | 마스터-디테일 관계는 외래키 또는 중첩 배열로 표현 |
| `Equip24DataManager` | Service/Store 레이어 | `PutRecord`, `DeleteKind` 등을 API 메서드로 구현 |
| `JangoApplication` | SPA 라우터 + 메인 메뉴 페이지 | 각 메뉴를 라우트로 분리 |
| `PasswordDialog` | 로그인/인증 페이지 | 서버 세션 또는 JWT |
| `BudaeName` 전역 | 전역 상태 / LocalStorage | 사용자 설정/세션 정보 |
| 차트 모듈 | Chart.js / D3.js / ECharts | 웹 차트 라이브러리 활용 |
| 프린트 출력 | 브라우저 인쇄 / PDF 라이브러리 | `window.print()` 또는 `jsPDF` |
