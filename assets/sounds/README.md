# Creature sounds

Optional WAV files placed here replace the built-in synthesized cues.

| Filename | Cue |
| --- | --- |
| button_click.wav | Button / action button |
| hit_impact.wav | Impact thump (optional) |
| reaction_happy.wav | Happy |
| reaction_sad.wav | Sad |
| reaction_angry.wav | Angry |
| reaction_fearful.wav | Fearful |
| reaction_surprised.wav | Surprised |
| reaction_curious.wav | Curious |
| reaction_affectionate.wav | Affectionate |
| reaction_disgusted.wav | Disgusted |

Required format: uncompressed PCM WAV, signed 16-bit, mono, 22,050 Hz.

The firmware has synthesized fallback tones for every cue. Missing or unsupported files are harmless.

Convert a source WAV using:

    python tools/convert_sounds.py source.wav reaction_happy

Use one cue name from the table without the .wav extension. The converted file is written to this folder. Rebuild and flash after changing sounds so the SPIFFS image is updated.
