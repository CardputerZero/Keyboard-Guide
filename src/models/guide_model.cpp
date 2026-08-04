#include "models/guide_model.hpp"

#include <cctype>
#include <string>
#include <utility>

namespace keyboard_guide {
namespace {

constexpr char kFinalText[] = "Hi,M5Stack!";

bool isSuccessPhase(GuidePhase phase)
{
    return phase == GuidePhase::SuccessHold || phase == GuidePhase::Done;
}

std::string oneShotPrompt(char character)
{
    const std::string target = std::string("\"") + character + '"';
    return "Tap shift once, then press " + target + ".";
}

std::string oneShotArmedPrompt(char character)
{
    const std::string target = std::string("\"") + character + '"';
    return "One-shot is ready. Press " + target + ".";
}

std::string lockedPrompt(char character)
{
    const std::string target = std::string("\"") + character + '"';
    return "shift is locked. Press " + target + ".";
}

std::string symPrompt(char character)
{
    const std::string target = std::string("\"") + character + '"';
    return "Use sym any way you like.\nType " + target + " next.";
}

std::string fnPrompt(GuideKey key)
{
    const char* target = "Up";
    switch (key) {
        case GuideKey::Down:
            target = "Down";
            break;
        case GuideKey::Left:
            target = "Left";
            break;
        case GuideKey::Right:
            target = "Right";
            break;
        default:
            break;
    }
    return std::string("Use fn any way you like.\nPress \"") + target + "\" next.";
}

std::string finalTextPrompt()
{
    return "Try \"Hi,M5Stack!\"";
}

char functionKeyMarker(GuideKey key)
{
    switch (key) {
        case GuideKey::Down:
            return 'D';
        case GuideKey::Left:
            return 'L';
        case GuideKey::Right:
            return 'R';
        default:
            return 'U';
    }
}

}  // namespace

smooth_ui_toolkit::SingleObservable<GuideLessonState>& GuideModel::state()
{
    return _state;
}

void GuideModel::handleInput(const GuideInputEvent& event)
{
    if (event.has_modifier_mode) {
        switch (event.key) {
            case GuideKey::Shift:
                handleShiftMode(event.modifier_mode);
                break;
            case GuideKey::Sym:
                handleSymMode(event.modifier_mode);
                break;
            case GuideKey::Fn:
                handleFnMode(event.modifier_mode);
                break;
            default:
                break;
        }
        return;
    }

    if (event.repeated) {
        return;
    }

    switch (event.key) {
        case GuideKey::Shift:
            if (_state.get().exercise_index < 4 || _state.get().exercise_index == 7) {
                handleShift(event.pressed, event.timestamp_ms);
            }
            break;
        case GuideKey::Sym:
            if (_state.get().exercise_index == 5 || _state.get().exercise_index == 7) {
                handleSym(event.pressed, event.timestamp_ms);
            }
            break;
        case GuideKey::Fn:
            if (_state.get().exercise_index == 6) {
                handleFn(event.pressed, event.timestamp_ms);
            }
            break;
        case GuideKey::Up:
        case GuideKey::Down:
        case GuideKey::Left:
        case GuideKey::Right:
            if (_state.get().exercise_index == 6) {
                handleFunctionKey(event);
            }
            break;
        case GuideKey::Character:
            handleCharacter(event);
            break;
        default:
            break;
    }
}

void GuideModel::tick(uint32_t now_ms)
{
    const GuideLessonState& current = _state.get();

    if (current.phase == GuidePhase::LockAwaitSecondTap && !_shift_down &&
        now_ms - _first_tap_released_at > kDoubleTapWindowMs) {
        GuideLessonState next  = current;
        next.phase             = GuidePhase::LockAwaitFirstTap;
        next.prompt            = "Double-tap shift a little faster.";
        _first_tap_released_at = 0;
        publish(std::move(next), false);
        return;
    }

    if (current.phase != GuidePhase::SuccessHold) {
        return;
    }
    if (!inputReleased()) {
        _success_started_at = 0;
        return;
    }
    if (_success_started_at == 0) {
        _success_started_at = now_ms;
        return;
    }
    if (now_ms - _success_started_at >= kSuccessHoldMs) {
        advanceExercise();
    }
}

void GuideModel::reset()
{
    _pressed_characters.fill(false);
    clearModifierState();
    _success_started_at      = 0;
    _pressed_character_count = 0;
    _state.set(GuideLessonState{});
}

bool GuideModel::previousExercise()
{
    const int exercise_index = _state.get().exercise_index;
    if (exercise_index <= 0) {
        return false;
    }
    navigateToExercise(exercise_index - 1);
    return true;
}

bool GuideModel::nextExercise()
{
    const int exercise_index = _state.get().exercise_index;
    if (exercise_index >= kExerciseCount - 1) {
        return false;
    }
    navigateToExercise(exercise_index + 1);
    return true;
}

void GuideModel::handleShift(bool pressed, uint32_t timestamp_ms)
{
    if (pressed) {
        if (_shift_down) {
            return;
        }
        handleShiftPressed(_state.get(), timestamp_ms);
        return;
    }

    if (!_shift_down) {
        return;
    }
    handleShiftReleased(_state.get(), timestamp_ms);
}

void GuideModel::handleShiftMode(GuideModifierMode mode)
{
    if (_shift_mode == mode) {
        return;
    }

    _shift_mode            = mode;
    _shift_down            = mode == GuideModifierMode::Held;
    _shift_tap_invalid     = false;
    _shift_pressed_at      = 0;
    _first_tap_released_at = 0;

    GuideLessonState next = _state.get();
    next.shift_pressed    = mode == GuideModifierMode::Held;
    next.shift_locked     = mode == GuideModifierMode::Locked;

    if (isSuccessPhase(next.phase)) {
        publish(std::move(next), false);
        return;
    }

    switch (next.exercise_index) {
        case 0:
            if (mode == GuideModifierMode::Inactive) {
                next.prompt = "Press \"A\" to type lowercase \"a\".";
                publish(std::move(next), false);
            } else {
                next.prompt = "Turn shift off, then press \"A\".";
                publish(std::move(next), true);
            }
            return;

        case 1:
            if (mode == GuideModifierMode::Held) {
                next.phase         = GuidePhase::HoldAwaitLetter;
                next.cursor_target = GuideTarget::A;
                next.prompt        = "Keep holding shift and press \"A\".";
                publish(std::move(next), false);
            } else {
                next.phase         = GuidePhase::HoldAwaitShift;
                next.cursor_target = GuideTarget::Shift;
                next.prompt        = "Hold shift while pressing \"A\".\nThis types uppercase \"A\".";
                publish(std::move(next), mode != GuideModifierMode::Inactive);
            }
            return;

        case 2:
            if (mode == GuideModifierMode::OneShot) {
                const char target  = expectedSequenceCharacter(next);
                next.phase         = GuidePhase::OneShotAwaitLetter;
                next.cursor_target = targetForCharacter(target);
                next.prompt        = oneShotArmedPrompt(target);
                publish(std::move(next), false);
            } else if (mode == GuideModifierMode::Inactive && next.phase == GuidePhase::OneShotAwaitLetter) {
                next.phase         = GuidePhase::OneShotAwaitShift;
                next.cursor_target = GuideTarget::Shift;
                next.prompt        = oneShotPrompt(expectedSequenceCharacter(next));
                publish(std::move(next), false);
            } else {
                publish(std::move(next), false);
            }
            return;

        case 3:
            if (mode == GuideModifierMode::Locked) {
                if (next.phase != GuidePhase::LockAwaitUnlock) {
                    const char target  = expectedSequenceCharacter(next);
                    next.phase         = GuidePhase::LockAwaitLetter;
                    next.cursor_target = targetForCharacter(target);
                    next.prompt        = lockedPrompt(target);
                }
                publish(std::move(next), false);
            } else if (mode == GuideModifierMode::Inactive && next.phase == GuidePhase::LockAwaitUnlock) {
                next.cursor_target = GuideTarget::Shift;
                completeExercise(std::move(next), "Unlocked. shift guide complete!");
            } else if (mode == GuideModifierMode::Inactive && next.phase == GuidePhase::LockAwaitLetter) {
                next.phase         = GuidePhase::LockAwaitFirstTap;
                next.cursor_target = GuideTarget::Shift;
                next.prompt        = "shift unlocked. Double-tap it again.";
                publish(std::move(next), true);
            } else {
                publish(std::move(next), false);
            }
            return;

        default:
            publish(std::move(next), false);
            return;
    }
}

void GuideModel::handleShiftPressed(GuideLessonState next, uint32_t timestamp_ms)
{
    _shift_down        = true;
    _shift_mode        = GuideModifierMode::Held;
    _shift_tap_invalid = false;
    _shift_pressed_at  = timestamp_ms;
    next.shift_pressed = true;

    switch (next.phase) {
        case GuidePhase::PlainAwaitLetter:
            next.prompt = "Release shift, then press \"A\".";
            publish(std::move(next), true);
            return;

        case GuidePhase::HoldAwaitShift:
            next.phase         = GuidePhase::HoldAwaitLetter;
            next.cursor_target = GuideTarget::A;
            next.prompt        = "Keep holding shift and press \"A\".";
            publish(std::move(next), false);
            return;

        case GuidePhase::OneShotAwaitShift:
            publish(std::move(next), false);
            return;

        case GuidePhase::OneShotAwaitLetter:
            _shift_tap_invalid = true;
            publish(std::move(next), true);
            return;

        case GuidePhase::LockAwaitFirstTap:
            publish(std::move(next), false);
            return;

        case GuidePhase::LockAwaitSecondTap:
            if (timestamp_ms - _first_tap_released_at > kDoubleTapWindowMs) {
                next.phase             = GuidePhase::LockAwaitFirstTap;
                next.prompt            = "Double-tap shift a little faster.";
                _first_tap_released_at = 0;
            }
            publish(std::move(next), false);
            return;

        case GuidePhase::LockAwaitLetter:
            next.prompt = "Release shift; it is already locked.";
            publish(std::move(next), true);
            return;

        case GuidePhase::LockAwaitUnlock:
            publish(std::move(next), false);
            return;

        case GuidePhase::Intro:
        case GuidePhase::HoldAwaitLetter:
        case GuidePhase::ModifierOverview:
        case GuidePhase::SymAwaitSymbol:
        case GuidePhase::FnAwaitKey:
        case GuidePhase::TextAwaitCharacter:
        case GuidePhase::SuccessHold:
        case GuidePhase::Done:
            publish(std::move(next), false);
            return;
    }
}

void GuideModel::handleShiftReleased(GuideLessonState next, uint32_t timestamp_ms)
{
    _shift_down             = false;
    _shift_mode             = GuideModifierMode::Inactive;
    next.shift_pressed      = false;
    const uint32_t held_for = timestamp_ms - _shift_pressed_at;
    const bool valid_tap    = !_shift_tap_invalid && held_for <= kMaximumTapDurationMs;
    _shift_tap_invalid      = false;

    switch (next.phase) {
        case GuidePhase::PlainAwaitLetter:
            next.prompt = "Press \"A\" to type lowercase \"a\".";
            publish(std::move(next), false);
            return;

        case GuidePhase::HoldAwaitLetter:
            next.phase         = GuidePhase::HoldAwaitShift;
            next.cursor_target = GuideTarget::Shift;
            next.prompt        = "Keep shift held while pressing \"A\".";
            publish(std::move(next), true);
            return;

        case GuidePhase::OneShotAwaitShift: {
            const char target = expectedSequenceCharacter(next);
            if (valid_tap) {
                _shift_mode        = GuideModifierMode::OneShot;
                next.phase         = GuidePhase::OneShotAwaitLetter;
                next.cursor_target = targetForCharacter(target);
                next.prompt        = oneShotArmedPrompt(target);
                publish(std::move(next), false);
            } else {
                next.prompt = std::string("Tap shift quickly, then press \"") + target + "\".";
                publish(std::move(next), true);
            }
            return;
        }

        case GuidePhase::OneShotAwaitLetter:
            next.phase         = GuidePhase::OneShotAwaitShift;
            next.cursor_target = GuideTarget::Shift;
            next.prompt        = oneShotPrompt(expectedSequenceCharacter(next));
            publish(std::move(next), true);
            return;

        case GuidePhase::LockAwaitFirstTap:
            if (valid_tap) {
                _shift_mode            = GuideModifierMode::OneShot;
                next.phase             = GuidePhase::LockAwaitSecondTap;
                _first_tap_released_at = timestamp_ms;
                publish(std::move(next), false);
            } else {
                next.prompt = "Double-tap shift a little faster.";
                publish(std::move(next), true);
            }
            return;

        case GuidePhase::LockAwaitSecondTap: {
            const bool inside_window = _shift_pressed_at - _first_tap_released_at <= kDoubleTapWindowMs;
            if (valid_tap && inside_window) {
                _shift_mode            = GuideModifierMode::Locked;
                const char target      = expectedSequenceCharacter(next);
                next.phase             = GuidePhase::LockAwaitLetter;
                next.shift_locked      = true;
                next.cursor_target     = targetForCharacter(target);
                next.prompt            = lockedPrompt(target);
                _first_tap_released_at = 0;
                publish(std::move(next), false);
            } else if (valid_tap) {
                _shift_mode            = GuideModifierMode::OneShot;
                next.prompt            = "Double-tap shift to keep uppercase on.\nThen type \"A\", \"B\", \"C\".";
                _first_tap_released_at = timestamp_ms;
                publish(std::move(next), false);
            } else {
                next.phase             = GuidePhase::LockAwaitFirstTap;
                next.prompt            = "Double-tap shift a little faster.";
                _first_tap_released_at = 0;
                publish(std::move(next), true);
            }
            return;
        }

        case GuidePhase::LockAwaitLetter:
            if (valid_tap) {
                next.phase         = GuidePhase::LockAwaitFirstTap;
                next.shift_locked  = false;
                next.cursor_target = GuideTarget::Shift;
                next.prompt        = "shift unlocked. Double-tap it again.";
                publish(std::move(next), true);
            } else {
                next.prompt = lockedPrompt(expectedSequenceCharacter(next));
                publish(std::move(next), true);
            }
            return;

        case GuidePhase::LockAwaitUnlock:
            if (valid_tap) {
                next.shift_locked  = false;
                next.cursor_target = GuideTarget::Shift;
                completeExercise(std::move(next), "Unlocked. shift guide complete!");
            } else {
                next.prompt = "Tap shift quickly to unlock.";
                publish(std::move(next), true);
            }
            return;

        case GuidePhase::Intro:
        case GuidePhase::HoldAwaitShift:
        case GuidePhase::ModifierOverview:
        case GuidePhase::SymAwaitSymbol:
        case GuidePhase::FnAwaitKey:
        case GuidePhase::TextAwaitCharacter:
        case GuidePhase::SuccessHold:
        case GuidePhase::Done:
            publish(std::move(next), false);
            return;
    }
}

void GuideModel::handleSym(bool pressed, uint32_t timestamp_ms)
{
    if (pressed) {
        if (_sym_down) {
            return;
        }
        handleSymPressed(_state.get(), timestamp_ms);
        return;
    }

    if (!_sym_down) {
        return;
    }
    handleSymReleased(_state.get(), timestamp_ms);
}

void GuideModel::handleSymMode(GuideModifierMode mode)
{
    if (_sym_mode == mode) {
        return;
    }

    _sym_mode            = mode;
    _sym_down            = mode == GuideModifierMode::Held;
    _sym_tap_invalid     = false;
    _sym_second_tap      = false;
    _sym_cancel_one_shot = false;
    _sym_pressed_at      = 0;
    _sym_first_tap_at    = 0;

    GuideLessonState next = _state.get();
    next.sym_pressed      = mode == GuideModifierMode::Held;
    next.sym_one_shot     = mode == GuideModifierMode::OneShot;
    next.sym_locked       = mode == GuideModifierMode::Locked;
    if (next.exercise_index == 5) {
        if (next.phase == GuidePhase::LockAwaitUnlock && mode == GuideModifierMode::Inactive) {
            next.cursor_target = GuideTarget::Hash;
            completeExercise(std::move(next), "Unlocked. sym guide complete!");
        } else {
            publish(std::move(next), false);
        }
    } else {
        publish(std::move(next), false);
    }
}

void GuideModel::handleSymPressed(GuideLessonState next, uint32_t timestamp_ms)
{
    _sym_down            = true;
    _sym_mode            = GuideModifierMode::Held;
    _sym_tap_invalid     = false;
    _sym_pressed_at      = timestamp_ms;
    _sym_second_tap      = false;
    _sym_cancel_one_shot = false;
    next.sym_pressed     = true;

    if (next.sym_locked) {
        publish(std::move(next), false);
        return;
    }

    if (next.sym_one_shot) {
        const bool inside_window = _sym_first_tap_at != 0 && timestamp_ms - _sym_first_tap_at <= kDoubleTapWindowMs;
        _sym_second_tap          = inside_window;
        _sym_cancel_one_shot     = !inside_window;
        next.sym_one_shot        = false;
    }
    publish(std::move(next), false);
}

void GuideModel::handleSymReleased(GuideLessonState next, uint32_t timestamp_ms)
{
    _sym_down               = false;
    next.sym_pressed        = false;
    const uint32_t held_for = timestamp_ms - _sym_pressed_at;
    const bool valid_tap    = !_sym_tap_invalid && held_for <= kMaximumTapDurationMs;

    if (next.sym_locked) {
        if (valid_tap) {
            next.sym_locked   = false;
            next.sym_one_shot = false;
            _sym_first_tap_at = 0;
        }
    } else if (_sym_second_tap && valid_tap) {
        next.sym_locked   = true;
        next.sym_one_shot = false;
        _sym_first_tap_at = 0;
    } else if (_sym_cancel_one_shot) {
        next.sym_one_shot = false;
        _sym_first_tap_at = 0;
    } else if (valid_tap) {
        next.sym_one_shot = true;
        _sym_first_tap_at = timestamp_ms;
    } else {
        next.sym_one_shot = false;
        _sym_first_tap_at = 0;
    }

    _sym_tap_invalid     = false;
    _sym_second_tap      = false;
    _sym_cancel_one_shot = false;
    _sym_mode            = next.sym_locked     ? GuideModifierMode::Locked
                           : next.sym_one_shot ? GuideModifierMode::OneShot
                                               : GuideModifierMode::Inactive;
    if (next.phase == GuidePhase::LockAwaitUnlock && _sym_mode == GuideModifierMode::Inactive) {
        next.cursor_target = GuideTarget::Hash;
        completeExercise(std::move(next), "Unlocked. sym guide complete!");
    } else {
        publish(std::move(next), false);
    }
}

void GuideModel::handleFn(bool pressed, uint32_t timestamp_ms)
{
    if (pressed) {
        if (_fn_down) {
            return;
        }
        handleFnPressed(_state.get(), timestamp_ms);
        return;
    }

    if (!_fn_down) {
        return;
    }
    handleFnReleased(_state.get(), timestamp_ms);
}

void GuideModel::handleFnMode(GuideModifierMode mode)
{
    if (_fn_mode == mode) {
        return;
    }

    _fn_mode            = mode;
    _fn_down            = mode == GuideModifierMode::Held;
    _fn_tap_invalid     = false;
    _fn_second_tap      = false;
    _fn_cancel_one_shot = false;
    _fn_pressed_at      = 0;
    _fn_first_tap_at    = 0;

    GuideLessonState next = _state.get();
    next.fn_pressed       = mode == GuideModifierMode::Held;
    next.fn_one_shot      = mode == GuideModifierMode::OneShot;
    next.fn_locked        = mode == GuideModifierMode::Locked;
    if (next.exercise_index == 6) {
        if (next.phase == GuidePhase::LockAwaitUnlock && mode == GuideModifierMode::Inactive) {
            next.cursor_target = GuideTarget::Right;
            completeExercise(std::move(next), "Unlocked. fn guide complete!");
        } else {
            publish(std::move(next), false);
        }
    } else {
        publish(std::move(next), false);
    }
}

void GuideModel::handleFnPressed(GuideLessonState next, uint32_t timestamp_ms)
{
    _fn_down            = true;
    _fn_mode            = GuideModifierMode::Held;
    _fn_tap_invalid     = false;
    _fn_pressed_at      = timestamp_ms;
    _fn_second_tap      = false;
    _fn_cancel_one_shot = false;
    next.fn_pressed     = true;

    if (next.fn_locked) {
        publish(std::move(next), false);
        return;
    }

    if (next.fn_one_shot) {
        const bool inside_window = _fn_first_tap_at != 0 && timestamp_ms - _fn_first_tap_at <= kDoubleTapWindowMs;
        _fn_second_tap           = inside_window;
        _fn_cancel_one_shot      = !inside_window;
        next.fn_one_shot         = false;
    }
    publish(std::move(next), false);
}

void GuideModel::handleFnReleased(GuideLessonState next, uint32_t timestamp_ms)
{
    _fn_down                = false;
    next.fn_pressed         = false;
    const uint32_t held_for = timestamp_ms - _fn_pressed_at;
    const bool valid_tap    = !_fn_tap_invalid && held_for <= kMaximumTapDurationMs;

    if (next.fn_locked) {
        if (valid_tap) {
            next.fn_locked   = false;
            next.fn_one_shot = false;
            _fn_first_tap_at = 0;
        }
    } else if (_fn_second_tap && valid_tap) {
        next.fn_locked   = true;
        next.fn_one_shot = false;
        _fn_first_tap_at = 0;
    } else if (_fn_cancel_one_shot) {
        next.fn_one_shot = false;
        _fn_first_tap_at = 0;
    } else if (valid_tap) {
        next.fn_one_shot = true;
        _fn_first_tap_at = timestamp_ms;
    } else {
        next.fn_one_shot = false;
        _fn_first_tap_at = 0;
    }

    _fn_tap_invalid     = false;
    _fn_second_tap      = false;
    _fn_cancel_one_shot = false;
    _fn_mode            = next.fn_locked     ? GuideModifierMode::Locked
                          : next.fn_one_shot ? GuideModifierMode::OneShot
                                             : GuideModifierMode::Inactive;
    if (next.phase == GuidePhase::LockAwaitUnlock && _fn_mode == GuideModifierMode::Inactive) {
        next.cursor_target = GuideTarget::Right;
        completeExercise(std::move(next), "Unlocked. fn guide complete!");
    } else {
        publish(std::move(next), false);
    }
}

void GuideModel::handleCharacter(const GuideInputEvent& event)
{
    if (event.character == '\0') {
        return;
    }

    const int exercise_index = _state.get().exercise_index;
    if (exercise_index == 7) {
        handleFinalText(event);
        return;
    }

    const char character = static_cast<char>(std::toupper(static_cast<unsigned char>(event.character)));
    if (exercise_index == 4 || exercise_index == 6) {
        return;
    }

    if (exercise_index == 5) {
        if (event.pressed) {
            markCharacterDown(character);
            handleSymCharacterPressed(_state.get(), character);
        } else {
            markCharacterUp(character);
            handleSymCharacterReleased(_state.get());
        }
        return;
    }

    if (event.pressed) {
        markCharacterDown(character);
        handleCharacterPressed(_state.get(), character);
    } else {
        markCharacterUp(character);
        handleCharacterReleased(_state.get(), character);
    }
}

void GuideModel::handleCharacterPressed(GuideLessonState next, char character)
{
    if (isSuccessPhase(next.phase)) {
        return;
    }

    const char expected = next.exercise_index < 2 ? 'A' : expectedSequenceCharacter(next);
    if (character == expected) {
        next.character_pressed = character;
    }

    switch (next.phase) {
        case GuidePhase::PlainAwaitLetter:
            if (_shift_mode != GuideModifierMode::Inactive) {
                _shift_tap_invalid = true;
                next.prompt        = "Turn shift off, then press \"A\".";
                publish(std::move(next), true);
            } else if (character == 'A') {
                next.typed_text.assign(1, 'a');
                ++next.result_revision;
                completeExercise(std::move(next), "Nice! You typed \"a\".");
            } else {
                next.prompt = "Press the \"A\" key.";
                publish(std::move(next), true);
            }
            return;

        case GuidePhase::HoldAwaitShift:
            next.prompt = "Hold shift first, then press \"A\".";
            publish(std::move(next), true);
            return;

        case GuidePhase::HoldAwaitLetter:
            if (_shift_mode == GuideModifierMode::Held && character == 'A') {
                _shift_tap_invalid = true;
                next.typed_text.assign(1, 'A');
                ++next.result_revision;
                completeExercise(std::move(next), "Nice! You typed uppercase \"A\".");
            } else {
                next.prompt = "Keep holding shift and press \"A\".";
                publish(std::move(next), true);
            }
            return;

        case GuidePhase::OneShotAwaitShift:
            if (_shift_down) {
                _shift_tap_invalid = true;
            }
            next.prompt = oneShotPrompt(expected);
            publish(std::move(next), true);
            return;

        case GuidePhase::OneShotAwaitLetter:
            if (character == expected && _shift_mode == GuideModifierMode::OneShot) {
                appendSequenceCharacter(std::move(next), character);
            } else {
                next.phase         = GuidePhase::OneShotAwaitShift;
                next.cursor_target = GuideTarget::Shift;
                next.prompt        = std::string("Try again: tap shift, then press \"") + expected + "\".";
                publish(std::move(next), true);
            }
            return;

        case GuidePhase::LockAwaitFirstTap:
        case GuidePhase::LockAwaitSecondTap:
            if (_shift_down) {
                _shift_tap_invalid = true;
            }
            next.prompt = std::string("Double-tap shift before typing \"") + expected + "\".";
            publish(std::move(next), true);
            return;

        case GuidePhase::LockAwaitLetter:
            if (_shift_mode == GuideModifierMode::Locked && character == expected) {
                appendSequenceCharacter(std::move(next), character);
            } else {
                next.prompt = std::string("Press \"") + expected + "\" next. shift stays locked.";
                publish(std::move(next), true);
            }
            return;

        case GuidePhase::LockAwaitUnlock:
            next.prompt = "Tap shift once to unlock.";
            publish(std::move(next), true);
            return;

        case GuidePhase::Intro:
        case GuidePhase::ModifierOverview:
        case GuidePhase::SymAwaitSymbol:
        case GuidePhase::FnAwaitKey:
        case GuidePhase::TextAwaitCharacter:
        case GuidePhase::SuccessHold:
        case GuidePhase::Done:
            return;
    }
}

void GuideModel::handleCharacterReleased(GuideLessonState next, char character)
{
    if (next.character_pressed != character) {
        return;
    }
    next.character_pressed = '\0';
    publish(std::move(next), false);
}

void GuideModel::handleSymCharacterPressed(GuideLessonState next, char character)
{
    if (isSuccessPhase(next.phase)) {
        return;
    }
    if (next.phase == GuidePhase::LockAwaitUnlock) {
        next.prompt = "Tap sym once to unlock.";
        publish(std::move(next), true);
        return;
    }

    const bool direct_symbol   = character == '!' || character == '@' || character == '#';
    const bool modifier_active = _sym_mode != GuideModifierMode::Inactive || next.sym_one_shot || next.sym_locked;
    char symbol                = character;
    if (!direct_symbol && modifier_active) {
        switch (character) {
            case '1':
                symbol = '!';
                break;
            case '2':
                symbol = '@';
                break;
            case '3':
                symbol = '#';
                break;
            default:
                break;
        }
    }

    if (_sym_down) {
        _sym_tap_invalid = true;
    }
    if (next.sym_one_shot) {
        next.sym_one_shot = false;
        _sym_mode         = GuideModifierMode::Inactive;
        _sym_first_tap_at = 0;
    }

    const char expected = expectedSymCharacter(next);
    if ((direct_symbol || modifier_active) && symbol == expected) {
        next.character_pressed = symbol;
        appendSymCharacter(std::move(next), symbol);
        return;
    }

    next.prompt = symPrompt(expected);
    publish(std::move(next), true);
}

void GuideModel::handleSymCharacterReleased(GuideLessonState next)
{
    if (next.character_pressed == '\0') {
        return;
    }
    next.character_pressed = '\0';
    publish(std::move(next), false);
}

void GuideModel::handleFinalText(const GuideInputEvent& event)
{
    if (event.pressed) {
        markCharacterDown(event.character);
        handleFinalTextPressed(_state.get(), resolveFinalCharacter(_state.get(), event.character));
    } else {
        markCharacterUp(event.character);
        handleFinalTextReleased(_state.get());
    }
}

void GuideModel::handleFinalTextPressed(GuideLessonState next, char character)
{
    if (isSuccessPhase(next.phase)) {
        return;
    }

    if (_shift_down) {
        _shift_tap_invalid = true;
    }
    if (_sym_down) {
        _sym_tap_invalid = true;
    }
    if (_shift_mode == GuideModifierMode::OneShot) {
        _shift_mode = GuideModifierMode::Inactive;
    }
    if (next.sym_one_shot) {
        next.sym_one_shot = false;
        _sym_mode         = GuideModifierMode::Inactive;
        _sym_first_tap_at = 0;
    }

    const char expected = expectedFinalCharacter(next);
    if (character != expected) {
        clearModifierState();
        next.shift_pressed = false;
        next.shift_locked  = false;
        next.sym_pressed   = false;
        next.sym_one_shot  = false;
        next.sym_locked    = false;
        next.fn_pressed    = false;
        next.fn_one_shot   = false;
        next.fn_locked     = false;
        next.prompt        = finalTextPrompt();
        publish(std::move(next), true);
        return;
    }

    next.character_pressed = character;
    next.typed_text.push_back(character);
    ++next.result_revision;
    if (next.typed_text.size() == sizeof(kFinalText) - 1) {
        next.cursor_target = GuideTarget::None;
        completeExercise(std::move(next), "Perfect! You are ready to type.");
        return;
    }

    next.prompt = finalTextPrompt();
    publish(std::move(next), false);
}

void GuideModel::handleFinalTextReleased(GuideLessonState next)
{
    if (next.character_pressed == '\0') {
        return;
    }
    next.character_pressed = '\0';
    publish(std::move(next), false);
}

void GuideModel::handleFunctionKey(const GuideInputEvent& event)
{
    const char marker = functionKeyMarker(event.key);
    if (event.pressed) {
        markCharacterDown(marker);
        handleFunctionKeyPressed(_state.get(), event.key);
    } else {
        markCharacterUp(marker);
        handleFunctionKeyReleased(_state.get(), event.key);
    }
}

void GuideModel::handleFunctionKeyPressed(GuideLessonState next, GuideKey key)
{
    if (isSuccessPhase(next.phase)) {
        return;
    }
    if (next.phase == GuidePhase::LockAwaitUnlock) {
        next.prompt = "Tap fn once to unlock.";
        publish(std::move(next), true);
        return;
    }

    if (_fn_down) {
        _fn_tap_invalid = true;
    }
    if (next.fn_one_shot) {
        next.fn_one_shot = false;
        _fn_mode         = GuideModifierMode::Inactive;
        _fn_first_tap_at = 0;
    }

    const GuideKey expected = expectedFunctionKey(next);
    if (key != expected) {
        next.prompt = fnPrompt(expected);
        publish(std::move(next), true);
        return;
    }

    next.character_pressed = functionKeyMarker(key);
    appendFunctionKey(std::move(next), key);
}

void GuideModel::handleFunctionKeyReleased(GuideLessonState next, GuideKey key)
{
    if (next.character_pressed != functionKeyMarker(key)) {
        return;
    }
    next.character_pressed = '\0';
    publish(std::move(next), false);
}

void GuideModel::appendSequenceCharacter(GuideLessonState next, char character)
{
    if (next.exercise_index == 2) {
        _shift_mode = GuideModifierMode::Inactive;
    }
    next.typed_text.push_back(character);
    ++next.result_revision;

    if (next.typed_text.size() >= 3) {
        if (next.exercise_index == 2) {
            next.cursor_target = GuideTarget::C;
            completeExercise(std::move(next), "Nice! You typed \"A\", \"B\", \"C\".");
        } else {
            next.phase         = GuidePhase::LockAwaitUnlock;
            next.cursor_target = GuideTarget::Shift;
            next.prompt        = "\"A\", \"B\", \"C\" complete.\nTap shift once to unlock.";
            publish(std::move(next), false);
        }
        return;
    }

    const char target = expectedSequenceCharacter(next);
    if (next.exercise_index == 2) {
        next.phase         = GuidePhase::OneShotAwaitShift;
        next.cursor_target = GuideTarget::Shift;
        next.prompt        = oneShotPrompt(target);
    } else {
        next.phase         = GuidePhase::LockAwaitLetter;
        next.cursor_target = targetForCharacter(target);
        next.prompt        = lockedPrompt(target);
    }
    publish(std::move(next), false);
}

void GuideModel::appendSymCharacter(GuideLessonState next, char character)
{
    next.typed_text.push_back(character);
    ++next.result_revision;

    if (next.typed_text.size() >= 3) {
        if (_sym_mode == GuideModifierMode::Locked || next.sym_locked) {
            next.phase         = GuidePhase::LockAwaitUnlock;
            next.cursor_target = GuideTarget::Shift;
            next.prompt        = "\"!\", \"@\", \"#\" complete.\nTap sym once to unlock.";
            publish(std::move(next), false);
        } else {
            next.cursor_target = GuideTarget::Hash;
            completeExercise(std::move(next), "Nice! You typed \"!\", \"@\", \"#\".");
        }
        return;
    }

    const char target      = expectedSymCharacter(next);
    next.cursor_target     = targetForSymCharacter(target);
    next.prompt            = symPrompt(target);
    next.last_action_error = false;
    publish(std::move(next), false);
}

void GuideModel::appendFunctionKey(GuideLessonState next, GuideKey key)
{
    next.typed_text.push_back(functionKeyMarker(key));
    ++next.result_revision;

    if (next.typed_text.size() >= 4) {
        if (_fn_mode == GuideModifierMode::Locked || next.fn_locked) {
            next.phase         = GuidePhase::LockAwaitUnlock;
            next.cursor_target = GuideTarget::Fn;
            next.prompt        = "All directions complete.\nTap fn once to unlock.";
            publish(std::move(next), false);
        } else {
            next.cursor_target = GuideTarget::Right;
            completeExercise(std::move(next), "Nice! You used all four directions.");
        }
        return;
    }

    const GuideKey target = expectedFunctionKey(next);
    next.cursor_target    = targetForFunctionKey(target);
    next.prompt           = fnPrompt(target);
    publish(std::move(next), false);
}

void GuideModel::completeExercise(GuideLessonState next, std::string prompt)
{
    next.phase          = GuidePhase::SuccessHold;
    next.prompt         = std::move(prompt);
    _success_started_at = 0;
    publish(std::move(next), false);
}

void GuideModel::clearModifierState()
{
    _shift_down            = false;
    _shift_tap_invalid     = false;
    _shift_mode            = GuideModifierMode::Inactive;
    _shift_pressed_at      = 0;
    _first_tap_released_at = 0;
    _sym_down              = false;
    _sym_tap_invalid       = false;
    _sym_second_tap        = false;
    _sym_cancel_one_shot   = false;
    _sym_mode              = GuideModifierMode::Inactive;
    _sym_pressed_at        = 0;
    _sym_first_tap_at      = 0;
    _fn_down               = false;
    _fn_tap_invalid        = false;
    _fn_second_tap         = false;
    _fn_cancel_one_shot    = false;
    _fn_mode               = GuideModifierMode::Inactive;
    _fn_pressed_at         = 0;
    _fn_first_tap_at       = 0;
}

void GuideModel::navigateToExercise(int exercise_index)
{
    _pressed_characters.fill(false);
    _shift_tap_invalid       = false;
    _sym_tap_invalid         = false;
    _sym_second_tap          = false;
    _sym_cancel_one_shot     = false;
    _fn_tap_invalid          = false;
    _fn_second_tap           = false;
    _fn_cancel_one_shot      = false;
    _shift_down              = _shift_mode == GuideModifierMode::Held;
    _sym_down                = _sym_mode == GuideModifierMode::Held;
    _fn_down                 = _fn_mode == GuideModifierMode::Held;
    _shift_pressed_at        = 0;
    _first_tap_released_at   = 0;
    _sym_pressed_at          = 0;
    _sym_first_tap_at        = 0;
    _fn_pressed_at           = 0;
    _fn_first_tap_at         = 0;
    _success_started_at      = 0;
    _pressed_character_count = 0;

    if (exercise_index == kExerciseCount - 1) {
        clearModifierState();
    }

    GuideLessonState next = _state.get();
    next.exercise_index   = exercise_index;
    next.typed_text.clear();
    next.shift_pressed     = _shift_mode == GuideModifierMode::Held;
    next.shift_locked      = _shift_mode == GuideModifierMode::Locked;
    next.sym_pressed       = _sym_mode == GuideModifierMode::Held;
    next.sym_one_shot      = _sym_mode == GuideModifierMode::OneShot;
    next.sym_locked        = _sym_mode == GuideModifierMode::Locked;
    next.fn_pressed        = _fn_mode == GuideModifierMode::Held;
    next.fn_one_shot       = _fn_mode == GuideModifierMode::OneShot;
    next.fn_locked         = _fn_mode == GuideModifierMode::Locked;
    next.character_pressed = '\0';

    switch (exercise_index) {
        case 0:
            next.phase         = GuidePhase::PlainAwaitLetter;
            next.cursor_target = GuideTarget::A;
            next.prompt        = _shift_mode == GuideModifierMode::Inactive ? "Press \"A\" to type lowercase \"a\"."
                                                                            : "Turn shift off, then press \"A\".";
            break;
        case 1:
            next.phase =
                _shift_mode == GuideModifierMode::Held ? GuidePhase::HoldAwaitLetter : GuidePhase::HoldAwaitShift;
            next.cursor_target = _shift_mode == GuideModifierMode::Held ? GuideTarget::A : GuideTarget::Shift;
            next.prompt        = _shift_mode == GuideModifierMode::Held
                                     ? "Keep holding shift and press \"A\"."
                                     : "Hold shift while pressing \"A\".\nThis types uppercase \"A\".";
            break;
        case 2:
            next.phase         = _shift_mode == GuideModifierMode::OneShot ? GuidePhase::OneShotAwaitLetter
                                                                           : GuidePhase::OneShotAwaitShift;
            next.cursor_target = _shift_mode == GuideModifierMode::OneShot ? GuideTarget::A : GuideTarget::Shift;
            next.prompt        = _shift_mode == GuideModifierMode::OneShot
                                     ? oneShotArmedPrompt('A')
                                     : "Tap shift once for one uppercase key.\nThen press \"A\".";
            break;
        case 3:
            next.phase =
                _shift_mode == GuideModifierMode::Locked ? GuidePhase::LockAwaitLetter : GuidePhase::LockAwaitFirstTap;
            next.cursor_target = _shift_mode == GuideModifierMode::Locked ? GuideTarget::A : GuideTarget::Shift;
            next.prompt        = _shift_mode == GuideModifierMode::Locked
                                     ? lockedPrompt('A')
                                     : "Double-tap shift to keep uppercase on.\nThen type \"A\", \"B\", \"C\".";
            break;
        case 4:
            next.phase         = GuidePhase::ModifierOverview;
            next.cursor_target = GuideTarget::Shift;
            next.prompt =
                "sym, shift, and fn work the same way.\nTap once, double-tap to lock, or hold.\nPress Enter to "
                "continue.";
            break;
        case 5:
            next.phase         = GuidePhase::SymAwaitSymbol;
            next.cursor_target = GuideTarget::Bang;
            next.prompt        = symPrompt('!');
            break;
        case 6:
            next.phase         = GuidePhase::FnAwaitKey;
            next.cursor_target = GuideTarget::Up;
            next.prompt        = fnPrompt(GuideKey::Up);
            break;
        case 7:
        default:
            next.phase         = GuidePhase::TextAwaitCharacter;
            next.cursor_target = GuideTarget::None;
            next.prompt        = finalTextPrompt();
            break;
    }
    publish(std::move(next), false);
}

void GuideModel::advanceExercise()
{
    GuideLessonState next = _state.get();
    _success_started_at   = 0;
    ++next.exercise_index;
    next.typed_text.clear();
    next.character_pressed = '\0';
    next.last_action_error = false;

    switch (next.exercise_index) {
        case 1:
            next.phase         = GuidePhase::HoldAwaitShift;
            next.cursor_target = GuideTarget::Shift;
            next.prompt        = "Hold shift while pressing \"A\".\nThis types uppercase \"A\".";
            break;
        case 2:
            next.phase         = GuidePhase::OneShotAwaitShift;
            next.cursor_target = GuideTarget::Shift;
            next.prompt        = "Tap shift once for one uppercase key.\nThen press \"A\".";
            break;
        case 3:
            next.phase             = GuidePhase::LockAwaitFirstTap;
            next.cursor_target     = GuideTarget::Shift;
            next.shift_locked      = false;
            next.prompt            = "Double-tap shift to keep uppercase on.\nThen type \"A\", \"B\", \"C\".";
            _first_tap_released_at = 0;
            break;
        case 4:
            next.phase         = GuidePhase::ModifierOverview;
            next.cursor_target = GuideTarget::Shift;
            next.shift_pressed = false;
            next.shift_locked  = false;
            next.prompt =
                "sym, shift, and fn work the same way.\nTap once, double-tap to lock, or hold.\nPress Enter to "
                "continue.";
            break;
        case 5:
            next.phase         = GuidePhase::SymAwaitSymbol;
            next.cursor_target = GuideTarget::Bang;
            next.sym_pressed   = false;
            next.sym_one_shot  = false;
            next.sym_locked    = false;
            next.prompt        = symPrompt('!');
            _sym_first_tap_at  = 0;
            break;
        case 6:
            next.phase         = GuidePhase::FnAwaitKey;
            next.cursor_target = GuideTarget::Up;
            next.fn_pressed    = _fn_mode == GuideModifierMode::Held;
            next.fn_one_shot   = _fn_mode == GuideModifierMode::OneShot;
            next.fn_locked     = _fn_mode == GuideModifierMode::Locked;
            next.prompt        = fnPrompt(GuideKey::Up);
            _fn_first_tap_at   = 0;
            break;
        case 7:
            clearModifierState();
            next.phase         = GuidePhase::TextAwaitCharacter;
            next.cursor_target = GuideTarget::None;
            next.shift_pressed = false;
            next.shift_locked  = false;
            next.sym_pressed   = false;
            next.sym_one_shot  = false;
            next.sym_locked    = false;
            next.fn_pressed    = false;
            next.fn_one_shot   = false;
            next.fn_locked     = false;
            next.prompt        = finalTextPrompt();
            break;
        default:
            clearModifierState();
            next.exercise_index = kExerciseCount - 1;
            next.phase          = GuidePhase::Done;
            next.cursor_target  = GuideTarget::None;
            next.shift_pressed  = false;
            next.shift_locked   = false;
            next.sym_pressed    = false;
            next.sym_one_shot   = false;
            next.sym_locked     = false;
            next.fn_pressed     = false;
            next.fn_one_shot    = false;
            next.fn_locked      = false;
            next.typed_text     = kFinalText;
            next.prompt         = "All done. Press Enter to finish.";
            break;
    }
    publish(std::move(next), false);
}

void GuideModel::markCharacterDown(char character)
{
    const auto index = static_cast<unsigned char>(character);
    if (index >= _pressed_characters.size() || _pressed_characters[index]) {
        return;
    }
    _pressed_characters[index] = true;
    ++_pressed_character_count;
}

void GuideModel::markCharacterUp(char character)
{
    const auto index = static_cast<unsigned char>(character);
    if (index >= _pressed_characters.size() || !_pressed_characters[index]) {
        return;
    }
    _pressed_characters[index] = false;
    --_pressed_character_count;
}

bool GuideModel::inputReleased() const
{
    return !_shift_down && !_sym_down && !_fn_down && _pressed_character_count == 0;
}

void GuideModel::publish(GuideLessonState next, bool is_error)
{
    next.last_action_error = is_error;
    ++next.attention_revision;
    _state.set(std::move(next));
}

char GuideModel::expectedSequenceCharacter(const GuideLessonState& state)
{
    const size_t index = state.typed_text.size();
    return index < 3 ? "ABC"[index] : 'C';
}

char GuideModel::expectedSymCharacter(const GuideLessonState& state)
{
    const size_t index = state.typed_text.size();
    return index < 3 ? "!@#"[index] : '#';
}

GuideKey GuideModel::expectedFunctionKey(const GuideLessonState& state)
{
    constexpr std::array<GuideKey, 4> kDirections = {
        GuideKey::Up,
        GuideKey::Down,
        GuideKey::Left,
        GuideKey::Right,
    };
    const size_t index = state.typed_text.size();
    return index < kDirections.size() ? kDirections[index] : GuideKey::Right;
}

char GuideModel::resolveFinalCharacter(const GuideLessonState& state, char character) const
{
    const bool shift_active = _shift_mode != GuideModifierMode::Inactive || state.shift_locked;
    const bool sym_active   = _sym_mode != GuideModifierMode::Inactive || state.sym_one_shot || state.sym_locked;

    if (sym_active || shift_active) {
        switch (character) {
            case '1':
                return '!';
            case '2':
                if (sym_active) {
                    return '@';
                }
                break;
            case '3':
                if (sym_active) {
                    return '#';
                }
                break;
            default:
                break;
        }
    }

    const auto value = static_cast<unsigned char>(character);
    if (!std::isalpha(value)) {
        return character;
    }
    return static_cast<char>(shift_active ? std::toupper(value) : std::tolower(value));
}

char GuideModel::expectedFinalCharacter(const GuideLessonState& state)
{
    const size_t index = state.typed_text.size();
    return index < sizeof(kFinalText) - 1 ? kFinalText[index] : kFinalText[sizeof(kFinalText) - 2];
}

GuideTarget GuideModel::targetForCharacter(char character)
{
    switch (character) {
        case 'B':
            return GuideTarget::B;
        case 'C':
            return GuideTarget::C;
        default:
            return GuideTarget::A;
    }
}

GuideTarget GuideModel::targetForSymCharacter(char character)
{
    switch (character) {
        case '@':
            return GuideTarget::At;
        case '#':
            return GuideTarget::Hash;
        default:
            return GuideTarget::Bang;
    }
}

GuideTarget GuideModel::targetForFunctionKey(GuideKey key)
{
    switch (key) {
        case GuideKey::Down:
            return GuideTarget::Down;
        case GuideKey::Left:
            return GuideTarget::Left;
        case GuideKey::Right:
            return GuideTarget::Right;
        default:
            return GuideTarget::Up;
    }
}

}  // namespace keyboard_guide
