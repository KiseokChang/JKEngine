# JKDBASE: 데이터 관리 레이어 아키텍처

JKDBASE는 JKENGINE의 **파일 기반 데이터 관리 라이브러리**입니다. DOS 환경에서 DBMS 없이 독립적으로 동작하며, 고정 길이 레코드(fixed-length record)와 연결 리스트 기반의 삭제/삽입 관리를 핵심으로 합니다.

---

## 1. JKDBASE 파일 구성

```
JKDBASE/
├── FHEADER.H        # 파일 헤더 구조체
├── RECMAN.H/.CPP     # 물리적 레코드 I/O
├── TABLEMAN.H/.CPP   # 논리적 테이블/링크 관리
├── DATAMAN.H/.CPP    # 상위 CRUD/검색/정렬 API
├── ENTBASE.H/.CPP    # 필드(Entry) 기반 클래스
├── RECBASE.H/.CPP     # 레코드(Entry 집합) 클래스
├── NUMENTRY.H/.CPP   # 숫자형 Entry
├── REALENT.H/.CPP    # 실수형 Entry
├── DATEENT.H/.CPP    # 날짜형 Entry
├── TIMEENT.H/.CPP    # 시간형 Entry
├── BOOLENT.H/.CPP    # 논리형 Entry
└── DBTEST.CPP        # 테스트 프로그램(추정)
```

---

## 2. 데이터 모델 개념

```
DataManager
    ├── RecordManager  →  .dat 파일 (실제 데이터 블록)
    └── TableManager   →  .tbl 파일 (사용/링크 메타데이터)

RecordBase (레코드 정의)
    ├── EntryBase A (필드 A)
    ├── EntryBase B (필드 B)
    └── ...
```

| 개념 | C++ 클래스/구조체 | 역할 |
|------|-----------------|------|
| **파일 헤더** | `FILEHEADER` | `RecordID`, `AllocCount`, `RealCount` 저장 |
| **데이터 파일** | `.dat` | `RecordManager`가 관리. 고정 길이 바이너리 레코드 배열 |
| **테이블 파일** | `.tbl` | `TableManager`가 관리. `REFENTRY` 배열로 사용 여부/이전/다음 인덱스 |
| **Entry(필드)** | `EntryBase` 파생 클래스 | 하나의 필드 정의 + 값 + 비교/직렬화 |
| **Record(행)** | `RecordBase` 파생 클래스 | 여러 Entry의 집합, 파일 저장 단위 |
| **DataManager** | `DataManager` | CRUD, 검색, 정렬의 상위 API |

---

## 3. 파일 헤더: FILEHEADER

```cpp
class FILEHEADER {
    WORD   RecordID;    // 레코드 타입 식별자
    uint16 AllocCount;  // 할당된 전체 슬롯 수
    uint16 RealCount;   // 실제 사용 중인 슬롯 수
};
```

- `.dat`와 `.tbl` 모두 동일한 헤더 구조로 시작합니다.
- 파일 열기 시 `RecordID`, `AllocCount`, `RealCount`가 일치해야 정상 연결됩니다.

---

## 4. RecordManager: 물리적 레코드 I/O

`RecordManager`는 `.dat` 파일을 다룹니다.

| 메서드 | 설명 |
|--------|------|
| `ChangeFileName(fname)` | 기존 파일 열기, 헤더 검증 |
| `MakeEmptyFile()` | 빈 `.dat` 파일 생성 |
| `ExpandSpace(count)` | 파일 끝에 `count`개의 빈 레코드 추가 |
| `GetRecord(index, rec)` | `index` 위치의 바이너리를 `RecordBase`로 역직렬화 |
| `PutRecord(index, rec)` | `RecordBase`를 `index` 위치에 직렬화 |

### 4.1 .dat 파일 레이아웃

```
[FILEHEADER: 6바이트]
[Record 0: RecordSize 바이트]
[Record 1: RecordSize 바이트]
...
[Record N: RecordSize 바이트]
```

- `RecordSize = WorkRecord->GetFileSize()` (모든 Entry의 `FileSize` 합계)
- 빈 레코드는 `' '` (공백)으로 채워집니다.

---

## 5. TableManager: 논리적 테이블/링크 관리

`TableManager`는 `.tbl` 파일을 다루며, **삭제 후 재사용**과 **레코드 간 순서(연결 리스트)**를 관리합니다.

```cpp
typedef struct {
    BYTE  HasData;   // 0=빈 슬롯, 1=사용 중
    int16 prev;      // 이전 사용 슬롯 인덱스
    int16 next;      // 다음 사용 슬롯 인덱스
} REFENTRY;
```

| 메서드 | 설명 |
|--------|------|
| `FindEmptyIndex()` | 첫 번째 빈 슬롯 검색 |
| `AddRefEntry(allocindex)` | 맨 뒤(tail)에 추가 |
| `InsertRefEntry(index, allocindex)` | 특정 레코드 앞에 삽입 |
| `FreeIndex(index)` | 연결 리스트에서 제거, 빈 슬롯으로 표시 |
| `ReArrangeTable(recarray)` | `recarray` 순서로 연결 리스트 재구성(정렬 후 사용) |

### 5.1 .tbl 파일 레이아웃

```
[FILEHEADER: 6바이트]
[TableHead: int16]
[TableTail: int16]
[REFENTRY 0]
[REFENTRY 1]
...
[REFENTRY N]
```


### 5.2 연결 리스트 상태 예시

```
사용 중인 인덱스: 0 → 2 → 5 → 3
REFENTRY[0]: HasData=1, prev=-1, next=2   (head)
REFENTRY[2]: HasData=1, prev=0,  next=5
REFENTRY[5]: HasData=1, prev=2,  next=3
REFENTRY[3]: HasData=1, prev=5,  next=-1  (tail)
REFENTRY[1]: HasData=0, prev=-1, next=-1  (빈 슬롯)
```

---

## 6. DataManager: 상위 CRUD/검색/정렬

`DataManager`는 `RecordManager`와 `TableManager`를 조합해 **객체 지향적인 데이터 API**를 제공합니다.

### 6.1 핵심 멤버

```cpp
class DataManager {
    uint16       AllocCount;
    uint16       RealCount;
    uint16       RecordSize;
    RecordManager RecordMan;
    TableManager  TableMan;
    RecordBase*   WorkRecord;  // 스키마/작업용 템플릿 레코드
    char          RecFileName[MAXPATH];
};
```

### 6.2 CRUD 흐름

| 동작 | 흐름 |
|------|------|
| **연결 파일** | `ChangeFileName(fname, rec)` → `fname` 8자 기준으로 `.dat`/`.tbl` 생성/오픈 |
| **빈 파일 만들기** | `MakeEmptyFile(alloccount)` → 헤더 초기화 + `alloccount`개 빈 슬롯 생성 |
| **추가** | `AddRecord(rec)` → 빈 인덱스 확보 → `.dat` 쓰기 → `.tbl` 연결 리스트 추가 → 카운트 갱신 |
| **삽입** | `InsertRecord(index, rec)` → 지정 인덱스 앞에 삽입 |
| **수정** | `SetRecord(index, rec)` → 기존 사용 슬롯에 덮어쓰기 |
| **삭제** | `DeleteRecord(index)` → `.tbl` 연결 리스트에서 제거 → `RealCount--` |
| **전체 삭제** | `DeleteAll()` → head부터 순회하며 모두 삭제 |

### 6.3 검색/정렬

```cpp
int16 Search(EntryBase* ent, int16 findstart=-1);
int16 SearchScope(EntryBase* min, EntryBase* max, int16 findstart=-1);
int16 Search(RecordBase* rec, uint16 index, int16 findstart=-1);
int16 Search(RecordBase* rec, WORD* comporder, uint16 ordercount, int16 findstart=-1);

BOOL Sort(uint16* recarray, uint16 reccount, uint16 entid);
BOOL Sort(uint16* recarray, uint16 reccount, WORD* comporder, uint16 ordercount);
BOOL FileSort(uint16 entid);
BOOL FileSort(WORD* comporder, uint16 ordercount);
```

- **검색**: 연결 리스트를 따라가며 `RecordBase::Compare`를 이용한 선형 검색
- **정렬**: 버블/선택 정렬 방식의 단순 정렬 후 `ReArrangeTable`로 `.tbl` 재배열
- **비교 종류(compkind)**: 비트마스크로 필드별 비교 조합 지정

---

## 7. EntryBase / RecordBase

### 7.1 EntryBase: 필드 단위

```cpp
class EntryBase {
    WORD    EntryID;
    BYTE*   DataBuffer;   // 현재 값
    BYTE*   SpaceBuffer;  // 초기값/공백 패턴
    char    EntryName[MAXNAME+1];
    uint32  BufferSize;   // 메모리상 버퍼 크기
    uint32  FileSize;     // 파일에 저장되는 크기
    HuboManager* HuboMan; // 코드-텍스트 변환 매니저(선택)
};
```

| 가상 함수 | 역할 |
|-----------|------|
| `Compare(ent, compkind)` | 대소/일치 비교(-1, 0, 1, 2) |
| `SetData(data)` | 값 설정 |
| `GetData(data)` | 값 읽기 |
| `Get/Put(FILE*)` | 파일 직렬화/역직렬화 |
| `IsEmpty()` | 초기값과 동일한지 검사 |
| `Assign(ent)` | 다른 Entry 값 복사 |

### 7.2 Entry 비교 우선순위

```cpp
int16 EntryBase::Compare(EntryBase* ent, uint16 compkind) {
    // 1. 빈 값 처리
    // 2. HuboManager 인덱스 비교(코드값 비교)
    // 3. DataBuffer 문자열 비교(strcmp)
}
```

`HuboManager`가 설정되면 한글 조합형/완성형 코드 변환이 우선 적용됩니다.

### 7.3 RecordBase: 레코드 단위

```cpp
class RecordBase : public DLLFrame<EntryBase> {
    WORD RecordID;
    char RecordName[MAXNAME+1];
    // EntryBase의 이중 연결 리스트 상속
};
```

| 기능 | 설명 |
|------|------|
| `AddEntry(ent)` | Entry 추가 및 초기화 |
| `GetEntry(entid)` | ID로 Entry 검색 |
| `SetData/GetData(entid, data)` | 특정 Entry 값 조작 |
| `GetFileSize()` | 모든 Entry `FileSize` 합계 |
| `IsEmpty()` | 모든 Entry가 초기값인지 |
| `IsFullfilled()` | 모든 Entry가 비어있지 않은지 |
| `Compare(...)` | 단일/다중 필드 비교 |
| `CompareScope(...)` | 범위(min~max) 비교 |

### 7.4 제공 Entry 타입

| 클래스 | 저장 형태 | 특징 |
|--------|----------|------|
| `NumberEntry` | `int32` | 진수(Radix), 상한/하한 검증 |
| `RealEntry` | `double` | 실수 상한/하한 검증 |
| `DateEntry` | `struct date` | `dos.h`의 date 구조체 기반, 문자열 변환 |
| `TimeEntry` | `struct time` | `dos.h`의 time 구조체 기반 |
| `BooleanEntry` | `BOOL` | 0/1 |

---

## 8. 파일 연결 규칙

```cpp
BOOL DataManager::ChangeFileName(char* fname, RecordBase& rec) {
    // 1. fname에서 8자 추출
    // 2. <fname8>.dat, <fname8>.tbl 결정
    // 3. .dat 헤더가 존재하면 오픈 및 검증
    // 4. 없으면 MakeEmptyFile(0)으로 빈 파일 생성
}
```

- **주의**: DOS 8.3 파일명 규칙을 따르므로, 웹 포팅 시에는 긴 파일명으로 마이그레이션해야 합니다.

---

## 9. 핵심 기술적 특징

1. **고정 길이 레코드**: 가변 길이 문자열이 아닌 고정 길이 바이너리 저장.
2. **소프트 삭제**: 실제 데이터는 `.dat`에 남아 있고, `.tbl`의 `HasData`만 0으로 표시.
3. **연결 리스트 순서 관리**: 물리적 인덱스와 논리적 순서를 분리.
4. **헤더 동기화**: 모든 쓰기 후 `PutHeader()`로 `.dat`와 `.tbl` 헤더를 갱신.
5. **HuboManager 연동**: 한글 코드표 변환을 Entry 수준에서 처리.

---

## 10. 웹 포팅 시 JKDBASE 대체 전략

| 원본 JKDBASE | 웹 대안 | 비고 |
|--------------|--------|------|
| `.dat` + `.tbl` 바이너리 | IndexedDB / SQLite(WASM) / 서버 DB | 스키마 정보는 `RecordBase` 파생 클래스에서 추출 |
| `RecordBase` 파생 클래스 | TypeScript/JavaScript Class / JSON Schema | 필드 정의, 기본값, 검증 규칙 마이그레이션 |
| `EntryBase::Compare` | DB 쿼리 또는 JS 비교 함수 | `compkind` 비트마스크를 query operator로 변환 |
| `DataManager::FileSort` | DB ORDER BY 또는 JS sort | 클라이언트 정렬 필요 시 메모리 내 정렬 |
| `HuboManager` | 별도 codebook 테이블 | 한글/코드 매핑 데이터 마이그레이션 |
| 8.3 파일명 | 명확한 테이블명/파일명 | 예: `eq24name.dat` → `equipment_names.json` |

---

## 11. JKDBASE 외 추가 파일 관리자: DataFileManager

`WINDBASE/JANGO`에서는 JKDBASE와 별개로 `DataFileManager`(`dfilemn.h/cpp`)를 사용합니다.

- 파일 헤더: `Delete_Record`, `Record_Count`, `Record_Length`
- 삭제된 레코드는 `Delete_Record` 체인으로 재사용
- `MAXFILS=11`개 파일 동시 오픈
- `FLOCATE` 매크로로 레코드 위치 계산

> `DataFileManager`는 JKDBASE보다 단순한 "편평한 파일 관리자"이며, JANGO의 `Equip24Name`/`Equip24Kind` 등에서 사용됩니다. 자세한 내용은 `04_windbase_jango.md` 참고.
