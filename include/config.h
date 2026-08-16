#pragma once

/*
 * Compile-time configuration for the LVGL Video Player.
 * Every value can be overridden from the command line / CMake, e.g.
 *     make CXXFLAGS="-DVIDEO_PATH=/mnt/sd/a.mp4 ..."
 */

#ifndef VIDEO_PATH
  #define VIDEO_PATH "/userdata/my_test.mp4"
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
