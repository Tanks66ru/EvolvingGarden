# EvolvingGarden 0.2 - First Behavior and Morphology Slice

## Goals

This iteration establishes a small, testable foundation. It is deliberately not a full life simulator.

## Personality genes

All six personality values are signed integers in [-100, +100]:

- Friendliness: willingness to accept contact and approach others.
- Optimism: tendency to interpret uncertain events positively.
- Sociability: desire for social contact.
- Curiosity: motivation to investigate unfamiliar things.
- Activity: preference for movement and energetic actions.
- Sensitivity: strength of reactions to external stimuli.

These values are inherited genome data. They do not change directly when a temporary emotion occurs.

## Emotional state

The prototype has eight simultaneously active emotional channels: Joy, Sadness, Anger, Fear, Surprise, Curiosity, Affection and Disgust.

Each channel is in [0, 100] and decays over time at its own rate. Mood is a slower aggregate state; arousal tracks the strongest current emotion.

## Needs and action selection

Needs are Hunger, Energy, Social and Boredom. The action selector scores a small set of actions using needs and personality, then chooses the highest score with a small seeded random variation.

Actions are Idle, Explore, Play, Rest, SeekCompany, Observe and Forage. Forage is only a search behavior; food is supplied through the Feed interaction.

## Interactions

- Feed reduces hunger and can create a meal memory when the creature was hungry.
- Pet is interpreted through friendliness and sensitivity.
- Novelty creates surprise and curiosity. Curiosity and optimism increase the relationship gain.
- LoudSound simulates a strong external audio stimulus.
- BroughtToForeground represents the player choosing this creature for individual attention. Curiosity, optimism and sociability affect relationship gain.
- Repeated novelty and foreground interactions have diminishing returns.

## Memory journal

The journal is a fixed-size array of 24 compact records. Each record stores a simulation timestamp, event type, emotional impact, significance and an optional related-creature identifier.

Only events with significance >= 28 are stored. When the journal is full, the oldest entry is discarded. Event text is generated from event IDs at render time instead of being stored in each record.

This is a bounded prototype journal, not a complete long-term memory model.

## Procedural morphology

The renderer derives a 2D body outline from trigonometric contour functions. Genome parameters control body proportions, roundness, asymmetry, contour complexity, appendages, eyes and pattern density.

Appendages are rendered as short quadratic curves. Decorative spots are generated deterministically from the genome seed. The shape is reconstructed from genes at runtime; no per-creature bitmap assets are stored.

## Platform boundary

The simulation headers do not include Raylib. Only renderer/ depends on Raylib. This separation is intentional so the same simulation rules can later be compiled for ESP32 firmware.

## Automated checks

EvolvingGardenTests checks trait normalization, fixed-capacity memory behavior, novelty-based relationship differences and initial chronicle entries.
