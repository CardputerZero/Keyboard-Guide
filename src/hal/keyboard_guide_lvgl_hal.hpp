#pragma once

#include <cstdint>

namespace keyboard_guide {

bool initLvglHal(int32_t width, int32_t height);
void shutdownLvglHal();

}  // namespace keyboard_guide
