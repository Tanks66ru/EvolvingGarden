#pragma once
#include "creatures/genome.hpp"
#include "simulation/emotions.hpp"
#include "simulation/memory.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
namespace evolvinggarden {
struct CreatureNeeds {
    float hunger = 24.0F;
    float energy = 82.0F;
    float social = 34.0F;
    float boredom = 28.0F;
};
enum class Interaction : std::uint8_t { Feed, Pet, Novelty, LoudSound, BroughtToForeground };
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
    explicit CreatureSimulation(Genome genome = makeDefaultGenome())
        : genome_(genome), random_(genome.seed) {
        genome_.normalize();
        remember(EventType::Birth, 0, 80);
    }
    const Genome& genome() const { return genome_; }
    const CreatureNeeds& needs() const { return needs_; }
    const EmotionalState& emotions() const { return emotions_; }
    const MemoryJournal& memories() const { return memories_; }
    Action action() const { return action_; }
    float mood() const { return mood_; }
    float arousal() const { return emotions_.arousal(); }
    float affectionToPlayer() const { return affectionToPlayer_; }
    float simulationTime() const { return simulationTime_; }

    void applyInteraction(Interaction interaction) {
        switch (interaction) {
        case Interaction::Feed: applyFeed(); break;
        case Interaction::Pet: applyPet(); break;
        case Interaction::Novelty: applyNovelty(); break;
        case Interaction::LoudSound: applyLoudSound(); break;
        case Interaction::BroughtToForeground: applyForegroundVisit(); break;
        }
        updateMood(0.12F);
    }
    void update(float deltaSeconds) {
        const float dt = clampValue(deltaSeconds, 0.0F, 0.25F);
        if (dt <= 0.0F) return;
        simulationTime_ += dt;
        updateNeeds(dt);
        emotions_.update(dt);
        actionTimer_ -= dt;
        if (actionTimer_ <= 0.0F) chooseAction();
        applyAction(dt);
        updateMood(dt);
        float strongestEmotion = 0.0F;
        for (float value : emotions_.values()) strongestEmotion = std::max(strongestEmotion, value);
        emotions_.setArousal(strongestEmotion);
    }
private:
    void remember(EventType type, int emotionalImpact, int significance) {
        const auto impact = static_cast<std::int8_t>(clampValue(emotionalImpact, -100, 100));
        const auto importance = static_cast<std::uint8_t>(clampValue(significance, 0, 100));
        memories_.add(MemoryEvent{
            static_cast<std::uint32_t>(simulationTime_), type, impact, importance, 0
        });
    }
    void applyFeed() {
        const float hungerBefore = needs_.hunger;
        needs_.hunger = clampPercent(needs_.hunger - 42.0F);
        emotions_.add(Emotion::Joy, hungerBefore > 35.0F ? 28.0F : 10.0F);
        emotions_.add(Emotion::Affection, 5.0F);
        if (hungerBefore > 35.0F)
            remember(EventType::Fed, 35, hungerBefore > 70.0F ? 58 : 38);
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
        } else {
            emotions_.add(Emotion::Anger, 8.0F + (0.48F - welcome) * 26.0F);
            emotions_.add(Emotion::Fear, sensitivity * 10.0F);
        }
        affectionToPlayer_ = clampPercent(affectionToPlayer_ + (welcome - 0.45F) * 1.35F);
        if (welcome > 0.82F || welcome < 0.18F)
            remember(EventType::Petted, welcome > 0.5F ? 22 : -18, 34);
        checkBondMilestone();
    }
    void applyNovelty() {
        const auto& genes = genome_.personality;
        const float curiosity = normalizeTrait(genes.curiosity);
        const float optimism = normalizeTrait(genes.optimism);
        const float diminishingReturn = 1.0F / std::sqrt(1.0F + static_cast<float>(noveltyCount_) * 0.25F);
        emotions_.add(Emotion::Surprise, 26.0F);
        emotions_.add(Emotion::Curiosity, 18.0F + curiosity * 20.0F);
        emotions_.add(Emotion::Joy, optimism * 17.0F);
        const float bondGain = (0.10F + curiosity * 0.72F + optimism * 0.48F) * diminishingReturn;
        affectionToPlayer_ = clampPercent(affectionToPlayer_ + bondGain);
        ++noveltyCount_;
        needs_.boredom = clampPercent(needs_.boredom - 14.0F);
        remember(EventType::NoveltyDiscovered, static_cast<int>(curiosity * 35.0F), 48);
        checkBondMilestone();
    }
    void applyLoudSound() {
        const float sensitivity = normalizeTrait(genome_.personality.sensitivity);
        emotions_.add(Emotion::Surprise, 42.0F);
        emotions_.add(Emotion::Fear, 12.0F + sensitivity * 72.0F);
        remember(EventType::Startled, -static_cast<int>(sensitivity * 65.0F), 32);
    }
    void applyForegroundVisit() {
        const auto& genes = genome_.personality;
        const float curiosity = normalizeTrait(genes.curiosity);
        const float optimism = normalizeTrait(genes.optimism);
        const float sociability = normalizeTrait(genes.sociability);
        const float diminishingReturn = 1.0F / std::sqrt(1.0F + static_cast<float>(foregroundVisits_) * 0.35F);
        const float bondGain = (0.08F + curiosity * 0.24F + optimism * 0.28F
            + sociability * 0.35F) * diminishingReturn;
        affectionToPlayer_ = clampPercent(affectionToPlayer_ + bondGain);
        emotions_.add(Emotion::Joy, 5.0F + optimism * 8.0F);
        emotions_.add(Emotion::Curiosity, curiosity * 8.0F);
        needs_.social = clampPercent(needs_.social - 4.0F);
        ++foregroundVisits_;
        if (foregroundVisits_ == 1 || foregroundVisits_ % 5 == 0)
            remember(EventType::BroughtToForeground, static_cast<int>(bondGain * 50.0F), 34);
        checkBondMilestone();
    }
    void checkBondMilestone() {
        const int milestone = static_cast<int>(affectionToPlayer_ / 25.0F);
        if (milestone > lastBondMilestone_ && milestone > 0) {
            lastBondMilestone_ = milestone;
            remember(EventType::BondMilestone, 25, 55);
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
        const float target = emotions_.value(Emotion::Joy) * 0.48F
            + emotions_.value(Emotion::Affection) * 0.24F
            - emotions_.value(Emotion::Sadness) * 0.72F
            - emotions_.value(Emotion::Fear) * 0.42F
            - emotions_.value(Emotion::Anger) * 0.34F
            - emotions_.value(Emotion::Disgust) * 0.30F
            - needs_.hunger * 0.10F - needs_.boredom * 0.06F;
        const float blend = clampValue(dt * 0.55F, 0.0F, 1.0F);
        mood_ += (clampValue(target, -100.0F, 100.0F) - mood_) * blend;
    }

    Genome genome_;
    CreatureNeeds needs_{};
    EmotionalState emotions_{};
    MemoryJournal memories_{};
    Action action_ = Action::Idle;
    float mood_ = 0.0F;
    float affectionToPlayer_ = 8.0F;
    float simulationTime_ = 0.0F;
    float actionTimer_ = 0.0F;
    std::uint32_t noveltyCount_ = 0;
    std::uint32_t foregroundVisits_ = 0;
    int lastBondMilestone_ = 0;
    std::mt19937 random_;
};
}
