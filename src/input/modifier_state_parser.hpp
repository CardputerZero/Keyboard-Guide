#pragma once

#include "core/guide_types.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace keyboard_guide {

struct ModifierStateSnapshot {
    static constexpr uint32_t kSupportedVersion = 1;

    uint32_t version      = 0;
    uint64_t sequence     = 0;
    uint8_t changed_mask  = 0;
    uint8_t reason        = 0;
    bool has_changed_mask = false;
    bool has_reason       = false;

    // Internal ordering is Shift, Sym, Fn.
    std::array<uint8_t, 3> raw_modes{};
};

bool parseModifierStateSnapshot(std::string_view text, ModifierStateSnapshot& snapshot, std::string& error);
bool modifierModeForSysfsRaw(uint8_t raw, GuideModifierMode& mode);
bool modifierModeForLegacyRaw(uint8_t raw, GuideModifierMode& mode);

}  // namespace keyboard_guide
