#pragma once
#include <cstdint>

namespace evolvinggarden::audio {
enum class Cue : std::uint8_t {
    Button, Happy, Sad, Angry, Fearful, Surprised, Curious, Affectionate, Disgusted
};
void initialize();
void play(Cue cue);
bool consumeSoundEvent(float& intensity, float& durationSeconds);
std::uint8_t microphoneLevelPercent();
bool microphoneReady();
bool speakerReady();
} // namespace evolvinggarden::audio
