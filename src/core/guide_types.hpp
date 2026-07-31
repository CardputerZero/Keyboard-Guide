#pragma once

#include <cstdint>
#include <string>

namespace keyboard_guide {

enum class GuideKey {
    Unknown,
    Character,
    Shift,
    Sym,
    Fn,
    Escape,
};

struct GuideInputEvent {
    GuideKey key          = GuideKey::Unknown;
    char character        = '\0';
    bool pressed          = false;
    bool repeated         = false;
    uint32_t timestamp_ms = 0;
};

struct GuideLessonState {
    int step_index = 0;
    std::string typed_text;
    std::string prompt          = "Tap SHIFT once";
    bool modifier_pressed       = false;
    bool completed              = false;
    bool last_action_error      = false;
    uint32_t attention_revision = 0;
};

}  // namespace keyboard_guide
