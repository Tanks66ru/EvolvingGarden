#include "platform/audio.hpp"

#include <SDL.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <iterator>
#include <vector>

namespace evolvinggarden::audio {
namespace {

constexpr int PreferredSampleRate = 22050;
constexpr std::size_t PreferredBufferSamples = 512;
constexpr float PiTimesTwo = 6.28318530718F;

SDL_AudioDeviceID playbackDevice = 0;
SDL_AudioDeviceID captureDevice = 0;
SDL_AudioSpec playbackFormat{};
SDL_AudioSpec captureFormat{};

std::atomic<bool> soundPending{false};
std::atomic<float> eventIntensity{0.0F};
std::atomic<float> eventDuration{0.0F};
std::atomic<std::uint8_t> inputLevel{0};
std::atomic<std::uint8_t> outputVolumePercent{100};
std::atomic<std::uint64_t> ignoreMicUntil{0};

struct Tone {
    std::uint16_t frequency;
    std::uint16_t durationMs;
    std::uint16_t gapMs;
};

struct ToneSequence {
    const Tone* tones;
    std::size_t count;
};

ToneSequence sequenceFor(Cue cue) {
    static constexpr Tone button[] = {{1250, 34, 0}};
    static constexpr Tone hit[] = {{115, 42, 0}, {78, 48, 0}};
    static constexpr Tone happy[] = {{660, 55, 8}, {880, 65, 8}, {1175, 90, 0}};
    static constexpr Tone sad[] = {{440, 110, 18}, {349, 150, 0}};
    static constexpr Tone angry[] = {{190, 70, 12}, {155, 80, 0}};
    static constexpr Tone fearful[] = {{990, 45, 8}, {740, 65, 8}, {510, 95, 0}};
    static constexpr Tone surprised[] = {{740, 42, 5}, {1120, 45, 5}, {1480, 85, 0}};
    static constexpr Tone curious[] = {{560, 55, 12}, {760, 65, 12}, {980, 85, 0}};
    static constexpr Tone affectionate[] = {{523, 75, 10}, {659, 75, 10}, {784, 120, 0}};
    static constexpr Tone disgusted[] = {{310, 60, 8}, {250, 75, 8}, {180, 95, 0}};

    switch (cue) {
    case Cue::Button: return {button, std::size(button)};
    case Cue::Hit: return {hit, std::size(hit)};
    case Cue::Happy: return {happy, std::size(happy)};
    case Cue::Sad: return {sad, std::size(sad)};
    case Cue::Angry: return {angry, std::size(angry)};
    case Cue::Fearful: return {fearful, std::size(fearful)};
    case Cue::Surprised: return {surprised, std::size(surprised)};
    case Cue::Curious: return {curious, std::size(curious)};
    case Cue::Affectionate: return {affectionate, std::size(affectionate)};
    case Cue::Disgusted: return {disgusted, std::size(disgusted)};
    }
    return {button, std::size(button)};
}

void publishSoundEvent(float peakRms, std::uint32_t sampleCount, float noiseFloor) {
    if (sampleCount == 0) return;

    const float duration = std::min(
        3.0F, static_cast<float>(sampleCount) / static_cast<float>(captureFormat.freq));
    if (duration < 0.03F) return;

    const float threshold = std::max(900.0F, noiseFloor * 3.0F + 450.0F);
    const float intensity = std::clamp(
        0.20F + (peakRms - threshold) / (threshold * 3.5F), 0.12F, 1.0F);

    eventIntensity.store(intensity);
    eventDuration.store(duration);
    soundPending.store(true);
}

void captureCallback(void*, Uint8* stream, int length) {
    if (stream == nullptr || length <= 0 || captureFormat.freq <= 0) return;

    const auto* samples = reinterpret_cast<const std::int16_t*>(stream);
    const std::size_t sampleCount = static_cast<std::size_t>(length) / sizeof(std::int16_t);
    if (sampleCount == 0) return;

    std::uint64_t sum = 0;
    for (std::size_t i = 0; i < sampleCount; ++i) {
        const std::int32_t value = samples[i];
        sum += static_cast<std::uint64_t>(value * value);
    }

    const float rms = std::sqrt(static_cast<float>(sum) / static_cast<float>(sampleCount));
    inputLevel.store(static_cast<std::uint8_t>(std::min(100.0F, rms / 50.0F)));

    static float noiseFloor = 180.0F;
    static float peakRms = 0.0F;
    static std::uint32_t eventSamples = 0;
    static int loudFrames = 0;

    if (SDL_GetTicks64() < ignoreMicUntil.load()) {
        peakRms = 0.0F;
        eventSamples = 0;
        loudFrames = 0;
        return;
    }

    const float threshold = std::max(900.0F, noiseFloor * 3.0F + 450.0F);
    if (rms > threshold) {
        ++loudFrames;
        eventSamples += static_cast<std::uint32_t>(sampleCount);
        peakRms = std::max(peakRms, rms);

        if (eventSamples >= static_cast<std::uint32_t>(captureFormat.freq * 3)) {
            publishSoundEvent(peakRms, eventSamples, noiseFloor);
            peakRms = 0.0F;
            eventSamples = 0;
            loudFrames = 0;
        }
    } else {
        if (loudFrames >= 2) {
            publishSoundEvent(peakRms, eventSamples, noiseFloor);
        }

        peakRms = 0.0F;
        eventSamples = 0;
        loudFrames = 0;

        if (rms < threshold * 1.35F) {
            noiseFloor = noiseFloor * 0.97F + rms * 0.03F;
        }
    }
}

bool openPlayback() {
    SDL_AudioSpec wanted{};
    wanted.freq = PreferredSampleRate;
    wanted.format = AUDIO_S16SYS;
    wanted.channels = 1;
    wanted.samples = static_cast<Uint16>(PreferredBufferSamples);

    playbackDevice = SDL_OpenAudioDevice(
        nullptr, 0, &wanted, &playbackFormat,
        SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_SAMPLES_CHANGE);
    if (playbackDevice == 0) {
        std::fprintf(stderr, "Desktop speaker unavailable: %s\n", SDL_GetError());
        return false;
    }

    SDL_PauseAudioDevice(playbackDevice, 0);
    return true;
}

bool openCapture() {
    SDL_AudioSpec wanted{};
    wanted.freq = PreferredSampleRate;
    wanted.format = AUDIO_S16SYS;
    wanted.channels = 1;
    wanted.samples = static_cast<Uint16>(PreferredBufferSamples);
    wanted.callback = captureCallback;

    captureDevice = SDL_OpenAudioDevice(
        nullptr, 1, &wanted, &captureFormat,
        SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_SAMPLES_CHANGE);
    if (captureDevice == 0) {
        std::fprintf(stderr, "Desktop microphone unavailable: %s\n", SDL_GetError());
        return false;
    }

    SDL_PauseAudioDevice(captureDevice, 0);
    return true;
}

} // namespace

void initialize() {
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        std::fprintf(stderr, "SDL audio initialization failed: %s\n", SDL_GetError());
        return;
    }

    const bool speakerOnline = openPlayback();
    const bool microphoneOnline = openCapture();

    std::printf("Desktop audio: speaker %s, microphone %s.\n",
        speakerOnline ? "ready" : "off",
        microphoneOnline ? "ready" : "off");
}

void play(Cue cue) {
    if (playbackDevice == 0) return;

    const ToneSequence sequence = sequenceFor(cue);
    if (sequence.tones == nullptr || sequence.count == 0) return;

    if (SDL_GetQueuedAudioSize(playbackDevice) > static_cast<Uint32>(playbackFormat.freq * 4)) {
        return;
    }

    std::vector<std::int16_t> samples;
    std::uint32_t totalDurationMs = 0;

    for (std::size_t toneIndex = 0; toneIndex < sequence.count; ++toneIndex) {
        const Tone& tone = sequence.tones[toneIndex];
        const std::size_t toneSamples =
            static_cast<std::size_t>(playbackFormat.freq) * tone.durationMs / 1000U;
        const float phaseStep = PiTimesTwo * static_cast<float>(tone.frequency)
            / static_cast<float>(playbackFormat.freq);
        float phase = 0.0F;

        for (std::size_t i = 0; i < toneSamples; ++i) {
            const float attack = std::min(1.0F, static_cast<float>(i) / 110.0F);
            const float release = std::min(1.0F, static_cast<float>(toneSamples - i) / 220.0F);
            const float envelope = std::min(attack, release);
            samples.push_back(static_cast<std::int16_t>(
                std::sin(phase) * 2600.0F
                * (static_cast<float>(outputVolumePercent.load()) / 100.0F)
                * envelope));
            phase += phaseStep;
        }

        const std::size_t gapSamples =
            static_cast<std::size_t>(playbackFormat.freq) * tone.gapMs / 1000U;
        samples.insert(samples.end(), gapSamples, 0);
        totalDurationMs += tone.durationMs + tone.gapMs;
    }

    if (!samples.empty()) {
        const auto bytes = static_cast<Uint32>(samples.size() * sizeof(std::int16_t));
        if (SDL_QueueAudio(playbackDevice, samples.data(), bytes) == 0) {
            ignoreMicUntil.store(SDL_GetTicks64() + totalDurationMs + 160U);
        }
    }
}

void setVolumePercent(std::uint8_t percent) {
    outputVolumePercent.store(std::min<std::uint8_t>(
        percent, static_cast<std::uint8_t>(100)));
}

std::uint8_t volumePercent() {
    return outputVolumePercent.load();
}

bool consumeSoundEvent(float& intensity, float& durationSeconds) {
    if (!soundPending.exchange(false)) return false;
    intensity = eventIntensity.load();
    durationSeconds = eventDuration.load();
    return true;
}

std::uint8_t microphoneLevelPercent() {
    return inputLevel.load();
}

bool microphoneReady() {
    return captureDevice != 0;
}

bool speakerReady() {
    return playbackDevice != 0;
}

} // namespace evolvinggarden::audio
