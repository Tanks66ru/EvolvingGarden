#pragma once

#include "simulation/creature_simulation.hpp"

namespace evolvinggarden::persistence {

bool initialize();
bool load(CreatureSimulation& creature);
bool save(const CreatureSimulation& creature);

} // namespace evolvinggarden::persistence
