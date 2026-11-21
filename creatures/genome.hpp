#pragma once
#include "core/types.hpp"
#include <cstdint>
namespace evolvinggarden {
struct PersonalityGenes {
    std::int16_t friendliness = 42;
    std::int16_t optimism = 38;
    std::int16_t sociability = 18;
    std::int16_t curiosity = 72;
    std::int16_t activity = 30;
    std::int16_t sensitivity = 24;
    void normalize() {
        friendliness = clampTrait(friendliness); optimism = clampTrait(optimism);
        sociability = clampTrait(sociability); curiosity = clampTrait(curiosity);
        activity = clampTrait(activity); sensitivity = clampTrait(sensitivity);
    }
};
struct MorphologyGenes {
    std::uint8_t bodyWidth = 55, bodyHeight = 48, roundness = 64, asymmetry = 42;
    std::uint8_t surfaceComplexity = 44, appendageCount = 3, appendageLength = 52;
    std::uint8_t appendageCurvature = 58, eyeSize = 55, eyeSpacing = 48;
    std::uint8_t pigmentation = 62, patternDensity = 38, palette = 2;
    void normalize() {
        bodyWidth = clampValue<std::uint8_t>(bodyWidth, 10, 100);
        bodyHeight = clampValue<std::uint8_t>(bodyHeight, 10, 100);
        roundness = clampValue<std::uint8_t>(roundness, 0, 100);
        asymmetry = clampValue<std::uint8_t>(asymmetry, 0, 100);
        surfaceComplexity = clampValue<std::uint8_t>(surfaceComplexity, 0, 100);
        appendageCount = clampValue<std::uint8_t>(appendageCount, 0, 6);
        appendageLength = clampValue<std::uint8_t>(appendageLength, 0, 100);
        appendageCurvature = clampValue<std::uint8_t>(appendageCurvature, 0, 100);
        eyeSize = clampValue<std::uint8_t>(eyeSize, 10, 100);
        eyeSpacing = clampValue<std::uint8_t>(eyeSpacing, 0, 100);
        pigmentation = clampValue<std::uint8_t>(pigmentation, 0, 100);
        patternDensity = clampValue<std::uint8_t>(patternDensity, 0, 100);
        palette = static_cast<std::uint8_t>(palette % 5);
    }
};
struct Genome {
    std::uint32_t seed = 0xE701A11U;
    PersonalityGenes personality{};
    MorphologyGenes morphology{};
    void normalize() { personality.normalize(); morphology.normalize(); }
};
inline Genome makeDefaultGenome() {
    Genome genome{};
    genome.seed = 0xE701A11U;
    genome.personality = PersonalityGenes{42, 38, 18, 72, 30, 24};
    genome.morphology = MorphologyGenes{55, 48, 64, 42, 44, 3, 52, 58, 55, 48, 62, 38, 2};
    return genome;
}
}
