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
inline void drawAppendages(Vector2 center, const Genome& genome, float time) {
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

    for (int i = 0; i < count; ++i) {
        const float angle = -Pi * 0.5F + (2.0F * Pi * static_cast<float>(i)) / static_cast<float>(count);
        const float sway = std::sin(time * (1.1F + static_cast<float>(i) * 0.13F) + static_cast<float>(i))
            * (2.0F + curvature * 7.0F);
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
            (start.x + end.x) * 0.5F + perpendicular.x * (curvature * 24.0F),
            (start.y + end.y) * 0.5F + perpendicular.y * (curvature * 24.0F)
        };

        Vector2 previous = start;
        constexpr int CurveSegments = 9;
        for (int segment = 1; segment <= CurveSegments; ++segment) {
            const float t = static_cast<float>(segment) / static_cast<float>(CurveSegments);
            const float inverse = 1.0F - t;
            const Vector2 point{
                inverse * inverse * start.x + 2.0F * inverse * t * control.x + t * t * end.x,
                inverse * inverse * start.y + 2.0F * inverse * t * control.y + t * t * end.y
            };
            const float progress = static_cast<float>(segment - 1) / static_cast<float>(CurveSegments);
            const float thickness = 12.0F * (1.0F - progress * 0.58F);
            DrawLineEx(previous, point, thickness + 3.0F, outline);
            DrawLineEx(previous, point, thickness, color);
            previous = point;
        }
        DrawCircleV(end, 5.0F, darken(color, 0.18F));
    }
}
inline void drawProceduralCreature(Vector2 center, const CreatureSimulation& creature, float time) {
    constexpr float Pi = 3.14159265358979323846F;
    constexpr int Segments = 48;
    const Genome& genome = creature.genome();
    const auto& genes = genome.morphology;
    center.y += std::sin(time * 2.1F) * 4.0F;

    const float widthScale = 0.82F + static_cast<float>(genes.bodyWidth) / 100.0F * 0.38F;
    const float heightScale = 0.82F + static_cast<float>(genes.bodyHeight) / 100.0F * 0.42F;
    const float radiusX = 112.0F * widthScale;
    const float radiusY = 101.0F * heightScale;
    const float roundness = static_cast<float>(genes.roundness) / 100.0F;
    const float asymmetry = static_cast<float>(genes.asymmetry) / 100.0F;
    const float complexity = static_cast<float>(genes.surfaceComplexity) / 100.0F;
    const float irregularity = (0.025F + asymmetry * 0.065F + complexity * 0.075F) * (1.0F - roundness * 0.56F);
    const float phase = static_cast<float>(genome.seed % 997U) * 0.013F;
    const Color bodyColor = bodyColorFor(genes.palette);
    const Color outlineColor = darken(bodyColor, 0.28F);

    DrawEllipse(static_cast<int>(center.x), static_cast<int>(center.y + radiusY * 0.88F),
        radiusX * 0.82F, 17.0F, Fade(BLACK, 0.22F));
    drawAppendages(center, genome, time);

    std::array<Vector2, Segments + 2> fan{};
    std::array<Vector2, Segments> contour{};
    fan[0] = center;
    for (int i = 0; i < Segments; ++i) {
        const float angle = (2.0F * Pi * static_cast<float>(i)) / static_cast<float>(Segments);
        const float wave = std::cos(2.0F * angle + phase) * 0.42F
            + std::sin(3.0F * angle + phase * 1.7F) * 0.34F
            + std::cos(5.0F * angle - phase * 0.8F) * 0.24F;
        const float radius = 1.0F + irregularity * wave;
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

    // Deterministic spots: the same genome always produces the same pattern.
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
        const float size = 2.5F + static_cast<float>(i % 3) * 1.2F;
        DrawCircleV(spot, size, Fade(darken(bodyColor, 0.3F), 0.45F));
    }

    const float eyeScale = 0.72F + static_cast<float>(genes.eyeSize) / 100.0F * 0.58F;
    const float eyeSpacing = 27.0F + static_cast<float>(genes.eyeSpacing) * 0.31F;
    const float eyeY = center.y - 10.0F;
    const float eyeRadius = 8.0F * eyeScale;
    float pupilOffsetX = 0.0F;
    float pupilOffsetY = 0.0F;
    switch (creature.action()) {
    case Action::Explore:
    case Action::Observe: pupilOffsetX = 2.8F; pupilOffsetY = -1.0F; break;
    case Action::Forage: pupilOffsetY = 2.0F; break;
    case Action::SeekCompany: pupilOffsetX = -2.2F; break;
    default: break;
    }

    const bool blinking = std::sin(time * 0.73F + phase) > 0.9985F;
    for (int side = -1; side <= 1; side += 2) {
        const Vector2 eyeCenter{center.x + static_cast<float>(side) * eyeSpacing, eyeY};
        if (blinking) {
            DrawLineEx({eyeCenter.x - eyeRadius, eyeCenter.y},
                {eyeCenter.x + eyeRadius, eyeCenter.y}, 3.0F, DARKGRAY);
            continue;
        }
        DrawCircleV(eyeCenter, eyeRadius, RAYWHITE);
        DrawCircleV({eyeCenter.x + pupilOffsetX, eyeCenter.y + pupilOffsetY},
            eyeRadius * 0.48F, Color{35, 43, 49, 255});
        DrawCircleV({eyeCenter.x + pupilOffsetX - 2.0F, eyeCenter.y + pupilOffsetY - 2.5F},
            eyeRadius * 0.16F, RAYWHITE);
    }

    if (creature.emotions().value(Emotion::Fear) > 42.0F) {
        DrawCircleLines(static_cast<int>(center.x), static_cast<int>(center.y + 31.0F),
            8.0F, darken(bodyColor, 0.60F));
    } else {
        const float smile = creature.mood() > 12.0F ? 1.0F : (creature.mood() < -18.0F ? -0.45F : 0.25F);
        const Vector2 left{center.x - 11.0F, center.y + 27.0F};
        const Vector2 middle{center.x, center.y + 27.0F + smile * 6.0F};
        const Vector2 right{center.x + 11.0F, center.y + 27.0F};
        DrawLineEx(left, middle, 2.7F, darken(bodyColor, 0.65F));
        DrawLineEx(middle, right, 2.7F, darken(bodyColor, 0.65F));
    }
}
}
