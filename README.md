# JKEngine
JKEngine is GWES based on SDL2/3

## Layout (2026-09 restructure)

```
engine/    — the SDL2 window-server engine (was prototype/sdl2_jkwindow)
             self-contained: legacy/, assets/fonts/, tools/ vendored in-repo
legacy/    — frozen legacy sources & data (JKWINDOW, WINDBASE, JKDBASE,
             RESOUCES, engine-sources/) — read-only reference
docs/      — architecture documents (was ARCHITECTURE_DOCS)
tools/     — repo-level tools (sfxgen etc.)
```

## Build & run (engine/)

```
cd engine
build_sdl2_jkwindow.bat      # configure + build (or: cmake -B build && cmake --build build)
run_sdl2_jkwindow.bat        # single-process demo modes
build\jkdesktop.exe --server   # window-server mode
build\jkdesktop.exe test      # self-test
```

See `docs/19_sdl2_window_server.md` for the window-server architecture,
`docs/24_folder_restructure.md` for the layout rationale.