#include "core/keyboard_guide_app.hpp"

#include "assets/font_assets.hpp"

#include <spdlog/spdlog.h>

namespace keyboard_guide {

KeyboardGuideApp::KeyboardGuideApp() : _view_model(_model), _view(_view_model)
{
}

KeyboardGuideApp::~KeyboardGuideApp()
{
    stop();
}

bool KeyboardGuideApp::start(lv_obj_t* parent)
{
    if (_started || !parent) {
        return _started;
    }

    initFontAssets();
    _view_model.onEnter();
    _view.onEnter(parent);
    _input.setEventCallback([this](const GuideInputEvent& event) { onInput(event); });
    if (!_input.openDefault()) {
        spdlog::warn("Keyboard Guide: keyboard input is unavailable");
    }
    _started = true;
    spdlog::info("Keyboard Guide: started keyboard lesson");
    return true;
}

void KeyboardGuideApp::stop()
{
    if (!_started) {
        return;
    }
    _input.close();
    _view.onExit();
    _view_model.onExit();
    _started = false;
}

void KeyboardGuideApp::tick(uint32_t now_ms)
{
    if (!_started) {
        return;
    }
    _input.poll();
    _view_model.tick(now_ms);
    if (_escape_down && now_ms - _escape_pressed_at >= kExitHoldMs) {
        _quit_requested = true;
        _escape_down    = false;
    }
    _view.tick(now_ms);
}

bool KeyboardGuideApp::quitRequested() const
{
    return _quit_requested;
}

void KeyboardGuideApp::onInput(const GuideInputEvent& event)
{
    if (event.key == GuideKey::Escape) {
        if (event.pressed && !event.repeated && !_escape_down) {
            _escape_down       = true;
            _escape_pressed_at = event.timestamp_ms;
        } else if (!event.pressed) {
            _escape_down = false;
        }
        return;
    }
    _view_model.onInput(event);
}

}  // namespace keyboard_guide
