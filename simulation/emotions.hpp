#pragma once
#include "core/types.hpp"
#include <array>
#include <cstddef>
namespace evolvinggarden {
enum class Emotion : std::size_t {
    Joy = 0, Sadness, Anger, Fear, Surprise, Curiosity, Affection, Disgust, Count
};
inline constexpr std::size_t EmotionCount = static_cast<std::size_t>(Emotion::Count);
class EmotionalState {
public:
    float value(Emotion emotion) const { return values_[static_cast<std::size_t>(emotion)]; }
    void add(Emotion emotion, float amount) {
        const auto index = static_cast<std::size_t>(emotion);
        values_[index] = clampPercent(values_[index] + amount);
    }
    void set(Emotion emotion, float amount) {
        values_[static_cast<std::size_t>(emotion)] = clampPercent(amount);
    }
    void update(float deltaSeconds) {
        constexpr std::array<float, EmotionCount> decayRates{
            2.4F, 0.75F, 1.5F, 1.2F, 5.0F, 1.8F, 0.25F, 1.1F
        };
        for (std::size_t i = 0; i < values_.size(); ++i)
            values_[i] = clampPercent(values_[i] - decayRates[i] * deltaSeconds);
        arousal_ = clampPercent(arousal_ - 2.0F * deltaSeconds);
    }
    void setArousal(float value) { arousal_ = clampPercent(value); }
    float arousal() const { return arousal_; }
    const std::array<float, EmotionCount>& values() const { return values_; }
private:
    std::array<float, EmotionCount> values_{};
    float arousal_ = 0.0F;
};
}
