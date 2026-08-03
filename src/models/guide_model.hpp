#pragma once

#include "core/guide_types.hpp"

#include <tools/observable/single_observable.hpp>

#include <array>

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
    static constexpr uint32_t kDoubleTapWindowMs    = 450;
    static constexpr uint32_t kSuccessHoldMs        = 1250;

    smooth_ui_toolkit::SingleObservable<GuideLessonState> _state{GuideLessonState{}};
    std::array<bool, 128> _pressed_characters{};
    bool _shift_down                = false;
    bool _shift_tap_invalid         = false;
    uint32_t _shift_pressed_at      = 0;
    uint32_t _first_tap_released_at = 0;
    uint32_t _success_started_at    = 0;
    int _pressed_character_count    = 0;

    void handleShift(bool pressed, uint32_t timestamp_ms);
    void handleShiftPressed(GuideLessonState next, uint32_t timestamp_ms);
    void handleShiftReleased(GuideLessonState next, uint32_t timestamp_ms);
    void handleCharacter(const GuideInputEvent& event);
    void handleCharacterPressed(GuideLessonState next, char character);
    void handleCharacterReleased(GuideLessonState next, char character);
    void appendSequenceCharacter(GuideLessonState next, char character);
    void completeExercise(GuideLessonState next, std::string prompt);
    void advanceExercise();
    void markCharacterDown(char character);
    void markCharacterUp(char character);
    bool inputReleased() const;
    void publish(GuideLessonState next, bool is_error);
    static char expectedSequenceCharacter(const GuideLessonState& state);
    static GuideTarget targetForCharacter(char character);
};

}  // namespace keyboard_guide
