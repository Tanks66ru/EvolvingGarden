# EvolvingGarden PC simulator

This is a desktop host for the same LVGL application used by the ESP32 firmware. It compiles:

- `app/creature_app.cpp` - the shared creature UI, animation and update loop
- `platform/desktop/audio.cpp` - SDL audio input/output for a PC
- LVGL 9.5 with its official SDL2 display and input drivers

It does not use the Raylib laboratory renderer and does not contain a second implementation of the creature UI.

## Requirements

- Windows 10/11
- Visual Studio 2022 with the Desktop development with C++ workload
- CMake 3.24 or newer
- Ninja
- Internet access on the first configure/build to download pinned SDL2 and LVGL sources

SDL2 and LVGL are fetched and built by CMake. No separate SDL installation is required.

## Build

Run from the repository root in Developer PowerShell for VS 2022:

```powershell
cmake -S simulator -B build-simulator -G Ninja
cmake --build build-simulator
```

Run:

```powershell
.\build-simulator\EvolvingGardenSimulator.exe
```

The simulator opens the same 466 x 466 LVGL UI as the device, scaled up to 140% for viewing. The mouse acts as the touch input: click the creature/action button and drag horizontally to trigger the same swipe gestures.

The desktop audio backend uses the default Windows playback and capture devices. If Windows denies microphone permission or no device is available, the UI still runs and the debug panel reports the unavailable device.

The simulator stores up to six creatures in `evolving_garden_save.bin` in its working directory. This file is local runtime data and is ignored by Git.

## Important

This is a UI and application simulator, not a full ESP32 hardware emulator. It runs the same application code and LVGL widgets, but cannot reproduce the CO5300 panel's exact color response, touch controller behavior, timing jitter, PSRAM limits or audio codec characteristics.
