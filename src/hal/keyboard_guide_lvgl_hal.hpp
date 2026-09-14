/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>

namespace keyboard_guide {

bool initLvglHal(int32_t width, int32_t height);
bool flushExitLogo(const char* path = "/usr/share/APPLaunch/share/images/logo_lcd.png");
void shutdownLvglHal();

}  // namespace keyboard_guide
