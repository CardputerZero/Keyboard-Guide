#include "views/guide_view.hpp"

#include "assets/assets.h"
#include "assets/font_assets.hpp"

#include <lvgl/lvgl_cpp/image.hpp>
#include <lvgl/lvgl_cpp/label.hpp>
#include <lvgl/lvgl_cpp/line.hpp>
#include <lvgl/lvgl_cpp/obj.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

namespace keyboard_guide {
namespace {

constexpr int kScreenWidth   = 320;
constexpr int kScreenHeight  = 170;
constexpr int kKeyY          = 28;
constexpr int kKeyHeight     = 40;
constexpr int kShiftX        = 16;
constexpr int kShiftWidth    = 72;
constexpr int kArrowX        = 106;
constexpr int kArrowY        = 42;
constexpr int kArrowWidth    = 22;
constexpr int kArrowHeight   = 12;
constexpr int kLetterWidth   = 44;
constexpr int kNormalKeyX    = 90;
constexpr int kShiftedKeyX   = 146;
constexpr int kNormalEqualX  = 149;
constexpr int kShiftEqualX   = 205;
constexpr int kNormalResultX = 187;
constexpr int kShiftResultX  = 241;
constexpr int kResultWidth   = 32;
constexpr int kOperatorY     = 35;
constexpr int kResultY       = 30;
constexpr int kResultHeight  = 37;
constexpr int kCursorY       = 80;
constexpr int kCursorSize    = 25;
constexpr int kPromptX       = 8;
constexpr int kPromptY       = 132;
constexpr int kKeyTravel     = 3;

constexpr float kShiftCursorX      = 79.0f;
constexpr float kNormalKeyCursorX  = 125.0f;
constexpr float kShiftedKeyCursorX = 181.0f;
constexpr float kTau               = 6.28318530717958647692f;

constexpr uint32_t kBackground   = 0x000000;
constexpr uint32_t kShiftFill    = 0x990099;
constexpr uint32_t kShiftBorder  = 0xEF4FEF;
constexpr uint32_t kShiftBase    = 0x510051;
constexpr uint32_t kLetterFill   = 0x676666;
constexpr uint32_t kLetterBorder = 0xAFAFAF;
constexpr uint32_t kLetterBase   = 0x383737;
constexpr uint32_t kArrow        = 0x565656;
constexpr uint32_t kCurrent      = 0xF5F5F5;
constexpr uint32_t kFuture       = 0x525252;
constexpr uint32_t kComplete     = 0x3FCC75;
constexpr uint32_t kPrompt       = 0xA7A7A7;
constexpr uint32_t kError        = 0xFF6B6B;

constexpr std::array<lv_point_precise_t, 5> kArrowPoints{
    lv_point_precise_t{0, 6},  lv_point_precise_t{20, 6},  lv_point_precise_t{15, 1},
    lv_point_precise_t{20, 6}, lv_point_precise_t{15, 11},
};

constexpr std::array<int, 8> kConfettiDx{-24, -17, -8, 7, 16, 24, -20, 20};
constexpr std::array<int, 8> kConfettiDy{-7, -20, -25, -25, -19, -6, 8, 8};
constexpr std::array<uint32_t, 8> kConfettiColors{
    0xFFD45A, 0x59D4FF, 0xFF6B9E, 0x3FCC75, 0xC77DFF, 0xFF9F43, 0x59D4FF, 0xFFD45A,
};

using smooth_ui_toolkit::lvgl_cpp::Container;
using smooth_ui_toolkit::lvgl_cpp::Image;
using smooth_ui_toolkit::lvgl_cpp::Label;
using smooth_ui_toolkit::lvgl_cpp::Line;

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

bool requiresShift(int exercise_index)
{
    return (exercise_index % 2) == 1;
}

char exerciseKey(int exercise_index)
{
    return exercise_index < 2 ? 'A' : 'B';
}

char exerciseResult(int exercise_index)
{
    const char key = exerciseKey(exercise_index);
    return requiresShift(exercise_index) ? key : static_cast<char>(key - 'A' + 'a');
}

}  // namespace

struct GuideView::KeyVisual {
    std::unique_ptr<Container> base;
    std::unique_ptr<Container> face;
    std::unique_ptr<Label> label;
    int x        = 0;
    int y        = 0;
    bool pressed = false;

    KeyVisual(lv_obj_t* parent, int width, uint32_t fill, uint32_t border, uint32_t base_color, int radius)
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
    }

    void setPos(int new_x, int new_y)
    {
        x = new_x;
        y = new_y;
        base->setPos(x, y + kKeyTravel);
        face->setPos(x, y + (pressed ? kKeyTravel : 0));
    }

    void setPressed(bool is_pressed)
    {
        pressed = is_pressed;
        face->setY(y + (pressed ? kKeyTravel : 0));
    }

    void setHidden(bool hidden)
    {
        base->setHidden(hidden);
        face->setHidden(hidden);
    }

    void setText(char text)
    {
        label->setText(std::string(1, text));
    }

    void setText(const char* text)
    {
        label->setText(text);
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

    _shift_key = std::make_unique<KeyVisual>(_root->raw_ptr(), kShiftWidth, kShiftFill, kShiftBorder, kShiftBase,
                                             kKeyHeight / 2);
    _shift_key->setPos(kShiftX, kKeyY);
    _shift_key->setText("Shift");

    _arrow = std::make_unique<Line>(_root->raw_ptr());
    _arrow->setSize(kArrowWidth, kArrowHeight);
    _arrow->setPos(kArrowX, kArrowY);
    _arrow->setPoints(kArrowPoints.data(), static_cast<uint32_t>(kArrowPoints.size()));
    _arrow->setLineColor(lv_color_hex(kArrow));
    _arrow->setLineWidth(2);
    _arrow->setLineRounded(true);

    _letter_key =
        std::make_unique<KeyVisual>(_root->raw_ptr(), kLetterWidth, kLetterFill, kLetterBorder, kLetterBase, 8);

    _equals_label = std::make_unique<Label>(_root->raw_ptr());
    _equals_label->setSize(22, lv_font_get_line_height(&lv_font_montserrat_20) + 4);
    _equals_label->setPos(kNormalEqualX, kOperatorY);
    _equals_label->setTextAlign(LV_TEXT_ALIGN_CENTER);
    _equals_label->setTextFont(&lv_font_montserrat_20);
    _equals_label->setTextColor(lv_color_hex(kArrow));
    _equals_label->setText("=");

    _result_label = std::make_unique<Label>(_root->raw_ptr());
    _result_label->setSize(kResultWidth, lv_font_get_line_height(&lv_font_montserrat_30) + 4);
    _result_label->setPos(kNormalResultX, kResultY);
    _result_label->setTextAlign(LV_TEXT_ALIGN_CENTER);
    _result_label->setTextFont(&lv_font_montserrat_30);
    _result_label->setTransformPivot(kResultWidth / 2, kResultHeight / 2);

    for (size_t index = 0; index < _confetti.size(); ++index) {
        auto& piece = _confetti[index];
        piece       = std::make_unique<Container>(_root->raw_ptr());
        piece->setSize(index % 2 == 0 ? 3 : 2, index % 2 == 0 ? 2 : 4);
        setupContainer(*piece, LV_OPA_COVER);
        piece->setBgColor(lv_color_hex(kConfettiColors[index]));
        piece->setRadius(1);
        piece->setHidden(true);
    }

    _cursor = std::make_unique<Image>(_root->raw_ptr());
    _cursor->setSrc(&image_cursor_hover);
    _cursor->setSize(kCursorSize, kCursorSize);

    _prompt_label = std::make_unique<Label>(_root->raw_ptr());
    _prompt_label->setSize(kScreenWidth - 16, lv_font_get_line_height(uiFont12()) + 3);
    _prompt_label->setPos(kPromptX, kPromptY);
    _prompt_label->setLongMode(LV_LABEL_LONG_CLIP);
    _prompt_label->setTextAlign(LV_TEXT_ALIGN_CENTER);
    _prompt_label->setTextFont(uiFont12());

    _cursor_x.stop();
    _cursor_x.springOptions().visualDuration = 0.45f;
    _cursor_x.springOptions().bounce         = 0.35f;
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

    render(_view_model.state().get());
    _view_model.state().observe(this, onStateChanged);
}

void GuideView::onExit()
{
    _view_model.state().removeObserver();
    _cursor_x.stop();
    _cursor_bob.cancel();
    _result_pop.cancel();
    _result_pop_progress = 1.0f;
    _result_pop_active   = false;
    _prompt_label.reset();
    _cursor.reset();
    for (auto& piece : _confetti) {
        piece.reset();
    }
    _result_label.reset();
    _equals_label.reset();
    _letter_key.reset();
    _arrow.reset();
    _shift_key.reset();
    _root.reset();
}

void GuideView::tick(uint32_t now_ms)
{
    const float now_seconds = static_cast<float>(now_ms) / 1000.0f;
    _cursor_bob.update(now_seconds);
    if (_result_pop_active) {
        _result_pop.update(now_seconds);
    }
    applyTransforms(now_ms);
}

void GuideView::render(const GuideLessonState& state)
{
    const bool success_started = state.exercise_complete && (!_shown_state.exercise_complete ||
                                                             _shown_state.exercise_index != state.exercise_index);
    _shown_state               = state;

    const bool with_shift = requiresShift(state.exercise_index);
    const bool success    = state.exercise_complete || state.completed;
    const int key_x       = with_shift ? kShiftedKeyX : kNormalKeyX;
    const int equal_x     = with_shift ? kShiftEqualX : kNormalEqualX;
    const int result_x    = with_shift ? kShiftResultX : kNormalResultX;

    _shift_key->setHidden(!with_shift);
    _shift_key->setPressed(state.modifier_pressed);
    _arrow->setHidden(!with_shift);
    _letter_key->setPos(key_x, kKeyY);
    _letter_key->setPressed(state.character_pressed);
    _letter_key->setText(exerciseKey(state.exercise_index));
    _equals_label->setX(equal_x);
    _result_label->setX(result_x);
    _result_label->setText(std::string(1, exerciseResult(state.exercise_index)));
    _result_label->setTextColor(lv_color_hex(success ? kComplete : kFuture));

    _prompt_label->setText(state.prompt);
    _prompt_label->setTextColor(lv_color_hex(state.last_action_error ? kError : (success ? kComplete : kPrompt)));

    _cursor->setHidden(false);
    _cursor_x.move(cursorTargetX(state));

    if (_shown_attention_revision != state.attention_revision) {
        _shown_attention_revision = state.attention_revision;
        if (state.last_action_error) {
            _shake_started_at = lv_tick_get();
        }
    }

    if (success_started) {
        playResultPop();
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

    const int bob_offset = static_cast<int>(std::lround(_cursor_bob_value * 4.0f));
    const int cursor_x   = static_cast<int>(std::lround(_cursor_x.value())) - bob_offset;
    _cursor->setPos(cursor_x, kCursorY - bob_offset);
    lv_obj_move_foreground(_cursor->raw_ptr());
    _prompt_label->setX(kPromptX + shake_offset);

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
    lv_obj_set_style_transform_scale_x(_result_label->raw_ptr(), scale, LV_PART_MAIN);
    lv_obj_set_style_transform_scale_y(_result_label->raw_ptr(), scale, LV_PART_MAIN);
    _result_label->setY(kResultY - static_cast<int>(std::lround(std::sin(pop_progress * kTau * 0.5f) * 4.0f)));
    lv_obj_move_foreground(_result_label->raw_ptr());

    const bool show_confetti = _result_pop_active && pop_progress > 0.03f && pop_progress < 0.94f;
    const float burst        = std::sin(std::min(pop_progress / 0.72f, 1.0f) * kTau * 0.25f);
    const float fade         = 1.0f - std::clamp((pop_progress - 0.42f) / 0.52f, 0.0f, 1.0f);
    const int result_x       = requiresShift(_shown_state.exercise_index) ? kShiftResultX : kNormalResultX;
    const int origin_x       = result_x + kResultWidth / 2;
    const int origin_y       = kResultY + kResultHeight / 2;
    for (size_t index = 0; index < _confetti.size(); ++index) {
        auto& piece = _confetti[index];
        piece->setHidden(!show_confetti);
        if (!show_confetti) {
            continue;
        }
        const int x = origin_x + static_cast<int>(std::lround(static_cast<float>(kConfettiDx[index]) * burst));
        const int y = origin_y + static_cast<int>(std::lround(static_cast<float>(kConfettiDy[index]) * burst +
                                                              pop_progress * pop_progress * 14.0f));
        piece->setPos(x, y);
        piece->setOpa(static_cast<lv_opa_t>(std::lround(fade * 255.0f)));
        piece->setRotation(static_cast<int32_t>((static_cast<float>(index) * 35.0f + pop_progress * 220.0f) * 10.0f));
    }
}

void GuideView::playResultPop()
{
    _result_pop.cancel();
    _result_pop_progress = 0.0f;
    _result_pop_active   = true;
    _result_pop.start    = 0.0f;
    _result_pop.end      = 1.0f;
    _result_pop.init();
    _result_pop.play();
}

float GuideView::cursorTargetX(const GuideLessonState& state)
{
    const bool with_shift = requiresShift(state.exercise_index);
    const bool success    = state.exercise_complete || state.completed;
    if (with_shift && !state.awaiting_character && !success) {
        return kShiftCursorX;
    }
    return with_shift ? kShiftedKeyCursorX : kNormalKeyCursorX;
}

void GuideView::onStateChanged(void* context, const GuideLessonState& state)
{
    auto* view = static_cast<GuideView*>(context);
    if (view) {
        view->render(state);
    }
}

}  // namespace keyboard_guide
