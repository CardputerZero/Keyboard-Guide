#include "input/keyboard_guide_input.hpp"

#include <lvgl.h>
#include <spdlog/spdlog.h>

#include <cctype>
#include <cstdlib>
#include <iterator>
#include <utility>

#if !KEYBOARD_GUIDE_USE_SDL && defined(__linux__)
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <sys/ioctl.h>
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
    switch (code) {
        case 26:
        case 183:
            return '!';
        case 27:
        case 184:
            return '@';
        case 39:
        case 185:
            return '#';
        case KEY_COMMA:
            return ',';
        default:
            break;
    }

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

int rawFnDirectionIndex(uint16_t code)
{
    switch (code) {
        case KEY_F:
            return 0;
        case KEY_X:
            return 1;
        case KEY_Z:
            return 2;
        case KEY_C:
            return 3;
        default:
            return -1;
    }
}

GuideKey directionForIndex(size_t index)
{
    constexpr std::array<GuideKey, 4> kDirections = {
        GuideKey::Up,
        GuideKey::Down,
        GuideKey::Left,
        GuideKey::Right,
    };
    return index < kDirections.size() ? kDirections[index] : GuideKey::Unknown;
}

GuideKey directionForKeyCode(uint16_t code)
{
    switch (code) {
        case KEY_UP:
            return GuideKey::Up;
        case KEY_DOWN:
            return GuideKey::Down;
        case KEY_LEFT:
            return GuideKey::Left;
        case KEY_RIGHT:
            return GuideKey::Right;
        default:
            return GuideKey::Unknown;
    }
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

const char* configuredModifierI2cDevice()
{
    if (const char* value = std::getenv("KEYBOARD_GUIDE_MODIFIER_I2C_DEVICE"); value && value[0] != '\0') {
        return value;
    }
    return "/dev/i2c-1";
}

uint8_t configuredByte(const char* variable, uint8_t fallback, unsigned long maximum)
{
    const char* value = std::getenv(variable);
    if (!value || value[0] == '\0') {
        return fallback;
    }

    errno             = 0;
    char* end         = nullptr;
    const auto parsed = std::strtoul(value, &end, 0);
    if (errno != 0 || end == value || *end != '\0' || parsed > maximum) {
        spdlog::warn("Keyboard Guide modifier I2C: invalid {}='{}'; using 0x{:02X}", variable, value, fallback);
        return fallback;
    }
    return static_cast<uint8_t>(parsed);
}

const char* modifierName(size_t index)
{
    constexpr const char* kNames[] = {"SHIFT", "SYM", "FN"};
    return index < std::size(kNames) ? kNames[index] : "UNKNOWN";
}

const char* modifierModeName(GuideModifierMode mode)
{
    switch (mode) {
        case GuideModifierMode::Inactive:
            return "inactive";
        case GuideModifierMode::OneShot:
            return "one-shot";
        case GuideModifierMode::Locked:
            return "locked";
        case GuideModifierMode::Held:
            return "held";
    }
    return "unknown";
}

bool modifierModeForRaw(uint8_t raw, GuideModifierMode& mode)
{
    if (raw > static_cast<uint8_t>(GuideModifierMode::Held)) {
        return false;
    }
    mode = static_cast<GuideModifierMode>(raw);
    return true;
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
    bool keyboard_open = false;
    if (const char* device = configuredDevice()) {
        keyboard_open = openDevice(device);
    } else {
        keyboard_open = openDevice("/dev/input/by-path/platform-3f804000.i2c-event");
    }
    if (!openModifierI2c()) {
        spdlog::warn("Keyboard Guide modifier I2C: unavailable; using evdev modifier fallback");
    }
    return keyboard_open;
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
    if (_modifier_i2c_fd >= 0) {
        ::close(_modifier_i2c_fd);
        _modifier_i2c_fd = -1;
    }
    _modifier_modes.fill(GuideModifierMode::Inactive);
    _modifier_modes_known.fill(false);
    _pending_inactive.fill(false);
    _raw_fn_direction_down.fill(false);
    _modifier_last_poll_ms         = 0;
    _modifier_consecutive_failures = 0;
    _modifier_snapshot_seen        = false;
    _modifier_read_failed          = false;
#endif
    _event_fds.clear();
}

void KeyboardGuideInput::poll()
{
#if !KEYBOARD_GUIDE_USE_SDL && defined(__linux__)
    constexpr uint32_t kModifierPollIntervalMs = 25;
    const uint32_t now_ms                      = lv_tick_get();
    if (_modifier_i2c_fd >= 0 && now_ms - _modifier_last_poll_ms >= kModifierPollIntervalMs) {
        _modifier_last_poll_ms = now_ms;
        refreshModifierModes(true);
    }

    for (int fd : _event_fds) {
        while (true) {
            input_event event{};
            const ssize_t bytes_read = ::read(fd, &event, sizeof(event));
            if (bytes_read == sizeof(event)) {
                if (event.type == EV_MSC && event.code == MSC_SCAN && (event.value == 66 || event.value == 67)) {
                    if (modifierStateAvailable()) {
                        continue;
                    }
                    const GuideKey key = event.value == 66 ? GuideKey::Fn : GuideKey::Sym;
                    emit(key, true, false);
                    emit(key, false, false);
                    continue;
                }
                if (event.type != EV_KEY || (event.value != 0 && event.value != 1 && event.value != 2)) {
                    continue;
                }

                const int raw_fn_direction_index = rawFnDirectionIndex(event.code);
                const GuideKey direct_direction  = directionForKeyCode(event.code);
                const bool modifier_consumer =
                    direct_direction != GuideKey::Unknown || characterForKeyCode(event.code) != '\0';
                if (modifier_consumer && event.value == 1 && _modifier_i2c_fd >= 0) {
                    refreshModifierModes(true);
                }

                bool raw_fn_direction = false;
                if (raw_fn_direction_index >= 0) {
                    const size_t index = static_cast<size_t>(raw_fn_direction_index);
                    raw_fn_direction   = _raw_fn_direction_down[index] || (event.value != 0 && fnModifierActive());
                    if (event.value == 1 && raw_fn_direction) {
                        _raw_fn_direction_down[index] = true;
                    }
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
                    case KEY_UP:
                        key = GuideKey::Up;
                        break;
                    case KEY_DOWN:
                        key = GuideKey::Down;
                        break;
                    case KEY_LEFT:
                        key = GuideKey::Left;
                        break;
                    case KEY_RIGHT:
                        key = GuideKey::Right;
                        break;
                    case KEY_ESC:
                        key = GuideKey::Escape;
                        break;
                    case KEY_ENTER:
                    case KEY_KPENTER:
                        key = GuideKey::Enter;
                        break;
                    default:
                        if (raw_fn_direction) {
                            key = directionForIndex(static_cast<size_t>(raw_fn_direction_index));
                        } else {
                            character = characterForKeyCode(event.code);
                            key       = character == '\0' ? GuideKey::Unknown : GuideKey::Character;
                        }
                        break;
                }
                if (modifierStateAvailable() &&
                    (key == GuideKey::Shift || key == GuideKey::Sym || key == GuideKey::Fn)) {
                    continue;
                }
                if (key != GuideKey::Unknown) {
                    emit(key, event.value != 0, event.value == 2, character);
                }
                if (raw_fn_direction_index >= 0 && event.value == 0) {
                    _raw_fn_direction_down[static_cast<size_t>(raw_fn_direction_index)] = false;
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
    flushPendingModifierModes();
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

void KeyboardGuideInput::emitModifierMode(GuideKey key, GuideModifierMode mode)
{
    if (!_callback) {
        return;
    }

    GuideInputEvent event;
    event.key               = key;
    event.timestamp_ms      = lv_tick_get();
    event.has_modifier_mode = true;
    event.modifier_mode     = mode;
    _callback(event);
}

#if !KEYBOARD_GUIDE_USE_SDL && defined(__linux__)
bool KeyboardGuideInput::openModifierI2c()
{
    _modifier_registers = {
        configuredByte("KEYBOARD_GUIDE_SHIFT_REGISTER", 0xBD, 0xFF),
        configuredByte("KEYBOARD_GUIDE_SYM_REGISTER", 0xBE, 0xFF),
        configuredByte("KEYBOARD_GUIDE_FN_REGISTER", 0xBF, 0xFF),
    };
    const uint8_t address = configuredByte("KEYBOARD_GUIDE_MODIFIER_I2C_ADDRESS", 0x4F, 0x7F);
    const char* device    = configuredModifierI2cDevice();

    const int fd = ::open(device, O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        spdlog::warn("Keyboard Guide modifier I2C: failed to open {}: {}", device, std::strerror(errno));
        return false;
    }
    if (::ioctl(fd, I2C_SLAVE_FORCE, address) < 0) {
        const int saved_errno = errno;
        ::close(fd);
        spdlog::warn("Keyboard Guide modifier I2C: I2C_SLAVE_FORCE address 0x{:02X} failed on {}: {}", address, device,
                     std::strerror(saved_errno));
        return false;
    }

    _modifier_i2c_fd = fd;
    spdlog::info("Keyboard Guide modifier I2C: opened {}, address=0x{:02X} via I2C_SLAVE_FORCE", device, address);
    refreshModifierModes(false);
    _modifier_last_poll_ms = lv_tick_get();
    return true;
}

bool KeyboardGuideInput::refreshModifierModes(bool defer_inactive)
{
    if (_modifier_i2c_fd < 0) {
        return false;
    }

    std::array<uint8_t, kModifierCount> raw{};
    for (size_t index = 0; index < raw.size(); ++index) {
        if (!readModifierRegister(_modifier_registers[index], raw[index])) {
            if (!_modifier_read_failed) {
                spdlog::warn("Keyboard Guide modifier I2C: read {} register 0x{:02X} failed: {}", modifierName(index),
                             _modifier_registers[index], std::strerror(errno));
            }
            _modifier_read_failed = true;
            handleModifierReadFailure();
            return false;
        }
    }

    std::array<GuideModifierMode, kModifierCount> modes{};
    for (size_t index = 0; index < raw.size(); ++index) {
        if (!modifierModeForRaw(raw[index], modes[index])) {
            if (!_modifier_read_failed) {
                spdlog::warn("Keyboard Guide modifier I2C: {} register 0x{:02X} returned invalid mode 0x{:02X}",
                             modifierName(index), _modifier_registers[index], raw[index]);
            }
            _modifier_read_failed = true;
            handleModifierReadFailure();
            return false;
        }
    }

    if (_modifier_read_failed) {
        spdlog::info("Keyboard Guide modifier I2C: register reads recovered");
        _modifier_read_failed = false;
    }
    _modifier_consecutive_failures = 0;

    const bool first_snapshot = !_modifier_snapshot_seen;
    if (first_snapshot) {
        spdlog::info(
            "Keyboard Guide modifier registers: SHIFT[0x{:02X}]=0x{:02X}, SYM[0x{:02X}]=0x{:02X}, "
            "FN[0x{:02X}]=0x{:02X}",
            _modifier_registers[0], raw[0], _modifier_registers[1], raw[1], _modifier_registers[2], raw[2]);
        _modifier_snapshot_seen = true;
    }

    constexpr std::array<GuideKey, kModifierCount> kKeys = {GuideKey::Shift, GuideKey::Sym, GuideKey::Fn};
    for (size_t index = 0; index < modes.size(); ++index) {
        const GuideModifierMode mode = modes[index];
        if (_modifier_modes_known[index] && mode == _modifier_modes[index]) {
            if (mode != GuideModifierMode::Inactive) {
                _pending_inactive[index] = false;
            }
            continue;
        }
        if (defer_inactive && _modifier_modes_known[index] && mode == GuideModifierMode::Inactive) {
            _pending_inactive[index] = true;
            continue;
        }

        _pending_inactive[index]     = false;
        _modifier_modes[index]       = mode;
        _modifier_modes_known[index] = true;
        if (!first_snapshot) {
            spdlog::info("Keyboard Guide modifier I2C: {} -> {} (register 0x{:02X}=0x{:02X})", modifierName(index),
                         modifierModeName(mode), _modifier_registers[index], raw[index]);
        }
        emitModifierMode(kKeys[index], mode);
    }
    return true;
}

void KeyboardGuideInput::flushPendingModifierModes()
{
    constexpr std::array<GuideKey, kModifierCount> kKeys = {GuideKey::Shift, GuideKey::Sym, GuideKey::Fn};
    for (size_t index = 0; index < _pending_inactive.size(); ++index) {
        if (!_pending_inactive[index]) {
            continue;
        }
        _pending_inactive[index]     = false;
        _modifier_modes[index]       = GuideModifierMode::Inactive;
        _modifier_modes_known[index] = true;
        spdlog::info("Keyboard Guide modifier I2C: {} -> inactive (register 0x{:02X}=0x00)", modifierName(index),
                     _modifier_registers[index]);
        emitModifierMode(kKeys[index], GuideModifierMode::Inactive);
    }
}

void KeyboardGuideInput::handleModifierReadFailure()
{
    constexpr uint32_t kFailureThreshold = 4;
    if (_modifier_consecutive_failures < kFailureThreshold) {
        ++_modifier_consecutive_failures;
    }
    if (!_modifier_snapshot_seen || _modifier_consecutive_failures < kFailureThreshold) {
        return;
    }

    spdlog::warn(
        "Keyboard Guide modifier I2C: {} consecutive reads failed; clearing stale state and enabling evdev "
        "fallback",
        kFailureThreshold);
    _modifier_snapshot_seen = false;
    _pending_inactive.fill(false);

    constexpr std::array<GuideKey, kModifierCount> kKeys = {GuideKey::Shift, GuideKey::Sym, GuideKey::Fn};
    for (size_t index = 0; index < _modifier_modes.size(); ++index) {
        if (_modifier_modes_known[index] && _modifier_modes[index] != GuideModifierMode::Inactive) {
            emitModifierMode(kKeys[index], GuideModifierMode::Inactive);
        }
        _modifier_modes[index]       = GuideModifierMode::Inactive;
        _modifier_modes_known[index] = false;
    }
}

bool KeyboardGuideInput::readModifierRegister(uint8_t register_address, uint8_t& value) const
{
    i2c_smbus_data data{};
    i2c_smbus_ioctl_data request{};
    request.read_write = I2C_SMBUS_READ;
    request.command    = register_address;
    request.size       = I2C_SMBUS_BYTE_DATA;
    request.data       = &data;
    if (::ioctl(_modifier_i2c_fd, I2C_SMBUS, &request) < 0) {
        return false;
    }
    value = data.byte;
    return true;
}

bool KeyboardGuideInput::modifierStateAvailable() const
{
    return _modifier_snapshot_seen;
}

bool KeyboardGuideInput::fnModifierActive() const
{
    constexpr size_t kFnIndex = 2;
    return _modifier_modes_known[kFnIndex] &&
           (_modifier_modes[kFnIndex] != GuideModifierMode::Inactive || _pending_inactive[kFnIndex]);
}
#endif

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
    } else if (scancode == SDL_SCANCODE_F1) {
        input->emit(GuideKey::Sym, pressed, repeated);
    } else if (scancode == SDL_SCANCODE_F2) {
        input->emit(GuideKey::Fn, pressed, repeated);
    } else if (scancode == SDL_SCANCODE_UP) {
        input->emit(GuideKey::Up, pressed, repeated);
    } else if (scancode == SDL_SCANCODE_DOWN) {
        input->emit(GuideKey::Down, pressed, repeated);
    } else if (scancode == SDL_SCANCODE_LEFT) {
        input->emit(GuideKey::Left, pressed, repeated);
    } else if (scancode == SDL_SCANCODE_RIGHT) {
        input->emit(GuideKey::Right, pressed, repeated);
    } else if (scancode == SDL_SCANCODE_ESCAPE) {
        input->emit(GuideKey::Escape, pressed, repeated);
    } else if (scancode == SDL_SCANCODE_RETURN || scancode == SDL_SCANCODE_KP_ENTER) {
        input->emit(GuideKey::Enter, pressed, repeated);
    } else if (scancode >= SDL_SCANCODE_A && scancode <= SDL_SCANCODE_Z) {
        input->emit(GuideKey::Character, pressed, repeated, static_cast<char>('a' + scancode - SDL_SCANCODE_A));
    } else if (scancode >= SDL_SCANCODE_1 && scancode <= SDL_SCANCODE_9) {
        input->emit(GuideKey::Character, pressed, repeated, static_cast<char>('1' + scancode - SDL_SCANCODE_1));
    } else if (scancode == SDL_SCANCODE_0) {
        input->emit(GuideKey::Character, pressed, repeated, '0');
    } else if (scancode == SDL_SCANCODE_COMMA) {
        input->emit(GuideKey::Character, pressed, repeated, ',');
    } else if (scancode == SDL_SCANCODE_SPACE) {
        input->emit(GuideKey::Character, pressed, repeated, ' ');
    }
    return 1;
}
#endif

}  // namespace keyboard_guide
