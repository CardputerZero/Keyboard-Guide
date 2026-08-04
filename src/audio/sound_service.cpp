#include "audio/sound_service.hpp"

#include <miniaudio.h>
#include <spdlog/spdlog.h>

#include <array>
#include <cstdlib>
#include <filesystem>
#include <mutex>

namespace keyboard_guide {
namespace {

struct CueDefinition {
    const char* file_name;
    float volume;
};

constexpr float kMasterVolume = 5.0f;

constexpr std::array<CueDefinition, 6> kCueDefinitions = {{
    {"mechanical-typing.wav", 1.00f},
    {"mechanical-lock.wav", 0.67f},
    {"mechanical-unlock.wav", 0.70f},
    {"mechanical-error.wav", 0.70f},
    {"mechanical-notification.wav", 0.432f},
    {"mechanical-achievement.wav", 0.72f},
}};

std::filesystem::path audioAssetDirectory()
{
    if (const char* override_path = std::getenv("KEYBOARD_GUIDE_AUDIO_DIR"); override_path && *override_path) {
        return override_path;
    }
    return KEYBOARD_GUIDE_AUDIO_ASSET_DIR;
}

std::size_t cueIndex(SoundCue cue)
{
    return static_cast<std::size_t>(cue);
}

}  // namespace

struct SoundService::Impl {
    ma_context context{};
    ma_engine engine{};
    std::array<ma_sound, kCueDefinitions.size()> sounds{};
    std::array<bool, kCueDefinitions.size()> sound_initialized{};
    bool context_initialized = false;
    bool engine_initialized  = false;
    bool playback_locked     = false;
    std::mutex mutex;

    ~Impl()
    {
        stop();
    }

    bool start()
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (engine_initialized) {
            return true;
        }

#if defined(__linux__)
        ma_backend backends[]          = {ma_backend_pulseaudio};
        const ma_result context_result = ma_context_init(backends, 1, nullptr, &context);
#else
        const ma_result context_result = ma_context_init(nullptr, 0, nullptr, &context);
#endif
        if (context_result != MA_SUCCESS) {
            spdlog::warn("Keyboard Guide sound: audio context initialization failed: {}",
                         ma_result_description(context_result));
            return false;
        }
        context_initialized = true;

        ma_engine_config config       = ma_engine_config_init();
        config.pContext               = &context;
        config.channels               = 2;
        config.sampleRate             = 48000;
        const ma_result engine_result = ma_engine_init(&config, &engine);
        if (engine_result != MA_SUCCESS) {
            spdlog::warn("Keyboard Guide sound: audio engine initialization failed: {}",
                         ma_result_description(engine_result));
            cleanupLocked();
            return false;
        }
        engine_initialized = true;
        ma_engine_set_volume(&engine, kMasterVolume);

        const std::filesystem::path asset_directory = audioAssetDirectory();
        bool loaded_any                             = false;
        for (std::size_t index = 0; index < sounds.size(); ++index) {
            const std::filesystem::path path = asset_directory / kCueDefinitions[index].file_name;
            const ma_result result           = ma_sound_init_from_file(&engine, path.string().c_str(),
                                                                       MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_NO_SPATIALIZATION,
                                                                       nullptr, nullptr, &sounds[index]);
            if (result != MA_SUCCESS) {
                spdlog::warn("Keyboard Guide sound: failed to load '{}': {}", path.string(),
                             ma_result_description(result));
                continue;
            }
            ma_sound_set_volume(&sounds[index], kCueDefinitions[index].volume);
            sound_initialized[index] = true;
            loaded_any               = true;
        }

        if (!loaded_any) {
            cleanupLocked();
            return false;
        }

        spdlog::info("Keyboard Guide sound: loaded mechanical cues from '{}' using {}", asset_directory.string(),
                     ma_get_backend_name(context.backend));
        return true;
    }

    void stop()
    {
        std::lock_guard<std::mutex> lock(mutex);
        cleanupLocked();
    }

    void play(SoundCue cue)
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (playback_locked) {
            return;
        }
        playLocked(cue);
    }

    void playExclusive(SoundCue cue)
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (!playback_locked && playLocked(cue)) {
            playback_locked = true;
        }
    }

private:
    bool playLocked(SoundCue cue)
    {
        const std::size_t index = cueIndex(cue);
        if (!engine_initialized || index >= sounds.size() || !sound_initialized[index]) {
            return false;
        }

        for (std::size_t current = 0; current < sounds.size(); ++current) {
            if (sound_initialized[current] && ma_sound_is_playing(&sounds[current])) {
                ma_sound_stop(&sounds[current]);
            }
        }
        if (ma_sound_seek_to_pcm_frame(&sounds[index], 0) != MA_SUCCESS ||
            ma_sound_start(&sounds[index]) != MA_SUCCESS) {
            spdlog::warn("Keyboard Guide sound: failed to play '{}'", kCueDefinitions[index].file_name);
            return false;
        }
        return true;
    }

    void cleanupLocked()
    {
        for (std::size_t index = 0; index < sounds.size(); ++index) {
            if (sound_initialized[index]) {
                ma_sound_uninit(&sounds[index]);
                sound_initialized[index] = false;
            }
        }
        if (engine_initialized) {
            ma_engine_uninit(&engine);
            engine_initialized = false;
        }
        if (context_initialized) {
            ma_context_uninit(&context);
            context_initialized = false;
        }
        playback_locked = false;
    }
};

SoundService::SoundService() : _impl(std::make_unique<Impl>())
{
}

SoundService::~SoundService() = default;

bool SoundService::start()
{
    return _impl->start();
}

void SoundService::stop()
{
    _impl->stop();
}

void SoundService::play(SoundCue cue)
{
    _impl->play(cue);
}

void SoundService::playExclusive(SoundCue cue)
{
    _impl->playExclusive(cue);
}

}  // namespace keyboard_guide
