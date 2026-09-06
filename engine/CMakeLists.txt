cmake_minimum_required(VERSION 3.16)
project(jkproto_sdl2_jkwindow LANGUAGES CXX RC)

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

# The prototype is nested under JKENGINE/prototype/sdl2_jkwindow, but
# build_with_temp.sh copies it to a temp tree on C:. Allow the build script
# (or any out-of-tree configure) to point back at the real JKENGINE root so
# we can share the legacy WANCODE conversion table.
if(NOT DEFINED JKENGINE_ROOT)
    # Source tree is JKENGINE/prototype/sdl2_jkwindow; the JKENGINE root is two levels up.
    get_filename_component(JKENGINE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
endif()

add_executable(jkproto_sdl2_jkwindow
    src/main.cpp
    src/jkproto.rc
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
    src/JKHangulManager.cpp
    src/JKResourceCache.cpp
    src/JKSoundManager.cpp
    src/client/JKClientSurface.cpp
    src/client/JKClientApplication.cpp
    src/server/JKClientConnection.cpp
    src/server/JKWindowServer.cpp
    src/server/JKCompositor.cpp
    src/server/JKCompositorOutput.cpp
    src/JKDataFile.cpp
    src/JKVectorFont.cpp
    src/apps/JangoApp.cpp
    src/apps/Equip24App.cpp
    src/apps/EquipApp.cpp
    src/apps/InsaApp.cpp
    src/apps/OccApp.cpp
    src/apps/PcxApp.cpp
    src/apps/VectorApp.cpp
    src/apps/IconEditApp.cpp
    src/apps/RecogApp.cpp
    src/apps/VectorFontApp.cpp
    src/apps/VectorPresApp.cpp
    src/apps/MineSweeperApp.cpp
    src/apps/AppLauncherItem.cpp
    src/apps/TetrisApp.cpp
    src/apps/ClientMineSweeperApp.cpp
    src/apps/ClientTetrisApp.cpp
    "${JKENGINE_ROOT}/JKWINDOW/WANCODE.CPP"
)

target_include_directories(jkproto_sdl2_jkwindow PRIVATE
    include
    "${JKENGINE_ROOT}/JKWINDOW"
    "${JKENGINE_ROOT}"
)

# Default font directory for the vector font demos.
target_compile_definitions(jkproto_sdl2_jkwindow PRIVATE
    JKENGINE_FONT_DIR="${JKENGINE_ROOT}/WINDBASE/2CAOCC"
)

# Use a plain main() entry point instead of SDL2main's WinMain wrapper.
# SDL2main caused silent launch failures on MinGW/Windows in this project.
# SDL itself is initialized explicitly with SDL_Init in JKApplication.
target_link_libraries(jkproto_sdl2_jkwindow PRIVATE SDL2::SDL2)

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

target_link_libraries(jkproto_sdl2_jkwindow PRIVATE SDL2_mixer::SDL2_mixer)

if(WIN32)
    target_link_libraries(jkproto_sdl2_jkwindow PRIVATE imm32)
endif()
