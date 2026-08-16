# ============================================================================
#  On-device build (tested on RK3566 / LubanCat, Ubuntu 20.04, aarch64)
#
#  Expects a local LVGL 9.x source tree as a SIBLING directory: ../lvgl
#  (i.e. /home/cat/lvgl_video_player/ next to /home/cat/lvgl/).
#
#  The lv_ffmpeg.c patch MUST be applied first, see patches/lv_ffmpeg/:
#      git apply -p1 patches/lv_ffmpeg/lv_ffmpeg_v9.5.0.patch   (run inside ../lvgl)
#
#  Build & run:
#      make
#      ./demo
# ============================================================================

CC       ?= gcc
CXX      ?= g++
LVGL_SRC  = ../lvgl/src
LVGL_INC  = ../lvgl/include/lvgl

# C sources = all of LVGL (compiled by gcc). C++ = our modules (compiled by g++).
# -DLV_CONF_INCLUDE_SIMPLE + -I . lets LVGL find our lv_conf.h next to the tree.
# Both LVGL_SRC (internal "debugging/...", "libs/..." headers) and LVGL_INC
# (the public <lvgl.h>) must be on the include path: LVGL's own headers are
# reached via directory-scoped include_directories that are NOT propagated to
# consumers of the `lvgl` target.
CFLAGS    = -I $(LVGL_SRC) -I $(LVGL_INC) -I include -I . -DLV_CONF_INCLUDE_SIMPLE -O2 -Wall
CXXFLAGS  = -I $(LVGL_SRC) -I $(LVGL_INC) -I include -I . -DLV_CONF_INCLUDE_SIMPLE -O2 -Wall -std=c++17

LDFLAGS   = $(shell pkg-config --cflags --libs libavformat libavcodec libavutil libswscale libswresample) -lm -lpthread -lasound -pthread

SRCS_C    = $(shell find $(LVGL_SRC) -name '*.c')
SRCS_CPP  = $(wildcard src/*.cpp)
OBJS_C    = $(SRCS_C:.c=.o)
OBJS_CPP  = $(SRCS_CPP:.cpp=.o)
OBJS      = $(OBJS_C) $(OBJS_CPP)

demo: $(OBJS)
	$(CXX) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) demo

.PHONY: clean
