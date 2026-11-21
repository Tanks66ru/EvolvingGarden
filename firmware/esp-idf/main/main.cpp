#include "app/creature_app.hpp"
#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "lvgl.h"
#include "platform/audio.hpp"
#include "platform/motion.hpp"

namespace {

constexpr char Tag[] = "EvolvingGarden";
constexpr float FrameStepSeconds = 1.0F / 30.0F;
int activeBrightness = 100;
bool displaySleeping = false;

void onFrame(lv_timer_t*) {
    evolvinggarden::motion::update(FrameStepSeconds);

    const auto sensor = evolvinggarden::motion::state();
    if (sensor.screenSleeping != displaySleeping) {
        displaySleeping = sensor.screenSleeping;
        (void)bsp_display_brightness_set(displaySleeping ? 0 : activeBrightness);
    }

    evolvinggarden::app::update(FrameStepSeconds);
}

} // namespace

extern "C" void app_main(void) {
    ESP_LOGI(Tag, "Starting EvolvingGarden on ESP32-S3-Touch-AMOLED-1.75C");
    if (bsp_display_start() == nullptr) {
        ESP_LOGE(Tag, "Display initialization failed");
        return;
    }

    activeBrightness = bsp_display_brightness_get();
    if (activeBrightness < 1) activeBrightness = 100;

    evolvinggarden::motion::initialize();
    evolvinggarden::audio::initialize();

    bsp_display_lock(-1);
    evolvinggarden::app::initialize();
    lv_timer_create(onFrame, 33, nullptr);
    bsp_display_unlock();

    ESP_LOGI(Tag, "Ready. FEED button; tap creature to spar; circle around it to pet; LAB opens diagnostics.");
}
