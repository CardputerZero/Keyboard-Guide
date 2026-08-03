#pragma once

#include "core/guide_types.hpp"

#include <tools/observable/single_observable.hpp>

#include <array>

namespace keyboard_guide {

class GuideModel {
public:
    static constexpr int kExerciseCount = 8;

    smooth_ui_toolkit::SingleObservable<GuideLessonState>& state();
    void handleInput(const GuideInputEvent& event);
    void tick(uint32_t now_ms);
    void reset();
    bool previousExercise();
    bool nextExercise();

private:
    static constexpr uint32_t kMaximumTapDurationMs = 650;
    static constexpr uint32_t kDoubleTapWindowMs    = 450;
    static constexpr uint32_t kSuccessHoldMs        = 1250;

    smooth_ui_toolkit::SingleObservable<GuideLessonState> _state{GuideLessonState{}};
    std::array<bool, 128> _pressed_characters{};
    bool _shift_down                = false;
    bool _shift_tap_invalid         = false;
    bool _sym_down                  = false;
    bool _sym_tap_invalid           = false;
    bool _sym_second_tap            = false;
    bool _sym_cancel_one_shot       = false;
    GuideModifierMode _shift_mode   = GuideModifierMode::Inactive;
    GuideModifierMode _sym_mode     = GuideModifierMode::Inactive;
    bool _fn_down                   = false;
    bool _fn_tap_invalid            = false;
    bool _fn_second_tap             = false;
    bool _fn_cancel_one_shot        = false;
    GuideModifierMode _fn_mode      = GuideModifierMode::Inactive;
    uint32_t _shift_pressed_at      = 0;
    uint32_t _first_tap_released_at = 0;
    uint32_t _sym_pressed_at        = 0;
    uint32_t _sym_first_tap_at      = 0;
    uint32_t _fn_pressed_at         = 0;
    uint32_t _fn_first_tap_at       = 0;
    uint32_t _success_started_at    = 0;
    int _pressed_character_count    = 0;

    void handleShift(bool pressed, uint32_t timestamp_ms);
    void handleShiftMode(GuideModifierMode mode);
    void handleShiftPressed(GuideLessonState next, uint32_t timestamp_ms);
    void handleShiftReleased(GuideLessonState next, uint32_t timestamp_ms);
    void handleSym(bool pressed, uint32_t timestamp_ms);
    void handleSymMode(GuideModifierMode mode);
    void handleSymPressed(GuideLessonState next, uint32_t timestamp_ms);
    void handleSymReleased(GuideLessonState next, uint32_t timestamp_ms);
    void handleCharacter(const GuideInputEvent& event);
    void handleCharacterPressed(GuideLessonState next, char character);
    void handleCharacterReleased(GuideLessonState next, char character);
    void handleSymCharacterPressed(GuideLessonState next, char character);
    void handleSymCharacterReleased(GuideLessonState next);
    void handleFn(bool pressed, uint32_t timestamp_ms);
    void handleFnMode(GuideModifierMode mode);
    void handleFnPressed(GuideLessonState next, uint32_t timestamp_ms);
    void handleFnReleased(GuideLessonState next, uint32_t timestamp_ms);
    void handleFunctionKey(const GuideInputEvent& event);
    void handleFunctionKeyPressed(GuideLessonState next, GuideKey key);
    void handleFunctionKeyReleased(GuideLessonState next, GuideKey key);
    void handleFinalText(const GuideInputEvent& event);
    void handleFinalTextPressed(GuideLessonState next, char character);
    void handleFinalTextReleased(GuideLessonState next);
    void appendSequenceCharacter(GuideLessonState next, char character);
    void appendSymCharacter(GuideLessonState next, char character);
    void appendFunctionKey(GuideLessonState next, GuideKey key);
    void completeExercise(GuideLessonState next, std::string prompt);
    void clearModifierState();
    void navigateToExercise(int exercise_index);
    void advanceExercise();
    void markCharacterDown(char character);
    void markCharacterUp(char character);
    bool inputReleased() const;
    void publish(GuideLessonState next, bool is_error);
    static char expectedSequenceCharacter(const GuideLessonState& state);
    static char expectedSymCharacter(const GuideLessonState& state);
    static GuideKey expectedFunctionKey(const GuideLessonState& state);
    char resolveFinalCharacter(const GuideLessonState& state, char character) const;
    static char expectedFinalCharacter(const GuideLessonState& state);
    static GuideTarget targetForCharacter(char character);
    static GuideTarget targetForSymCharacter(char character);
    static GuideTarget targetForFunctionKey(GuideKey key);
};

}  // namespace keyboard_guide
