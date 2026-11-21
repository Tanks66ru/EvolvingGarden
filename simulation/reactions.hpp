#pragma once
#include "core/types.hpp"
#include <cmath>
namespace evolvinggarden {
enum class Reaction : unsigned char {
    None, Happy, Affectionate, Curious, Startled, Annoyed
};
class ReactionState {
public:
    void trigger(Reaction reaction, float strength, float durationSeconds) {
        reaction_ = reaction;
        strength_ = clampPercent(strength);
        duration_ = durationSeconds > 0.05F ? durationSeconds : 0.05F;
        elapsed_ = 0.0F;
    }
    void update(float deltaSeconds) {
        if (reaction_ == Reaction::None) return;
        elapsed_ += deltaSeconds;
        if (elapsed_ >= duration_) {
            reaction_ = Reaction::None;
            strength_ = 0.0F;
            elapsed_ = duration_;
        }
    }
    Reaction kind() const { return reaction_; }
    bool active() const { return reaction_ != Reaction::None; }
    float progress() const {
        if (!active()) return 1.0F;
        return clampValue(elapsed_ / duration_, 0.0F, 1.0F);
    }
    float intensity() const {
        if (!active()) return 0.0F;
        constexpr float Pi = 3.14159265358979323846F;
        return (strength_ / 100.0F) * std::sin(progress() * Pi);
    }
private:
    Reaction reaction_ = Reaction::None;
    float strength_ = 0.0F;
    float duration_ = 0.0F;
    float elapsed_ = 0.0F;
};
}
