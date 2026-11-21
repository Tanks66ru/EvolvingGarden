#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "lvgl.h"

#include "simulation/creature_simulation.hpp"
#include "audio_controller.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>

using namespace evolvinggarden;

namespace {

constexpr char Tag[] = "EvolvingGarden";
constexpr float FrameStepSeconds = 1.0F / 30.0F;

struct PetUi {
    lv_obj_t* body = nullptr;
    lv_obj_t* eyes[2]{};
    lv_obj_t* pupils[2]{};
    lv_obj_t* mouth = nullptr;
    lv_obj_t* brows[2]{};
    lv_obj_t* appendages[3]{};
    lv_obj_t* status = nullptr;
    lv_obj_t* actionButton = nullptr;
    lv_obj_t* actionLabel = nullptr;
    lv_obj_t* debugPanel = nullptr;
    lv_obj_t* debugLabel = nullptr;
};

struct ActionOption {
    Interaction interaction;
    const char* label;
    const char* acceptedMessage;
    std::uint32_t buttonColor;
};

CreatureSimulation pet(makeDefaultGenome());
PetUi ui;
float animationTime = 0.0F;
float messageUntil = 0.0F;
float nextDebugRefresh = 0.0F;
const char* messageOverride = nullptr;
const char* lastStatus = nullptr;
int selectedAction = 0;
Reaction lastAudioReaction = Reaction::None;

ActionOption actionOptions[] = {
    {Interaction::Pet, "PET", "THAT'S NICE", 0xB7E3D1},
    {Interaction::Feed, "FEED", "YUM!", 0xE5C98F},
    {Interaction::Novelty, "DISCOVER", "WHAT IS THAT?", 0xB9CAE9}
};

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

void updateActionButton() {
    if (ui.actionButton == nullptr || ui.actionLabel == nullptr) return;
    const ActionOption& option = actionOptions[selectedAction];
    lv_label_set_text(ui.actionLabel, option.label);
    lv_obj_set_style_bg_color(ui.actionButton, color(option.buttonColor), LV_PART_MAIN);
}
void selectNextAction() {
    selectedAction = (selectedAction + 1) % (sizeof(actionOptions) / sizeof(actionOptions[0]));
    updateActionButton();
}
void performSelectedAction() {
    const ActionOption& option = actionOptions[selectedAction];
    if (pet.applyInteraction(option.interaction)) {
        messageOverride = option.acceptedMessage;
        messageUntil = animationTime + 1.15F;
        setStatus(messageOverride);
        audio::play(audio::Cue::Button);
    }
}
void onActionClicked(lv_event_t*) { performSelectedAction(); }

void refreshDebugPanel() {
    if (ui.debugLabel == nullptr) return;
    const CreatureNeeds& needs = pet.needs();
    char text[512]{};
    std::snprintf(text, sizeof(text),
        "MICROPHONE   %s   %u%%\n"
        "SPEAKER      %s\n\n"
        "HUNGER       %3.0f%%\n"
        "ENERGY       %3.0f%%\n"
        "SOCIAL       %3.0f%%\n"
        "BOREDOM      %3.0f%%\n\n"
        "MOOD         %+.0f\n"
        "REACTION     %s\n"
        "ACTION       %s\n\n"
        "AUDIO INPUT   %s\n"
        "AUDIO OUTPUT  %s",
        audio::microphoneReady() ? "READY" : "OFF",
        static_cast<unsigned>(audio::microphoneLevelPercent()),
        audio::speakerReady() ? "READY" : "OFF",
        needs.hunger, needs.energy, needs.social, needs.boredom,
        pet.mood(), reactionName(pet.reaction().kind()), actionTag(pet.action()),
        audio::microphoneReady() ? "ONBOARD MIC" : "UNAVAILABLE",
        audio::speakerReady() ? "ONBOARD SPEAKER" : "UNAVAILABLE");
    lv_label_set_text(ui.debugLabel, text);
}
void showDebugPanel() {
    if (ui.debugPanel == nullptr) return;
    refreshDebugPanel();
    lv_obj_clear_flag(ui.debugPanel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(ui.debugPanel);
}
void hideDebugPanel() {
    if (ui.debugPanel != nullptr) lv_obj_add_flag(ui.debugPanel, LV_OBJ_FLAG_HIDDEN);
}
void onMainGesture(lv_event_t*) {
    lv_indev_t* indev = lv_indev_active();
    if (indev == nullptr) return;
    const lv_dir_t direction = lv_indev_get_gesture_dir(indev);
    if (direction == LV_DIR_LEFT) showDebugPanel();
    else if (direction == LV_DIR_RIGHT) selectNextAction();
}
void onDebugGesture(lv_event_t*) {
    lv_indev_t* indev = lv_indev_active();
    if (indev != nullptr && lv_indev_get_gesture_dir(indev) == LV_DIR_RIGHT) hideDebugPanel();
}
void onBodyTouched(lv_event_t*) {
    if (pet.applyInteraction(Interaction::Pet)) {
        messageOverride = "THAT'S NICE";
        messageUntil = animationTime + 1.15F;
        setStatus(messageOverride);
    }
}

void createCreature(lv_obj_t* parent) {
    // Appendages are created first so the body is drawn over their roots.
    ui.appendages[0] = lv_obj_create(parent);
    lv_obj_add_flag(ui.appendages[0], LV_OBJ_FLAG_GESTURE_BUBBLE);
    makeFlatObject(ui.appendages[0], 0x40977F, LV_RADIUS_CIRCLE);

    ui.appendages[1] = lv_obj_create(parent);
    lv_obj_add_flag(ui.appendages[1], LV_OBJ_FLAG_GESTURE_BUBBLE);
    makeFlatObject(ui.appendages[1], 0x40977F, LV_RADIUS_CIRCLE);

    ui.appendages[2] = lv_obj_create(parent);
    lv_obj_add_flag(ui.appendages[2], LV_OBJ_FLAG_GESTURE_BUBBLE);
    makeFlatObject(ui.appendages[2], 0x40977F, LV_RADIUS_CIRCLE);

    ui.body = lv_obj_create(parent);
    makeFlatObject(ui.body, 0x64C5A7, LV_RADIUS_CIRCLE);
    lv_obj_set_style_border_width(ui.body, 4, LV_PART_MAIN);
    lv_obj_set_style_border_color(ui.body, color(0x347E70), LV_PART_MAIN);
    lv_obj_set_style_shadow_width(ui.body, 18, LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(ui.body, 45, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(ui.body, color(0x0A151C), LV_PART_MAIN);
    lv_obj_add_flag(ui.body, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(ui.body, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(ui.body, onBodyTouched, LV_EVENT_CLICKED, nullptr);

    for (int i = 0; i < 2; ++i) {
        ui.eyes[i] = lv_obj_create(ui.body);
        lv_obj_add_flag(ui.eyes[i], LV_OBJ_FLAG_GESTURE_BUBBLE);
        makeFlatObject(ui.eyes[i], 0xF5F7F4, LV_RADIUS_CIRCLE);
        lv_obj_set_style_border_width(ui.eyes[i], 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(ui.eyes[i], color(0xDCE5E1), LV_PART_MAIN);

        ui.pupils[i] = lv_obj_create(ui.eyes[i]);
        lv_obj_add_flag(ui.pupils[i], LV_OBJ_FLAG_GESTURE_BUBBLE);
        makeFlatObject(ui.pupils[i], 0x182630, LV_RADIUS_CIRCLE);

        ui.brows[i] = lv_obj_create(ui.body);
        lv_obj_add_flag(ui.brows[i], LV_OBJ_FLAG_GESTURE_BUBBLE);
        makeFlatObject(ui.brows[i], 0x347E70, LV_RADIUS_CIRCLE);
        lv_obj_add_flag(ui.brows[i], LV_OBJ_FLAG_HIDDEN);
    }

    ui.mouth = lv_obj_create(ui.body);
    lv_obj_add_flag(ui.mouth, LV_OBJ_FLAG_GESTURE_BUBBLE);
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
    lv_obj_add_event_cb(screen, onMainGesture, LV_EVENT_GESTURE, nullptr);

    createCreature(screen);

    ui.status = makeLabel(screen, "WANDERING", &lv_font_montserrat_14, 0xBDE5D5);
    lv_obj_add_flag(ui.status, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_set_width(ui.status, 420);
    lv_obj_set_pos(ui.status, 23, 354);

    ui.actionButton = lv_button_create(screen);
    lv_obj_set_size(ui.actionButton, 280, 66);
    lv_obj_set_pos(ui.actionButton, 93, 384);
    lv_obj_set_style_bg_color(ui.actionButton, color(actionOptions[0].buttonColor), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui.actionButton, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(ui.actionButton, 24, LV_PART_MAIN);
    lv_obj_set_style_border_width(ui.actionButton, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(ui.actionButton, 0, LV_PART_MAIN);
    lv_obj_add_flag(ui.actionButton, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(ui.actionButton, onActionClicked, LV_EVENT_CLICKED, nullptr);

    ui.actionLabel = makeLabel(ui.actionButton, "PET", &lv_font_montserrat_18, 0x14252C);
    lv_obj_center(ui.actionLabel);
    lv_obj_add_flag(ui.actionLabel, LV_OBJ_FLAG_GESTURE_BUBBLE);

    ui.debugPanel = lv_obj_create(screen);
    lv_obj_remove_style_all(ui.debugPanel);
    lv_obj_set_size(ui.debugPanel, 466, 466);
    lv_obj_set_pos(ui.debugPanel, 0, 0);
    lv_obj_set_style_bg_color(ui.debugPanel, color(0x111B24), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ui.debugPanel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(ui.debugPanel, 30, LV_PART_MAIN);
    lv_obj_add_flag(ui.debugPanel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(ui.debugPanel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(ui.debugPanel, onDebugGesture, LV_EVENT_GESTURE, nullptr);

    lv_obj_t* debugTitle = makeLabel(ui.debugPanel, "CREATURE LAB", &lv_font_montserrat_18, 0xEAF2EF);
    lv_obj_set_width(debugTitle, 406);
    lv_obj_set_pos(debugTitle, 0, 8);
    ui.debugLabel = makeLabel(ui.debugPanel, "", &lv_font_montserrat_14, 0xBDE5D5);
    lv_obj_set_width(ui.debugLabel, 406);
    lv_obj_set_pos(ui.debugLabel, 0, 72);
    lv_obj_set_style_text_align(ui.debugLabel, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_label_set_long_mode(ui.debugLabel, LV_LABEL_LONG_WRAP);
    lv_obj_t* debugHint = makeLabel(ui.debugPanel, "SWIPE RIGHT TO RETURN", &lv_font_montserrat_12, 0x91AAA4);
    lv_obj_set_width(debugHint, 406);
    lv_obj_set_pos(debugHint, 0, 408);
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

    float bodyX = 103.0F;
    float bodyY = 104.0F + std::sin(phase * 1.8F) * (1.5F + activity * 2.2F);
    if (happyFace) bodyY -= std::abs(std::sin(phase * 8.0F)) * 8.0F * std::max(0.25F, reactionPower);
    if (afraidFace) bodyX += std::sin(phase * 24.0F) * (exaggerated ? 6.0F : 3.0F);
    if (angryFace) bodyX += std::sin(phase * 18.0F) * 2.5F;
    if (reaction == Reaction::Curious) bodyX += 3.5F * reactionPower;

    const int bodyWidth = static_cast<int>(260.0F * (0.88F + genes.bodyWidth * 0.0024F));
    const int bodyHeight = static_cast<int>(232.0F * (0.88F + genes.bodyHeight * 0.0024F));
    bodyX = (466.0F - static_cast<float>(bodyWidth)) * 0.5F;
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

void petTimer(lv_timer_t*) {
    animationTime += FrameStepSeconds;
    float soundIntensity = 0.0F;
    float soundDuration = 0.0F;
    if (audio::consumeSoundEvent(soundIntensity, soundDuration)) {
        pet.applySoundStimulus(soundIntensity, soundDuration);
    }
    pet.update(FrameStepSeconds);

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

} // namespace

extern "C" void app_main(void) {
    ESP_LOGI(Tag, "Starting EvolvingGarden on ESP32-S3-Touch-AMOLED-1.75C");
    if (bsp_display_start() == nullptr) {
        ESP_LOGE(Tag, "Display initialization failed");
        return;
    }
    audio::initialize();

    bsp_display_lock(-1);
    if (lv_indev_t* indev = bsp_display_get_input_dev()) {
        lv_indev_set_gesture_min_distance(indev, 42);
        lv_indev_set_gesture_min_velocity(indev, 100);
    }
    createUi();
    lv_timer_create(petTimer, 33, nullptr);
    bsp_display_unlock();

    ESP_LOGI(Tag, "Ready. Tap the action button; swipe right to cycle actions, left for debug.");
}
