#pragma once

#include "core/guide_types.hpp"

#include <tools/observable/single_observable.hpp>

namespace keyboard_guide {

class GuideModel {
public:
    static constexpr int kStepCount = 6;

    smooth_ui_toolkit::SingleObservable<GuideLessonState>& state();
    void handleInput(const GuideInputEvent& event);
    void reset();

private:
    static constexpr uint32_t kMaximumTapDurationMs = 650;

    smooth_ui_toolkit::SingleObservable<GuideLessonState> _state{GuideLessonState{}};
    bool _shift_down           = false;
    bool _shift_tap_invalid    = false;
    bool _reject_shift_release = false;
    uint32_t _shift_pressed_at = 0;

    void handleShift(bool pressed, uint32_t timestamp_ms);
    void handleCharacter(char character);
    void publish(GuideLessonState next, bool is_error);
    static char expectedCharacter(int step_index);
    static int shiftStepFor(int step_index);
};

}  // namespace keyboard_guide
