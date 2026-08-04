#include "core/keyboard_guide_app.hpp"

#include "assets/font_assets.hpp"

#include <spdlog/spdlog.h>

namespace keyboard_guide {
namespace {

std::size_t modifierIndex(GuideKey key)
{
    switch (key) {
        case GuideKey::Shift:
            return 0;
        case GuideKey::Sym:
            return 1;
        case GuideKey::Fn:
            return 2;
        default:
            return 3;
    }
}

bool modifierLocked(const GuideLessonState& state, GuideKey key)
{
    switch (key) {
        case GuideKey::Shift:
            return state.shift_locked;
        case GuideKey::Sym:
            return state.sym_locked;
        case GuideKey::Fn:
            return state.fn_locked;
        default:
            return false;
    }
}

}  // namespace

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
    _sound.start();
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
    _sound.stop();
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
            _sound.play(SoundCue::Typing);
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
                _sound.play(SoundCue::Typing);
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
            if (_view_model.nextExercise()) {
                _sound.play(SoundCue::Typing);
            } else {
                _quit_requested = true;
            }
        }
        return;
    }
    const GuideLessonState before = _model.state().get();
    _view_model.onInput(event);
    playInputSound(event, before, _model.state().get());
}

void KeyboardGuideApp::playInputSound(const GuideInputEvent& event, const GuideLessonState& before,
                                      const GuideLessonState& after)
{
    if (after.attention_revision != before.attention_revision && after.last_action_error) {
        _sound.play(SoundCue::Error);
        if (event.has_modifier_mode) {
            const std::size_t index = modifierIndex(event.key);
            if (index < _modifier_modes.size()) {
                _modifier_modes[index] = event.modifier_mode;
            }
        }
        return;
    }

    const std::size_t index = modifierIndex(event.key);
    if (index < _modifier_modes.size()) {
        const bool was_locked = modifierLocked(before, event.key);
        const bool is_locked  = modifierLocked(after, event.key);
        if (!was_locked && is_locked) {
            _sound.play(SoundCue::Lock);
        } else if (was_locked && !is_locked) {
            _sound.play(SoundCue::Unlock);
        } else if (event.has_modifier_mode) {
            const GuideModifierMode previous_mode = _modifier_modes[index];
            if (event.modifier_mode == GuideModifierMode::OneShot || event.modifier_mode == GuideModifierMode::Held ||
                (event.modifier_mode == GuideModifierMode::Locked && previous_mode != GuideModifierMode::Held)) {
                _sound.play(SoundCue::Typing);
            }
        } else if (event.pressed && !event.repeated) {
            _sound.play(SoundCue::Typing);
        }

        if (event.has_modifier_mode) {
            _modifier_modes[index] = event.modifier_mode;
        }
        return;
    }

    if (!event.pressed || event.repeated || event.key == GuideKey::Unknown) {
        return;
    }
    const bool result_completed = after.result_revision != before.result_revision;
    const bool final_exercise   = after.exercise_index == GuideModel::kExerciseCount - 1;
    const bool final_completed  = final_exercise && result_completed && after.phase == GuidePhase::SuccessHold;
    if (final_completed) {
        _sound.playExclusive(SoundCue::LessonComplete);
    } else if (result_completed) {
        _sound.play(SoundCue::CharacterComplete);
    } else {
        _sound.play(SoundCue::Typing);
    }
}

bool KeyboardGuideApp::onIntroPage()
{
    return _model.state().get().phase == GuidePhase::Intro;
}

}  // namespace keyboard_guide
