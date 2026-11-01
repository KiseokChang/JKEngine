cmake_minimum_required(VERSION 3.22)
# #124 스파이크-B — docs/101 §5 시험판 (폐기 코드 — 라인 병입 금지).
# 질문: "방향 B(서버=SDLActivity 유일 화면 + 클라=nativeLibraryDir exec,
# SDL 비디오 없이 공유메모리 합성)가 이 폰(SM-F926N·Android 15)에서 되는가?"
#
# 이 CMakeLists는 엔진 최상위 CMakeLists를 전혀 포함하지 않는다 — 엔진 본판은
# pkg-config REQUIRED 3종(SDL2·SDL2_mixer·FFmpeg)이라 NDK 환경에서 configure가
# 죽는다(정찰 원존 — CMakeLists L24-53). 스파이크는 jkcore/jkserver/
# jkdesktop_shell/jkclient 소스 목록만 그대로 흡수한 별도 타깃 나무를 쓰고,
# SDL2는 벤더(add_subdirectory), SDL2_mixer는 음소거 심, iconv는 정직 스텁
 # (항상 실패 → JKTextConv fail-closed 룰링 그대로 작동 — CP949 leg 실측값이
#  이 스파이크의 결과물)로 대체한다.
project(spikeb LANGUAGES C CXX)

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
# SDL2_mixer 심 — 스파이크는 소리가 목표가 아니다. 엔진이 참조하는 Mix_*
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
    # 스파이크 흡수 갱신 — 병렬 음성 세션의 대화 세션 기록(TU 봉입,
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
# 스파이크 APK 상수 경로 — MainActivity가 assets를 files/로 추출해 둔다.
target_compile_definitions(jkcore PUBLIC
    JKENGINE_FONT_DIR="/data/data/chang.spikeb/files/fonts"
    JK_SCRIPTS_DIR="/data/data/chang.spikeb/files/scripts"
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

# ------------------------------------------------- 모듈 (lib*.so 패키징) --
# jniLibs 규약: nativeLibraryDir에 실리려면 lib 접두 필요 — 엔진 main.cpp의
# ClientModulePath이 __ANDROID__ 스파이크 leg에서 lib접두를 판정한다.
add_library(jkapp_minesweeper SHARED
    ${ENG}/src/apps/JKAppModule_minesweeper.cpp
    ${ENG}/src/apps/ClientMineSweeperApp.cpp
    ${ENG}/src/apps/MineSweeperApp.cpp
    ${ENG}/src/apps/AppLauncherItem.cpp
    ${ENG}/src/apps/TetrisApp.cpp
)
target_compile_definitions(jkapp_minesweeper PRIVATE JKAPP_MODULE_BUILD)
target_link_libraries(jkapp_minesweeper PRIVATE jkclient)
set_target_properties(jkapp_minesweeper PROPERTIES PREFIX "lib")

add_library(jkapp_taskbar SHARED
    ${ENG}/src/apps/JKAppModule_taskbar.cpp
    ${ENG}/src/apps/ClientTaskbarApp.cpp
)
target_compile_definitions(jkapp_taskbar PRIVATE JKAPP_MODULE_BUILD)
target_link_libraries(jkapp_taskbar PRIVATE jkclient)
set_target_properties(jkapp_taskbar PROPERTIES PREFIX "lib")