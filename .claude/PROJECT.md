# JKENGINE — Claude Project Map

> 이 파일은 Claude Code가 JKENGINE 작업을 시작할 때 참고하는 요약 맵입니다.
> 복잡한 세부 사항은 ARCHITECTURE_DOCS/ 와 tools/ 에 남아 있으며, 이 문서는 위치 파악용입니다.

## 1. Workspace Structure

- **Root**: DOS-era legacy engine sources, fonts (ENGLISH.FNT, HANGUL.FNT, HANJA.FNT, SPECIAL.FNT), legacy data (OCCDATA.DAT, OCCUNIT.DAT, OCCFIRE.DAT, JKIMAGE.DAT, JKPALETT.DAT), JKENGIN.LIB.
- **JKDBASE/**: Original database layer (DataManager, TableManager, `.tbl` structures).
- **JKWINDOW/**: Original windowing/GWES layer (JKAPP.CPP, etc.). WANCODE.CPP is shared with the SDL2 prototype.
- **WINDBASE/**: Original application layer. `WINDBASE/JANGO` = launcher, `WINDBASE/2CAOCC` = fire-control C2.
- **prototype/sdl2_jkwindow/**: Active SDL2 + C++17 JKWindow prototype.
  - `src/`: framework (JKApplication, JKWindow, JKControl, JKDC, ...) + `src/apps/` (Jango, Occ, Equip, Equip24, Insa, Pcx, Vector, IconEdit, Recog, VectorFont, VectorPres).
  - `include/`: headers.
  - `build_sdl2_jkwindow.bat` / `run_sdl2_jkwindow.bat`: build/run entry points.
- **ARCHITECTURE_DOCS/**: numbered architecture docs. Key: `10_sdl2_windows_setup.md`, `11_jkwindow_sdl_mapping.md`, `12_sdl2_prototype_roadmap.md`, `14_sdl2_window_dpi.md`, `15_verification_playbook.md`, `20_sdl2_jango_porting_plan.md`.
- **tools/**: build helpers, screen-verification probes, mouse/DPI probes, BOM fixer.

## 2. Conventions

- Docs and code comments are in Korean.
- Legacy sources keep Win16 Korean GDI style; prototype uses C++17 + SDL2.
- JKRect is inclusive (`h=24` draws 25 rows, Win16 GDI convention).

## 3. Build / Run

- Always run `prototype\sdl2_jkwindow\build_sdl2_jkwindow.bat` (calls `build_with_temp.sh`).
- The build copies the source tree to `C:\temp_jkproto_sdl2_jkwindow`, builds there with CMake+Ninja (UCRT64), and copies only the `.exe` back to `I:\...\prototype\sdl2_jkwindow\build\`.
- Do **not** run cmake/ninja directly inside the I: drive build directory — MinGW fails writing object files there.
- Run: `prototype\sdl2_jkwindow\run_sdl2_jkwindow.bat [mode]`.
- Modes: (none)=main demo, `test`=self-test, `jango`, `occ`, `pcx FILE`, `vector`, `iconedit`, `recog`, `vfont`, `vpres`.

## 4. Verification Checklist

After any framework, UI, layout, or mouse change:

1. `jkproto_sdl2_jkwindow.exe test` → `AppSelfTest: 0 failure(s)`
2. `tools\verify_fixwin3.ps1` → 14 pass
3. `tools\click_jango_probe.ps1` → PASS
4. `tools\verify_iconedit_mouse.ps1` → 2 pass
5. If mouse/scale suspected: `tools\probe_mouse_scaling.ps1 -Phase both`

## 5. Critical Pitfalls (TL;DR)

- **DPI**: app logical coords are 1920×1080; render uses `fit = min(clientW/1920, clientH/1080)` with letterboxing.
- **Mouse/screen mismatch** is usually `JKApplication::Render` target/scale order or stale `ptToPhys` use.
- **Never** scale Win32 window/monitor coordinates by hand — PMv2 returns physical pixels.
- **PS1 scripts with Korean text must be UTF-8 with BOM**; use `tools\fix_bom.ps1` if saved via editor.
- **.claude/** local working dir is ignored; old `.clinerules/` is removed.
