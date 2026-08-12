#include "input/keyboard_guide_input.hpp"

#include "input/modifier_state_parser.hpp"

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
        case 90:
        case 231:
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

int rawFnKeyIndex(uint16_t code)
{
    switch (code) {
        case KEY_F:
            return 0;
        case KEY_X:
            return 1;
        case KEY_S:
            return 2;
        case KEY_D:
            return 3;
        default:
            return -1;
    }
}

GuideKey fnKeyForIndex(size_t index)
{
    constexpr std::array<GuideKey, 4> kFnKeys = {
        GuideKey::Up,
        GuideKey::Down,
        GuideKey::VolumeDown,
        GuideKey::VolumeUp,
    };
    return index < kFnKeys.size() ? kFnKeys[index] : GuideKey::Unknown;
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

GuideKey volumeKeyForKeyCode(uint16_t code)
{
    switch (code) {
        case KEY_VOLUMEDOWN:
            return GuideKey::VolumeDown;
        case KEY_VOLUMEUP:
            return GuideKey::VolumeUp;
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

const char* configuredModifierStatePath()
{
    if (const char* value = std::getenv("KEYBOARD_GUIDE_MODIFIER_STATE_PATH"); value && value[0] != '\0') {
        return value;
    }
    return "/sys/bus/i2c/devices/1-0034/modifier_state";
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
    if (!openModifierBackend()) {
        spdlog::warn("Keyboard Guide modifier state: sysfs and legacy I2C unavailable; using evdev fallback");
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
    closeModifierBackend();
    clearModifierCache(false);
    _raw_fn_key_down.fill(false);
    _modifier_last_poll_ms         = 0;
    _modifier_consecutive_failures = 0;
    _modifier_read_failed          = false;
#endif
    _event_fds.clear();
}

void KeyboardGuideInput::poll()
{
#if !KEYBOARD_GUIDE_USE_SDL && defined(__linux__)
    constexpr uint32_t kModifierPollIntervalMs = 25;
    const uint32_t now_ms                      = lv_tick_get();
    if (_modifier_backend != ModifierBackend::None && now_ms - _modifier_last_poll_ms >= kModifierPollIntervalMs) {
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

                const int raw_fn_key_index      = rawFnKeyIndex(event.code);
                const GuideKey direct_direction = directionForKeyCode(event.code);
                const GuideKey direct_volume    = volumeKeyForKeyCode(event.code);
                const bool modifier_consumer    = direct_direction != GuideKey::Unknown ||
                                               direct_volume != GuideKey::Unknown ||
                                               characterForKeyCode(event.code) != '\0';
                if (modifier_consumer && event.value == 1 && _modifier_backend != ModifierBackend::None) {
                    refreshModifierModes(true);
                }

                bool raw_fn_key = false;
                if (raw_fn_key_index >= 0) {
                    const size_t index = static_cast<size_t>(raw_fn_key_index);
                    raw_fn_key         = _raw_fn_key_down[index] || (event.value != 0 && fnModifierActive());
                    if (event.value == 1 && raw_fn_key) {
                        _raw_fn_key_down[index] = true;
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
                    case KEY_VOLUMEDOWN:
                        key = GuideKey::VolumeDown;
                        break;
                    case KEY_VOLUMEUP:
                        key = GuideKey::VolumeUp;
                        break;
                    case KEY_ESC:
                        key = GuideKey::Escape;
                        break;
                    case KEY_TAB:
                        key = GuideKey::Tab;
                        break;
                    case KEY_ENTER:
                    case KEY_KPENTER:
                        key = GuideKey::Enter;
                        break;
                    default:
                        if (raw_fn_key) {
                            key = fnKeyForIndex(static_cast<size_t>(raw_fn_key_index));
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
                if (raw_fn_key_index >= 0 && event.value == 0) {
                    _raw_fn_key_down[static_cast<size_t>(raw_fn_key_index)] = false;
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
bool KeyboardGuideInput::openModifierBackend()
{
    if (openModifierSysfs()) {
        return true;
    }
    return openModifierI2c();
}

bool KeyboardGuideInput::openModifierSysfs()
{
    const char* path = configuredModifierStatePath();
    const int fd     = ::open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        spdlog::info("Keyboard Guide modifier sysfs: {} unavailable: {}", path, std::strerror(errno));
        return false;
    }

    _modifier_fd                = fd;
    _modifier_backend           = ModifierBackend::Sysfs;
    const bool initial_snapshot = refreshModifierModes(false);
    if (!initial_snapshot) {
        spdlog::warn("Keyboard Guide modifier sysfs: initial snapshot unavailable; keeping {} open for retry", path);
    } else {
        spdlog::info("Keyboard Guide modifier sysfs: using {} (ABI version {})", path,
                     ModifierStateSnapshot::kSupportedVersion);
    }

    _modifier_last_poll_ms = lv_tick_get();
    return true;
}

bool KeyboardGuideInput::openModifierI2c(bool defer_inactive)
{
    _modifier_registers = {
        configuredByte("KEYBOARD_GUIDE_SHIFT_REGISTER", 0xBD, 0xFF),
        configuredByte("KEYBOARD_GUIDE_SYM_REGISTER", 0xBF, 0xFF),
        configuredByte("KEYBOARD_GUIDE_FN_REGISTER", 0xBE, 0xFF),
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

    _modifier_fd      = fd;
    _modifier_backend = ModifierBackend::LegacyI2c;
    spdlog::info("Keyboard Guide modifier I2C: opened {}, address=0x{:02X} via I2C_SLAVE_FORCE", device, address);
    if (!refreshModifierModes(defer_inactive)) {
        spdlog::warn("Keyboard Guide modifier I2C: initial register snapshot unavailable; keeping {} open for retry",
                     device);
    }
    _modifier_last_poll_ms = lv_tick_get();
    return true;
}

bool KeyboardGuideInput::refreshModifierModes(bool defer_inactive)
{
    if (_modifier_backend == ModifierBackend::None || _modifier_fd < 0) {
        return false;
    }

    std::array<uint8_t, kModifierCount> raw{};
    uint64_t sequence    = 0;
    uint8_t changed_mask = 0;
    uint8_t reason       = 0;
    if (_modifier_backend == ModifierBackend::Sysfs) {
        if (!readModifierSysfs(raw, sequence, changed_mask, reason)) {
            if (!_modifier_read_failed) {
                spdlog::warn("Keyboard Guide modifier sysfs: snapshot read failed");
            }
            _modifier_read_failed = true;
            handleModifierReadFailure(defer_inactive);
            return false;
        }
    } else {
        for (size_t index = 0; index < raw.size(); ++index) {
            if (!readModifierRegister(_modifier_registers[index], raw[index])) {
                if (!_modifier_read_failed) {
                    spdlog::warn("Keyboard Guide modifier I2C: read {} register 0x{:02X} failed: {}",
                                 modifierName(index), _modifier_registers[index], std::strerror(errno));
                }
                _modifier_read_failed = true;
                handleModifierReadFailure(defer_inactive);
                return false;
            }
        }
    }

    std::array<GuideModifierMode, kModifierCount> modes{};
    for (size_t index = 0; index < raw.size(); ++index) {
        const bool valid = _modifier_backend == ModifierBackend::Sysfs
                               ? modifierModeForSysfsRaw(raw[index], modes[index])
                               : modifierModeForLegacyRaw(raw[index], modes[index]);
        if (!valid) {
            if (!_modifier_read_failed) {
                spdlog::warn("Keyboard Guide modifier state: {} returned invalid mode {}", modifierName(index),
                             raw[index]);
            }
            _modifier_read_failed = true;
            handleModifierReadFailure(defer_inactive);
            return false;
        }
    }

    if (_modifier_read_failed) {
        spdlog::info("Keyboard Guide modifier state: reads recovered");
        _modifier_read_failed = false;
    }
    _modifier_consecutive_failures = 0;

    const bool first_snapshot = !_modifier_snapshot_seen;
    if (first_snapshot) {
        if (_modifier_backend == ModifierBackend::Sysfs) {
            spdlog::info(
                "Keyboard Guide modifier sysfs: sequence={}, changed_mask=0x{:02X}, reason={}, SHIFT={}, SYM={}, "
                "FN={}",
                sequence, changed_mask, reason, raw[0], raw[1], raw[2]);
        } else {
            spdlog::info(
                "Keyboard Guide modifier registers: SHIFT[0x{:02X}]=0x{:02X}, SYM[0x{:02X}]=0x{:02X}, "
                "FN[0x{:02X}]=0x{:02X}",
                _modifier_registers[0], raw[0], _modifier_registers[1], raw[1], _modifier_registers[2], raw[2]);
        }
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
            if (_modifier_backend == ModifierBackend::Sysfs) {
                spdlog::info("Keyboard Guide modifier sysfs: {} -> {} (raw={}, sequence={}, reason={})",
                             modifierName(index), modifierModeName(mode), raw[index], sequence, reason);
            } else {
                spdlog::info("Keyboard Guide modifier I2C: {} -> {} (register 0x{:02X}=0x{:02X})", modifierName(index),
                             modifierModeName(mode), _modifier_registers[index], raw[index]);
            }
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
        spdlog::info("Keyboard Guide modifier state: {} -> inactive", modifierName(index));
        emitModifierMode(kKeys[index], GuideModifierMode::Inactive);
    }
}

void KeyboardGuideInput::handleModifierReadFailure(bool defer_inactive)
{
    constexpr uint32_t kFailureThreshold = 4;
    if (_modifier_consecutive_failures < kFailureThreshold) {
        ++_modifier_consecutive_failures;
    }
    if (_modifier_consecutive_failures < kFailureThreshold ||
        (_modifier_backend == ModifierBackend::LegacyI2c && !_modifier_snapshot_seen)) {
        return;
    }

    const ModifierBackend failed_backend = _modifier_backend;
    spdlog::warn("Keyboard Guide modifier state: {} consecutive reads failed; disabling current backend",
                 kFailureThreshold);
    closeModifierBackend();
    _modifier_consecutive_failures = 0;
    _modifier_read_failed          = false;

    if (failed_backend == ModifierBackend::Sysfs && openModifierI2c(defer_inactive)) {
        spdlog::warn("Keyboard Guide modifier state: switched from sysfs to legacy I2C");
    } else {
        clearModifierCache(true);
        spdlog::warn("Keyboard Guide modifier state: using evdev fallback");
    }
}

void KeyboardGuideInput::closeModifierBackend()
{
    if (_modifier_fd >= 0) {
        ::close(_modifier_fd);
        _modifier_fd = -1;
    }
    _modifier_backend = ModifierBackend::None;
}

void KeyboardGuideInput::clearModifierCache(bool emit_inactive)
{
    constexpr std::array<GuideKey, kModifierCount> kKeys = {GuideKey::Shift, GuideKey::Sym, GuideKey::Fn};
    for (size_t index = 0; index < _modifier_modes.size(); ++index) {
        if (emit_inactive && _modifier_modes_known[index] && _modifier_modes[index] != GuideModifierMode::Inactive) {
            emitModifierMode(kKeys[index], GuideModifierMode::Inactive);
        }
        _modifier_modes[index]       = GuideModifierMode::Inactive;
        _modifier_modes_known[index] = false;
    }
    _pending_inactive.fill(false);
    _modifier_snapshot_seen = false;
}

bool KeyboardGuideInput::readModifierSysfs(std::array<uint8_t, kModifierCount>& raw, uint64_t& sequence,
                                           uint8_t& changed_mask, uint8_t& reason)
{
    std::array<char, 512> buffer{};
    const ssize_t bytes_read = ::pread(_modifier_fd, buffer.data(), buffer.size() - 1, 0);
    if (bytes_read <= 0 || static_cast<size_t>(bytes_read) == buffer.size() - 1) {
        return false;
    }

    ModifierStateSnapshot snapshot;
    std::string error;
    if (!parseModifierStateSnapshot(std::string_view(buffer.data(), static_cast<size_t>(bytes_read)), snapshot,
                                    error)) {
        if (!_modifier_read_failed) {
            spdlog::warn("Keyboard Guide modifier sysfs: invalid snapshot: {}", error);
        }
        return false;
    }

    raw          = snapshot.raw_modes;
    sequence     = snapshot.sequence;
    changed_mask = snapshot.has_changed_mask ? snapshot.changed_mask : 0;
    reason       = snapshot.has_reason ? snapshot.reason : 0;
    return true;
}

bool KeyboardGuideInput::readModifierRegister(uint8_t register_address, uint8_t& value) const
{
    i2c_smbus_data data{};
    i2c_smbus_ioctl_data request{};
    request.read_write = I2C_SMBUS_READ;
    request.command    = register_address;
    request.size       = I2C_SMBUS_BYTE_DATA;
    request.data       = &data;
    if (::ioctl(_modifier_fd, I2C_SMBUS, &request) < 0) {
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
        input->emit(GuideKey::VolumeDown, pressed, repeated);
    } else if (scancode == SDL_SCANCODE_RIGHT) {
        input->emit(GuideKey::VolumeUp, pressed, repeated);
    } else if (scancode == SDL_SCANCODE_ESCAPE) {
        input->emit(GuideKey::Escape, pressed, repeated);
    } else if (scancode == SDL_SCANCODE_TAB) {
        input->emit(GuideKey::Tab, pressed, repeated);
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
