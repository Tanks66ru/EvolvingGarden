#pragma once
#include "core/types.hpp"
#include <cmath>
namespace evolvinggarden {
enum class Reaction : unsigned char {
    None, Happy, Sad, Angry, Fearful, Surprised, Curious, Affectionate, Disgusted
};
class ReactionState {
public:
    bool trigger(Reaction reaction, float strength, float durationSeconds, bool exaggerated = false) {
        if (active() || reaction == Reaction::None) return false;
        reaction_ = reaction;
        strength_ = clampPercent(strength);
        duration_ = durationSeconds > 0.05F ? durationSeconds : 0.05F;
        elapsed_ = 0.0F;
        exaggerated_ = exaggerated;
        return true;
    }
    void update(float deltaSeconds) {
        if (!active()) return;
        elapsed_ += deltaSeconds;
        if (elapsed_ >= duration_) {
            reaction_ = Reaction::None;
            strength_ = 0.0F;
            elapsed_ = duration_;
            exaggerated_ = false;
        }
    }
    Reaction kind() const { return reaction_; }
    float strength() const { return strength_; }
    float duration() const { return duration_; }
    float elapsed() const { return elapsed_; }

    void restore(Reaction reaction, float strength, float duration,
                 float elapsed, bool exaggerated) {
        if (reaction == Reaction::None) {
            reaction_ = Reaction::None;
            strength_ = 0.0F;
            duration_ = 0.0F;
            elapsed_ = 0.0F;
            exaggerated_ = false;
            return;
        }
        reaction_ = reaction;
        strength_ = clampPercent(strength);
        duration_ = std::max(0.05F, duration);
        elapsed_ = clampValue(elapsed, 0.0F, duration_);
        exaggerated_ = exaggerated;
        if (elapsed_ >= duration_) {
            reaction_ = Reaction::None;
            strength_ = 0.0F;
            exaggerated_ = false;
        }
    }
    bool active() const { return reaction_ != Reaction::None; }
    bool exaggerated() const { return exaggerated_; }
    float progress() const {
        if (!active()) return 1.0F;
        return clampValue(elapsed_ / duration_, 0.0F, 1.0F);
    }
    float intensity() const {
        if (!active()) return 0.0F;
        constexpr float Pi = 3.14159265358979323846F;
        const float pulse = 0.20F + 0.80F * std::sin(progress() * Pi);
        const float multiplier = exaggerated_ ? 1.35F : 1.0F;
        return clampValue((strength_ / 100.0F) * pulse * multiplier, 0.0F, 1.0F);
    }
private:
    Reaction reaction_ = Reaction::None;
    float strength_ = 0.0F;
    float duration_ = 0.0F;
    float elapsed_ = 0.0F;
    bool exaggerated_ = false;
};
}
