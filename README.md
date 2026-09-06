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

Prerequisites: MSYS2 UCRT64 toolchain — see **`docs/10_sdl2_windows_setup.md`**
for the full clone → build → run guide (packages, troubleshooting).

```
cd engine
export PATH="/c/msys64/ucrt64/bin:$PATH"
cmake -B build -G Ninja && cmake --build build
build/jkdesktop.exe --server    # window-server desktop (launcher + taskbar)
build/jkdesktop.exe terminal    # single-process ConPTY terminal
build/jkdesktop.exe test        # self-test
```

See `docs/19_sdl2_window_server.md` for the window-server architecture,
`docs/24_folder_restructure.md` for the layout rationale.