#include "lvgl.h"

#include "simulation/creature_simulation.hpp"
#include "platform/audio.hpp"
#include "platform/motion.hpp"

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
    lv_obj_t* mouth = nullptr;
    lv_obj_t* brows[2]{};
    lv_obj_t* appendages[3]{};
    lv_obj_t* status = nullptr;
    lv_obj_t* stepsLabel = nullptr;
    lv_obj_t* hintLabel = nullptr;
    lv_obj_t* actionButton = nullptr;
    lv_obj_t* actionLabel = nullptr;
    lv_obj_t* debugOpenButton = nullptr;
    lv_obj_t* debugCloseButton = nullptr;
    lv_obj_t* debugPanel = nullptr;
    lv_obj_t* debugLabel = nullptr;
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
float deviceRoll = 0.0F;
float devicePitch = 0.0F;
const char* messageOverride = nullptr;
const char* lastStatus = nullptr;
std::uint32_t lastMotionSteps = 0;
std::uint32_t lastShakeEvents = 0;
std::uint32_t lastTiltEvents = 0;
std::uint32_t lastRotationEvents = 0;
std::uint32_t lastDisplayedSteps = 0xFFFFFFFFU;
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

void setMessage(const char* message, float duration = 1.2F) {
    messageOverride = message;
    messageUntil = animationTime + duration;
    setStatus(message);
}

void performFeed() {
    motion::requestWake();
    if (pet.applyInteraction(Interaction::Feed)) {
        setMessage("YUM!");
        audio::play(audio::Cue::Button);
    }
}

void performPet() {
    motion::requestWake();
    if (pet.applyInteraction(Interaction::Pet)) setMessage("ENJOYING THE SCRATCH", 1.0F);
}

void performHit(float touchX) {
    motion::requestWake();
    hitDirection = touchX < 233.0F ? 1.0F : -1.0F;
    if (pet.applyInteraction(Interaction::Hit)) {
        lastHitAnimation = animationTime;
        setMessage(pet.enjoysSparring() ? "AGAIN! AGAIN!" : "HEY! THAT HURT!");
    }
}

void refreshDebugPanel() {
    if (ui.debugLabel == nullptr) return;
    const CreatureNeeds& needs = pet.needs();
    const motion::State sensor = motion::state();
    char text[768]{};
    std::snprintf(text, sizeof(text),
        "MICROPHONE   %s   %u%%\n"
        "SPEAKER      %s\n"
        "MOTION       %s\n"
        "ORIENTATION  %s\n"
        "SCREEN       %s\n\n"
        "STEPS        %lu\n"
        "SHAKES       %lu\n"
        "TILTS        %lu\n"
        "ROTATIONS    %lu\n"
        "ROLL / PITCH %+.0f / %+.0f\n"
        "ACCEL / GYRO %.1f / %.1f\n\n"
        "HUNGER       %3.0f%%\n"
        "ENERGY       %3.0f%%\n"
        "SOCIAL       %3.0f%%\n"
        "BOREDOM      %3.0f%%\n\n"
        "MOOD         %+.0f\n"
        "REACTION     %s\n"
        "ACTION       %s",
        audio::microphoneReady() ? "READY" : "OFF",
        static_cast<unsigned>(audio::microphoneLevelPercent()),
        audio::speakerReady() ? "READY" : "OFF",
        sensor.ready ? "READY" : "OFF",
        sensor.orientationCalibrated ? "CALIBRATED" : "HOLD FLAT",
        sensor.screenSleeping ? "SLEEPING" : "AWAKE",
        static_cast<unsigned long>(pet.stepsTaken()),
        static_cast<unsigned long>(sensor.shakeEvents),
        static_cast<unsigned long>(sensor.tiltEvents),
        static_cast<unsigned long>(sensor.rotationEvents),
        sensor.rollDegrees, sensor.pitchDegrees,
        sensor.linearAcceleration, sensor.angularVelocity,
        needs.hunger, needs.energy, needs.social, needs.boredom,
        pet.mood(), reactionName(pet.reaction().kind()), actionTag(pet.action()));
    lv_label_set_text(ui.debugLabel, text);
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

void onActionClicked(lv_event_t*) { performFeed(); }
void onDebugOpenClicked(lv_event_t*) { showDebugPanel(); }
void onDebugCloseClicked(lv_event_t*) { hideDebugPanel(); }

void onScreenTouch(lv_event_t*) {
    motion::requestWake();
}

void onBackgroundTapped(lv_event_t*) {
    motion::requestWake();
    lv_indev_t* indev = lv_indev_active();
    if (indev == nullptr) return;
    lv_point_t point{};
    lv_indev_get_point(indev, &point);
    performHit(static_cast<float>(point.x));
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
        const float offsetY = y - 220.0F;
        const float radius = std::sqrt(offsetX * offsetX + offsetY * offsetY);
        if (radius < 48.0F || radius > 158.0F) return;

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

        const int direction = delta > 0.0F ? 1 : -1;
        if (creatureTouch.direction == 0) creatureTouch.direction = direction;
        if (direction == creatureTouch.direction) {
            creatureTouch.accumulatedAngle += std::fabs(delta);
        } else {
            creatureTouch.accumulatedAngle = std::max(
                0.0F, creatureTouch.accumulatedAngle - std::fabs(delta) * 0.65F);
        }

        constexpr float FullCircle = 6.28318530718F;
        if (creatureTouch.accumulatedAngle >= FullCircle
            && animationTime - creatureTouch.lastPetTime >= 0.70F) {
            performPet();
            creatureTouch.accumulatedAngle -= FullCircle;
            creatureTouch.lastPetTime = animationTime;
            creatureTouch.petTriggered = true;
        }
        return;
    }

    if (code == LV_EVENT_RELEASED) {
        if (creatureTouch.accumulatedAngle >= 4.7F && !creatureTouch.petTriggered) {
            performPet();
        } else if (creatureTouch.pathLength < 18.0F && !creatureTouch.petTriggered) {
            lv_indev_t* indev = lv_indev_active();
            lv_point_t point{};
            if (indev != nullptr) lv_indev_get_point(indev, &point);
            performHit(static_cast<float>(point.x));
        }
        creatureTouch.active = false;
    }
}

void createCreature(lv_obj_t* parent) {
    // Appendages are created first so the body is drawn over their roots.
    ui.appendages[0] = lv_obj_create(parent);
    lv_obj_add_flag(ui.appendages[0], LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_clear_flag(ui.appendages[0], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(ui.appendages[0], onCreaturePointer, LV_EVENT_ALL, nullptr);
    makeFlatObject(ui.appendages[0], 0x40977F, LV_RADIUS_CIRCLE);

    ui.appendages[1] = lv_obj_create(parent);
    lv_obj_add_flag(ui.appendages[1], LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_clear_flag(ui.appendages[1], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(ui.appendages[1], onCreaturePointer, LV_EVENT_ALL, nullptr);
    makeFlatObject(ui.appendages[1], 0x40977F, LV_RADIUS_CIRCLE);

    ui.appendages[2] = lv_obj_create(parent);
    lv_obj_add_flag(ui.appendages[2], LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_clear_flag(ui.appendages[2], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(ui.appendages[2], onCreaturePointer, LV_EVENT_ALL, nullptr);
    makeFlatObject(ui.appendages[2], 0x40977F, LV_RADIUS_CIRCLE);

    ui.body = lv_obj_create(parent);
    makeFlatObject(ui.body, 0x64C5A7, LV_RADIUS_CIRCLE);
    lv_obj_set_style_border_width(ui.body, 4, LV_PART_MAIN);
    lv_obj_set_style_border_color(ui.body, color(0x347E70), LV_PART_MAIN);
    lv_obj_set_style_shadow_width(ui.body, 18, LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(ui.body, 45, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(ui.body, color(0x0A151C), LV_PART_MAIN);
    lv_obj_add_flag(ui.body, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK);
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

    lv_obj_set_size(ui.body, 260, 232);
    lv_obj_set_pos(ui.body, 103, 104);
    lv_obj_set_size(ui.eyes[0], 56, 62);
    lv_obj_set_size(ui.eyes[1], 56, 62);
    lv_obj_set_pos(ui.eyes[0], 48, 68);
    lv_obj_set_pos(ui.eyes[1], 146, 68);
    lv_obj_set_size(ui.pupils[0], 19, 25);
    lv_obj_set_size(ui.pupils[1], 19, 25);
    lv_obj_set_pos(ui.pupils[0], 18, 19);
    lv_obj_set_pos(ui.pupils[1], 18, 19);
    lv_obj_set_size(ui.mouth, 34, 8);
    lv_obj_set_pos(ui.mouth, 108, 157);
}

void createUi() {
    lv_obj_t* screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, color(0x111B24), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(screen, 0, LV_PART_MAIN);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(screen, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(screen, onScreenTouch, LV_EVENT_PRESSED, nullptr);
    lv_obj_add_event_cb(screen, onBackgroundTapped, LV_EVENT_CLICKED, nullptr);

    createCreature(screen);

    ui.stepsLabel = makeLabel(screen, "STEPS 000000", &lv_font_montserrat_14, 0xBDE5D5);
    lv_obj_set_width(ui.stepsLabel, 300);
    lv_obj_set_pos(ui.stepsLabel, 24, 18);

    ui.debugOpenButton = lv_button_create(screen);
    lv_obj_set_size(ui.debugOpenButton, 76, 34);
    lv_obj_set_pos(ui.debugOpenButton, 366, 12);
    lv_obj_set_style_bg_color(ui.debugOpenButton, color(0x344451), LV_PART_MAIN);
    lv_obj_set_style_radius(ui.debugOpenButton, 12, LV_PART_MAIN);
    lv_obj_add_event_cb(ui.debugOpenButton, onDebugOpenClicked, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* debugOpenLabel = makeLabel(ui.debugOpenButton, "LAB", &lv_font_montserrat_12, 0xEAF2EF);
    lv_obj_center(debugOpenLabel);

    ui.hintLabel = makeLabel(screen, "CIRCLE: PET     TAP: HIT", &lv_font_montserrat_12, 0x91AAA4);
    lv_obj_set_width(ui.hintLabel, 420);
    lv_obj_set_pos(ui.hintLabel, 23, 48);

    ui.status = makeLabel(screen, "WAITING FOR YOU", &lv_font_montserrat_14, 0xBDE5D5);
    lv_obj_set_width(ui.status, 420);
    lv_obj_set_pos(ui.status, 23, 354);

    ui.actionButton = lv_button_create(screen);
    lv_obj_set_size(ui.actionButton, 280, 66);
    lv_obj_set_pos(ui.actionButton, 93, 384);
    lv_obj_set_style_bg_color(ui.actionButton, color(0xE5C98F), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui.actionButton, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(ui.actionButton, 24, LV_PART_MAIN);
    lv_obj_set_style_border_width(ui.actionButton, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(ui.actionButton, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(ui.actionButton, onActionClicked, LV_EVENT_CLICKED, nullptr);
    ui.actionLabel = makeLabel(ui.actionButton, "FEED", &lv_font_montserrat_18, 0x14252C);
    lv_obj_center(ui.actionLabel);

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

    ui.debugLabel = makeLabel(ui.debugPanel, "", &lv_font_montserrat_14, 0xBDE5D5);
    lv_obj_set_width(ui.debugLabel, 430);
    lv_obj_set_pos(ui.debugLabel, 0, 54);
    lv_obj_set_style_text_align(ui.debugLabel, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_label_set_long_mode(ui.debugLabel, LV_LABEL_LONG_WRAP);

    lv_obj_t* debugHint = makeLabel(ui.debugPanel, "TAP BACK TO RETURN", &lv_font_montserrat_12, 0x91AAA4);
    lv_obj_set_width(debugHint, 430);
    lv_obj_set_pos(debugHint, 0, 420);
    refreshDebugPanel();
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
    if (hitElapsed >= 0.0F && hitElapsed < 0.72F) {
        const float envelope = std::sin((hitElapsed / 0.72F) * 3.14159265F);
        if (pet.enjoysSparring()) {
            // Dodge first, then make a short counter-lunge.
            if (hitElapsed < 0.20F) bodyOffsetX -= hitDirection * 24.0F * envelope;
            else bodyOffsetX += hitDirection * 15.0F * envelope;
            bodyOffsetY -= 5.0F * envelope;
        } else {
            bodyOffsetX -= hitDirection * 11.0F * envelope;
            bodyOffsetY += 4.0F * envelope;
        }
    }

    float bodyX = 103.0F;
    float bodyY = 104.0F + std::sin(phase * 1.8F) * (1.5F + activity * 2.2F);
    if (happyFace) bodyY -= std::abs(std::sin(phase * 8.0F)) * 8.0F * std::max(0.25F, reactionPower);
    if (afraidFace) bodyX += std::sin(phase * 24.0F) * (exaggerated ? 6.0F : 3.0F);
    if (angryFace) bodyX += std::sin(phase * 18.0F) * 2.5F;
    if (reaction == Reaction::Curious) bodyX += 3.5F * reactionPower;

    const int bodyWidth = static_cast<int>(260.0F * (0.88F + genes.bodyWidth * 0.0024F));
    const int bodyHeight = static_cast<int>(232.0F * (0.88F + genes.bodyHeight * 0.0024F));
    bodyX = (466.0F - static_cast<float>(bodyWidth)) * 0.5F + bodyOffsetX;
    bodyY += bodyOffsetY;
    lv_obj_set_size(ui.body, bodyWidth, bodyHeight);
    lv_obj_set_pos(ui.body, static_cast<int>(bodyX), static_cast<int>(bodyY));

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
    const int eyeWidth = static_cast<int>(56.0F * eyeScale);
    const int baseEyeHeight = static_cast<int>(61.0F * eyeScale);
    const int eyeY = static_cast<int>(bodyHeight * 0.30F);
    const int leftX = static_cast<int>(bodyWidth * 0.19F);
    const int rightX = static_cast<int>(bodyWidth * 0.59F);

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

    for (int i = 0; i < 2; ++i) {
        const int eyeX = i == 0 ? leftX : rightX;
        lv_obj_set_size(ui.eyes[i], eyeWidth, eyeHeight);
        lv_obj_set_pos(ui.eyes[i], eyeX, eyeY + (blink ? 20 : 0));
        lv_obj_set_style_bg_color(ui.eyes[i], color(blink ? 0x4DAD94 : 0xF5F7F4), LV_PART_MAIN);

        if (blink) {
            lv_obj_add_flag(ui.pupils[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(ui.pupils[i], LV_OBJ_FLAG_HIDDEN);
            const int pupilWidth = std::max(13, static_cast<int>(eyeWidth * (afraidFace ? 0.28F : 0.34F)));
            const int pupilHeight = std::max(16, static_cast<int>(eyeHeight * 0.40F));
            lv_obj_set_size(ui.pupils[i], pupilWidth, pupilHeight);
            const int pupilX = (eyeWidth - pupilWidth) / 2 + static_cast<int>(gazeX);
            const int pupilY = (eyeHeight - pupilHeight) / 2 + static_cast<int>(gazeY);
            lv_obj_set_pos(ui.pupils[i], pupilX, pupilY);
        }

        if (angryFace) {
            lv_obj_clear_flag(ui.brows[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_size(ui.brows[i], eyeWidth + 3, 7);
            lv_obj_set_pos(ui.brows[i], eyeX - 1, eyeY - 8 + (i == 0 ? 4 : 0));
        } else {
            lv_obj_add_flag(ui.brows[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

    int mouthWidth = 34;
    int mouthHeight = 8;
    int mouthY = static_cast<int>(bodyHeight * 0.72F);
    if (happyFace) { mouthWidth = 40; mouthHeight = 10; mouthY -= 2; }
    if (sadFace) { mouthWidth = 27; mouthHeight = 6; mouthY += 5; }
    if (angryFace) { mouthWidth = 31; mouthHeight = 5; mouthY += 2; }
    if (surprisedFace || afraidFace) { mouthWidth = 18; mouthHeight = 21; }
    if (disgustedFace) { mouthWidth = 37; mouthHeight = 5; }
    if (reaction == Reaction::Curious) { mouthWidth = 13; mouthHeight = 13; }

    lv_obj_set_size(ui.mouth, mouthWidth, mouthHeight);
    lv_obj_set_pos(ui.mouth, (bodyWidth - mouthWidth) / 2, mouthY);
    if (surprisedFace || afraidFace || reaction == Reaction::Curious) {
        lv_obj_set_style_bg_color(ui.mouth, color(0x25333A), LV_PART_MAIN);
    } else {
        lv_obj_set_style_bg_color(ui.mouth, color(0x25333A), LV_PART_MAIN);
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
    createUi();
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
    }
    if (sensor.shakeEvents > lastShakeEvents) {
        pet.applyInteraction(Interaction::Shake);
        lastShakeEvents = sensor.shakeEvents;
    }
    if (sensor.tiltEvents > lastTiltEvents) {
        pet.applyInteraction(Interaction::Tilt);
        lastTiltEvents = sensor.tiltEvents;
    }
    if (sensor.rotationEvents > lastRotationEvents) {
        pet.applyInteraction(Interaction::Rotate);
        lastRotationEvents = sensor.rotationEvents;
    }

    if (ui.stepsLabel != nullptr && lastDisplayedSteps != pet.stepsTaken()) {
        char stepsText[40]{};
        std::snprintf(stepsText, sizeof(stepsText), "STEPS %06lu",
            static_cast<unsigned long>(pet.stepsTaken()));
        lv_label_set_text(ui.stepsLabel, stepsText);
        lastDisplayedSteps = pet.stepsTaken();
    }

    float soundIntensity = 0.0F;
    float soundDuration = 0.0F;
    if (audio::consumeSoundEvent(soundIntensity, soundDuration)) {
        pet.applySoundStimulus(soundIntensity, soundDuration);
    }
    pet.update(dt);

    const Reaction currentReaction = pet.reaction().kind();
    if (currentReaction == Reaction::None) lastAudioReaction = Reaction::None;
    else if (currentReaction != lastAudioReaction) {
        audio::play(cueForReaction(currentReaction));
        lastAudioReaction = currentReaction;
    }

    updateCreatureVisuals();
    if (ui.debugPanel != nullptr
        && !lv_obj_has_flag(ui.debugPanel, LV_OBJ_FLAG_HIDDEN)
        && animationTime >= nextDebugRefresh) {
        refreshDebugPanel();
        nextDebugRefresh = animationTime + 0.20F;
    }
}

} // namespace evolvinggarden::app
