#include "view_models/guide_view_model.hpp"

#include <spdlog/spdlog.h>

namespace keyboard_guide {

GuideViewModel::GuideViewModel(GuideModel& model) : _model(model)
{
}

void GuideViewModel::onEnter()
{
    spdlog::info("Keyboard Guide: one-shot Shift lesson entered");
}

void GuideViewModel::onExit()
{
}

void GuideViewModel::onInput(const GuideInputEvent& event)
{
    _model.handleInput(event);
}

smooth_ui_toolkit::SingleObservable<GuideLessonState>& GuideViewModel::state()
{
    return _model.state();
}

}  // namespace keyboard_guide
