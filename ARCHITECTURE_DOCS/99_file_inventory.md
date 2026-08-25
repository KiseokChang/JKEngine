# JKENGINE 주요 파일 인벤토리

> 본 문서는 JKENGINE의 주요 소스/헤더/리소스 파일을 모듈별로 정리합니다. 전체 파일 목록은 `tree /f` 명령으로 확인할 수 있습니다.

---

## 1. 루트 프로젝트 파일

| 파일 | 설명 |
|------|------|
| `JKENGIN.IDE` | Borland C++ 4.5 IDE 프로젝트 파일 (바이너리) |
| `JKENGIN.DSW` | Borland Workshop / IDE 워크스페이스 파일 (바이너리) |
| `TechDoc_extracted.txt` | 기존 문서 추출본 (깨진 텍스트, 재활용 불가) |

---

## 2. JKDBASE (데이터 관리 라이브러리)

| 파일 | 설명 |
|------|------|
| `FHEADER.H` | 파일 헤더 구조체 (`RecordID`, `AllocCount`, `RealCount`) |
| `RECMAN.H/.CPP` | 물리적 레코드 I/O (`.dat` 파일) |
| `TABLEMAN.H/.CPP` | 논리적 테이블/링크 관리 (`.tbl` 파일, `REFENTRY`) |
| `DATAMAN.H/.CPP` | 상위 CRUD, 검색, 정렬 |
| `ENTBASE.H/.CPP` | `EntryBase` 필드 기반 클래스 |
| `RECBASE.H/.CPP` | `RecordBase` 레코드 클래스 |
| `NUMENTRY.H/.CPP` | 정수형 Entry |
| `REALENT.H/.CPP` | 실수형 Entry |
| `DATEENT.H/.CPP` | 날짜형 Entry |
| `TIMEENT.H/.CPP` | 시간형 Entry |
| `BOOLENT.H/.CPP` | 논리형 Entry |
| `DBTEST.CPP` | JKDBASE 테스트 프로그램 |

---

## 3. JKWINDOW (GUI/그래픽 프레임워크)

### 3.1 애플리케이션/메시지/이벤트

| 파일 | 설명 |
|------|------|
| `JKMAIN.H/.CPP` | `main()` 진입점 |
| `JKINIT.H/.CPP` | 전역 초기화/종료 (`InitJKProgram`, `CloseJKProgram`) |
| `JKMODULE.H` | `MainApplication` 전역 |
| `JKAPP.H/.CPP` | `JKApplication` 최상위 앱 |
| `EVNTHAND.H/.CPP` | `EventHandler` 메시지 루프 |
| `MSGQUE.H/.CPP` | 메시지 큐 (`JKMSG`, `MessageQue`) |
| `MSGFILT.H/.CPP` | 메시지 필터/정규화 |
| `MSGCREAT.H/.CPP` | 메시지 생성자 기반 클래스 |
| `CONTROL.H/.CPP` | `JKControl` 모든 UI 기반 |
| `JKWINDEF.H` | 메시지/색상/ID 상수 |

### 3.2 윈도우/다이얼로그

| 파일 | 설명 |
|------|------|
| `JKWINDOW.H/.CPP` | `JKWindow` |
| `JKDIALOG.H/.CPP` | `JKDialog` |
| `OKDIALOG.H/.CPP` | OK 대화상자 |
| `YESNODLG.H/.CPP` | Yes/No 대화상자 |
| `FILEDLG.H/.CPP` | 파일 선택 대화상자 |
| `HANJADLG.H/.CPP` | 한자 대화상자 |

### 3.3 컨트롤

| 파일 | 설명 |
|------|------|
| `JKBUTTON.H/.CPP` | 버튼 |
| `JKSTATIC.H/.CPP` | 정적 텍스트 |
| `JKEDIT.H/.CPP` | 에디트/텍스트 입력 |
| `LISTBOX.H/.CPP` | 리스트박스 |
| `COMBOBOX.H/.CPP` | 콤보 박스 |
| `CHECKBOX.H/.CPP` | 체크 박스 |
| `SCROLBAR.H/.CPP` | 스크롤 바 |
| `SPIN.H/.CPP` | 스핀 컨트롤 |
| `FLTBTTN.H/.CPP` | 플랫 버튼 |
| `RESBTTN.H/.CPP` | 리소스 버튼 |
| `RESCHKBX.H/.CPP` | 리소스 체크박스 |
| `STRCHKBX.H/.CPP` | 문자열 체크박스 |
| `POPUPSTC.H/.CPP` | 팝업 정적 텍스트 |
| `POPLIST.H/.CPP` | 팝업 리스트 |
| `EDITCTRL.H/.CPP` | 에디트 컨트롤 세부 |
| `CTRLRES.H/.CPP` | 컨트롤 리소스 |

### 3.4 그래픽/DC

| 파일 | 설명 |
|------|------|
| `JKDC.H/.CPP` | `JKDC` Device Context 기반 |
| `JKSCRNDC.H/.CPP` | 화면 DC |
| `MEMDC.H/.CPP` | 메모리 DC |
| `METADC.H/.CPP` | 메타파일 DC |
| `JKPRNDC.H/.CPP` | 프린터 DC |
| `KGRAPHIC.H/.CPP` | 그래픽 함수 래퍼 |
| `GRBASE.H/.CPP` | 그래픽 기반 |
| `GRMAIN.H/.CPP` | 그래픽 메인 |
| `GRAPH_PM.H/.CPP` | 그래픽 포트/매니저 |
| `VESA256.H/.CPP` | VESA 256색 모드 제어 |
| `SINTBL.CPP` | 사인표(그래픽 계산용) |

### 3.5 입력/타이머/리소스/출력

| 파일 | 설명 |
|------|------|
| `MOUSEMAN.H/.CPP` | 마우스 이벤트 생성/커서 |
| `KEYBDMAN.H/.CPP` | 키보드 이벤트 생성 |
| `TIMERMAN.H/.CPP` | 타이머 관리 |
| `RESMAN.H/.CPP` | 리소스 관리자 |
| `PRINTMAN.H/.CPP` | 프린터 관리 |
| `HANMAN.H/.CPP` | 한글 입출력 관리자 |
| `VFONTMAN.H/.CPP` | 벡터 폰트 관리자 |
| `JOCLOCK.H/.CPP` | 시계 |

### 3.6 테스트/유틸리티

| 파일 | 설명 |
|------|------|
| `TESTAPP.H/.CPP` | 테스트 앱 |
| `TESTWIN.H/.CPP` | 테스트 윈도우 |
| `TESTMAIN.CPP` | 테스트 메인 |
| `TESTELPS.CPP` | 타원 테스트 |
| `TESTVESA.CPP` | VESA 테스트 |
| `LOWMEM.H/.CPP` | 저메모리 관리 |
| `MAKEPAL.CPP` | 팔레트 생성 |
| `WANCODE.CPP` | 완성형 코드 처리 |
| `ZEROSEL.CPP` | 영 선택 |
| `AUTOMATA.CPP` | 상태 머신 |

### 3.7 서브 폴더

| 폴더 | 설명 |
|------|------|
| `256PCX/` | 256색 PCX 이미지 관련 |
| `ICONEDIT/` | 아이콘 에디터 |
| `NEWTECH/` | 신기술 관련 |
| `VECTFONT/` | 벡터 폰트 데이터 |
| `VECTOR/` | 벡터 그래픽 관련 |

---

---

## 5. WINDBASE (애플리케이션 레이어)

### 5.1 공용 데이터 바인딩 컨트롤

| 파일 | 설명 |
|------|------|
| `RECVIEW.H/.CPP` | `RecordViewBase` 레코드-뷰 바인딩 |
| `ENTCTRL.H/.CPP` | `EntryControl` 필드 편집 기반 |
| `SELCTRL.H/.CPP` | `SelectEntryControl` 선택 필드 |
| `TYPECTRL.H/.CPP` | `TypeEntryControl` 직접 입력 필드 |
| `CHKCTRL.H/.CPP` | `CheckEntryControl` 체크박스 필드 |

### 5.2 JANGO 애플리케이션 (주요 파일)

| 파일 | 설명 |
|------|------|
| `JANGO/JANGOAPP.H/.CPP` | 메인 `JangoApplication` |
| `JANGO/JOGLOBAL.H/.CPP` | JANGO 전역 (`BudaeName`, `PasswordTable`) |
| `JANGO/INSAWIN.H/.CPP` | 인사 윈도우 |
| `JANGO/ADDDLG.H/.CPP` | 추가 다이얼로그 |
| `JANGO/PASSDLG.H/.CPP` | 비밀번호 다이얼로그 |
| `JANGO/BUDAEDLG.H/.CPP` | 부대 변경 다이얼로그 |
| `JANGO/EQP24WIN.H/.CPP` | 2.4G 장비 윈도우 |
| `JANGO/EQ24DMAN.H/.CPP` | 2.4G 데이터 관리자 |
| `JANGO/EQ24NAME.H/.CPP` | 장비명 데이터 관리 |
| `JANGO/EQ24KIND.H/.CPP` | 장비종류 데이터 관리 |
| `JANGO/EQP24DEF.H` | `NAME24`, `KIND24`, `EQP24` 구조체 |
| `JANGO/EQP24GLB.H/.CPP` | 2.4G 전역 상수/설정 |
| `JANGO/DFILEMN.H/.CPP` | `DataFileManager` 단순 파일 관리자 |
| `JANGO/EQUIPWIN.H/.CPP` | 장비 윈도우 |
| `JANGO/EQSELGDLG.H/.CPP` | 장비 선택 다이얼로그 |
| `JANGO/BARGRAPH.H/.CPP` | 막대 그래프 |
| `JANGO/PIECHART.H/.CPP` | 파이 차트 |
| `JANGO/BLTCHART.H/.CPP` | BLT 차트 |
| `JANGO/INWONMAN.H/.CPP` | 인원 관리 |
| `JANGO/PERSNMAN.H/.CPP` | 인적 사항 관리 |
| `JANGO/PERSNREC.H/.CPP` | 인적 사항 레코드 |
| `JANGO/PERSNWIN.H/.CPP` | 인적 사항 윈도우 |
| `JANGO/OBSMAN.H/.CPP` | 관측 관리 |
| `JANGO/SAGOMAN.H/.CPP` | 사고 관리 |
| `JANGO/STATBAR.H/.CPP` | 상태 바 |
| `JANGO/JOAPPABT.H/.CPP` | About 창 |
| `JANGO/BIORYTHM.H/.CPP` | 바이오리듬 |
| `JANGO/INBIRTH.H/.CPP` | 출생 입력 |

### 5.3 2CAOCC 애플리케이션 (주요 파일)

| 파일 | 설명 |
|------|------|
| `2CAOCC/OCCAPP.H/.CPP` | 최상위 `OCCApplication` |
| `2CAOCC/OCCWIN.H/.CPP` | 메인 `OCCWindow` |
| `2CAOCC/OCCMAIN.H/.CPP` | 진입점/전역 |
| `2CAOCC/OCGLOBAL.H/.CPP` | 2CAOCC 전역 상태 |
| `2CAOCC/MENUBAR.CPP` | 메뉴 바 |
| `2CAOCC/METABANK.CPP` | 메타 데이터 뱅크 |
| `2CAOCC/METAELEM.CPP` | 메타 요소 |
| `2CAOCC/POSDTMAN.CPP` | 위치 데이터 관리자 |
| `2CAOCC/POSREC.CPP` | 위치 레코드 |
| `2CAOCC/DATAOS.CPP` | 데이터 OS/공유 |
| `2CAOCC/DTMANSET.CPP` | 데이터 관리자 설정 |
| `2CAOCC/COORDENT.CPP` | 좌표 입력 |
| `2CAOCC/CORDVIEW.CPP` | 좌표 뷰 |
| `2CAOCC/REALPOS.CPP` | 실제 위치 |
| `2CAOCC/REAL2PIX.CPP` | 실제→픽셀 변환 |
| `2CAOCC/CALCCORD.CPP` | 좌표 계산 |
| `2CAOCC/GETWIDTH.CPP` | 폭/스케일 계산 |
| `2CAOCC/AIRDLG.CPP` | 항공 다이얼로그 |
| `2CAOCC/AIRMAN.CPP` | 항공 관리 |
| `2CAOCC/AIRREC.CPP` | 항공 레코드 |
| `2CAOCC/BATTDLG.CPP` | 포대 다이얼로그 |
| `2CAOCC/BATTMAN.CPP` | 포대 관리 |
| `2CAOCC/BATTREC.CPP` | 포대 레코드 |
| `2CAOCC/FIREDLG.CPP` | 사격 다이얼로그 |
| `2CAOCC/FIREMAN.CPP` | 사격 관리 |
| `2CAOCC/FIREREC.CPP` | 사격 레코드 |
| `2CAOCC/PLNTGDLG.CPP` | 진지 다이얼로그 |
| `2CAOCC/PLNTGMAN.CPP` | 진지 관리 |
| `2CAOCC/PLNTGREC.CPP` | 진지 레코드 |
| `2CAOCC/HOWIZDLG.CPP` | 유도/마법사 다이얼로그 |
| `2CAOCC/HOWIZMAN.CPP` | 유도 관리 |
| `2CAOCC/HPTDLG.CPP` | HPT 다이얼로그 |
| `2CAOCC/HPTMAN.CPP` | HPT 관리 |
| `2CAOCC/HPTREC.CPP` | HPT 레코드 |
| `2CAOCC/DANDDLG.CPP` | 단대 다이얼로그 |
| `2CAOCC/DANDENT.CPP` | 단대 입력 |
| `2CAOCC/EDANDDLG.CPP` | 단대 편집 |
| `2CAOCC/ROADDLG.CPP` | 도로 다이얼로그 |
| `2CAOCC/CIRCLDLG.CPP` | 원 다이얼로그 |
| `2CAOCC/MOONCTRL.CPP` | 달/위성 컨트롤 |
| `2CAOCC/GMETTBL.CPP` | 기상 테이블 |
| `2CAOCC/MT_CHAR.CPP` | 문자 메타 |
| `2CAOCC/MT_PLLN.CPP` | 선 메타 |

---

## 6. 핵심 공통 유틸리티 헤더

| 파일 | 설명 |
|------|------|
| `keydef.h` | 키 코드/기본 타입 정의 |
| `jkpoint.h` | `JKPoint`, `JKRect` 등 기하 구조체 |
| `rectque.h` | `RectQue` 사각형 큐 |
| `tmpltdll.h` | `DLLFrame<T>` 이중 연결 리스트 템플릿 |
| `huboman.h` | `HuboManager` 코드-텍스트 변환 |

---

## 7. 참고: 전체 파일 트리 보기

Windows 명령 프롬프트에서 다음 명령으로 전체 구조를 확인할 수 있습니다.

```cmd
cd /d i:\progwork\JKENGINE
tree /f /a
```


## 4. RESOUCES (리소스, 폴더명 오타)

| 폴더/파일 | 설명 |
|-----------|------|
| `ICON/` | 아이콘 리소스 |
| `ICON/MOON/` | 달/위성 관련 아이콘 |
