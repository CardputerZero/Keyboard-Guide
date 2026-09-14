#include "hal/keyboard_guide_lvgl_hal.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <limits>
#include <lvgl.h>
#include <spdlog/spdlog.h>
#include <vector>

#include "src/draw/lv_image_decoder_private.h"

#if LV_USE_SDL
#include "src/drivers/sdl/lv_sdl_keyboard.h"
#include "src/drivers/sdl/lv_sdl_mouse.h"
#include "src/drivers/sdl/lv_sdl_window.h"
#elif LV_USE_LINUX_FBDEV
#include "src/drivers/display/fb/lv_linux_fbdev.h"
#endif

namespace keyboard_guide {
namespace {

const char* envOrDefault(const char* name, const char* fallback)
{
    const char* value = std::getenv(name);
    return value && value[0] != '\0' ? value : fallback;
}

#if LV_USE_SDL
float envFloatOrDefault(const char* name, float fallback)
{
    const char* value = std::getenv(name);
    if (!value || value[0] == '\0') {
        return fallback;
    }

    char* end          = nullptr;
    const float parsed = std::strtof(value, &end);
    return end && end != value && parsed > 0.0f ? parsed : fallback;
}
#endif

}  // namespace

bool initLvglHal(int32_t width, int32_t height)
{
#if LV_USE_SDL
    lv_display_t* display = lv_sdl_window_create(width, height);
    if (!display) {
        spdlog::error("Keyboard Guide HAL: failed to create SDL display");
        return false;
    }

    const float zoom = envFloatOrDefault("KEYBOARD_GUIDE_SDL_ZOOM", 1.0f);
    lv_sdl_window_set_resizeable(display, false);
    lv_sdl_window_set_zoom(display, zoom);
    lv_sdl_window_set_title(display, envOrDefault("LV_SDL_WINDOW_TITLE", "Keyboard Guide"));
    spdlog::info("Keyboard Guide HAL: SDL logical display {}x{}, zoom {}", width, height, zoom);

    lv_sdl_mouse_create();
    if (!lv_sdl_keyboard_create()) {
        spdlog::warn("Keyboard Guide HAL: failed to create SDL keyboard input");
    }
    return true;
#elif LV_USE_LINUX_FBDEV
    (void)width;
    (void)height;
    lv_display_t* display = lv_linux_fbdev_create();
    if (!display) {
        spdlog::error("Keyboard Guide HAL: failed to create framebuffer display");
        return false;
    }

    const char* device = envOrDefault("LV_LINUX_FBDEV_DEVICE", "/dev/fb0");
    if (lv_linux_fbdev_set_file(display, device) != LV_RESULT_OK) {
        spdlog::error("Keyboard Guide HAL: failed to open framebuffer {}", device);
        return false;
    }
    return true;
#else
    spdlog::error("Keyboard Guide HAL: no LVGL display driver enabled");
    return false;
#endif
}

bool flushExitLogo(const char* path)
{
    lv_display_t* display = lv_display_get_default();
    if (!display || !path) {
        return false;
    }

    // Read the installed PNG without LVGL's application-relative filesystem prefix.
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        spdlog::warn("Keyboard Guide: cannot open exit PNG {}", path);
        return false;
    }
    const std::vector<uint8_t> png((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    constexpr uint8_t kPngSignature[] = {0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a};
    if (file.bad() || png.size() < 24 || png.size() > std::numeric_limits<uint32_t>::max() ||
        !std::equal(std::begin(kPngSignature), std::end(kPngSignature), png.begin())) {
        spdlog::warn("Keyboard Guide: cannot read exit PNG {}", path);
        return false;
    }

    lv_image_dsc_t source{};
    source.header.magic = LV_IMAGE_HEADER_MAGIC;
    source.header.cf    = LV_COLOR_FORMAT_RAW;
    source.data         = png.data();
    source.data_size    = static_cast<uint32_t>(png.size());
    lv_image_decoder_args_t args{};
    args.no_cache = true;
    lv_image_decoder_dsc_t decoder{};
    if (lv_image_decoder_open(&decoder, &source, &args) != LV_RESULT_OK) {
        spdlog::warn("Keyboard Guide: cannot decode exit PNG {}", path);
        return false;
    }
    if (!decoder.decoded) {
        lv_image_decoder_close(&decoder);
        return false;
    }

    lv_image_dsc_t decoded_image{};
    lv_draw_buf_to_image(decoder.decoded, &decoded_image);
    lv_obj_t* previous_screen = lv_display_get_screen_active(display);
    lv_obj_t* screen = lv_obj_create(nullptr);
    lv_obj_remove_style_all(screen);
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_t* image = lv_image_create(screen);
    lv_image_set_src(image, &decoded_image);
    lv_obj_center(image);
    lv_screen_load(screen);
    lv_obj_invalidate(screen);
    // The fbdev flush copies pixels to the framebuffer before this call returns.
    lv_refr_now(display);

    lv_screen_load(previous_screen);
    lv_obj_delete(screen);
    lv_image_cache_drop(&decoded_image);
    lv_image_decoder_close(&decoder);
    spdlog::info("Keyboard Guide: flushed exit PNG {}", path);
    return true;
}

void shutdownLvglHal()
{
#if LV_USE_SDL
    lv_sdl_quit();
#endif
}

}  // namespace keyboard_guide
