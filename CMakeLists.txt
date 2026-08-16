cmake_minimum_required(VERSION 3.16)
project(lvgl_video_player C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- LVGL (as a git submodule, expected at ./lvgl) -------------------------
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/lvgl/CMakeLists.txt)
    # Apply the lv_ffmpeg patch (adds pause / resume / seek / get_time /
    # get_duration on top of the stock "fire-and-forget" decoder) once.
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

# ---- FFmpeg / ALSA ----------------------------------------------------------
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED
    libavformat libavcodec libavutil libswscale libswresample)
find_library(ALSA_LIB asound REQUIRED)

# ---- Our application --------------------------------------------------------
file(GLOB APP_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
add_executable(lvgl-video-player ${APP_SOURCES})
target_include_directories(lvgl-video-player PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${FFMPEG_INCLUDE_DIRS}
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
    pthread)
target_compile_options(lvgl-video-player PRIVATE ${FFMPEG_CFLAGS_OTHER})
