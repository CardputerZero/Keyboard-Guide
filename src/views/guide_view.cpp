#include "views/guide_view.hpp"

#include "assets/assets.h"
#include "assets/font_assets.hpp"

#include <lvgl/lvgl_cpp/image.hpp>
#include <lvgl/lvgl_cpp/label.hpp>
#include <lvgl/lvgl_cpp/line.hpp>
#include <lvgl/lvgl_cpp/obj.hpp>

#include <array>
#include <cmath>

namespace keyboard_guide {
namespace {

constexpr int kScreenWidth  = 320;
constexpr int kScreenHeight = 170;

constexpr int kTitleY      = 12;
constexpr int kShiftX      = 20;
constexpr int kShiftY      = 47;
constexpr int kShiftWidth  = 72;
constexpr int kShiftHeight = 40;
constexpr int kArrowX      = 106;
constexpr int kArrowY      = 57;
constexpr int kArrowWidth  = 32;
constexpr int kArrowHeight = 20;
constexpr int kLetterY     = 54;
constexpr int kLetterWidth = 28;
constexpr int kCursorY     = 101;
constexpr int kCursorSize  = 25;
constexpr int kPromptY     = 143;

constexpr float kShiftCursorX = 83.0f;
constexpr float kACursorX     = 175.0f;
constexpr float kBCursorX     = 221.0f;
constexpr float kCCursorX     = 267.0f;
constexpr float kTau          = 6.28318530717958647692f;

constexpr uint32_t kBackground  = 0x000000;
constexpr uint32_t kTitle       = 0xE4E4E4;
constexpr uint32_t kShiftFill   = 0x990099;
constexpr uint32_t kShiftBorder = 0xEF4FEF;
constexpr uint32_t kArrow       = 0x565656;
constexpr uint32_t kCurrent     = 0xF5F5F5;
constexpr uint32_t kFuture      = 0x525252;
constexpr uint32_t kComplete    = 0x3FCC75;
constexpr uint32_t kPrompt      = 0xA7A7A7;
constexpr uint32_t kError       = 0xFF6B6B;

constexpr std::array<int, 3> kLetterX{164, 210, 256};
constexpr std::array<const char*, 3> kLetters{"A", "B", "C"};
constexpr std::array<lv_point_t, 4> kCheckPositions{
    lv_point_t{81, 40},
    lv_point_t{186, 47},
    lv_point_t{232, 47},
    lv_point_t{278, 47},
};
constexpr std::array<lv_point_precise_t, 5> kArrowPoints{
    lv_point_precise_t{0, 10},  lv_point_precise_t{30, 10}, lv_point_precise_t{22, 2},
    lv_point_precise_t{30, 10}, lv_point_precise_t{22, 18},
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

}  // namespace

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

    _title_label = std::make_unique<Label>(_root->raw_ptr());
    _title_label->setSize(kScreenWidth, lv_font_get_line_height(uiFont14()) + 3);
    _title_label->setPos(0, kTitleY);
    _title_label->setTextAlign(LV_TEXT_ALIGN_CENTER);
    _title_label->setTextFont(uiFont14());
    _title_label->setTextColor(lv_color_hex(kTitle));
    _title_label->setText("One-shot Shift");

    _shift_key = std::make_unique<Container>(_root->raw_ptr());
    _shift_key->setSize(kShiftWidth, kShiftHeight);
    _shift_key->setPos(kShiftX, kShiftY);
    _shift_key->setRadius(kShiftHeight / 2);
    setupContainer(*_shift_key, LV_OPA_COVER);
    _shift_key->setBorderColor(lv_color_hex(kShiftBorder));
    _shift_key->setBorderWidth(2);

    _shift_label = std::make_unique<Label>(_shift_key->raw_ptr());
    _shift_label->setSize(kShiftWidth, lv_font_get_line_height(uiFont14()) + 3);
    _shift_label->center();
    _shift_label->setTextAlign(LV_TEXT_ALIGN_CENTER);
    _shift_label->setTextFont(uiFont14());
    _shift_label->setTextColor(lv_color_hex(kCurrent));
    _shift_label->setText("Shift");

    _arrow = std::make_unique<Line>(_root->raw_ptr());
    _arrow->setSize(kArrowWidth, kArrowHeight);
    _arrow->setPos(kArrowX, kArrowY);
    _arrow->setPoints(kArrowPoints.data(), static_cast<uint32_t>(kArrowPoints.size()));
    _arrow->setLineColor(lv_color_hex(kArrow));
    _arrow->setLineWidth(2);
    _arrow->setLineRounded(true);

    for (size_t index = 0; index < _letter_labels.size(); ++index) {
        auto& label = _letter_labels[index];
        label       = std::make_unique<Label>(_root->raw_ptr());
        label->setSize(kLetterWidth, lv_font_get_line_height(&lv_font_montserrat_20) + 4);
        label->setPos(kLetterX[index], kLetterY);
        label->setTextAlign(LV_TEXT_ALIGN_CENTER);
        label->setTextFont(&lv_font_montserrat_20);
        label->setText(kLetters[index]);
    }

    for (size_t index = 0; index < _check_badges.size(); ++index) {
        auto& badge = _check_badges[index];
        badge       = std::make_unique<Container>(_root->raw_ptr());
        badge->setSize(16, 16);
        badge->setPos(kCheckPositions[index].x, kCheckPositions[index].y);
        setupContainer(*badge, LV_OPA_COVER);
        badge->setBgColor(lv_color_hex(kComplete));
        badge->setRadius(8);

        auto& label = _check_labels[index];
        label       = std::make_unique<Label>(badge->raw_ptr());
        label->setSize(16, lv_font_get_line_height(&lv_font_montserrat_12));
        label->center();
        label->setTextAlign(LV_TEXT_ALIGN_CENTER);
        label->setTextFont(&lv_font_montserrat_12);
        label->setTextColor(lv_color_hex(0xFFFFFF));
        label->setText(LV_SYMBOL_OK);
    }

    _cursor = std::make_unique<Image>(_root->raw_ptr());
    _cursor->setSrc(&image_cursor_hover);
    _cursor->setSize(kCursorSize, kCursorSize);

    _prompt_label = std::make_unique<Label>(_root->raw_ptr());
    _prompt_label->setSize(kScreenWidth - 16, lv_font_get_line_height(uiFont12()) + 3);
    _prompt_label->setPos(8, kPromptY);
    _prompt_label->setLongMode(LV_LABEL_LONG_CLIP);
    _prompt_label->setTextAlign(LV_TEXT_ALIGN_CENTER);
    _prompt_label->setTextFont(uiFont12());

    _cursor_x.stop();
    _cursor_x.springOptions().visualDuration = 0.45f;
    _cursor_x.springOptions().bounce         = 0.35f;
    _cursor_x.teleport(kShiftCursorX);
    _cursor_x.begin();

    _target_bob.start                          = 0.0f;
    _target_bob.end                            = 1.0f;
    _target_bob.repeat                         = -1;
    _target_bob.repeatType                     = smooth_ui_toolkit::AnimateRepeatType::Reverse;
    _target_bob.springOptions().visualDuration = 0.62f;
    _target_bob.springOptions().bounce         = 0.22f;
    _target_bob.onUpdate([this](float value) { _target_bob_value = value; });
    _target_bob.init();
    _target_bob.play();

    render(_view_model.state().get());
    _view_model.state().observe(this, onStateChanged);
}

void GuideView::onExit()
{
    _view_model.state().removeObserver();
    _cursor_x.stop();
    _target_bob.cancel();
    _prompt_label.reset();
    _cursor.reset();
    for (auto& label : _check_labels) {
        label.reset();
    }
    for (auto& badge : _check_badges) {
        badge.reset();
    }
    for (auto& label : _letter_labels) {
        label.reset();
    }
    _arrow.reset();
    _shift_label.reset();
    _shift_key.reset();
    _title_label.reset();
    _root.reset();
}

void GuideView::tick(uint32_t now_ms)
{
    _target_bob.update(static_cast<float>(now_ms) / 1000.0f);
    applyTransforms(now_ms);
}

void GuideView::render(const GuideLessonState& state)
{
    _shown_state = state;

    _shift_key->setBgColor(lv_color_hex(kShiftFill));
    _shift_label->setTextColor(lv_color_hex(kCurrent));

    std::array<bool, 3> letter_complete{};
    for (size_t index = 0; index < _letter_labels.size(); ++index) {
        const int letter_step  = static_cast<int>(index) * 2 + 1;
        letter_complete[index] = state.completed || state.step_index > letter_step;
        const bool current     = !state.completed && state.step_index == letter_step;
        _letter_labels[index]->setTextColor(
            lv_color_hex(letter_complete[index] ? kComplete : (current ? kCurrent : kFuture)));
        _check_badges[index + 1]->setHidden(!letter_complete[index]);
    }

    const bool shift_checked = state.completed || (state.step_index % 2) == 1;
    _check_badges[0]->setHidden(!shift_checked);

    _prompt_label->setText(state.prompt);
    _prompt_label->setTextColor(
        lv_color_hex(state.last_action_error ? kError : (state.completed ? kComplete : kPrompt)));
    if (state.completed) {
        _cursor->addFlag(LV_OBJ_FLAG_HIDDEN);
    } else {
        _cursor->removeFlag(LV_OBJ_FLAG_HIDDEN);
        _cursor_x.move(cursorTargetX(state));
    }

    if (_shown_attention_revision != state.attention_revision) {
        _shown_attention_revision = state.attention_revision;
        if (state.last_action_error) {
            _shake_started_at = lv_tick_get();
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

    const int cursor_x = static_cast<int>(std::lround(_cursor_x.value())) + shake_offset;
    _cursor->setPos(cursor_x, kCursorY);
    lv_obj_move_foreground(_cursor->raw_ptr());

    const int target_offset  = static_cast<int>(std::lround(_target_bob_value * 3.0f));
    const bool shift_current = !_shown_state.completed && (_shown_state.step_index % 2) == 0;
    _shift_key->setY(kShiftY - (shift_current ? target_offset : 0));
    for (size_t index = 0; index < _letter_labels.size(); ++index) {
        const int letter_step = static_cast<int>(index) * 2 + 1;
        const bool current    = !_shown_state.completed && _shown_state.step_index == letter_step;
        _letter_labels[index]->setY(kLetterY - (current ? target_offset : 0));
    }
}

float GuideView::cursorTargetX(const GuideLessonState& state)
{
    switch (state.step_index) {
        case 1:
            return kACursorX;
        case 3:
            return kBCursorX;
        case 5:
            return kCCursorX;
        case 0:
        case 2:
        case 4:
        default:
            return kShiftCursorX;
    }
}

void GuideView::onStateChanged(void* context, const GuideLessonState& state)
{
    auto* view = static_cast<GuideView*>(context);
    if (view) {
        view->render(state);
    }
}

}  // namespace keyboard_guide
