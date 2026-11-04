cmake_minimum_required(VERSION 3.22)
# #126 jkdesktop-apk — 단일 APK 서버/클라 네이티브 빌드 (스펙 2026-11-01§2;
# 수형 원문 = docs/103 스파이크-B 폐곡 리포트).
#
# 이 CMakeLists는 엔진 최상위 CMakeLists를 포함하지 않는다 — 엔진 본판은

# pkg-config REQUIRED 3종(SDL2·SDL2_mixer·FFmpeg)이라 NDK 환경에서 configure가
# 죽는다(정찰 원존 — CMakeLists L24-53). #126는 jkcore/jkserver/
# jkdesktop_shell/jkclient 소스 목록만 그대로 흡수한 별도 타깃 나무를 쓰고,
# SDL2는 벤더(add_subdirectory), SDL2_mixer는 음소거 심, iconv는 정직 스텁
 # (항상 실패 → JKTextConv fail-closed 룰링 그대로 작동 — CP949 leg 실측값이
#  이 #126의 결과물)로 대체한다.
project(jkdeskapk LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# 엔진 루트 — gradle이 주는 cwd와 무관하게 이 파일 위치에서 6단 위(jni→main→
# src→app→spike-b→android→engine).
set(ENG "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../..")

# 벤더 SDL2 소스 — 조달기 승계: build/android-sdk/sdl2-src (무추적,
# install_android_sdk 계약과 동형 — 부재 시 fail-closed).
set(SDL2_SRC "$ENV{JK_SPIKE_SDL2_SRC}")
if(NOT SDL2_SRC)
    set(SDL2_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../../../build/android-sdk/sdl2-src")
endif()
if(NOT EXISTS "${SDL2_SRC}/CMakeLists.txt")
    message(FATAL_ERROR "벤더 SDL2 소스 부재: ${SDL2_SRC} — build/android-sdk/downloads/sdl2-2.32.10.tar.gz 풀어서 조달")
endif()

set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL2_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
set(SDL2_DISABLE_UNINSTALL ON CACHE BOOL "" FORCE)
add_subdirectory("${SDL2_SRC}" "sdl2-build")

# ---------------------------------------------------------------------------
# SDL2_mixer 심 — #126는 소리가 목표가 아니다. 엔진이 참조하는 Mix_*
# 표면(grep 실측 20종)만 음소거 no-op로 봉인해 링크를 살린다. 실제 오디오
# 금형은 본판 이식의 몫.
# ---------------------------------------------------------------------------
set(SPIKE_JNI "${CMAKE_CURRENT_SOURCE_DIR}")
add_library(mixer_shim STATIC "${SPIKE_JNI}/mixer_shim.c")
target_include_directories(mixer_shim PUBLIC "${SPIKE_JNI}/include")
target_link_libraries(mixer_shim PUBLIC SDL2::SDL2 ${log-lib} android)

# ---------------------------------------------------------------------------
# iconv 헤더(실물 bionic iconv — API 28 libc.so에 심볼 실측, 링커 원문 r5).
# 스텁 폐기 — 실물이 정직 진실원. CP949 호환 여부는 iconv_open 실패로 관측.
# ---------------------------------------------------------------------------
add_library(iconv_shim INTERFACE)
target_include_directories(iconv_shim INTERFACE "${SPIKE_JNI}/iconv_shim")

# ---------------------------------------------------------------------------
# vendored quickjs-ng (엔진 CMakeLists L57-72 동일 소스 목록)
# ---------------------------------------------------------------------------
set(QJS_DIR "${ENG}/third_party/quickjs-ng")
add_library(quickjs STATIC
    "${QJS_DIR}/quickjs.c"
    "${QJS_DIR}/dtoa.c"
    "${QJS_DIR}/libregexp.c"
    "${QJS_DIR}/libunicode.c"
)
target_include_directories(quickjs PUBLIC "${QJS_DIR}")
target_compile_definitions(quickjs PRIVATE _GNU_SOURCE QUICKJS_NG_BUILD)
set_target_properties(quickjs PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS ON)

find_library(log-lib log)

# ------------------------------------------------------------------ jkcore --
# 엔진 CMakeLists L138-278 소스 목록을 그대로 흡수(주석은 엔진 원존 참조).
add_library(jkcore STATIC
    ${ENG}/src/JKEvent.cpp
    ${ENG}/src/JKMessageQue.cpp
    ${ENG}/src/JKDC.cpp
    ${ENG}/src/JKSDLRenderBackend.cpp
    ${ENG}/src/JKOffscreenSurface.cpp
    ${ENG}/src/JKControl.cpp
    ${ENG}/src/JKStatic.cpp
    ${ENG}/src/JKButton.cpp
    ${ENG}/src/JKCheckBox.cpp
    ${ENG}/src/JOClock.cpp
    ${ENG}/src/JKEdit.cpp
    ${ENG}/src/JKHangulAutomata.cpp
    ${ENG}/src/JKHangulUtil.cpp
    ${ENG}/src/JKHanjaDict.cpp
    ${ENG}/src/JKPlatform_win32.cpp
    ${ENG}/src/JKImeHook_win32.cpp
    ${ENG}/src/JKCrashHandler_win32.cpp
    ${ENG}/src/JKScrollBar.cpp
    ${ENG}/src/JKListBox.cpp
    ${ENG}/src/JKComboBox.cpp
    ${ENG}/src/JKFileDialog.cpp
    ${ENG}/src/JKMenu.cpp
    ${ENG}/src/JKMessageBox.cpp
    ${ENG}/src/JKDialog.cpp
    ${ENG}/src/JKWindow.cpp
    ${ENG}/src/JKWindowManager.cpp
    ${ENG}/src/JKMessageBus.cpp
    ${ENG}/src/JKTimerThread.cpp
    ${ENG}/src/JKAudioThread.cpp
    ${ENG}/src/JKAudioCommand.cpp
    ${ENG}/src/JKSDLAudioBackend.cpp
    ${ENG}/src/JKRenderThread.cpp
    ${ENG}/src/JKRenderCommandList.cpp
    ${ENG}/src/JKApplication.cpp
    ${ENG}/src/crypto/JKSha256.cpp
    ${ENG}/src/ipc/JKWireProtocol.cpp
    ${ENG}/src/ipc/JKPipeTransport_win32.cpp
    ${ENG}/src/ipc/JKPipeTransport_posix.cpp
    ${ENG}/src/ipc/JKSharedMemory_win32.cpp
    ${ENG}/src/ipc/JKSharedMemory_posix.cpp
    ${ENG}/src/fs/JKFs_win32.cpp
    ${ENG}/src/fs/JKFs_posix.cpp
    ${ENG}/src/fs/JKInstanceLock_win32.cpp
    ${ENG}/src/fs/JKInstanceLock_posix.cpp
    ${ENG}/src/text/JKTextConv_win32.cpp
    ${ENG}/src/text/JKTextConv_posix.cpp
    ${ENG}/src/process/JKProcess_win32.cpp
    ${ENG}/src/process/JKProcess_posix.cpp
    ${ENG}/src/net/JKNet_win32.cpp
    ${ENG}/src/net/JKNet_posix.cpp
    ${ENG}/src/agent/JKAgentClient.cpp
    ${ENG}/src/agent/JKAgentJson.cpp
    ${ENG}/src/agent/JKLlmEngine.cpp
    ${ENG}/src/agent/JKLlmTurnHttp.cpp
    ${ENG}/src/voice/SttVoice.cpp
    ${ENG}/src/voice/VoiceCaptureSDL.cpp
    ${ENG}/src/voice/VoiceCaptureTermux.cpp
    ${ENG}/src/JKHangulManager.cpp
    ${ENG}/src/JKTextAtlas.cpp
    ${ENG}/src/JKResourceCache.cpp
    ${ENG}/src/JKSoundManager.cpp
    ${ENG}/src/terminal/JKConPtyBridge_win32.cpp
    ${ENG}/src/terminal/JKConPtyBridge_posix.cpp
    ${ENG}/src/theme/JKThemeConfig.cpp
    ${ENG}/src/JKDataFile.cpp
    ${ENG}/src/JKImageLoader.cpp
    ${ENG}/src/JKJkxFile.cpp
    ${ENG}/src/JKLibraryCatalog.cpp
    ${ENG}/src/apps/ChatRouter.cpp
    # #126 흡수 갱신 — 병렬 음성 세션의 대화 세션 기록(TU 봉입,
    # main.cpp selftest가 이 심볼을 만난다: r7 링크 실패 원문)
    ${ENG}/src/apps/ChatSessionLog.cpp
    ${ENG}/src/JKVectorFont.cpp
    ${ENG}/src/script/JKScriptHost.cpp
    ${ENG}/src/script/JKScriptCanvas.cpp
    ${ENG}/src/script/JKWorkshopStore.cpp
    ${ENG}/legacy/wancode/WANCODE.CPP
)
target_include_directories(jkcore PUBLIC
    ${ENG}/include
    ${ENG}/third_party/stb
    ${ENG}/legacy/wancode
)
# #126 APK 상수 경로 — MainActivity가 assets를 files/로 추출해 둔다.
target_compile_definitions(jkcore PUBLIC
    JKENGINE_FONT_DIR="/data/data/chang.jkdesktop/files/fonts"
    JK_SCRIPTS_DIR="/data/data/chang.jkdesktop/files/scripts"
)
target_link_libraries(jkcore PUBLIC quickjs SDL2::SDL2 mixer_shim iconv_shim log android)

# ---------------------------------------------------------------- jkserver --
add_library(jkserver STATIC
    ${ENG}/src/server/JKClientConnection.cpp
    ${ENG}/src/server/JKWindowServer.cpp
    ${ENG}/src/server/JKSemanticCursor.cpp
    ${ENG}/src/server/JKCompositor.cpp
    ${ENG}/src/server/JKCompositorOutput.cpp
    ${ENG}/src/server/JKFrameDirty.cpp
)
target_link_libraries(jkserver PUBLIC jkcore)

# --------------------------------------------------------- jkdesktop_shell --
add_library(jkdesktop_shell STATIC
    ${ENG}/src/desktop/JKDesktopShell.cpp
)
target_include_directories(jkdesktop_shell PUBLIC
    ${ENG}/include
    ${ENG}/third_party/stb
)
target_link_libraries(jkdesktop_shell PUBLIC jkcore)
target_link_libraries(jkserver PUBLIC jkdesktop_shell)

# ---------------------------------------------------------------- jkclient --
add_library(jkclient STATIC
    ${ENG}/src/client/JKClientSurface.cpp
    ${ENG}/src/client/JKClientApplication.cpp
    ${ENG}/src/terminal/JKVtParser.cpp
    ${ENG}/src/terminal/JKTerminalGrid.cpp
    ${ENG}/src/terminal/JKGlyphAtlas.cpp
    ${ENG}/src/apps/JKTerminalConfig.cpp
    ${ENG}/src/apps/JKWorkshopSeed.cpp
)
target_link_libraries(jkclient PUBLIC jkcore)

# ---------------------------------------------------- libjkdesk.so (서버 본체) --
# SDLActivity가 dlopen하는 SDLMain 진입물(jksrv_main.cpp — 엔진 main()을
# 호출). 서버용 — ET_DYN.
add_library(jkdesk SHARED
    "${SPIKE_JNI}/jksrv_main.cpp"
    ${ENG}/src/main.cpp
    ${ENG}/tools/jkweb/main.cpp   # in-process /talk
    ${SPIKE_JNI}/jkvoice_bridge.cpp   # 음성 브리지(스펙 §5) (JK_WEB_INPROCESS)
    ${ENG}/src/apps/JangoApp.cpp
    ${ENG}/src/apps/JangoUI.cpp
    ${ENG}/src/apps/Equip24App.cpp
    ${ENG}/src/apps/EquipApp.cpp
    ${ENG}/src/apps/InsaApp.cpp
    ${ENG}/src/apps/OccApp.cpp
    ${ENG}/src/apps/OccUI.cpp
    ${ENG}/src/apps/PcxApp.cpp
    ${ENG}/src/apps/VectorApp.cpp
    ${ENG}/src/apps/IconEditApp.cpp
    ${ENG}/src/apps/RecogApp.cpp
    ${ENG}/src/apps/VectorFontApp.cpp
    ${ENG}/src/apps/VectorPresApp.cpp
    ${ENG}/src/apps/MineSweeperApp.cpp
    ${ENG}/src/apps/AppLauncherItem.cpp
    ${ENG}/src/apps/TetrisApp.cpp
    ${ENG}/src/apps/TerminalApp.cpp
    ${ENG}/src/apps/TerminalView.cpp
    ${ENG}/src/apps/TerminalHangulInput.cpp
)
target_compile_definitions(jkdesk PRIVATE JK_WEB_INPROCESS)
target_link_libraries(jkdesk PRIVATE jkcore jkserver jkclient)

# --------------------------------------------------- libjkc.so (exec 전용) --
# r12 실측 원문: exec당 "Illegal instruction" — 공유 객체(ET_DYN, PT_INTERP
# 부재)는 그 자체로 exec 불가, 커널이 e_entry로 점프. W^X 우회의 정석 수형 =
# 진짜 PIE 실행파일을 lib*.so로 포장(jniLibs 규약 통과). 엔진 main.cpp를
# 직접 진입으로 하는 실행파일 — 클라 exec(--client/--jkx)의 물리.
add_executable(jkc
    ${ENG}/src/main.cpp
    ${ENG}/src/apps/JangoApp.cpp
    ${ENG}/src/apps/JangoUI.cpp
    ${ENG}/src/apps/Equip24App.cpp
    ${ENG}/src/apps/EquipApp.cpp
    ${ENG}/src/apps/InsaApp.cpp
    ${ENG}/src/apps/OccApp.cpp
    ${ENG}/src/apps/OccUI.cpp
    ${ENG}/src/apps/PcxApp.cpp
    ${ENG}/src/apps/VectorApp.cpp
    ${ENG}/src/apps/IconEditApp.cpp
    ${ENG}/src/apps/RecogApp.cpp
    ${ENG}/src/apps/VectorFontApp.cpp
    ${ENG}/src/apps/VectorPresApp.cpp
    ${ENG}/src/apps/MineSweeperApp.cpp
    ${ENG}/src/apps/AppLauncherItem.cpp
    ${ENG}/src/apps/TetrisApp.cpp
    ${ENG}/src/apps/TerminalApp.cpp
    ${ENG}/src/apps/TerminalView.cpp
    ${ENG}/src/apps/TerminalHangulInput.cpp
)
set_target_properties(jkc PROPERTIES
    OUTPUT_NAME "libjkc"   # jniLibs 패키징 규약(lib*.so만 surviving)
    SUFFIX ".so")
target_link_libraries(jkc PRIVATE jkcore jkserver jkclient)
# AGP는 공유 lib만 빌드 대상으로 만든다(r4 ninja 원문: SDL2 jkapp_* jkdesk) —
# jkdesk를 의존으로 묶어 gradle 빌드 사슬 안에 흡수하고, 산출물을 jniLibs 트리로
# POST_BUILD 복사(포장은 jniLibs/<abi>/lib*.so 정주 경로로 확립).
file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../jniLibs/arm64-v8a")
add_custom_command(TARGET jkc POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:jkc>
            "${CMAKE_CURRENT_SOURCE_DIR}/../jniLibs/arm64-v8a/libjkc.so")
add_dependencies(jkdesk jkc)

# ------------------------------------------------- imgui/implot (모듈 의존) --
set(IMGUI_DIR ${ENG}/third_party/imgui)
add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${ENG}/src/imgui_impl_jkwindow.cpp
)
target_include_directories(imgui PUBLIC
    ${IMGUI_DIR}
    ${ENG}/include
    ${ENG}/third_party/imgui_club
)
target_link_libraries(imgui PUBLIC SDL2::SDL2)

set(IMPLOT_DIR ${ENG}/third_party/implot)
add_library(implot STATIC
    ${IMPLOT_DIR}/implot.cpp
    ${IMPLOT_DIR}/implot_items.cpp
    ${IMPLOT_DIR}/implot_demo.cpp
)
target_include_directories(implot PUBLIC ${IMPLOT_DIR})
target_link_libraries(implot PUBLIC imgui)

# ------------------------------------------------- 모듈 (lib*.so 패키징) --
# 엔진 30 모듈 흡수(vplayer 제외 — 스펙 §6 v2). jniLibs 규약 lib 접두 —
# ClientModulePath lib 접두 판정(docs/103)과 짝.
add_library(jkapp_minesweeper SHARED
    ${ENG}/src/apps/JKAppModule_minesweeper.cpp
    ${ENG}/src/apps/ClientMineSweeperApp.cpp
    ${ENG}/src/apps/MineSweeperApp.cpp
    ${ENG}/src/apps/AppLauncherItem.cpp
    ${ENG}/src/apps/TetrisApp.cpp)
target_compile_definitions(jkapp_minesweeper PRIVATE JKAPP_MODULE_BUILD)
target_link_libraries(jkapp_minesweeper PRIVATE jkclient)
set_target_properties(jkapp_minesweeper PROPERTIES PREFIX "lib")
add_library(jkapp_tetris SHARED
    ${ENG}/src/apps/JKAppModule_tetris.cpp
    ${ENG}/src/apps/ClientTetrisApp.cpp
    ${ENG}/src/apps/TetrisApp.cpp)
target_compile_definitions(jkapp_tetris PRIVATE JKAPP_MODULE_BUILD)
target_link_libraries(jkapp_tetris PRIVATE jkclient)
set_target_properties(jkapp_tetris PROPERTIES PREFIX "lib")
add_library(jkapp_testwin SHARED
    ${ENG}/src/apps/JKAppModule_testwin.cpp
    ${ENG}/src/apps/ClientTestWindowApp.cpp)
target_compile_definitions(jkapp_testwin PRIVATE JKAPP_MODULE_BUILD)
target_link_libraries(jkapp_testwin PRIVATE jkclient)
set_target_properties(jkapp_testwin PROPERTIES PREFIX "lib")
add_library(jkapp_jango SHARED
    ${ENG}/src/apps/JKAppModule_jango.cpp
    ${ENG}/src/apps/ClientJangoApp.cpp
    ${ENG}/src/apps/JangoUI.cpp
    ${ENG}/src/apps/Equip24App.cpp
    ${ENG}/src/apps/EquipApp.cpp
    ${ENG}/src/apps/InsaApp.cpp)
target_compile_definitions(jkapp_jango PRIVATE JKAPP_MODULE_BUILD)
target_link_libraries(jkapp_jango PRIVATE jkclient)
set_target_properties(jkapp_jango PROPERTIES PREFIX "lib")
add_library(jkapp_occ SHARED
    ${ENG}/src/apps/JKAppModule_occ.cpp
    ${ENG}/src/apps/ClientOccApp.cpp
    ${ENG}/src/apps/OccUI.cpp)
target_compile_definitions(jkapp_occ PRIVATE JKAPP_MODULE_BUILD)
target_link_libraries(jkapp_occ PRIVATE jkclient)
set_target_properties(jkapp_occ PROPERTIES PREFIX "lib")
add_library(jkapp_pcx SHARED
    ${ENG}/src/apps/JKAppModule_pcx.cpp
    ${ENG}/src/apps/ClientPcxApp.cpp
    ${ENG}/src/apps/PcxApp.cpp)
target_compile_definitions(jkapp_pcx PRIVATE JKAPP_MODULE_BUILD)
target_link_libraries(jkapp_pcx PRIVATE jkclient)
set_target_properties(jkapp_pcx PROPERTIES PREFIX "lib")
add_library(jkapp_vector SHARED
    ${ENG}/src/apps/JKAppModule_vector.cpp
    ${ENG}/src/apps/ClientVectorApp.cpp
    ${ENG}/src/apps/VectorApp.cpp)
target_compile_definitions(jkapp_vector PRIVATE JKAPP_MODULE_BUILD)
target_link_libraries(jkapp_vector PRIVATE jkclient)
set_target_properties(jkapp_vector PROPERTIES PREFIX "lib")
add_library(jkapp_iconedit SHARED
    ${ENG}/src/apps/JKAppModule_iconedit.cpp
    ${ENG}/src/apps/ClientIconEditApp.cpp
    ${ENG}/src/apps/IconEditApp.cpp)
target_compile_definitions(jkapp_iconedit PRIVATE JKAPP_MODULE_BUILD)
target_link_libraries(jkapp_iconedit PRIVATE jkclient)
set_target_properties(jkapp_iconedit PROPERTIES PREFIX "lib")
add_library(jkapp_recog SHARED
    ${ENG}/src/apps/JKAppModule_recog.cpp
    ${ENG}/src/apps/ClientRecogApp.cpp
    ${ENG}/src/apps/RecogApp.cpp)
target_compile_definitions(jkapp_recog PRIVATE JKAPP_MODULE_BUILD)
target_link_libraries(jkapp_recog PRIVATE jkclient)
set_target_properties(jkapp_recog PROPERTIES PREFIX "lib")
add_library(jkapp_vfont SHARED
    ${ENG}/src/apps/JKAppModule_vfont.cpp
    ${ENG}/src/apps/ClientVectorFontApp.cpp
    ${ENG}/src/apps/VectorFontApp.cpp)
target_compile_definitions(jkapp_vfont PRIVATE JKAPP_MODULE_BUILD)
target_link_libraries(jkapp_vfont PRIVATE jkclient)
set_target_properties(jkapp_vfont PROPERTIES PREFIX "lib")
add_library(jkapp_vpres SHARED
    ${ENG}/src/apps/JKAppModule_vpres.cpp
    ${ENG}/src/apps/ClientVectorPresApp.cpp
    ${ENG}/src/apps/VectorPresApp.cpp)
target_compile_definitions(jkapp_vpres PRIVATE JKAPP_MODULE_BUILD)
target_link_libraries(jkapp_vpres PRIVATE jkclient)
set_target_properties(jkapp_vpres PROPERTIES PREFIX "lib")
add_library(jkapp_terminal SHARED
    ${ENG}/src/apps/JKAppModule_terminal.cpp
    ${ENG}/src/apps/ClientTerminalApp.cpp
    ${ENG}/src/apps/TerminalView.cpp
    ${ENG}/src/apps/TerminalHangulInput.cpp)
target_compile_definitions(jkapp_terminal PRIVATE JKAPP_MODULE_BUILD)
target_link_libraries(jkapp_terminal PRIVATE jkclient)
set_target_properties(jkapp_terminal PROPERTIES PREFIX "lib")
add_library(jkapp_script SHARED
    ${ENG}/src/apps/JKAppModule_script.cpp)
target_compile_definitions(jkapp_script PRIVATE JKAPP_MODULE_BUILD)
target_link_libraries(jkapp_script PRIVATE jkclient)
set_target_properties(jkapp_script PROPERTIES PREFIX "lib")
add_library(jkapp_taskbar SHARED
    ${ENG}/src/apps/JKAppModule_taskbar.cpp
    ${ENG}/src/apps/ClientTaskbarApp.cpp)
target_compile_definitions(jkapp_taskbar PRIVATE JKAPP_MODULE_BUILD)
target_link_libraries(jkapp_taskbar PRIVATE jkclient)
set_target_properties(jkapp_taskbar PROPERTIES PREFIX "lib")
add_library(jkapp_imguidemo SHARED
    ${ENG}/src/apps/JKAppModule_imguidemo.cpp
    ${ENG}/src/apps/ClientImGuiDemoApp.cpp)
target_compile_definitions(jkapp_imguidemo PRIVATE JKAPP_MODULE_BUILD)
target_link_libraries(jkapp_imguidemo PRIVATE jkclient imgui)
set_target_properties(jkapp_imguidemo PROPERTIES PREFIX "lib")
add_library(jkapp_taskmgr SHARED
    ${ENG}/src/apps/JKAppModule_taskmgr.cpp
    ${ENG}/src/apps/ClientTaskmgrApp.cpp)
target_compile_definitions(jkapp_taskmgr PRIVATE JKAPP_MODULE_BUILD)
target_link_libraries(jkapp_taskmgr PRIVATE jkclient imgui implot)
set_target_properties(jkapp_taskmgr PROPERTIES PREFIX "lib")
add_library(jkapp_palette SHARED
    ${ENG}/src/apps/JKAppModule_palette.cpp
    ${ENG}/src/apps/ClientPaletteApp.cpp)
target_compile_definitions(jkapp_palette PRIVATE JKAPP_MODULE_BUILD)
target_link_libraries(jkapp_palette PRIVATE jkclient imgui)
set_target_properties(jkapp_palette PROPERTIES PREFIX "lib")
add_library(jkapp_notify SHARED
    ${ENG}/src/apps/JKAppModule_notify.cpp
    ${ENG}/src/apps/ClientNotifyApp.cpp)
target_compile_definitions(jkapp_notify PRIVATE JKAPP_MODULE_BUILD)
target_link_libraries(jkapp_notify PRIVATE jkclient imgui)
set_target_properties(jkapp_notify PROPERTIES PREFIX "lib")
add_library(jkapp_agentmgr SHARED
    ${ENG}/src/apps/JKAppModule_agentmgr.cpp
    ${ENG}/src/apps/ClientAgentMgrApp.cpp)
target_compile_definitions(jkapp_agentmgr PRIVATE JKAPP_MODULE_BUILD)
target_link_libraries(jkapp_agentmgr PRIVATE jkclient imgui)
set_target_properties(jkapp_agentmgr PROPERTIES PREFIX "lib")
add_library(jkapp_settings SHARED
    ${ENG}/src/apps/JKAppModule_settings.cpp
    ${ENG}/src/apps/ClientSettingsApp.cpp)
target_compile_definitions(jkapp_settings PRIVATE JKAPP_MODULE_BUILD)
target_link_libraries(jkapp_settings PRIVATE jkclient imgui)
set_target_properties(jkapp_settings PROPERTIES PREFIX "lib")
add_library(jkapp_notes SHARED
    ${ENG}/src/apps/JKAppModule_notes.cpp
    ${ENG}/src/apps/ClientNotesApp.cpp)
target_compile_definitions(jkapp_notes PRIVATE JKAPP_MODULE_BUILD)
target_link_libraries(jkapp_notes PRIVATE jkclient imgui)
set_target_properties(jkapp_notes PROPERTIES PREFIX "lib")
add_library(jkapp_files SHARED
    ${ENG}/src/apps/JKAppModule_files.cpp
    ${ENG}/src/apps/ClientFilesApp.cpp)
target_compile_definitions(jkapp_files PRIVATE JKAPP_MODULE_BUILD)
target_link_libraries(jkapp_files PRIVATE jkclient imgui)
set_target_properties(jkapp_files PROPERTIES PREFIX "lib")
add_library(jkapp_library SHARED
    ${ENG}/src/apps/JKAppModule_library.cpp
    ${ENG}/src/apps/ClientLibraryApp.cpp)
target_compile_definitions(jkapp_library PRIVATE JKAPP_MODULE_BUILD)
target_link_libraries(jkapp_library PRIVATE jkclient imgui)
set_target_properties(jkapp_library PROPERTIES PREFIX "lib")
add_library(jkapp_chat SHARED
    ${ENG}/src/apps/JKAppModule_chat.cpp
    ${ENG}/src/apps/ClientChatApp.cpp)
target_compile_definitions(jkapp_chat PRIVATE JKAPP_MODULE_BUILD)
target_link_libraries(jkapp_chat PRIVATE jkclient imgui)
set_target_properties(jkapp_chat PROPERTIES PREFIX "lib")
add_library(jkapp_snap SHARED
    ${ENG}/src/apps/JKAppModule_snap.cpp
    ${ENG}/src/apps/ClientSnapApp.cpp)
target_compile_definitions(jkapp_snap PRIVATE JKAPP_MODULE_BUILD)
target_link_libraries(jkapp_snap PRIVATE jkclient imgui)
set_target_properties(jkapp_snap PROPERTIES PREFIX "lib")
add_library(jkapp_gallery SHARED
    ${ENG}/src/apps/JKAppModule_gallery.cpp
    ${ENG}/src/apps/ClientGalleryApp.cpp)
target_compile_definitions(jkapp_gallery PRIVATE JKAPP_MODULE_BUILD)
target_link_libraries(jkapp_gallery PRIVATE jkclient imgui)
set_target_properties(jkapp_gallery PROPERTIES PREFIX "lib")
# music 무조건 소스집합(엔진 원존 706-712 — SPATIAL_PLAYER_ROOT 조건부 leg는
# T8에서 audio_core+OpenAL 링크로 재판정 — 그때까지는 vplayer 위임 앱 동형).
# music unconditional set (engine 706-712 - conditional leg is T8)
add_library(jkapp_music SHARED
    ${ENG}/src/apps/JKAppModule_music.cpp
    ${ENG}/src/apps/ClientMusicApp.cpp)
target_compile_definitions(jkapp_music PRIVATE JKAPP_MODULE_BUILD)
target_link_libraries(jkapp_music PRIVATE jkclient imgui)
set_target_properties(jkapp_music PROPERTIES PREFIX "lib")
add_library(jkapp_shot SHARED
    ${ENG}/src/apps/JKAppModule_shot.cpp
    ${ENG}/src/apps/ClientShotApp.cpp)
target_compile_definitions(jkapp_shot PRIVATE JKAPP_MODULE_BUILD)
target_link_libraries(jkapp_shot PRIVATE jkclient imgui)
set_target_properties(jkapp_shot PROPERTIES PREFIX "lib")
# filedlg (engine unconditioned set) - browser=CEF(Win axis) excluded, see docs/100.
add_library(jkapp_filedlg SHARED
    ${ENG}/src/apps/JKAppModule_filedlg.cpp
    ${ENG}/src/apps/ClientFileDialogApp.cpp)
target_compile_definitions(jkapp_filedlg PRIVATE JKAPP_MODULE_BUILD)
target_link_libraries(jkapp_filedlg PRIVATE jkclient imgui)
set_target_properties(jkapp_filedlg PROPERTIES PREFIX "lib")
