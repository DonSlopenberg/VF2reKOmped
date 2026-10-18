# vf2 - ReXGlue Recompiled Project
#
# SDK boilerplate lives in generated/rexglue.cmake.

cmake_minimum_required(VERSION 3.25)
project(vf2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(generated/rexglue.cmake)

# Reproducible, path-neutral binaries: no absolute build paths (and so no
# local user name) in __FILE__ strings, debug info or the PDB reference.
get_filename_component(VF2_REPO_ROOT "${CMAKE_SOURCE_DIR}/.." ABSOLUTE)
add_compile_options("-ffile-prefix-map=${VF2_REPO_ROOT}/=" "-ffile-prefix-map=${VF2_REPO_ROOT}=."
                    "-fdebug-compilation-dir=.")
add_link_options("LINKER:/PDBALTPATH:%_PDB%")

# Release versions shown in the launcher footer and stored in VERSIONINFO.
set(VF2_GAME_VERSION 1.3.3)
set(VF2_LAUNCHER_VERSION 1.3.3)
foreach(part GAME LAUNCHER)
    string(REPLACE "." ";" parts "${VF2_${part}_VERSION}")
    list(GET parts 0 VF2_${part}_VERSION_MAJOR)
    list(GET parts 1 VF2_${part}_VERSION_MINOR)
    list(GET parts 2 VF2_${part}_VERSION_PATCH)
endforeach()
configure_file(resources/version.h.in ${CMAKE_CURRENT_BINARY_DIR}/version/vf2_version.h @ONLY)

# Sources
set(VF2_SOURCES
    src/main.cpp
    src/vf2_video.cpp
    src/vf2_test_input.cpp
    src/vf2_keyboard.cpp
    src/vf2_logo.cpp
    src/vf2_fight.cpp
    src/vf2_fight_host.cpp
    src/vf2_i960.cpp
    src/vf2_demo.cpp
    src/vf2_interpolation.cpp
    src/vf2_arcade.cpp
)

list(APPEND VF2_SOURCES src/vf2_background_scroll.cpp src/vf2_panorama.cpp)

if(WIN32)
    add_executable(vf2 WIN32 ${VF2_SOURCES})
else()
    add_executable(vf2 ${VF2_SOURCES})
endif()

rexglue_setup_target(vf2 GPU_PLUGINS xenos)
target_compile_definitions(vf2 PRIVATE VF2_PANORAMA_RENDERER=1)
if(WIN32)
    enable_language(RC)
    target_sources(vf2 PRIVATE resources/engine.rc)
    target_include_directories(vf2 PRIVATE resources ${CMAKE_CURRENT_BINARY_DIR}/version)
    target_link_libraries(vf2 PRIVATE user32 xinput9_1_0 windowscodecs ole32)
endif()

if(WIN32)
    string(TIMESTAMP VF2_BUILD_STAMP "%Y.%m.%d %H:%M UTC" UTC)
    add_executable(vf2_launcher WIN32 src/launcher.cpp resources/vf2.rc)
    target_include_directories(vf2_launcher PRIVATE resources ${CMAKE_CURRENT_BINARY_DIR}/version)
    target_compile_definitions(vf2_launcher PRIVATE VF2_BUILD_STAMP="${VF2_BUILD_STAMP}"
        VF2_REXGLUE_VERSION="${REXGLUE_VERSION_STRING}")
    target_link_options(vf2_launcher PRIVATE "LINKER:/MANIFESTINPUT:${CMAKE_CURRENT_SOURCE_DIR}/resources/launcher.manifest")
    target_link_libraries(vf2_launcher PRIVATE user32 gdi32 xinput9_1_0 gdiplus ole32 dwmapi uxtheme comctl32 advapi32 version winhttp shell32)
endif()
