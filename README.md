# EvolvingGarden

A small creature-evolution simulator inspired by virtual pets, procedural life and artificial selection.

## Current prototype: 0.4

The desktop prototype contains a first behavior, character-budget and reaction-animation slice:

- Six inherited personality traits with a shared point budget.
- Positive trait values spend points; negative values refund points.
- The base genome starts with 150 points. The budget itself is a heritable genome parameter, currently constrained to 120-180 for future mutation rules.
- Eight emotional channels, plus mood and arousal.
- Four basic needs: hunger, energy, social contact and boredom.
- A lightweight action selector responding to personality and needs.
- Eight shared reaction archetypes, each with a standard and exaggerated presentation. Strong neglect can trigger sadness; a bad-taste test stimulus triggers disgust.
- Procedural 2D creature geometry generated from compact morphology genes.
- Autonomous gaze, blinking, emotion-based facial expressions, body motion and appendage animation.
- Input cooldowns, including a 1.5-second cooldown for sound stimuli.
- A bounded chronicle that stores only standout events, with at most one significant event per 10 simulated minutes.
- Automated tests for the platform-independent simulation layer.

## Personality point budget

The base genome has a budget of 150 points. The six signed traits are added together:

    remaining points = genome budget - sum(six personality traits)

A positive value spends points. A negative value refunds points.

Example:

- Friendliness +100 costs 100 points.
- Curiosity -50 refunds 50 points.
- Net cost is 50, leaving 100 points from a 150-point budget.

The base creature's traits add up to exactly 150:
Friendliness +45, Optimism +35, Sociability +5, Curiosity +55, Activity +20, Sensitivity -10.

The budget is stored in the genome so future mutation rules can change the available point pool as well as individual traits. The actual breeding and mutation engine is not implemented yet.

## Controls

- F: Feed
- P: Pet
- N: Present an unusual stimulus
- S: Play a short sound stimulus
- L: Simulate a sustained, very loud sound (exaggerated fear reaction)
- G: Simulate an unpleasant taste (disgust reaction)
- B: Bring the creature to the main screen
- R: Return the creature to the farm
- H: Open or close the life chronicle
- Up/Down: Browse older and newer chronicle entries
- Space: Pause or resume simulation
- Backspace: Return from the chronicle

Interactions have short cooldowns so repeated input cannot restart or stack an animation. Sound input has a 1.5-second cooldown. The sensor layer will later call applySoundStimulus(intensity, durationSeconds) with measured microphone data.

## Reactions

The shared reaction library contains Happy, Sad, Angry, Fearful, Surprised, Curious, Affectionate and Disgusted. Each has a standard form and an exaggerated form. The current strong/long sound test triggers the exaggerated Fearful reaction.

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

This is still a prototype. The environment is represented by test interactions, and the behavior model is intentionally small. Breeding, mutation execution, multiple simultaneously simulated creatures, save files and physical sensor input are not implemented yet.

## ESP32-S3 firmware prototype

A separate ESP-IDF application now targets the Waveshare ESP32-S3-Touch-AMOLED-1.75C. It uses the official Waveshare managed BSP and LVGL 9, and reuses the same simulation/genome code as the desktop prototype.

The first hardware milestone displays one animated procedural pet, supports touch interaction (pet, feed, discover), and reuses the shared emotions, reactions, needs, cooldowns, and personality logic. It does not yet read the microphone or QMI8658 motion sensor.

See [firmware/esp-idf/README.md](firmware/esp-idf/README.md) for setup, build, and flashing instructions. This target has not yet been validated on the user's physical board.
