# How EvolvingGarden works

EvolvingGarden is a persistent virtual creature running on the Waveshare ESP32-S3-Touch-AMOLED-1.75C. The ESP32 firmware is the primary runtime. The older desktop renderer and simulator are development experiments, not required by the device.

## Runtime flow

The ESP-IDF entry point initializes the display, physical BOOT key, QMI8658 motion sensor, audio codec and shared LVGL application. A 30 Hz timer polls the sensor and button, then advances the creature simulation and redraws the UI.

The source is separated by responsibility:

| Folder / file | Responsibility |
|---|---|
| `app/` | Touch gestures, screen layout, expressions, animation, LAB panel and save scheduling |
| `simulation/` | Needs, emotions, reactions, memory journal, action selection and progression |
| `creatures/` | Genome: personality and morphology genes |
| `core/` | Shared utilities and deterministic random generator |
| `platform/motion.hpp` | Hardware-independent motion API |
| `platform/esp32/motion.cpp` | QMI8658 sensor driver and step / shake / tilt / rotation detection |
| `platform/persistence.hpp` | Hardware-independent save API |
| `platform/esp32/persistence.cpp` | ESP-IDF NVS save-slot implementation |
| `platform/audio.hpp` | Hardware-independent audio cue API |
| `firmware/esp-idf/` | Board startup, LVGL, audio integration, partitions and build configuration |

## Controls

| Input | Action |
|---|---|
| FEED on screen | Feed the creature |
| Tap the creature | Hit / spar |
| Draw a circle around it | Pet; clockwise and counter-clockwise both work |
| Walk while carrying the board | Add steps; each ten steps can trigger a discovery |
| Shake the board | Surprise / play response |
| Tilt the board | Curiosity / balance response |
| Rotate it around the screen normal | Rotation response |
| Place it face-down and still | Blank the AMOLED after a short delay |
| Physical BOOT button | Open or close the LAB diagnostics panel |

The BOOT button is GPIO0, active low, and is sampled while the application is running. Holding BOOT during reset remains the ESP32 download-mode procedure. The PWR key is not used by the application.

The board does not provide a vibration motor. A hit therefore uses a low-frequency speaker thump, an expanding ring at the touch point, a brief body flash, squash and recoil animation. Creatures with a sparring personality dodge and counter-lunge.

## Creature simulation

Each creature has an inherited genome with six personality traits and a set of morphology genes. The personality changes how interactions are perceived. For example, friendly creatures dislike being hit, while low-friendliness creatures treat it as sparring.

Needs and emotions evolve continuously. The creature selects its own actions from hunger, energy, social needs, curiosity, activity and boredom. Reactions are temporary, while selected significant events are written to a bounded memory journal.

Walking progress is tied to the IMU step detector. Ten accumulated steps form a discovery milestone and affect curiosity, joy, boredom and the relationship with the player.

## Persistence

The ESP32 stores one versioned snapshot in NVS under namespace `garden`, key `pet_state`. The snapshot includes:

- genome and personality / morphology;
- hunger, energy, social and boredom;
- emotion values and arousal;
- memory journal and current reaction;
- current action and interaction cooldowns;
- mood and relationship with the player;
- simulation timers and progression counters;
- lifetime step count and random-generator state.

Interaction and step changes request a save. Flash writes are throttled so they occur no more often than every three seconds, with a periodic checkpoint every minute for passive changes to needs and mood. A reset may lose a short unsaved interval, but it should not erase the creature's entire progress. The save is local; this firmware does not use Wi-Fi or a remote server.

## Audio

The ES8311 speaker plays optional PCM WAV cues from `assets/sounds/`. If a cue file is missing, the firmware synthesizes a fallback tone. `hit_impact.wav` is optional; a low thump is generated when it is absent. The ES7210 microphones detect sound bursts only. There is no speech recognition.

WAV format: PCM, 16-bit, mono, 22,050 Hz. See [assets/sounds/README.md](assets/sounds/README.md).

## Build and flash

Activate ESP-IDF and open PowerShell in `firmware/esp-idf`:

```powershell
idf.py build
idf.py -p COM4 -b 460800 flash monitor
```

The exact serial port may differ. See [firmware/esp-idf/README.md](firmware/esp-idf/README.md) for environment setup and troubleshooting.
