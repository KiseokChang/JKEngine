# 빌드 / 실행 / 테스트 (SDL2 프로토타입)

## 빌드

- 항상 `prototype\sdl2_jkwindow\build_sdl2_jkwindow.bat`를 실행한다 (내부에서 `build_with_temp.sh` 호출)
- 절차: 소스 전체를 `C:\temp_jkproto_sdl2_jkwindow`로 복사 → 그곳에서 CMake+Ninja(UCRT64) 빌드 → **exe만** 다시 `I:\...\prototype\sdl2_jkwindow\build\`로 복사
- 이유: MinGW가 I: 드라이브에 오브젝트 파일을 쓰지 못한다. **절대 I:의 build 디렉터리에서 직접 cmake/ninja 하지 말 것**
- 툴체인: MSYS2 UCRT64(`C:\msys64\ucrt64`). bash 호출은 반드시 `C:\msys64\usr\bin\bash.exe -l` 형태(로그인 셸) — PATH에 ucrt64/bin이 잡히지 않으면 cmake/ninja를 못 찾음
- 빌드 실패 시: `build*.log`, `err.txt`, `compile_err.txt` 등 스크립트가 지정한 로그 파일을 읽어 원인 파악. 콘솔 출력이 비어 보여도 실패로 단정하지 말 것 (30_tool-workarounds.md 참조)

## 실행

- `prototype\sdl2_jkwindow\run_sdl2_jkwindow.bat [mode]` — 스크립트가 `build/`로 cd 후 exe 실행
- 모드: (인자 없음)=JKWindow 메인 데모 | `jango`=장고 런처 | `occ`=2CAOCC C2(부대+사격지시+타이머) | `test`=데이터 매니저 셀프테스트
- **앱은 cwd에서 데이터 파일을 찾는다**: OCCDATA.DAT, OCCUNIT.DAT, OCCFIRE.DAT, *.FNT 등은 `build/` 디렉터리에 있어야 함. 새 데이터 파일 추가 시 `build/`에 수동 복사 (빌드는 exe만 덮어쓰므로 기존 데이터는 유지됨)

## 변경 후 필수 검증

1. `jkproto_sdl2_jkwindow.exe test` 실행 → `AppSelfTest: 0 failure(s)` (28 체크) 확인
2. UI/윈도우 변경 시 `run_sdl2_jkwindow.bat jango`와 `occ`를 각각 실행, 화면 뜨는지 확인
3. 화면 좌표·레이아웃·스케일 변경 시: `tools\verify_fixwin3.ps1` (DPI-aware 캡처, `-mode occ|jango`, 모드당 14항목) 실행 — 방법론/체크 상세: `ARCHITECTURE_DOCS\15_verification_playbook.md`