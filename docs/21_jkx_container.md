# .jkx 단일 파일 앱 컨테이너 (Phase C)

> `engine`의 APK 유사 앱 패키징 규약. 2026-09-05 도입.
> Phase B(앱 모듈 DLL 동적 로딩, doc 19 §2) 위에서 동작한다.

## TL;DR

- `.jkx` = **매니페스트 + 앱 모듈 DLL + 런처 아이콘 PNG**를 하나로 묶은 컨테이너.
- 서버는 시작 시 `apps/*.jkx`를 스캔해 런처 셀을 만들고(아이콘도 컨테이너에서 디코드), 클릭 시 클라이언트를 `--jkx <경로>`로 스폰한다.
- 클라이언트 호스트는 컨테이너의 MODL 엔트리를 **임시 파일로 추출 → LoadLibrary → C ABI 실행**한다.
- `.jkx`가 없는 내장 앱은 기존 `--client <이름>` 셀이 폴백으로 유지된다.

## 1. 컨테이너 레이아웃

```
[0..3]   magic "JKX1"
[4..7]   tocOffset  (uint32 LE)
[8..11]  entryCount (uint32 LE)
[tocOffset .. tocOffset + 128*count)  TOC 엔트리:
  type[4]   "MANI"(매니페스트) | "MODL"(모듈 DLL) | "ICON"(아이콘 PNG)
  name[60]  NUL 패딩 ("manifest.txt", "jkapp_minesweeper.dll", "launcher@1x.png")
  offset[4] 페이로드 절대 오프셋
  size[4]   페이로드 바이트 수
[tocOffset + 128*count ..]  페이로드들 (TOC 순서대로 붙임)
```

- 타입은 이름에서 유도된다(`JKJkxFile::TypeForName`): `manifest.*` → MANI, `*.dll` → MODL, 그 외 ICON.
- 매니페스트는 key=value 텍스트(`JkxManifest::Parse`):

```
name=minesweeper        # 스폰 키 / 런처 식별 (필수)
title=Minesweeper       # 표시용 (정보성 — 런타임 권위는 모듈의 jk_app_meta)
width=320
height=380
module=jkapp_minesweeper.dll   # MODL 엔트리 이름 (필수)
icon=launcher@1x.png           # ICON 엔트리 이름 (선택)
icon2x=launcher@2x.png         # ICON 엔트리 이름 (선택)
```

## 2. 도구

| 명령 | 동작 |
|------|------|
| `jkx-pack <app>` | exe 옆의 `jkapp_<app>.dll` + `assets/icons/launcher_<pfx>@{1,2}x.png` + 생성한 매니페스트를 `apps/<app>.jkx`로 패킹. 메타는 모듈의 `jk_app_meta()`에서 읽음(단일 출처) |
| `--jkx <file>` | 컨테이너 열기 → MODL을 `%TEMP%\jkapp_<name>_<pid>.dll`로 추출 → LoadLibrary → `jk_app_meta`/`jk_app_run_client` 실행 |
| `--server` | 시작 시 `apps/*.jkx` 스캔(`ScanJkxApps`) → .jkx당 런처 셀 1개, 클릭 시 `--jkx <절대경로>` 스폰 |

- 패킹 순서: 매니페스트 → 모듈 → 아이콘. 아이콘은 없어도 되고, 매니페스트의 icon/icon2x 키가 생략된다.
- **빌드 자동 repack(2026-09-05)**: CMake 커스텀 타깃 `jkx_packages`(ALL)가 exe/DLL이 패키지보다 새로우면 `jkx-pack`을 재실행한다. 클라이언트 코드는 .jkx 안에 들어가므로 평범한 `cmake --build`만으로는 수정이 반영되지 않는다 — 이 타깃이 빌드→패킹을 한 커맨드로 묶어준다. 실행 중인 클라이언트가 자기 .jkx를 열어두므로(락) repack은 클라이언트 종료 후 유효하다.
- 서버 아이콘 선택은 asset 규약과 동일(`outputScale >= 1.5`면 @2x) — doc 20 §2.

## 3. 런타임 우선순위와 안전 규칙

- **런타임 메타 권위**: 창 제목/크기는 모듈의 `jk_app_meta()`가 결정한다. 매니페스트의 title/width/height는 컨테이너 도구(서버 런처 등)가 쓰는 정보성 사본.
- **모듈 경계**: Phase B와 동일하게 C ABI 2개만 사용(doc 19 §2). 컨테이너는 ABI를 바꾸지 않는다.
- **FreeLibrary 금지(중요)**: MinGW로 빌드한 앱 모듈을 프로세스 도중 언로드하면 힙이 오염된다(실측: 다음 malloc에서 SIGSEGV). 호스트/패커 모두 **모듈을 언로드하지 않고** 프로세스 종료에 맡긴다. 이 때문에 임시 DLL은 실행 후 남는다 — per-pid 파일명이 재실행 때 덮어써서 누적을 막는다.
- 스폰 스로틀(동일 앱 500ms)은 .jkx 경로에서도 경로 키로 동일 적용.

## 4. 코드 위치

| 요소 | 파일 |
|------|------|
| 컨테이너 읽기/쓰기 + 매니페스트 | `include/JKJkxFile.h`, `src/JKJkxFile.cpp` |
| 메모리 PNG 디코드 | `jk::LoadImageMemory` (`JKImageLoader`) |
| 패커 + `--jkx` 호스트 | `src/main.cpp` (`RunJkxPack`, `RunClientFromJkx`, `RunClientModule`) |
| 서버 스캔/스폰 | `JKWindowServer::ScanJkxApps`, `SpawnClient(name, fromJkx)` |
| 모듈 ABI | `include/apps/JKAppModule.h` (Phase B와 동일) |

## 5. 검증 (2026-09-05 통과)

1. 셀프테스트 `test`: pack → reopen → 매니페스트/페이로드 라운드트립 8건 PASS.
2. `jkx-pack minesweeper|tetris` → `apps/*.jkx` (각 3.5MB/3.0MB) 생성.
3. 서버 기동 로그: `installed app 'minesweeper' from minesweeper.jkx (icon decoded)` ×2 — 런처가 컨테이너 아이콘으로 구성됨. `.jkx`가 전부 있으면 레거시 셀은 생략됨.
4. 아이콘 클릭 → `spawned client --jkx ...` → 클라가 컨테이너에서 임시 DLL 추출·로드 → surface 연결(320x380/320x520) → 게임 렌더 확인.
5. 닫기 버튼 → 클라 exit=0, 서버 레이어 제거.

## 6. 알려진 제약

- 임시 DLL이 `%TEMP%`에 남음(§3 FreeLibrary 금지의 트레이드오프).
- 서버는 시작 시 1회만 스캔 — 실행 중 .jkx 설치/제거 반영 안 함(Phase 3 후보).
- 서명/무결성 검증 없음. 컨테이너 압축 없음(DLL 원본 그대로) — 이후 압축/서명 필드를 TOC에 추가 가능하도록 type 4cc 확려남.