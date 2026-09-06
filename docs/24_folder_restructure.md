# 폴더 구조 정리 — A·B 실행 완료

> 2026-09-05. 사용자 요청 "작업 폴드 정리는 a, b 모두"에 따라 A(자족화) 실행 후,
> B(루트 재구조화)도 승인받아 같은 날 실행 완료. 아래는 실행 결과와 절차 기록.

## A. 프로토타입 자족화 (실행 완료 2026-09-05)

`engine`이 JKENGINE 루트 밖 파일을 전혀 참조하지 않게 정리했다.
빌드·셀프테스트(0 failures)·vfont 스폰 렌더(한글 벡터 글리프)까지 확인.

| 이전 (외부 참조) | 이후 (자족) | 비고 |
|---|---|---|
| `${JKENGINE_ROOT}/JKWINDOW/WANCODE.CPP` + `WANCODE.H` → 컴파일 | `legacy/wancode/` 벤더 (19KB) | `typedef.h`(911B)도 함께 벤더 — `wancode.h`가 include |
| `${JKENGINE_ROOT}/JKWINDOW`, `${JKENGINE_ROOT}` include dir | 제거 | `legacy/wancode` include만 추가 |
| `${JKENGINE_ROOT}/WINDBASE/2CAOCC` 런타임 폰트 (`JKENGINE_FONT_DIR`) | `assets/fonts/` 벤더 (english.vft 20KB + hanmoon.vft 1.6MB) | CMake 정의가 `assets/fonts`를 가리킴 |
| CWD-상대 `*.fnt` 4종 (HangulManager) | `assets/fonts/` 벤더 (~180KB) | `SDL_GetBasePath()` 우선 → CWD 폴백 (`JKHangulManager.cpp`) |
| 루트 `tmp/` 테스트 프로브 흩어짐 (444MB) | `tools/probes/*.ps1` 19종 + `tools/cdb/` 디버거 | 일회성 산출물(PNG/로그/disasm 380MB+)은 삭제 |

- 루트 `tmp/`는 삭제됨. 합성입력 프로브는 이제 `engine/tools/probes/`.
- 자족 효과: 프로토타입 디렉터리만 복사하면 어디서든 빌드·실행된다
  (`build_with_temp.sh`의 `JKENGINE_ROOT` 변수는 더 이상 불필요).
- `%TEMP%`의 stale `jkapp_*.dll` 104MB도 정리 (C: 풀잠 원인 제거 겸사).

## B. JKENGINE 루트 승격 구조 (제안 — 미실행)

### 현황의 문제

- 루트에 레거시 산물이 뒤섞여 있다: `WINDBASE/`(470MB), `JKWINDOW/`(210MB),
  `JKDBASE/`(29MB), 루트 산발 소스·폰트·IDE 잔재(ENGIN.IDE/.DSW/.LIB, .EXE),
  `build/`(빈 디렉터), 일회성 py/로그.
- `.git`이 1.9GB — 레거시 데이터 블롭이 히스토리에 누적. `git mv`는 같은 볼륨
  이름 변경이라 추가 디스크가 필요 없지만, **이동 자체가 모든 문서의 경로 표기,
  메모리, 스크립트 하드코딩을 깨뜨린다** (docs 19-23 전부 `engine`
  기준).

### 제안 트리

```
JKENGINE/
├── engine/                    ← engine 승격 (이것이 진짜 엔진)
│   ├── CMakeLists.txt  src/  include/  assets/  third_party/
│   ├── legacy/                ← A에서 벤더한 레거시 소스
│   ├── tools/                 ← probes/ + cdb/ (A에서 정리됨)
│   └── build/                 (gitignore)
├── legacy/                    ← 레거시 원본 보관 (읽기 전용)
│   ├── JKWINDOW/              ← 1995-96 Borland GUI lib (WANCODE는 engine에 벤더됨)
│   ├── WINDBASE/              ← OCC/JANGO 원본 + 에셋
│   ├── JKDBASE/
│   └── engine-sources/        ← 루트 산발 소스·헤더·폰트 (BASEFUNC, TYPEDEF.H…)
├── docs/                      ← docs/ 이름 변경
├── tools/                     ← 기존 루트 tools/ (sfxgen 등)
└── README.md
```

### 실행 기록 (2026-09-05 승인 후 완료)

1. `git mv prototype/sdl2_jkwindow engine` — 시도 시 `build/`(빈 껍데기)가
   알 수 없는 프로세스 핸들에 붙잡혀 디렉터 rename 실패(Permission denied).
   대응: 자식 단위 이동 — 추적 파일은 `git mv`(rename 스테이징, 미커밋 수정분은
   unstaged 유지), untracked는 `mv`. `build/`는 내용물 삭제 후 빈 껍데기만 남김
   (gitignore 대상, 재생성 물건 — 잠금 해제 후 삭제하면 됨).
   `third_party/`(stb_image.h 1개)는 복사로 해결.
2. `git mv ARCHITECTURE_DOCS docs`, `git mv phase1_input_focus.md docs/`.
3. `git mv JKWINDOW|WINDBASE|JKDBASE|RESOUCES → legacy/`, 루트 산발
   소스·헤더·폰트·IDE 잔재는 `legacy/engine-sources/`로(31종), 일회성
   로그/stackdump/py/typescript 삭제, 빈 루트 `build/` 제거.
4. 참조 일괄 수정: `docs/*.md` + `engine/*.sh|.bat` + `engine/tools/probes/*.ps1`
   + `.gitignore`(`prototype/sdl2_jkwindow` → `engine`, `ARCHITECTURE_DOCS` →
   `docs`), README.md 재작성.
5. 클린 재구성 빌드 + 셀프테스트 + 서버 스모크로 검증(아래).
6. Claude 메모리 경로 갱신.

### 남은 것

- `prototype/sdl2_jkwindow/build/` 빈 껍데기: 잠금 해제되면 삭제
  (탐색기에서 수동 또는 재부팅 후 `rmdir`). git 무관.
- `.git` 1.9GB — 별도 과제로 `git gc`/LFS 마이그레이션 검토(백업 후).

**권고했던 대로 터미널(문서 22)·ImGui(문서 23) 구현 착수 전에 경로가 확정됐다.**