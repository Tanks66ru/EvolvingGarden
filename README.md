# EvolvingGarden

A small creature-evolution simulator inspired by virtual pets, procedural life and artificial selection.

## Current prototype: 0.2

The desktop prototype now contains the first vertical slice of the creature simulation:

- Six inherited personality traits, each in the range -100 to +100.
- Eight emotional channels, plus mood and arousal.
- Four basic needs: hunger, energy, social contact and boredom.
- A lightweight action selector that responds to personality and needs.
- Player interactions: feed, pet, novelty, loud sound and foreground attention.
- A bounded significant-event journal.
- Procedural 2D creature geometry generated from compact morphology genes.
- A small automated test executable for the platform-independent simulation layer.

## Controls

- F: Feed
- P: Pet
- N: Present an unusual stimulus
- S: Simulate a loud sound
- B: Bring the creature to the main screen
- H: Open or close the life chronicle
- Space: Pause or resume simulation
- Backspace: Return from the chronicle

## Build

The desktop application uses CMake, C++20 and Raylib 5.5.

Run these commands in Developer PowerShell:

    cmake -S . -B build -G Ninja
    cmake --build build
    ctest --test-dir build --output-on-failure

Run build/EvolvingGarden.exe after building.

## Architecture direction

- core/ contains platform-independent types and common rules.
- creatures/ contains genome data.
- simulation/ contains needs, emotions, memory and behavior.
- renderer/ turns the phenotype into procedural geometry and presents the desktop prototype.
- firmware/ will eventually provide the ESP32-S3 target.

The simulation layer must remain independent of Raylib so it can be reused by future firmware. The desktop renderer is a development and inspection tool, not a copy of the small device interface.

## Current limitations

This is a first iteration, not a finished artificial-life system. The environment is currently represented by a few test interactions, the behavior model is intentionally small, and morphology is a compact procedural 2D shape. Genetic inheritance, real mutation rules, other creatures, save files and physical sensor input are not implemented yet.
