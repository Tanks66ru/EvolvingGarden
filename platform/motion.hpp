#pragma once

#include <cstdint>

namespace evolvinggarden::motion {

struct State {
    bool ready = false;
    bool orientationCalibrated = false;
    bool screenSleeping = false;
    float rollDegrees = 0.0F;
    float pitchDegrees = 0.0F;
    float linearAcceleration = 0.0F;
    float angularVelocity = 0.0F;
    std::uint32_t steps = 0;
    std::uint32_t shakeEvents = 0;
    std::uint32_t tiltEvents = 0;
    std::uint32_t rotationEvents = 0;
};

bool initialize();
void update(float deltaSeconds);
State state();
bool requestWake();

} // namespace evolvinggarden::motion
