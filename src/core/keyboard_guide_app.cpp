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
    _quit_requested     = false;
    _escape_down        = false;
    _enter_down         = false;
    _enter_hold_handled = false;
    _started            = true;
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
    now_ms = lv_tick_get();
    _view_model.tick(now_ms);
    if (_escape_down && now_ms - _escape_pressed_at >= kExitHoldMs) {
        _quit_requested = true;
        _escape_down    = false;
    }
    if (onIntroPage() && _enter_down && !_enter_hold_handled && now_ms - _enter_pressed_at >= kEnterHoldMs) {
        _quit_requested     = true;
        _enter_hold_handled = true;
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
    if (event.key == GuideKey::Enter) {
        if (onIntroPage()) {
            if (event.pressed && !event.repeated && !_enter_down) {
                _enter_down         = true;
                _enter_hold_handled = false;
                _enter_pressed_at   = event.timestamp_ms;
            } else if (!event.pressed && _enter_down) {
                const bool held = _enter_hold_handled || event.timestamp_ms - _enter_pressed_at >= kEnterHoldMs;
                _enter_down     = false;
                if (held) {
                    _quit_requested = true;
                } else {
                    _view_model.nextExercise();
                }
            }
        } else if (event.pressed && !event.repeated) {
            if (!_view_model.nextExercise()) {
                _quit_requested = true;
            }
        }
        return;
    }
    _view_model.onInput(event);
}

bool KeyboardGuideApp::onIntroPage()
{
    return _model.state().get().phase == GuidePhase::Intro;
}

}  // namespace keyboard_guide
