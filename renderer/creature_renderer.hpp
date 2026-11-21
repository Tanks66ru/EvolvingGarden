#pragma once
#include "simulation/creature_simulation.hpp"
#include "raylib.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <random>
namespace evolvinggarden::renderer {
inline Color bodyColorFor(std::uint8_t palette) {
    constexpr std::array<Color, 5> colors{
        Color{130, 194, 171, 255}, Color{232, 164, 126, 255},
        Color{147, 166, 219, 255}, Color{201, 153, 209, 255},
        Color{220, 195, 117, 255}
    };
    return colors[palette % colors.size()];
}
inline Color darken(Color color, float amount) {
    const float factor = 1.0F - amount;
    return Color{
        static_cast<unsigned char>(static_cast<float>(color.r) * factor),
        static_cast<unsigned char>(static_cast<float>(color.g) * factor),
        static_cast<unsigned char>(static_cast<float>(color.b) * factor),
        color.a
    };
}
inline float emotion(const CreatureSimulation& creature, Emotion value) {
    return creature.emotions().value(value);
}
inline void drawAppendages(
    Vector2 center,
    const Genome& genome,
    float time,
    Action action,
    Reaction reaction,
    float reactionPower)
{
    constexpr float Pi = 3.14159265358979323846F;
    const auto& genes = genome.morphology;
    const int count = static_cast<int>(genes.appendageCount);
    if (count <= 0) return;

    const float widthScale = 0.82F + static_cast<float>(genes.bodyWidth) / 100.0F * 0.38F;
    const float heightScale = 0.82F + static_cast<float>(genes.bodyHeight) / 100.0F * 0.42F;
    const float length = 24.0F + static_cast<float>(genes.appendageLength) * 0.62F;
    const float curvature = static_cast<float>(genes.appendageCurvature) / 100.0F;
    const Color color = bodyColorFor(genes.palette);
    const Color outline = darken(color, 0.25F);

    float activity = 1.0F;
    if (action == Action::Rest || action == Action::Idle) activity = 0.45F;
    if (action == Action::Play) activity = 1.65F;
    if (action == Action::Explore) activity = 1.3F;
    float amplitude = 1.0F;
    if (reaction == Reaction::Happy || reaction == Reaction::Curious) amplitude = 1.5F;
    if (reaction == Reaction::Fearful || reaction == Reaction::Angry) amplitude = 1.9F;
    if (reaction == Reaction::Affectionate) amplitude = 0.65F;

    for (int i = 0; i < count; ++i) {
        const float index = static_cast<float>(i);
        const float angle = -Pi * 0.5F + (2.0F * Pi * index) / static_cast<float>(count);
        float sway = std::sin(time * (1.25F + index * 0.16F) + index * 1.7F)
            * (3.0F + curvature * 13.0F) * activity * amplitude;
        sway += std::sin(time * 8.0F + index * 2.2F) * reactionPower * 7.0F;
        if (reaction == Reaction::Fearful || reaction == Reaction::Surprised)
            sway += std::sin(time * 23.0F + index) * reactionPower * 8.0F;

        const Vector2 direction{std::cos(angle), std::sin(angle)};
        const Vector2 perpendicular{-direction.y, direction.x};
        const Vector2 start{
            center.x + direction.x * 74.0F * widthScale,
            center.y + direction.y * 74.0F * heightScale
        };
        const Vector2 end{
            start.x + direction.x * length + perpendicular.x * sway,
            start.y + direction.y * length + perpendicular.y * sway
        };
        const Vector2 control{
            (start.x + end.x) * 0.5F + perpendicular.x * (curvature * 24.0F + sway * 0.35F),
            (start.y + end.y) * 0.5F + perpendicular.y * (curvature * 24.0F + sway * 0.35F)
        };

        Vector2 previous = start;
        constexpr int CurveSegments = 10;
        for (int segment = 1; segment <= CurveSegments; ++segment) {
            const float t = static_cast<float>(segment) / static_cast<float>(CurveSegments);
            const float inverse = 1.0F - t;
            const Vector2 point{
                inverse * inverse * start.x + 2.0F * inverse * t * control.x + t * t * end.x,
                inverse * inverse * start.y + 2.0F * inverse * t * control.y + t * t * end.y
            };
            const float progress = static_cast<float>(segment - 1) / static_cast<float>(CurveSegments);
            const float thickness = 12.5F * (1.0F - progress * 0.58F);
            DrawLineEx(previous, point, thickness + 3.0F, outline);
            DrawLineEx(previous, point, thickness, color);
            previous = point;
        }
        DrawCircleV(end, 5.5F, darken(color, 0.18F));
    }
}

inline void drawEye(
    Vector2 center,
    float eyeRadius,
    float eyeOpen,
    float pupilX,
    float pupilY,
    float pupilScale,
    int side,
    bool angry,
    bool sad,
    bool surprised,
    bool happy,
    bool disgusted)
{
    const Color dark = Color{48, 53, 62, 255};
    if (happy && eyeOpen < 0.58F) {
        DrawLineEx(
            {center.x - eyeRadius, center.y + 1.0F},
            {center.x, center.y + eyeRadius * 0.25F},
            3.4F, dark);
        DrawLineEx(
            {center.x, center.y + eyeRadius * 0.25F},
            {center.x + eyeRadius, center.y + 1.0F},
            3.4F, dark);
    } else {
        const float visibleHeight = eyeRadius * eyeOpen;
        DrawEllipse(
            static_cast<int>(center.x),
            static_cast<int>(center.y),
            eyeRadius,
            std::max(1.0F, visibleHeight),
            RAYWHITE
        );
        if (eyeOpen > 0.20F) {
            const float pupilRadius = eyeRadius * 0.47F * pupilScale;
            DrawEllipse(
                static_cast<int>(center.x + pupilX),
                static_cast<int>(center.y + pupilY),
                pupilRadius,
                std::min(visibleHeight * 0.72F, pupilRadius),
                dark
            );
            DrawCircleV(
                {center.x + pupilX - pupilRadius * 0.34F,
                 center.y + pupilY - pupilRadius * 0.37F},
                std::max(1.0F, pupilRadius * 0.20F),
                RAYWHITE
            );
        }
    }

    if (angry) {
        const Vector2 outer{center.x + static_cast<float>(side) * eyeRadius * 1.05F,
                            center.y - eyeRadius * 1.25F};
        const Vector2 inner{center.x - static_cast<float>(side) * eyeRadius * 1.05F,
                            center.y - eyeRadius * 0.48F};
        DrawLineEx(outer, inner, 4.0F, dark);
    } else if (sad) {
        const Vector2 outer{center.x + static_cast<float>(side) * eyeRadius,
                            center.y - eyeRadius * 0.75F};
        const Vector2 inner{center.x - static_cast<float>(side) * eyeRadius,
                            center.y - eyeRadius * 1.25F};
        DrawLineEx(outer, inner, 3.0F, Color{70, 79, 91, 255});
    } else if (surprised) {
        DrawLineEx(
            {center.x - eyeRadius * 0.75F, center.y - eyeRadius * 1.55F},
            {center.x + eyeRadius * 0.75F, center.y - eyeRadius * 1.55F},
            2.4F, dark);
    } else if (disgusted && side == 1) {
        DrawLineEx(
            {center.x - eyeRadius, center.y - eyeRadius * 0.75F},
            {center.x + eyeRadius, center.y - eyeRadius * 1.1F},
            3.0F, dark);
    }
}

inline void drawProceduralCreature(
    Vector2 center,
    const CreatureSimulation& creature,
    float time)
{
    constexpr float Pi = 3.14159265358979323846F;
    constexpr int Segments = 56;
    const Genome& genome = creature.genome();
    const auto& genes = genome.morphology;
    const auto& reactionState = creature.reaction();
    const Reaction reaction = reactionState.kind();
    const float reactionPower = reactionState.intensity();
    const bool superReaction = reactionState.exaggerated();

    const float joy = std::max(emotion(creature, Emotion::Joy),
                               emotion(creature, Emotion::Affection) * 0.82F);
    const float sadness = emotion(creature, Emotion::Sadness);
    const float anger = emotion(creature, Emotion::Anger);
    const float fear = emotion(creature, Emotion::Fear);
    const float surprise = emotion(creature, Emotion::Surprise);
    const float curiosity = emotion(creature, Emotion::Curiosity);
    const float disgust = emotion(creature, Emotion::Disgust);

    const bool happyFace = joy > 35.0F || reaction == Reaction::Happy || reaction == Reaction::Affectionate;
    const bool sadFace = sadness > 28.0F || reaction == Reaction::Sad;
    const bool angryFace = anger > 18.0F || reaction == Reaction::Angry;
    const bool fearfulFace = fear > 32.0F || reaction == Reaction::Fearful;
    const bool surprisedFace = surprise > 28.0F || reaction == Reaction::Surprised;
    const bool curiousFace = curiosity > 38.0F || reaction == Reaction::Curious;
    const bool disgustedFace = disgust > 28.0F || reaction == Reaction::Disgusted;

    float offsetX = std::sin(time * 0.52F) * 2.0F;
    float offsetY = std::sin(time * 2.0F) * 2.6F;
    float squash = std::sin(time * 1.7F) * 0.012F;
    float stretch = 0.0F;
    float lean = 0.0F;

    switch (creature.action()) {
    case Action::Explore:
        offsetX += std::sin(time * 1.4F) * 5.0F;
        offsetY -= std::abs(std::sin(time * 3.3F)) * 2.0F;
        break;
    case Action::Play:
        offsetY -= std::abs(std::sin(time * 5.5F)) * 7.0F;
        squash += std::sin(time * 5.5F) * 0.025F;
        stretch += std::sin(time * 5.5F) * 0.035F;
        break;
    case Action::Rest:
        offsetY += std::sin(time * 0.85F) * 1.5F;
        squash -= 0.025F;
        break;
    case Action::Observe:
        offsetX += std::sin(time * 0.75F) * 2.5F;
        break;
    case Action::SeekCompany:
        offsetX += std::sin(time * 1.25F) * 3.0F;
        break;
    case Action::Forage:
        offsetY += 2.0F + std::sin(time * 2.8F) * 1.5F;
        break;
    case Action::Idle:
        break;
    }

    switch (reaction) {
    case Reaction::Happy:
        offsetY -= 13.0F * reactionPower;
        squash += 0.035F * reactionPower;
        stretch -= 0.025F * reactionPower;
        break;
    case Reaction::Sad:
        offsetY += 8.0F * reactionPower;
        lean -= 4.0F * reactionPower;
        squash -= 0.025F * reactionPower;
        break;
    case Reaction::Angry:
        offsetX += std::sin(time * 25.0F) * 5.0F * reactionPower;
        squash += 0.035F * reactionPower;
        break;
    case Reaction::Fearful:
        offsetY -= 17.0F * reactionPower;
        offsetX += std::sin(time * 25.0F) * 3.0F * reactionPower;
        squash -= 0.045F * reactionPower;
        stretch += 0.035F * reactionPower;
        break;
    case Reaction::Surprised:
        offsetY -= 10.0F * reactionPower;
        stretch += 0.035F * reactionPower;
        break;
    case Reaction::Curious:
        offsetX += 5.0F * reactionPower;
        offsetY -= 3.0F * reactionPower;
        lean = 8.0F * reactionPower;
        break;
    case Reaction::Affectionate:
        offsetY -= 5.0F * reactionPower;
        offsetX += std::sin(time * 8.0F) * 2.5F * reactionPower;
        break;
    case Reaction::Disgusted:
        offsetX -= 8.0F * reactionPower;
        lean -= 9.0F * reactionPower;
        break;
    case Reaction::None:
        break;
    }
    if (superReaction) {
        offsetY += std::sin(time * 18.0F) * 5.0F * reactionPower;
        squash += std::sin(time * 18.0F) * 0.035F * reactionPower;
    }

    center.x += offsetX;
    center.y += offsetY;

    const float widthScale = 0.82F + static_cast<float>(genes.bodyWidth) / 100.0F * 0.38F;
    const float heightScale = 0.82F + static_cast<float>(genes.bodyHeight) / 100.0F * 0.42F;
    const float radiusX = 112.0F * widthScale * (1.0F + squash + stretch);
    const float radiusY = 101.0F * heightScale * (1.0F - squash - stretch * 0.65F);
    const float roundness = static_cast<float>(genes.roundness) / 100.0F;
    const float asymmetry = static_cast<float>(genes.asymmetry) / 100.0F;
    const float complexity = static_cast<float>(genes.surfaceComplexity) / 100.0F;
    const float activity = static_cast<float>(genes.appendageCurvature) / 100.0F;
    const float irregularity = (0.025F + asymmetry * 0.065F + complexity * 0.075F)
        * (1.0F - roundness * 0.56F);
    const float phase = static_cast<float>(genome.seed % 997U) * 0.013F;
    const Color bodyColor = bodyColorFor(genes.palette);
    const Color outlineColor = darken(bodyColor, 0.28F);

    DrawEllipse(
        static_cast<int>(center.x),
        static_cast<int>(center.y + radiusY * 0.88F),
        radiusX * 0.82F,
        17.0F,
        Fade(BLACK, 0.22F)
    );
    drawAppendages(center, genome, time, creature.action(), reaction, reactionPower);

    std::array<Vector2, Segments + 2> fan{};
    std::array<Vector2, Segments> contour{};
    fan[0] = center;
    for (int i = 0; i < Segments; ++i) {
        const float angle = (2.0F * Pi * static_cast<float>(i)) / static_cast<float>(Segments);
        const float wave = std::cos(2.0F * angle + phase) * 0.42F
            + std::sin(3.0F * angle + phase * 1.7F) * 0.34F
            + std::cos(5.0F * angle - phase * 0.8F) * 0.24F;
        const float breathing = std::sin(time * 1.8F + angle * 2.0F + phase)
            * (0.006F + activity * 0.006F);
        const float ripple = superReaction
            ? std::sin(time * 22.0F + angle * 5.0F) * reactionPower * 0.012F : 0.0F;
        const float radius = 1.0F + irregularity * wave + breathing + ripple;
        const Vector2 point{
            center.x + std::cos(angle) * radiusX * radius,
            center.y + std::sin(angle) * radiusY * radius
        };
        contour[static_cast<std::size_t>(i)] = point;
        fan[static_cast<std::size_t>(i) + 1] = point;
    }
    fan[Segments + 1] = contour[0];
    DrawTriangleFan(fan.data(), static_cast<int>(fan.size()), bodyColor);
    for (int i = 0; i < Segments; ++i) {
        const Vector2 from = contour[static_cast<std::size_t>(i)];
        const Vector2 to = contour[static_cast<std::size_t>((i + 1) % Segments)];
        DrawLineEx(from, to, 3.0F, outlineColor);
    }

    std::mt19937 spotRandom(genome.seed ^ 0x51A7U);
    std::uniform_real_distribution<float> angleDistribution(0.0F, 2.0F * Pi);
    std::uniform_real_distribution<float> radiusDistribution(0.18F, 0.68F);
    const int spotCount = static_cast<int>(genes.patternDensity / 7U);
    for (int i = 0; i < spotCount; ++i) {
        const float angle = angleDistribution(spotRandom);
        const float radius = radiusDistribution(spotRandom);
        const Vector2 spot{
            center.x + std::cos(angle) * radiusX * radius,
            center.y + std::sin(angle) * radiusY * radius
        };
        DrawCircleV(spot, 2.5F + static_cast<float>(i % 3) * 1.2F,
            Fade(darken(bodyColor, 0.3F), 0.45F));
    }

    const float eyeScale = 0.76F + static_cast<float>(genes.eyeSize) / 100.0F * 0.52F;
    const float eyeSpacing = 27.0F + static_cast<float>(genes.eyeSpacing) * 0.31F;
    const float eyeRadius = 8.8F * eyeScale;
    float gazeX = std::sin(time * 0.58F + phase) * (2.0F + curiosity * 0.035F);
    float gazeY = std::cos(time * 0.41F + phase * 0.7F) * 1.8F;

    switch (creature.action()) {
    case Action::Explore: gazeX += std::sin(time * 1.2F) * 5.5F; gazeY -= 1.5F; break;
    case Action::Observe: gazeX += std::sin(time * 0.86F) * 7.0F; break;
    case Action::Forage: gazeY += 3.0F; break;
    case Action::SeekCompany: gazeX -= 3.5F; break;
    case Action::Play: gazeX += std::sin(time * 4.0F) * 3.0F; break;
    case Action::Idle:
    case Action::Rest: break;
    }
    if (curiousFace && reaction != Reaction::Curious) gazeX += std::sin(time * 0.45F + phase) * 2.5F;
    if (reaction == Reaction::Curious) gazeX += 7.0F * reactionPower;
    if (reaction == Reaction::Fearful || fearfulFace) gazeY -= 3.0F;
    if (reaction == Reaction::Affectionate) gazeY += 2.0F;

    const float blinkCycle = std::fmod(time + phase, 4.8F);
    const float blinkAmount = clampValue(1.0F - std::abs(blinkCycle - 4.48F) / 0.12F, 0.0F, 1.0F);
    float eyeOpen = 1.0F;
    if (angryFace) eyeOpen = 0.62F;
    else if (sadFace) eyeOpen = 0.82F;
    else if (surprisedFace || fearfulFace) eyeOpen = 1.13F;
    else if (happyFace) eyeOpen = 0.48F;
    eyeOpen *= 1.0F - blinkAmount * 0.96F;
    const float pupilScale = (fearfulFace || surprisedFace) ? 0.78F : 1.0F;

    for (int side = -1; side <= 1; side += 2) {
        const float sideOffset = static_cast<float>(side);
        const Vector2 eyeCenter{
            center.x + sideOffset * eyeSpacing + lean * 0.18F * sideOffset,
            center.y - 10.0F + (sadFace ? 2.0F : 0.0F) + lean * sideOffset * 0.12F
        };
        drawEye(eyeCenter, eyeRadius, eyeOpen, gazeX, gazeY, pupilScale, side,
            angryFace, sadFace, surprisedFace, happyFace, disgustedFace);
    }

    const float mouthY = center.y + 29.0F;
    const float mouthWidth = 10.0F + static_cast<float>(genes.eyeSize) * 0.025F;
    if (surprisedFace || fearfulFace) {
        const float mouthRadius = superReaction ? 9.0F : (surprisedFace ? 7.0F : 5.5F);
        DrawEllipse(static_cast<int>(center.x), static_cast<int>(mouthY),
            mouthRadius, mouthRadius * (fearfulFace ? 1.25F : 1.0F), Color{55, 58, 66, 255});
        DrawEllipse(static_cast<int>(center.x), static_cast<int>(mouthY - 1.0F),
            mouthRadius * 0.55F, mouthRadius * 0.65F, darken(bodyColor, 0.18F));
    } else if (disgustedFace) {
        DrawLineEx({center.x - mouthWidth, mouthY + 1.0F},
            {center.x, mouthY - 4.0F}, 3.0F, Color{55, 58, 66, 255});
        DrawLineEx({center.x, mouthY - 4.0F},
            {center.x + mouthWidth * 0.75F, mouthY - 1.0F}, 3.0F, Color{55, 58, 66, 255});
    } else {
        float curve = creature.mood() > 12.0F ? 5.5F : -3.5F;
        if (angryFace) curve = -5.5F;
        if (sadFace) curve = -6.0F;
        if (happyFace) curve = 7.0F;
        const Color mouthColor{55, 58, 66, 255};
        const Vector2 left{center.x - mouthWidth, mouthY};
        const Vector2 middle{center.x, mouthY + curve};
        const Vector2 right{center.x + mouthWidth, mouthY};
        DrawLineEx(left, middle, 2.8F, mouthColor);
        DrawLineEx(middle, right, 2.8F, mouthColor);
    }

    if (emotion(creature, Emotion::Affection) > 30.0F || happyFace) {
        const Color cheek = Fade(Color{222, 125, 133, 255},
            clampValue((joy / 100.0F) * 0.68F, 0.0F, 0.68F));
        DrawEllipse(static_cast<int>(center.x - eyeSpacing - 10.0F),
            static_cast<int>(center.y + 13.0F), 7.0F, 3.0F, cheek);
        DrawEllipse(static_cast<int>(center.x + eyeSpacing + 10.0F),
            static_cast<int>(center.y + 13.0F), 7.0F, 3.0F, cheek);
    }
}
}
