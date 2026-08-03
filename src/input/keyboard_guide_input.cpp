#include "input/keyboard_guide_input.hpp"

#include <lvgl.h>
#include <spdlog/spdlog.h>

#include <cctype>
#include <cstdlib>
#include <utility>

#if !KEYBOARD_GUIDE_USE_SDL && defined(__linux__)
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <linux/input.h>
#include <unistd.h>

#ifndef KEY_FN
#define KEY_FN 0x1d0
#endif
#endif

namespace keyboard_guide {
namespace {

#if !KEYBOARD_GUIDE_USE_SDL && defined(__linux__)
char characterForKeyCode(uint16_t code)
{
    if (code >= KEY_A && code <= KEY_L) {
        constexpr char kRow[] = "asdfghjkl";
        return kRow[code - KEY_A];
    }
    if (code >= KEY_Q && code <= KEY_P) {
        constexpr char kRow[] = "qwertyuiop";
        return kRow[code - KEY_Q];
    }
    if (code >= KEY_Z && code <= KEY_M) {
        constexpr char kRow[] = "zxcvbnm";
        return kRow[code - KEY_Z];
    }
    if (code >= KEY_1 && code <= KEY_9) {
        return static_cast<char>('1' + code - KEY_1);
    }
    if (code == KEY_0) {
        return '0';
    }
    if (code == KEY_SPACE) {
        return ' ';
    }
    return '\0';
}
#endif

#if !KEYBOARD_GUIDE_USE_SDL && defined(__linux__)
const char* configuredDevice()
{
    constexpr const char* kVariables[] = {
        "KEYBOARD_GUIDE_KEYBOARD_DEVICE",
        "APPLAUNCH_LINUX_KEYBOARD_DEVICE",
        "LV_LINUX_KEYBOARD_DEVICE",
    };
    for (const char* variable : kVariables) {
        const char* value = std::getenv(variable);
        if (value && value[0] != '\0') {
            return value;
        }
    }
    return nullptr;
}
#endif

}  // namespace

KeyboardGuideInput::~KeyboardGuideInput()
{
    close();
}

bool KeyboardGuideInput::openDefault()
{
#if KEYBOARD_GUIDE_USE_SDL
    if (!_sdl_watch_registered) {
        SDL_AddEventWatch(sdlEventWatch, this);
        _sdl_watch_registered = true;
        spdlog::info("Keyboard Guide input: SDL event watch enabled");
    }
    return true;
#elif defined(__linux__)
    if (const char* device = configuredDevice()) {
        return openDevice(device);
    }
    return openDevice("/dev/input/by-path/platform-3f804000.i2c-event");
#else
    spdlog::warn("Keyboard Guide input: no input backend on this platform");
    return false;
#endif
}

bool KeyboardGuideInput::openDevice(const std::string& path)
{
#if !KEYBOARD_GUIDE_USE_SDL && defined(__linux__)
    const int fd = ::open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        spdlog::warn("Keyboard Guide input: failed to open {}: {}", path, std::strerror(errno));
        return false;
    }
    _event_fds.push_back(fd);
    spdlog::info("Keyboard Guide input: opened {} (shared, no EVIOCGRAB)", path);
    return true;
#else
    (void)path;
    return false;
#endif
}

void KeyboardGuideInput::close()
{
#if KEYBOARD_GUIDE_USE_SDL
    if (_sdl_watch_registered) {
        SDL_DelEventWatch(sdlEventWatch, this);
        _sdl_watch_registered = false;
    }
#elif defined(__linux__)
    for (int fd : _event_fds) {
        if (fd >= 0) {
            ::close(fd);
        }
    }
#endif
    _event_fds.clear();
}

void KeyboardGuideInput::poll()
{
#if !KEYBOARD_GUIDE_USE_SDL && defined(__linux__)
    for (int fd : _event_fds) {
        while (true) {
            input_event event{};
            const ssize_t bytes_read = ::read(fd, &event, sizeof(event));
            if (bytes_read == sizeof(event)) {
                if (event.type == EV_MSC && event.code == MSC_SCAN && (event.value == 66 || event.value == 67)) {
                    const GuideKey key = event.value == 66 ? GuideKey::Fn : GuideKey::Sym;
                    emit(key, true, false);
                    emit(key, false, false);
                    continue;
                }
                if (event.type != EV_KEY || (event.value != 0 && event.value != 1 && event.value != 2)) {
                    continue;
                }

                GuideKey key   = GuideKey::Unknown;
                char character = '\0';
                switch (event.code) {
                    case KEY_LEFTSHIFT:
                    case KEY_RIGHTSHIFT:
                        key = GuideKey::Shift;
                        break;
                    case KEY_COMPOSE:
                        key = GuideKey::Sym;
                        break;
                    case KEY_FN:
                        key = GuideKey::Fn;
                        break;
                    case KEY_ESC:
                        key = GuideKey::Escape;
                        break;
                    default:
                        character = characterForKeyCode(event.code);
                        key       = character == '\0' ? GuideKey::Unknown : GuideKey::Character;
                        break;
                }
                if (key != GuideKey::Unknown) {
                    emit(key, event.value != 0, event.value == 2, character);
                }
                continue;
            }

            if (bytes_read < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
                break;
            }
            if (bytes_read < 0) {
                spdlog::warn("Keyboard Guide input: read failed: {}", std::strerror(errno));
            }
            break;
        }
    }
#endif
}

void KeyboardGuideInput::setEventCallback(EventCallback callback)
{
    _callback = std::move(callback);
}

void KeyboardGuideInput::emit(GuideKey key, bool pressed, bool repeated, char character)
{
    if (!_callback) {
        return;
    }
    _callback({key, character, pressed, repeated, lv_tick_get()});
}

#if KEYBOARD_GUIDE_USE_SDL
int KeyboardGuideInput::sdlEventWatch(void* context, SDL_Event* event)
{
    auto* input = static_cast<KeyboardGuideInput*>(context);
    if (!input || !event || (event->type != SDL_KEYDOWN && event->type != SDL_KEYUP)) {
        return 1;
    }

    const bool pressed          = event->type == SDL_KEYDOWN;
    const bool repeated         = pressed && event->key.repeat != 0;
    const SDL_Scancode scancode = event->key.keysym.scancode;
    if (scancode == SDL_SCANCODE_LSHIFT || scancode == SDL_SCANCODE_RSHIFT) {
        input->emit(GuideKey::Shift, pressed, repeated);
    } else if (scancode == SDL_SCANCODE_ESCAPE) {
        input->emit(GuideKey::Escape, pressed, repeated);
    } else if (scancode >= SDL_SCANCODE_A && scancode <= SDL_SCANCODE_Z) {
        input->emit(GuideKey::Character, pressed, repeated, static_cast<char>('a' + scancode - SDL_SCANCODE_A));
    } else if (scancode >= SDL_SCANCODE_1 && scancode <= SDL_SCANCODE_9) {
        input->emit(GuideKey::Character, pressed, repeated, static_cast<char>('1' + scancode - SDL_SCANCODE_1));
    } else if (scancode == SDL_SCANCODE_0) {
        input->emit(GuideKey::Character, pressed, repeated, '0');
    } else if (scancode == SDL_SCANCODE_SPACE) {
        input->emit(GuideKey::Character, pressed, repeated, ' ');
    }
    return 1;
}
#endif

}  // namespace keyboard_guide
