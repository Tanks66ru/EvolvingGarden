#pragma once

namespace evolvinggarden::app {

// Call after LVGL and the display driver are initialized, while holding the LVGL lock.
void initialize();

// Advance simulation and refresh the shared LVGL interface.
void update(float deltaSeconds);

} // namespace evolvinggarden::app
