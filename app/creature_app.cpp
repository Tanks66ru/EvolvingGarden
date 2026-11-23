#include "lvgl.h"

#include "simulation/creature_simulation.hpp"
#include "creatures/name_table.hpp"
#include "platform/audio.hpp"
#include "platform/motion.hpp"
#include "platform/persistence.hpp"
#include "third_party/pip_face/face.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <esp_random.h>

using namespace evolvinggarden;

namespace evolvinggarden::app {
namespace {

struct PetUi {
    lv_obj_t* body = nullptr;
    lv_obj_t* eyeCanvas = nullptr;
    lv_draw_buf_t* eyeBuffer = nullptr;
    lv_obj_t* eyes[2]{};
    lv_obj_t* pupils[2]{};
    lv_obj_t* highlights[2]{};
    lv_obj_t* secondaryHighlights[2]{};
    lv_obj_t* appendages[3]{};
    lv_obj_t* screen = nullptr;
    lv_obj_t* status = nullptr;
    lv_obj_t* impactRing = nullptr;
    lv_obj_t* debugCloseButton = nullptr;
    lv_obj_t* debugPanel = nullptr;
    lv_obj_t* debugContentCard = nullptr;
    lv_obj_t* debugContentLabel = nullptr;
    lv_obj_t* debugTabButtons[3]{};
    lv_obj_t* menuPanel = nullptr;
    lv_obj_t* volumePanel = nullptr;
    lv_obj_t* volumeSlider = nullptr;
    lv_obj_t* volumeLabel = nullptr;
    lv_obj_t* gardenPanel = nullptr;
    lv_obj_t* gardenCountLabel = nullptr;
    lv_obj_t* gardenRows[persistence::MaximumGardenCreatures]{};
    lv_obj_t* gardenRowLabels[persistence::MaximumGardenCreatures]{};
    lv_obj_t* gardenDeleteButtons[persistence::MaximumGardenCreatures]{};
    lv_obj_t* debugAddCreatureButton = nullptr;
};

struct CreatureTouchGesture {
    bool active = false;
    bool hasAngle = false;
    bool petTriggered = false;
    int direction = 0;
    float lastAngle = 0.0F;
    float accumulatedAngle = 0.0F;
    float pathLength = 0.0F;
    float lastPetTime = -10.0F;
    float startX = 0.0F;
    float startY = 0.0F;
};

CreatureSimulation pet(makeDefaultGenome());
PetUi ui;
face_t pipEyeFace{};
persistence::GardenState gardenState{};
int gardenRowIndices[persistence::MaximumGardenCreatures]{};
constexpr int PipEyeCanvasWidth = 320;
constexpr int PipEyeCanvasHeight = 190;
enum class LabPage : std::uint8_t { Device, Creature, Gens };
LabPage activeLabPage = LabPage::Device;
int deviceBatteryPercent = -1;
bool deviceBatteryCharging = false;
CreatureTouchGesture creatureTouch;
float animationTime = 0.0F;
float messageUntil = 0.0F;
float nextDebugRefresh = 0.0F;
float lastHitAnimation = -10.0F;
float hitDirection = 1.0F;
float hitPointX = 233.0F;
float hitPointY = 233.0F;
float deviceRoll = 0.0F;
bool storageReady = false;
bool restoredFromSave = false;
bool saveRequested = false;
float saveInterval = 0.0F;
float secondsSinceSave = 1000.0F;
float devicePitch = 0.0F;
float backgroundRefreshSeconds = 0.0F;
std::uint32_t currentBackgroundColor = 0x111B24;
const char* messageOverride = nullptr;
const char* lastStatus = nullptr;
std::uint32_t lastMotionSteps = 0;
std::uint32_t lastShakeEvents = 0;
std::uint32_t lastTiltEvents = 0;
std::uint32_t lastRotationEvents = 0;
Reaction lastAudioReaction = Reaction::None;

lv_color_t color(std::uint32_t hex) {
    return lv_color_hex(hex);
}

void makeFlatObject(lv_obj_t* object, std::uint32_t background, int radius) {
    lv_obj_set_style_bg_color(object, color(background), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(object, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(object, radius, LV_PART_MAIN);
    lv_obj_set_style_border_width(object, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(object, 0, LV_PART_MAIN);
}

lv_obj_t* makeLabel(lv_obj_t* parent, const char* text, const lv_font_t* font,
                    std::uint32_t textColor) {
    lv_obj_t* label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, color(textColor), LV_PART_MAIN);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    return label;
}

void setStatus(const char* text) {
    if (ui.status != nullptr && text != nullptr && text != lastStatus) {
        lv_label_set_text(ui.status, text);
        lastStatus = text;
    }
}

const char* reactionName(Reaction reaction) {
    switch (reaction) {
    case Reaction::Happy: return "HAPPY";
    case Reaction::Sad: return "SAD";
    case Reaction::Angry: return "ANGRY";
    case Reaction::Fearful: return "SCARED";
    case Reaction::Surprised: return "SURPRISED";
    case Reaction::Curious: return "CURIOUS";
    case Reaction::Affectionate: return "AFFECTIONATE";
    case Reaction::Disgusted: return "DISGUSTED";
    case Reaction::None: break;
    }
    return "IDLE";
}

const char* actionTag(Action action) {
    switch (action) {
    case Action::Explore: return "EXPLORING";
    case Action::Play: return "PLAYING";
    case Action::Rest: return "RESTING";
    case Action::SeekCompany: return "LOOKING FOR YOU";
    case Action::Observe: return "OBSERVING";
    case Action::Forage: return "LOOKING FOR FOOD";
    case Action::Idle: break;
    }
    return "WANDERING";
}

std::uint32_t blendHex(std::uint32_t from, std::uint32_t to, float amount) {
    const float t = std::clamp(amount, 0.0F, 1.0F);
    const auto channel = [t](std::uint32_t a, std::uint32_t b, unsigned shift) {
        const float first = static_cast<float>((a >> shift) & 0xFFU);
        const float second = static_cast<float>((b >> shift) & 0xFFU);
        return static_cast<std::uint32_t>(first + (second - first) * t + 0.5F);
    };
    return (channel(from, to, 16) << 16)
        | (channel(from, to, 8) << 8)
        | channel(from, to, 0);
}

void updateBackground(float dt) {
    if (ui.screen == nullptr) return;
    backgroundRefreshSeconds += dt;
    if (backgroundRefreshSeconds < 0.12F) return;
    backgroundRefreshSeconds = 0.0F;

    const auto& genes = pet.genome().personality;
    const auto trait01 = [](int value) {
        return std::clamp((static_cast<float>(value) + 100.0F) / 200.0F, 0.0F, 1.0F);
    };
    const float optimism = trait01(genes.optimism);
    const float friendliness = trait01(genes.friendliness);
    const float moodTone = std::clamp((pet.mood() + 100.0F) / 200.0F, 0.0F, 1.0F);
    const float characterTone = optimism * 0.65F + friendliness * 0.35F;

    // Negative mood moves toward a deep blue; positive mood moves toward a muted green.
    std::uint32_t target = blendHex(0x10182D, 0x183B31,
        moodTone * 0.84F + characterTone * 0.16F);
    target = blendHex(target, 0x1A3A35, characterTone * 0.10F);

    const auto& emotions = pet.emotions();
    float strongestEmotion = emotions.value(Emotion::Joy);
    std::uint32_t emotionTint = 0x1D4435;
    const auto considerTint = [&strongestEmotion, &emotionTint](
        float strength, std::uint32_t tint) {
        if (strength > strongestEmotion) {
            strongestEmotion = strength;
            emotionTint = tint;
        }
    };
    considerTint(emotions.value(Emotion::Sadness), 0x111D3B);
    considerTint(emotions.value(Emotion::Anger), 0x382027);
    considerTint(emotions.value(Emotion::Fear), 0x202541);
    considerTint(emotions.value(Emotion::Curiosity), 0x1E3543);
    considerTint(emotions.value(Emotion::Affection), 0x194238);
    considerTint(emotions.value(Emotion::Disgust), 0x302428);
    const float emotionInfluence = std::clamp(
        (strongestEmotion - 24.0F) / 220.0F, 0.0F, 0.30F);
    target = blendHex(target, emotionTint, emotionInfluence);

    // Ease the palette toward its target rather than flashing on every reaction.
    currentBackgroundColor = blendHex(currentBackgroundColor, target, 0.24F);
    lv_obj_set_style_bg_color(ui.screen, color(currentBackgroundColor), LV_PART_MAIN);
}

void setMessage(const char* message, float duration = 1.2F) {
    messageOverride = message;
    messageUntil = animationTime + duration;
    setStatus(message);
}

void requestSave() {
    saveRequested = true;
}

void captureActiveCreatureSnapshot() {
    if (gardenState.count == 0U || gardenState.activeIndex >= gardenState.count) return;
    gardenState.creatures[gardenState.activeIndex] = pet.persistentState();
}

bool saveGardenImmediately() {
    if (!storageReady || gardenState.count == 0U) return false;
    captureActiveCreatureSnapshot();
    if (!persistence::saveGarden(gardenState)) return false;
    saveRequested = false;
    saveInterval = 0.0F;
    secondsSinceSave = 0.0F;
    return true;
}

void updatePersistence(float dt) {
    if (!storageReady) return;
    saveInterval += dt;
    secondsSinceSave += dt;

    // Keep flash writes sparse, but flush interaction and step changes promptly.
    if ((saveRequested && secondsSinceSave >= 3.0F) || saveInterval >= 60.0F) {
        if (saveGardenImmediately()) {
            saveRequested = false;
        } else {
            saveInterval = 0.0F;
            secondsSinceSave = 0.0F;
        }
    }
}

void performFeed() {
    motion::requestWake();
    if (pet.applyInteraction(Interaction::Feed)) {
        setMessage("YUM!");
        audio::play(audio::Cue::Button);
        requestSave();
    }
}

void performPet() {
    motion::requestWake();
    if (pet.applyInteraction(Interaction::Pet)) {
        setMessage("ENJOYING THE SCRATCH", 1.0F);
        requestSave();
    }
}

void performHit(float touchX, float touchY) {
    motion::requestWake();
    hitDirection = touchX < 233.0F ? 1.0F : -1.0F;
    if (pet.applyInteraction(Interaction::Hit)) {
        lastHitAnimation = animationTime;
        hitPointX = std::clamp(touchX, 18.0F, 448.0F);
        hitPointY = std::clamp(touchY, 18.0F, 448.0F);
        if (ui.impactRing != nullptr) {
            lv_obj_clear_flag(ui.impactRing, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_to_index(ui.impactRing, -1);
        }
        audio::play(audio::Cue::Hit);
        setMessage(pet.enjoysSparring() ? "AGAIN! AGAIN!" : "HEY! THAT HURT!");
        requestSave();
    }
}

void refreshDebugPanel() {
    if (ui.debugContentLabel == nullptr) return;
    const CreatureNeeds& needs = pet.needs();
    const EmotionalState& emotions = pet.emotions();
    const motion::State sensor = motion::state();
    const Genome& genome = pet.genome();
    // Reused by the single LVGL/UI task; keep the large formatting buffer off its stack.
    static char text[1500];
    static char batteryText[24];
    text[0] = '\0';
    batteryText[0] = '\0';
    if (deviceBatteryPercent < 0) std::snprintf(batteryText, sizeof(batteryText), "NOT DETECTED");
    else std::snprintf(batteryText, sizeof(batteryText), "%d%%", deviceBatteryPercent);

    switch (activeLabPage) {
    case LabPage::Device:
        std::snprintf(text, sizeof(text),
            "DEVICE STATUS\n\n"
            "BATTERY     %s\nCHARGING    %s\n"
            "MICROPHONE  %s  %u%%\nSPEAKER     %s\nVOLUME      %u%%\n"
            "STORAGE     %s\nGARDEN      %u/%u\nSAVE        %s\nMOTION      %s\n"
            "ORIENTATION %s\nDISPLAY     %s\n\n"
            "MOVEMENT\nSTEPS       %lu\nSHAKES      %lu\nTILTS       %lu\nROTATIONS   %lu\n\n"
            "SENSORS\nROLL        %+.1f deg\nPITCH       %+.1f deg\n"
            "ACCEL       %.2f m/s2\nGYRO        %.2f rad/s",
            batteryText, deviceBatteryCharging ? "YES" : "NO",
            audio::microphoneReady() ? "READY" : "OFF",
            static_cast<unsigned>(audio::microphoneLevelPercent()),
            audio::speakerReady() ? "READY" : "OFF",
            static_cast<unsigned>(audio::volumePercent()),
            storageReady ? "READY" : "OFF",
            static_cast<unsigned>(gardenState.count),
            static_cast<unsigned>(persistence::MaximumGardenCreatures),
            saveRequested ? "PENDING" : "CLEAN",
            sensor.ready ? "READY" : "OFF",
            sensor.orientationCalibrated ? "CALIBRATED" : "HOLD FLAT",
            sensor.screenSleeping ? "SLEEPING" : "AWAKE",
            static_cast<unsigned long>(pet.stepsTaken()),
            static_cast<unsigned long>(sensor.shakeEvents),
            static_cast<unsigned long>(sensor.tiltEvents),
            static_cast<unsigned long>(sensor.rotationEvents),
            sensor.rollDegrees, sensor.pitchDegrees,
            sensor.linearAcceleration, sensor.angularVelocity);
        break;

    case LabPage::Creature:
        std::snprintf(text, sizeof(text),
            "LIVE CREATURE STATUS\n\n"
            "MOOD        %+.0f\nREACTION    %s\nACTION      %s\n"
            "AFFECTION   %3.0f%%\nSTEPS       %lu\n\n"
            "NEEDS\nENERGY      %s  (DEVICE BATTERY)\n"
            "HUNGER      %3.0f%%\nSOCIAL      %3.0f%%\nBOREDOM     %3.0f%%\n\n"
            "EMOTIONS\nJOY         %3.0f\nSADNESS     %3.0f\n"
            "ANGER       %3.0f\nFEAR        %3.0f\nSURPRISE    %3.0f\n"
            "CURIOSITY   %3.0f\nAFFECTION   %3.0f\nDISGUST     %3.0f",
            pet.mood(), reactionName(pet.reaction().kind()), actionTag(pet.action()),
            pet.affectionToPlayer(), static_cast<unsigned long>(pet.stepsTaken()),
            batteryText, needs.hunger, needs.social, needs.boredom,
            emotions.value(Emotion::Joy), emotions.value(Emotion::Sadness),
            emotions.value(Emotion::Anger), emotions.value(Emotion::Fear),
            emotions.value(Emotion::Surprise), emotions.value(Emotion::Curiosity),
            emotions.value(Emotion::Affection), emotions.value(Emotion::Disgust));
        break;

    case LabPage::Gens: {
        const GeneratedCreatureName name = generateCreatureName(genome.seed);
        char fullName[96]{};
        if (name.title != nullptr)
            std::snprintf(fullName, sizeof(fullName), "%s %s %s", name.title, name.adjective, name.noun);
        else
            std::snprintf(fullName, sizeof(fullName), "%s %s", name.adjective, name.noun);
        const auto& p = genome.personality;
        const auto& m = genome.morphology;
        std::snprintf(text, sizeof(text),
            "BIRTH RECORD\n\nNAME\n%s\nGENOME SEED  %08lX\n"
            "TRAIT BUDGET %u  |  LEFT %d\n\nPERSONALITY GENES\n"
            "Friendliness  %d\nOptimism      %d\nSociability   %d\n"
            "Curiosity     %d\nActivity      %d\nSensitivity   %d\n\n"
            "MORPHOLOGY GENES\nBody width       %u\nBody height      %u\n"
            "Roundness        %u\nAsymmetry        %u\nSurface detail   %u\n"
            "Appendage count  %u\nAppendage length %u\nAppendage curve  %u\n"
            "Eye size         %u\nEye spacing      %u\nPigmentation     %u\n"
            "Pattern density  %u\nPalette          %u",
            fullName, static_cast<unsigned long>(genome.seed),
            static_cast<unsigned>(genome.traitPointBudget), genome.remainingTraitPoints(),
            static_cast<int>(p.friendliness), static_cast<int>(p.optimism),
            static_cast<int>(p.sociability), static_cast<int>(p.curiosity),
            static_cast<int>(p.activity), static_cast<int>(p.sensitivity),
            static_cast<unsigned>(m.bodyWidth), static_cast<unsigned>(m.bodyHeight),
            static_cast<unsigned>(m.roundness), static_cast<unsigned>(m.asymmetry),
            static_cast<unsigned>(m.surfaceComplexity), static_cast<unsigned>(m.appendageCount),
            static_cast<unsigned>(m.appendageLength), static_cast<unsigned>(m.appendageCurvature),
            static_cast<unsigned>(m.eyeSize), static_cast<unsigned>(m.eyeSpacing),
            static_cast<unsigned>(m.pigmentation), static_cast<unsigned>(m.patternDensity),
            static_cast<unsigned>(m.palette));
        break;
    }
    }
    lv_label_set_text(ui.debugContentLabel, text);
}


std::uint32_t createUniqueCreatureSeed() {
    for (int attempt = 0; attempt < 16; ++attempt) {
        std::uint32_t seed = esp_random();
        if (seed == 0U) seed = 0xA341316CU + static_cast<std::uint32_t>(attempt);
        bool used = false;
        for (std::size_t i = 0; i < gardenState.count; ++i) {
            if (gardenState.creatures[i].genome.seed == seed) {
                used = true;
                break;
            }
        }
        if (!used) return seed;
    }
    return mixNameSeed(esp_random() ^ static_cast<std::uint32_t>(animationTime * 1000.0F));
}

int seededRange(std::uint32_t seed, std::uint32_t salt, int minimum, int maximum) {
    if (maximum <= minimum) return minimum;
    return minimum + static_cast<int>(
        mixNameSeed(seed ^ salt) % static_cast<std::uint32_t>(maximum - minimum + 1));
}

Genome createGardenGenome(std::uint32_t seed) {
    Genome genome = makeDefaultGenome();
    genome.seed = seed;
    auto& p = genome.personality;
    p.friendliness = static_cast<std::int16_t>(seededRange(seed, 0x1A01U, -30, 80));
    p.optimism = static_cast<std::int16_t>(seededRange(seed, 0x1A02U, -35, 75));
    p.sociability = static_cast<std::int16_t>(seededRange(seed, 0x1A03U, -45, 75));
    p.curiosity = static_cast<std::int16_t>(seededRange(seed, 0x1A04U, -25, 90));
    p.activity = static_cast<std::int16_t>(seededRange(seed, 0x1A05U, -40, 85));
    p.sensitivity = static_cast<std::int16_t>(seededRange(seed, 0x1A06U, -70, 75));

    // Stay inside the inherited point budget while allowing different personalities.
    int excess = p.netPointCost() - static_cast<int>(genome.traitPointBudget);
    std::int16_t* traits[] = {
        &p.sensitivity, &p.sociability, &p.activity,
        &p.optimism, &p.friendliness, &p.curiosity
    };
    for (std::int16_t* trait : traits) {
        if (excess <= 0) break;
        const int removable = static_cast<int>(*trait) + 100;
        const int reduction = std::min(excess, std::max(0, removable));
        *trait = static_cast<std::int16_t>(static_cast<int>(*trait) - reduction);
        excess -= reduction;
    }

    auto& m = genome.morphology;
    m.bodyWidth = static_cast<std::uint8_t>(seededRange(seed, 0x2B01U, 25, 90));
    m.bodyHeight = static_cast<std::uint8_t>(seededRange(seed, 0x2B02U, 25, 90));
    m.roundness = static_cast<std::uint8_t>(seededRange(seed, 0x2B03U, 15, 95));
    m.asymmetry = static_cast<std::uint8_t>(seededRange(seed, 0x2B04U, 5, 95));
    m.surfaceComplexity = static_cast<std::uint8_t>(seededRange(seed, 0x2B05U, 15, 95));
    m.appendageCount = static_cast<std::uint8_t>(seededRange(seed, 0x2B06U, 1, 5));
    m.appendageLength = static_cast<std::uint8_t>(seededRange(seed, 0x2B07U, 20, 95));
    m.appendageCurvature = static_cast<std::uint8_t>(seededRange(seed, 0x2B08U, 5, 95));
    m.eyeSize = static_cast<std::uint8_t>(seededRange(seed, 0x2B09U, 35, 95));
    m.eyeSpacing = static_cast<std::uint8_t>(seededRange(seed, 0x2B0AU, 20, 85));
    m.pigmentation = static_cast<std::uint8_t>(seededRange(seed, 0x2B0BU, 20, 95));
    m.patternDensity = static_cast<std::uint8_t>(seededRange(seed, 0x2B0CU, 5, 90));
    m.palette = static_cast<std::uint8_t>(seededRange(seed, 0x2B0DU, 0, 4));
    genome.normalize();
    return genome;
}

void formatCreatureName(std::uint32_t seed, char* destination, std::size_t capacity) {
    if (destination == nullptr || capacity == 0U) return;
    const GeneratedCreatureName name = generateCreatureName(seed);
    if (name.title != nullptr) {
        std::snprintf(destination, capacity, "%s %s %s",
            name.title, name.adjective, name.noun);
    } else {
        std::snprintf(destination, capacity, "%s %s", name.adjective, name.noun);
    }
}

void refreshGardenPanel() {
    if (ui.gardenPanel == nullptr) return;

    char countText[24]{};
    std::snprintf(countText, sizeof(countText), "%u / %u",
        static_cast<unsigned>(gardenState.count),
        static_cast<unsigned>(persistence::MaximumGardenCreatures));
    if (ui.gardenCountLabel != nullptr) lv_label_set_text(ui.gardenCountLabel, countText);

    for (std::size_t i = 0; i < persistence::MaximumGardenCreatures; ++i) {
        const bool occupied = i < gardenState.count;
        if (ui.gardenRows[i] == nullptr || ui.gardenDeleteButtons[i] == nullptr) continue;
        if (!occupied) {
            lv_obj_add_flag(ui.gardenRows[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui.gardenDeleteButtons[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }

        char name[88]{};
        char rowText[112]{};
        formatCreatureName(gardenState.creatures[i].genome.seed, name, sizeof(name));
        if (i == gardenState.activeIndex) {
            std::snprintf(rowText, sizeof(rowText), "ACTIVE  |  %s", name);
        } else {
            std::snprintf(rowText, sizeof(rowText), "%s", name);
        }
        lv_label_set_text(ui.gardenRowLabels[i], rowText);
        lv_obj_set_style_bg_color(ui.gardenRows[i],
            color(i == gardenState.activeIndex ? 0x365B55 : 0x263A45), LV_PART_MAIN);
        lv_obj_clear_flag(ui.gardenRows[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ui.gardenDeleteButtons[i], LV_OBJ_FLAG_HIDDEN);
        if (gardenState.count <= 1U) {
            lv_obj_add_state(ui.gardenDeleteButtons[i], LV_STATE_DISABLED);
        } else {
            lv_obj_clear_state(ui.gardenDeleteButtons[i], LV_STATE_DISABLED);
        }
    }
}

bool restoreGardenCreature(std::size_t index) {
    if (index >= gardenState.count) return false;
    CreatureSimulation selected(gardenState.creatures[index].genome);
    if (!selected.restorePersistentState(gardenState.creatures[index])) return false;

    pet = selected;
    gardenState.activeIndex = static_cast<std::uint8_t>(index);
    (void)pet.applyInteraction(Interaction::BroughtToForeground);
    gardenState.creatures[index] = pet.persistentState();
    lastAudioReaction = Reaction::None;
    lastHitAnimation = -10.0F;
    backgroundRefreshSeconds = 0.0F;
    return true;
}

void activateGardenCreature(std::size_t index) {
    if (index >= gardenState.count) return;
    if (index != gardenState.activeIndex) {
        (void)pet.applyInteraction(Interaction::ReturnedToFarm);
        captureActiveCreatureSnapshot();
        if (!restoreGardenCreature(index)) {
            setMessage("CREATURE SAVE COULD NOT BE LOADED");
            return;
        }
        (void)saveGardenImmediately();
    }
    hideGardenPanel();
    setMessage("CREATURE SELECTED", 1.0F);
}

void addCreatureToGarden() {
    if (gardenState.count >= persistence::MaximumGardenCreatures) {
        setMessage("GARDEN IS FULL");
        return;
    }

    const std::uint32_t seed = createUniqueCreatureSeed();
    CreatureSimulation newcomer(createGardenGenome(seed));
    gardenState.creatures[gardenState.count] = newcomer.persistentState();
    ++gardenState.count;
    (void)saveGardenImmediately();
    refreshGardenPanel();
    refreshDebugPanel();
    setMessage("NEW CREATURE ADDED", 1.6F);
}

void deleteGardenCreature(std::size_t index) {
    if (index >= gardenState.count) return;
    if (gardenState.count <= 1U) {
        setMessage("KEEP AT LEAST ONE CREATURE");
        return;
    }

    captureActiveCreatureSnapshot();
    const bool deletingActive = index == gardenState.activeIndex;
    for (std::size_t i = index; i + 1U < gardenState.count; ++i) {
        gardenState.creatures[i] = gardenState.creatures[i + 1U];
    }
    --gardenState.count;
    gardenState.creatures[gardenState.count] = {};

    if (deletingActive) {
        const std::size_t replacement = std::min(index,
            static_cast<std::size_t>(gardenState.count - 1U));
        if (!restoreGardenCreature(replacement)) {
            setMessage("COULD NOT LOAD REPLACEMENT");
            return;
        }
    } else if (index < gardenState.activeIndex) {
        --gardenState.activeIndex;
    }

    (void)saveGardenImmediately();
    refreshGardenPanel();
    refreshDebugPanel();
    setMessage("CREATURE REMOVED", 1.2F);
}

void onGardenRowClicked(lv_event_t* event) {
    const auto* index = static_cast<const int*>(lv_event_get_user_data(event));
    if (index == nullptr || *index < 0) return;
    activateGardenCreature(static_cast<std::size_t>(*index));
}

void onGardenDeleteClicked(lv_event_t* event) {
    const auto* index = static_cast<const int*>(lv_event_get_user_data(event));
    if (index == nullptr || *index < 0) return;
    deleteGardenCreature(static_cast<std::size_t>(*index));
}

void onDebugAddCreatureClicked(lv_event_t*) {
    addCreatureToGarden();
    refreshDebugPanel();
}


void selectLabPage(LabPage page) {
    activeLabPage = page;
    const int activeIndex = static_cast<int>(page);
    for (int i = 0; i < 3; ++i) {
        if (ui.debugTabButtons[i] == nullptr) continue;
        lv_obj_set_style_bg_color(ui.debugTabButtons[i],
            color(i == activeIndex ? 0x72C7A8 : 0x344451), LV_PART_MAIN);
    }
    if (ui.debugContentCard != nullptr)
        lv_obj_scroll_to_y(ui.debugContentCard, 0, LV_ANIM_OFF);
    refreshDebugPanel();
}
void onLabDeviceClicked(lv_event_t*) { selectLabPage(LabPage::Device); }
void onLabCreatureClicked(lv_event_t*) { selectLabPage(LabPage::Creature); }
void onLabGensClicked(lv_event_t*) { selectLabPage(LabPage::Gens); }

lv_obj_t* createLabTabButton(lv_obj_t* parent, int x, const char* text,
                             lv_event_cb_t callback) {
    lv_obj_t* button = lv_button_create(parent);
    lv_obj_set_size(button, 126, 40);
    lv_obj_set_pos(button, x, 48);
    lv_obj_set_style_bg_color(button, color(0x344451), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(button, 13, LV_PART_MAIN);
    lv_obj_set_style_border_width(button, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* label = makeLabel(button, text, &lv_font_montserrat_12, 0xEAF2EF);
    lv_obj_center(label);
    return button;
}

void showDebugPanel() {
    motion::requestWake();
    if (ui.debugPanel == nullptr) return;
    selectLabPage(LabPage::Device);
    refreshDebugPanel();
    lv_obj_clear_flag(ui.debugPanel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_to_index(ui.debugPanel, -1);
}

void hideDebugPanel() {
    motion::requestWake();
    if (ui.debugPanel != nullptr) lv_obj_add_flag(ui.debugPanel, LV_OBJ_FLAG_HIDDEN);
}

void showMenuPanel() {
    motion::requestWake();
    if (ui.menuPanel == nullptr) return;
    lv_obj_clear_flag(ui.menuPanel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_to_index(ui.menuPanel, -1);
}

void hideMenuPanel() {
    motion::requestWake();
    if (ui.menuPanel != nullptr) lv_obj_add_flag(ui.menuPanel, LV_OBJ_FLAG_HIDDEN);
}

void showGardenPanel() {
    motion::requestWake();
    if (ui.gardenPanel == nullptr) return;
    refreshGardenPanel();
    lv_obj_clear_flag(ui.gardenPanel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_to_index(ui.gardenPanel, -1);
}

void hideGardenPanel() {
    motion::requestWake();
    if (ui.gardenPanel != nullptr) lv_obj_add_flag(ui.gardenPanel, LV_OBJ_FLAG_HIDDEN);
}

void updateVolumeLabel(int percent) {
    if (ui.volumeLabel == nullptr) return;
    char text[12]{};
    std::snprintf(text, sizeof(text), "%d%%", percent);
    lv_label_set_text(ui.volumeLabel, text);
}

void showVolumePanel() {
    motion::requestWake();
    if (ui.volumePanel == nullptr) return;
    const int currentVolume = static_cast<int>(audio::volumePercent());
    if (ui.volumeSlider != nullptr) {
        lv_slider_set_value(ui.volumeSlider, currentVolume, LV_ANIM_OFF);
    }
    updateVolumeLabel(currentVolume);
    lv_obj_clear_flag(ui.volumePanel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_to_index(ui.volumePanel, -1);
}

void hideVolumePanel() {
    motion::requestWake();
    if (ui.volumePanel != nullptr) lv_obj_add_flag(ui.volumePanel, LV_OBJ_FLAG_HIDDEN);
}

void onVolumeSliderChanged(lv_event_t* event) {
    lv_obj_t* slider = static_cast<lv_obj_t*>(lv_event_get_target(event));
    const int value = lv_slider_get_value(slider);
    audio::setVolumePercent(static_cast<std::uint8_t>(value));
    updateVolumeLabel(value);
}

void onVolumeBackClicked(lv_event_t*) {
    hideVolumePanel();
    showMenuPanel();
}

void onDebugCloseClicked(lv_event_t*) { hideDebugPanel(); }
void onMenuFeedClicked(lv_event_t*) {
    hideMenuPanel();
    performFeed();
}
void onMenuLabClicked(lv_event_t*) {
    hideMenuPanel();
    showDebugPanel();
}
void onMenuVolumeClicked(lv_event_t*) {
    hideMenuPanel();
    showVolumePanel();
}
void onMenuGardenClicked(lv_event_t*) {
    hideMenuPanel();
    showGardenPanel();
}
void onGardenBackClicked(lv_event_t*) {
    hideGardenPanel();
    showMenuPanel();
}
void onMenuCloseClicked(lv_event_t*) { hideMenuPanel(); }

lv_obj_t* createMenuButton(lv_obj_t* parent, int y, const char* text,
                           std::uint32_t background, lv_event_cb_t callback) {
    lv_obj_t* button = lv_button_create(parent);
    lv_obj_set_size(button, 316, 54);
    lv_obj_set_pos(button, 24, y);
    lv_obj_set_style_bg_color(button, color(background), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(button, 18, LV_PART_MAIN);
    lv_obj_set_style_border_width(button, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(button, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* label = makeLabel(button, text, &lv_font_montserrat_16, 0x14252C);
    lv_obj_center(label);
    return button;
}

void onScreenTouch(lv_event_t*) {
    motion::requestWake();
}

void onBackgroundTapped(lv_event_t*) {
    motion::requestWake();
    lv_indev_t* indev = lv_indev_active();
    if (indev == nullptr) return;
    lv_point_t point{};
    lv_indev_get_point(indev, &point);
    performHit(static_cast<float>(point.x), static_cast<float>(point.y));
}

void onCreaturePointer(lv_event_t* event) {
    const lv_event_code_t code = lv_event_get_code(event);

    if (code == LV_EVENT_PRESSED) {
        const bool wokeScreen = motion::requestWake();
        creatureTouch = {};
        if (wokeScreen) return;

        lv_indev_t* indev = lv_indev_active();
        if (indev == nullptr) return;
        lv_point_t point{};
        lv_indev_get_point(indev, &point);
        creatureTouch.active = true;
        creatureTouch.startX = static_cast<float>(point.x);
        creatureTouch.startY = static_cast<float>(point.y);
        return;
    }

    if (!creatureTouch.active) return;

    if (code == LV_EVENT_PRESSING) {
        lv_indev_t* indev = lv_indev_active();
        if (indev == nullptr) return;

        lv_point_t point{};
        lv_indev_get_point(indev, &point);
        const float x = static_cast<float>(point.x);
        const float y = static_cast<float>(point.y);
        const float dx = x - creatureTouch.startX;
        const float dy = y - creatureTouch.startY;
        creatureTouch.pathLength += std::sqrt(dx * dx + dy * dy);
        creatureTouch.startX = x;
        creatureTouch.startY = y;

        const float offsetX = x - 233.0F;
        const float offsetY = y - 233.0F;
        const float radius = std::sqrt(offsetX * offsetX + offsetY * offsetY);
        if (radius < 52.0F || radius > 218.0F) {
            creatureTouch.hasAngle = false;
            return;
        }

        const float angle = std::atan2(offsetY, offsetX);
        if (!creatureTouch.hasAngle) {
            creatureTouch.lastAngle = angle;
            creatureTouch.hasAngle = true;
            return;
        }

        const float rawDelta = angle - creatureTouch.lastAngle;
        const float delta = std::atan2(std::sin(rawDelta), std::cos(rawDelta));
        creatureTouch.lastAngle = angle;
        if (std::fabs(delta) < 0.10F || std::fabs(delta) > 1.65F) return;

        // Keep a signed angle: clockwise and counter-clockwise strokes are equal.
        // Small reversals naturally cancel instead of permanently locking a direction.
        creatureTouch.accumulatedAngle += delta;

        constexpr float FullCircle = 6.28318530718F;
        if (std::fabs(creatureTouch.accumulatedAngle) >= FullCircle
            && animationTime - creatureTouch.lastPetTime >= 0.70F) {
            performPet();
            creatureTouch.accumulatedAngle -=
                std::copysign(FullCircle, creatureTouch.accumulatedAngle);
            creatureTouch.lastPetTime = animationTime;
            creatureTouch.petTriggered = true;
        }
        return;
    }

    if (code == LV_EVENT_RELEASED) {
        if (std::fabs(creatureTouch.accumulatedAngle) >= 4.7F && !creatureTouch.petTriggered) {
            performPet();
        } else if (creatureTouch.pathLength < 18.0F && !creatureTouch.petTriggered) {
            lv_indev_t* indev = lv_indev_active();
            lv_point_t point{};
            if (indev != nullptr) {
                lv_indev_get_point(indev, &point);
                performHit(static_cast<float>(point.x), static_cast<float>(point.y));
            }
        }
        creatureTouch.active = false;
    }
}

void createCreature(lv_obj_t* parent) {
    // Appendages are created first so the body is drawn over their roots.
    ui.appendages[0] = lv_obj_create(parent);
    lv_obj_add_flag(ui.appendages[0], static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK));
    lv_obj_clear_flag(ui.appendages[0], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(ui.appendages[0], onCreaturePointer, LV_EVENT_ALL, nullptr);
    makeFlatObject(ui.appendages[0], 0x3D817B, LV_RADIUS_CIRCLE);
    lv_obj_set_style_border_width(ui.appendages[0], 3, LV_PART_MAIN);
    lv_obj_set_style_border_color(ui.appendages[0], color(0x21414C), LV_PART_MAIN);
    lv_obj_set_style_radius(ui.appendages[0], LV_RADIUS_CIRCLE, LV_PART_MAIN);

    ui.appendages[1] = lv_obj_create(parent);
    lv_obj_add_flag(ui.appendages[1], static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK));
    lv_obj_clear_flag(ui.appendages[1], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(ui.appendages[1], onCreaturePointer, LV_EVENT_ALL, nullptr);
    makeFlatObject(ui.appendages[1], 0x3D817B, LV_RADIUS_CIRCLE);
    lv_obj_set_style_border_width(ui.appendages[1], 3, LV_PART_MAIN);
    lv_obj_set_style_border_color(ui.appendages[1], color(0x21414C), LV_PART_MAIN);
    lv_obj_set_style_radius(ui.appendages[1], LV_RADIUS_CIRCLE, LV_PART_MAIN);

    ui.appendages[2] = lv_obj_create(parent);
    lv_obj_add_flag(ui.appendages[2], static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK));
    lv_obj_clear_flag(ui.appendages[2], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(ui.appendages[2], onCreaturePointer, LV_EVENT_ALL, nullptr);
    makeFlatObject(ui.appendages[2], 0x3D817B, LV_RADIUS_CIRCLE);
    lv_obj_set_style_border_width(ui.appendages[2], 3, LV_PART_MAIN);
    lv_obj_set_style_border_color(ui.appendages[2], color(0x21414C), LV_PART_MAIN);
    lv_obj_set_style_radius(ui.appendages[2], LV_RADIUS_CIRCLE, LV_PART_MAIN);

    ui.body = lv_obj_create(parent);
    makeFlatObject(ui.body, 0x427E83, LV_RADIUS_CIRCLE);
    lv_obj_set_style_border_width(ui.body, 6, LV_PART_MAIN);
    lv_obj_set_style_border_color(ui.body, color(0x173B48), LV_PART_MAIN);
    // Keep the creature flat: software shadow blur is expensive on the ESP32-S3.
    lv_obj_add_flag(ui.body, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK));
    lv_obj_clear_flag(ui.body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(ui.body, onCreaturePointer, LV_EVENT_ALL, nullptr);

    // Pip's renderer paints both eyes into a small RGB565 canvas. The canvas
    // stays inside the creature silhouette, so its background matches the body.
    ui.eyeBuffer = lv_draw_buf_create(
        PipEyeCanvasWidth, PipEyeCanvasHeight, LV_COLOR_FORMAT_RGB565, 0);
    if (ui.eyeBuffer != nullptr) {
        ui.eyeCanvas = lv_canvas_create(ui.body);
        if (ui.eyeCanvas != nullptr) {
            lv_canvas_set_draw_buf(ui.eyeCanvas, ui.eyeBuffer);
            lv_obj_set_size(ui.eyeCanvas, PipEyeCanvasWidth, PipEyeCanvasHeight);
            lv_obj_set_pos(ui.eyeCanvas, 16, 87);
            lv_obj_clear_flag(ui.eyeCanvas, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_add_flag(ui.eyeCanvas, LV_OBJ_FLAG_EVENT_BUBBLE);
            pipEyeFace.color = 0xFFFF;
            pipEyeFace.pupil_color = 0x18C3;
            pipEyeFace.heart_color = 0xFA2F;
            pipEyeFace.shading = 0.0F;
            pipEyeFace.blush = 0.0F;
        }
    }

    for (int i = 0; i < 2; ++i) {
        ui.eyes[i] = lv_obj_create(ui.body);
        lv_obj_add_flag(ui.eyes[i], LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_clear_flag(ui.eyes[i], LV_OBJ_FLAG_SCROLLABLE);
        makeFlatObject(ui.eyes[i], 0xF1F3EE, LV_RADIUS_CIRCLE);
        lv_obj_set_style_border_width(ui.eyes[i], 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(ui.eyes[i], color(0xAAB8B7), LV_PART_MAIN);

        ui.pupils[i] = lv_obj_create(ui.eyes[i]);
        lv_obj_add_flag(ui.pupils[i], LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_clear_flag(ui.pupils[i], LV_OBJ_FLAG_SCROLLABLE);
        makeFlatObject(ui.pupils[i], 0x15232D, LV_RADIUS_CIRCLE);
        lv_obj_set_style_border_width(ui.pupils[i], 2, LV_PART_MAIN);
        lv_obj_set_style_border_color(ui.pupils[i], color(0x506872), LV_PART_MAIN);

        ui.highlights[i] = lv_obj_create(ui.pupils[i]);
        lv_obj_clear_flag(ui.highlights[i], LV_OBJ_FLAG_SCROLLABLE);
        makeFlatObject(ui.highlights[i], 0xFFFFFF, LV_RADIUS_CIRCLE);
        lv_obj_set_size(ui.highlights[i], 10, 12);
        lv_obj_set_pos(ui.highlights[i], 7, 8);

        ui.secondaryHighlights[i] = lv_obj_create(ui.pupils[i]);
        lv_obj_clear_flag(ui.secondaryHighlights[i], LV_OBJ_FLAG_SCROLLABLE);
        makeFlatObject(ui.secondaryHighlights[i], 0xFFFFFF, LV_RADIUS_CIRCLE);
        lv_obj_set_size(ui.secondaryHighlights[i], 4, 4);
        lv_obj_set_pos(ui.secondaryHighlights[i], 24, 29);

        if (ui.eyeCanvas != nullptr) {
            lv_obj_add_flag(ui.eyes[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui.pupils[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui.highlights[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(ui.secondaryHighlights[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

    lv_obj_set_size(ui.body, 392, 374);
    lv_obj_set_pos(ui.body, 37, 46);
    lv_obj_set_size(ui.eyes[0], 84, 96);
    lv_obj_set_size(ui.eyes[1], 83, 96);
    lv_obj_set_pos(ui.eyes[0], 86, 94);
    lv_obj_set_pos(ui.eyes[1], 227, 94);
    lv_obj_set_size(ui.pupils[0], 42, 60);
    lv_obj_set_size(ui.pupils[1], 42, 60);
    lv_obj_set_pos(ui.pupils[0], 21, 18);
    lv_obj_set_pos(ui.pupils[1], 20, 18);
}

void createUi() {
    ui.screen = lv_screen_active();
    lv_obj_t* screen = ui.screen;
    lv_obj_set_style_bg_color(screen, color(currentBackgroundColor), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(screen, 0, LV_PART_MAIN);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(screen, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(screen, onScreenTouch, LV_EVENT_PRESSED, nullptr);
    lv_obj_add_event_cb(screen, onBackgroundTapped, LV_EVENT_CLICKED, nullptr);

    createCreature(screen);

    ui.impactRing = lv_obj_create(screen);
    lv_obj_remove_style_all(ui.impactRing);
    lv_obj_set_size(ui.impactRing, 24, 24);
    lv_obj_set_style_radius(ui.impactRing, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui.impactRing, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(ui.impactRing, 6, LV_PART_MAIN);
    lv_obj_set_style_border_color(ui.impactRing, color(0xFFE3A7), LV_PART_MAIN);
    lv_obj_set_style_border_opa(ui.impactRing, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(ui.impactRing,
        static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_obj_add_flag(ui.impactRing, LV_OBJ_FLAG_HIDDEN);

    ui.status = makeLabel(screen, "WAITING FOR YOU", &lv_font_montserrat_14, 0xBDE5D5);
    lv_obj_set_width(ui.status, 420);
    lv_obj_set_pos(ui.status, 23, 432);

    ui.debugPanel = lv_obj_create(screen);
    lv_obj_remove_style_all(ui.debugPanel);
    lv_obj_set_size(ui.debugPanel, 466, 466);
    lv_obj_set_pos(ui.debugPanel, 0, 0);
    lv_obj_set_style_bg_color(ui.debugPanel, color(0x111B24), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui.debugPanel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(ui.debugPanel, 18, LV_PART_MAIN);
    lv_obj_add_flag(ui.debugPanel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui.debugPanel, onScreenTouch, LV_EVENT_PRESSED, nullptr);
    lv_obj_clear_flag(ui.debugPanel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(ui.debugPanel, LV_DIR_NONE);
    lv_obj_set_scrollbar_mode(ui.debugPanel, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(ui.debugPanel, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* debugTitle = makeLabel(ui.debugPanel, "CREATURE LAB", &lv_font_montserrat_18, 0xEAF2EF);
    lv_obj_set_width(debugTitle, 260);
    lv_obj_set_pos(debugTitle, 0, 4);

    ui.debugCloseButton = lv_button_create(ui.debugPanel);
    lv_obj_set_size(ui.debugCloseButton, 96, 36);
    lv_obj_set_pos(ui.debugCloseButton, 316, 0);
    lv_obj_set_style_bg_color(ui.debugCloseButton, color(0x344451), LV_PART_MAIN);
    lv_obj_set_style_radius(ui.debugCloseButton, 12, LV_PART_MAIN);
    lv_obj_add_event_cb(ui.debugCloseButton, onDebugCloseClicked, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* closeLabel = makeLabel(ui.debugCloseButton, "BACK", &lv_font_montserrat_12, 0xEAF2EF);
    lv_obj_center(closeLabel);

    ui.debugTabButtons[0] = createLabTabButton(ui.debugPanel, 0, "DEVICE", onLabDeviceClicked);
    ui.debugTabButtons[1] = createLabTabButton(ui.debugPanel, 134, "CREATURE", onLabCreatureClicked);
    ui.debugTabButtons[2] = createLabTabButton(ui.debugPanel, 268, "GENS", onLabGensClicked);

    ui.debugContentCard = lv_obj_create(ui.debugPanel);
    lv_obj_remove_style_all(ui.debugContentCard);
    lv_obj_set_size(ui.debugContentCard, 430, 282);
    lv_obj_set_pos(ui.debugContentCard, 0, 96);
    lv_obj_set_style_bg_color(ui.debugContentCard, color(0x1A2A35), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui.debugContentCard, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(ui.debugContentCard, 16, LV_PART_MAIN);
    lv_obj_set_style_border_width(ui.debugContentCard, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(ui.debugContentCard, color(0x344B58), LV_PART_MAIN);
    lv_obj_set_style_pad_all(ui.debugContentCard, 12, LV_PART_MAIN);
    lv_obj_add_flag(ui.debugContentCard, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(ui.debugContentCard, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(ui.debugContentCard, LV_SCROLLBAR_MODE_AUTO);
    ui.debugContentLabel = makeLabel(ui.debugContentCard, "", &lv_font_montserrat_12, 0xBDE5D5);
    lv_obj_set_width(ui.debugContentLabel, 400);
    lv_obj_set_pos(ui.debugContentLabel, 0, 0);
    lv_obj_set_style_text_align(ui.debugContentLabel, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_label_set_long_mode(ui.debugContentLabel, LV_LABEL_LONG_WRAP);

    ui.debugAddCreatureButton = lv_button_create(ui.debugPanel);
    lv_obj_set_size(ui.debugAddCreatureButton, 430, 44);
    lv_obj_set_pos(ui.debugAddCreatureButton, 0, 390);
    lv_obj_set_style_bg_color(ui.debugAddCreatureButton, color(0xC8A86D), LV_PART_MAIN);
    lv_obj_set_style_radius(ui.debugAddCreatureButton, 14, LV_PART_MAIN);
    lv_obj_set_style_border_width(ui.debugAddCreatureButton, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(ui.debugAddCreatureButton,
        onDebugAddCreatureClicked, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* addCreatureLabel = makeLabel(
        ui.debugAddCreatureButton, "DEBUG: ADD CREATURE TO GARDEN",
        &lv_font_montserrat_14, 0x172831);
    lv_obj_center(addCreatureLabel);

    selectLabPage(LabPage::Device);

    ui.menuPanel = lv_obj_create(screen);
    lv_obj_remove_style_all(ui.menuPanel);
    lv_obj_set_size(ui.menuPanel, 364, 390);
    lv_obj_set_pos(ui.menuPanel, 51, 38);
    lv_obj_set_style_bg_color(ui.menuPanel, color(0x1A2A35), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui.menuPanel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(ui.menuPanel, 30, LV_PART_MAIN);
    lv_obj_set_style_border_width(ui.menuPanel, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(ui.menuPanel, color(0x4F8D79), LV_PART_MAIN);
    lv_obj_set_style_pad_all(ui.menuPanel, 0, LV_PART_MAIN);
    lv_obj_add_flag(ui.menuPanel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui.menuPanel, onScreenTouch, LV_EVENT_PRESSED, nullptr);
    lv_obj_clear_flag(ui.menuPanel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ui.menuPanel, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* menuTitle = makeLabel(ui.menuPanel, "GARDEN", &lv_font_montserrat_18, 0xEAF2EF);
    lv_obj_set_width(menuTitle, 316);
    lv_obj_set_pos(menuTitle, 24, 15);

    createMenuButton(ui.menuPanel, 64, "MY GARDEN", 0x78C9A9, onMenuGardenClicked);
    createMenuButton(ui.menuPanel, 122, "FEED", 0xE5C98F, onMenuFeedClicked);
    createMenuButton(ui.menuPanel, 180, "CREATURE LAB", 0x8EBBD0, onMenuLabClicked);
    createMenuButton(ui.menuPanel, 238, "VOLUME", 0xA8B9B5, onMenuVolumeClicked);
    createMenuButton(ui.menuPanel, 296, "CLOSE", 0xA8B9B5, onMenuCloseClicked);

    ui.gardenPanel = lv_obj_create(screen);
    lv_obj_remove_style_all(ui.gardenPanel);
    lv_obj_set_size(ui.gardenPanel, 430, 430);
    lv_obj_set_pos(ui.gardenPanel, 18, 18);
    lv_obj_set_style_bg_color(ui.gardenPanel, color(0x111B24), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui.gardenPanel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(ui.gardenPanel, 26, LV_PART_MAIN);
    lv_obj_set_style_border_width(ui.gardenPanel, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(ui.gardenPanel, color(0x4F8D79), LV_PART_MAIN);
    lv_obj_set_style_pad_all(ui.gardenPanel, 0, LV_PART_MAIN);
    lv_obj_add_flag(ui.gardenPanel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui.gardenPanel, onScreenTouch, LV_EVENT_PRESSED, nullptr);
    lv_obj_clear_flag(ui.gardenPanel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ui.gardenPanel, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* gardenTitle = makeLabel(
        ui.gardenPanel, "MY GARDEN", &lv_font_montserrat_18, 0xEAF2EF);
    lv_obj_set_width(gardenTitle, 250);
    lv_obj_set_pos(gardenTitle, 18, 14);

    ui.gardenCountLabel = makeLabel(
        ui.gardenPanel, "0 / 6", &lv_font_montserrat_14, 0x91AAA4);
    lv_obj_set_size(ui.gardenCountLabel, 70, 32);
    lv_obj_set_pos(ui.gardenCountLabel, 270, 14);

    lv_obj_t* gardenBackButton = lv_button_create(ui.gardenPanel);
    lv_obj_set_size(gardenBackButton, 72, 34);
    lv_obj_set_pos(gardenBackButton, 340, 14);
    lv_obj_set_style_bg_color(gardenBackButton, color(0x344451), LV_PART_MAIN);
    lv_obj_set_style_radius(gardenBackButton, 11, LV_PART_MAIN);
    lv_obj_add_event_cb(gardenBackButton, onGardenBackClicked, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* gardenBackLabel = makeLabel(
        gardenBackButton, "BACK", &lv_font_montserrat_12, 0xEAF2EF);
    lv_obj_center(gardenBackLabel);

    lv_obj_t* gardenHint = makeLabel(
        ui.gardenPanel, "SELECT A CREATURE  |  DEL TO REMOVE",
        &lv_font_montserrat_12, 0x91AAA4);
    lv_obj_set_width(gardenHint, 390);
    lv_obj_set_pos(gardenHint, 20, 54);

    for (std::size_t i = 0; i < persistence::MaximumGardenCreatures; ++i) {
        gardenRowIndices[i] = static_cast<int>(i);
        const int rowY = 86 + static_cast<int>(i) * 55;

        ui.gardenRows[i] = lv_button_create(ui.gardenPanel);
        lv_obj_set_size(ui.gardenRows[i], 300, 48);
        lv_obj_set_pos(ui.gardenRows[i], 18, rowY);
        lv_obj_set_style_bg_color(ui.gardenRows[i], color(0x263A45), LV_PART_MAIN);
        lv_obj_set_style_radius(ui.gardenRows[i], 12, LV_PART_MAIN);
        lv_obj_set_style_border_width(ui.gardenRows[i], 0, LV_PART_MAIN);
        lv_obj_add_event_cb(ui.gardenRows[i],
            onGardenRowClicked, LV_EVENT_CLICKED, &gardenRowIndices[i]);

        ui.gardenRowLabels[i] = makeLabel(
            ui.gardenRows[i], "EMPTY", &lv_font_montserrat_12, 0xEAF2EF);
        lv_obj_set_width(ui.gardenRowLabels[i], 280);
        lv_obj_set_style_text_align(ui.gardenRowLabels[i], LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
        lv_obj_center(ui.gardenRowLabels[i]);

        ui.gardenDeleteButtons[i] = lv_button_create(ui.gardenPanel);
        lv_obj_set_size(ui.gardenDeleteButtons[i], 76, 48);
        lv_obj_set_pos(ui.gardenDeleteButtons[i], 332, rowY);
        lv_obj_set_style_bg_color(ui.gardenDeleteButtons[i], color(0x633D43), LV_PART_MAIN);
        lv_obj_set_style_radius(ui.gardenDeleteButtons[i], 12, LV_PART_MAIN);
        lv_obj_set_style_border_width(ui.gardenDeleteButtons[i], 0, LV_PART_MAIN);
        lv_obj_add_event_cb(ui.gardenDeleteButtons[i],
            onGardenDeleteClicked, LV_EVENT_CLICKED, &gardenRowIndices[i]);
        lv_obj_t* deleteLabel = makeLabel(
            ui.gardenDeleteButtons[i], "DEL", &lv_font_montserrat_12, 0xF4E8E8);
        lv_obj_center(deleteLabel);
        lv_obj_add_flag(ui.gardenRows[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(ui.gardenDeleteButtons[i], LV_OBJ_FLAG_HIDDEN);
    }

    ui.volumePanel = lv_obj_create(screen);
    lv_obj_remove_style_all(ui.volumePanel);
    lv_obj_set_size(ui.volumePanel, 364, 252);
    lv_obj_set_pos(ui.volumePanel, 51, 107);
    lv_obj_set_style_bg_color(ui.volumePanel, color(0x1A2A35), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui.volumePanel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(ui.volumePanel, 30, LV_PART_MAIN);
    lv_obj_set_style_border_width(ui.volumePanel, 2, LV_PART_MAIN);
    lv_obj_set_style_border_color(ui.volumePanel, color(0x4F8D79), LV_PART_MAIN);
    lv_obj_set_style_pad_all(ui.volumePanel, 0, LV_PART_MAIN);
    lv_obj_add_flag(ui.volumePanel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui.volumePanel, onScreenTouch, LV_EVENT_PRESSED, nullptr);
    lv_obj_clear_flag(ui.volumePanel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ui.volumePanel, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* volumeTitle = makeLabel(
        ui.volumePanel, "SOUND", &lv_font_montserrat_18, 0xEAF2EF);
    lv_obj_set_width(volumeTitle, 316);
    lv_obj_set_pos(volumeTitle, 24, 15);

    lv_obj_t* volumeHint = makeLabel(
        ui.volumePanel, "MASTER VOLUME", &lv_font_montserrat_12, 0x91AAA4);
    lv_obj_set_width(volumeHint, 316);
    lv_obj_set_pos(volumeHint, 24, 57);

    ui.volumeLabel = makeLabel(
        ui.volumePanel, "100%", &lv_font_montserrat_18, 0xBDE5D5);
    lv_obj_set_width(ui.volumeLabel, 316);
    lv_obj_set_pos(ui.volumeLabel, 24, 79);

    ui.volumeSlider = lv_slider_create(ui.volumePanel);
    lv_obj_set_size(ui.volumeSlider, 286, 18);
    lv_obj_set_pos(ui.volumeSlider, 39, 143);
    lv_slider_set_range(ui.volumeSlider, 0, 100);
    lv_slider_set_value(ui.volumeSlider, static_cast<int>(audio::volumePercent()), LV_ANIM_OFF);
    lv_obj_set_style_bg_color(ui.volumeSlider, color(0x344A54), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui.volumeSlider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(ui.volumeSlider, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ui.volumeSlider, color(0x72C7A8), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(ui.volumeSlider, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(ui.volumeSlider, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(ui.volumeSlider, color(0xEAF2EF), LV_PART_KNOB);
    lv_obj_set_style_bg_opa(ui.volumeSlider, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_radius(ui.volumeSlider, LV_RADIUS_CIRCLE, LV_PART_KNOB);
    lv_obj_add_event_cb(ui.volumeSlider, onVolumeSliderChanged, LV_EVENT_VALUE_CHANGED, nullptr);

    lv_obj_t* volumeMinimum = makeLabel(
        ui.volumePanel, "MUTE", &lv_font_montserrat_12, 0x91AAA4);
    lv_obj_set_pos(volumeMinimum, 38, 169);
    lv_obj_t* volumeMaximum = makeLabel(
        ui.volumePanel, "MAX", &lv_font_montserrat_12, 0x91AAA4);
    lv_obj_set_pos(volumeMaximum, 364 - 38 - 38, 169);

    createMenuButton(ui.volumePanel, 188, "BACK TO GARDEN", 0xA8B9B5, onVolumeBackClicked);
}


void updatePipEyeCanvas(
    int bodyWidth,
    int bodyHeight,
    float phase,
    float gazeX,
    float gazeY,
    bool angryFace,
    bool sadFace,
    bool happyFace,
    bool afraidFace,
    bool surprisedFace,
    bool curiousFace,
    bool disgustedFace,
    bool hitFlash)
{
    if (ui.eyeCanvas == nullptr || ui.eyeBuffer == nullptr) return;

    static float nextEyeRenderAt = 0.0F;
    static std::uint8_t previousExpression = 0xFFU;
    static bool previousHitFlash = false;
    const std::uint8_t expression =
        static_cast<std::uint8_t>((angryFace ? 1U : 0U)
        | (sadFace ? 2U : 0U) | (happyFace ? 4U : 0U)
        | (afraidFace ? 8U : 0U) | (surprisedFace ? 16U : 0U)
        | (curiousFace ? 32U : 0U) | (disgustedFace ? 64U : 0U));
    const bool expressionChanged = expression != previousExpression;
    const bool hitFlashChanged = hitFlash != previousHitFlash;
    if (phase < nextEyeRenderAt && !expressionChanged && !hitFlashChanged) return;
    previousExpression = expression;
    previousHitFlash = hitFlash;
    nextEyeRenderAt = phase + (1.0F / 15.0F);

    const int canvasX = (bodyWidth - PipEyeCanvasWidth) / 2;
    const int canvasY = (bodyHeight - PipEyeCanvasHeight) / 2;
    lv_obj_set_pos(ui.eyeCanvas, canvasX, canvasY);

    // Keep the canvas aligned with the body colour; Pip's renderer paints the
    // same lit ellipsoid eyes and surface-following pupils used by Pip-KeyPet.
    auto* pixels = reinterpret_cast<std::uint16_t*>(ui.eyeBuffer->data);
    const std::uint16_t bodyPixel = lv_color_to_u16(
        color(hitFlash ? 0xF1D39A : 0x427E83));
    std::fill_n(pixels, PipEyeCanvasWidth * PipEyeCanvasHeight, bodyPixel);
    for (int i = 0; i < 4; ++i) {
        pipEyeFace.drawn[i] = face_rect_t{0, 0, -1, -1};
    }

    pipEyeFace.color = 0xFFFF;
    pipEyeFace.pupil_color = 0x18C3; // Pip's original #1C1C1C, not pure black.
    pipEyeFace.shading = 0.0F;
    pipEyeFace.blush = 0.0F;

    // Pip uses 118 x 138 neutral eye shapes, scaled by EYE_SCALE=1.45.
    // The 320 px canvas is a 0.6867 scale of Pip's 466 px face coordinate space.
    constexpr float PipScreenScale =
        static_cast<float>(PipEyeCanvasWidth) / 466.0F;
    constexpr float PipEyeScale = 1.45F;
    constexpr float PipSpread = 0.55F;
    constexpr float PipHeadRadius =
        (237.0F / (2.0F * 0.5227F)) * PipScreenScale;

    float shapeWidth = 118.0F;
    float shapeHeight = 138.0F;
    float shapeRoll = 0.0F;
    float shapeTaper = 0.0F;
    float lid = 0.0F;
    float lidTilt = 0.0F;
    float smile = 0.0F;
    float pupilWidth = 0.40F;
    float pupilHeight = 0.44F;

    // These are the corresponding shape_t values from Pip's mood table.
    if (angryFace) {
        shapeWidth = 114.0F; shapeHeight = 114.0F;
        shapeRoll = 0.16F; shapeTaper = 0.30F;
        lid = 0.30F; lidTilt = 0.45F;
        pupilWidth = 0.20F; pupilHeight = 0.40F;
    } else if (disgustedFace) {
        shapeWidth = 118.0F; shapeHeight = 104.0F;
        shapeRoll = -0.10F; lid = 0.30F; lidTilt = -0.10F;
        smile = 0.25F; pupilWidth = 0.32F; pupilHeight = 0.34F;
    } else if (sadFace) {
        shapeWidth = 112.0F; shapeHeight = 126.0F;
        shapeRoll = -0.14F; lid = 0.22F; lidTilt = -0.35F;
        pupilWidth = 0.52F; pupilHeight = 0.56F;
    } else if (happyFace) {
        shapeWidth = 118.0F; shapeHeight = 124.0F;
        shapeRoll = -0.06F; shapeTaper = -0.10F;
        smile = 0.42F; pupilWidth = 0.36F; pupilHeight = 0.38F;
    } else if (surprisedFace) {
        shapeWidth = 124.0F; shapeHeight = 160.0F;
        shapeTaper = -0.05F; pupilWidth = 0.24F; pupilHeight = 0.26F;
    } else if (afraidFace) {
        shapeWidth = 126.0F; shapeHeight = 160.0F;
        shapeRoll = -0.10F; shapeTaper = -0.10F;
        lid = 0.08F; lidTilt = -0.30F;
        pupilWidth = 0.18F; pupilHeight = 0.20F;
    } else if (curiousFace) {
        shapeWidth = 124.0F; shapeHeight = 148.0F;
        shapeTaper = -0.05F; pupilWidth = 0.50F; pupilHeight = 0.54F;
    }

    const float headYaw = std::clamp(gazeX * 0.012F, -0.10F, 0.10F);
    const float headPitch = std::clamp(gazeY * 0.010F, -0.06F, 0.06F);
    float headRoll = std::sin(phase * 0.17F) * 0.015F;
    if (happyFace) headRoll = 0.12F;
    else if (sadFace) headRoll = -0.10F;
    else if (curiousFace) headRoll = 0.16F;

    // Keep Pip's natural blink cadence, but close the lid rather than collapsing
    // the mesh to zero height, which is safer for this small embedded renderer.
    const float blinkPhase = std::fmod(
        phase + static_cast<float>(pet.genome().seed % 31U) * 0.07F, 4.2F);
    const bool blink = blinkPhase > 3.92F && blinkPhase < 4.12F;
    const float blinkLid = blink ? 0.94F : 0.0F;

    for (int i = 0; i < 2; ++i) {
        const float side = i ? 1.0F : -1.0F;
        const float phi = headYaw + side * PipSpread;
        const float perspective = PipEyeScale
            * (1.0F + 0.35F * (std::cos(phi) - 0.8525F));
        const float xOffset = PipHeadRadius * std::sin(phi);
        const float yOffset = 240.0F * PipScreenScale * std::sin(headPitch);

        eye_t& eye = pipEyeFace.eye[i];
        eye.w = shapeWidth * perspective * PipScreenScale;
        eye.h = shapeHeight * perspective * PipScreenScale;
        eye.depth = shapeWidth * perspective * PipScreenScale * 0.5F;
        eye.x = PipEyeCanvasWidth * 0.5F
            + xOffset * std::cos(headRoll) - yOffset * std::sin(headRoll);
        eye.y = PipEyeCanvasHeight * 0.5F
            + xOffset * std::sin(headRoll) + yOffset * std::cos(headRoll);
        eye.yaw = headYaw + side * PipSpread * 0.5F;
        eye.pitch = headPitch;
        eye.roll = headRoll - side * shapeRoll;
        eye.taper = shapeTaper;
        eye.lid = std::max(lid, blinkLid);
        eye.tilt = -side * lidTilt;
        eye.smile = smile;
        eye.look_x = std::clamp(gazeX * 0.040F - eye.yaw, -0.85F, 0.85F);
        eye.look_y = std::clamp(gazeY * 0.035F - eye.pitch, -0.70F, 0.70F);
        eye.pw = pupilWidth;
        eye.ph = pupilHeight;
        eye.square = 0.0F;
        eye.heart = 0.0F;
        eye.wobble = 0.022F * std::min(eye.w, eye.h) * 0.5F;
        eye.wob_t = phase + static_cast<float>(i) * 7.3F;
        eye.glint = 0.0F;
        eye.spiral = 0.0F;
        eye.veins = 0.0F;
        eye.wet = sadFace ? 0.18F : (afraidFace ? 0.08F : 0.0F);
    }

    face_draw(&pipEyeFace, pixels, PipEyeCanvasWidth, PipEyeCanvasHeight);
    lv_obj_invalidate(ui.eyeCanvas);
}

void updateCreatureVisuals() {
    const auto& genes = pet.genome().morphology;
    const float activity = static_cast<float>(pet.genome().personality.activity + 100) / 200.0F;
    const float phase = animationTime;
    const Reaction reaction = pet.reaction().kind();
    const float reactionPower = pet.reaction().active() ? pet.reaction().intensity() : 0.0F;
    const bool exaggerated = pet.reaction().exaggerated();

    const float joy = pet.emotions().value(Emotion::Joy);
    const float sadness = pet.emotions().value(Emotion::Sadness);
    const float anger = pet.emotions().value(Emotion::Anger);
    const float fear = pet.emotions().value(Emotion::Fear);
    const float surprise = pet.emotions().value(Emotion::Surprise);
    const float disgust = pet.emotions().value(Emotion::Disgust);

    const bool angryFace = reaction == Reaction::Angry || anger > 48.0F;
    const bool sadFace = reaction == Reaction::Sad || sadness > 48.0F;
    const bool happyFace = reaction == Reaction::Happy || reaction == Reaction::Affectionate || joy > 52.0F;
    const bool afraidFace = reaction == Reaction::Fearful || fear > 48.0F;
    const bool surprisedFace = reaction == Reaction::Surprised || surprise > 55.0F;
    const bool disgustedFace = reaction == Reaction::Disgusted || disgust > 50.0F;
    const bool curiousFace = reaction == Reaction::Curious
        || pet.emotions().value(Emotion::Curiosity) > 48.0F;

    float bodyOffsetX = std::clamp(deviceRoll * 0.14F, -10.0F, 10.0F);
    float bodyOffsetY = std::clamp(devicePitch * 0.12F, -9.0F, 9.0F);
    const float hitElapsed = animationTime - lastHitAnimation;
    if (hitElapsed >= 0.0F && hitElapsed < 0.16F) {
        const float recoil = std::exp(-hitElapsed * 18.0F);
        bodyOffsetX -= hitDirection * (pet.enjoysSparring() ? 5.0F : 3.0F) * recoil;
        bodyOffsetY += (pet.enjoysSparring() ? -1.0F : 1.5F) * recoil;
    }

    if (ui.impactRing != nullptr) {
        if (hitElapsed >= 0.0F && hitElapsed < 0.24F) {
            const float progress = hitElapsed / 0.24F;
            const int diameter = static_cast<int>(24.0F + progress * 60.0F);
            lv_obj_set_size(ui.impactRing, diameter, diameter);
            lv_obj_set_pos(ui.impactRing,
                static_cast<int>(hitPointX - diameter * 0.5F),
                static_cast<int>(hitPointY - diameter * 0.5F));
            lv_obj_set_style_border_width(ui.impactRing,
                static_cast<int>(3.5F - progress * 2.0F), LV_PART_MAIN);
            lv_obj_set_style_border_opa(ui.impactRing,
                static_cast<lv_opa_t>(190.0F * (1.0F - progress)), LV_PART_MAIN);
            lv_obj_clear_flag(ui.impactRing, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_to_index(ui.impactRing, -1);
        } else {
            lv_obj_add_flag(ui.impactRing, LV_OBJ_FLAG_HIDDEN);
        }
    }

    float bodyX = 87.0F;
    float bodyY = 0.0F;
    if (happyFace) bodyY -= std::abs(std::sin(phase * 8.0F)) * 8.0F * std::max(0.25F, reactionPower);
    if (afraidFace) bodyX += std::sin(phase * 24.0F) * (exaggerated ? 6.0F : 3.0F);
    if (angryFace) bodyX += std::sin(phase * 18.0F) * 2.5F;
    if (reaction == Reaction::Curious) bodyX += 3.5F * reactionPower;

    float hitScale = 1.0F;
    if (hitElapsed >= 0.0F && hitElapsed < 0.065F) {
        const float t = hitElapsed / 0.065F;
        const float eased = t * t * (3.0F - 2.0F * t);
        hitScale = 1.0F - 0.10F * eased;
    } else if (hitElapsed >= 0.065F && hitElapsed < 0.16F) {
        const float t = (hitElapsed - 0.065F) / 0.095F;
        const float eased = t * (2.0F - t);
        hitScale = 0.90F + 0.12F * eased;
    } else if (hitElapsed >= 0.16F && hitElapsed < 0.24F) {
        const float t = (hitElapsed - 0.16F) / 0.08F;
        const float eased = t * t * (3.0F - 2.0F * t);
        hitScale = 1.02F - 0.02F * eased;
    }

    // Apply the same scale on both axes: the creature compresses and rebounds,
    // but its proportions never stretch sideways.
    const int bodyWidth = static_cast<int>(
        392.0F * 0.95F * (0.94F + static_cast<float>(genes.bodyWidth) * 0.0011F) * hitScale);
    const int bodyHeight = static_cast<int>(
        374.0F * 0.95F * (0.94F + static_cast<float>(genes.bodyHeight) * 0.0011F) * hitScale);
    bodyY = 233.0F - static_cast<float>(bodyHeight) * 0.5F
        + std::sin(phase * 1.8F) * (1.5F + activity * 2.2F);
    bodyX = (466.0F - static_cast<float>(bodyWidth)) * 0.5F + bodyOffsetX;
    bodyY += bodyOffsetY;
    lv_obj_set_size(ui.body, bodyWidth, bodyHeight);
    lv_obj_set_pos(ui.body, static_cast<int>(bodyX), static_cast<int>(bodyY));
    static bool previousHitFlash = false;
    const bool hitFlash = hitElapsed >= 0.0F && hitElapsed < 0.045F;
    if (hitFlash != previousHitFlash) {
        lv_obj_set_style_bg_color(ui.body, color(hitFlash ? 0xF1D39A : 0x427E83), LV_PART_MAIN);
        previousHitFlash = hitFlash;
    }

    const float appendageWave = std::sin(phase * (2.3F + activity * 2.8F));
    const float appendageBoost = exaggerated ? 1.8F : 1.0F;
    const int appendageLength = 32 + static_cast<int>(genes.appendageLength * 0.32F);
    const int sideAppendageWidth = appendageLength + 26;
    constexpr float sideAppendageOffset = 14.0F;
    lv_obj_set_size(ui.appendages[0], sideAppendageWidth, 20);
    lv_obj_set_size(ui.appendages[1], sideAppendageWidth, 20);
    lv_obj_set_size(ui.appendages[2], 22, appendageLength + 18);
    lv_obj_set_style_transform_rotation(ui.appendages[0], -85, LV_PART_MAIN);
    lv_obj_set_style_transform_rotation(ui.appendages[1], 85, LV_PART_MAIN);
    lv_obj_set_pos(ui.appendages[0], static_cast<int>(bodyX - sideAppendageOffset),
        static_cast<int>(bodyY + bodyHeight * 0.54F + appendageWave * 5.0F * appendageBoost));
    lv_obj_set_pos(ui.appendages[1],
        static_cast<int>(bodyX + bodyWidth - sideAppendageWidth + sideAppendageOffset),
        static_cast<int>(bodyY + bodyHeight * 0.49F - appendageWave * 6.0F * appendageBoost));
    lv_obj_set_pos(ui.appendages[2], static_cast<int>(bodyX + bodyWidth * 0.47F),
        static_cast<int>(bodyY - appendageLength * 0.36F + std::sin(phase * 3.0F) * 4.0F));
    for (int i = 0; i < 3; ++i) {
        const bool visible = genes.appendageCount > i;
        if (visible) lv_obj_clear_flag(ui.appendages[i], LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(ui.appendages[i], LV_OBJ_FLAG_HIDDEN);
    }


    if (ui.eyeCanvas != nullptr && ui.eyeBuffer != nullptr) {
        float gazeX = std::sin(phase * 0.72F) * 5.5F
            + std::sin(phase * 0.21F) * 2.0F;
        float gazeY = std::cos(phase * 0.51F) * 3.0F;
        if (curiousFace) gazeX += 6.0F;
        if (afraidFace || surprisedFace) {
            gazeX += std::sin(phase * 5.0F) * 4.0F;
            gazeY -= 3.0F;
        }
        if (sadFace) gazeY += 3.5F;
        gazeX = std::clamp(gazeX, -8.0F, 8.0F);
        gazeY = std::clamp(gazeY, -6.0F, 6.0F);

        updatePipEyeCanvas(bodyWidth, bodyHeight, phase, gazeX, gazeY,
            angryFace, sadFace, happyFace, afraidFace, surprisedFace,
            curiousFace, disgustedFace, hitFlash);

    } else {
    const float eyeScale = 0.90F + static_cast<float>(genes.eyeSize) * 0.0028F;
    const int eyeWidth = static_cast<int>(78.0F * eyeScale);
    const int baseEyeHeight = static_cast<int>(90.0F * eyeScale);
    const int eyeY = static_cast<int>(bodyHeight * 0.26F);
    const int leftX = static_cast<int>(bodyWidth * 0.22F);
    const int rightX = static_cast<int>(bodyWidth * 0.58F);

    float gazeX = std::sin(phase * 0.72F) * 5.5F + std::sin(phase * 0.21F) * 2.0F;
    float gazeY = std::cos(phase * 0.51F) * 3.0F;
    if (curiousFace) gazeX += 6.0F;
    if (afraidFace || surprisedFace) {
        gazeX += std::sin(phase * 5.0F) * 4.0F;
        gazeY -= 3.0F;
    }
    if (sadFace) gazeY += 3.5F;
    gazeX = std::max(-8.0F, std::min(8.0F, gazeX));
    gazeY = std::max(-6.0F, std::min(6.0F, gazeY));

    const float blinkPhase = std::fmod(phase + static_cast<float>(pet.genome().seed % 31U) * 0.07F, 4.2F);
    const bool blink = blinkPhase > 3.92F && blinkPhase < 4.12F;
    int eyeHeight = static_cast<int>(baseEyeHeight * 0.90F);
    if (angryFace) eyeHeight = static_cast<int>(baseEyeHeight * 0.69F);
    else if (happyFace) eyeHeight = static_cast<int>(baseEyeHeight * 0.76F);
    else if (sadFace) eyeHeight = static_cast<int>(baseEyeHeight * 0.84F);
    else if (surprisedFace || afraidFace) eyeHeight = static_cast<int>(baseEyeHeight * 1.08F);
    if (blink) eyeHeight = 7;

    static bool previousBlink = false;
    if (blink != previousBlink) {
        for (int i = 0; i < 2; ++i) {
            lv_obj_set_style_bg_color(ui.eyes[i], color(blink ? 0x427E83 : 0xF1F3EE), LV_PART_MAIN);
        }
        previousBlink = blink;
    }

    for (int i = 0; i < 2; ++i) {
        const int eyeX = i == 0 ? leftX : rightX;
        const int individualEyeWidth = eyeWidth + (i == 0 ? 2 : -1);
        lv_obj_set_size(ui.eyes[i], individualEyeWidth, eyeHeight);
        lv_obj_set_pos(ui.eyes[i], eyeX, eyeY + (blink ? 20 : 0));
        // Pip-inspired spherical eye treatment: slight inward tilt, deep pupil and paired catchlights.
        const int eyeTilt = i == 0 ? 75 : -75;
        lv_obj_set_style_transform_rotation(ui.eyes[i], eyeTilt, LV_PART_MAIN);
        lv_obj_set_style_transform_pivot_x(ui.eyes[i], individualEyeWidth / 2, LV_PART_MAIN);
        lv_obj_set_style_transform_pivot_y(ui.eyes[i], eyeHeight / 2, LV_PART_MAIN);

        if (blink) {
            lv_obj_add_flag(ui.pupils[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(ui.pupils[i], LV_OBJ_FLAG_HIDDEN);
            const int pupilWidth = std::max(22, static_cast<int>(individualEyeWidth * (afraidFace ? 0.44F : 0.50F)));
            const int pupilHeight = std::max(30, static_cast<int>(eyeHeight * 0.62F));
            lv_obj_set_size(ui.pupils[i], pupilWidth, pupilHeight);
            const int pupilX = (individualEyeWidth - pupilWidth) / 2 + static_cast<int>(gazeX);
            const int pupilY = (eyeHeight - pupilHeight) / 2 + static_cast<int>(gazeY);
            lv_obj_set_pos(ui.pupils[i], pupilX, pupilY);

            const int largeGlint = std::max(7, pupilWidth / 4);
            const int tallGlint = std::max(8, pupilHeight / 5);
            lv_obj_set_size(ui.highlights[i], largeGlint, tallGlint);
            lv_obj_set_pos(ui.highlights[i], pupilWidth / 6, pupilHeight / 7);
            lv_obj_set_size(ui.secondaryHighlights[i], 4, 4);
            lv_obj_set_pos(ui.secondaryHighlights[i], pupilWidth * 2 / 3, pupilHeight * 3 / 5);
        }

    }


    }

    if (messageOverride != nullptr && animationTime < messageUntil) {
        setStatus(messageOverride);
    } else {
        messageOverride = nullptr;
        setStatus(pet.reaction().active() ? reactionName(reaction) : actionTag(pet.action()));
    }
}

audio::Cue cueForReaction(Reaction reaction) {
    switch (reaction) {
    case Reaction::Happy: return audio::Cue::Happy;
    case Reaction::Sad: return audio::Cue::Sad;
    case Reaction::Angry: return audio::Cue::Angry;
    case Reaction::Fearful: return audio::Cue::Fearful;
    case Reaction::Surprised: return audio::Cue::Surprised;
    case Reaction::Curious: return audio::Cue::Curious;
    case Reaction::Affectionate: return audio::Cue::Affectionate;
    case Reaction::Disgusted: return audio::Cue::Disgusted;
    case Reaction::None: break;
    }
    return audio::Cue::Button;
}


} // namespace


void initialize() {
    storageReady = persistence::initialize();
    restoredFromSave = false;
    if (storageReady && persistence::loadGarden(gardenState)
        && gardenState.count > 0U && gardenState.activeIndex < gardenState.count) {
        CreatureSimulation selected(gardenState.creatures[gardenState.activeIndex].genome);
        restoredFromSave = selected.restorePersistentState(
            gardenState.creatures[gardenState.activeIndex]);
        if (restoredFromSave) pet = selected;
    }

    if (!restoredFromSave) {
        gardenState = {};
        Genome birthGenome = makeDefaultGenome();
        birthGenome.seed = esp_random();
        if (birthGenome.seed == 0U) birthGenome.seed = 0xA341316CU;
        pet = CreatureSimulation(birthGenome);
        gardenState.count = 1U;
        gardenState.activeIndex = 0U;
        gardenState.creatures[0] = pet.persistentState();
        requestSave();
    } else if (pet.genome().seed == makeDefaultGenome().seed) {
        // Give pre-name-system saves a one-time stable identity without resetting simulation data.
        auto savedState = pet.persistentState();
        savedState.genome.seed = esp_random();
        if (savedState.genome.seed == 0U) savedState.genome.seed = 0xA341316CU;
        (void)pet.restorePersistentState(savedState);
        gardenState.creatures[gardenState.activeIndex] = pet.persistentState();
        requestSave();
    }

    createUi();
    refreshGardenPanel();
    if (restoredFromSave) setMessage("WELCOME BACK", 2.0F);
}

void setBatteryStatus(int percent, bool charging) {
    deviceBatteryPercent = percent < 0 ? -1 : std::clamp(percent, 0, 100);
    deviceBatteryCharging = charging && deviceBatteryPercent >= 0;
}

void toggleDebugPanel() {
    if (ui.volumePanel != nullptr
        && !lv_obj_has_flag(ui.volumePanel, LV_OBJ_FLAG_HIDDEN)) hideVolumePanel();
    if (ui.gardenPanel != nullptr
        && !lv_obj_has_flag(ui.gardenPanel, LV_OBJ_FLAG_HIDDEN)) {
        hideGardenPanel();
        return;
    }
    if (ui.menuPanel != nullptr && !lv_obj_has_flag(ui.menuPanel, LV_OBJ_FLAG_HIDDEN)) {
        hideMenuPanel();
        return;
    }
    if (ui.debugPanel == nullptr) return;
    if (lv_obj_has_flag(ui.debugPanel, LV_OBJ_FLAG_HIDDEN)) {
        hideMenuPanel();
        showDebugPanel();
    } else {
        hideDebugPanel();
    }
}

void toggleMenuPanel() {
    if (ui.volumePanel != nullptr
        && !lv_obj_has_flag(ui.volumePanel, LV_OBJ_FLAG_HIDDEN)) hideVolumePanel();
    if (ui.gardenPanel != nullptr
        && !lv_obj_has_flag(ui.gardenPanel, LV_OBJ_FLAG_HIDDEN)) hideGardenPanel();
    if (ui.debugPanel != nullptr) hideDebugPanel();
    if (ui.menuPanel == nullptr) return;
    if (lv_obj_has_flag(ui.menuPanel, LV_OBJ_FLAG_HIDDEN)) showMenuPanel();
    else hideMenuPanel();
}

void update(float deltaSeconds) {
    const float dt = std::clamp(deltaSeconds, 0.0F, 0.25F);
    if (dt <= 0.0F) return;

    animationTime += dt;

    const motion::State sensor = motion::state();
    deviceRoll = sensor.rollDegrees;
    devicePitch = sensor.pitchDegrees;

    if (sensor.steps > lastMotionSteps) {
        const std::uint32_t newSteps = sensor.steps - lastMotionSteps;
        if (pet.registerSteps(newSteps)) setMessage("YOU FOUND SOMETHING!", 1.6F);
        lastMotionSteps = sensor.steps;
        requestSave();
    }
    if (sensor.shakeEvents > lastShakeEvents) {
        if (pet.applyInteraction(Interaction::Shake)) requestSave();
        lastShakeEvents = sensor.shakeEvents;
    }
    if (sensor.tiltEvents > lastTiltEvents) {
        if (pet.applyInteraction(Interaction::Tilt)) requestSave();
        lastTiltEvents = sensor.tiltEvents;
    }
    if (sensor.rotationEvents > lastRotationEvents) {
        if (pet.applyInteraction(Interaction::Rotate)) requestSave();
        lastRotationEvents = sensor.rotationEvents;
    }

    float soundIntensity = 0.0F;
    float soundDuration = 0.0F;
    if (audio::consumeSoundEvent(soundIntensity, soundDuration)) {
        if (pet.applySoundStimulus(soundIntensity, soundDuration)) requestSave();
    }
    pet.update(dt);
    updatePersistence(dt);

    const bool debugVisible = ui.debugPanel != nullptr
        && !lv_obj_has_flag(ui.debugPanel, LV_OBJ_FLAG_HIDDEN);
    const bool menuVisible = ui.menuPanel != nullptr
        && !lv_obj_has_flag(ui.menuPanel, LV_OBJ_FLAG_HIDDEN);
    const bool volumeVisible = ui.volumePanel != nullptr
        && !lv_obj_has_flag(ui.volumePanel, LV_OBJ_FLAG_HIDDEN);
    const bool gardenVisible = ui.gardenPanel != nullptr
        && !lv_obj_has_flag(ui.gardenPanel, LV_OBJ_FLAG_HIDDEN);
    const bool overlayVisible = debugVisible || menuVisible || volumeVisible || gardenVisible;
    if (!overlayVisible && !sensor.screenSleeping) updateBackground(dt);

    const Reaction currentReaction = pet.reaction().kind();
    if (currentReaction == Reaction::None) lastAudioReaction = Reaction::None;
    else if (currentReaction != lastAudioReaction) {
        audio::play(cueForReaction(currentReaction));
        lastAudioReaction = currentReaction;
    }

    if (!overlayVisible && !sensor.screenSleeping) {
        updateCreatureVisuals();
    }
    if (debugVisible && animationTime >= nextDebugRefresh) {
        refreshDebugPanel();
        nextDebugRefresh = animationTime + 0.20F;
    }
}

} // namespace evolvinggarden::app
