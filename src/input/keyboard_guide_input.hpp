#pragma once

#include "core/guide_types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
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
    static constexpr size_t kModifierCount = 3;

    EventCallback _callback;
    std::vector<int> _event_fds;

    void emit(GuideKey key, bool pressed, bool repeated, char character = '\0');
    void emitModifierMode(GuideKey key, GuideModifierMode mode);

#if !KEYBOARD_GUIDE_USE_SDL && defined(__linux__)
    int _modifier_i2c_fd = -1;
    std::array<uint8_t, kModifierCount> _modifier_registers{};
    std::array<GuideModifierMode, kModifierCount> _modifier_modes{};
    std::array<bool, kModifierCount> _modifier_modes_known{};
    std::array<bool, kModifierCount> _pending_inactive{};
    uint32_t _modifier_last_poll_ms         = 0;
    uint32_t _modifier_consecutive_failures = 0;
    bool _modifier_snapshot_seen            = false;
    bool _modifier_read_failed              = false;

    bool openModifierI2c();
    bool refreshModifierModes(bool defer_inactive);
    void flushPendingModifierModes();
    void handleModifierReadFailure();
    bool readModifierRegister(uint8_t register_address, uint8_t& value) const;
    bool modifierStateAvailable() const;
#endif

#if KEYBOARD_GUIDE_USE_SDL
    bool _sdl_watch_registered = false;
    static int sdlEventWatch(void* context, SDL_Event* event);
#endif
};

}  // namespace keyboard_guide
