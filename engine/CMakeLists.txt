cmake_minimum_required(VERSION 3.16)
project(jkdesktop LANGUAGES C CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# SDL2: CMake config가 있으면 사용, 없으면 pkg-config fallback
find_package(SDL2 QUIET)
if(NOT TARGET SDL2::SDL2)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED sdl2)

    add_library(SDL2::SDL2 INTERFACE IMPORTED)
    set_target_properties(SDL2::SDL2 PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${SDL2_INCLUDE_DIRS}"
        INTERFACE_LINK_LIBRARIES "${SDL2_LIBRARIES}"
    )
endif()

# SDL2_mixer: CMake config가 있으면 사용, 없으면 pkg-config fallback.
find_package(SDL2_mixer QUIET)
if(NOT TARGET SDL2_mixer::SDL2_mixer)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2_MIXER REQUIRED SDL2_mixer)

    add_library(SDL2_mixer::SDL2_mixer INTERFACE IMPORTED)
    set_target_properties(SDL2_mixer::SDL2_mixer PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${SDL2_MIXER_INCLUDE_DIRS}"
        INTERFACE_LINK_LIBRARIES "${SDL2_MIXER_LIBRARIES}"
    )
endif()

# FFmpeg (docs/23 §11.8 vplayer): msys2 ucrt64 pacman 패키지. jkcore에 걸지
# 않고 jkapp_vplayer 타깃만 링크 — FFmpeg는 오디오/비디오 앱 전용 의존성.
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
    libavformat libavcodec libswscale libswresample libavutil)

# The prototype is self-contained: the legacy WANCODE conversion table and the
# runtime fonts are vendored under legacy/ and assets/fonts/ (copied verbatim
# from JKENGINE/JKWINDOW and JKENGINE/WINDBASE/2CAOCC), so the build no longer
# references paths outside this directory.

# ---------------------------------------------------------------------------
# quickjs (vendored quickjs-ng, docs/27 단계 1): the C11 core interpreter
# behind JKScriptHost. Vendored at master 02368b6b (2026-09-05), trimmed to
# the four library sources + headers (no CLI/libc/test262). Mirrors the
# upstream CMake's compile definitions for the library.
# ---------------------------------------------------------------------------
set(QJS_DIR third_party/quickjs-ng)
add_library(quickjs STATIC
    ${QJS_DIR}/quickjs.c
    ${QJS_DIR}/dtoa.c
    ${QJS_DIR}/libregexp.c
    ${QJS_DIR}/libunicode.c
)
target_include_directories(quickjs PUBLIC ${QJS_DIR})
target_compile_definitions(quickjs PRIVATE
    _GNU_SOURCE QUICKJS_NG_BUILD
    $<$<PLATFORM_ID:Windows>:WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0601>
)
set_target_properties(quickjs PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS ON
)

# ---------------------------------------------------------------------------
# imgui (vendored Dear ImGui v1.92.9b, docs/23 §3): immediate-mode GUI core
# for the utility-app track. Tag commit f1cc2ae15e53a861a874c3034aae6798fde194ab
# (2026-07-31 hotfix release; also upstream latest as of 2026-09-06).
# Deliberately NOT inside jkcore — jkcore is statically embedded into every
# app module DLL, and ImGui would tax all the modules that never use it.
# Only ImGui apps link this target. The custom backend
# imgui_impl_jkwindow.cpp joins this target (docs/23 §3.2/§4) and needs the
# JKEvent.h header path from jkcore's include tree.
# ---------------------------------------------------------------------------
set(IMGUI_DIR third_party/imgui)
add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    # Custom fused backend (docs/23 §4): JKEvent feeding + SDL_RenderGeometry
    # raster onto the client's offscreen target texture. Needs SDL headers and
    # the JKEvent.h path from jkcore's include tree (already in include/).
    src/imgui_impl_jkwindow.cpp
)
target_include_directories(imgui PUBLIC
    ${IMGUI_DIR}
    ${CMAKE_SOURCE_DIR}/include
    # imgui_club extras (imgui_memory_editor.h, docs/23 §11.2 Phase 3) —
    # header-only, so only the include path is needed.
    third_party/imgui_club
)
target_link_libraries(imgui PUBLIC SDL2::SDL2)

# ---------------------------------------------------------------------------
# implot (vendored master 7eeb916, 2026-08-06 — first line with Dear ImGui
# 1.92 texture-API support, docs/23 §11.2 Phase 3): plotting extension on
# top of the imgui target. Same rule as imgui itself: NOT inside jkcore,
# only apps that plot link this.
# ---------------------------------------------------------------------------
set(IMPLOT_DIR third_party/implot)
add_library(implot STATIC
    ${IMPLOT_DIR}/implot.cpp
    ${IMPLOT_DIR}/implot_items.cpp
    ${IMPLOT_DIR}/implot_demo.cpp
)
target_include_directories(implot PUBLIC ${IMPLOT_DIR})
target_link_libraries(implot PUBLIC imgui)

# ---------------------------------------------------------------------------
# jkcore: everything except main(), the RC file, and the apps. The exe and
# each app module DLL link this statically — every module carries its own
# copy of the core, and all cross-module calls go through the C ABI in
# include/apps/JKAppModule.h (no C++ objects cross a module boundary).
# ---------------------------------------------------------------------------
add_library(jkcore STATIC
    src/JKEvent.cpp
    src/JKMessageQue.cpp
    src/JKDC.cpp
    src/JKSDLRenderBackend.cpp
    src/JKOffscreenSurface.cpp
    src/JKControl.cpp
    src/JKStatic.cpp
    src/JKButton.cpp
    src/JKCheckBox.cpp
    src/JOClock.cpp
    src/JKEdit.cpp
    src/JKHangulAutomata.cpp
    src/JKHangulUtil.cpp
    src/JKPlatform_win32.cpp
    src/JKScrollBar.cpp
    src/JKListBox.cpp
    src/JKComboBox.cpp
    src/JKFileDialog.cpp
    src/JKMenu.cpp
    src/JKMessageBox.cpp
    src/JKDialog.cpp
    src/JKWindow.cpp
    src/JKWindowManager.cpp
    src/JKMessageBus.cpp
    src/JKTimerThread.cpp
    src/JKAudioThread.cpp
    src/JKAudioCommand.cpp
    src/JKSDLAudioBackend.cpp
    src/JKRenderThread.cpp
    src/JKRenderCommandList.cpp
    src/JKApplication.cpp
    src/ipc/JKWireProtocol.cpp
    src/ipc/JKPipeTransport_win32.cpp
    src/ipc/JKPipeTransport_posix.cpp
    src/ipc/JKSharedMemory_win32.cpp
    src/ipc/JKSharedMemory_posix.cpp
    src/agent/JKAgentClient.cpp
    src/agent/JKAgentJson.cpp
    src/JKHangulManager.cpp
    src/JKResourceCache.cpp
    src/JKSoundManager.cpp
    src/client/JKClientSurface.cpp
    src/client/JKClientApplication.cpp
    src/terminal/JKVtParser.cpp
    src/terminal/JKTerminalGrid.cpp
    src/terminal/JKGlyphAtlas.cpp
    src/terminal/JKConPtyBridge.cpp
    src/apps/JKTerminalConfig.cpp
    src/server/JKClientConnection.cpp
    src/server/JKWindowServer.cpp
    src/server/JKCompositor.cpp
    src/server/JKCompositorOutput.cpp
    src/JKDataFile.cpp
    src/JKImageLoader.cpp
    src/JKJkxFile.cpp
    src/JKVectorFont.cpp
    src/script/JKScriptHost.cpp
    legacy/wancode/WANCODE.CPP
)

target_include_directories(jkcore PUBLIC
    include
    third_party/stb
    legacy/wancode
)

# Default font directory for the vector font demos (vendored in-repo).
# JK_SCRIPTS_DIR: the script apps' source tree (jkx-pack script branch).
target_compile_definitions(jkcore PUBLIC
    JKENGINE_FONT_DIR="${CMAKE_SOURCE_DIR}/assets/fonts"
    JK_SCRIPTS_DIR="${CMAKE_SOURCE_DIR}/scripts"
)

target_link_libraries(jkcore PUBLIC quickjs SDL2::SDL2 SDL2_mixer::SDL2_mixer)

if(WIN32)
    target_link_libraries(jkcore PUBLIC imm32)
endif()

# ---------------------------------------------------------------------------
# jkdesktop.exe: demo host + single-process apps + window server.
# Client game apps are NOT linked here — they load jkapp_<name>.dll at runtime.
# ---------------------------------------------------------------------------
add_executable(jkdesktop
    src/main.cpp
    src/jkdesktop.rc
    src/apps/JangoApp.cpp
    src/apps/JangoUI.cpp
    src/apps/Equip24App.cpp
    src/apps/EquipApp.cpp
    src/apps/InsaApp.cpp
    src/apps/OccApp.cpp
    src/apps/OccUI.cpp
    src/apps/PcxApp.cpp
    src/apps/VectorApp.cpp
    src/apps/IconEditApp.cpp
    src/apps/RecogApp.cpp
    src/apps/VectorFontApp.cpp
    src/apps/VectorPresApp.cpp
    src/apps/MineSweeperApp.cpp
    src/apps/AppLauncherItem.cpp
    src/apps/TetrisApp.cpp
    # terminal: single-process ConPTY mode. TerminalView is otherwise only in
    # jkapp_terminal; jkcore does not carry it, so the exe needs its own copy.
    src/apps/TerminalApp.cpp
    src/apps/TerminalView.cpp
)

target_link_libraries(jkdesktop PRIVATE jkcore)

# ---------------------------------------------------------------------------
# App module DLLs (Phase B). Each exports the C ABI from include/apps/JKAppModule.h
# and statically contains its game + client scaffolding on top of jkcore.
# ---------------------------------------------------------------------------
add_library(jkapp_minesweeper SHARED
    src/apps/JKAppModule_minesweeper.cpp
    src/apps/ClientMineSweeperApp.cpp
    src/apps/MineSweeperApp.cpp
    # MineSweeperApp (single-process launcher) opens Tetris windows and uses
    # launcher items, so the module needs their sources too.
    src/apps/AppLauncherItem.cpp
    src/apps/TetrisApp.cpp
)
target_compile_definitions(jkapp_minesweeper PRIVATE JKAPP_MODULE_BUILD)
target_link_libraries(jkapp_minesweeper PRIVATE jkcore)
# Windows DLL naming without the MinGW "lib" prefix — the host loads
# "jkapp_<name>.dll" and the .jkx container (Phase C) will use the same name.
set_target_properties(jkapp_minesweeper PROPERTIES PREFIX "")

add_library(jkapp_tetris SHARED
    src/apps/JKAppModule_tetris.cpp
    src/apps/ClientTetrisApp.cpp
    src/apps/TetrisApp.cpp
)
target_compile_definitions(jkapp_tetris PRIVATE JKAPP_MODULE_BUILD)
target_link_libraries(jkapp_tetris PRIVATE jkcore)
set_target_properties(jkapp_tetris PROPERTIES PREFIX "")

add_library(jkapp_testwin SHARED
    src/apps/JKAppModule_testwin.cpp
    src/apps/ClientTestWindowApp.cpp
)
target_compile_definitions(jkapp_testwin PRIVATE JKAPP_MODULE_BUILD)
target_link_libraries(jkapp_testwin PRIVATE jkcore)
set_target_properties(jkapp_testwin PROPERTIES PREFIX "")

# JANGO launcher module: reuses the shared JangoUI plus the ported WINDBASE
# dialogs (Equip24/Equip/Insa) that JangoUI opens.
add_library(jkapp_jango SHARED
    src/apps/JKAppModule_jango.cpp
    src/apps/ClientJangoApp.cpp
    src/apps/JangoUI.cpp
    src/apps/Equip24App.cpp
    src/apps/EquipApp.cpp
    src/apps/InsaApp.cpp
)
target_compile_definitions(jkapp_jango PRIVATE JKAPP_MODULE_BUILD)
target_link_libraries(jkapp_jango PRIVATE jkcore)
set_target_properties(jkapp_jango PROPERTIES PREFIX "")

# OCC (2CAOCC C2) module: data model + UI live in OccUI.cpp.
add_library(jkapp_occ SHARED
    src/apps/JKAppModule_occ.cpp
    src/apps/ClientOccApp.cpp
    src/apps/OccUI.cpp
)
target_compile_definitions(jkapp_occ PRIVATE JKAPP_MODULE_BUILD)
target_link_libraries(jkapp_occ PRIVATE jkcore)
set_target_properties(jkapp_occ PROPERTIES PREFIX "")

add_library(jkapp_pcx SHARED
    src/apps/JKAppModule_pcx.cpp
    src/apps/ClientPcxApp.cpp
    src/apps/PcxApp.cpp
)
target_compile_definitions(jkapp_pcx PRIVATE JKAPP_MODULE_BUILD)
target_link_libraries(jkapp_pcx PRIVATE jkcore)
set_target_properties(jkapp_pcx PROPERTIES PREFIX "")

add_library(jkapp_vector SHARED
    src/apps/JKAppModule_vector.cpp
    src/apps/ClientVectorApp.cpp
    src/apps/VectorApp.cpp
)
target_compile_definitions(jkapp_vector PRIVATE JKAPP_MODULE_BUILD)
target_link_libraries(jkapp_vector PRIVATE jkcore)
set_target_properties(jkapp_vector PROPERTIES PREFIX "")

add_library(jkapp_iconedit SHARED
    src/apps/JKAppModule_iconedit.cpp
    src/apps/ClientIconEditApp.cpp
    src/apps/IconEditApp.cpp
)
target_compile_definitions(jkapp_iconedit PRIVATE JKAPP_MODULE_BUILD)
target_link_libraries(jkapp_iconedit PRIVATE jkcore)
set_target_properties(jkapp_iconedit PROPERTIES PREFIX "")

add_library(jkapp_recog SHARED
    src/apps/JKAppModule_recog.cpp
    src/apps/ClientRecogApp.cpp
    src/apps/RecogApp.cpp
)
target_compile_definitions(jkapp_recog PRIVATE JKAPP_MODULE_BUILD)
target_link_libraries(jkapp_recog PRIVATE jkcore)
set_target_properties(jkapp_recog PROPERTIES PREFIX "")

add_library(jkapp_vfont SHARED
    src/apps/JKAppModule_vfont.cpp
    src/apps/ClientVectorFontApp.cpp
    src/apps/VectorFontApp.cpp
)
target_compile_definitions(jkapp_vfont PRIVATE JKAPP_MODULE_BUILD)
target_link_libraries(jkapp_vfont PRIVATE jkcore)
set_target_properties(jkapp_vfont PROPERTIES PREFIX "")

add_library(jkapp_vpres SHARED
    src/apps/JKAppModule_vpres.cpp
    src/apps/ClientVectorPresApp.cpp
    src/apps/VectorPresApp.cpp
)
target_compile_definitions(jkapp_vpres PRIVATE JKAPP_MODULE_BUILD)
target_link_libraries(jkapp_vpres PRIVATE jkcore)
set_target_properties(jkapp_vpres PROPERTIES PREFIX "")

# Terminal (ConPTY) module — docs/22. View + client app live in the module;
# parser/grid/atlas/bridge are in jkcore so the self-test can exercise them.
add_library(jkapp_terminal SHARED
    src/apps/JKAppModule_terminal.cpp
    src/apps/ClientTerminalApp.cpp
    src/apps/TerminalView.cpp
)
target_compile_definitions(jkapp_terminal PRIVATE JKAPP_MODULE_BUILD)
target_link_libraries(jkapp_terminal PRIVATE jkcore)
set_target_properties(jkapp_terminal PROPERTIES PREFIX "")

# Common script module (docs/27 단계 1) — every script .jkx shares this DLL;
# app.js is data extracted next to it by the client host (SCRPT TOC entry).
add_library(jkapp_script SHARED
    src/apps/JKAppModule_script.cpp
)
target_compile_definitions(jkapp_script PRIVATE JKAPP_MODULE_BUILD)
target_link_libraries(jkapp_script PRIVATE jkcore)
set_target_properties(jkapp_script PROPERTIES PREFIX "")

# Taskbar shell module (docs/28) — a privileged client, NOT a launcher app:
# never packaged as .jkx; the server auto-spawns it from jkapp_taskbar.dll.
add_library(jkapp_taskbar SHARED
    src/apps/JKAppModule_taskbar.cpp
    src/apps/ClientTaskbarApp.cpp
)
target_compile_definitions(jkapp_taskbar PRIVATE JKAPP_MODULE_BUILD)
target_link_libraries(jkapp_taskbar PRIVATE jkcore)
set_target_properties(jkapp_taskbar PROPERTIES PREFIX "")

# Dear ImGui demo module (docs/23 §9 Phase 1): the first ImGui app — links the
# vendored imgui target (kept out of jkcore on purpose, see the imgui block).
add_library(jkapp_imguidemo SHARED
    src/apps/JKAppModule_imguidemo.cpp
    src/apps/ClientImGuiDemoApp.cpp
)
target_compile_definitions(jkapp_imguidemo PRIVATE JKAPP_MODULE_BUILD)
target_link_libraries(jkapp_imguidemo PRIVATE jkcore imgui)
set_target_properties(jkapp_imguidemo PROPERTIES PREFIX "")

# Task manager module (docs/23 §11.2, ImGui Phase 2 showcase): shell protocol
# consumer — subscribes to WindowList, samples per-process stats itself.
add_library(jkapp_taskmgr SHARED
    src/apps/JKAppModule_taskmgr.cpp
    src/apps/ClientTaskmgrApp.cpp
)
target_compile_definitions(jkapp_taskmgr PRIVATE JKAPP_MODULE_BUILD)
target_link_libraries(jkapp_taskmgr PRIVATE jkcore imgui implot)
set_target_properties(jkapp_taskmgr PROPERTIES PREFIX "")

# Video player module (docs/23 §11.8, ImGui Phase 4 long-term track): FFmpeg
# demux/decode on a worker thread, decoded frames -> ImGui::Image via the
# fused backend's GetTexID SDL texture path, audio -> client-side SDL device.
add_library(jkapp_vplayer SHARED
    src/apps/JKAppModule_vplayer.cpp
    src/apps/ClientVPlayerApp.cpp
)
target_compile_definitions(jkapp_vplayer PRIVATE JKAPP_MODULE_BUILD)
target_link_libraries(jkapp_vplayer PRIVATE jkcore imgui PkgConfig::FFMPEG)
set_target_properties(jkapp_vplayer PROPERTIES PREFIX "")

# ---------------------------------------------------------------------------
# CEF OSR browser (docs/23 §11.9): jkapp_browser links libcef.dll directly
# (GNU ld -l:libcef.dll, no import lib / no libcef_dll_wrapper — proven by the
# tools/cefosr standalone demo), and cefosr.exe doubles as the CEF subprocess
# worker (browser_subprocess_path). The whole block is gated on the vendored
# binary distribution being fetched (third_party/cef/README.md).
# ---------------------------------------------------------------------------
if(EXISTS "${CMAKE_SOURCE_DIR}/third_party/cef/Release/libcef.dll")
    set(CEF_DIR "${CMAKE_SOURCE_DIR}/third_party/cef")

    # Standalone OSR demo + subprocess worker for jkapp_browser. Kept as a C
    # target so it stays a minimal, dependency-free re-entry point.
    add_executable(cefosr tools/cefosr/osr_main.c)
    target_compile_definitions(cefosr PRIVATE SDL_MAIN_HANDLED)
    target_include_directories(cefosr PRIVATE ${CEF_DIR})
    target_link_libraries(cefosr PRIVATE SDL2::SDL2
        "-L${CEF_DIR}/Release" "-l:libcef.dll")

    add_library(jkapp_browser SHARED
        src/apps/JKAppModule_browser.cpp
        src/apps/ClientBrowserApp.cpp
    )
    target_compile_definitions(jkapp_browser PRIVATE JKAPP_MODULE_BUILD)
    target_include_directories(jkapp_browser PRIVATE ${CEF_DIR})
    target_link_libraries(jkapp_browser PRIVATE jkcore imgui
        "-L${CEF_DIR}/Release" "-l:libcef.dll")
    set_target_properties(jkapp_browser PROPERTIES PREFIX "")
    target_link_options(jkapp_browser PRIVATE -static-libstdc++ -static-libgcc)

    # Ship the CEF runtime set (libcef.dll + chromium resources, ~563 MB) next
    # to jkdesktop.exe — clients spawn from that directory, and the browser
    # process resolves resources/locales relative to the main exe. Runs on
    # every build but is a no-op scan once copied.
    add_custom_target(cef_deploy ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
            "${CEF_DIR}/Release" "$<TARGET_FILE_DIR:jkdesktop>"
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
            "${CEF_DIR}/Resources" "$<TARGET_FILE_DIR:jkdesktop>"
        COMMENT "Deploying CEF runtime next to jkdesktop.exe"
        VERBATIM)

    # Demo page for the cefosr.exe default URL.
    add_custom_command(TARGET cefosr POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/cefosr/demo.html"
            "$<TARGET_FILE_DIR:cefosr>/demo.html"
    )
endif()

# ---------------------------------------------------------------------------
# jkagentd: Desktop Agent MCP broker (spec §2 sidecar). stdin MCP JSON-RPC →
# window-server pipe queries → stdout. Links jkcore for JKAgentClient (pipe)
# and JKAgentJson (quickjs) — the same stack the server side uses.
# ---------------------------------------------------------------------------
add_executable(jkagentd tools/jkagentd/main.cpp)
target_link_libraries(jkagentd PRIVATE jkcore)
if(WIN32)
    target_link_options(jkagentd PRIVATE -static-libstdc++ -static-libgcc)
endif()

if(WIN32)
    # Keep each module self-contained: no libstdc++/libgcc runtime DLLs needed
    # next to the app modules.
    foreach(jkapp_mod jkapp_minesweeper jkapp_tetris jkapp_testwin
                      jkapp_jango jkapp_occ jkapp_pcx jkapp_vector
                      jkapp_iconedit jkapp_recog jkapp_vfont jkapp_vpres
                      jkapp_terminal jkapp_script jkapp_taskbar
                      jkapp_imguidemo jkapp_taskmgr jkapp_vplayer)
        target_link_options(${jkapp_mod} PRIVATE -static-libstdc++ -static-libgcc)
    endforeach()
endif()

# Ship the assets/ tree next to the executable so SDL_GetBasePath()-relative
# asset paths work regardless of where the exe is launched from.
add_custom_command(TARGET jkdesktop POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:jkdesktop>/assets"
)

# Ship the MinGW/SDL runtime DLL closure next to the executable (relink only)
# so a fresh build tree runs without the MSYS bin dirs on PATH.
get_filename_component(JK_TOOLCHAIN_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)
find_program(JK_OBJDUMP objdump HINTS "${JK_TOOLCHAIN_BIN}")
if(JK_OBJDUMP)
    add_custom_command(TARGET jkdesktop POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -DTARGET_FILE="$<TARGET_FILE:jkdesktop>"
            -DDEST_DIR="$<TARGET_FILE_DIR:jkdesktop>"
            -DSEARCH_DIRS="${JK_TOOLCHAIN_BIN}"
            -DSEED_DLLS="SDL2.dll;SDL2_image.dll;SDL2_mixer.dll;avformat-63.dll;avcodec-63.dll;avutil-61.dll;swscale-10.dll;swresample-7.dll"
            -DOBJDUMP="${JK_OBJDUMP}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CopyRuntimeDlls.cmake"
    )
endif()

# ---------------------------------------------------------------------------
# .jkx auto-repack (Phase C). Client app code ships INSIDE apps/<app>.jkx
# (MODL entry), so a plain `cmake --build` is not enough after touching app
# sources — the containers must be regenerated. This custom target reruns
# `jkx-pack` whenever the host exe or a module DLL is newer than its package.
# Depends on target names (not files) so Ninja always links exe+DLLs first.
# ---------------------------------------------------------------------------
# Launcher icon art is embedded by jkx-pack at repack time. The packer reads
# assets/ next to the exe (build dir), where the tree is only refreshed by the
# jkdesktop POST_BUILD copy — i.e. never after an icons-only edit. Sync each
# icon into the build tree as an explicit output so icon edits chain into
# repacking (minesweeper uses the launcher_mine prefix).
set(JKX_ICON_APPS
    mine tetris testwin jango occ pcx vector iconedit recog vfont vpres
    terminal imguidemo taskmgr vplayer browser scriptdemo passworddemo)
set(JKX_ICONS "${CMAKE_BINARY_DIR}/assets/icons")
foreach(icon_app IN LISTS JKX_ICON_APPS)
    foreach(icon_px IN ITEMS 1x 2x)
        set(icon_src "${CMAKE_SOURCE_DIR}/assets/icons/launcher_${icon_app}@${icon_px}.png")
        set(icon_dst "${JKX_ICONS}/launcher_${icon_app}@${icon_px}.png")
        add_custom_command(OUTPUT "${icon_dst}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${icon_src}" "${icon_dst}"
            DEPENDS "${icon_src}"
            COMMENT "Syncing ${icon_dst}"
            VERBATIM)
    endforeach()
endforeach()
add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/apps/minesweeper.jkx"
    COMMAND "$<TARGET_FILE:jkdesktop>" jkx-pack minesweeper
    DEPENDS jkdesktop jkapp_minesweeper
        "${JKX_ICONS}/launcher_mine@1x.png"
        "${JKX_ICONS}/launcher_mine@2x.png"
    COMMENT "Repacking apps/minesweeper.jkx"
    VERBATIM)
add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/apps/tetris.jkx"
    COMMAND "$<TARGET_FILE:jkdesktop>" jkx-pack tetris
    DEPENDS jkdesktop jkapp_tetris
        "${JKX_ICONS}/launcher_tetris@1x.png"
        "${JKX_ICONS}/launcher_tetris@2x.png"
    COMMENT "Repacking apps/tetris.jkx"
    VERBATIM)
add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/apps/testwin.jkx"
    COMMAND "$<TARGET_FILE:jkdesktop>" jkx-pack testwin
    DEPENDS jkdesktop jkapp_testwin
        "${JKX_ICONS}/launcher_testwin@1x.png"
        "${JKX_ICONS}/launcher_testwin@2x.png"
    COMMENT "Repacking apps/testwin.jkx"
    VERBATIM)
add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/apps/jango.jkx"
    COMMAND "$<TARGET_FILE:jkdesktop>" jkx-pack jango
    DEPENDS jkdesktop jkapp_jango
        "${JKX_ICONS}/launcher_jango@1x.png"
        "${JKX_ICONS}/launcher_jango@2x.png"
    COMMENT "Repacking apps/jango.jkx"
    VERBATIM)
add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/apps/occ.jkx"
    COMMAND "$<TARGET_FILE:jkdesktop>" jkx-pack occ
    DEPENDS jkdesktop jkapp_occ
        "${JKX_ICONS}/launcher_occ@1x.png"
        "${JKX_ICONS}/launcher_occ@2x.png"
    COMMENT "Repacking apps/occ.jkx"
    VERBATIM)
add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/apps/pcx.jkx"
    COMMAND "$<TARGET_FILE:jkdesktop>" jkx-pack pcx
    DEPENDS jkdesktop jkapp_pcx
        "${JKX_ICONS}/launcher_pcx@1x.png"
        "${JKX_ICONS}/launcher_pcx@2x.png"
    COMMENT "Repacking apps/pcx.jkx"
    VERBATIM)
add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/apps/vector.jkx"
    COMMAND "$<TARGET_FILE:jkdesktop>" jkx-pack vector
    DEPENDS jkdesktop jkapp_vector
        "${JKX_ICONS}/launcher_vector@1x.png"
        "${JKX_ICONS}/launcher_vector@2x.png"
    COMMENT "Repacking apps/vector.jkx"
    VERBATIM)
add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/apps/iconedit.jkx"
    COMMAND "$<TARGET_FILE:jkdesktop>" jkx-pack iconedit
    DEPENDS jkdesktop jkapp_iconedit
        "${JKX_ICONS}/launcher_iconedit@1x.png"
        "${JKX_ICONS}/launcher_iconedit@2x.png"
    COMMENT "Repacking apps/iconedit.jkx"
    VERBATIM)
add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/apps/recog.jkx"
    COMMAND "$<TARGET_FILE:jkdesktop>" jkx-pack recog
    DEPENDS jkdesktop jkapp_recog
        "${JKX_ICONS}/launcher_recog@1x.png"
        "${JKX_ICONS}/launcher_recog@2x.png"
    COMMENT "Repacking apps/recog.jkx"
    VERBATIM)
add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/apps/vfont.jkx"
    COMMAND "$<TARGET_FILE:jkdesktop>" jkx-pack vfont
    DEPENDS jkdesktop jkapp_vfont
        "${JKX_ICONS}/launcher_vfont@1x.png"
        "${JKX_ICONS}/launcher_vfont@2x.png"
    COMMENT "Repacking apps/vfont.jkx"
    VERBATIM)
add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/apps/vpres.jkx"
    COMMAND "$<TARGET_FILE:jkdesktop>" jkx-pack vpres
    DEPENDS jkdesktop jkapp_vpres
        "${JKX_ICONS}/launcher_vpres@1x.png"
        "${JKX_ICONS}/launcher_vpres@2x.png"
    COMMENT "Repacking apps/vpres.jkx"
    VERBATIM)
add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/apps/terminal.jkx"
    COMMAND "$<TARGET_FILE:jkdesktop>" jkx-pack terminal
    DEPENDS jkdesktop jkapp_terminal
        "${JKX_ICONS}/launcher_terminal@1x.png"
        "${JKX_ICONS}/launcher_terminal@2x.png"
    COMMENT "Repacking apps/terminal.jkx"
    VERBATIM)
add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/apps/imguidemo.jkx"
    COMMAND "$<TARGET_FILE:jkdesktop>" jkx-pack imguidemo
    DEPENDS jkdesktop jkapp_imguidemo
        "${JKX_ICONS}/launcher_imguidemo@1x.png"
        "${JKX_ICONS}/launcher_imguidemo@2x.png"
    COMMENT "Repacking apps/imguidemo.jkx"
    VERBATIM)
add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/apps/taskmgr.jkx"
    COMMAND "$<TARGET_FILE:jkdesktop>" jkx-pack taskmgr
    DEPENDS jkdesktop jkapp_taskmgr
        "${JKX_ICONS}/launcher_taskmgr@1x.png"
        "${JKX_ICONS}/launcher_taskmgr@2x.png"
    COMMENT "Repacking apps/taskmgr.jkx"
    VERBATIM)
add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/apps/vplayer.jkx"
    COMMAND "$<TARGET_FILE:jkdesktop>" jkx-pack vplayer
    DEPENDS jkdesktop jkapp_vplayer
        "${JKX_ICONS}/launcher_vplayer@1x.png"
        "${JKX_ICONS}/launcher_vplayer@2x.png"
    COMMENT "Repacking apps/vplayer.jkx"
    VERBATIM)
# Browser needs libcef.dll next to jkdesktop.exe at pack time (the packer
# LoadLibraryA's the module), hence the cef_deploy dependency.
add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/apps/browser.jkx"
    COMMAND "$<TARGET_FILE:jkdesktop>" jkx-pack browser
    DEPENDS jkdesktop jkapp_browser cef_deploy
        "${JKX_ICONS}/launcher_browser@1x.png"
        "${JKX_ICONS}/launcher_browser@2x.png"
    COMMENT "Repacking apps/browser.jkx"
    VERBATIM)
# Script packages: the script source itself is a payload dependency so an
# app.js edit repacks without a rebuild.
add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/apps/scriptdemo.jkx"
    COMMAND "$<TARGET_FILE:jkdesktop>" jkx-pack scriptdemo
    DEPENDS jkdesktop jkapp_script
        "${CMAKE_SOURCE_DIR}/scripts/apps/scriptdemo/manifest.txt"
        "${CMAKE_SOURCE_DIR}/scripts/apps/scriptdemo/app.js"
        "${JKX_ICONS}/launcher_scriptdemo@1x.png"
        "${JKX_ICONS}/launcher_scriptdemo@2x.png"
    COMMENT "Repacking apps/scriptdemo.jkx"
    VERBATIM)
add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/apps/passworddemo.jkx"
    COMMAND "$<TARGET_FILE:jkdesktop>" jkx-pack passworddemo
    DEPENDS jkdesktop jkapp_script
        "${CMAKE_SOURCE_DIR}/scripts/apps/passworddemo/manifest.txt"
        "${CMAKE_SOURCE_DIR}/scripts/apps/passworddemo/app.js"
        "${JKX_ICONS}/launcher_passworddemo@1x.png"
        "${JKX_ICONS}/launcher_passworddemo@2x.png"
    COMMENT "Repacking apps/passworddemo.jkx"
    VERBATIM)
add_custom_target(jkx_packages ALL
    DEPENDS "${CMAKE_BINARY_DIR}/apps/minesweeper.jkx"
            "${CMAKE_BINARY_DIR}/apps/tetris.jkx"
            "${CMAKE_BINARY_DIR}/apps/testwin.jkx"
            "${CMAKE_BINARY_DIR}/apps/jango.jkx"
            "${CMAKE_BINARY_DIR}/apps/occ.jkx"
            "${CMAKE_BINARY_DIR}/apps/pcx.jkx"
            "${CMAKE_BINARY_DIR}/apps/vector.jkx"
            "${CMAKE_BINARY_DIR}/apps/iconedit.jkx"
            "${CMAKE_BINARY_DIR}/apps/recog.jkx"
            "${CMAKE_BINARY_DIR}/apps/vfont.jkx"
            "${CMAKE_BINARY_DIR}/apps/vpres.jkx"
            "${CMAKE_BINARY_DIR}/apps/terminal.jkx"
            "${CMAKE_BINARY_DIR}/apps/imguidemo.jkx"
            "${CMAKE_BINARY_DIR}/apps/taskmgr.jkx"
            "${CMAKE_BINARY_DIR}/apps/vplayer.jkx"
            "${CMAKE_BINARY_DIR}/apps/browser.jkx"
            "${CMAKE_BINARY_DIR}/apps/scriptdemo.jkx"
            "${CMAKE_BINARY_DIR}/apps/passworddemo.jkx")