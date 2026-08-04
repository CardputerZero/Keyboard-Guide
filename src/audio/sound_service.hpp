#pragma once

#include <memory>

namespace keyboard_guide {

enum class SoundCue {
    Typing,
    Lock,
    Unlock,
    Error,
    CharacterComplete,
    LessonComplete,
};

class SoundService {
public:
    SoundService();
    ~SoundService();

    SoundService(const SoundService&)            = delete;
    SoundService& operator=(const SoundService&) = delete;

    bool start();
    void stop();
    void play(SoundCue cue);
    void playExclusive(SoundCue cue);

private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

}  // namespace keyboard_guide
