#include "platform/motion.hpp"

#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "qmi8658.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace evolvinggarden::motion {
namespace {

constexpr char Tag[] = "GardenMotion";
constexpr float Pi = 3.14159265358979323846F;
constexpr float RadiansToDegrees = 180.0F / Pi;

qmi8658_dev_t imu{};
State current{};
bool initialized = false;
float gravityX = 0.0F;
float gravityY = 0.0F;
float gravityZ = 0.0F;
bool filterInitialized = false;

float stepCooldown = 0.0F;
bool stepArmed = false;
float shakeCooldown = 0.0F;
int shakeFrames = 0;
float tiltDuration = 0.0F;
bool tiltLatched = false;
float rotationRadians = 0.0F;
float rotationCooldown = 0.0F;
float faceDownDuration = 0.0F;
float wakeHoldoff = 0.0F;
float calibrationDuration = 0.0F;
int screenUpSign = 0;

float magnitude(float x, float y, float z) {
    return std::sqrt(x * x + y * y + z * z);
}

} // namespace

bool initialize() {
    const auto bus = bsp_i2c_get_handle();
    if (bus == nullptr) {
        ESP_LOGE(Tag, "BSP I2C bus is unavailable");
        return false;
    }

    const esp_err_t initResult = qmi8658_init(&imu, bus, QMI8658_ADDRESS_HIGH);
    if (initResult != ESP_OK) {
        ESP_LOGE(Tag, "QMI8658 initialization failed: %s", esp_err_to_name(initResult));
        return false;
    }

    esp_err_t result = qmi8658_set_accel_range(&imu, QMI8658_ACCEL_RANGE_8G);
    if (result == ESP_OK) result = qmi8658_set_accel_odr(&imu, QMI8658_ACCEL_ODR_250HZ);
    if (result == ESP_OK) result = qmi8658_set_gyro_range(&imu, QMI8658_GYRO_RANGE_512DPS);
    if (result == ESP_OK) result = qmi8658_set_gyro_odr(&imu, QMI8658_GYRO_ODR_250HZ);
    if (result == ESP_OK) qmi8658_set_accel_unit_mps2(&imu, true);
    if (result == ESP_OK) qmi8658_set_gyro_unit_rads(&imu, true);

    if (result != ESP_OK) {
        ESP_LOGE(Tag, "QMI8658 configuration failed: %s", esp_err_to_name(result));
        return false;
    }

    current.ready = true;
    initialized = true;
    ESP_LOGI(Tag, "QMI8658 ready");
    return true;
}

void update(float deltaSeconds) {
    if (!initialized) return;

    const float dt = std::clamp(deltaSeconds, 0.005F, 0.12F);
    stepCooldown = std::max(0.0F, stepCooldown - dt);
    shakeCooldown = std::max(0.0F, shakeCooldown - dt);
    rotationCooldown = std::max(0.0F, rotationCooldown - dt);
    wakeHoldoff = std::max(0.0F, wakeHoldoff - dt);

    bool dataReady = false;
    if (qmi8658_is_data_ready(&imu, &dataReady) != ESP_OK || !dataReady) return;

    qmi8658_data_t data{};
    if (qmi8658_read_sensor_data(&imu, &data) != ESP_OK) return;

    if (!filterInitialized) {
        gravityX = data.accelX;
        gravityY = data.accelY;
        gravityZ = data.accelZ;
        filterInitialized = true;
    }

    const float alpha = std::clamp(dt / (0.55F + dt), 0.01F, 0.20F);
    gravityX += (data.accelX - gravityX) * alpha;
    gravityY += (data.accelY - gravityY) * alpha;
    gravityZ += (data.accelZ - gravityZ) * alpha;

    const float linear = magnitude(
        data.accelX - gravityX,
        data.accelY - gravityY,
        data.accelZ - gravityZ);
    const float angular = magnitude(data.gyroX, data.gyroY, data.gyroZ);

    current.linearAcceleration = linear;
    current.angularVelocity = angular;
    current.rollDegrees = std::atan2(gravityY, gravityZ) * RadiansToDegrees;
    current.pitchDegrees = std::atan2(
        -gravityX, std::sqrt(gravityY * gravityY + gravityZ * gravityZ)) * RadiansToDegrees;

    // Calibrate the screen normal from a stable, flat, screen-up start.
    if (screenUpSign == 0) {
        if (std::fabs(data.accelZ) > 7.2F && linear < 0.65F && angular < 0.45F) {
            calibrationDuration += dt;
            if (calibrationDuration >= 0.8F) {
                screenUpSign = data.accelZ >= 0.0F ? 1 : -1;
                current.orientationCalibrated = true;
                ESP_LOGI(Tag, "Screen orientation calibrated");
            }
        } else {
            calibrationDuration = 0.0F;
        }
    }

    // Moderate periodic movement is counted as steps. Large impulses are reserved for shakes.
    if (!stepArmed && stepCooldown <= 0.0F
        && linear >= 1.35F && linear <= 3.6F && angular < 2.5F) {
        ++current.steps;
        stepArmed = true;
        stepCooldown = 0.30F;
    }
    if (stepArmed && linear < 0.62F) stepArmed = false;

    const bool shakeCandidate = linear >= 3.9F && (angular >= 1.8F || linear >= 5.5F);
    if (shakeCandidate) {
        ++shakeFrames;
        if (shakeFrames >= 2 && shakeCooldown <= 0.0F) {
            ++current.shakeEvents;
            shakeCooldown = 1.10F;
            shakeFrames = 0;
        }
    } else {
        shakeFrames = 0;
    }

    const float tilt = std::max(std::fabs(current.rollDegrees), std::fabs(current.pitchDegrees));
    if (!tiltLatched && tilt >= 32.0F) {
        tiltDuration += dt;
        if (tiltDuration >= 0.42F) {
            ++current.tiltEvents;
            tiltLatched = true;
            tiltDuration = 0.0F;
        }
    } else if (tilt < 19.0F) {
        tiltDuration = 0.0F;
        tiltLatched = false;
    }

    // Gyroscope Z measures rotation around the screen normal (not a board tilt).
    const float yawRate = std::fabs(data.gyroZ);
    if (yawRate >= 1.55F) {
        rotationRadians += yawRate * dt;
    } else if (rotationRadians >= 1.55F && rotationCooldown <= 0.0F) {
        ++current.rotationEvents;
        rotationCooldown = 1.20F;
        rotationRadians = 0.0F;
    } else if (yawRate < 0.45F) {
        rotationRadians = std::max(0.0F, rotationRadians - dt * 1.8F);
    }
    if (rotationCooldown > 0.0F) rotationRadians = 0.0F;

    // Blank only the AMOLED. MCU + IMU remain awake so the device can wake again.
    if (screenUpSign != 0 && wakeHoldoff <= 0.0F) {
        const float projectedNormal = data.accelZ * static_cast<float>(screenUpSign);
        const bool faceDownCandidate =
            projectedNormal < -7.4F
            && std::fabs(data.accelX) < 2.8F
            && std::fabs(data.accelY) < 2.8F
            && linear < 0.75F
            && angular < 0.40F;

        if (faceDownCandidate) {
            faceDownDuration += dt;
            if (faceDownDuration >= 1.45F) current.screenSleeping = true;
        } else {
            faceDownDuration = 0.0F;
            if (current.screenSleeping
                && (projectedNormal > -1.0F || linear > 1.0F || angular > 0.9F)) {
                current.screenSleeping = false;
            }
        }
    } else {
        faceDownDuration = 0.0F;
    }
}

State state() {
    return current;
}

bool requestWake() {
    const bool wasSleeping = current.screenSleeping;
    current.screenSleeping = false;
    wakeHoldoff = 3.0F;
    faceDownDuration = 0.0F;
    return wasSleeping;
}

} // namespace evolvinggarden::motion
