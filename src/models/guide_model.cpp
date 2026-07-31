#include "models/guide_model.hpp"

#include <cctype>

namespace keyboard_guide {

smooth_ui_toolkit::SingleObservable<GuideLessonState>& GuideModel::state()
{
    return _state;
}

void GuideModel::handleInput(const GuideInputEvent& event)
{
    if (event.repeated || _state.get().completed) {
        return;
    }

    switch (event.key) {
        case GuideKey::Shift:
            handleShift(event.pressed, event.timestamp_ms);
            break;
        case GuideKey::Character:
            if (event.pressed) {
                handleCharacter(event.character);
            }
            break;
        default:
            break;
    }
}

void GuideModel::reset()
{
    _shift_down           = false;
    _shift_tap_invalid    = false;
    _reject_shift_release = false;
    _shift_pressed_at     = 0;
    _state.set(GuideLessonState{});
}

void GuideModel::handleShift(bool pressed, uint32_t timestamp_ms)
{
    GuideLessonState next = _state.get();

    if (pressed) {
        if (_shift_down) {
            return;
        }

        _shift_down           = true;
        _shift_tap_invalid    = false;
        _shift_pressed_at     = timestamp_ms;
        next.modifier_pressed = true;

        if ((next.step_index % 2) != 0) {
            next.step_index       = shiftStepFor(next.step_index);
            next.prompt           = "One tap only - try again";
            _reject_shift_release = true;
            publish(std::move(next), true);
            return;
        }

        _reject_shift_release = false;
        next.prompt           = "Now release SHIFT";
        publish(std::move(next), false);
        return;
    }

    if (!_shift_down) {
        return;
    }

    _shift_down             = false;
    next.modifier_pressed   = false;
    const uint32_t held_for = timestamp_ms - _shift_pressed_at;
    const bool valid_tap    = !_shift_tap_invalid && !_reject_shift_release && held_for <= kMaximumTapDurationMs;
    _reject_shift_release   = false;

    if (valid_tap && next.step_index < kStepCount && (next.step_index % 2) == 0) {
        ++next.step_index;
        const char expected = expectedCharacter(next.step_index);
        next.prompt         = std::string("Type ") + expected;
        publish(std::move(next), false);
        return;
    }

    next.step_index = shiftStepFor(next.step_index);
    next.prompt     = held_for > kMaximumTapDurationMs ? "Use a quick tap" : "Tap SHIFT once";
    publish(std::move(next), true);
}

void GuideModel::handleCharacter(char character)
{
    GuideLessonState next = _state.get();
    if (_shift_down) {
        _shift_tap_invalid = true;
        next.step_index    = shiftStepFor(next.step_index);
        next.prompt        = "Release SHIFT before typing";
        publish(std::move(next), true);
        return;
    }

    if ((next.step_index % 2) == 0) {
        next.prompt = "SHIFT comes first";
        publish(std::move(next), true);
        return;
    }

    const char expected   = expectedCharacter(next.step_index);
    const char normalized = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
    if (normalized != expected) {
        next.step_index = shiftStepFor(next.step_index);
        next.prompt     = std::string("Try again with ") + expected;
        publish(std::move(next), true);
        return;
    }

    next.typed_text.push_back(expected);
    ++next.step_index;
    if (next.step_index >= kStepCount) {
        next.step_index = kStepCount;
        next.completed  = true;
        next.prompt     = "Nice! One tap changes one key.";
    } else {
        next.prompt = "Great - tap SHIFT again";
    }
    publish(std::move(next), false);
}

void GuideModel::publish(GuideLessonState next, bool is_error)
{
    next.last_action_error = is_error;
    ++next.attention_revision;
    _state.set(std::move(next));
}

char GuideModel::expectedCharacter(int step_index)
{
    if (step_index <= 1) {
        return 'A';
    }
    if (step_index <= 3) {
        return 'B';
    }
    return 'C';
}

int GuideModel::shiftStepFor(int step_index)
{
    if (step_index <= 1) {
        return 0;
    }
    if (step_index <= 3) {
        return 2;
    }
    if (step_index <= 5) {
        return 4;
    }
    return kStepCount;
}

}  // namespace keyboard_guide
