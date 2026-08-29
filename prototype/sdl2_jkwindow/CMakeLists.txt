cmake_minimum_required(VERSION 3.16)
project(jkproto_sdl2_jkwindow LANGUAGES CXX)

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
    get_filename_component(JKENGINE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
endif()

add_executable(jkproto_sdl2_jkwindow
    src/main.cpp
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
    src/JKScrollBar.cpp
    src/JKListBox.cpp
    src/JKComboBox.cpp
    src/JKFileDialog.cpp
    src/JKMenu.cpp
    src/JKMessageBox.cpp
    src/JKDialog.cpp
    src/JKWindow.cpp
    src/JKApplication.cpp
    src/JKHangulManager.cpp
    src/JKResourceCache.cpp
    src/JKDataFile.cpp
    src/apps/JangoApp.cpp
    src/apps/Equip24App.cpp
    src/apps/EquipApp.cpp
    src/apps/InsaApp.cpp
    src/apps/OccApp.cpp
    "${JKENGINE_ROOT}/JKWINDOW/WANCODE.CPP"
)

target_include_directories(jkproto_sdl2_jkwindow PRIVATE
    include
    "${JKENGINE_ROOT}/JKWINDOW"
    "${JKENGINE_ROOT}"
)

if(TARGET SDL2::SDL2main)
    target_link_libraries(jkproto_sdl2_jkwindow PRIVATE SDL2::SDL2main)
endif()

target_link_libraries(jkproto_sdl2_jkwindow PRIVATE SDL2::SDL2)
