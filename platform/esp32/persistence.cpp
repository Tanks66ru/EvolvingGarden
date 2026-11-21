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
constexpr char Key[] = "pet_state";

static_assert(std::is_trivially_copyable<CreatureSimulation::PersistentState>::value,
    "NVS snapshots must remain trivially copyable");

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

bool load(CreatureSimulation& creature) {
    nvs_handle_t handle{};
    esp_err_t result = nvs_open(Namespace, NVS_READONLY, &handle);
    if (result == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(Tag, "No previous save; starting a new creature");
        return false;
    }
    if (result != ESP_OK) {
        ESP_LOGE(Tag, "Cannot open save namespace: %s", esp_err_to_name(result));
        return false;
    }

    CreatureSimulation::PersistentState state{};
    std::size_t size = sizeof(state);
    result = nvs_get_blob(handle, Key, &state, &size);
    nvs_close(handle);

    if (result == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(Tag, "No previous save; starting a new creature");
        return false;
    }
    if (result != ESP_OK || size != sizeof(state)) {
        ESP_LOGW(Tag, "Save is unreadable or from an incompatible firmware version");
        return false;
    }
    if (!creature.restorePersistentState(state)) {
        ESP_LOGW(Tag, "Save contents failed validation; starting a new creature");
        return false;
    }

    ESP_LOGI(Tag, "Creature restored: %lu steps, %.0f%% bond",
        static_cast<unsigned long>(creature.stepsTaken()),
        creature.affectionToPlayer());
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
    result = nvs_set_blob(handle, Key, &state, sizeof(state));
    if (result == ESP_OK) result = nvs_commit(handle);
    nvs_close(handle);

    if (result != ESP_OK) {
        ESP_LOGE(Tag, "Save failed: %s", esp_err_to_name(result));
        return false;
    }
    ESP_LOGD(Tag, "Creature state saved (%u bytes)", static_cast<unsigned>(sizeof(state)));
    return true;
}

} // namespace evolvinggarden::persistence
