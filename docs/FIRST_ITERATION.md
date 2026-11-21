# EvolvingGarden 0.3 - Behavior and Expression Pass

## Foreground selection

Bringing a creature to the main screen is a state transition, not a repeatable interaction. The creature must first be returned to the farm before it can be brought forward again. Relationship rewards from foreground visits are limited to one per 10 simulated minutes.

## Reactions and animation

Reactions are short shared animation types triggered by interactions:

- Happy: a small bounce and squash/stretch.
- Affectionate: a softer bob and relaxed gaze.
- Curious: a forward lean and directed gaze.
- Startled: a quick recoil, body ripple and faster appendage motion.
- Annoyed: a short side-to-side shake.

The same reaction library is used by every creature. Genes, current emotion and action state change the strength and presentation.

The renderer also animates eye movement, autonomous gaze, blinking, pupils, brows, mouth, subtle body breathing and appendages. Eye size remains a morphology gene; the ancestral default was increased slightly.

## Chronicle filtering

The chronicle stores only events with significance >= 75/100. Birth is always recorded. Routine feeding, petting and repeated ordinary novelty are not automatically journalled. A global cooldown allows no more than one significant event per 600 seconds of simulation time.

The journal remains a fixed-size array. It does not store prose; event titles and descriptions are resolved from compact event IDs.

## Still not implemented

The prototype does not yet have multiple creatures, a real farm environment, persistent saves, real sensor input, breeding or the mutation engine. The keyboard actions are temporary ways to simulate future touch, motion and sound events.
