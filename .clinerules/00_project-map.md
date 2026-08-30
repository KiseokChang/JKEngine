# JKENGINE 프로젝트 맵

작업 시작 전 이 맵으로 위치를 파악한다.

## 워크스페이스 구조 (i:\progwork\JKENGINE)

- **루트**: 도스 시절 레거시 엔진 소스(BASEFUNC.CPP, JKPOINT.CPP, RECTQUE.CPP 등), 폰트(ENGLISH.FNT / HANGUL.FNT / HANJA.FNT / SPECIAL.FNT), 레거시 데이터(OCCDATA.DAT, OCCUNIT.DAT, OCCFIRE.DAT, JKIMAGE.DAT, JKPALETT.DAT), JKENGIN.LIB
- **JKDBASE/**: 데이터베이스 계층 원본 (DataManager, TableManager, `.tbl` 구조)
- **JKWINDOW/**: 윈도우/GWES 계층 원본 (JKAPP.CPP 등). WANCODE.CPP는 SDL2 프로토타입이 공유 사용
- **WINDBASE/**: 애플리케이션 계층 원본. `WINDBASE/JANGO` = 장고 런처, `WINDBASE/2CAOCC` = 2차 포병 지휘(C2)
- **RESOUCES/**: 리소스
- **prototype/sdl2_jkwindow/**: SDL2 + C++17 JKWindow 프로토타입 — **활성 개발 대상**
  - `src/`: JKApplication/JKWindow/JKControl/JKDC 등 프레임워크 + `src/apps/` (Jango, Occ, Equip, Equip24, Insa, Pcx, Vector, IconEdit, Recog, VectorFont, VectorPres)
  - `include/`: 헤더
  - `build_sdl2_jkwindow.bat` / `run_sdl2_jkwindow.bat`: 빌드/실행 진입점
- **ARCHITECTURE_DOCS/**: 번호 순서 문서. 주요 문서: `10_sdl2_windows_setup.md`(개발환경), `11_jkwindow_sdl_mapping.md`(API 매핑), `12_sdl2_prototype_roadmap.md`(로드맵), `20_sdl2_jango_porting_plan.md`(JANGO 포팅 계획). **구조/계획 변경 시 해당 문서도 갱신**
- **tools/**: `install_sdl2_toolchain.sh`(MSYS2 환경 구축), 빌드/런 bat, `verify_fixwin3.ps1`(DPI-aware 화면 검증 — `ARCHITECTURE_DOCS/15_verification_playbook.md`), `app_mouse_test3.ps1`(드래그 테스트 드라이버), `probe_mouse_scaling.ps1`(마우스/DPI 배율 실측 — 15 문서 §9), `click_jango_probe.ps1`(런처 버튼 클릭 E2E — 캡처 경로 회귀 검증), `verify_iconedit_mouse.ps1`(iconedit 그리드 셀 클릭 E2E — 마우스/화면 좌표 불일치 탐지), `fix_bom.ps1`(ps1 BOM 복구)

## 임시 디렉터리 (레포 외부)

- `C:\temp_jkproto_sdl2_jkwindow\`: 빌드용 임시 소스 트리 (빌드마다 재생성됨. 지워도 무방)
- `C:\temp_jkwin_verify\`: 화면 검증 스크린샷/읽기용 복사본 임시 작업장 (지워도 무방 — 검증 스크립트 원본은 `tools/`에 상주)

## 문서/코드 규칙

- 문서와 코드 주석은 한국어
- 레거시 소스는 Win16 한글 GDI 스타일을 유지; 프로토타입은 C++17 + SDL2
- JKRect 등 레거시 구조체는 inclusive 좌표(아래 20_dpi-coordinates.md 참조)