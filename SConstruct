Import("env")
import os
with open(env["PROJECT_TOOL_S"]) as f:
    exec(f.read())

SRCS = append_srcs_dir(ADir("src"))
INCLUDE = [
    ADir("."),
    ADir("src"),
    ADir("src/assets"),
]
PRIVATE_INCLUDE = []
REQUIREMENTS = [
    "cp0_lvgl",
    "lvgl_component",
    "pthread",
    "SmoothUI",
    "Spdlog",
    "Miniaudio",
    "dl",
    "m",
]
STATIC_LIB = []
DYNAMIC_LIB = []
DEFINITIONS = [
    '-DKEYBOARD_GUIDE_AUDIO_ASSET_DIR=\\\\\\"/usr/share/Keyboard-Guide/audio\\\\\\"',
    "-DKEYBOARD_GUIDE_USE_SDL=0",
]
DEFINITIONS_PRIVATE = []
LDFLAGS = []
LINK_SEARCH_PATH = []
STATIC_FILES = [(ADir("assets/audio"), "audio")]

env["COMPONENTS"].append(
    {
        "target": "Keyboard_Guide",
        "SRCS": SRCS,
        "INCLUDE": INCLUDE,
        "PRIVATE_INCLUDE": PRIVATE_INCLUDE,
        "REQUIREMENTS": REQUIREMENTS,
        "STATIC_LIB": STATIC_LIB,
        "DYNAMIC_LIB": DYNAMIC_LIB,
        "DEFINITIONS": DEFINITIONS,
        "DEFINITIONS_PRIVATE": DEFINITIONS_PRIVATE,
        "LDFLAGS": LDFLAGS,
        "LINK_SEARCH_PATH": LINK_SEARCH_PATH,
        "STATIC_FILES": STATIC_FILES,
        "REGISTER": "project",
    }
)
