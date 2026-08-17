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
# FFMPEG_PREFIX lets you build against a non-system FFmpeg (e.g. a Rockchip
# build with RKMPP hardware decode). When set, headers/libs are taken from
# $(FFMPEG_PREFIX)/usr/include/aarch64-linux-gnu and
# $(FFMPEG_PREFIX)/usr/lib/aarch64-linux-gnu. This matches the layout produced
# by `dpkg-deb -x <rockchip-ffmpeg>.deb $(FFMPEG_PREFIX)` (note the usr/ layer).
# Leave empty to use the system FFmpeg via pkg-config (software decode). At
# runtime the matching libs must be on LD_LIBRARY_PATH (e.g. the same prefix).
FFMPEG_PREFIX ?=
ifeq ($(FFMPEG_PREFIX),)
  FFMPEG_CFLAGS  =
  FFMPEG_LDFLAGS =
else
  FFMPEG_CFLAGS  = -I $(FFMPEG_PREFIX)/usr/include/aarch64-linux-gnu
  FFMPEG_LDFLAGS = -L $(FFMPEG_PREFIX)/usr/lib/aarch64-linux-gnu -Wl,-rpath,$(FFMPEG_PREFIX)/usr/lib/aarch64-linux-gnu
endif

CFLAGS    = -I $(LVGL_SRC) -I $(LVGL_INC) -I include -I . -DLV_CONF_INCLUDE_SIMPLE -O2 -Wall $(FFMPEG_CFLAGS)
CXXFLAGS  = -I $(LVGL_SRC) -I $(LVGL_INC) -I include -I . -DLV_CONF_INCLUDE_SIMPLE -O2 -Wall -std=c++17 $(FFMPEG_CFLAGS)

# FFMPEG_LDFLAGS must come BEFORE the pkg-config -l flags so the linker prefers
# the non-system FFmpeg; the rpath makes the matching libs load at runtime too.
LDFLAGS   = $(FFMPEG_LDFLAGS) $(shell pkg-config --cflags --libs libavformat libavcodec libavutil libswscale libswresample) -lm -lpthread -lasound -pthread -lrga

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
