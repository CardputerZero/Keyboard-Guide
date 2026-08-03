#pragma once

#include "models/guide_model.hpp"

namespace keyboard_guide {

class GuideViewModel {
public:
    explicit GuideViewModel(GuideModel& model);

    void onEnter();
    void onExit();
    void onInput(const GuideInputEvent& event);
    void tick(uint32_t now_ms);

    smooth_ui_toolkit::SingleObservable<GuideLessonState>& state();

private:
    GuideModel& _model;
};

}  // namespace keyboard_guide
