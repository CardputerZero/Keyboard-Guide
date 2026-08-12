#include "input/modifier_state_parser.hpp"

#include <charconv>
#include <cctype>
#include <limits>

namespace keyboard_guide {
namespace {

enum RequiredField : uint8_t {
    VersionField  = 1U << 0U,
    SequenceField = 1U << 1U,
    SymField      = 1U << 2U,
    ShiftField    = 1U << 3U,
    FnField       = 1U << 4U,
};

constexpr uint8_t kAllRequiredFields = VersionField | SequenceField | SymField | ShiftField | FnField;

template <typename Unsigned>
bool parseUnsigned(std::string_view text, Unsigned& value)
{
    if (text.empty()) {
        return false;
    }

    int base = 10;
    if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        text.remove_prefix(2);
        base = 16;
    }
    if (text.empty()) {
        return false;
    }

    Unsigned parsed   = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), parsed, base);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size()) {
        return false;
    }
    value = parsed;
    return true;
}

template <typename Unsigned>
bool assignRequiredField(std::string_view name, std::string_view value, uint8_t field, uint8_t& fields_seen,
                         Unsigned& destination, std::string& error)
{
    if ((fields_seen & field) != 0) {
        error = "duplicate field: " + std::string(name);
        return false;
    }
    if (!parseUnsigned(value, destination)) {
        error = "invalid value for " + std::string(name);
        return false;
    }
    fields_seen |= field;
    return true;
}

bool assignRawMode(std::string_view name, std::string_view value, uint8_t field, uint8_t& fields_seen,
                   uint8_t& destination, std::string& error)
{
    uint32_t parsed = 0;
    if (!assignRequiredField(name, value, field, fields_seen, parsed, error)) {
        return false;
    }
    if (parsed > 4) {
        error = "modifier mode out of range for " + std::string(name);
        return false;
    }
    destination = static_cast<uint8_t>(parsed);
    return true;
}

}  // namespace

bool parseModifierStateSnapshot(std::string_view text, ModifierStateSnapshot& snapshot, std::string& error)
{
    ModifierStateSnapshot parsed;
    uint8_t fields_seen = 0;
    error.clear();

    std::size_t position = 0;
    while (position < text.size()) {
        while (position < text.size() && std::isspace(static_cast<unsigned char>(text[position])) != 0) {
            ++position;
        }
        if (position == text.size()) {
            break;
        }

        const std::size_t token_begin = position;
        while (position < text.size() && std::isspace(static_cast<unsigned char>(text[position])) == 0) {
            ++position;
        }
        const std::string_view token = text.substr(token_begin, position - token_begin);
        const std::size_t equals     = token.find('=');
        if (equals == std::string_view::npos || equals == 0) {
            error = "malformed token: " + std::string(token);
            return false;
        }

        const std::string_view name  = token.substr(0, equals);
        const std::string_view value = token.substr(equals + 1);
        if (name == "version") {
            if (!assignRequiredField(name, value, VersionField, fields_seen, parsed.version, error)) {
                return false;
            }
        } else if (name == "sequence") {
            if (!assignRequiredField(name, value, SequenceField, fields_seen, parsed.sequence, error)) {
                return false;
            }
        } else if (name == "shift") {
            if (!assignRawMode(name, value, ShiftField, fields_seen, parsed.raw_modes[0], error)) {
                return false;
            }
        } else if (name == "sym") {
            if (!assignRawMode(name, value, SymField, fields_seen, parsed.raw_modes[1], error)) {
                return false;
            }
        } else if (name == "fn") {
            if (!assignRawMode(name, value, FnField, fields_seen, parsed.raw_modes[2], error)) {
                return false;
            }
        } else if (name == "changed_mask") {
            uint32_t value_parsed = 0;
            if (!parseUnsigned(value, value_parsed) || value_parsed > std::numeric_limits<uint8_t>::max()) {
                error = "invalid value for changed_mask";
                return false;
            }
            parsed.changed_mask     = static_cast<uint8_t>(value_parsed);
            parsed.has_changed_mask = true;
        } else if (name == "reason") {
            uint32_t value_parsed = 0;
            if (!parseUnsigned(value, value_parsed) || value_parsed > std::numeric_limits<uint8_t>::max()) {
                error = "invalid value for reason";
                return false;
            }
            parsed.reason     = static_cast<uint8_t>(value_parsed);
            parsed.has_reason = true;
        }
    }

    if ((fields_seen & kAllRequiredFields) != kAllRequiredFields) {
        error = "missing required modifier state field";
        return false;
    }
    if (parsed.version != ModifierStateSnapshot::kSupportedVersion) {
        error = "unsupported modifier state ABI version " + std::to_string(parsed.version);
        return false;
    }

    snapshot = parsed;
    return true;
}

bool modifierModeForSysfsRaw(uint8_t raw, GuideModifierMode& mode)
{
    switch (raw) {
        case 0:
            mode = GuideModifierMode::Inactive;
            return true;
        case 1:  // PRESSED
        case 4:  // HELD
            mode = GuideModifierMode::Held;
            return true;
        case 2:
            mode = GuideModifierMode::OneShot;
            return true;
        case 3:
            mode = GuideModifierMode::Locked;
            return true;
        default:
            return false;
    }
}

bool modifierModeForLegacyRaw(uint8_t raw, GuideModifierMode& mode)
{
    switch (raw) {
        case 0:
            mode = GuideModifierMode::Inactive;
            return true;
        case 1:
            mode = GuideModifierMode::OneShot;
            return true;
        case 2:
            mode = GuideModifierMode::Locked;
            return true;
        case 3:
            mode = GuideModifierMode::Held;
            return true;
        default:
            return false;
    }
}

}  // namespace keyboard_guide
