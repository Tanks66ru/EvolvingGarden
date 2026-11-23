#include "app/creature_app.hpp"
#include "bsp/esp-bsp.h"
#include "bsp/display.h"
#include "bsp/touch.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_lcd_co5300.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lv_adapter.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "platform/audio.hpp"
#include "platform/motion.hpp"

#include <algorithm>
#include <cstdint>

namespace {

constexpr char Tag[] = "EvolvingGarden";
constexpr float FrameStepSeconds = 1.0F / 30.0F;
int activeBrightness = 100;
bool displaySleeping = false;
constexpr gpio_num_t AdminButtonPin = GPIO_NUM_0; // BOOT, active low
bool adminButtonReady = false;
i2c_master_dev_handle_t pmuHandle = nullptr;
bool pmuDeviceReady = false;
bool pmuButtonReady = false;
float pmuPollSeconds = 0.0F;
float batteryPollSeconds = 0.0F;
float fpsLogSeconds = 0.0F;
lv_display_t* activeDisplay = nullptr;
esp_lcd_panel_handle_t displayPanel = nullptr;
esp_lcd_panel_io_handle_t displayPanelIo = nullptr;
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
    if (!pmuDeviceReady || pmuHandle == nullptr) return false;
    return i2c_master_transmit_receive(pmuHandle, &reg, 1, &value, 1, 10) == ESP_OK;
}

bool writePmuRegister(std::uint8_t reg, std::uint8_t value) {
    if (!pmuDeviceReady || pmuHandle == nullptr) return false;
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
    pmuDeviceReady = true;

    std::uint8_t irqEnable = 0;
    if (!readPmuRegister(0x41, irqEnable)
        || !writePmuRegister(0x41, static_cast<std::uint8_t>(irqEnable | 0x08))
        || !writePmuRegister(0x49, 0xFF)) {
        ESP_LOGE(Tag, "PMIC power-button interrupt setup failed");
        pmuButtonReady = false;
        return;
    }
    pmuButtonReady = true;
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

void pollBattery(float dt) {
    if (!pmuDeviceReady) return;
    batteryPollSeconds += dt;
    if (batteryPollSeconds < 2.0F) return;
    batteryPollSeconds = 0.0F;

    // AXP2101 STATUS1 bit 3 indicates a connected battery; 0xA4 is fuel-gauge percent.
    std::uint8_t status1 = 0;
    if (!readPmuRegister(0x00, status1)) return;
    if ((status1 & 0x08U) == 0U) {
        evolvinggarden::app::setBatteryStatus(-1, false);
        return;
    }

    std::uint8_t status2 = 0;
    std::uint8_t batteryPercent = 0;
    if (!readPmuRegister(0x01, status2)
        || !readPmuRegister(0xA4, batteryPercent)) return;

    const int safePercent = std::min(static_cast<int>(batteryPercent), 100);
    const bool charging = (status2 >> 5U) == 0x01U;
    evolvinggarden::app::setBatteryStatus(safePercent, charging);
}


constexpr int DisplayWidth = 466;
constexpr int DisplayHeight = 466;
constexpr int DisplayBitsPerPixel = 16;
constexpr std::uint32_t DisplayQspiClockHz = 80'000'000;
constexpr gpio_num_t DisplayTearingEffectPin = GPIO_NUM_13;

static const std::uint8_t displayPage20[] = {0x20};
static const std::uint8_t displaySelectPage00[] = {0x00};
static const std::uint8_t displayModeSelect[] = {0x10};
static const std::uint8_t displayPanelMode[] = {0xA0};
static const std::uint8_t displayDrive[] = {0x80};
static const std::uint8_t displayRgb565[] = {0x55};
static const std::uint8_t displayTeEnable[] = {0x00};
static const std::uint8_t displayInterfaceMode[] = {0x20};
static const std::uint8_t displayBrightness[] = {0xFF};
static const std::uint8_t displayColumnRange[] = {0x00, 0x06, 0x01, 0xD7};
static const std::uint8_t displayRowRange[] = {0x00, 0x00, 0x01, 0xD1};

static const co5300_lcd_init_cmd_t displayInitCommands[] = {
    {0xFE, displayPage20, sizeof(displayPage20), 0},
    {0x19, displayModeSelect, sizeof(displayModeSelect), 0},
    {0x1C, displayPanelMode, sizeof(displayPanelMode), 0},
    {0xFE, displaySelectPage00, sizeof(displaySelectPage00), 0},
    {0xC4, displayDrive, sizeof(displayDrive), 0},
    {0x3A, displayRgb565, sizeof(displayRgb565), 0},
    {0x35, displayTeEnable, sizeof(displayTeEnable), 0},
    {0x53, displayInterfaceMode, sizeof(displayInterfaceMode), 0},
    {0x51, displayBrightness, sizeof(displayBrightness), 0},
    {0x63, displayBrightness, sizeof(displayBrightness), 0},
    {0x2A, displayColumnRange, sizeof(displayColumnRange), 0},
    {0x2B, displayRowRange, sizeof(displayRowRange), 600},
    {0x11, nullptr, 0, 600},
    {0x29, nullptr, 0, 0},
};

esp_err_t setDisplayBrightness(int percent) {
    if (displayPanelIo == nullptr) return ESP_ERR_INVALID_STATE;
    if (percent < 0 || percent > 100) return ESP_ERR_INVALID_ARG;

    const std::uint8_t brightness =
        static_cast<std::uint8_t>(percent * 255 / 100);
    std::uint32_t command = 0x51;
    command &= 0xFFU;
    command <<= 8U;
    command |= 0x02U << 24U;
    return esp_lcd_panel_io_tx_param(displayPanelIo, command, &brightness, 1);
}

void roundDisplayInvalidationArea(lv_event_t* event) {
    auto* area = static_cast<lv_area_t*>(lv_event_get_param(event));
    if (area == nullptr) return;

    const std::int32_t x1 = area->x1;
    const std::int32_t x2 = area->x2;
    const std::int32_t y1 = area->y1;
    const std::int32_t y2 = area->y2;
    area->x1 = static_cast<lv_coord_t>((x1 >> 1) << 1);
    area->y1 = static_cast<lv_coord_t>((y1 >> 1) << 1);
    area->x2 = static_cast<lv_coord_t>(((x2 >> 1) << 1) + 1);
    area->y2 = static_cast<lv_coord_t>(((y2 >> 1) << 1) + 1);
}

lv_display_t* startDisplayWithTearingSync() {
    ESP_LOGI(Tag, "Initialize CO5300 QSPI display at %lu Hz",
        static_cast<unsigned long>(DisplayQspiClockHz));

    const spi_bus_config_t busConfig = CO5300_PANEL_BUS_QSPI_CONFIG(
        BSP_LCD_PCLK, BSP_LCD_DATA0, BSP_LCD_DATA1, BSP_LCD_DATA2, BSP_LCD_DATA3,
        DisplayWidth * DisplayHeight * DisplayBitsPerPixel / 8);
    ESP_ERROR_CHECK(spi_bus_initialize(BSP_LCD_SPI_NUM, &busConfig, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_spi_config_t ioConfig =
        CO5300_PANEL_IO_QSPI_CONFIG(BSP_LCD_CS, nullptr, nullptr);
    ioConfig.pclk_hz = DisplayQspiClockHz;
    ioConfig.trans_queue_depth = CONFIG_BSP_LCD_TRANS_QUEUE_DEPTH;
    co5300_vendor_config_t vendorConfig = {
        .init_cmds = displayInitCommands,
        .init_cmds_size = static_cast<std::uint16_t>(
            sizeof(displayInitCommands) / sizeof(displayInitCommands[0])),
        .flags = {.use_qspi_interface = 1},
    };

    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(
        static_cast<esp_lcd_spi_bus_handle_t>(BSP_LCD_SPI_NUM),
        &ioConfig, &displayPanelIo));

    const esp_lcd_panel_dev_config_t panelConfig = {
        .reset_gpio_num = BSP_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = DisplayBitsPerPixel,
        .vendor_config = &vendorConfig,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_co5300(
        displayPanelIo, &panelConfig, &displayPanel));
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(displayPanel, 0x06, 0));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(displayPanel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(displayPanel));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(displayPanel, true));

    esp_lv_adapter_config_t adapterConfig = ESP_LV_ADAPTER_DEFAULT_CONFIG();
    ESP_ERROR_CHECK(esp_lv_adapter_init(&adapterConfig));

    const esp_lv_adapter_display_config_t displayConfig = {
        .panel = displayPanel,
        .panel_io = displayPanelIo,
        .profile = {
            .interface = ESP_LV_ADAPTER_PANEL_IF_OTHER,
            .rotation = ESP_LV_ADAPTER_ROTATE_0,
            .hor_res = DisplayWidth,
            .ver_res = DisplayHeight,
            .buffer_height = 50,
            .use_psram = true,
            .enable_ppa_accel = false,
            .require_double_buffer = false,
        },
        .tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_TE_SYNC,
        .te_sync = {
            .gpio_num = DisplayTearingEffectPin,
            .time_tvdl_ms = 13,
            .time_tvdh_ms = 1,
            .bus_freq_hz = DisplayQspiClockHz,
            .data_lines = 4,
            .bits_per_pixel = DisplayBitsPerPixel,
            .intr_type = GPIO_INTR_POSEDGE,
            .refresh_window_percent = 66,
        },
    };

    lv_display_t* display = esp_lv_adapter_register_display(&displayConfig);
    if (display == nullptr) {
        ESP_LOGE(Tag, "Could not register CO5300 display with TE synchronization");
        return nullptr;
    }
    lv_display_add_event_cb(
        display, roundDisplayInvalidationArea, LV_EVENT_INVALIDATE_AREA, nullptr);

    bsp_display_cfg_t touchBspConfig = {
        .lv_adapter_cfg = adapterConfig,
        .rotation = ESP_LV_ADAPTER_ROTATE_0,
        .tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_TE_SYNC,
        .touch_flags = {
            .swap_xy = 0,
            .mirror_x = 1,
            .mirror_y = 1,
        },
    };
    esp_lcd_touch_handle_t touchHandle = nullptr;
    ESP_ERROR_CHECK(bsp_touch_new(&touchBspConfig, &touchHandle));
    const esp_lv_adapter_touch_config_t touchConfig =
        ESP_LV_ADAPTER_TOUCH_DEFAULT_CONFIG(display, touchHandle);
    if (esp_lv_adapter_register_touch(&touchConfig) == nullptr) {
        ESP_LOGE(Tag, "Could not register CST9217 touchscreen");
        return nullptr;
    }

    ESP_ERROR_CHECK(setDisplayBrightness(100));
    ESP_ERROR_CHECK(esp_lv_adapter_start());
    ESP_LOGI(Tag, "CO5300 TE synchronization enabled on GPIO%d",
        static_cast<int>(DisplayTearingEffectPin));
    return display;
}

void onFrame(lv_timer_t*) {
    pollAdminButton();
    pollPowerButton(FrameStepSeconds);
    pollBattery(FrameStepSeconds);
    evolvinggarden::motion::update(FrameStepSeconds);

    const auto sensor = evolvinggarden::motion::state();
    if (sensor.screenSleeping != displaySleeping) {
        displaySleeping = sensor.screenSleeping;
        (void)setDisplayBrightness(displaySleeping ? 0 : activeBrightness);
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

void initializeGardenUiTask(void*) {
    // LVGL object creation and the first diagnostics refresh use more stack
    // than ESP-IDF's small default app_main task provides.
    bsp_display_lock(-1);
    evolvinggarden::app::initialize();
    lv_timer_create(onFrame, 33, nullptr);
    bsp_display_unlock();

    const UBaseType_t stackHighWatermark = uxTaskGetStackHighWaterMark(nullptr);
    ESP_LOGI(Tag, "UI initialized; init-task stack high-water mark: %u bytes",
        static_cast<unsigned>(stackHighWatermark));
    ESP_LOGI(Tag, "Ready. PWR opens Garden menu; BOOT opens LAB; tap to hit; circle to pet.");
    vTaskDelete(nullptr);
}

extern "C" void app_main(void) {
    ESP_LOGI(Tag, "Starting EvolvingGarden on ESP32-S3-Touch-AMOLED-1.75C");
    activeDisplay = startDisplayWithTearingSync();
    if (activeDisplay == nullptr) {
        ESP_LOGE(Tag, "TE-synchronized display initialization failed");
        return;
    }
    ESP_ERROR_CHECK(esp_lv_adapter_fps_stats_enable(activeDisplay, true));
    activeBrightness = 100;

    configureAdminButton();
    configurePowerButton();
    evolvinggarden::motion::initialize();
    evolvinggarden::audio::initialize();

    constexpr std::uint32_t InitTaskStackBytes = 8192;
    const BaseType_t taskCreated = xTaskCreate(
        initializeGardenUiTask, "garden_init", InitTaskStackBytes, nullptr, 5, nullptr);
    if (taskCreated != pdPASS) {
        ESP_LOGE(Tag, "Could not create UI initialization task");
    }
}
