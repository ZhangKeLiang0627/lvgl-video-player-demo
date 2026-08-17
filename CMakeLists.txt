cmake_minimum_required(VERSION 3.16)
project(lvgl_video_player C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Default to Release when no build type is given on the command line, so
# `cmake -S . -B build` alone gets -O3 (this is a performance-sensitive video
# player). Passing -DCMAKE_BUILD_TYPE=Debug still overrides it as usual.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    message(STATUS "CMAKE_BUILD_TYPE not set - defaulting to Release")
endif()

# ---- Build options ----------------------------------------------------------
# Desktop simulator (SDL backend, default window 1280x800):
#   cmake -S . -B build/sdl -DENABLE_SDL=ON
# Cross compile for aarch64 (e.g. Rockchip boards):
#   cmake -S . -B build/arm64 -DCMAKE_TOOLCHAIN_FILE=cmake/aarch64-linux-gnu.cmake
#   (keep ENABLE_SDL=OFF for the framebuffer device build)
option(ENABLE_SDL "Build with LVGL's SDL backend for desktop simulation" OFF)

# ---- FFmpeg: optional Rockchip prefix (RKMPP hardware decode) --------------
# Passing -DFFMPEG_PREFIX=/path/to/rockchip-ffmpeg prepends that prefix's
# pkg-config dir, so the h264_rkmpp-enabled libs are found below. The layout
# is the one produced by `dpkg-deb -x <rockchip-ffmpeg>.deb <prefix>`
# (note the usr/ layer). Omit for system FFmpeg (software decode).
if(FFMPEG_PREFIX)
    set(ENV{PKG_CONFIG_PATH}
        "${FFMPEG_PREFIX}/usr/lib/aarch64-linux-gnu/pkgconfig:$ENV{PKG_CONFIG_PATH}")
endif()

# ---- LVGL (as a git submodule, expected at ./lvgl) -------------------------
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/lvgl/CMakeLists.txt)
    # Apply the lv_ffmpeg patch (RGA 2D offload + pause / resume / seek /
    # get_time / get_duration on top of the stock "fire-and-forget" decoder)
    # exactly once, at configure time.
    find_program(GIT_EXECUTABLE git)
    set(LV_FFMPEG_PATCH ${CMAKE_CURRENT_SOURCE_DIR}/patches/lv_ffmpeg/lv_ffmpeg_v9.5.0.patch)
    execute_process(COMMAND ${GIT_EXECUTABLE} -C ${CMAKE_CURRENT_SOURCE_DIR}/lvgl apply --check ${LV_FFMPEG_PATCH}
                    OUTPUT_QUIET ERROR_QUIET RESULT_VARIABLE PATCH_CHK)
    if(PATCH_CHK EQUAL 0)
        execute_process(COMMAND ${GIT_EXECUTABLE} -C ${CMAKE_CURRENT_SOURCE_DIR}/lvgl apply ${LV_FFMPEG_PATCH}
                        RESULT_VARIABLE PATCH_RC)
        if(NOT PATCH_RC EQUAL 0)
            message(WARNING "lv_ffmpeg patch failed to apply (rc=${PATCH_RC})")
        else()
            message(STATUS "lv_ffmpeg patch applied")
        endif()
    else()
        message(STATUS "lv_ffmpeg patch already applied or not applicable (skipping)")
    endif()

    set(LV_CONF_PATH ${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h CACHE STRING "" FORCE)
    add_subdirectory(lvgl)
else()
    message(FATAL_ERROR "LVGL submodule not found. Run: git submodule update --init --recursive")
endif()

# ---- SDL2 for the desktop simulator backend --------------------------------
# LVGL's own sdl driver (lv_sdl_window) lives inside the lvgl library, so SDL2
# must be linked to the lvgl target, exactly like Keyboard-Guide does it. The
# driver is compiled in via LV_USE_SDL=1 (lv_conf.h keeps it OFF by default so
# device builds don't need SDL2 headers).
if(ENABLE_SDL)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
    target_link_libraries(lvgl PRIVATE PkgConfig::SDL2)
    target_compile_definitions(lvgl PRIVATE LV_USE_SDL=1)
    message(STATUS "SDL simulator backend enabled (default window 1280x800)")
endif()

# ---- FFmpeg / ALSA / RGA ----------------------------------------------------
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED
    libavformat libavcodec libavutil libswscale libswresample)
find_library(ALSA_LIB asound REQUIRED)
# RGA 2D accelerator (Rockchip): NV12 -> RGB + scale offload used by lv_ffmpeg.c.
# Optional — generic PCs (SDL simulator) don't ship librga, in which case we
# build without it and lv_ffmpeg.c falls back to pure CPU sws_scale
# (LV_FFMPEG_USE_RGA=0).
find_path(RGA_INCLUDE_DIR rga/rga.h)
find_library(RGA_LIB rga)
if(RGA_LIB AND RGA_INCLUDE_DIR)
    message(STATUS "librga found - RGA 2D hardware offload enabled")
else()
    message(STATUS "librga NOT found - building WITHOUT RGA (CPU sws_scale only)")
    set(RGA_LIB "")
    set(RGA_INCLUDE_DIR "")
    target_compile_definitions(lvgl PRIVATE LV_FFMPEG_USE_RGA=0)
endif()

# ---- Our application --------------------------------------------------------
# src/utils/*.cpp carries the lv_snapshot screen-capture tool.
file(GLOB APP_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/*.cpp)
add_executable(lvgl-video-player ${APP_SOURCES})

# Tell the app which backend to init at runtime: PLAYER_USE_SDL=1 -> SDL window,
# otherwise the Linux framebuffer (/dev/fb0). Same macro pattern as
# Keyboard-Guide's KEYBOARD_GUIDE_USE_SDL. LV_USE_SDL is also needed by the app
# because lv_sdl_window.h only exposes its API under #if LV_USE_SDL.
target_compile_definitions(lvgl-video-player PRIVATE
    PLAYER_USE_SDL=$<IF:$<BOOL:${ENABLE_SDL}>,1,0>
    $<$<BOOL:${ENABLE_SDL}>:LV_USE_SDL=1>)

# The Rockchip FFmpeg .pc files are dpkg-packed with prefix=/usr, so they
# contribute no -I/-L of their own. Add the prefix paths explicitly so
# headers AND libs (with h264_rkmpp) actually come from $FFMPEG_PREFIX.
if(FFMPEG_PREFIX)
    target_include_directories(lvgl-video-player PRIVATE
        "${FFMPEG_PREFIX}/usr/include/aarch64-linux-gnu")
    target_link_options(lvgl-video-player PRIVATE
        "-L${FFMPEG_PREFIX}/usr/lib/aarch64-linux-gnu"
        "-Wl,-rpath,${FFMPEG_PREFIX}/usr/lib/aarch64-linux-gnu")
endif()

target_include_directories(lvgl-video-player PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${FFMPEG_INCLUDE_DIRS}
    ${RGA_INCLUDE_DIR}
    # LVGL's own headers are reached via directory-scoped include_directories
    # that are NOT propagated to consumers of the `lvgl` target, so we must
    # add them explicitly. `include/lvgl` carries the public <lvgl.h>; `src`
    # carries the internal "debugging/...", "widgets/...", "libs/..." headers
    # that lvgl.h itself pulls in (and that app.cpp includes directly).
    ${CMAKE_CURRENT_SOURCE_DIR}/lvgl/include/lvgl
    ${CMAKE_CURRENT_SOURCE_DIR}/lvgl/src)
target_link_libraries(lvgl-video-player PRIVATE
    lvgl
    ${FFMPEG_LIBRARIES}
    ${ALSA_LIB}
    ${RGA_LIB}
    z          # PNG encoder for utils/lv_snapshot.cpp
    pthread)
target_compile_options(lvgl-video-player PRIVATE ${FFMPEG_CFLAGS_OTHER})
