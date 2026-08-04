#include "views/guide_view.hpp"

#include "assets/assets.h"
#include "assets/font_assets.hpp"

#include <lvgl/lvgl_cpp/image.hpp>
#include <lvgl/lvgl_cpp/label.hpp>
#include <lvgl/lvgl_cpp/obj.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

namespace keyboard_guide {
namespace {

constexpr int kScreenWidth  = 320;
constexpr int kScreenHeight = 170;

constexpr int kKeyY          = 45;
constexpr int kKeyHeight     = 42;
constexpr int kShiftWidth    = 72;
constexpr int kLetterWidth   = 54;
constexpr int kKeyTravel     = 6;
constexpr int kNormalKeyX    = 85;
constexpr int kHoldShiftX    = 26;
constexpr int kHoldLetterX   = 151;
constexpr int kPlusX         = 115;
constexpr int kNormalEqualX  = 149;
constexpr int kHoldEqualX    = 215;
constexpr int kNormalResultX = 187;
constexpr int kHoldResultX   = 251;
constexpr int kOperatorY     = 52;
constexpr int kResultY       = 47;
constexpr int kResultWidth   = 32;
constexpr int kResultHeight  = 37;

constexpr int kSequenceShiftX   = 28;
constexpr int kDividerX         = 121;
constexpr int kDividerY         = 48;
constexpr int kDividerWidth     = 2;
constexpr int kDividerHeight    = 34;
constexpr int kSequenceY        = 47;
constexpr int kSequenceWidth    = 32;
constexpr int kSequenceHeight   = 37;
constexpr int kFnSequenceY      = 51;
constexpr int kFnSequenceWidth  = 32;
constexpr int kFnSequenceHeight = 26;

constexpr int kOverviewShiftX = 30;
constexpr int kOverviewSymX   = 124;
constexpr int kOverviewFnX    = 218;

constexpr int kCursorY       = 89;
constexpr int kCursorSize    = 25;
constexpr int kCursorXOffset = 10;
constexpr int kPromptX       = 8;
constexpr int kPromptY       = 113;
constexpr int kPromptHeight  = 56;

constexpr int kNavInset      = 6;
constexpr int kNavY          = 2;
constexpr int kNavLabelWidth = 112;

constexpr float kHoldShiftCursorX     = 89.0f;
constexpr float kNormalKeyCursorX     = 125.0f;
constexpr float kHoldKeyCursorX       = 191.0f;
constexpr float kSequenceShiftCursorX = 91.0f;
constexpr float kTau                  = 6.28318530717958647692f;

constexpr uint32_t kBackground   = 0x000000;
constexpr uint32_t kShiftFill    = 0x990099;
constexpr uint32_t kShiftBorder  = 0xEF4FEF;
constexpr uint32_t kShiftBase    = 0x681568;
constexpr uint32_t kSymFill      = 0x336699;
constexpr uint32_t kSymBorder    = 0x72B6FB;
constexpr uint32_t kSymBase      = 0x24486C;
constexpr uint32_t kFnFill       = 0xFF6633;
constexpr uint32_t kFnBorder     = 0xFFC8B5;
constexpr uint32_t kFnBase       = 0xB94724;
constexpr uint32_t kLetterFill   = 0x676666;
constexpr uint32_t kLetterBorder = 0xAFAFAF;
constexpr uint32_t kLetterBase   = 0x4A4949;
constexpr uint32_t kOperator     = 0x565656;
constexpr uint32_t kCurrent      = 0xF5F5F5;
constexpr uint32_t kFuture       = 0x525252;
constexpr uint32_t kComplete     = 0x3FCC75;
constexpr uint32_t kPrompt       = 0xD5D5D5;
constexpr uint32_t kError        = 0xFF6B6B;
constexpr uint32_t kNavText      = 0x868686;

constexpr std::array<int, 3> kSequenceX{145, 190, 235};
constexpr std::array<float, 3> kSequenceCursorX{170.0f, 215.0f, 260.0f};
constexpr std::array<char, 3> kShiftSequenceCharacters{'A', 'B', 'C'};
constexpr std::array<char, 3> kSymSequenceCharacters{'!', '@', '#'};
constexpr std::array<int, 4> kFnSequenceX{132, 174, 216, 258};
constexpr std::array<float, 4> kFnSequenceCursorX{157.0f, 199.0f, 241.0f, 283.0f};
constexpr std::array<const char*, 4> kFnSequenceNames{
    "\xE2\x96\xB2",
    "\xE2\x96\xBC",
    "\xE2\x97\x80",
    "\xE2\x96\xB6",
};

constexpr int kFinalTextX      = 35;
constexpr int kFinalTextY      = 51;
constexpr int kFinalTextStep   = 23;
constexpr int kFinalTextWidth  = 20;
constexpr int kFinalTextHeight = 26;
constexpr std::array<char, 11> kFinalTextCharacters{'H', 'i', ',', 'M', '5', 'S', 't', 'a', 'c', 'k', '!'};

constexpr std::array<int, 8> kConfettiDx{-24, -17, -8, 7, 16, 24, -20, 20};
constexpr std::array<int, 8> kConfettiDy{-7, -20, -25, -25, -19, -6, 8, 8};
constexpr std::array<uint32_t, 6> kConfettiColors{
    0xFFD45A, 0x59D4FF, 0xFF6B9E, 0x3FCC75, 0xC77DFF, 0xFF9F43,
};
constexpr float kFinalConfettiAngleStep = 2.39996322972865332f;

using smooth_ui_toolkit::lvgl_cpp::Container;
using smooth_ui_toolkit::lvgl_cpp::Image;
using smooth_ui_toolkit::lvgl_cpp::Label;

void setupContainer(Container& container, lv_opa_t background_opacity)
{
    container.setBgOpa(background_opacity);
    container.setBorderWidth(0);
    container.setOutlineWidth(0);
    container.setShadowWidth(0);
    container.setPaddingAll(0);
    container.setScrollbarMode(LV_SCROLLBAR_MODE_OFF);
    container.removeFlag(LV_OBJ_FLAG_SCROLLABLE);
}

bool isShiftSequenceExercise(int exercise_index)
{
    return exercise_index == 2 || exercise_index == 3;
}

bool isSymExercise(int exercise_index)
{
    return exercise_index == 5;
}

bool isFnExercise(int exercise_index)
{
    return exercise_index == 6;
}

bool isFinalTextExercise(int exercise_index)
{
    return exercise_index == 7;
}

bool usesSequenceLayout(int exercise_index)
{
    return isShiftSequenceExercise(exercise_index) || isSymExercise(exercise_index) || isFnExercise(exercise_index);
}

bool isSuccessPhase(GuidePhase phase)
{
    return phase == GuidePhase::SuccessHold || phase == GuidePhase::Done;
}

size_t targetIndex(GuideTarget target)
{
    switch (target) {
        case GuideTarget::B:
        case GuideTarget::At:
            return 1;
        case GuideTarget::C:
        case GuideTarget::Hash:
            return 2;
        default:
            return 0;
    }
}

size_t fnTargetIndex(GuideTarget target)
{
    switch (target) {
        case GuideTarget::Down:
            return 1;
        case GuideTarget::Left:
            return 2;
        case GuideTarget::Right:
            return 3;
        default:
            return 0;
    }
}

}  // namespace

struct GuideView::KeyVisual {
    std::unique_ptr<Container> base;
    std::unique_ptr<Container> face;
    std::unique_ptr<Label> label;
    std::unique_ptr<Container> one_shot_dot;
    std::unique_ptr<Container> lock_shackle;
    std::unique_ptr<Container> lock_body;
    int x                     = 0;
    int y                     = 0;
    int face_offset           = 0;
    bool pressed              = false;
    bool one_shot_armed       = false;
    bool press_pulse_active   = false;
    uint32_t pulse_started_at = 0;

    KeyVisual(lv_obj_t* parent, int width, uint32_t fill, uint32_t border, uint32_t base_color, int radius,
              bool supports_lock = false)
    {
        base = std::make_unique<Container>(parent);
        base->setSize(width, kKeyHeight);
        setupContainer(*base, LV_OPA_COVER);
        base->setBgColor(lv_color_hex(base_color));
        base->setRadius(radius);

        face = std::make_unique<Container>(parent);
        face->setSize(width, kKeyHeight);
        setupContainer(*face, LV_OPA_COVER);
        face->setBgColor(lv_color_hex(fill));
        face->setBorderColor(lv_color_hex(border));
        face->setBorderWidth(2);
        face->setRadius(radius);

        label = std::make_unique<Label>(face->raw_ptr());
        label->setSize(width, lv_font_get_line_height(uiFont14()) + 3);
        label->center();
        label->setTextAlign(LV_TEXT_ALIGN_CENTER);
        label->setTextFont(uiFont14());
        label->setTextColor(lv_color_hex(kCurrent));

        if (supports_lock) {
            one_shot_dot = std::make_unique<Container>(face->raw_ptr());
            one_shot_dot->setSize(6, 6);
            one_shot_dot->setPos(width - 17, 10);
            setupContainer(*one_shot_dot, LV_OPA_COVER);
            one_shot_dot->setBgColor(lv_color_hex(kCurrent));
            one_shot_dot->setRadius(3);
            one_shot_dot->setHidden(true);

            lock_shackle = std::make_unique<Container>(face->raw_ptr());
            lock_shackle->setSize(8, 8);
            lock_shackle->setPos(width - 17, 7);
            setupContainer(*lock_shackle, LV_OPA_TRANSP);
            lock_shackle->setBorderColor(lv_color_hex(kCurrent));
            lock_shackle->setBorderWidth(2);
            lock_shackle->setRadius(4);
            lock_shackle->setHidden(true);

            lock_body = std::make_unique<Container>(face->raw_ptr());
            lock_body->setSize(10, 8);
            lock_body->setPos(width - 18, 13);
            setupContainer(*lock_body, LV_OPA_COVER);
            lock_body->setBgColor(lv_color_hex(kCurrent));
            lock_body->setRadius(2);
            lock_body->setHidden(true);
        }
    }

    void setPos(int new_x, int new_y)
    {
        x = new_x;
        y = new_y;
        base->setPos(x, y + kKeyTravel);
        face->setPos(x, y + face_offset);
    }

    void setPressed(bool is_pressed)
    {
        pressed = is_pressed;
        if (pressed) {
            press_pulse_active = false;
        }
        setFaceOffset(pressed ? kKeyTravel : (press_pulse_active ? face_offset : 0));
    }

    void setHidden(bool hidden)
    {
        base->setHidden(hidden);
        face->setHidden(hidden);
    }

    void setLocked(bool locked)
    {
        if (lock_shackle && lock_body) {
            lock_shackle->setHidden(!locked);
            lock_body->setHidden(!locked);
        }
    }

    void setOneShotArmed(bool armed, uint32_t now_ms)
    {
        if (armed && !one_shot_armed && !pressed) {
            press_pulse_active = true;
            pulse_started_at   = now_ms;
            setFaceOffset(kKeyTravel);
        }
        one_shot_armed = armed;
        if (one_shot_dot) {
            one_shot_dot->setHidden(!armed);
        }
    }

    void update(uint32_t now_ms)
    {
        if (pressed) {
            setFaceOffset(kKeyTravel);
            return;
        }
        if (!press_pulse_active) {
            setFaceOffset(0);
            return;
        }

        constexpr uint32_t kPressedDurationMs = 120;
        constexpr uint32_t kReleaseDurationMs = 180;
        constexpr uint32_t kPulseDurationMs   = kPressedDurationMs + kReleaseDurationMs;
        const uint32_t elapsed                = now_ms - pulse_started_at;
        if (elapsed >= kPulseDurationMs) {
            press_pulse_active = false;
            setFaceOffset(0);
            return;
        }

        float travel = 1.0f;
        if (elapsed >= kPressedDurationMs) {
            const float progress =
                static_cast<float>(elapsed - kPressedDurationMs) / static_cast<float>(kReleaseDurationMs);
            travel = 1.0f - smooth_ui_toolkit::ease::ease_out_quad(progress);
        }
        setFaceOffset(static_cast<int>(std::lround(travel * static_cast<float>(kKeyTravel))));
    }

    void setText(char text)
    {
        label->setText(std::string(1, text));
    }

    void setText(const char* text)
    {
        label->setText(text);
    }

private:
    void setFaceOffset(int offset)
    {
        if (face_offset == offset) {
            return;
        }
        face_offset = offset;
        face->setY(y + face_offset);
    }
};

GuideView::GuideView(GuideViewModel& view_model) : _view_model(view_model)
{
}

GuideView::~GuideView()
{
    onExit();
}

void GuideView::onEnter(lv_obj_t* parent)
{
    onExit();

    _root = std::make_unique<Container>(parent);
    _root->setSize(kScreenWidth, kScreenHeight);
    _root->setPos(0, 0);
    _root->setBgColor(lv_color_hex(kBackground));
    _root->setRadius(0);
    setupContainer(*_root, LV_OPA_COVER);

    _skip_label = std::make_unique<Label>(_root->raw_ptr());
    _skip_label->setSize(kNavLabelWidth, lv_font_get_line_height(uiFont10()) + 2);
    _skip_label->setPos(kScreenWidth - kNavInset - kNavLabelWidth, kNavY);
    _skip_label->setTextAlign(LV_TEXT_ALIGN_RIGHT);
    _skip_label->setTextFont(uiFont10());
    _skip_label->setTextColor(lv_color_hex(kNavText));
    _skip_label->setText("Skip: Enter >");

    _intro_title = std::make_unique<Label>(_root->raw_ptr());
    _intro_title->setSize(220, lv_font_get_line_height(&lv_font_montserrat_20) + 4);
    _intro_title->setPos(50, 12);
    _intro_title->setTextAlign(LV_TEXT_ALIGN_CENTER);
    _intro_title->setTextFont(&lv_font_montserrat_20);
    _intro_title->setTextColor(lv_color_hex(kCurrent));
    _intro_title->setText("Keyboard Guide");

    _shift_key = std::make_unique<KeyVisual>(_root->raw_ptr(), kShiftWidth, kShiftFill, kShiftBorder, kShiftBase,
                                             kKeyHeight / 2, true);
    _shift_key->setPos(kHoldShiftX, kKeyY);
    _shift_key->setText("Shift");

    _sym_key = std::make_unique<KeyVisual>(_root->raw_ptr(), kShiftWidth, kSymFill, kSymBorder, kSymBase,
                                           kKeyHeight / 2, true);
    _sym_key->setPos(kOverviewSymX, kKeyY);
    _sym_key->setText("Sym");

    _fn_key =
        std::make_unique<KeyVisual>(_root->raw_ptr(), kShiftWidth, kFnFill, kFnBorder, kFnBase, kKeyHeight / 2, true);
    _fn_key->setPos(kOverviewFnX, kKeyY);
    _fn_key->setText("Fn");

    _letter_key = std::make_unique<KeyVisual>(_root->raw_ptr(), kLetterWidth, kLetterFill, kLetterBorder, kLetterBase,
                                              kKeyHeight / 2);
    _letter_key->setPos(kNormalKeyX, kKeyY);
    _letter_key->setText('A');

    _plus_label = std::make_unique<Label>(_root->raw_ptr());
    _plus_label->setSize(22, lv_font_get_line_height(&lv_font_montserrat_20) + 4);
    _plus_label->setPos(kPlusX, kOperatorY);
    _plus_label->setTextAlign(LV_TEXT_ALIGN_CENTER);
    _plus_label->setTextFont(&lv_font_montserrat_20);
    _plus_label->setTextColor(lv_color_hex(kOperator));
    _plus_label->setText("+");

    _equals_label = std::make_unique<Label>(_root->raw_ptr());
    _equals_label->setSize(22, lv_font_get_line_height(&lv_font_montserrat_20) + 4);
    _equals_label->setPos(kNormalEqualX, kOperatorY);
    _equals_label->setTextAlign(LV_TEXT_ALIGN_CENTER);
    _equals_label->setTextFont(&lv_font_montserrat_20);
    _equals_label->setTextColor(lv_color_hex(kOperator));
    _equals_label->setText("=");

    _result_label = std::make_unique<Label>(_root->raw_ptr());
    _result_label->setSize(kResultWidth, lv_font_get_line_height(&lv_font_montserrat_30) + 4);
    _result_label->setPos(kNormalResultX, kResultY);
    _result_label->setTextAlign(LV_TEXT_ALIGN_CENTER);
    _result_label->setTextFont(&lv_font_montserrat_30);
    _result_label->setTransformPivot(kResultWidth / 2, kResultHeight / 2);

    _divider = std::make_unique<Container>(_root->raw_ptr());
    _divider->setSize(kDividerWidth, kDividerHeight);
    _divider->setPos(kDividerX, kDividerY);
    setupContainer(*_divider, LV_OPA_COVER);
    _divider->setBgColor(lv_color_hex(kOperator));
    _divider->setRadius(1);

    for (size_t index = 0; index < _sequence_labels.size(); ++index) {
        auto& label = _sequence_labels[index];
        label       = std::make_unique<Label>(_root->raw_ptr());
        label->setSize(kSequenceWidth, lv_font_get_line_height(&lv_font_montserrat_30) + 4);
        label->setPos(kSequenceX[index], kSequenceY);
        label->setTextAlign(LV_TEXT_ALIGN_CENTER);
        label->setTextFont(&lv_font_montserrat_30);
        label->setText(std::string(1, kShiftSequenceCharacters[index]));
        label->setTransformPivot(kSequenceWidth / 2, kSequenceHeight / 2);
    }

    for (size_t index = 0; index < _fn_sequence_labels.size(); ++index) {
        auto& label = _fn_sequence_labels[index];
        label       = std::make_unique<Label>(_root->raw_ptr());
        label->setSize(kFnSequenceWidth, kFnSequenceHeight);
        label->setPos(kFnSequenceX[index], kFnSequenceY);
        label->setTextAlign(LV_TEXT_ALIGN_CENTER);
        label->setTextFont(uiSymbolFont20());
        label->setText(kFnSequenceNames[index]);
        label->setTransformPivot(kFnSequenceWidth / 2, kFnSequenceHeight / 2);
    }

    for (size_t index = 0; index < _final_text_labels.size(); ++index) {
        auto& label = _final_text_labels[index];
        label       = std::make_unique<Label>(_root->raw_ptr());
        label->setSize(kFinalTextWidth, kFinalTextHeight);
        label->setPos(kFinalTextX + static_cast<int>(index) * kFinalTextStep, kFinalTextY);
        label->setTextAlign(LV_TEXT_ALIGN_CENTER);
        label->setTextFont(&lv_font_montserrat_20);
        label->setText(std::string(1, kFinalTextCharacters[index]));
        label->setTransformPivot(kFinalTextWidth / 2, kFinalTextHeight / 2);
    }

    for (size_t index = 0; index < _confetti.size(); ++index) {
        auto& piece = _confetti[index];
        piece       = std::make_unique<Container>(_root->raw_ptr());
        piece->setSize(index % 2 == 0 ? 5 : 3, index % 2 == 0 ? 3 : 6);
        setupContainer(*piece, LV_OPA_COVER);
        piece->setBgColor(lv_color_hex(kConfettiColors[index % kConfettiColors.size()]));
        piece->setRadius(1);
        piece->setHidden(true);
    }
    for (size_t index = 0; index < _final_confetti.size(); ++index) {
        auto& piece = _final_confetti[index];
        piece       = std::make_unique<Container>(_root->raw_ptr());
        piece->setSize(index % 3 == 0 ? 6 : 3, index % 3 == 0 ? 3 : 7);
        setupContainer(*piece, LV_OPA_COVER);
        piece->setBgColor(lv_color_hex(kConfettiColors[index % kConfettiColors.size()]));
        piece->setRadius(1);
        piece->setHidden(true);
    }

    _cursor = std::make_unique<Image>(_root->raw_ptr());
    _cursor->setSrc(&image_cursor_hover);
    _cursor->setSize(kCursorSize, kCursorSize);

    _prompt_label = std::make_unique<Label>(_root->raw_ptr());
    _prompt_label->setSize(kScreenWidth - 16, kPromptHeight);
    _prompt_label->setPos(kPromptX, kPromptY);
    _prompt_label->setLongMode(LV_LABEL_LONG_WRAP);
    _prompt_label->setTextAlign(LV_TEXT_ALIGN_CENTER);
    _prompt_label->setTextFont(uiFont14());

    _cursor_x.stop();
    _cursor_x.springOptions().visualDuration = 0.45f;
    _cursor_x.springOptions().bounce         = 0.18f;
    _cursor_x.teleport(kNormalKeyCursorX);
    _cursor_x.begin();

    _cursor_bob.start                          = 0.0f;
    _cursor_bob.end                            = 1.0f;
    _cursor_bob.repeat                         = -1;
    _cursor_bob.repeatType                     = smooth_ui_toolkit::AnimateRepeatType::Reverse;
    _cursor_bob.springOptions().visualDuration = 0.58f;
    _cursor_bob.springOptions().bounce         = 0.28f;
    _cursor_bob.onUpdate([this](float value) { _cursor_bob_value = value; });
    _cursor_bob.init();
    _cursor_bob.play();

    _result_pop.start                          = 0.0f;
    _result_pop.end                            = 1.0f;
    _result_pop.easingOptions().duration       = 0.72f;
    _result_pop.easingOptions().easingFunction = smooth_ui_toolkit::ease::linear;
    _result_pop.onUpdate([this](float value) { _result_pop_progress = value; });
    _result_pop.onComplete([this]() {
        _result_pop_progress = 1.0f;
        _result_pop_active   = false;
    });
    _result_pop.init();
    _result_pop.cancel();
    _result_pop_progress = 1.0f;
    _result_pop_active   = false;
    _whole_text_pop      = false;

    const GuideLessonState& state = _view_model.state().get();
    _shown_result_revision        = state.result_revision;
    render(state);
    _view_model.state().observe(this, onStateChanged);
}

void GuideView::onExit()
{
    _view_model.state().removeObserver();
    _cursor_x.stop();
    _cursor_bob.cancel();
    _result_pop.cancel();
    resetPopTarget();
    _result_pop_progress      = 1.0f;
    _result_pop_active        = false;
    _whole_text_pop           = false;
    _shown_result_revision    = 0;
    _shown_attention_revision = 0;
    _shake_started_at         = 0;
    _prompt_label.reset();
    _cursor.reset();
    for (auto& piece : _confetti) {
        piece.reset();
    }
    for (auto& piece : _final_confetti) {
        piece.reset();
    }
    for (auto& label : _final_text_labels) {
        label.reset();
    }
    for (auto& label : _fn_sequence_labels) {
        label.reset();
    }
    for (auto& label : _sequence_labels) {
        label.reset();
    }
    _divider.reset();
    _result_label.reset();
    _equals_label.reset();
    _plus_label.reset();
    _letter_key.reset();
    _fn_key.reset();
    _sym_key.reset();
    _shift_key.reset();
    _intro_title.reset();
    _skip_label.reset();
    _root.reset();
}

void GuideView::tick(uint32_t now_ms)
{
    _shift_key->update(now_ms);
    _sym_key->update(now_ms);
    _fn_key->update(now_ms);
    _letter_key->update(now_ms);

    const float now_seconds = static_cast<float>(now_ms) / 1000.0f;
    _cursor_bob.update(now_seconds);
    if (_result_pop_active) {
        _result_pop.update(now_seconds);
    }
    applyTransforms(now_ms);
}

void GuideView::render(const GuideLessonState& state)
{
    const bool intro          = state.phase == GuidePhase::Intro;
    const bool shift_sequence = isShiftSequenceExercise(state.exercise_index);
    const bool sym_exercise   = isSymExercise(state.exercise_index);
    const bool fn_exercise    = isFnExercise(state.exercise_index);
    const bool final_text     = isFinalTextExercise(state.exercise_index);
    const bool sequence       = usesSequenceLayout(state.exercise_index);
    const bool overview       = state.exercise_index == 4;
    const bool hold           = state.exercise_index == 1;
    const bool success        = isSuccessPhase(state.phase);

    _skip_label->setHidden(intro);
    _intro_title->setHidden(!intro);

    _shift_key->setHidden(!(intro || overview || hold || shift_sequence));
    _shift_key->setPos(intro || overview ? kOverviewShiftX : (shift_sequence ? kSequenceShiftX : kHoldShiftX), kKeyY);
    _shift_key->setPressed(!intro && !overview && (state.shift_pressed || state.shift_locked));
    _shift_key->setLocked(!intro && !overview && state.shift_locked);
    _shift_key->setOneShotArmed(
        !intro && !overview && state.phase == GuidePhase::OneShotAwaitLetter && !state.shift_locked, lv_tick_get());

    _sym_key->setHidden(!(intro || overview || sym_exercise));
    _sym_key->setPos(intro || overview ? kOverviewSymX : kSequenceShiftX, kKeyY);
    _sym_key->setPressed(sym_exercise && (state.sym_pressed || state.sym_locked));
    _sym_key->setLocked(sym_exercise && state.sym_locked);
    _sym_key->setOneShotArmed(sym_exercise && state.sym_one_shot && !state.sym_locked, lv_tick_get());

    _fn_key->setHidden(!(intro || overview || fn_exercise));
    _fn_key->setPos(intro || overview ? kOverviewFnX : kSequenceShiftX, kKeyY);
    _fn_key->setPressed(fn_exercise && (state.fn_pressed || state.fn_locked));
    _fn_key->setLocked(fn_exercise && state.fn_locked);
    _fn_key->setOneShotArmed(fn_exercise && state.fn_one_shot && !state.fn_locked, lv_tick_get());

    _letter_key->setHidden(intro || sequence || overview || final_text);
    _letter_key->setPos(hold ? kHoldLetterX : kNormalKeyX, kKeyY);
    _letter_key->setPressed(state.character_pressed == 'A');

    _plus_label->setHidden(!hold);
    _equals_label->setHidden(intro || sequence || overview || final_text);
    _equals_label->setX(hold ? kHoldEqualX : kNormalEqualX);
    _result_label->setHidden(intro || sequence || overview || final_text);
    _result_label->setX(hold ? kHoldResultX : kNormalResultX);
    _result_label->setText(hold ? "A" : "a");
    _result_label->setTextColor(lv_color_hex(state.typed_text.empty() ? kFuture : kComplete));

    _divider->setHidden(!sequence);
    for (size_t index = 0; index < _sequence_labels.size(); ++index) {
        auto& label = _sequence_labels[index];
        label->setHidden(!(shift_sequence || sym_exercise));
        label->setText(std::string(1, sym_exercise ? kSymSequenceCharacters[index] : kShiftSequenceCharacters[index]));
        const bool complete = index < state.typed_text.size();
        const bool current = index == state.typed_text.size() && !success && state.phase != GuidePhase::LockAwaitUnlock;
        const uint32_t complete_color = sym_exercise ? kSymBorder : kComplete;
        label->setTextColor(lv_color_hex(complete ? complete_color : (current ? kCurrent : kFuture)));
    }
    for (size_t index = 0; index < _fn_sequence_labels.size(); ++index) {
        auto& label         = _fn_sequence_labels[index];
        const bool complete = index < state.typed_text.size();
        const bool current = index == state.typed_text.size() && !success && state.phase != GuidePhase::LockAwaitUnlock;
        label->setHidden(!fn_exercise);
        label->setTextColor(lv_color_hex(complete ? kFnFill : (current ? kCurrent : kFuture)));
    }
    for (size_t index = 0; index < _final_text_labels.size(); ++index) {
        auto& label         = _final_text_labels[index];
        const bool complete = index < state.typed_text.size();
        const bool current  = index == state.typed_text.size() && !success;
        label->setHidden(!final_text);
        label->setTextColor(lv_color_hex(complete ? kComplete : (current ? kCurrent : kFuture)));
    }

    const int prompt_line_count  = 1 + static_cast<int>(std::count(state.prompt.begin(), state.prompt.end(), '\n'));
    const int prompt_text_height = lv_font_get_line_height(uiFont14()) * prompt_line_count;
    _prompt_label->setHeight(prompt_text_height);
    _prompt_label->setY(kPromptY + (kPromptHeight - prompt_text_height) / 2);
    _prompt_label->setText(state.prompt);
    _prompt_label->setTextColor(lv_color_hex(state.last_action_error ? kError : (success ? kComplete : kPrompt)));

    _cursor->setHidden(intro || overview || (final_text && success));
    if (!intro && !overview && !(final_text && success)) {
        _cursor_x.move(cursorTargetX(state));
    }

    if (_shown_attention_revision != state.attention_revision) {
        _shown_attention_revision = state.attention_revision;
        if (state.last_action_error) {
            _shake_started_at = lv_tick_get();
        }
    }

    if (_shown_result_revision != state.result_revision) {
        _shown_result_revision = state.result_revision;
        if (final_text && state.typed_text.size() == _final_text_labels.size() && success) {
            playFinalTextPop();
        } else if (final_text && !state.typed_text.empty()) {
            const size_t index = std::min(state.typed_text.size() - 1, _final_text_labels.size() - 1);
            playResultPop(*_final_text_labels[index], kFinalTextX + static_cast<int>(index) * kFinalTextStep,
                          kFinalTextY, kFinalTextWidth, kFinalTextHeight);
        } else if (fn_exercise && !state.typed_text.empty()) {
            const size_t index = std::min(state.typed_text.size() - 1, _fn_sequence_labels.size() - 1);
            playResultPop(*_fn_sequence_labels[index], kFnSequenceX[index], kFnSequenceY, kFnSequenceWidth,
                          kFnSequenceHeight);
        } else if (sequence && !state.typed_text.empty()) {
            const size_t index = std::min(state.typed_text.size() - 1, _sequence_labels.size() - 1);
            playResultPop(*_sequence_labels[index], kSequenceX[index], kSequenceY, kSequenceWidth, kSequenceHeight);
        } else if (!sequence) {
            playResultPop(*_result_label, hold ? kHoldResultX : kNormalResultX, kResultY, kResultWidth, kResultHeight);
        }
    }
}

void GuideView::applyTransforms(uint32_t now_ms)
{
    int shake_offset             = 0;
    const uint32_t shake_elapsed = now_ms - _shake_started_at;
    if (_shake_started_at != 0 && shake_elapsed < 320) {
        const float progress = static_cast<float>(shake_elapsed) / 320.0f;
        shake_offset = static_cast<int>(std::lround(std::sin(progress * kTau * 3.0f) * (1.0f - progress) * 4.0f));
    }

    const bool final_text = isFinalTextExercise(_view_model.state().get().exercise_index);
    const int bob_offset  = static_cast<int>(std::lround(_cursor_bob_value * 4.0f));
    const int cursor_x =
        static_cast<int>(std::lround(_cursor_x.value())) + (final_text ? 0 : kCursorXOffset) - bob_offset;
    _cursor->setPos(cursor_x, kCursorY - (final_text ? 6 : 0) - bob_offset);
    lv_obj_move_foreground(_cursor->raw_ptr());
    _prompt_label->setX(kPromptX + shake_offset);

    if (!_pop_label && !_whole_text_pop) {
        for (auto& piece : _confetti) {
            piece->setHidden(true);
        }
        for (auto& piece : _final_confetti) {
            piece->setHidden(true);
        }
        return;
    }

    const float pop_progress = _result_pop_active ? std::clamp(_result_pop_progress, 0.0f, 1.0f) : 1.0f;
    float result_scale       = 1.0f;
    if (pop_progress < 0.28f) {
        const float progress = pop_progress / 0.28f;
        result_scale         = 0.62f + 0.60f * smooth_ui_toolkit::ease::ease_out_back(progress);
    } else if (pop_progress < 0.56f) {
        const float progress = (pop_progress - 0.28f) / 0.28f;
        result_scale         = 1.22f - 0.28f * smooth_ui_toolkit::ease::ease_out_quad(progress);
    } else {
        const float progress = (pop_progress - 0.56f) / 0.44f;
        result_scale         = 0.94f + 0.06f * smooth_ui_toolkit::ease::ease_out_back(progress);
    }
    const int scale = static_cast<int>(std::lround(result_scale * 256.0f));
    const int lift  = static_cast<int>(std::lround(std::sin(pop_progress * kTau * 0.5f) * 4.0f));
    if (_whole_text_pop) {
        for (auto& label : _final_text_labels) {
            lv_obj_set_style_transform_scale_x(label->raw_ptr(), scale, LV_PART_MAIN);
            lv_obj_set_style_transform_scale_y(label->raw_ptr(), scale, LV_PART_MAIN);
            label->setY(kFinalTextY - lift);
            lv_obj_move_foreground(label->raw_ptr());
        }
    } else {
        lv_obj_set_style_transform_scale_x(_pop_label->raw_ptr(), scale, LV_PART_MAIN);
        lv_obj_set_style_transform_scale_y(_pop_label->raw_ptr(), scale, LV_PART_MAIN);
        _pop_label->setY(_pop_y - lift);
        lv_obj_move_foreground(_pop_label->raw_ptr());
    }

    const bool show_confetti = _result_pop_active && pop_progress > 0.03f && pop_progress < 0.94f;
    const float burst        = std::sin(std::min(pop_progress / 0.72f, 1.0f) * kTau * 0.25f);
    const float fade         = 1.0f - std::clamp((pop_progress - 0.42f) / 0.52f, 0.0f, 1.0f);
    const int origin_x       = _pop_x + _pop_width / 2;
    const int origin_y       = _pop_y + _pop_height / 2;
    for (size_t index = 0; index < _confetti.size(); ++index) {
        auto& piece = _confetti[index];
        piece->setHidden(!show_confetti || _whole_text_pop);
        if (!show_confetti || _whole_text_pop) {
            continue;
        }
        const int x = origin_x + static_cast<int>(std::lround(static_cast<float>(kConfettiDx[index]) * burst));
        const int y = origin_y + static_cast<int>(std::lround(static_cast<float>(kConfettiDy[index]) * burst +
                                                              pop_progress * pop_progress * 14.0f));
        piece->setPos(x, y);
        piece->setOpa(static_cast<lv_opa_t>(std::lround(fade * 255.0f)));
        piece->setRotation(static_cast<int32_t>((static_cast<float>(index) * 35.0f + pop_progress * 220.0f) * 10.0f));
    }
    for (size_t index = 0; index < _final_confetti.size(); ++index) {
        auto& piece = _final_confetti[index];
        piece->setHidden(!show_confetti || !_whole_text_pop);
        if (!show_confetti || !_whole_text_pop) {
            continue;
        }

        const float angle  = static_cast<float>(index) * kFinalConfettiAngleStep;
        const float radius = 52.0f + static_cast<float>(index % 8) * 10.0f;
        const int x        = kScreenWidth / 2 + static_cast<int>(std::lround(std::cos(angle) * radius * burst));
        const int y        = kFinalTextY + kFinalTextHeight / 2 +
                      static_cast<int>(
                          std::lround(std::sin(angle) * radius * 0.62f * burst + pop_progress * pop_progress * 16.0f));
        piece->setPos(x, y);
        piece->setOpa(static_cast<lv_opa_t>(std::lround(fade * 255.0f)));
        piece->setRotation(static_cast<int32_t>((static_cast<float>(index) * 47.0f + pop_progress * 260.0f) * 10.0f));
    }
}

void GuideView::playResultPop(Label& label, int x, int y, int width, int height)
{
    _result_pop.cancel();
    resetPopTarget();
    _pop_label           = &label;
    _pop_x               = x;
    _pop_y               = y;
    _pop_width           = width;
    _pop_height          = height;
    _result_pop_progress = 0.0f;
    _result_pop_active   = true;
    _result_pop.start    = 0.0f;
    _result_pop.end      = 1.0f;
    _result_pop.init();
    _result_pop.play();
}

void GuideView::playFinalTextPop()
{
    _result_pop.cancel();
    resetPopTarget();
    _whole_text_pop      = true;
    _result_pop_progress = 0.0f;
    _result_pop_active   = true;
    _result_pop.start    = 0.0f;
    _result_pop.end      = 1.0f;
    _result_pop.init();
    _result_pop.play();
}

void GuideView::resetPopTarget()
{
    if (_whole_text_pop) {
        for (auto& label : _final_text_labels) {
            if (label && label->isValid()) {
                lv_obj_set_style_transform_scale_x(label->raw_ptr(), 256, LV_PART_MAIN);
                lv_obj_set_style_transform_scale_y(label->raw_ptr(), 256, LV_PART_MAIN);
                label->setY(kFinalTextY);
            }
        }
        _whole_text_pop = false;
    }
    if (_pop_label) {
        if (_pop_label->isValid()) {
            lv_obj_set_style_transform_scale_x(_pop_label->raw_ptr(), 256, LV_PART_MAIN);
            lv_obj_set_style_transform_scale_y(_pop_label->raw_ptr(), 256, LV_PART_MAIN);
            _pop_label->setY(_pop_y);
        }
        _pop_label = nullptr;
    }
    for (auto& piece : _confetti) {
        if (piece && piece->isValid()) {
            piece->setHidden(true);
        }
    }
    for (auto& piece : _final_confetti) {
        if (piece && piece->isValid()) {
            piece->setHidden(true);
        }
    }
}

float GuideView::cursorTargetX(const GuideLessonState& state)
{
    if (isFinalTextExercise(state.exercise_index)) {
        const size_t index = std::min(state.typed_text.size(), kFinalTextCharacters.size() - 1);
        const float target = static_cast<float>(kFinalTextX + static_cast<int>(index) * kFinalTextStep + 25);
        return std::min(target, static_cast<float>(kScreenWidth - kCursorSize - kCursorXOffset));
    }
    if (isFnExercise(state.exercise_index)) {
        if (state.cursor_target == GuideTarget::Fn) {
            return kSequenceShiftCursorX;
        }
        return kFnSequenceCursorX[fnTargetIndex(state.cursor_target)];
    }
    if (usesSequenceLayout(state.exercise_index)) {
        if (state.cursor_target == GuideTarget::Shift) {
            return kSequenceShiftCursorX;
        }
        return kSequenceCursorX[targetIndex(state.cursor_target)];
    }
    if (state.cursor_target == GuideTarget::Shift) {
        return kHoldShiftCursorX;
    }
    return state.exercise_index == 1 ? kHoldKeyCursorX : kNormalKeyCursorX;
}

void GuideView::onStateChanged(void* context, const GuideLessonState& state)
{
    auto* view = static_cast<GuideView*>(context);
    if (view) {
        view->render(state);
    }
}

}  // namespace keyboard_guide
