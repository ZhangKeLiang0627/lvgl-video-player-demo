cmake_minimum_required(VERSION 3.16)
project(lvgl_video_player C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- LVGL (as a git submodule, expected at ./lvgl) -------------------------
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/lvgl/CMakeLists.txt)
    # Apply the lv_ffmpeg patch once, if not already applied.
    execute_process(
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/patches/lv_ffmpeg/patch_lvffmpeg_v3.py
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lvgl
        RESULT_VARIABLE PATCH_RC
    )
    if(NOT PATCH_RC EQUAL 0 AND NOT PATCH_RC EQUAL 0)
        message(WARNING "lv_ffmpeg patch step returned ${PATCH_RC} (may already be applied)")
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
    ${FFMPEG_INCLUDE_DIRS})
target_link_libraries(lvgl-video-player PRIVATE
    lvgl
    ${FFMPEG_LIBRARIES}
    ${ALSA_LIB}
    pthread)
target_compile_options(lvgl-video-player PRIVATE ${FFMPEG_CFLAGS_OTHER})
