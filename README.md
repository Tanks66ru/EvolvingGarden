# EvolvingGarden

A small creature-evolution simulator inspired by virtual pets and artificial selection.

## Project direction

- One common ancestral organism; descendants inherit traits through a genome.
- Mutations and environmental pressures shape later generations.
- Personality affects individual behavior, not just displayed stats.
- Feeding, care, and life stages influence development.
- The simulation should run locally and offline first.
- The PC simulator and future ESP32 firmware should share the same platform-independent simulation core.

## Planned structure

| Directory | Responsibility |
|---|---|
| `core/` | Shared domain types, genome rules, mutation and evolution logic |
| `simulation/` | Time, needs, behavior, life-cycle and world simulation |
| `creatures/` | Creature definitions, traits, phenotype and lineage |
| `renderer/` | PC visualization and presentation |
| `assets/` | Sprites, animations, fonts and audio |
| `data/` | Tunable game data such as foods, trait tables and species parameters |
| `firmware/` | Future ESP32-S3 application |
| `tests/` | Automated tests for deterministic game logic |
| `third_party/` | External dependencies |
| `tools/` | Development and content-pipeline utilities |
| `docs/` | Design notes, architecture and technical documentation |

The structure is intentionally a starting point. New modules should be added when the implementation needs them, not just because folders are cheap.

## Development

The desktop simulator is the first target. The embedded target comes after the simulation rules are stable and can be tested independently of the renderer.
