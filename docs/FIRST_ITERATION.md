# EvolvingGarden 0.4 - Character Budget and Reaction Pass

## Personality point budget

The base genome starts with a 150-point trait budget. The six signed personality traits are summed:

    remaining = traitPointBudget - sum(personality traits)

Positive trait values spend points. Negative trait values refund points.

Example: Friendliness +100 and Curiosity -50 have a net cost of 50. With a 150-point budget, 100 points remain.

The base genome has:
- Friendliness +45
- Optimism +35
- Sociability +5
- Curiosity +55
- Activity +20
- Sensitivity -10

The net total is exactly 150.

The budget itself is stored in the genome and currently clamped to 120-180. This makes the total pool available to future inheritance and mutation logic. The current prototype does not yet implement breeding or mutations; it stores and validates the budget data.

Negative values describe the opposite end of a personality axis. They are not necessarily universally bad: low sensitivity can mean a calmer creature, while low sociability can mean an independent one.

## Interaction cooldowns

Inputs have independent cooldowns to prevent the same action from being spammed or its animation restarted mid-playback.

- Feed: 1.2 seconds
- Pet: 1.4 seconds
- Novelty: 1.4 seconds
- Bad taste: 1.4 seconds
- Loud sound: 1.5 seconds

The foreground screen is a state transition. Bringing a creature forward when it is already there does nothing. It must return to the farm before it can be brought forward again. Relationship rewards from foreground visits also have a 10-minute simulation-time cooldown.

## Sound intensity and exaggerated reactions

applySoundStimulus(intensity, durationSeconds) accepts normalized loudness and measured event duration. S triggers a short ordinary Surprised reaction. L simulates a sustained, very loud event and triggers the exaggerated Fearful reaction. A 1.5-second cooldown is applied to all sound stimuli. Recent exposure also decays over time, so repeated loud events can build up only when they are close enough together.

G simulates an unpleasant taste and triggers Disgusted.

## Reaction library

There are eight common reaction archetypes:
- Happy
- Sad
- Angry
- Fearful
- Surprised
- Curious
- Affectionate
- Disgusted

Each archetype supports a standard and an exaggerated presentation. The renderer has a shared procedural animation for each type. Exaggeration increases the motion and deformation rather than requiring a second set of image assets.

Happy is demonstrated by feeding, Affectionate and Angry by petting, Curious by novelty, Surprised and Fearful by sound, and Disgusted by the bad-taste test. Sadness builds when the creature's hunger becomes severely high or its social need remains high while it is strongly sociable. The autonomous reaction system then plays Sad. Extreme reactions can use their exaggerated variant.

## Chronicle filtering

The journal stores only standout events with significance >= 75/100. A global cooldown allows at most one stored significant event per 600 seconds of simulation time. Routine care and ordinary repeated interactions do not fill the journal.

## Procedural morphology

The renderer derives a 2D body outline from trigonometric contour functions. Genome parameters control body proportions, roundness, asymmetry, contour complexity, appendages, eyes and pattern density. Geometry is recalculated from genes and animation state; no per-creature bitmap assets are stored.

## Platform boundary

The simulation headers do not include Raylib. Only renderer/ depends on Raylib. This separation is intentional so the same simulation rules can later be compiled for ESP32 firmware.

## Automated checks

EvolvingGardenTests covers trait normalization, point-budget arithmetic, fixed-capacity memory behavior, personality-dependent reactions, foreground transition guards, sound cooldown/escalation, bad-taste reaction, autonomous sadness and significant-event filtering.
