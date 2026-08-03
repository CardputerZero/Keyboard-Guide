#include "models/guide_model.hpp"

#include <cctype>
#include <string>
#include <utility>

namespace keyboard_guide {

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
    if (!current.exercise_complete || current.completed || current.modifier_pressed || current.character_pressed ||
        now_ms - _success_started_at < kSuccessHoldMs) {
        return;
    }

    GuideLessonState next  = current;
    next.exercise_complete = false;
    next.character_pressed = false;
    next.last_action_error = false;

    if (next.exercise_index + 1 >= kExerciseCount) {
        next.completed = true;
        next.prompt    = "Done! Tap SHIFT, then a key for uppercase";
    } else {
        ++next.exercise_index;
        next.typed_text.clear();
        next.awaiting_character = !requiresShift(next.exercise_index);
        next.prompt             = promptForExercise(next.exercise_index);
    }
    publish(std::move(next), false);
}

void GuideModel::reset()
{
    _shift_down           = false;
    _shift_tap_invalid    = false;
    _reject_shift_release = false;
    _shift_pressed_at     = 0;
    _success_started_at   = 0;
    _state.set(GuideLessonState{});
}

void GuideModel::handleShift(bool pressed, uint32_t timestamp_ms)
{
    GuideLessonState next = _state.get();

    if (next.completed || next.exercise_complete) {
        if (!pressed && _shift_down) {
            _shift_down            = false;
            _shift_tap_invalid     = false;
            _reject_shift_release  = false;
            next.modifier_pressed  = false;
            next.last_action_error = false;
            publish(std::move(next), false);
        }
        return;
    }

    if (pressed) {
        if (_shift_down) {
            return;
        }
        _shift_down           = true;
        _shift_tap_invalid    = false;
        _shift_pressed_at     = timestamp_ms;
        next.modifier_pressed = true;

        if (!requiresShift(next.exercise_index)) {
            _reject_shift_release = true;
            next.prompt           = std::string("No SHIFT: press ") + expectedKey(next.exercise_index) + " to type " +
                          expectedResult(next.exercise_index);
            publish(std::move(next), true);
            return;
        }
        if (next.awaiting_character) {
            _reject_shift_release   = true;
            next.awaiting_character = false;
            next.prompt = std::string("Release SHIFT; then tap SHIFT, then ") + expectedKey(next.exercise_index);
            publish(std::move(next), true);
            return;
        }

        _reject_shift_release = false;
        next.prompt           = std::string("Release SHIFT, then press ") + expectedKey(next.exercise_index);
        publish(std::move(next), false);
        return;
    }

    if (!_shift_down) {
        return;
    }

    _shift_down             = false;
    next.modifier_pressed   = false;
    const uint32_t held_for = timestamp_ms - _shift_pressed_at;
    const bool valid_tap    = requiresShift(next.exercise_index) && !_shift_tap_invalid && !_reject_shift_release &&
                           held_for <= kMaximumTapDurationMs;
    _reject_shift_release = false;

    if (valid_tap) {
        next.awaiting_character = true;
        next.prompt             = std::string("Now press ") + expectedKey(next.exercise_index) + " to type " +
                      expectedResult(next.exercise_index);
        publish(std::move(next), false);
        return;
    }

    next.awaiting_character = !requiresShift(next.exercise_index);
    next.prompt             = held_for > kMaximumTapDurationMs
                                  ? std::string("Tap SHIFT quickly, then ") + expectedKey(next.exercise_index) + " to type " +
                            expectedResult(next.exercise_index)
                                  : promptForExercise(next.exercise_index);
    publish(std::move(next), true);
}

void GuideModel::handleCharacter(const GuideInputEvent& event)
{
    GuideLessonState next  = _state.get();
    const char normalized  = static_cast<char>(std::toupper(static_cast<unsigned char>(event.character)));
    const char expected    = expectedKey(next.exercise_index);
    const bool is_expected = normalized == expected;

    if (!event.pressed) {
        if (is_expected && next.character_pressed) {
            next.character_pressed = false;
            publish(std::move(next), false);
        }
        return;
    }
    if (next.completed || next.exercise_complete) {
        return;
    }
    if (is_expected) {
        next.character_pressed = true;
    }

    if (_shift_down) {
        _shift_tap_invalid = true;
        if (requiresShift(next.exercise_index)) {
            next.awaiting_character = false;
        }
        next.prompt = std::string("Release SHIFT; then tap SHIFT, then ") + expectedKey(next.exercise_index);
        publish(std::move(next), true);
        return;
    }

    if (!next.awaiting_character) {
        next.prompt = std::string("Try SHIFT, then ") + expectedKey(next.exercise_index) + " to type " +
                      expectedResult(next.exercise_index);
        publish(std::move(next), true);
        return;
    }

    if (!is_expected) {
        if (requiresShift(next.exercise_index)) {
            next.awaiting_character = false;
            next.prompt =
                std::string("Try SHIFT, then ") + expected + " to type " + expectedResult(next.exercise_index);
        } else {
            next.prompt = std::string("Try ") + expected + " to type " + expectedResult(next.exercise_index);
        }
        publish(std::move(next), true);
        return;
    }

    completeExercise(std::move(next), event.timestamp_ms);
}

void GuideModel::completeExercise(GuideLessonState next, uint32_t timestamp_ms)
{
    next.typed_text.assign(1, expectedResult(next.exercise_index));
    next.awaiting_character = false;
    next.exercise_complete  = true;
    next.prompt             = std::string("Success: you typed ") + expectedResult(next.exercise_index);
    _success_started_at     = timestamp_ms;
    publish(std::move(next), false);
}

void GuideModel::publish(GuideLessonState next, bool is_error)
{
    next.last_action_error = is_error;
    ++next.attention_revision;
    _state.set(std::move(next));
}

bool GuideModel::requiresShift(int exercise_index)
{
    return (exercise_index % 2) == 1;
}

char GuideModel::expectedKey(int exercise_index)
{
    return exercise_index < 2 ? 'A' : 'B';
}

char GuideModel::expectedResult(int exercise_index)
{
    const char key = expectedKey(exercise_index);
    return requiresShift(exercise_index) ? key : static_cast<char>(key - 'A' + 'a');
}

std::string GuideModel::promptForExercise(int exercise_index)
{
    if (requiresShift(exercise_index)) {
        return std::string("Tap SHIFT, then ") + expectedKey(exercise_index) + " to type " +
               expectedResult(exercise_index);
    }
    return std::string("Press ") + expectedKey(exercise_index) + " to type " + expectedResult(exercise_index);
}

}  // namespace keyboard_guide
