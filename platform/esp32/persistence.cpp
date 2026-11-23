#include "platform/persistence.hpp"

#include "esp_err.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

#include <type_traits>

namespace evolvinggarden::persistence {
namespace {

constexpr char Tag[] = "GardenSave";
constexpr char Namespace[] = "garden";
constexpr char LegacyKey[] = "pet_state";
constexpr char GardenKey[] = "garden_state";

static_assert(std::is_trivially_copyable<CreatureSimulation::PersistentState>::value,
    "NVS snapshots must remain trivially copyable");
static_assert(std::is_trivially_copyable<GardenState>::value,
    "Garden snapshots must remain trivially copyable");

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

} // namespace

bool initialize() {
    const esp_err_t result = nvs_flash_init();
    if (result != ESP_OK) {
        ESP_LOGE(Tag, "NVS initialization failed: %s", esp_err_to_name(result));
        return false;
    }
    ESP_LOGI(Tag, "NVS storage ready");
    return true;
}

bool loadGarden(GardenState& garden) {
    nvs_handle_t handle{};
    esp_err_t result = nvs_open(Namespace, NVS_READONLY, &handle);
    if (result == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(Tag, "No previous Garden save");
        return false;
    }
    if (result != ESP_OK) {
        ESP_LOGE(Tag, "Cannot open Garden namespace: %s", esp_err_to_name(result));
        return false;
    }

    GardenState stored{};
    std::size_t size = sizeof(stored);
    result = nvs_get_blob(handle, GardenKey, &stored, &size);

    if (result == ESP_OK) {
        nvs_close(handle);
        if (size != sizeof(stored) || !validGardenState(stored)) {
            ESP_LOGW(Tag, "Garden save is unreadable or incompatible");
            return false;
        }
        garden = stored;
        ESP_LOGI(Tag, "Garden restored: %u creatures, active slot %u",
            static_cast<unsigned>(garden.count),
            static_cast<unsigned>(garden.activeIndex + 1U));
        return true;
    }

    if (result != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(Tag, "Cannot read Garden save: %s", esp_err_to_name(result));
        nvs_close(handle);
        return false;
    }

    // One-time migration from the original single-creature pet_state blob.
    CreatureSimulation::PersistentState legacy{};
    size = sizeof(legacy);
    result = nvs_get_blob(handle, LegacyKey, &legacy, &size);
    nvs_close(handle);

    if (result != ESP_OK || size != sizeof(legacy) || !validCreatureState(legacy)) {
        ESP_LOGI(Tag, "No compatible previous save; a new Garden will be created");
        return false;
    }

    garden = {};
    garden.version = GardenStateVersion;
    garden.count = 1U;
    garden.activeIndex = 0U;
    garden.creatures[0] = legacy;
    ESP_LOGI(Tag, "Migrated the previous single creature into Garden");
    return true;
}

bool saveGarden(const GardenState& garden) {
    if (!validGardenState(garden)) {
        ESP_LOGE(Tag, "Refusing to save an invalid Garden snapshot");
        return false;
    }

    nvs_handle_t handle{};
    esp_err_t result = nvs_open(Namespace, NVS_READWRITE, &handle);
    if (result != ESP_OK) {
        ESP_LOGE(Tag, "Cannot open Garden namespace: %s", esp_err_to_name(result));
        return false;
    }

    result = nvs_set_blob(handle, GardenKey, &garden, sizeof(garden));
    if (result == ESP_OK) result = nvs_commit(handle);
    nvs_close(handle);

    if (result != ESP_OK) {
        ESP_LOGE(Tag, "Garden save failed: %s", esp_err_to_name(result));
        return false;
    }
    ESP_LOGD(Tag, "Garden saved (%u creatures, %u bytes)",
        static_cast<unsigned>(garden.count), static_cast<unsigned>(sizeof(garden)));
    return true;
}

// Legacy single-creature helpers are kept while older tools and branches migrate.
bool load(CreatureSimulation& creature) {
    nvs_handle_t handle{};
    esp_err_t result = nvs_open(Namespace, NVS_READONLY, &handle);
    if (result == ESP_ERR_NVS_NOT_FOUND) return false;
    if (result != ESP_OK) {
        ESP_LOGE(Tag, "Cannot open save namespace: %s", esp_err_to_name(result));
        return false;
    }

    CreatureSimulation::PersistentState state{};
    std::size_t size = sizeof(state);
    result = nvs_get_blob(handle, LegacyKey, &state, &size);
    nvs_close(handle);

    if (result != ESP_OK || size != sizeof(state) || !creature.restorePersistentState(state)) {
        ESP_LOGW(Tag, "Legacy save is unreadable or incompatible");
        return false;
    }
    return true;
}

bool save(const CreatureSimulation& creature) {
    nvs_handle_t handle{};
    esp_err_t result = nvs_open(Namespace, NVS_READWRITE, &handle);
    if (result != ESP_OK) {
        ESP_LOGE(Tag, "Cannot open save namespace: %s", esp_err_to_name(result));
        return false;
    }

    const CreatureSimulation::PersistentState state = creature.persistentState();
    result = nvs_set_blob(handle, LegacyKey, &state, sizeof(state));
    if (result == ESP_OK) result = nvs_commit(handle);
    nvs_close(handle);

    if (result != ESP_OK) {
        ESP_LOGE(Tag, "Legacy save failed: %s", esp_err_to_name(result));
        return false;
    }
    return true;
}

} // namespace evolvinggarden::persistence
