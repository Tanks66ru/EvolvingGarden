#include "app/creature_app.hpp"
#include "bsp/esp-bsp.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "lvgl.h"
#include "platform/audio.hpp"
#include "platform/motion.hpp"

namespace {

constexpr char Tag[] = "EvolvingGarden";
constexpr float FrameStepSeconds = 1.0F / 30.0F;
int activeBrightness = 100;
bool displaySleeping = false;
constexpr gpio_num_t AdminButtonPin = GPIO_NUM_0; // BOOT, active low
bool adminButtonReady = false;
bool buttonRawCandidate = false;
bool buttonStablePressed = false;
float buttonDebounceSeconds = 0.0F;

void configureAdminButton() {
    gpio_config_t config{};
    config.pin_bit_mask = 1ULL << static_cast<unsigned>(AdminButtonPin);
    config.mode = GPIO_MODE_INPUT;
    config.pull_up_en = GPIO_PULLUP_ENABLE;
    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config.intr_type = GPIO_INTR_DISABLE;
    const esp_err_t result = gpio_config(&config);
    adminButtonReady = result == ESP_OK;
    if (!adminButtonReady) {
        ESP_LOGE(Tag, "BOOT button setup failed: %s", esp_err_to_name(result));
    }
}

void pollAdminButton() {
    if (!adminButtonReady) return;
    const bool pressed = gpio_get_level(AdminButtonPin) == 0;
    if (pressed != buttonRawCandidate) {
        buttonRawCandidate = pressed;
        buttonDebounceSeconds = 0.0F;
        return;
    }
    if (buttonDebounceSeconds < 0.06F) {
        buttonDebounceSeconds += FrameStepSeconds;
        return;
    }
    if (buttonStablePressed != buttonRawCandidate) {
        buttonStablePressed = buttonRawCandidate;
        if (buttonStablePressed) evolvinggarden::app::toggleDebugPanel();
    }
}

void onFrame(lv_timer_t*) {
    pollAdminButton();
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

    configureAdminButton();
    evolvinggarden::motion::initialize();
    evolvinggarden::audio::initialize();

    bsp_display_lock(-1);
    evolvinggarden::app::initialize();
    lv_timer_create(onFrame, 33, nullptr);
    bsp_display_unlock();

    ESP_LOGI(Tag, "Ready. FEED button; tap to spar; circle to pet; BOOT toggles diagnostics.");
}
