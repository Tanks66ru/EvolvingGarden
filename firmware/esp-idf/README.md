# EvolvingGarden ESP32-S3 firmware

Target: Waveshare ESP32-S3-Touch-AMOLED-1.75C, 466 x 466 AMOLED.

## Physical interactions

- **FEED button:** feed the creature. This is the only on-screen gameplay button.
- **Tap the creature or empty main screen:** hit it. Creatures with low friendliness treat a hit as sparring and dodge/counter; friendly creatures become angry or frightened.
- **Circle around the creature:** pet it. Clockwise and counter-clockwise circles are both accepted.
- **Steps:** the QMI8658 accelerometer detects walking-like movement. Every ten counted steps trigger a discovery.
- **Shake:** a strong, short movement burst triggers a surprise/play response.
- **Tilt:** holding the board at a noticeable angle triggers a balance/curiosity response.
- **Rotate:** a quick rotation around the screen normal triggers a separate surprise response.
- **Face-down sleep:** after the board is stable with the screen facing down for about 1.5 seconds, AMOLED brightness is set to zero. Lifting or touching the board wakes the screen.
- **PWR top button:** opens or closes the Garden menu. It is read from the AXP2101 PMIC over the I2C bus initialized by the BSP.
- **BOOT bottom button:** opens or closes diagnostics; while the Garden menu is open it acts as Back. It is GPIO0, active-low. Holding BOOT during reset still enters ESP32 download mode.

The built-in microphone sound-burst detection and reaction audio remain active. The serial monitor reports measured display refresh FPS every two seconds.

## AMOLED display startup

The CO5300 display and CST9217 touch controller are initialized through the Waveshare 1.75C BSP. The adapter currently uses the BSP's standard refresh mode; GPIO13 TE synchronization is intentionally disabled until it can be enabled with the exact configuration supported by the installed adapter version. This preserves the known-good panel initialization path.

## IMU and screen sleep notes

The 1.75C BSP exposes the shared I2C bus but does not provide a ready-made IMU helper. The firmware uses the official `waveshare/qmi8658` managed component and passes it `bsp_i2c_get_handle()`. It does not create a second I2C bus.

At first startup, keep the board flat with its screen facing up for about one second so the firmware can calibrate the screen-normal direction. Face-down detection then requires a stable, nearly flat position.

This initial sleep implementation blanks the AMOLED while keeping the ESP32 and IMU polling awake. It is intentionally not deep sleep: reliable wake from deep sleep needs the QMI8658 interrupt routing and low-power wake sequence to be configured and verified for this exact board revision.

The step detector is a software heuristic based on accelerometer pulses and a cooldown. Its sensitivity will need physical calibration later.

## Save data

A versioned simulation snapshot is stored in ESP-IDF NVS. It includes the genome, needs, emotions, memory journal, reaction, action, cooldowns, relationship, simulation timers, progression counters, random-generator state and lifetime step count. Interaction/step changes request a throttled save, and a periodic checkpoint stores passive need and mood changes. Flash writes are rate-limited to no more than one interaction save every three seconds to avoid wearing the storage unnecessarily.


## Custom sounds

Place optional files in `assets/sounds`:

- button_click.wav
- hit_impact.wav (optional; a synthesized low thump is used when this file is absent)
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

The firmware builds a SPIFFS image from the sounds folder and flashes it with the application.

## Build and flash

Activate ESP-IDF and open PowerShell in this folder:

    cd firmware/esp-idf
    idf.py --version

The QMI8658 dependency is now part of the manifest. Let ESP-IDF resolve it during the next configure/build:

    idf.py build

Flash using the port shown in Device Manager (the board was previously detected as COM4):

    idf.py -p COM4 -b 460800 flash monitor

## Notes

- The same creature simulation, expressions, procedural geometry and LVGL widgets are shared by the firmware.
- LVGL software rendering is configured to use two draw units on the ESP32-S3; the refresh FPS is logged to the serial monitor.
- Audio uses the board's ES7210 microphone ADC and ES8311 speaker codec through the official BSP.
- The onboard microphones detect sound bursts only; there is no speech recognition.
- The board has no vibration motor, so hit feedback uses the display animation and speaker.
- Microphone events are suppressed while the speaker is playing to reduce feedback.
- If the SPIFFS partition cannot be mounted, synthesized fallback cues remain available.
