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
    Up,
    Down,
    VolumeDown,
    VolumeUp,
    Left,
    Right,
    Tab,
    Enter,
    Escape,
};

enum class GuideModifierMode {
    Inactive,
    OneShot,
    Locked,
    Held,
};

enum class GuidePhase {
    Intro,
    PlainAwaitLetter,
    HoldAwaitShift,
    HoldAwaitLetter,
    OneShotAwaitShift,
    OneShotAwaitLetter,
    LockAwaitFirstTap,
    LockAwaitSecondTap,
    LockAwaitLetter,
    LockAwaitUnlock,
    ModifierOverview,
    SymAwaitSymbol,
    FnAwaitKey,
    TextAwaitCharacter,
    SuccessHold,
    Done,
};

enum class GuideTarget {
    None,
    Shift,
    A,
    B,
    C,
    Bang,
    At,
    Hash,
    Fn,
    Up,
    Down,
    VolumeDown,
    VolumeUp,
};

struct GuideInputEvent {
    GuideKey key                    = GuideKey::Unknown;
    char character                  = '\0';
    bool pressed                    = false;
    bool repeated                   = false;
    uint32_t timestamp_ms           = 0;
    bool has_modifier_mode          = false;
    GuideModifierMode modifier_mode = GuideModifierMode::Inactive;
};

struct GuideLessonState {
    int exercise_index = 0;
    std::string typed_text;
    std::string prompt          = "Press \"A\" to type lowercase \"a\".";
    GuidePhase phase            = GuidePhase::PlainAwaitLetter;
    GuideTarget cursor_target   = GuideTarget::A;
    bool shift_pressed          = false;
    bool shift_locked           = false;
    bool sym_pressed            = false;
    bool sym_one_shot           = false;
    bool sym_locked             = false;
    bool fn_pressed             = false;
    bool fn_one_shot            = false;
    bool fn_locked              = false;
    char character_pressed      = '\0';
    bool last_action_error      = false;
    uint32_t attention_revision = 0;
    uint32_t result_revision    = 0;
};

}  // namespace keyboard_guide
