#include "models/guide_model.hpp"

#include <cctype>
#include <string>
#include <utility>

namespace keyboard_guide {
namespace {

bool isSuccessPhase(GuidePhase phase)
{
    return phase == GuidePhase::SuccessHold || phase == GuidePhase::Done;
}

std::string oneShotPrompt(char character)
{
    return std::string("Tap SHIFT, then ") + character + " to type " + character;
}

std::string oneShotArmedPrompt(char character)
{
    return std::string("Now press ") + character + " to type " + character;
}

std::string lockedPrompt(char character)
{
    return std::string("SHIFT locked. Now press ") + character;
}

}  // namespace

smooth_ui_toolkit::SingleObservable<GuideLessonState>& GuideModel::state()
{
    return _state;
}

void GuideModel::handleInput(const GuideInputEvent& event)
{
    if (event.repeated) {
        return;
    }

    switch (event.key) {
        case GuideKey::Shift:
            handleShift(event.pressed, event.timestamp_ms);
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
        next.prompt            = "Double-tap SHIFT to lock uppercase";
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
    _shift_down              = false;
    _shift_tap_invalid       = false;
    _shift_pressed_at        = 0;
    _first_tap_released_at   = 0;
    _success_started_at      = 0;
    _pressed_character_count = 0;
    _state.set(GuideLessonState{});
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

void GuideModel::handleShiftPressed(GuideLessonState next, uint32_t timestamp_ms)
{
    _shift_down        = true;
    _shift_tap_invalid = false;
    _shift_pressed_at  = timestamp_ms;
    next.shift_pressed = true;

    switch (next.phase) {
        case GuidePhase::PlainAwaitLetter:
            next.prompt = "Release SHIFT, then press A to type a";
            publish(std::move(next), true);
            return;

        case GuidePhase::HoldAwaitShift:
            next.phase         = GuidePhase::HoldAwaitLetter;
            next.cursor_target = GuideTarget::A;
            next.prompt        = "Keep holding SHIFT and press A";
            publish(std::move(next), false);
            return;

        case GuidePhase::OneShotAwaitShift:
            next.prompt = "Release SHIFT to arm one uppercase key";
            publish(std::move(next), false);
            return;

        case GuidePhase::OneShotAwaitLetter:
            _shift_tap_invalid = true;
            next.prompt        = "Release SHIFT; then tap it once again";
            publish(std::move(next), true);
            return;

        case GuidePhase::LockAwaitFirstTap:
            next.prompt = "Release SHIFT, then tap it again";
            publish(std::move(next), false);
            return;

        case GuidePhase::LockAwaitSecondTap:
            if (timestamp_ms - _first_tap_released_at <= kDoubleTapWindowMs) {
                next.prompt = "Release SHIFT to lock uppercase";
            } else {
                next.phase             = GuidePhase::LockAwaitFirstTap;
                next.prompt            = "Release SHIFT, then tap it again";
                _first_tap_released_at = 0;
            }
            publish(std::move(next), false);
            return;

        case GuidePhase::LockAwaitLetter:
            next.prompt = "Release SHIFT to unlock";
            publish(std::move(next), true);
            return;

        case GuidePhase::LockAwaitUnlock:
            next.prompt = "Release SHIFT to unlock";
            publish(std::move(next), false);
            return;

        case GuidePhase::HoldAwaitLetter:
        case GuidePhase::SuccessHold:
        case GuidePhase::Done:
            publish(std::move(next), false);
            return;
    }
}

void GuideModel::handleShiftReleased(GuideLessonState next, uint32_t timestamp_ms)
{
    _shift_down             = false;
    next.shift_pressed      = false;
    const uint32_t held_for = timestamp_ms - _shift_pressed_at;
    const bool valid_tap    = !_shift_tap_invalid && held_for <= kMaximumTapDurationMs;
    _shift_tap_invalid      = false;

    switch (next.phase) {
        case GuidePhase::PlainAwaitLetter:
            next.prompt = "Press A to type a";
            publish(std::move(next), false);
            return;

        case GuidePhase::HoldAwaitLetter:
            next.phase         = GuidePhase::HoldAwaitShift;
            next.cursor_target = GuideTarget::Shift;
            next.prompt        = "Keep SHIFT held while you press A";
            publish(std::move(next), true);
            return;

        case GuidePhase::OneShotAwaitShift: {
            const char target = expectedSequenceCharacter(next);
            if (valid_tap) {
                next.phase         = GuidePhase::OneShotAwaitLetter;
                next.cursor_target = targetForCharacter(target);
                next.prompt        = oneShotArmedPrompt(target);
                publish(std::move(next), false);
            } else {
                next.prompt = std::string("Use a quick SHIFT tap, then press ") + target;
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
                next.phase             = GuidePhase::LockAwaitSecondTap;
                next.prompt            = "Tap SHIFT again to lock";
                _first_tap_released_at = timestamp_ms;
                publish(std::move(next), false);
            } else {
                next.prompt = "Double-tap SHIFT quickly to lock";
                publish(std::move(next), true);
            }
            return;

        case GuidePhase::LockAwaitSecondTap: {
            const bool inside_window = _shift_pressed_at - _first_tap_released_at <= kDoubleTapWindowMs;
            if (valid_tap && inside_window) {
                const char target      = expectedSequenceCharacter(next);
                next.phase             = GuidePhase::LockAwaitLetter;
                next.shift_locked      = true;
                next.cursor_target     = targetForCharacter(target);
                next.prompt            = lockedPrompt(target);
                _first_tap_released_at = 0;
                publish(std::move(next), false);
            } else if (valid_tap) {
                next.prompt            = "Tap SHIFT again to lock";
                _first_tap_released_at = timestamp_ms;
                publish(std::move(next), false);
            } else {
                next.phase             = GuidePhase::LockAwaitFirstTap;
                next.prompt            = "Double-tap SHIFT quickly to lock";
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
                next.prompt        = "SHIFT unlocked. Double-tap to lock again";
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
                completeExercise(std::move(next), "Unlocked! You completed the SHIFT guide");
            } else {
                next.prompt = "Tap SHIFT quickly to unlock";
                publish(std::move(next), true);
            }
            return;

        case GuidePhase::HoldAwaitShift:
        case GuidePhase::SuccessHold:
        case GuidePhase::Done:
            publish(std::move(next), false);
            return;
    }
}

void GuideModel::handleCharacter(const GuideInputEvent& event)
{
    const char character = static_cast<char>(std::toupper(static_cast<unsigned char>(event.character)));
    if (character == '\0') {
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
            if (_shift_down) {
                _shift_tap_invalid = true;
                next.prompt        = "Release SHIFT, then press A to type a";
                publish(std::move(next), true);
            } else if (character == 'A') {
                next.typed_text.assign(1, 'a');
                ++next.result_revision;
                completeExercise(std::move(next), "Nice! You typed a");
            } else {
                next.prompt = "Try A to type a";
                publish(std::move(next), true);
            }
            return;

        case GuidePhase::HoldAwaitShift:
            next.prompt = "Hold SHIFT first, then press A";
            publish(std::move(next), true);
            return;

        case GuidePhase::HoldAwaitLetter:
            if (_shift_down && character == 'A') {
                _shift_tap_invalid = true;
                next.typed_text.assign(1, 'A');
                ++next.result_revision;
                completeExercise(std::move(next), "Nice! You held SHIFT to type A");
            } else {
                next.prompt = "Keep holding SHIFT and press A";
                publish(std::move(next), true);
            }
            return;

        case GuidePhase::OneShotAwaitShift:
            if (_shift_down) {
                _shift_tap_invalid = true;
                next.prompt        = "Release SHIFT, then tap it once";
            } else {
                next.prompt = std::string("Tap SHIFT first, then ") + expected;
            }
            publish(std::move(next), true);
            return;

        case GuidePhase::OneShotAwaitLetter:
            if (character == expected && !_shift_down) {
                appendSequenceCharacter(std::move(next), character);
            } else {
                next.phase         = GuidePhase::OneShotAwaitShift;
                next.cursor_target = GuideTarget::Shift;
                next.prompt        = std::string("Try again: tap SHIFT, then ") + expected;
                publish(std::move(next), true);
            }
            return;

        case GuidePhase::LockAwaitFirstTap:
        case GuidePhase::LockAwaitSecondTap:
            if (_shift_down) {
                _shift_tap_invalid = true;
            }
            next.prompt = "Double-tap SHIFT before typing ABC";
            publish(std::move(next), true);
            return;

        case GuidePhase::LockAwaitLetter:
            if (_shift_down) {
                _shift_tap_invalid = true;
                next.prompt        = "Release SHIFT; it is already locked";
                publish(std::move(next), true);
            } else if (character == expected) {
                appendSequenceCharacter(std::move(next), character);
            } else {
                next.prompt = std::string("SHIFT stays locked. Try ") + expected;
                publish(std::move(next), true);
            }
            return;

        case GuidePhase::LockAwaitUnlock:
            next.prompt = "ABC complete. Tap SHIFT once to unlock";
            publish(std::move(next), true);
            return;

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

void GuideModel::appendSequenceCharacter(GuideLessonState next, char character)
{
    next.typed_text.push_back(character);
    ++next.result_revision;

    if (next.typed_text.size() >= 3) {
        if (next.exercise_index == 2) {
            next.cursor_target = GuideTarget::C;
            completeExercise(std::move(next), "Nice! You typed ABC one key at a time");
        } else {
            next.phase         = GuidePhase::LockAwaitUnlock;
            next.cursor_target = GuideTarget::Shift;
            next.prompt        = "ABC complete. Tap SHIFT once to unlock";
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

void GuideModel::completeExercise(GuideLessonState next, std::string prompt)
{
    next.phase          = GuidePhase::SuccessHold;
    next.prompt         = std::move(prompt);
    _success_started_at = 0;
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
            next.prompt        = "Hold SHIFT, then press A to type A";
            break;
        case 2:
            next.phase         = GuidePhase::OneShotAwaitShift;
            next.cursor_target = GuideTarget::Shift;
            next.prompt        = "Tap SHIFT, then A to type A";
            break;
        case 3:
            next.phase             = GuidePhase::LockAwaitFirstTap;
            next.cursor_target     = GuideTarget::Shift;
            next.shift_locked      = false;
            next.prompt            = "Double-tap SHIFT to lock uppercase";
            _first_tap_released_at = 0;
            break;
        default:
            next.exercise_index = kExerciseCount - 1;
            next.phase          = GuidePhase::Done;
            next.cursor_target  = GuideTarget::Shift;
            next.shift_locked   = false;
            next.typed_text     = "ABC";
            next.prompt         = "Done! You know hold, one-shot, and lock";
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
    return !_shift_down && _pressed_character_count == 0;
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

}  // namespace keyboard_guide
