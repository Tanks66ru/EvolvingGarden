#include "renderer/creature_renderer.hpp"
#include "raylib.h"
#include <algorithm>
#include <array>
using namespace evolvinggarden;
namespace {
constexpr int WindowWidth = 1180;
constexpr int WindowHeight = 760;
constexpr Color Background{21, 25, 32, 255};
constexpr Color Panel{31, 37, 46, 255};
constexpr Color PanelLight{39, 47, 58, 255};
constexpr Color TextPrimary{235, 239, 242, 255};
constexpr Color TextMuted{145, 158, 171, 255};
constexpr Color Accent{112, 190, 163, 255};

void drawPanel(Rectangle bounds, const char* title) {
    DrawRectangleRounded(bounds, 0.04F, 10, Panel);
    DrawText(title, static_cast<int>(bounds.x + 18.0F), static_cast<int>(bounds.y + 15.0F), 16, TextPrimary);
}
void drawTraitBar(const char* label, int value, int x, int y, int width) {
    DrawText(label, x, y, 13, TextMuted);
    DrawText(TextFormat("%+d", value), x + width - 38, y, 13, TextPrimary);
    const int barY = y + 20;
    const int middle = x + width / 2;
    const int halfWidth = width / 2 - 2;
    DrawRectangle(x, barY, width, 7, PanelLight);
    DrawLine(middle, barY - 2, middle, barY + 9, TextMuted);
    if (value >= 0) {
        const int fill = static_cast<int>(static_cast<float>(halfWidth) * value / 100.0F);
        DrawRectangle(middle, barY, fill, 7, Accent);
    } else {
        const int fill = static_cast<int>(static_cast<float>(halfWidth) * -value / 100.0F);
        DrawRectangle(middle - fill, barY, fill, 7, Color{213, 125, 112, 255});
    }
}
void drawValueBar(const char* label, float value, int x, int y, int width, Color color) {
    DrawText(label, x, y, 13, TextMuted);
    DrawText(TextFormat("%d", static_cast<int>(value)), x + width - 30, y, 13, TextPrimary);
    DrawRectangle(x, y + 19, width, 8, PanelLight);
    DrawRectangle(x, y + 19, static_cast<int>(static_cast<float>(width) * clampPercent(value) / 100.0F), 8, color);
}
void drawPersonalityPanel(const CreatureSimulation& creature) {
    drawPanel({24.0F, 100.0F, 250.0F, 548.0F}, "PERSONALITY");
    const auto& genes = creature.genome().personality;
    drawTraitBar("Friendliness", genes.friendliness, 42, 145, 214);
    drawTraitBar("Optimism", genes.optimism, 42, 207, 214);
    drawTraitBar("Sociability", genes.sociability, 42, 269, 214);
    drawTraitBar("Curiosity", genes.curiosity, 42, 331, 214);
    drawTraitBar("Activity", genes.activity, 42, 393, 214);
    drawTraitBar("Sensitivity", genes.sensitivity, 42, 455, 214);
    DrawText("INHERITED TRAITS", 42, 532, 11, TextMuted);
    DrawText("Genome seed:", 42, 554, 12, TextMuted);
    DrawText(TextFormat("%08X", creature.genome().seed), 42, 574, 14, TextPrimary);
    DrawText("Values are prototype defaults", 42, 619, 10, TextMuted);
}
void drawNeedsAndEmotions(const CreatureSimulation& creature) {
    drawPanel({906.0F, 100.0F, 250.0F, 548.0F}, "STATE");
    const auto& needs = creature.needs();
    drawValueBar("Hunger", needs.hunger, 924, 145, 212, Color{219, 160, 94, 255});
    drawValueBar("Energy", needs.energy, 924, 192, 212, Color{112, 174, 219, 255});
    drawValueBar("Social", needs.social, 924, 239, 212, Color{174, 145, 214, 255});
    drawValueBar("Boredom", needs.boredom, 924, 286, 212, Color{211, 186, 105, 255});
    DrawText("EMOTIONS", 924, 342, 13, TextPrimary);
    constexpr std::array<Emotion, 6> shown{
        Emotion::Joy, Emotion::Sadness, Emotion::Anger,
        Emotion::Fear, Emotion::Curiosity, Emotion::Affection
    };
    constexpr std::array<const char*, 6> labels{
        "Joy", "Sadness", "Anger", "Fear", "Curiosity", "Affection"
    };
    for (std::size_t i = 0; i < shown.size(); ++i) {
        const float value = creature.emotions().value(shown[i]);
        const int y = 370 + static_cast<int>(i) * 34;
        DrawText(labels[i], 924, y, 12, TextMuted);
        DrawRectangle(1003, y + 3, 133, 7, PanelLight);
        DrawRectangle(1003, y + 3, static_cast<int>(133.0F * value / 100.0F), 7, Accent);
    }
    DrawText(TextFormat("Mood: %+.0f", creature.mood()), 924, 590, 13, TextPrimary);
    DrawText(TextFormat("Bond: %.1f / 100", creature.affectionToPlayer()), 924, 612, 13, TextPrimary);
}
void drawHabitat(const CreatureSimulation& creature, float time) {
    drawPanel({290.0F, 100.0F, 600.0F, 548.0F}, "LIVE HABITAT");
    DrawText(creature.isInForeground()
        ? "FOREGROUND  /  INDIVIDUAL ATTENTION"
        : "ON FARM  /  LIVING IN THE HABITAT",
        312, 140, 11, creature.isInForeground() ? Accent : TextMuted);
    DrawCircleGradient(590, 366, 190.0F, Color{49, 65, 72, 255}, Color{31, 37, 46, 255});
    DrawLine(325, 550, 855, 550, Color{68, 82, 91, 255});
    renderer::drawProceduralCreature({590.0F, 355.0F}, creature, time);
    DrawText(actionName(creature.action()), 312, 585, 16, TextPrimary);
    DrawText(TextFormat("Mood %+.0f   |   Arousal %.0f", creature.mood(), creature.arousal()),
        312, 611, 12, TextMuted);
}
void drawRecentMemory(const CreatureSimulation& creature) {
    const auto& journal = creature.memories();
    DrawText(TextFormat("CHRONICLE  /  %d EVENTS", static_cast<int>(journal.size())),
        312, 676, 12, TextMuted);
    if (journal.size() > 0)
        DrawText(eventTitle(journal.at(journal.size() - 1).type).data(), 500, 676, 12, Accent);
}
void drawChronicle(const CreatureSimulation& creature, int scrollOffset) {
    DrawRectangleRounded({24.0F, 100.0F, 1132.0F, 590.0F}, 0.025F, 10, Panel);
    DrawText("LIFE CHRONICLE", 50, 126, 20, TextPrimary);
    DrawText("Only standout moments are kept. Maximum one event per 10 simulated minutes.", 50, 158, 13, TextMuted);
    const auto& journal = creature.memories();
    if (journal.size() == 0) {
        DrawText("No significant events yet.", 50, 220, 16, TextMuted);
        return;
    }
    constexpr int VisibleEntries = 7;
    const int maxOffset = std::max(0, static_cast<int>(journal.size()) - VisibleEntries);
    const int safeOffset = std::clamp(scrollOffset, 0, maxOffset);
    const int endIndex = static_cast<int>(journal.size()) - safeOffset;
    const int startIndex = std::max(0, endIndex - VisibleEntries);
    int y = 198;
    for (int index = endIndex - 1; index >= startIndex; --index) {
        const MemoryEvent& event = journal.at(static_cast<std::size_t>(index));
        DrawRectangleRounded({48.0F, static_cast<float>(y), 1080.0F, 58.0F}, 0.08F, 8, PanelLight);
        const int day = static_cast<int>(event.timestampSeconds / 86400U) + 1;
        const int totalMinutes = static_cast<int>(event.timestampSeconds / 60U);
        const int hours = (totalMinutes / 60) % 24;
        const int minutes = totalMinutes % 60;
        DrawText(TextFormat("DAY %d  %02d:%02d", day, hours, minutes), 66, y + 8, 10, TextMuted);
        DrawText(eventTitle(event.type).data(), 220, y + 7, 15, TextPrimary);
        DrawText(eventDescription(event.type).data(), 220, y + 31, 12, TextMuted);
        DrawText(TextFormat("IMPORTANCE %d", event.significance), 982, y + 19, 10, Accent);
        y += 68;
    }
    DrawText(TextFormat("Showing %d-%d of %d",
        startIndex + 1, endIndex, static_cast<int>(journal.size())), 50, 675, 11, TextMuted);
}
void drawControls(bool chronicleOpen, bool paused) {
    DrawText("F  FEED", 30, 715, 12, TextPrimary);
    DrawText("P  PET", 135, 715, 12, TextPrimary);
    DrawText("N  NOVELTY", 235, 715, 12, TextPrimary);
    DrawText("S  SOUND", 365, 715, 12, TextPrimary);
    DrawText("B  MAIN SCREEN", 465, 715, 11, TextPrimary);
    DrawText("R  RETURN TO FARM", 595, 715, 11, TextPrimary);
    DrawText("H  CHRONICLE", 750, 715, 11, TextPrimary);
    DrawText(paused ? "SPACE  RESUME" : "SPACE  PAUSE", 890, 715, 11, Accent);
    if (chronicleOpen) DrawText("UP/DOWN SCROLL  |  BACKSPACE RETURN", 965, 715, 9, TextPrimary);
}
}
int main() {
    SetConfigFlags(FLAG_VSYNC_HINT);
    InitWindow(WindowWidth, WindowHeight, "EvolvingGarden | Behavior Prototype 0.2");
    SetTargetFPS(60);
    CreatureSimulation creature(makeDefaultGenome());
    float elapsedTime = 0.0F;
    bool chronicleOpen = false;
    bool paused = false;
    int chronicleScroll = 0;
    while (!WindowShouldClose()) {
        const float frameTime = GetFrameTime();
        if (IsKeyPressed(KEY_H)) { chronicleOpen = !chronicleOpen; chronicleScroll = 0; }
        if (IsKeyPressed(KEY_BACKSPACE)) chronicleOpen = false;
        if (chronicleOpen && IsKeyPressed(KEY_UP)) ++chronicleScroll;
        if (chronicleOpen && IsKeyPressed(KEY_DOWN)) chronicleScroll = std::max(0, chronicleScroll - 1);
        if (IsKeyPressed(KEY_SPACE)) paused = !paused;
        if (!chronicleOpen) {
            if (IsKeyPressed(KEY_F)) creature.applyInteraction(Interaction::Feed);
            if (IsKeyPressed(KEY_P)) creature.applyInteraction(Interaction::Pet);
            if (IsKeyPressed(KEY_N)) creature.applyInteraction(Interaction::Novelty);
            if (IsKeyPressed(KEY_S)) creature.applyInteraction(Interaction::LoudSound);
            if (IsKeyPressed(KEY_B)) creature.applyInteraction(Interaction::BroughtToForeground);
            if (IsKeyPressed(KEY_R)) creature.applyInteraction(Interaction::ReturnedToFarm);
        }
        if (!paused) {
            creature.update(frameTime);
            elapsedTime += frameTime;
        }
        BeginDrawing();
        ClearBackground(Background);
        DrawText("EVOLVING GARDEN", 28, 25, 25, TextPrimary);
        DrawText("BEHAVIOR & PROCEDURAL ORGANISM PROTOTYPE  /  0.3", 30, 58, 12, TextMuted);
        DrawText(paused ? "PAUSED" : "SIMULATING", 1010, 35, 12,
            paused ? Color{221, 174, 105, 255} : Accent);
        if (chronicleOpen) drawChronicle(creature, chronicleScroll);
        else {
            drawPersonalityPanel(creature);
            drawHabitat(creature, elapsedTime);
            drawNeedsAndEmotions(creature);
            drawRecentMemory(creature);
        }
        drawControls(chronicleOpen, paused);
        EndDrawing();
    }
    CloseWindow();
    return 0;
}
