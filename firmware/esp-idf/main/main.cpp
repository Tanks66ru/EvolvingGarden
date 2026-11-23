#include "app/creature_app.hpp"
#include "bsp/esp-bsp.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_lv_adapter.h"
#include "esp_log.h"
#include "lvgl.h"
#include "platform/audio.hpp"
#include "platform/motion.hpp"

#include <cstdint>

namespace {

constexpr char Tag[] = "EvolvingGarden";
constexpr float FrameStepSeconds = 1.0F / 30.0F;
int activeBrightness = 100;
bool displaySleeping = false;
constexpr gpio_num_t AdminButtonPin = GPIO_NUM_0; // BOOT, active low
bool adminButtonReady = false;
i2c_master_dev_handle_t pmuHandle = nullptr;
bool pmuButtonReady = false;
float pmuPollSeconds = 0.0F;
float fpsLogSeconds = 0.0F;
lv_display_t* activeDisplay = nullptr;
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
        if (buttonStablePressed) {
            ESP_LOGI(Tag, "BOOT button pressed");
            evolvinggarden::app::toggleDebugPanel();
        }
    }
}

bool readPmuRegister(std::uint8_t reg, std::uint8_t& value) {
    if (!pmuButtonReady || pmuHandle == nullptr) return false;
    return i2c_master_transmit_receive(pmuHandle, &reg, 1, &value, 1, 10) == ESP_OK;
}

bool writePmuRegister(std::uint8_t reg, std::uint8_t value) {
    if (!pmuButtonReady || pmuHandle == nullptr) return false;
    const std::uint8_t data[2] = {reg, value};
    return i2c_master_transmit(pmuHandle, data, sizeof(data), 10) == ESP_OK;
}

void configurePowerButton() {
    const auto bus = bsp_i2c_get_handle();
    if (bus == nullptr) {
        ESP_LOGE(Tag, "PMIC button unavailable: BSP I2C bus is null");
        return;
    }

    i2c_device_config_t config{};
    config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    config.device_address = 0x34; // AXP2101 power-management IC
    config.scl_speed_hz = 400000;

    const esp_err_t result = i2c_master_bus_add_device(bus, &config, &pmuHandle);
    if (result != ESP_OK) {
        ESP_LOGE(Tag, "PMIC device setup failed: %s", esp_err_to_name(result));
        return;
    }
    pmuButtonReady = true;

    std::uint8_t irqEnable = 0;
    if (!readPmuRegister(0x41, irqEnable)
        || !writePmuRegister(0x41, static_cast<std::uint8_t>(irqEnable | 0x08))
        || !writePmuRegister(0x49, 0xFF)) {
        ESP_LOGE(Tag, "PMIC power-button interrupt setup failed");
        pmuButtonReady = false;
        return;
    }
    ESP_LOGI(Tag, "PWR button ready (AXP2101)");
}

void pollPowerButton(float dt) {
    if (!pmuButtonReady) return;
    pmuPollSeconds += dt;
    if (pmuPollSeconds < 0.10F) return;
    pmuPollSeconds = 0.0F;

    std::uint8_t status = 0;
    if (!readPmuRegister(0x49, status) || (status & 0x08) == 0) return;
    (void)writePmuRegister(0x49, 0x08);
    ESP_LOGI(Tag, "PWR button pressed");
    evolvinggarden::app::toggleMenuPanel();
}

void onFrame(lv_timer_t*) {
    pollAdminButton();
    pollPowerButton(FrameStepSeconds);
    evolvinggarden::motion::update(FrameStepSeconds);

    const auto sensor = evolvinggarden::motion::state();
    if (sensor.screenSleeping != displaySleeping) {
        displaySleeping = sensor.screenSleeping;
        (void)bsp_display_brightness_set(displaySleeping ? 0 : activeBrightness);
    }

    evolvinggarden::app::update(FrameStepSeconds);

    fpsLogSeconds += FrameStepSeconds;
    if (activeDisplay != nullptr && fpsLogSeconds >= 2.0F) {
        fpsLogSeconds = 0.0F;
        std::uint32_t fps = 0;
        if (esp_lv_adapter_get_fps(activeDisplay, &fps) == ESP_OK) {
            ESP_LOGI(Tag, "Display refresh: %lu FPS", static_cast<unsigned long>(fps));
        }
    }
}

} // namespace

extern "C" void app_main(void) {
    ESP_LOGI(Tag, "Starting EvolvingGarden on ESP32-S3-Touch-AMOLED-1.75C");
    activeDisplay = bsp_display_start();
    if (activeDisplay == nullptr) {
        ESP_LOGE(Tag, "Display initialization failed");
        return;
    }
    ESP_ERROR_CHECK(esp_lv_adapter_fps_stats_enable(activeDisplay, true));

    activeBrightness = bsp_display_brightness_get();
    if (activeBrightness < 1) activeBrightness = 100;

    configureAdminButton();
    configurePowerButton();
    evolvinggarden::motion::initialize();
    evolvinggarden::audio::initialize();

    bsp_display_lock(-1);
    evolvinggarden::app::initialize();
    lv_timer_create(onFrame, 33, nullptr);
    bsp_display_unlock();

    ESP_LOGI(Tag, "Ready. PWR opens Garden menu; BOOT opens LAB; tap to hit; circle to pet.");
}
