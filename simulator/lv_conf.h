#ifndef LV_CONF_H
#define LV_CONF_H

/* Match the Waveshare panel's logical resolution and color depth. */
#define LV_COLOR_DEPTH 16
#define LV_USE_OS LV_OS_NONE

/* Use LVGL's official SDL2 display and input drivers. */
#define LV_USE_SDL 1
#define LV_SDL_INCLUDE_PATH <SDL.h>
#define LV_SDL_RENDER_MODE LV_DISPLAY_RENDER_MODE_DIRECT
#define LV_SDL_BUF_COUNT 1
#define LV_SDL_ACCELERATED 1
#define LV_SDL_FULLSCREEN 0
#define LV_SDL_DIRECT_EXIT 1
#define LV_SDL_MOUSEWHEEL_MODE LV_SDL_MOUSEWHEEL_MODE_ENCODER

/* The shared creature UI references these built-in fonts. */
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_18 1

#define LV_USE_LOG 0

#endif /* LV_CONF_H */
