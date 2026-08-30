# 에이전트 도구 워크어라운드 (반복 삽질 방지)

## read_files / run_commands — 캐시·빈 출력

- 방금 수정한 파일을 읽으면 `[outdated]` 표시나 빈 내용이 반환될 수 있음
- 해결: `copy /y <원본> C:\temp_jkwin_verify\<새이름>` 으로 **새 경로에 복사본을 만들어 읽고**, 끝나면 삭제
- `run_commands` 출력도 간헐적으로 빈 문자열로 표시됨 → 빈 출력 = 실패로 단정하지 말 것. 파일 리다이렉션(`> log.txt`) 후 read_files로 확인하거나 재시도
- 빈 결과를 보고 "명령이 실패했다/아무 일도 안 일어났다"고 사용자에게 보고하지 말 것

## editor — 크기 제한

- `new_text`는 한 번에 **6000자 이하**. 큰 파일은 여러 번 분할 편집
- `old_text`는 파일에서 정확히 한 번만 매치되어야 함 (매치 0회/2회 이상 → 실패)

## cmd.exe → PowerShell 호출

- cmd.exe는 `powershell -Command "...")` 안의 **중첩 따옴표를 깨뜨린다** → 복잡한 코드는 `.ps1` 파일로 저장하고 `powershell -NoProfile -ExecutionPolicy Bypass -File <script>` 로 실행
- 재사용할 스크립트는 레포 `tools/`에 남기고(예: `verify_fixwin3.ps1`), 일회성이면 `C:\temp_jkwin_verify\`에 작성
- PowerShell 스크립트 출력을 `> log.txt`로 리다이렉트하면 한글이 깨질 수 있음(콘솔 코드 페이지로 인코딩) — PASS/FAIL/RESULT 등 ASCII 판정 라인은 영향 없음

## .ps1 인코딩 — UTF-8 BOM 필수

- **한글 포함 .ps1은 UTF-8 with BOM으로 저장해야 함.** BOM이 없으면 PowerShell 5.1이 ANSI(cp949)로 읽어 파싱 에러/문자 깨짐
- editor 도구로 저장하면 BOM이 빠질 수 있음 → 저장 후 `powershell -NoProfile -ExecutionPolicy Bypass -File tools\fix_bom.ps1 -Path <파일>` 실행 (BOM 확인/부착 유틸)
- 증상: `Unexpected token`, 라벨 깨짐, 스크립트가 그저 빈 결과를 냄

## PowerShell 수치 처리

- PowerShell `[int]` 캐스팅은 **banker's rounding**(0.5 → 짝수). 일반 반올림 필요 시 `[Math]::Floor($x + 0.5)`
- 픽셀 검증 로직 작성 시 이 차이로 기대값이 어긋날 수 있음

## bash 호출

- MSYS2 bash는 `C:\msys64\usr\bin\bash.exe -l`(로그인 셸)로 호출할 것 — PATH에 `/c/msys64/ucrt64/bin`이 잡히지 않으면 cmake/ninja/g++을 못 찾음
- `build_with_temp.sh`, `smoke_*.sh` 등 기존 셸 스크립트를 우선 재사용

## 마우스 배율 세션(2026-08-29) 신규 함정

- **cmd.exe `%ERRORLEVEL%`은 같은 행 파싱 시점에 확장**된다 — `build.bat & echo BUILD_EXIT=%ERRORLEVEL%`은 실패여도 0을 찍는다. 판정은 래퍼 cmd가 `%ERRORLEVEL%`을 **별도 행으로 로그에 append**(`echo BAT-EXIT=%%ERRORLEVEL%>> log`)한 뒤 그 행을 읽어 판별.
- **findstr 다중 검색어/`/C:"a" /C:"b"`는 cp949 콘솔에서 인용 코드가 깨져** "찾을 수 없습니다" 오류를 뱉는다. 단일 토큰 검색 또는 문서 직접 읽기로 대체. 빈 출력·exit 1을 "결과 없음"으로 단정 금지.
- **run_commands는 30초 무응답 시 작업 트리째 kill**한다 — 풀빌드(33스텝)·프로브 드라이버(~50초)가 링크 직전에 죽는다. 오래 도는 것은 **WMI 분리 스폰**(`wmic process call create 'cmd /c C:\temp_jkwin_verify\wrapper.cmd'`) 후 로그 폴링. `start "" /min`·PowerShell `Start-Process`는 작업 트리 회수에 걸려 콜이 계속 블록될 수 있다.
- **`Start-Process -WindowStyle Hidden`은 SDL 첫 창에 SW_HIDE를 적용**한다 → MainWindowHandle=0, 보이지 않아 마우스 이벤트가 안 와서 앱이 "응답 없는 것"처럼 보인다. 해결: `EnumWindows`(클래스 `SDL_app`, PID 필터)로 HWND를 구해 `ShowWindow(5)` 복원 후 스윕.
- 에디터의 방금 파일 저장이 빌드 스크립트의 소스 복사와 **경합**할 수 있다 — 이유 없이 다수 TU가 FAIL(컴파일러 메시지 없음)이면 단일 TU 명령컴파일로 재확인 후 재빌드(대부분 통과).
- exe 문자열 존재 확인(계측 코드 반영 여부)은 `findstr /m MOUSEPROBE <exe>` 바이너리 검색으로 가능 — 단, `%ERRORLEVEL%` 파싱 확장 함정으로 별도 행에서 판정할 것.