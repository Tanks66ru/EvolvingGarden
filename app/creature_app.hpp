#pragma once

namespace evolvinggarden::app {

// Call after LVGL and the display driver are initialized, while holding the LVGL lock.
void initialize();

// Advance simulation and refresh the shared LVGL interface.
void update(float deltaSeconds);

// Toggle diagnostics from the physical BOOT key.
void toggleDebugPanel();

// Open or close the creature menu from the PMIC power key.
void toggleMenuPanel();

} // namespace evolvinggarden::app
