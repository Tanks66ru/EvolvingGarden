#define SDL_MAIN_HANDLED

#include <SDL.h>
#include <lvgl.h>
#include "src/drivers/sdl/lv_sdl_keyboard.h"
#include "src/drivers/sdl/lv_sdl_mouse.h"
#include "src/drivers/sdl/lv_sdl_mousewheel.h"
#include "src/drivers/sdl/lv_sdl_window.h"

#include "app/creature_app.hpp"
#include "platform/audio.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

constexpr std::int32_t DisplayWidth = 466;
constexpr std::int32_t DisplayHeight = 466;
constexpr std::uint32_t FramePeriodMs = 33;

std::uint64_t lastFrameTime = 0;

void updateFrame(lv_timer_t*) {
    const std::uint64_t now = SDL_GetTicks64();
    if (lastFrameTime == 0) {
        lastFrameTime = now;
        return;
    }

    const float deltaSeconds = static_cast<float>(now - lastFrameTime) / 1000.0F;
    lastFrameTime = now;
    evolvinggarden::app::update(deltaSeconds);
}

} // namespace

int main() {
    lv_init();

    lv_display_t* display = lv_sdl_window_create(DisplayWidth, DisplayHeight);
    if (display == nullptr) {
        std::fprintf(stderr, "Failed to create the LVGL SDL display: %s\n", SDL_GetError());
        return EXIT_FAILURE;
    }

    lv_sdl_window_set_title(display, "EvolvingGarden | ESP32 UI Simulator");
    lv_sdl_window_set_resizeable(display, false);
    lv_sdl_window_set_zoom(display, 1.40F);

    lv_indev_t* mouse = lv_sdl_mouse_create();
    if (mouse == nullptr
        || lv_sdl_mousewheel_create() == nullptr
        || lv_sdl_keyboard_create() == nullptr) {
        std::fprintf(stderr, "Failed to initialize an LVGL SDL input device.\n");
        return EXIT_FAILURE;
    }

    // Match the physical touchscreen gesture thresholds used by the firmware.
    lv_indev_set_gesture_min_distance(mouse, 42);
    lv_indev_set_gesture_min_velocity(mouse, 100);

    evolvinggarden::audio::initialize();

    evolvinggarden::app::initialize();
    lv_timer_create(updateFrame, FramePeriodMs, nullptr);

    std::puts("EvolvingGarden simulator is running.");
    std::puts("Click the creature to pet it. Drag left/right to use the swipe gestures.");

    for (;;) {
        std::uint32_t waitMs = lv_timer_handler();
        if (waitMs == LV_NO_TIMER_READY || waitMs > 5) {
            waitMs = 5;
        }
        lv_delay_ms(waitMs);
    }

    return EXIT_SUCCESS;
}
