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
- [ ] Define a hardware-independent simulation API.
- [ ] Profile memory and CPU requirements.
- [ ] Port the supported simulation subset to ESP32-S3.
