#include "core/keyboard_guide_app.hpp"
#include "hal/keyboard_guide_lvgl_hal.hpp"

#include <core/hal/hal.hpp>
#include <lvgl.h>
#include <spdlog/spdlog.h>

#include <cstdio>
#include <unistd.h>

int main()
{
    constexpr int32_t kScreenWidth  = 320;
    constexpr int32_t kScreenHeight = 170;

    lv_init();
    if (!keyboard_guide::initLvglHal(kScreenWidth, kScreenHeight)) {
        return 1;
    }

    lv_display_t* display = lv_display_get_default();
    if (!display) {
        std::fprintf(stderr, "Keyboard Guide: failed to create LVGL display\n");
        return 1;
    }
    spdlog::info("Keyboard Guide: display {}x{}", static_cast<int>(lv_display_get_horizontal_resolution(display)),
                 static_cast<int>(lv_display_get_vertical_resolution(display)));

    smooth_ui_toolkit::ui_hal::on_get_tick([]() { return lv_tick_get(); });
    smooth_ui_toolkit::ui_hal::on_delay([](uint32_t milliseconds) { usleep(milliseconds * 1000); });

    keyboard_guide::KeyboardGuideApp app;
    if (!app.start(lv_screen_active())) {
        keyboard_guide::shutdownLvglHal();
        return 1;
    }

    lv_obj_invalidate(lv_screen_active());
    while (!app.quitRequested()) {
        lv_timer_handler();
        app.tick(lv_tick_get());
        usleep(8000);
    }

    spdlog::info("Keyboard Guide: exit requested");
    app.stop();
    keyboard_guide::shutdownLvglHal();
    return 0;
}
