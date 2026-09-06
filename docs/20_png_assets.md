# PNG 에셋 스펙 (아이콘/배경)

> `engine`의 런타임 이미지 에셋(PNG) 규약. 2026-09-05 도입.
> 기존 PCX/WANCODE 경로는 레거시 호환용으로만 남겨두고, UI 아이콘·배경은
> **PNG 우선 + 절차적(코드 생성) 폴백** 원칙으로 간다.

## TL;DR

- 로더: **stb_image v2.30** 단일 헤더(`third_party/stb/stb_image.h`). 추가 의존성 없이 PNG/JPEG 디코드.
- 경로: `assets/icons/<이름>@1x.png` / `@2x.png`, `assets/backgrounds/<이름>@1x.png` / `@2x.png`.
- 해상도 배선택: **서버는 `outputScale >= 1.5`면 @2x, 아니면 @1x. 클라이언트는 항상 @1x**(클라 surface는 논리 px 공간이므로 — doc 19 §6).
- 로드 실패 시 앱이 정의한 절차적 폴백으로 자동 대체(에셋 없이도 동작 보장).
- 빌드: CMake POST_BUILD가 `assets/` 통째로 exe 옆에 복사(`SDL_GetBasePath()` 기준 로딩).

## 1. 아이콘 크기 표

| 용도 | @1x | @2x | 파일 | 사용처 |
|------|-----|-----|------|--------|
| UI 아이콘 (인게임) | 16×16 | 32×32 | `assets/icons/<이름>@1x\|@2x.png` | 지뢰찾기 mine/flag/question (클라, @1x만 사용) |
| 앱 아이콘 (일반 컨트롤) | 32×32 | 64×64 | `assets/icons/<이름>@1x\|@2x.png` | `AppLauncherItem` 등 (`kLauncherIconSize=32`) |
| 런처 아이콘 | 64×64 | 128×128 | `assets/icons/launcher_<앱>@1x\|@2x.png` | 서버 런처 데스크톱 셀(64×80: 64×64 아트 + 16px 라벨 영역) |
| 배경 | 1280×720 | 2560×1440 | `assets/backgrounds/<이름>@1x\|@2x.png` | 서버 런처 데스크톱 전체 |

- 런처 아이콘 셀 rect는 논리 pt(현재 `{50,50,64,80}` minesweeper, `{150,50,64,80}` tetris). 아트는 셀 상단에 1:1로 그리고(64×64), 나머지는 라벨 영역.
- 클라이언트가 @1x만 쓰는 이유: 클라 surface의 좌표 공간이 논리 px이고 서버가 합성 시 물리 px로 스케일링한다. 서버(런처/배경)는 물리 px로 직접 그리므로 @2x가 필요하다.

## 2. 로딩 규약

- **해상도 선택**: 서버 `JKWindowServer::LoadTextureScaled`가 `compositor_->OutputScale() >= 1.5f`일 때 `@2x.png`, 아니면 `@1x.png`. 클라이언트(`JKResourceCache::LoadImagePNG` 호출부)는 `@1x` 고정.
- **알파**: RGBA32. `stbi_load(req_comp=4)`로 강제 4채널. 서버 텍스처는 `SDL_BLENDMODE_BLEND`, 클라 `CreateImageFromRGBA` 기존 경로.
- **경로 해석**(`ResolveAssetPath`): `SDL_GetBasePath()` + 상대경로 → 없으면 cwd 기준. 빌드가 에셋을 exe 옆에 복사하므로 어디서 실행해도 로드된다.
- **폴백**: `LoadImagePNG` 실패(false 반환) → 호출 코드가 `CreateImageFromRGBA` 절차 아트로 대체. 폴백 픽셀은 PNG와 동일하게 유지(`ClientMineSweeperApp.cpp`의 16×16 생성기와 `tmp/make_assets.ps1`의 생성 루프가 같은 픽셀 로직).
- **실패 로그**: `JKImageLoader: failed to load '<절대경로>': <stb 에러>` 1줄. 폴백이 정상 동작하면 무해.

## 3. 코드 위치

| 요소 | 파일 |
|------|------|
| 디코드 + 경로 해석 | `include/JKImageLoader.h`, `src/JKImageLoader.cpp` (stb_image 구현 define은 이 파일 하나만) |
| 클라 캐시 진입 | `JKResourceCache::LoadImagePNG(key, path)` → `CreateImageFromRGBA` |
| 서버 텍스처 로딩 | `JKWindowServer::LoadTextureScaled` (SDL_Surface → 텍스처, blend on) |
| 서버 런처 그리기 | `JKWindowServer::DrawLauncherBackground` (배경 → 셀 아트 → 레거시 플랫 rect 폴백) |
| 에셋 생성 스크립트 | `tmp/make_assets.ps1` (System.Drawing — 폴백 픽셀 로직과 동일한 루프) |
| 빌드 복사 | `CMakeLists.txt` POST_BUILD `copy_directory assets → $<TARGET_FILE_DIR>/assets` |

## 4. 현재 에셋 목록

| 파일 | 크기 | 비고 |
|------|------|------|
| `assets/backgrounds/desktop@1x.png` | 1280×720 | 사진 배경 (picsum #1018) |
| `assets/backgrounds/desktop@2x.png` | 2560×1440 | 〃 |
| `assets/icons/launcher_mine@1x/2x.png` | 64/128 | 지뢰 런처 아이콘 (투명 배경) |
| `assets/icons/launcher_tetris@1x/2x.png` | 64/128 | 테트리스 T조각 런처 아이콘 |
| `assets/icons/mine@1x.png` | 16 | 인게임 지뢰 (폴백과 동일 픽셀) |
| `assets/icons/flag@1x.png` | 16 | 인게임 깃발 |
| `assets/icons/question@1x.png` | 16 | 인게임 물음표 |

## 5. 새 에셋 추가 절차

1. 위 표의 크기 규약에 맞춰 PNG 제작(@1x/@2x 세트).
2. `assets/icons/` 또는 `assets/backgrounds/`에 배치 — 빌드가 자동 복사.
3. 로딩 코드는 `LoadImagePNG`/`LoadTextureScaled` 호출 1줄 + 폴백 1줄.
4. 클라에서 쓰면 @1x, 서버에서 쓰면 `LoadTextureScaled`로 배선택.
5. 앱을 배포하려면 아이콘을 `.jkx` 컨테이너에 ICON 엔트리로 넣는다(`21_jkx_container.md`).