#include "lvgl.h"

#include "simulation/creature_simulation.hpp"
#include "platform/audio.hpp"
#include "platform/motion.hpp"
#include "platform/persistence.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>

using namespace evolvinggarden;

namespace evolvinggarden::app {
namespace {

struct PetUi {
    lv_obj_t* body = nullptr;
    lv_obj_t* eyes[2]{};
    lv_obj_t* pupils[2]{};
    lv_obj_t* highlights[2]{};
    lv_obj_t* mouth = nullptr;
    lv_obj_t* brows[2]{};
    lv_obj_t* appendages[3]{};
    lv_obj_t* screen = nullptr;
    lv_obj_t* status = nullptr;
    lv_obj_t* impactRing = nullptr;
    lv_obj_t* debugCloseButton = nullptr;
    lv_obj_t* debugPanel = nullptr;
    lv_obj_t* debugLeftLabel = nullptr;
    lv_obj_t* debugRightLabel = nullptr;
    lv_obj_t* menuPanel = nullptr;
    lv_obj_t* volumePanel = nullptr;
    lv_obj_t* volumeSlider = nullptr;
    lv_obj_t* volumeLabel = nullptr;
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
CreatureTouchGesture creatureTouch;
float animationTime = 0.0F;
float messageUntil = 0.0F;
float nextDebugRefresh = 0.0F;
float lastHitAnimation = -10.0F;
float hitDirection = 1.0F;
float hitPointX = 233.0F;
float hitPointY = 205.0F;
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

void updatePersistence(float dt) {
    if (!storageReady) return;
    saveInterval += dt;
    secondsSinceSave += dt;

    // Keep flash writes sparse, but flush interaction and step changes promptly.
    if ((saveRequested && secondsSinceSave >= 3.0F) || saveInterval >= 60.0F) {
        if (persistence::save(pet)) {
            saveRequested = false;
            saveInterval = 0.0F;
            secondsSinceSave = 0.0F;
        } else {
            // Keep the request pending and retry after the normal throttle interval.
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
    if (ui.debugLeftLabel == nullptr || ui.debugRightLabel == nullptr) return;
    const CreatureNeeds& needs = pet.needs();
    const EmotionalState& emotions = pet.emotions();
    const motion::State sensor = motion::state();
    char left[640]{};
    char right[640]{};

    std::snprintf(left, sizeof(left),
        "DEVICE\n\n"
        "MIC       %s  %u%%\n"
        "SPEAKER   %s\n"
        "STORAGE   %s\n"
        "SAVE      %s\n"
        "MOTION    %s\n"
        "ORIENTATION\n  %s\n"
        "SCREEN    %s\n\n"
        "MOVEMENT\n\n"
        "STEPS      %lu\n"
        "SHAKES     %lu\n"
        "TILTS      %lu\n"
        "ROTATIONS  %lu\n"
        "ROLL       %+.0f\n"
        "PITCH      %+.0f\n"
        "ACCEL      %.1f\n"
        "GYRO       %.1f",
        audio::microphoneReady() ? "READY" : "OFF",
        static_cast<unsigned>(audio::microphoneLevelPercent()),
        audio::speakerReady() ? "READY" : "OFF",
        storageReady ? "READY" : "OFF",
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

    std::snprintf(right, sizeof(right),
        "CREATURE\n\n"
        "MOOD      %+.0f\n"
        "REACTION  %s\n"
        "ACTION    %s\n\n"
        "NEEDS\n\n"
        "HUNGER    %3.0f%%\n"
        "ENERGY    %3.0f%%\n"
        "SOCIAL    %3.0f%%\n"
        "BOREDOM   %3.0f%%\n\n"
        "EMOTIONS\n\n"
        "JOY       %3.0f\n"
        "SADNESS   %3.0f\n"
        "ANGER     %3.0f\n"
        "FEAR      %3.0f\n"
        "CURIOSITY %3.0f\n"
        "AFFECTION %3.0f",
        pet.mood(), reactionName(pet.reaction().kind()), actionTag(pet.action()),
        needs.hunger, needs.energy, needs.social, needs.boredom,
        emotions.value(Emotion::Joy), emotions.value(Emotion::Sadness),
        emotions.value(Emotion::Anger), emotions.value(Emotion::Fear),
        emotions.value(Emotion::Curiosity), emotions.value(Emotion::Affection));

    lv_label_set_text(ui.debugLeftLabel, left);
    lv_label_set_text(ui.debugRightLabel, right);
}

void showDebugPanel() {
    motion::requestWake();
    if (ui.debugPanel == nullptr) return;
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
        const float offsetY = y - 205.0F;
        const float radius = std::sqrt(offsetX * offsetX + offsetY * offsetY);
        if (radius < 48.0F || radius > 158.0F) {
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
    makeFlatObject(ui.appendages[0], 0x40977F, LV_RADIUS_CIRCLE);

    ui.appendages[1] = lv_obj_create(parent);
    lv_obj_add_flag(ui.appendages[1], static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK));
    lv_obj_clear_flag(ui.appendages[1], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(ui.appendages[1], onCreaturePointer, LV_EVENT_ALL, nullptr);
    makeFlatObject(ui.appendages[1], 0x40977F, LV_RADIUS_CIRCLE);

    ui.appendages[2] = lv_obj_create(parent);
    lv_obj_add_flag(ui.appendages[2], static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK));
    lv_obj_clear_flag(ui.appendages[2], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(ui.appendages[2], onCreaturePointer, LV_EVENT_ALL, nullptr);
    makeFlatObject(ui.appendages[2], 0x40977F, LV_RADIUS_CIRCLE);

    ui.body = lv_obj_create(parent);
    makeFlatObject(ui.body, 0x64C5A7, LV_RADIUS_CIRCLE);
    lv_obj_set_style_border_width(ui.body, 4, LV_PART_MAIN);
    lv_obj_set_style_border_color(ui.body, color(0x347E70), LV_PART_MAIN);
    // Keep the creature flat: software shadow blur is expensive on the ESP32-S3.
    lv_obj_add_flag(ui.body, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK));
    lv_obj_clear_flag(ui.body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(ui.body, onCreaturePointer, LV_EVENT_ALL, nullptr);

    for (int i = 0; i < 2; ++i) {
        ui.eyes[i] = lv_obj_create(ui.body);
        lv_obj_add_flag(ui.eyes[i], LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_clear_flag(ui.eyes[i], LV_OBJ_FLAG_SCROLLABLE);
        makeFlatObject(ui.eyes[i], 0xF5F7F4, LV_RADIUS_CIRCLE);
        lv_obj_set_style_border_width(ui.eyes[i], 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(ui.eyes[i], color(0xDCE5E1), LV_PART_MAIN);

        ui.pupils[i] = lv_obj_create(ui.eyes[i]);
        lv_obj_add_flag(ui.pupils[i], LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_clear_flag(ui.pupils[i], LV_OBJ_FLAG_SCROLLABLE);
        makeFlatObject(ui.pupils[i], 0x182630, LV_RADIUS_CIRCLE);

        ui.highlights[i] = lv_obj_create(ui.pupils[i]);
        lv_obj_clear_flag(ui.highlights[i], LV_OBJ_FLAG_SCROLLABLE);
        makeFlatObject(ui.highlights[i], 0xFFFFFF, LV_RADIUS_CIRCLE);
        lv_obj_set_size(ui.highlights[i], 7, 7);
        lv_obj_set_pos(ui.highlights[i], 4, 4);

        ui.brows[i] = lv_obj_create(ui.body);
        lv_obj_add_flag(ui.brows[i], LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_clear_flag(ui.brows[i], LV_OBJ_FLAG_SCROLLABLE);
        makeFlatObject(ui.brows[i], 0x347E70, LV_RADIUS_CIRCLE);
        lv_obj_add_flag(ui.brows[i], LV_OBJ_FLAG_HIDDEN);
    }

    ui.mouth = lv_obj_create(ui.body);
    lv_obj_add_flag(ui.mouth, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_clear_flag(ui.mouth, LV_OBJ_FLAG_SCROLLABLE);
    makeFlatObject(ui.mouth, 0x25333A, LV_RADIUS_CIRCLE);

    lv_obj_set_size(ui.body, 291, 260);
    lv_obj_set_pos(ui.body, 87, 88);
    lv_obj_set_size(ui.eyes[0], 66, 70);
    lv_obj_set_size(ui.eyes[1], 64, 70);
    lv_obj_set_pos(ui.eyes[0], 49, 76);
    lv_obj_set_pos(ui.eyes[1], 169, 76);
    lv_obj_set_size(ui.pupils[0], 22, 29);
    lv_obj_set_size(ui.pupils[1], 22, 29);
    lv_obj_set_pos(ui.pupils[0], 22, 20);
    lv_obj_set_pos(ui.pupils[1], 21, 20);
    lv_obj_set_size(ui.mouth, 38, 10);
    lv_obj_set_pos(ui.mouth, 126, 187);
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
    lv_obj_set_pos(ui.status, 23, 354);

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

    lv_obj_t* debugLeftCard = lv_obj_create(ui.debugPanel);
    lv_obj_remove_style_all(debugLeftCard);
    lv_obj_set_size(debugLeftCard, 207, 350);
    lv_obj_set_pos(debugLeftCard, 0, 54);
    lv_obj_set_style_bg_color(debugLeftCard, color(0x1A2A35), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(debugLeftCard, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(debugLeftCard, 16, LV_PART_MAIN);
    lv_obj_set_style_border_width(debugLeftCard, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(debugLeftCard, color(0x344B58), LV_PART_MAIN);
    lv_obj_set_style_pad_all(debugLeftCard, 10, LV_PART_MAIN);
    lv_obj_clear_flag(debugLeftCard, LV_OBJ_FLAG_SCROLLABLE);
    ui.debugLeftLabel = makeLabel(debugLeftCard, "", &lv_font_montserrat_12, 0xBDE5D5);
    lv_obj_set_width(ui.debugLeftLabel, 187);
    lv_obj_set_pos(ui.debugLeftLabel, 0, 0);
    lv_obj_set_style_text_align(ui.debugLeftLabel, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_label_set_long_mode(ui.debugLeftLabel, LV_LABEL_LONG_WRAP);

    lv_obj_t* debugRightCard = lv_obj_create(ui.debugPanel);
    lv_obj_remove_style_all(debugRightCard);
    lv_obj_set_size(debugRightCard, 207, 350);
    lv_obj_set_pos(debugRightCard, 219, 54);
    lv_obj_set_style_bg_color(debugRightCard, color(0x1D3038), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(debugRightCard, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(debugRightCard, 16, LV_PART_MAIN);
    lv_obj_set_style_border_width(debugRightCard, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(debugRightCard, color(0x3A5A5D), LV_PART_MAIN);
    lv_obj_set_style_pad_all(debugRightCard, 10, LV_PART_MAIN);
    lv_obj_clear_flag(debugRightCard, LV_OBJ_FLAG_SCROLLABLE);
    ui.debugRightLabel = makeLabel(debugRightCard, "", &lv_font_montserrat_12, 0xD0E9DE);
    lv_obj_set_width(ui.debugRightLabel, 187);
    lv_obj_set_pos(ui.debugRightLabel, 0, 0);
    lv_obj_set_style_text_align(ui.debugRightLabel, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_label_set_long_mode(ui.debugRightLabel, LV_LABEL_LONG_WRAP);

    lv_obj_t* debugHint = makeLabel(ui.debugPanel, "BOOT: BACK TO CREATURE", &lv_font_montserrat_12, 0x91AAA4);
    lv_obj_set_width(debugHint, 430);
    lv_obj_set_pos(debugHint, 0, 420);
    refreshDebugPanel();

    ui.menuPanel = lv_obj_create(screen);
    lv_obj_remove_style_all(ui.menuPanel);
    lv_obj_set_size(ui.menuPanel, 364, 326);
    lv_obj_set_pos(ui.menuPanel, 51, 70);
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

    createMenuButton(ui.menuPanel, 64, "FEED", 0xE5C98F, onMenuFeedClicked);
    createMenuButton(ui.menuPanel, 128, "CREATURE LAB", 0x78C9A9, onMenuLabClicked);
    createMenuButton(ui.menuPanel, 192, "VOLUME", 0x8EBBD0, onMenuVolumeClicked);
    createMenuButton(ui.menuPanel, 256, "CLOSE", 0xA8B9B5, onMenuCloseClicked);

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
    const int bodyWidth = static_cast<int>(291.0F
        * (0.88F + genes.bodyWidth * 0.0024F) * hitScale);
    const int bodyHeight = static_cast<int>(260.0F
        * (0.88F + genes.bodyHeight * 0.0024F) * hitScale);
    bodyY = 205.0F - static_cast<float>(bodyHeight) * 0.5F
        + std::sin(phase * 1.8F) * (1.5F + activity * 2.2F);
    bodyX = (466.0F - static_cast<float>(bodyWidth)) * 0.5F + bodyOffsetX;
    bodyY += bodyOffsetY;
    lv_obj_set_size(ui.body, bodyWidth, bodyHeight);
    lv_obj_set_pos(ui.body, static_cast<int>(bodyX), static_cast<int>(bodyY));
    static bool previousHitFlash = false;
    const bool hitFlash = hitElapsed >= 0.0F && hitElapsed < 0.045F;
    if (hitFlash != previousHitFlash) {
        lv_obj_set_style_bg_color(ui.body, color(hitFlash ? 0xF4E2B2 : 0x64C5A7), LV_PART_MAIN);
        previousHitFlash = hitFlash;
    }

    const float appendageWave = std::sin(phase * (2.3F + activity * 2.8F));
    const float appendageBoost = exaggerated ? 1.8F : 1.0F;
    const int appendageLength = 28 + static_cast<int>(genes.appendageLength * 0.30F);
    lv_obj_set_size(ui.appendages[0], appendageLength + 36, 25);
    lv_obj_set_size(ui.appendages[1], appendageLength + 36, 25);
    lv_obj_set_size(ui.appendages[2], 22, appendageLength + 20);
    lv_obj_set_pos(ui.appendages[0], static_cast<int>(bodyX - appendageLength * 0.72F),
        static_cast<int>(bodyY + bodyHeight * 0.54F + appendageWave * 5.0F * appendageBoost));
    lv_obj_set_pos(ui.appendages[1], static_cast<int>(bodyX + bodyWidth - 48.0F + appendageLength * 0.30F),
        static_cast<int>(bodyY + bodyHeight * 0.49F - appendageWave * 6.0F * appendageBoost));
    lv_obj_set_pos(ui.appendages[2], static_cast<int>(bodyX + bodyWidth * 0.47F),
        static_cast<int>(bodyY - appendageLength * 0.80F + std::sin(phase * 3.0F) * 4.0F));
    for (int i = 0; i < 3; ++i) {
        const bool visible = genes.appendageCount > i;
        if (visible) lv_obj_clear_flag(ui.appendages[i], LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(ui.appendages[i], LV_OBJ_FLAG_HIDDEN);
    }

    const float eyeScale = 0.82F + static_cast<float>(genes.eyeSize) * 0.0032F;
    const int eyeWidth = static_cast<int>(64.0F * eyeScale);
    const int baseEyeHeight = static_cast<int>(70.0F * eyeScale);
    const int eyeY = static_cast<int>(bodyHeight * 0.29F);
    const int leftX = static_cast<int>(bodyWidth * 0.17F);
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
    int eyeHeight = baseEyeHeight;
    if (angryFace) eyeHeight = static_cast<int>(baseEyeHeight * 0.72F);
    else if (happyFace) eyeHeight = static_cast<int>(baseEyeHeight * 0.66F);
    else if (sadFace) eyeHeight = static_cast<int>(baseEyeHeight * 0.82F);
    else if (surprisedFace || afraidFace) eyeHeight = static_cast<int>(baseEyeHeight * 1.10F);
    if (blink) eyeHeight = 7;

    static bool previousBlink = false;
    if (blink != previousBlink) {
        for (int i = 0; i < 2; ++i) {
            lv_obj_set_style_bg_color(ui.eyes[i], color(blink ? 0x4DAD94 : 0xF5F7F4), LV_PART_MAIN);
        }
        previousBlink = blink;
    }

    for (int i = 0; i < 2; ++i) {
        const int eyeX = i == 0 ? leftX : rightX;
        const int individualEyeWidth = eyeWidth + (i == 0 ? 2 : -1);
        lv_obj_set_size(ui.eyes[i], individualEyeWidth, eyeHeight);
        lv_obj_set_pos(ui.eyes[i], eyeX, eyeY + (blink ? 20 : 0));

        if (blink) {
            lv_obj_add_flag(ui.pupils[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(ui.pupils[i], LV_OBJ_FLAG_HIDDEN);
            const int pupilWidth = std::max(16, static_cast<int>(individualEyeWidth * (afraidFace ? 0.29F : 0.34F)));
            const int pupilHeight = std::max(20, static_cast<int>(eyeHeight * 0.43F));
            lv_obj_set_size(ui.pupils[i], pupilWidth, pupilHeight);
            const int pupilX = (individualEyeWidth - pupilWidth) / 2 + static_cast<int>(gazeX);
            const int pupilY = (eyeHeight - pupilHeight) / 2 + static_cast<int>(gazeY);
            lv_obj_set_pos(ui.pupils[i], pupilX, pupilY);
        }

        if (angryFace) {
            lv_obj_clear_flag(ui.brows[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_size(ui.brows[i], individualEyeWidth + 3, 7);
            lv_obj_set_pos(ui.brows[i], eyeX - 1, eyeY - 8 + (i == 0 ? 4 : 0));
        } else {
            lv_obj_add_flag(ui.brows[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

    int mouthWidth = 38;
    int mouthHeight = 10;
    int mouthY = static_cast<int>(bodyHeight * 0.72F);
    if (happyFace) { mouthWidth = 40; mouthHeight = 10; mouthY -= 2; }
    if (sadFace) { mouthWidth = 27; mouthHeight = 6; mouthY += 5; }
    if (angryFace) { mouthWidth = 31; mouthHeight = 5; mouthY += 2; }
    if (surprisedFace || afraidFace) { mouthWidth = 18; mouthHeight = 21; }
    if (disgustedFace) { mouthWidth = 37; mouthHeight = 5; }
    if (reaction == Reaction::Curious) { mouthWidth = 13; mouthHeight = 13; }

    lv_obj_set_size(ui.mouth, mouthWidth, mouthHeight);
    lv_obj_set_pos(ui.mouth, (bodyWidth - mouthWidth) / 2, mouthY);
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
    if (storageReady) restoredFromSave = persistence::load(pet);
    createUi();
    if (restoredFromSave) setMessage("WELCOME BACK", 2.0F);
}

void toggleDebugPanel() {
    if (ui.volumePanel != nullptr
        && !lv_obj_has_flag(ui.volumePanel, LV_OBJ_FLAG_HIDDEN)) {
        hideVolumePanel();
    }
    if (ui.debugPanel == nullptr) return;
    if (ui.menuPanel != nullptr && !lv_obj_has_flag(ui.menuPanel, LV_OBJ_FLAG_HIDDEN)) {
        hideMenuPanel();
        return;
    }
    if (lv_obj_has_flag(ui.debugPanel, LV_OBJ_FLAG_HIDDEN)) {
        hideMenuPanel();
        showDebugPanel();
    } else {
        hideDebugPanel();
    }
}

void toggleMenuPanel() {
    if (ui.volumePanel != nullptr
        && !lv_obj_has_flag(ui.volumePanel, LV_OBJ_FLAG_HIDDEN)) {
        hideVolumePanel();
    }
    if (ui.menuPanel == nullptr) return;
    if (ui.debugPanel != nullptr) hideDebugPanel();
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
    updateBackground(dt);

    const Reaction currentReaction = pet.reaction().kind();
    if (currentReaction == Reaction::None) lastAudioReaction = Reaction::None;
    else if (currentReaction != lastAudioReaction) {
        audio::play(cueForReaction(currentReaction));
        lastAudioReaction = currentReaction;
    }

    const bool debugVisible = ui.debugPanel != nullptr
        && !lv_obj_has_flag(ui.debugPanel, LV_OBJ_FLAG_HIDDEN);
    const bool menuVisible = ui.menuPanel != nullptr
        && !lv_obj_has_flag(ui.menuPanel, LV_OBJ_FLAG_HIDDEN);
    const bool volumeVisible = ui.volumePanel != nullptr
        && !lv_obj_has_flag(ui.volumePanel, LV_OBJ_FLAG_HIDDEN);
    if (!debugVisible && !menuVisible && !volumeVisible && !sensor.screenSleeping) {
        updateCreatureVisuals();
    }
    if (debugVisible && animationTime >= nextDebugRefresh) {
        refreshDebugPanel();
        nextDebugRefresh = animationTime + 0.20F;
    }
}

} // namespace evolvinggarden::app
