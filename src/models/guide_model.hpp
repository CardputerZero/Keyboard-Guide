#pragma once

#include "core/guide_types.hpp"

#include <tools/observable/single_observable.hpp>

namespace keyboard_guide {

class GuideModel {
public:
    static constexpr int kExerciseCount = 4;

    smooth_ui_toolkit::SingleObservable<GuideLessonState>& state();
    void handleInput(const GuideInputEvent& event);
    void tick(uint32_t now_ms);
    void reset();

private:
    static constexpr uint32_t kMaximumTapDurationMs = 650;
    static constexpr uint32_t kSuccessHoldMs        = 1250;

    smooth_ui_toolkit::SingleObservable<GuideLessonState> _state{GuideLessonState{}};
    bool _shift_down             = false;
    bool _shift_tap_invalid      = false;
    bool _reject_shift_release   = false;
    uint32_t _shift_pressed_at   = 0;
    uint32_t _success_started_at = 0;

    void handleShift(bool pressed, uint32_t timestamp_ms);
    void handleCharacter(const GuideInputEvent& event);
    void completeExercise(GuideLessonState next, uint32_t timestamp_ms);
    void publish(GuideLessonState next, bool is_error);
    static bool requiresShift(int exercise_index);
    static char expectedKey(int exercise_index);
    static char expectedResult(int exercise_index);
    static std::string promptForExercise(int exercise_index);
};

}  // namespace keyboard_guide
