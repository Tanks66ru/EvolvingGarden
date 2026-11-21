# Development roadmap

## Milestone 0: Desktop foundation
- [x] Create the repository structure.
- [x] Add a CMake desktop target.
- [x] Fetch raylib through CMake FetchContent.
- [x] Open a resizable window and render a placeholder organism.
- [ ] Verify the first Windows build locally.

## Milestone 1: Creature model
- [ ] Define a platform-independent genome.
- [ ] Define phenotype traits separately from inherited genes.
- [ ] Create a deterministic organism instance from a genome and seed.
- [ ] Add unit tests for genome-to-trait mapping.

## Milestone 2: Simulation
- [ ] Add needs and life stages.
- [ ] Add behavior/personality parameters.
- [ ] Add fixed-step simulation updates.
- [ ] Add mutation and inheritance rules.
- [ ] Add lineage tracking.

## Milestone 3: Creature Lab
- [ ] Visualize phenotype traits.
- [ ] Allow controlled genome edits.
- [ ] Run and compare generations.
- [ ] Save and load experiment data.

## Milestone 4: Embedded client
- [x] Add a separate ESP-IDF project for Waveshare ESP32-S3-Touch-AMOLED-1.75C.
- [x] Reuse the shared simulation/genome headers on the embedded target.
- [x] Add the first LVGL pet screen with touch interactions and procedural facial/body animation.
- [x] Replace the crowded button row with one large swipe-selectable action button.
- [x] Add swipe navigation and a live debug panel.
- [x] Integrate onboard microphone sound-event detection.
- [x] Add speaker reaction cues, synthesized fallbacks, and optional custom WAV assets.
- [ ] Build this revision with the user's local ESP-IDF toolchain and verify touch/audio behavior on the physical board.
- [ ] Integrate QMI8658 motion input.
- [ ] Profile memory, CPU use, and display frame rate on hardware.
