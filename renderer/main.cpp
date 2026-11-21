#include "raylib.h"

namespace
{
constexpr int WindowWidth = 1100;
constexpr int WindowHeight = 700;

void DrawPlaceholderCreature(Vector2 center)
{
    // Temporary placeholder organism. The real body will be generated
    // from its genome once the creature model is implemented.
    DrawEllipse(
        static_cast<int>(center.x),
        static_cast<int>(center.y + 28.0F),
        92.0F,
        22.0F,
        Fade(BLACK, 0.22F)
    );

    DrawCircleV(center, 76.0F, Color{ 135, 190, 164, 255 });
    DrawCircleV(
        Vector2{ center.x - 25.0F, center.y - 12.0F },
        9.0F,
        RAYWHITE
    );
    DrawCircleV(
        Vector2{ center.x + 25.0F, center.y - 12.0F },
        9.0F,
        RAYWHITE
    );
    DrawCircleV(
        Vector2{ center.x - 23.0F, center.y - 11.0F },
        4.0F,
        DARKGRAY
    );
    DrawCircleV(
        Vector2{ center.x + 27.0F, center.y - 11.0F },
        4.0F,
        DARKGRAY
    );
    DrawLineEx(
        Vector2{ center.x - 9.0F, center.y + 22.0F },
        Vector2{ center.x + 9.0F, center.y + 22.0F },
        3.0F,
        Color{ 55, 75, 67, 255 }
    );
}
}

int main()
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(WindowWidth, WindowHeight, "EvolvingGarden | Creature Lab");
    SetTargetFPS(60);

    while (!WindowShouldClose())
    {
        BeginDrawing();
        ClearBackground(Color{ 22, 25, 31, 255 });

        DrawText("EVOLVING GARDEN", 36, 28, 28, RAYWHITE);
        DrawText("CREATURE LAB  /  PROTOTYPE 0.1", 38, 64, 14, Color{ 145, 155, 169, 255 });

        DrawRectangleRounded(
            Rectangle{ 30.0F, 112.0F, 1040.0F, 540.0F },
            0.025F,
            12,
            Color{ 31, 36, 44, 255 }
        );

        DrawText("ANCESTRAL ORGANISM", 58, 138, 16, Color{ 175, 190, 181, 255 });
        DrawPlaceholderCreature(Vector2{ 550.0F, 365.0F });

        DrawText(
            "Simulation core: not connected yet",
            58,
            604,
            16,
            Color{ 145, 155, 169, 255 }
        );
        DrawText(
            "Next: genome, traits, needs and mutation rules",
            58,
            626,
            14,
            Color{ 115, 130, 145, 255 }
        );

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
