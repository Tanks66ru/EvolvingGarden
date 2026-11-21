# EvolvingGarden ESP32-S3 firmware

Target: Waveshare ESP32-S3-Touch-AMOLED-1.75C, 466 x 466 AMOLED.

## Hardware demo

- Enlarged procedural pet with animated eyes, expressions and appendages.
- Tap the creature to pet it.
- One large action button at the bottom. Tap to perform the selected action.
- Swipe right to cycle PET, FEED and DISCOVER.
- Swipe left for the live creature/debug panel. Swipe right there to return.
- The built-in microphones detect sustained or loud sound bursts and pass intensity to the shared simulation.
- The built-in speaker plays a synthesized cue for each reaction and action-button press.
- Optional WAV files replace synthesized sounds.

## Shared application boundary

The creature interface and its frame update loop live in `app/creature_app.cpp`. This source is compiled directly into the ESP-IDF firmware and depends on LVGL plus the platform-neutral audio contract in `platform/audio.hpp`.

The ESP32 entry point in `firmware/esp-idf/main/main.cpp` is responsible for starting the Waveshare display/input driver, initializing the board audio backend, and scheduling the shared application. Board-specific BSP and codec calls stay outside the shared UI.

The next development step is a PC host for this same LVGL application. It will replace the board drivers with desktop display, mouse/touch and audio backends; it will not contain a second copy of the creature UI.

## Custom sounds

Place optional files in the repository folder assets/sounds:

- button_click.wav
- reaction_happy.wav
- reaction_sad.wav
- reaction_angry.wav
- reaction_fearful.wav
- reaction_surprised.wav
- reaction_curious.wav
- reaction_affectionate.wav
- reaction_disgusted.wav

Required format: uncompressed PCM WAV, 16-bit, mono, 22,050 Hz.

Convert a WAV using:

    python tools/convert_sounds.py source.wav reaction_happy

Use one cue name from the list without the .wav extension. The firmware builds a SPIFFS image from assets/sounds and flashes it with the application. No SD card is required.

## Build and flash

Activate ESP-IDF and open PowerShell in this folder:

    cd firmware/esp-idf
    idf.py --version

The first build after switching to the custom partition table should regenerate the local sdkconfig. For an existing build directory, run:

    idf.py fullclean
    Remove-Item sdkconfig -ErrorAction SilentlyContinue
    idf.py set-target esp32s3
    idf.py build

Flash using the port shown in Device Manager (the board was previously detected as COM4):

    idf.py -p COM4 -b 460800 flash monitor

The firmware project is separate from the Raylib desktop application.

## Notes

- The firmware reuses the same platform-independent creature simulation as the desktop prototype.
- Audio uses the board's ES7210 microphone ADC and ES8311 speaker codec through the official BSP.
- Input currently detects loudness/sound bursts only. It does not perform speech recognition or identify words.
- Microphone events are suppressed while the speaker is playing to reduce feedback.
- If the SPIFFS partition cannot be mounted, synthesized fallback cues remain available.
- This revision is committed to GitHub but still needs a local ESP-IDF build and physical touch/audio verification.
