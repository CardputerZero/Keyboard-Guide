#pragma once

#include "input/keyboard_guide_input.hpp"
#include "models/guide_model.hpp"
#include "view_models/guide_view_model.hpp"
#include "views/guide_view.hpp"

#include <cstdint>

namespace keyboard_guide {

class KeyboardGuideApp {
public:
    KeyboardGuideApp();
    ~KeyboardGuideApp();

    bool start(lv_obj_t* parent);
    void stop();
    void tick(uint32_t now_ms);
    bool quitRequested() const;

private:
    static constexpr uint32_t kExitHoldMs = 900;

    GuideModel _model;
    GuideViewModel _view_model;
    GuideView _view;
    KeyboardGuideInput _input;
    bool _started               = false;
    bool _quit_requested        = false;
    bool _escape_down           = false;
    uint32_t _escape_pressed_at = 0;

    void onInput(const GuideInputEvent& event);
};

}  // namespace keyboard_guide
