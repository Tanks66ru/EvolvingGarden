#include "platform/persistence.hpp"

#include <cstdio>
#include <type_traits>

namespace evolvinggarden::persistence {
namespace {

constexpr char GardenFile[] = "evolving_garden_save.bin";
constexpr char GardenTempFile[] = "evolving_garden_save.tmp";
constexpr char LegacyFile[] = "evolving_garden_legacy.bin";

static_assert(std::is_trivially_copyable<CreatureSimulation::PersistentState>::value,
    "Desktop snapshots must remain trivially copyable");
static_assert(std::is_trivially_copyable<GardenState>::value,
    "Desktop Garden snapshots must remain trivially copyable");

bool validCreatureState(const CreatureSimulation::PersistentState& state) {
    CreatureSimulation candidate(state.genome);
    return candidate.restorePersistentState(state);
}

bool validGardenState(const GardenState& garden) {
    if (garden.version != GardenStateVersion
        || garden.count == 0U
        || garden.count > MaximumGardenCreatures
        || garden.activeIndex >= garden.count) {
        return false;
    }

    for (std::size_t i = 0; i < garden.count; ++i) {
        if (!validCreatureState(garden.creatures[i])) return false;
    }
    return true;
}

template <typename T>
bool readSnapshot(const char* path, T& snapshot) {
    std::FILE* file = std::fopen(path, "rb");
    if (file == nullptr) return false;

    const bool readOk = std::fread(&snapshot, sizeof(snapshot), 1U, file) == 1U;
    const bool hasNoTrailingData = std::fgetc(file) == EOF;
    const bool closeOk = std::fclose(file) == 0;
    return readOk && hasNoTrailingData && closeOk;
}

template <typename T>
bool writeSnapshot(const char* path, const char* temporaryPath, const T& snapshot) {
    std::FILE* file = std::fopen(temporaryPath, "wb");
    if (file == nullptr) return false;

    const bool writeOk = std::fwrite(&snapshot, sizeof(snapshot), 1U, file) == 1U;
    const bool closeOk = std::fclose(file) == 0;
    if (!writeOk || !closeOk) {
        std::remove(temporaryPath);
        return false;
    }

    // Windows does not replace an existing file with rename(), so remove the
    // old snapshot first. The temporary file prevents partial writes.
    if (std::remove(path) != 0) {
        std::FILE* existing = std::fopen(path, "rb");
        if (existing != nullptr) {
            std::fclose(existing);
            std::remove(temporaryPath);
            return false;
        }
    }

    if (std::rename(temporaryPath, path) != 0) {
        std::remove(temporaryPath);
        return false;
    }
    return true;
}

} // namespace

bool initialize() {
    return true;
}

bool loadGarden(GardenState& garden) {
    GardenState stored{};
    if (readSnapshot(GardenFile, stored) && validGardenState(stored)) {
        garden = stored;
        return true;
    }

    // Migrate a legacy one-creature desktop snapshot if one exists.
    CreatureSimulation::PersistentState legacy{};
    if (!readSnapshot(LegacyFile, legacy) || !validCreatureState(legacy)) return false;

    garden = {};
    garden.count = 1U;
    garden.activeIndex = 0U;
    garden.creatures[0] = legacy;
    return true;
}

bool saveGarden(const GardenState& garden) {
    if (!validGardenState(garden)) return false;
    return writeSnapshot(GardenFile, GardenTempFile, garden);
}

bool load(CreatureSimulation& creature) {
    CreatureSimulation::PersistentState state{};
    if (!readSnapshot(LegacyFile, state)) return false;
    return creature.restorePersistentState(state);
}

bool save(const CreatureSimulation& creature) {
    const CreatureSimulation::PersistentState state = creature.persistentState();
    constexpr char LegacyTempFile[] = "evolving_garden_legacy.tmp";
    return writeSnapshot(LegacyFile, LegacyTempFile, state);
}

} // namespace evolvinggarden::persistence
