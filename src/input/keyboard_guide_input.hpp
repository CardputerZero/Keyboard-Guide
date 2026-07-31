#pragma once

#include "core/guide_types.hpp"

#include <functional>
#include <string>
#include <vector>

#if KEYBOARD_GUIDE_USE_SDL
#include <SDL.h>
#endif

namespace keyboard_guide {

class KeyboardGuideInput {
public:
    using EventCallback = std::function<void(const GuideInputEvent&)>;

    KeyboardGuideInput() = default;
    ~KeyboardGuideInput();

    KeyboardGuideInput(const KeyboardGuideInput&)            = delete;
    KeyboardGuideInput& operator=(const KeyboardGuideInput&) = delete;

    bool openDefault();
    bool openDevice(const std::string& path);
    void close();
    void poll();
    void setEventCallback(EventCallback callback);

private:
    EventCallback _callback;
    std::vector<int> _event_fds;

    void emit(GuideKey key, bool pressed, bool repeated, char character = '\0');

#if KEYBOARD_GUIDE_USE_SDL
    bool _sdl_watch_registered = false;
    static int sdlEventWatch(void* context, SDL_Event* event);
#endif
};

}  // namespace keyboard_guide
