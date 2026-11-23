#include "audio_controller.hpp"

#include "bsp/esp-bsp.h"
#include "esp_codec_dev.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_spiffs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace evolvinggarden::audio {
namespace {
constexpr char Tag[] = "GardenAudio";
constexpr char MountPath[] = "/spiffs";
constexpr char Partition[] = "storage";
constexpr std::uint32_t SampleRate = 22050;
constexpr std::size_t CaptureSamples = 512;
constexpr std::size_t PlaybackSamples = 256;

esp_codec_dev_handle_t mic = nullptr;
esp_codec_dev_handle_t spk = nullptr;
QueueHandle_t outputQueue = nullptr;
std::atomic<bool> micOnline{false};
std::atomic<bool> speakerOnline{false};
std::atomic<bool> fsOnline{false};
std::atomic<bool> playing{false};
std::atomic<bool> soundPending{false};
std::atomic<float> eventIntensity{0.0F};
std::atomic<float> eventDuration{0.0F};
std::atomic<std::uint8_t> inputLevel{0};

struct Tone { std::uint16_t hz; std::uint16_t ms; std::uint16_t gap; };

const char* pathFor(Cue cue) {
    switch (cue) {
    case Cue::Button: return "/spiffs/button_click.wav";
    case Cue::Hit: return "/spiffs/hit_impact.wav";
    case Cue::Happy: return "/spiffs/reaction_happy.wav";
    case Cue::Sad: return "/spiffs/reaction_sad.wav";
    case Cue::Angry: return "/spiffs/reaction_angry.wav";
    case Cue::Fearful: return "/spiffs/reaction_fearful.wav";
    case Cue::Surprised: return "/spiffs/reaction_surprised.wav";
    case Cue::Curious: return "/spiffs/reaction_curious.wav";
    case Cue::Affectionate: return "/spiffs/reaction_affectionate.wav";
    case Cue::Disgusted: return "/spiffs/reaction_disgusted.wav";
    }
    return nullptr;
}

std::uint16_t read16(FILE* f, bool& ok) {
    unsigned char b[2]{};
    if (std::fread(b, 1, 2, f) != 2) { ok = false; return 0; }
    return static_cast<std::uint16_t>(b[0] | (b[1] << 8));
}
std::uint32_t read32(FILE* f, bool& ok) {
    unsigned char b[4]{};
    if (std::fread(b, 1, 4, f) != 4) { ok = false; return 0; }
    return static_cast<std::uint32_t>(b[0])
        | (static_cast<std::uint32_t>(b[1]) << 8)
        | (static_cast<std::uint32_t>(b[2]) << 16)
        | (static_cast<std::uint32_t>(b[3]) << 24);
}

bool findWavData(FILE* f, std::uint32_t& dataSize) {
    char id[4]{};
    bool ok = true;
    if (std::fread(id, 1, 4, f) != 4 || std::memcmp(id, "RIFF", 4) != 0) return false;
    (void)read32(f, ok);
    if (!ok || std::fread(id, 1, 4, f) != 4 || std::memcmp(id, "WAVE", 4) != 0) return false;

    bool validPcm = false;
    for (;;) {
        if (std::fread(id, 1, 4, f) != 4) return false;
        const std::uint32_t size = read32(f, ok);
        if (!ok) return false;
        const long start = std::ftell(f);
        if (std::memcmp(id, "fmt ", 4) == 0) {
            if (size < 16) return false;
            const auto format = read16(f, ok);
            const auto channels = read16(f, ok);
            const auto rate = read32(f, ok);
            (void)read32(f, ok);
            (void)read16(f, ok);
            const auto bits = read16(f, ok);
            if (!ok) return false;
            validPcm = format == 1 && channels == 1 && rate == SampleRate && bits == 16;
        } else if (std::memcmp(id, "data", 4) == 0) {
            if (!validPcm) return false;
            dataSize = size;
            return true;
        }
        const long next = start + static_cast<long>(size + (size & 1U));
        if (std::fseek(f, next, SEEK_SET) != 0) return false;
    }
}

bool playWav(const char* path) {
    if (!fsOnline.load() || spk == nullptr) return false;
    FILE* f = std::fopen(path, "rb");
    if (f == nullptr) return false;
    std::uint32_t remaining = 0;
    if (!findWavData(f, remaining)) { std::fclose(f); return false; }

    playing.store(true);
    std::int16_t samples[PlaybackSamples];
    bool ok = true;
    while (remaining > 0) {
        const std::size_t bytes = std::min<std::size_t>(sizeof(samples), remaining);
        const std::size_t count = std::fread(samples, 1, bytes, f);
        if (count == 0 || (count & 1U) != 0
            || esp_codec_dev_write(spk, samples, static_cast<int>(count)) != ESP_CODEC_DEV_OK) {
            ok = false;
            break;
        }
        remaining -= static_cast<std::uint32_t>(count);
    }
    std::fclose(f);
    playing.store(false);
    return ok;
}

void playTones(const Tone* tones, std::size_t toneCount) {
    if (spk == nullptr) return;
    playing.store(true);
    std::int16_t samples[PlaybackSamples];

    for (std::size_t n = 0; n < toneCount; ++n) {
        const std::size_t total = static_cast<std::size_t>(SampleRate) * tones[n].ms / 1000U;
        const float step = 6.28318530718F * tones[n].hz / static_cast<float>(SampleRate);
        float phase = 0.0F;
        for (std::size_t offset = 0; offset < total; offset += PlaybackSamples) {
            const std::size_t count = std::min(PlaybackSamples, total - offset);
            for (std::size_t i = 0; i < count; ++i) {
                const std::size_t pos = offset + i;
                const float attack = std::min(1.0F, static_cast<float>(pos) / 110.0F);
                const float release = std::min(1.0F, static_cast<float>(total - pos) / 220.0F);
                samples[i] = static_cast<std::int16_t>(
                    std::sin(phase) * 2600.0F * std::min(attack, release));
                phase += step;
            }
            if (esp_codec_dev_write(spk, samples,
                    static_cast<int>(count * sizeof(std::int16_t))) != ESP_CODEC_DEV_OK) {
                playing.store(false);
                return;
            }
        }
        if (tones[n].gap > 0) vTaskDelay(pdMS_TO_TICKS(tones[n].gap));
    }
    playing.store(false);
}

void fallback(Cue cue) {
    static const Tone button[] = {{1250, 34, 0}};
    static const Tone hit[] = {{115, 42, 0}, {78, 48, 0}};
    static const Tone happy[] = {{660, 55, 8}, {880, 65, 8}, {1175, 90, 0}};
    static const Tone sad[] = {{440, 110, 18}, {349, 150, 0}};
    static const Tone angry[] = {{190, 70, 12}, {155, 80, 0}};
    static const Tone fearful[] = {{990, 45, 8}, {740, 65, 8}, {510, 95, 0}};
    static const Tone surprised[] = {{740, 42, 5}, {1120, 45, 5}, {1480, 85, 0}};
    static const Tone curious[] = {{560, 55, 12}, {760, 65, 12}, {980, 85, 0}};
    static const Tone affectionate[] = {{523, 75, 10}, {659, 75, 10}, {784, 120, 0}};
    static const Tone disgusted[] = {{310, 60, 8}, {250, 75, 8}, {180, 95, 0}};
    switch (cue) {
    case Cue::Button: playTones(button, 1); break;
    case Cue::Hit: playTones(hit, 2); break;
    case Cue::Happy: playTones(happy, 3); break;
    case Cue::Sad: playTones(sad, 2); break;
    case Cue::Angry: playTones(angry, 2); break;
    case Cue::Fearful: playTones(fearful, 3); break;
    case Cue::Surprised: playTones(surprised, 3); break;
    case Cue::Curious: playTones(curious, 3); break;
    case Cue::Affectionate: playTones(affectionate, 3); break;
    case Cue::Disgusted: playTones(disgusted, 3); break;
    }
}

void outputTask(void*) {
    Cue cue{};
    for (;;) {
        if (xQueueReceive(outputQueue, &cue, portMAX_DELAY) != pdTRUE) continue;
        const char* path = pathFor(cue);
        if (path == nullptr || !playWav(path)) fallback(cue);
    }
}

void inputTask(void*) {
    std::int16_t samples[CaptureSamples]{};
    float noiseFloor = 180.0F;
    float peakRms = 0.0F;
    std::uint32_t eventSamples = 0;
    int loudFrames = 0;

    auto publishEvent = [&]() {
        if (loudFrames < 2) return;
        const float duration = std::min(3.0F,
            static_cast<float>(eventSamples) / static_cast<float>(SampleRate));
        if (duration < 0.03F) return;
        const float threshold = std::max(900.0F, noiseFloor * 3.0F + 450.0F);
        const float intensity = std::clamp(
            0.20F + (peakRms - threshold) / (threshold * 3.5F), 0.12F, 1.0F);
        eventIntensity.store(intensity);
        eventDuration.store(duration);
        soundPending.store(true);
    };

    for (;;) {
        const auto result = esp_codec_dev_read(mic, samples, static_cast<int>(sizeof(samples)));
        if (result != ESP_CODEC_DEV_OK) {
            inputLevel.store(0);
            vTaskDelay(pdMS_TO_TICKS(40));
            continue;
        }

        std::uint64_t sum = 0;
        for (const auto sample : samples) {
            const std::int32_t value = sample;
            sum += static_cast<std::uint64_t>(value * value);
        }
        const float rms = std::sqrt(static_cast<float>(sum) / CaptureSamples);
        inputLevel.store(static_cast<std::uint8_t>(std::min(100.0F, rms / 50.0F)));

        // Ignore playback captured by the board's own microphones.
        if (playing.load()) {
            loudFrames = 0;
            eventSamples = 0;
            peakRms = 0.0F;
            continue;
        }

        const float threshold = std::max(900.0F, noiseFloor * 3.0F + 450.0F);
        if (rms > threshold) {
            ++loudFrames;
            eventSamples += static_cast<std::uint32_t>(CaptureSamples);
            peakRms = std::max(peakRms, rms);

            // Emit long continuous sounds in bounded chunks.
            if (eventSamples >= SampleRate * 3U) {
                publishEvent();
                loudFrames = 0;
                eventSamples = 0;
                peakRms = 0.0F;
                vTaskDelay(pdMS_TO_TICKS(180));
            }
        } else {
            // A short quiet gap closes the current sound event.
            if (loudFrames >= 2) {
                publishEvent();
                vTaskDelay(pdMS_TO_TICKS(180));
            }
            loudFrames = 0;
            eventSamples = 0;
            peakRms = 0.0F;
            if (rms < threshold * 1.35F) {
                noiseFloor = noiseFloor * 0.97F + rms * 0.03F;
            }
        }
    }
}

void mountStorage() {
    esp_vfs_spiffs_conf_t config{};
    config.base_path = MountPath;
    config.partition_label = Partition;
    config.max_files = 8;
    config.format_if_mount_failed = false;
    const esp_err_t err = esp_vfs_spiffs_register(&config);
    if (err == ESP_OK) {
        fsOnline.store(true);
        ESP_LOGI(Tag, "SPIFFS sound library mounted");
    } else {
        ESP_LOGW(Tag, "SPIFFS unavailable (%s); built-in tones remain active", esp_err_to_name(err));
    }
}
} // namespace

void initialize() {
    outputQueue = xQueueCreate(8, sizeof(Cue));
    if (outputQueue == nullptr) {
        ESP_LOGE(Tag, "Cannot allocate audio queue");
        return;
    }
    mountStorage();

    esp_codec_dev_sample_info_t format{};
    format.bits_per_sample = 16;
    format.channel = 1;
    format.sample_rate = SampleRate;

    spk = bsp_audio_codec_speaker_init();
    if (spk != nullptr && esp_codec_dev_open(spk, &format) == ESP_CODEC_DEV_OK) {
        speakerOnline.store(true);
        esp_codec_dev_set_out_vol(spk, 68);
        ESP_LOGI(Tag, "Speaker ready");
    } else {
        spk = nullptr;
        ESP_LOGW(Tag, "Speaker unavailable");
    }

    mic = bsp_audio_codec_microphone_init();
    if (mic != nullptr && esp_codec_dev_open(mic, &format) == ESP_CODEC_DEV_OK) {
        micOnline.store(true);
        esp_codec_dev_set_in_gain(mic, 24.0F);
        ESP_LOGI(Tag, "Microphone ready");
    } else {
        mic = nullptr;
        ESP_LOGW(Tag, "Microphone unavailable");
    }

    if (speakerOnline.load()
        && xTaskCreate(outputTask, "garden_audio", 6144, nullptr, 4, nullptr) != pdPASS) {
        speakerOnline.store(false);
        ESP_LOGE(Tag, "Cannot create audio output task");
    }
    if (micOnline.load()
        && xTaskCreate(inputTask, "garden_mic", 4096, nullptr, 3, nullptr) != pdPASS) {
        micOnline.store(false);
        ESP_LOGE(Tag, "Cannot create microphone task");
    }
}

void play(Cue cue) {
    if (outputQueue != nullptr && speakerOnline.load()) (void)xQueueSend(outputQueue, &cue, 0);
}
bool consumeSoundEvent(float& intensity, float& durationSeconds) {
    if (!soundPending.exchange(false)) return false;
    intensity = eventIntensity.load();
    durationSeconds = eventDuration.load();
    return true;
}
std::uint8_t microphoneLevelPercent() { return inputLevel.load(); }
bool microphoneReady() { return micOnline.load(); }
bool speakerReady() { return speakerOnline.load(); }
} // namespace evolvinggarden::audio
