# CEF (Chromium Embedded Framework) — vendored binary distribution

Not in git. Fetch manually and extract here (`engine/third_party/cef/`):

- Distribution: `cef_binary_144.0.6+g5f7e671+chromium-144.0.7559.59_windows64_minimal.tar.bz2`
  (Spotify builds — https://cef-builds.spotifycdn.com/index.html)
- Contents used by the engine:
  - `include/` — C API headers (`include/capi/*`, `include/cef_api_hash.h`)
  - `Release/libcef.dll` — the only link input (`-l:libcef.dll`, no import lib
    or libcef_dll_wrapper needed)
  - `Release/*`, `Resources/*` — runtime set copied to the app directory
    (chrome_elf.dll, icudtl.dat, v8_context_snapshot.bin, *.pak, locales/, …)
- License: BSD-3 (LICENSE.txt). Chromium components carry their own notices
  (CREDITS.html).

Used by `engine/tools/cefosr` (standalone OSR demo) and `jkapp_browser`.