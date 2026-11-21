# EvolvingGarden

A creature-evolution and virtual-pet project for the Waveshare ESP32-S3-Touch-AMOLED-1.75C.

## Physical interactions

- The large **FEED** button feeds the creature. It is the only gameplay button.
- A short tap on the creature is a hit. Creatures with low friendliness enjoy sparring; friendly creatures dislike being hit.
- Draw circles around the creature with your finger to pet it. Each completed circle can trigger another petting response.
- Walking while carrying the board increments the step counter. Every ten steps can produce a discovery.
- Shake the board for a surprise/play response.
- Tilt the board to trigger a balance/curiosity response.
- Rotate the board around the screen normal for a separate rotation response.
- Place the screen face-down and keep it still to blank the AMOLED. Lifting or touching the board wakes the screen.
- The **LAB** button opens diagnostics; the explicit **BACK** button closes them.

The microphone continues to detect sound bursts and the speaker plays reaction cues.

## Shared application

The LVGL creature interface is implemented once in `app/creature_app.cpp` and is compiled by the ESP32 firmware.

- `core/`, `creatures/`, `simulation/`: platform-independent creature data and behavior.
- `app/`: shared LVGL interface, animation and application update loop.
- `platform/audio.hpp`: platform-neutral audio contract.
- `platform/motion.hpp`: platform-neutral motion state contract.
- `platform/esp32/`: QMI8658 motion sensor backend for the Waveshare board.
- `firmware/esp-idf/`: Waveshare display/touch startup and ESP32 audio integration.
- `simulator/`: optional desktop host; current development is focused on the physical board firmware.
- `renderer/`: the older Raylib simulation laboratory, separate from the device UI.

## Build and flash

See [firmware/esp-idf/README.md](firmware/esp-idf/README.md) for ESP-IDF setup, build and flashing instructions.

The QMI8658 is initialized through the I2C bus already created by the board BSP. The firmware does not initialize a second I2C controller on the same pins.
