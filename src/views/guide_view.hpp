#pragma once

#include "core/guide_types.hpp"
#include "view_models/guide_view_model.hpp"

#include <core/animation/animate/animate.hpp>
#include <core/animation/animate_value/animate_value.hpp>
#include <lvgl.h>

#include <array>
#include <cstdint>
#include <memory>

namespace smooth_ui_toolkit::lvgl_cpp {
class Container;
class Image;
class Label;
class Line;
}  // namespace smooth_ui_toolkit::lvgl_cpp

namespace keyboard_guide {

class GuideView {
public:
    explicit GuideView(GuideViewModel& view_model);
    ~GuideView();

    void onEnter(lv_obj_t* parent);
    void onExit();
    void tick(uint32_t now_ms);

private:
    GuideViewModel& _view_model;
    std::unique_ptr<smooth_ui_toolkit::lvgl_cpp::Container> _root;
    std::unique_ptr<smooth_ui_toolkit::lvgl_cpp::Label> _title_label;
    std::unique_ptr<smooth_ui_toolkit::lvgl_cpp::Container> _shift_key;
    std::unique_ptr<smooth_ui_toolkit::lvgl_cpp::Label> _shift_label;
    std::unique_ptr<smooth_ui_toolkit::lvgl_cpp::Line> _arrow;
    std::array<std::unique_ptr<smooth_ui_toolkit::lvgl_cpp::Label>, 3> _letter_labels;
    std::array<std::unique_ptr<smooth_ui_toolkit::lvgl_cpp::Container>, 4> _check_badges;
    std::array<std::unique_ptr<smooth_ui_toolkit::lvgl_cpp::Label>, 4> _check_labels;
    std::unique_ptr<smooth_ui_toolkit::lvgl_cpp::Image> _cursor;
    std::unique_ptr<smooth_ui_toolkit::lvgl_cpp::Label> _prompt_label;

    smooth_ui_toolkit::AnimateValue _cursor_x;
    smooth_ui_toolkit::Animate _target_bob;
    GuideLessonState _shown_state;
    float _target_bob_value            = 0.0f;
    uint32_t _shake_started_at         = 0;
    uint32_t _shown_attention_revision = 0;

    void render(const GuideLessonState& state);
    void applyTransforms(uint32_t now_ms);
    static float cursorTargetX(const GuideLessonState& state);
    static void onStateChanged(void* context, const GuideLessonState& state);
};

}  // namespace keyboard_guide
