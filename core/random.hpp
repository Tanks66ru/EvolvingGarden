#pragma once

#include <cstdint>

namespace evolvinggarden {

// Small deterministic PRNG suitable for desktop tests and embedded targets.
// It avoids the larger <random> distribution machinery in the shared core.
class Random32 {
public:
    explicit Random32(std::uint32_t seed = 0xA341316CU)
        : state_(seed != 0U ? seed : 0xA341316CU) {}

    std::uint32_t next() {
        // xorshift32: compact, fast, and deterministic across platforms.
        std::uint32_t value = state_;
        value ^= value << 13U;
        value ^= value >> 17U;
        value ^= value << 5U;
        state_ = value;
        return value;
    }

    float unit() {
        // Use the upper 24 bits so the result is representable as a float.
        return static_cast<float>(next() >> 8U) * (1.0F / 16777216.0F);
    }

    float range(float minimum, float maximum) {
        return minimum + (maximum - minimum) * unit();
    }

    std::uint32_t state() const { return state_; }

    void setState(std::uint32_t state) {
        state_ = state != 0U ? state : 0xA341316CU;
    }

private:
    std::uint32_t state_;
};

} // namespace evolvinggarden
