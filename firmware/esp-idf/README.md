# ESP32-S3 firmware prototype

This is a separate ESP-IDF application for the **Waveshare ESP32-S3-Touch-AMOLED-1.75C**. It does not replace the Raylib desktop application.

## First hardware milestone

- Starts the official Waveshare board support package.
- Uses LVGL 9 on the 466 x 466 CO5300 AMOLED.
- Displays one procedurally sized pet with animated pupils, blinking, facial expressions, body bobbing, and moving appendages.
- Reuses the same platform-independent `CreatureSimulation` and genome as the desktop build.
- Touch the creature or use the PET, FEED, and DISCOVER buttons.
- Interaction cooldowns and emotional reactions are handled by the shared simulation core.

The touchscreen is the only physical input used in this first bring-up. Microphone and QMI8658 motion-sensor integration are planned for a later iteration.

## Requirements

Use an ESP-IDF version supported by the Waveshare component. The current board examples validate ESP-IDF **v5.5.5** and **v6.0.2**.

The ESP-IDF environment must be activated in the terminal before running the commands below.

## Build and flash

Open PowerShell in this folder:

```powershell
cd firmware\esp-idf
idf.py --version
idf.py set-target esp32s3
idf.py build
```

Connect the board over USB, find its COM port in Device Manager, then flash:

```powershell
idf.py -p COMx flash monitor
```

Replace `COMx` with the actual port, for example `COM7`.

The first build downloads the managed Waveshare BSP and LVGL dependencies. It uses its own `build` directory here, so the desktop build in the repository root is not overwritten.

## Controls

- Tap the creature: pet it.
- PET: pet it.
- FEED: feed it.
- DISCOVER: show an unusual stimulus.

The current UI is deliberately small. It is a hardware bring-up prototype, not the final game interface.

## Notes

- The application uses the Waveshare managed component `waveshare/esp32_s3_touch_amoled_1_75c` and LVGL 9.
- The shared simulation uses a compact deterministic PRNG instead of `<random>` to reduce embedded overhead and keep desktop/firmware behavior reproducible.
- No Wi-Fi, audio capture, IMU polling, persistent save system, or breeding is enabled yet.
- The firmware has not been built with the user's local ESP-IDF installation or tested on the physical board in this change.
