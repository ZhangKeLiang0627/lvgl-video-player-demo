cmake_minimum_required(VERSION 3.16)
project(lvgl_video_player C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

# ---- FFmpeg / ALSA / RGA ----------------------------------------------------
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED
    libavformat libavcodec libavutil libswscale libswresample)
find_library(ALSA_LIB asound REQUIRED)
# RGA 2D accelerator (Rockchip): NV12 -> RGB + scale offload used by lv_ffmpeg.c
find_path(RGA_INCLUDE_DIR rga/rga.h)
find_library(RGA_LIB rga REQUIRED)

# ---- Our application --------------------------------------------------------
file(GLOB APP_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
add_executable(lvgl-video-player ${APP_SOURCES})

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
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${FFMPEG_INCLUDE_DIRS}
    ${RGA_INCLUDE_DIR}
    # LVGL's own headers are reached via directory-scoped include_directories
    # that are NOT propagated to consumers of the `lvgl` target, so we must
    # add them explicitly. `include/lvgl` carries the public <lvgl.h>; `src`
    # carries the internal "debugging/...", "widgets/...", "libs/..." headers
    # that lvgl.h itself pulls in (and that App.cpp includes directly).
    ${CMAKE_CURRENT_SOURCE_DIR}/lvgl/include/lvgl
    ${CMAKE_CURRENT_SOURCE_DIR}/lvgl/src)
target_link_libraries(lvgl-video-player PRIVATE
    lvgl
    ${FFMPEG_LIBRARIES}
    ${ALSA_LIB}
    ${RGA_LIB}
    pthread)
target_compile_options(lvgl-video-player PRIVATE ${FFMPEG_CFLAGS_OTHER})
