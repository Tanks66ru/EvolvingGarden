#pragma once

#include "simulation/creature_simulation.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace evolvinggarden::persistence {

inline constexpr std::uint32_t GardenStateVersion = 1U;
inline constexpr std::size_t MaximumGardenCreatures = 6U;

struct GardenState {
    std::uint32_t version = GardenStateVersion;
    std::uint8_t count = 0;
    std::uint8_t activeIndex = 0;
    std::uint16_t reserved = 0;
    std::array<CreatureSimulation::PersistentState, MaximumGardenCreatures> creatures{};
};

bool initialize();

// Multi-creature Garden storage. loadGarden also migrates the previous single-pet save.
bool loadGarden(GardenState& garden);
bool saveGarden(const GardenState& garden);

// Legacy single-creature helpers retained for compatibility with older callers.
bool load(CreatureSimulation& creature);
bool save(const CreatureSimulation& creature);

} // namespace evolvinggarden::persistence
