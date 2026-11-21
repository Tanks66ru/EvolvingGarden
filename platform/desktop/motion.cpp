#include "platform/motion.hpp"

namespace evolvinggarden::motion {
bool initialize() { return false; }
void update(float) {}
State state() { return {}; }
bool requestWake() { return false; }
} // namespace evolvinggarden::motion
