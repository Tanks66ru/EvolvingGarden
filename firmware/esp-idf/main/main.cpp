#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "lvgl.h"

#include "simulation/creature_simulation.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

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
};

struct InteractionButton {
    Interaction interaction;
    const char* acceptedMessage;
};

CreatureSimulation pet(makeDefaultGenome());
PetUi ui;
float animationTime = 0.0F;
float messageUntil = 0.0F;
const char* messageOverride = nullptr;
const char* lastStatus = nullptr;

InteractionButton buttonData[] = {
    {Interaction::Pet, "THAT'S NICE"},
    {Interaction::Feed, "YUM!"},
    {Interaction::Novelty, "WHAT IS THAT?"}
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

void onInteraction(lv_event_t* event) {
    auto* data = static_cast<InteractionButton*>(lv_event_get_user_data(event));
    if (data == nullptr) return;

    if (pet.applyInteraction(data->interaction)) {
        messageOverride = data->acceptedMessage;
        messageUntil = animationTime + 1.15F;
        setStatus(messageOverride);
    }
}

void createButton(lv_obj_t* parent, int x, InteractionButton* data,
                  const char* labelText, std::uint32_t buttonColor) {
    lv_obj_t* button = lv_button_create(parent);
    lv_obj_set_size(button, 112, 42);
    lv_obj_set_pos(button, x, 397);
    lv_obj_set_style_bg_color(button, color(buttonColor), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(button, 17, LV_PART_MAIN);
    lv_obj_set_style_border_width(button, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(button, 0, LV_PART_MAIN);
    lv_obj_add_event_cb(button, onInteraction, LV_EVENT_CLICKED, data);

    lv_obj_t* label = makeLabel(button, labelText, &lv_font_montserrat_14, 0x14252C);
    lv_obj_center(label);
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
    makeFlatObject(ui.appendages[0], 0x40977F, LV_RADIUS_CIRCLE);

    ui.appendages[1] = lv_obj_create(parent);
    makeFlatObject(ui.appendages[1], 0x40977F, LV_RADIUS_CIRCLE);

    ui.appendages[2] = lv_obj_create(parent);
    makeFlatObject(ui.appendages[2], 0x40977F, LV_RADIUS_CIRCLE);

    ui.body = lv_obj_create(parent);
    makeFlatObject(ui.body, 0x64C5A7, LV_RADIUS_CIRCLE);
    lv_obj_set_style_border_width(ui.body, 4, LV_PART_MAIN);
    lv_obj_set_style_border_color(ui.body, color(0x347E70), LV_PART_MAIN);
    lv_obj_set_style_shadow_width(ui.body, 18, LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(ui.body, 45, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(ui.body, color(0x0A151C), LV_PART_MAIN);
    lv_obj_add_flag(ui.body, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ui.body, onBodyTouched, LV_EVENT_CLICKED, nullptr);

    for (int i = 0; i < 2; ++i) {
        ui.eyes[i] = lv_obj_create(ui.body);
        makeFlatObject(ui.eyes[i], 0xF5F7F4, LV_RADIUS_CIRCLE);
        lv_obj_set_style_border_width(ui.eyes[i], 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(ui.eyes[i], color(0xDCE5E1), LV_PART_MAIN);

        ui.pupils[i] = lv_obj_create(ui.eyes[i]);
        makeFlatObject(ui.pupils[i], 0x182630, LV_RADIUS_CIRCLE);

        ui.brows[i] = lv_obj_create(ui.body);
        makeFlatObject(ui.brows[i], 0x347E70, LV_RADIUS_CIRCLE);
        lv_obj_add_flag(ui.brows[i], LV_OBJ_FLAG_HIDDEN);
    }

    ui.mouth = lv_obj_create(ui.body);
    makeFlatObject(ui.mouth, 0x25333A, LV_RADIUS_CIRCLE);

    lv_obj_set_size(ui.body, 250, 220);
    lv_obj_set_pos(ui.body, 108, 130);
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

    lv_obj_t* title = makeLabel(screen, "EVOLVING GARDEN", &lv_font_montserrat_16, 0xEAF2EF);
    lv_obj_set_pos(title, 0, 15);
    lv_obj_set_width(title, 466);

    lv_obj_t* subtitle = makeLabel(screen, "A LITTLE LIFE, RIGHT HERE", &lv_font_montserrat_12, 0x91AAA4);
    lv_obj_set_pos(subtitle, 0, 39);
    lv_obj_set_width(subtitle, 466);

    createCreature(screen);

    ui.status = makeLabel(screen, "WANDERING", &lv_font_montserrat_14, 0xBDE5D5);
    lv_obj_set_width(ui.status, 420);
    lv_obj_set_pos(ui.status, 23, 361);

    createButton(screen, 38, &buttonData[0], "PET", 0xB7E3D1);
    createButton(screen, 177, &buttonData[1], "FEED", 0xE5C98F);
    createButton(screen, 316, &buttonData[2], "DISCOVER", 0xB9CAE9);

    lv_obj_t* footer = makeLabel(screen, "TAP THE CREATURE TOO", &lv_font_montserrat_12, 0x728A85);
    lv_obj_set_width(466, 466);
    lv_obj_set_pos(footer, 0, 449);
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

    float bodyX = 108.0F;
    float bodyY = 130.0F + std::sin(phase * 1.8F) * (1.5F + activity * 2.2F);
    if (happyFace) bodyY -= std::abs(std::sin(phase * 8.0F)) * 8.0F * std::max(0.25F, reactionPower);
    if (afraidFace) bodyX += std::sin(phase * 24.0F) * (exaggerated ? 6.0F : 3.0F);
    if (angryFace) bodyX += std::sin(phase * 18.0F) * 2.5F;
    if (reaction == Reaction::Curious) bodyX += 3.5F * reactionPower;

    const int bodyWidth = static_cast<int>(236.0F * (0.88F + genes.bodyWidth * 0.0024F));
    const int bodyHeight = static_cast<int>(210.0F * (0.88F + genes.bodyHeight * 0.0024F));
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

void petTimer(lv_timer_t*) {
    animationTime += FrameStepSeconds;
    pet.update(FrameStepSeconds);
    updateCreatureVisuals();
}

} // namespace

extern "C" void app_main(void) {
    ESP_LOGI(Tag, "Starting EvolvingGarden on ESP32-S3-Touch-AMOLED-1.75C");

    // The official Waveshare BSP initializes the CO5300 display, CST9217 touch
    // controller, LVGL port, and the required display buffers.
    bsp_display_start();

    bsp_display_lock(-1);
    createUi();
    lv_timer_create(petTimer, 33, nullptr);
    bsp_display_unlock();

    ESP_LOGI(Tag, "Pet UI is ready. Touch the creature or use the three buttons.");
}
