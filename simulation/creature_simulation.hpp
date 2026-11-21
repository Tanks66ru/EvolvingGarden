#pragma once
#include "creatures/genome.hpp"
#include "simulation/emotions.hpp"
#include "simulation/memory.hpp"
#include "simulation/reactions.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <random>
namespace evolvinggarden {
struct CreatureNeeds {
    float hunger = 24.0F;
    float energy = 82.0F;
    float social = 34.0F;
    float boredom = 28.0F;
};
enum class Interaction : std::uint8_t {
    Feed, Pet, Novelty, LoudSound, BadTaste, BroughtToForeground, ReturnedToFarm, Count
};
enum class Action : std::uint8_t { Idle, Explore, Play, Rest, SeekCompany, Observe, Forage };
inline const char* actionName(Action action) {
    switch (action) {
    case Action::Idle: return "Taking it easy";
    case Action::Explore: return "Exploring";
    case Action::Play: return "Playing";
    case Action::Rest: return "Resting";
    case Action::SeekCompany: return "Looking for company";
    case Action::Observe: return "Watching something";
    case Action::Forage: return "Looking for food";
    }
    return "Idle";
}
class CreatureSimulation {
public:
    static constexpr float SignificantEventCooldownSeconds = 600.0F;
    static constexpr float ForegroundBondCooldownSeconds = 600.0F;
    static constexpr float LoudSoundCooldownSeconds = 1.5F;

    explicit CreatureSimulation(Genome genome = makeDefaultGenome())
        : genome_(genome), random_(genome.seed) {
        genome_.normalize();
        remember(EventType::Birth, 0, 100, true);
    }

    const Genome& genome() const { return genome_; }
    const CreatureNeeds& needs() const { return needs_; }
    const EmotionalState& emotions() const { return emotions_; }
    const MemoryJournal& memories() const { return memories_; }
    const ReactionState& reaction() const { return reaction_; }
    Action action() const { return action_; }
    float mood() const { return mood_; }
    float arousal() const { return emotions_.arousal(); }
    float affectionToPlayer() const { return affectionToPlayer_; }
    float simulationTime() const { return simulationTime_; }
    bool isInForeground() const { return inForeground_; }

    float cooldownRemaining(Interaction interaction) const {
        if (interaction == Interaction::Count) return 0.0F;
        return cooldowns_[static_cast<std::size_t>(interaction)];
    }

    // Returns false if the action is on cooldown or the screen state would not change.
    bool applyInteraction(Interaction interaction) {
        if (interaction == Interaction::Count) return false;
        if (interaction == Interaction::LoudSound) {
            return applySoundStimulus(0.62F, 0.22F);
        }

        const std::size_t index = static_cast<std::size_t>(interaction);
        if (cooldowns_[index] > 0.0F) return false;

        bool changed = true;
        switch (interaction) {
        case Interaction::Feed:
            cooldowns_[index] = 1.20F;
            applyFeed();
            break;
        case Interaction::Pet:
            cooldowns_[index] = 1.40F;
            applyPet();
            break;
        case Interaction::Novelty:
            cooldowns_[index] = 1.40F;
            applyNovelty();
            break;
        case Interaction::BadTaste:
            cooldowns_[index] = 1.40F;
            applyBadTaste();
            break;
        case Interaction::BroughtToForeground:
            changed = bringToForeground();
            break;
        case Interaction::ReturnedToFarm:
            changed = returnToFarm();
            break;
        case Interaction::LoudSound:
        case Interaction::Count:
            changed = false;
            break;
        }
        if (changed) updateMood(0.12F);
        return changed;
    }

    // The sensor layer will pass measured loudness and event duration here.
    // intensity is normalized to [0, 1], duration is measured in seconds.
    bool applySoundStimulus(float intensity, float durationSeconds) {
        const std::size_t index = static_cast<std::size_t>(Interaction::LoudSound);
        if (cooldowns_[index] > 0.0F || reaction_.active()) return false;

        const float loudness = clampValue(intensity, 0.0F, 1.0F);
        const float duration = clampValue(durationSeconds, 0.0F, 3.0F);
        if (loudness < 0.08F || duration < 0.03F) return false;

        cooldowns_[index] = LoudSoundCooldownSeconds;
        soundExposure_ = clampValue(soundExposure_ + loudness * duration, 0.0F, 3.0F);

        const float sensitivity = normalizeTrait(genome_.personality.sensitivity);
        const float fear = loudness * (22.0F + sensitivity * 64.0F)
            * (0.65F + std::min(duration, 1.5F) * 0.30F);
        emotions_.add(Emotion::Surprise, 24.0F + loudness * 35.0F);
        emotions_.add(Emotion::Fear, fear);

        const bool sustainedOrVeryLoud =
            (loudness >= 0.88F && duration >= 0.75F) || soundExposure_ >= 1.05F;
        const Reaction reaction = sustainedOrVeryLoud ? Reaction::Fearful : Reaction::Surprised;
        const float strength = 45.0F + loudness * 55.0F;
        const float animationDuration = sustainedOrVeryLoud ? 1.9F : 1.05F;
        reaction_.trigger(reaction, strength, animationDuration, sustainedOrVeryLoud);

        if (sustainedOrVeryLoud && fear >= 68.0F) {
            remember(EventType::Startled, -static_cast<int>(fear), 92);
        }
        updateMood(0.12F);
        return true;
    }

    void update(float deltaSeconds) {
        const float dt = clampValue(deltaSeconds, 0.0F, 0.25F);
        if (dt <= 0.0F) return;
        simulationTime_ += dt;

        for (float& cooldown : cooldowns_) {
            cooldown = std::max(0.0F, cooldown - dt);
        }
        soundExposure_ = std::max(0.0F, soundExposure_ - dt * 0.38F);

        updateNeeds(dt);
        updateDistress(dt);
        emotions_.update(dt);
        reaction_.update(dt);
        updateAutonomousReaction();
        actionTimer_ -= dt;
        if (actionTimer_ <= 0.0F) chooseAction();
        applyAction(dt);
        updateMood(dt);

        float strongestEmotion = 0.0F;
        for (float value : emotions_.values()) strongestEmotion = std::max(strongestEmotion, value);
        emotions_.setArousal(strongestEmotion);
    }

private:
    void remember(EventType type, int emotionalImpact, int significance, bool bypassCooldown = false) {
        const auto impact = static_cast<std::int8_t>(clampValue(emotionalImpact, -100, 100));
        const auto importance = static_cast<std::uint8_t>(clampValue(significance, 0, 100));
        if (!bypassCooldown && importance < MemoryJournal::MinimumSignificance) return;
        if (!bypassCooldown && hasSignificantEvent_
            && simulationTime_ - lastSignificantEventTime_ < SignificantEventCooldownSeconds) return;

        const bool stored = memories_.add(MemoryEvent{
            static_cast<std::uint32_t>(simulationTime_), type, impact, importance, 0
        });
        if (stored) {
            hasSignificantEvent_ = true;
            lastSignificantEventTime_ = simulationTime_;
        }
    }

    void applyFeed() {
        const float hungerBefore = needs_.hunger;
        needs_.hunger = clampPercent(needs_.hunger - 42.0F);
        emotions_.add(Emotion::Joy, hungerBefore > 35.0F ? 28.0F : 10.0F);
        emotions_.add(Emotion::Affection, 5.0F);
        if (hungerBefore > 35.0F) {
            const bool exaggerated = hungerBefore >= 88.0F;
            reaction_.trigger(Reaction::Happy, 82.0F, exaggerated ? 1.65F : 1.15F, exaggerated);
        }
        if (hungerBefore >= 90.0F) remember(EventType::Fed, 55, 82);
    }

    void applyPet() {
        const auto& genes = genome_.personality;
        const float friendliness = normalizeTrait(genes.friendliness);
        const float sensitivity = normalizeTrait(genes.sensitivity);
        const float welcome = friendliness * 0.72F + (1.0F - sensitivity) * 0.28F;

        if (welcome >= 0.48F) {
            emotions_.add(Emotion::Joy, 10.0F + welcome * 18.0F);
            emotions_.add(Emotion::Affection, 15.0F + welcome * 20.0F);
            needs_.social = clampPercent(needs_.social - 9.0F);
            const bool exaggerated = welcome >= 0.92F;
            reaction_.trigger(Reaction::Affectionate, 65.0F + welcome * 30.0F,
                exaggerated ? 1.75F : 1.35F, exaggerated);
        } else {
            emotions_.add(Emotion::Anger, 8.0F + (0.48F - welcome) * 26.0F);
            emotions_.add(Emotion::Fear, sensitivity * 10.0F);
            const bool exaggerated = welcome <= 0.12F;
            reaction_.trigger(Reaction::Angry, 55.0F + (0.48F - welcome) * 70.0F,
                exaggerated ? 1.45F : 1.0F, exaggerated);
        }
        affectionToPlayer_ = clampPercent(affectionToPlayer_ + (welcome - 0.45F) * 0.35F);
        checkBondMilestone();
    }

    void applyNovelty() {
        const auto& genes = genome_.personality;
        const float curiosity = normalizeTrait(genes.curiosity);
        const float optimism = normalizeTrait(genes.optimism);
        const float diminishingReturn = 1.0F / std::sqrt(
            1.0F + static_cast<float>(noveltyCount_) * 0.25F);

        emotions_.add(Emotion::Surprise, 26.0F);
        emotions_.add(Emotion::Curiosity, 18.0F + curiosity * 20.0F);
        emotions_.add(Emotion::Joy, optimism * 17.0F);
        const bool exaggerated = curiosity >= 0.92F && noveltyCount_ == 0;
        reaction_.trigger(Reaction::Curious, 55.0F + curiosity * 40.0F,
            exaggerated ? 1.7F : 1.35F, exaggerated);

        const float bondGain = (0.10F + curiosity * 0.72F + optimism * 0.48F)
            * diminishingReturn;
        affectionToPlayer_ = clampPercent(affectionToPlayer_ + bondGain);
        const int significance = static_cast<int>(
            52.0F + curiosity * 24.0F + optimism * 10.0F
            - static_cast<float>(noveltyCount_) * 8.0F);
        ++noveltyCount_;
        needs_.boredom = clampPercent(needs_.boredom - 14.0F);
        remember(EventType::NoveltyDiscovered, static_cast<int>(curiosity * 40.0F), significance);
        checkBondMilestone();
    }


    void applyBadTaste() {
        const float sensitivity = normalizeTrait(genome_.personality.sensitivity);
        emotions_.add(Emotion::Disgust, 62.0F + sensitivity * 25.0F);
        emotions_.add(Emotion::Joy, -8.0F);
        needs_.hunger = clampPercent(needs_.hunger + 5.0F);
        const bool exaggerated = sensitivity >= 0.80F;
        reaction_.trigger(Reaction::Disgusted, 78.0F + sensitivity * 20.0F,
            exaggerated ? 1.65F : 1.15F, exaggerated);
    }

    bool bringToForeground() {
        if (inForeground_) return false;
        inForeground_ = true;

        const auto& genes = genome_.personality;
        const float curiosity = normalizeTrait(genes.curiosity);
        const float optimism = normalizeTrait(genes.optimism);
        const float sociability = normalizeTrait(genes.sociability);
        const bool rewardReady = !hasForegroundReward_
            || simulationTime_ - lastForegroundRewardTime_ >= ForegroundBondCooldownSeconds;

        emotions_.add(Emotion::Joy, 5.0F + optimism * 8.0F);
        emotions_.add(Emotion::Curiosity, curiosity * 8.0F);
        needs_.social = clampPercent(needs_.social - 4.0F);
        reaction_.trigger(Reaction::Curious, 60.0F + curiosity * 30.0F, 1.2F);

        if (rewardReady) {
            const float diminishingReturn = 1.0F / std::sqrt(
                1.0F + static_cast<float>(foregroundVisits_) * 0.35F);
            const float bondGain = (0.08F + curiosity * 0.24F
                + optimism * 0.28F + sociability * 0.35F) * diminishingReturn;
            affectionToPlayer_ = clampPercent(affectionToPlayer_ + bondGain);
            ++foregroundVisits_;
            hasForegroundReward_ = true;
            lastForegroundRewardTime_ = simulationTime_;
            if (foregroundVisits_ == 1) {
                const int significance = static_cast<int>(
                    65.0F + curiosity * 8.0F + sociability * 8.0F);
                remember(EventType::BroughtToForeground, static_cast<int>(bondGain * 50.0F), significance);
            }
            checkBondMilestone();
        }
        return true;
    }

    bool returnToFarm() {
        if (!inForeground_) return false;
        inForeground_ = false;
        return true;
    }

    void checkBondMilestone() {
        int milestone = 0;
        if (affectionToPlayer_ >= 100.0F) milestone = 100;
        else if (affectionToPlayer_ >= 75.0F) milestone = 75;
        else if (affectionToPlayer_ >= 50.0F) milestone = 50;
        if (milestone > lastBondMilestone_) {
            lastBondMilestone_ = milestone;
            remember(EventType::BondMilestone, 45, 88);
        }
    }


    void updateDistress(float dt) {
        if (needs_.hunger >= 85.0F) {
            emotions_.add(Emotion::Sadness, (needs_.hunger - 80.0F) * 0.070F * dt);
        }
        const float sociability = normalizeTrait(genome_.personality.sociability);
        if (needs_.social >= 82.0F && sociability >= 0.65F) {
            emotions_.add(Emotion::Sadness, (needs_.social - 78.0F) * 0.065F * dt);
        }
    }

    void updateAutonomousReaction() {
        if (reaction_.active() || simulationTime_ < nextAutonomousReactionTime_) return;
        const float sadness = emotions_.value(Emotion::Sadness);
        if (sadness >= 42.0F) {
            const bool exaggerated = sadness >= 82.0F;
            reaction_.trigger(Reaction::Sad, sadness, exaggerated ? 1.8F : 1.3F, exaggerated);
            nextAutonomousReactionTime_ = simulationTime_ + 4.0F;
        }
    }

    void updateNeeds(float dt) {
        needs_.hunger = clampPercent(needs_.hunger + 0.055F * dt);
        needs_.social = clampPercent(needs_.social + 0.075F * dt);
        needs_.boredom = clampPercent(needs_.boredom + 0.09F * dt);
        needs_.energy = clampPercent(needs_.energy - 0.035F * dt);
    }

    void chooseAction() {
        const auto& genes = genome_.personality;
        const float curiosity = normalizeTrait(genes.curiosity);
        const float sociability = normalizeTrait(genes.sociability);
        const float activity = normalizeTrait(genes.activity);
        struct Candidate { Action action; float score; };
        Candidate candidates[] = {
            {Action::Forage, needs_.hunger * 0.78F},
            {Action::Rest, (100.0F - needs_.energy) * 0.95F},
            {Action::SeekCompany, needs_.social * (0.25F + sociability * 0.95F)},
            {Action::Explore, curiosity * 38.0F + needs_.boredom * 0.28F},
            {Action::Play, needs_.boredom * (0.22F + activity * 0.72F)},
            {Action::Observe, curiosity * 30.0F + (1.0F - activity) * 8.0F},
            {Action::Idle, 18.0F + (1.0F - activity) * 8.0F}
        };
        std::uniform_real_distribution<float> variation(-3.0F, 3.0F);
        std::size_t bestIndex = 0;
        float bestScore = -100000.0F;
        for (std::size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); ++i) {
            const float score = candidates[i].score + variation(random_);
            if (score > bestScore) { bestScore = score; bestIndex = i; }
        }
        action_ = candidates[bestIndex].action;
        std::uniform_real_distribution<float> duration(2.5F, 6.0F);
        actionTimer_ = duration(random_);
    }

    void applyAction(float dt) {
        switch (action_) {
        case Action::Idle:
            needs_.energy = clampPercent(needs_.energy + 0.16F * dt); break;
        case Action::Explore:
            needs_.boredom = clampPercent(needs_.boredom - 0.46F * dt);
            needs_.energy = clampPercent(needs_.energy - 0.10F * dt);
            emotions_.add(Emotion::Curiosity, 0.10F * dt); break;
        case Action::Play:
            needs_.boredom = clampPercent(needs_.boredom - 0.85F * dt);
            needs_.energy = clampPercent(needs_.energy - 0.22F * dt);
            emotions_.add(Emotion::Joy, 0.18F * dt); break;
        case Action::Rest:
            needs_.energy = clampPercent(needs_.energy + 1.35F * dt);
            needs_.boredom = clampPercent(needs_.boredom + 0.08F * dt); break;
        case Action::SeekCompany:
            needs_.social = clampPercent(needs_.social - 0.65F * dt); break;
        case Action::Observe:
            needs_.boredom = clampPercent(needs_.boredom - 0.18F * dt);
            emotions_.add(Emotion::Curiosity, 0.08F * dt); break;
        case Action::Forage:
            needs_.boredom = clampPercent(needs_.boredom - 0.12F * dt); break;
        }
    }

    void updateMood(float dt) {
        const float optimismBias =
            (normalizeTrait(genome_.personality.optimism) - 0.5F) * 12.0F;
        const float target = emotions_.value(Emotion::Joy) * 0.48F
            + emotions_.value(Emotion::Affection) * 0.24F
            - emotions_.value(Emotion::Sadness) * 0.72F
            - emotions_.value(Emotion::Fear) * 0.42F
            - emotions_.value(Emotion::Anger) * 0.34F
            - emotions_.value(Emotion::Disgust) * 0.30F
            - needs_.hunger * 0.10F - needs_.boredom * 0.06F
            + optimismBias;
        const float blend = clampValue(dt * 0.55F, 0.0F, 1.0F);
        mood_ += (clampValue(target, -100.0F, 100.0F) - mood_) * blend;
    }

    Genome genome_;
    CreatureNeeds needs_{};
    EmotionalState emotions_{};
    MemoryJournal memories_{};
    ReactionState reaction_{};
    Action action_ = Action::Idle;
    std::array<float, static_cast<std::size_t>(Interaction::Count)> cooldowns_{};
    float mood_ = 0.0F;
    float affectionToPlayer_ = 8.0F;
    float simulationTime_ = 0.0F;
    float actionTimer_ = 0.0F;
    float lastSignificantEventTime_ = 0.0F;
    float lastForegroundRewardTime_ = 0.0F;
    float soundExposure_ = 0.0F;
    float nextAutonomousReactionTime_ = 0.0F;
    std::uint32_t noveltyCount_ = 0;
    std::uint32_t foregroundVisits_ = 0;
    int lastBondMilestone_ = 0;
    bool hasSignificantEvent_ = false;
    bool hasForegroundReward_ = false;
    bool inForeground_ = false;
    std::mt19937 random_;
};
}
