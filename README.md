# Keyboard Guide

Interactive keyboard onboarding for M5CardputerZero, built with LVGL, Smooth UI Toolkit, and MVVM.

The current first-page prototype teaches one-shot Shift by guiding the user through `Shift + A`, `Shift + B`, and `Shift + C` with animated step feedback.

## Desktop build

```sh
./bootstrap.sh
cmake -S . -B build/sdl -DKEYBOARD_GUIDE_USE_SDL=ON
cmake --build build/sdl -j"$(nproc)"
./dist/M5CardputerZero-Keyboard-Guide
```

The SDL window uses the native 320 x 170 logical resolution. Set `KEYBOARD_GUIDE_SDL_ZOOM` only when an explicit preview scale is useful.

## CardputerZero build

```sh
cmake -S . -B build/cp0 \
  -DKEYBOARD_GUIDE_USE_SDL=OFF \
  -DCMAKE_TOOLCHAIN_FILE=cmake/aarch64-linux-gnu.cmake
cmake --build build/cp0 -j"$(nproc)"
```
