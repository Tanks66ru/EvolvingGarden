# EvolvingGarden

A creature-evolution and virtual-pet project for the Waveshare ESP32-S3-Touch-AMOLED-1.75C.

## Shared application

The LVGL creature interface is implemented once in `app/creature_app.cpp`. The ESP32 firmware and the PC simulator both compile this same source file.

- `core/`, `creatures/`, `simulation/`: platform-independent creature data and behavior.
- `app/`: shared LVGL interface, animation and application update loop.
- `platform/audio.hpp`: platform-neutral audio contract.
- `platform/desktop/`: SDL audio input/output for the PC simulator.
- `firmware/esp-idf/`: Waveshare display/touch startup and ESP32 audio backend.
- `simulator/`: desktop CMake project using LVGL 9.5's built-in SDL2 drivers.
- `renderer/`: the older Raylib simulation laboratory, kept separate from the device UI.

## Run the PC simulator

See [simulator/README.md](simulator/README.md) for requirements and build instructions.

The simulator uses the same 466 x 466 LVGL interface as the ESP32 firmware. The mouse replaces the touch input; the PC's default microphone and speakers replace the board audio devices.

## Run on the Waveshare board

See [firmware/esp-idf/README.md](firmware/esp-idf/README.md) for ESP-IDF setup, build and flashing instructions.

The simulator and firmware share the application/UI code. Only the platform drivers and startup are different.
