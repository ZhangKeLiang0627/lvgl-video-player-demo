#pragma once

/*
 * Compile-time configuration for the LVGL Video Player.
 * Every value can be overridden from the command line / CMake, e.g.
 *     make CXXFLAGS="-DVIDEO_PATH=/mnt/sd/a.mp4 ..."
 */

#ifndef VIDEO_PATH
  #define VIDEO_PATH "/tmp/l1080_long.mp4"
#endif

#ifndef TOUCH_DEV
  #define TOUCH_DEV "/dev/input/event1"
#endif

#ifndef AUDIO_DEV
  #define AUDIO_DEV "plughw:0,0"
#endif

/* Playlist root: the file browser never navigates above this directory. */
#ifndef ROOT_DIR
  #define ROOT_DIR "/userdata"
#endif

/* Software volume gain range: slider 0..100 maps to 0.0 .. VOL_MAX_GAIN. */
#define VOL_MAX_GAIN 2.0f
#define VOL_DEFAULT 75

/* Default display rotation in degrees (0 / 90 / 180 / 270). Override at
 * runtime with `-r <deg>` or the LVGL_ROTATE environment variable. */
#ifndef DEFAULT_ROTATION
  #define DEFAULT_ROTATION 0
#endif
