# EvolvingGarden

A small creature-evolution simulator inspired by virtual pets, procedural life and artificial selection.

## Current prototype: 0.3

The desktop prototype contains a first behavior and animation slice:

- Six inherited personality traits, each in the range -100 to +100.
- Eight emotional channels, plus mood and arousal.
- Four basic needs: hunger, energy, social contact and boredom.
- A lightweight action selector responding to personality and needs.
- Contextual reactions to feeding, petting, novelty, loud sounds and foreground selection.
- A bounded chronicle that stores only standout events, with at most one significant event per 10 simulated minutes.
- Procedural 2D creature geometry generated from compact morphology genes.
- Moving pupils, autonomous gaze, blinking, emotion-based expressions, body motion and appendage animation.
- Automated tests for the platform-independent simulation layer.

## Controls

- F: Feed
- P: Pet
- N: Present an unusual stimulus
- S: Simulate a loud sound
- B: Bring the creature to the main screen
- R: Return the creature to the farm
- H: Open or close the life chronicle
- Up/Down: Browse older and newer chronicle entries
- Space: Pause or resume simulation
- Backspace: Return from the chronicle

Pressing B repeatedly while the creature is already in the foreground does not trigger another visit or relationship reward. A new visit requires returning to the farm first, and foreground affection rewards have a 10-minute cooldown.

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
- simulation/ contains needs, emotions, reactions, memory and behavior.
- renderer/ turns the phenotype into procedural geometry and presents the desktop prototype.
- firmware/ will eventually provide the ESP32-S3 target.

The simulation layer must remain independent of Raylib so it can be reused by future firmware. The desktop renderer is a development and inspection tool, not a copy of the small device interface.

## Current limitations

This is a first iteration, not a finished artificial-life system. The environment is currently represented by a few test interactions, the behavior model is intentionally small, and morphology is a compact procedural 2D shape. Genetic inheritance, full mutation rules, multiple simultaneously simulated creatures, save files and physical sensor input are not implemented yet.
