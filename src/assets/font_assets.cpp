#include "assets/font_assets.hpp"

#include "assets/assets.h"

namespace keyboard_guide {
namespace {

bool g_initialized = false;
lv_font_t g_ui_sc_10;
lv_font_t g_ui_sc_14;
lv_font_t g_ui_symbols_20;

}  // namespace

void initFontAssets()
{
    if (g_initialized) {
        return;
    }

    g_ui_sc_10               = font_noto_sans_sc_semibold_10;
    g_ui_sc_14               = font_noto_sans_sc_semibold_14;
    g_ui_symbols_20          = font_noto_sans_sc_semibold_symbols_20;
    g_ui_sc_10.fallback      = nullptr;
    g_ui_sc_14.fallback      = nullptr;
    g_ui_symbols_20.fallback = nullptr;
    g_initialized            = true;
}

const lv_font_t* uiFont10()
{
    initFontAssets();
    return &g_ui_sc_10;
}

const lv_font_t* uiFont14()
{
    initFontAssets();
    return &g_ui_sc_14;
}

const lv_font_t* uiSymbolFont20()
{
    initFontAssets();
    return &g_ui_symbols_20;
}

}  // namespace keyboard_guide
