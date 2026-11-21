#pragma once
#include <algorithm>
#include <cstdint>
namespace evolvinggarden {
template <typename T> constexpr T clampValue(T value, T minimum, T maximum) {
    return std::clamp(value, minimum, maximum);
}
inline std::int16_t clampTrait(int value) {
    return static_cast<std::int16_t>(clampValue(value, -100, 100));
}
inline float clampPercent(float value) { return clampValue(value, 0.0F, 100.0F); }
inline float normalizeTrait(std::int16_t value) {
    return (static_cast<float>(value) + 100.0F) / 200.0F;
}
}
