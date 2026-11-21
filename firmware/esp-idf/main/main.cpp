#include "app/creature_app.hpp"
#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "lvgl.h"
#include "platform/audio.hpp"

namespace {

constexpr char Tag[] = "EvolvingGarden";
constexpr float FrameStepSeconds = 1.0F / 30.0F;

void onFrame(lv_timer_t*) {
    evolvinggarden::app::update(FrameStepSeconds);
}

} // namespace

extern "C" void app_main(void) {
    ESP_LOGI(Tag, "Starting EvolvingGarden on ESP32-S3-Touch-AMOLED-1.75C");
    if (bsp_display_start() == nullptr) {
        ESP_LOGE(Tag, "Display initialization failed");
        return;
    }

    evolvinggarden::audio::initialize();

    bsp_display_lock(-1);
    if (lv_indev_t* indev = bsp_display_get_input_dev()) {
        lv_indev_set_gesture_min_distance(indev, 42);
        lv_indev_set_gesture_min_velocity(indev, 100);
    }

    evolvinggarden::app::initialize();
    lv_timer_create(onFrame, 33, nullptr);
    bsp_display_unlock();

    ESP_LOGI(Tag, "Ready. Tap the action button; swipe right to cycle actions, left for debug.");
}
