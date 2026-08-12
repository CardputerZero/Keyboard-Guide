cmake_minimum_required(VERSION 3.16)

project(M5CardputerZeroKeyboardGuide VERSION 0.1.0 LANGUAGES C CXX ASM)

option(KEYBOARD_GUIDE_USE_SDL "Use LVGL's SDL backend for desktop testing" ON)
option(KEYBOARD_GUIDE_CONVERT_ASSETS "Refresh generated asset declarations during configure" ON)
option(KEYBOARD_GUIDE_BUILD_TESTS "Build Keyboard Guide unit tests" OFF)
set(KEYBOARD_GUIDE_BIN_NAME "M5CardputerZero-Keyboard-Guide" CACHE STRING "Output executable name")
set(KEYBOARD_GUIDE_DEPS_DIR "${CMAKE_CURRENT_LIST_DIR}/dependencies" CACHE PATH "Third-party source directory")
set(KEYBOARD_GUIDE_OUTPUT_DIR "${CMAKE_CURRENT_LIST_DIR}/dist" CACHE PATH "Runtime output directory")

if(APPLE AND NOT KEYBOARD_GUIDE_USE_SDL)
    message(FATAL_ERROR "macOS builds only support KEYBOARD_GUIDE_USE_SDL=ON.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(KEYBOARD_GUIDE_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(KEYBOARD_GUIDE_SRC_DIR "${KEYBOARD_GUIDE_ROOT}/src")
set(KEYBOARD_GUIDE_LVGL_DIR "${KEYBOARD_GUIDE_DEPS_DIR}/lvgl")
set(KEYBOARD_GUIDE_SPDLOG_DIR "${KEYBOARD_GUIDE_DEPS_DIR}/spdlog")
set(KEYBOARD_GUIDE_SMOOTH_UI_DIR "${KEYBOARD_GUIDE_DEPS_DIR}/smooth_ui_toolkit")
set(KEYBOARD_GUIDE_MINIAUDIO_DIR "${KEYBOARD_GUIDE_DEPS_DIR}/miniaudio")
set(KEYBOARD_GUIDE_SYSTEM_LIBS ${CMAKE_DL_LIBS})

if(UNIX AND NOT APPLE)
    list(APPEND KEYBOARD_GUIDE_SYSTEM_LIBS m)
endif()

if(APPLE)
    list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew" "/usr/local")
endif()

if(NOT DEFINED Python3_EXECUTABLE)
    foreach(candidate IN ITEMS
        "${KEYBOARD_GUIDE_ROOT}/.venv/bin/python"
        "${KEYBOARD_GUIDE_ROOT}/../.venv/bin/python"
        "${KEYBOARD_GUIDE_ROOT}/../../.venv/bin/python"
    )
        if(EXISTS "${candidate}")
            execute_process(
                COMMAND "${candidate}" -m pip --version
                RESULT_VARIABLE KEYBOARD_GUIDE_PYTHON_PIP_RESULT
                OUTPUT_QUIET
                ERROR_QUIET
            )
            if(KEYBOARD_GUIDE_PYTHON_PIP_RESULT EQUAL 0)
                set(Python3_EXECUTABLE "${candidate}")
                break()
            endif()
        endif()
    endforeach()
endif()

foreach(required_dir IN LISTS KEYBOARD_GUIDE_LVGL_DIR KEYBOARD_GUIDE_SPDLOG_DIR KEYBOARD_GUIDE_SMOOTH_UI_DIR)
    if(NOT EXISTS "${required_dir}/CMakeLists.txt")
        message(FATAL_ERROR "Missing dependency: ${required_dir}. Run ./bootstrap.sh first or set KEYBOARD_GUIDE_DEPS_DIR.")
    endif()
endforeach()
if(NOT EXISTS "${KEYBOARD_GUIDE_MINIAUDIO_DIR}/miniaudio.h")
    message(FATAL_ERROR "Missing dependency: ${KEYBOARD_GUIDE_MINIAUDIO_DIR}. Run ./bootstrap.sh first or set KEYBOARD_GUIDE_DEPS_DIR.")
endif()

find_package(Python3 COMPONENTS Interpreter REQUIRED)
find_package(Threads REQUIRED)
find_package(PkgConfig QUIET)

if(KEYBOARD_GUIDE_CONVERT_ASSETS)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E env
                "KEYBOARD_GUIDE_LVGL_DIR=${KEYBOARD_GUIDE_LVGL_DIR}"
                "${Python3_EXECUTABLE}" "${KEYBOARD_GUIDE_SRC_DIR}/assets/convert_images.py"
        WORKING_DIRECTORY "${KEYBOARD_GUIDE_ROOT}"
        RESULT_VARIABLE KEYBOARD_GUIDE_ASSET_RESULT
    )
    if(NOT KEYBOARD_GUIDE_ASSET_RESULT EQUAL 0)
        message(FATAL_ERROR "Asset declaration refresh failed")
    endif()
endif()

set(LV_BUILD_CONF_PATH "${KEYBOARD_GUIDE_ROOT}/lv_conf.h" CACHE PATH "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)

set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_BENCH OFF CACHE BOOL "" FORCE)

set(SMOOTH_UI_TOOLKIT_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SMOOTH_UI_TOOLKIT_BUILD_TEST OFF CACHE BOOL "" FORCE)

add_subdirectory("${KEYBOARD_GUIDE_LVGL_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/dependencies/lvgl" EXCLUDE_FROM_ALL)
add_subdirectory("${KEYBOARD_GUIDE_SPDLOG_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/dependencies/spdlog" EXCLUDE_FROM_ALL)
add_subdirectory("${KEYBOARD_GUIDE_SMOOTH_UI_DIR}"
                 "${CMAKE_CURRENT_BINARY_DIR}/dependencies/smooth_ui_toolkit" EXCLUDE_FROM_ALL)

function(keyboard_guide_add_sdl2_parent_include target imported_target)
    if(NOT TARGET "${imported_target}")
        return()
    endif()

    get_target_property(sdl2_include_dirs "${imported_target}" INTERFACE_INCLUDE_DIRECTORIES)
    if(NOT sdl2_include_dirs)
        return()
    endif()

    foreach(sdl2_include_dir IN LISTS sdl2_include_dirs)
        if(sdl2_include_dir MATCHES "/SDL2$")
            get_filename_component(sdl2_parent_include_dir "${sdl2_include_dir}" DIRECTORY)
            target_include_directories("${target}" SYSTEM PRIVATE "${sdl2_parent_include_dir}")
        endif()
    endforeach()
endfunction()

set(KEYBOARD_GUIDE_LVGL_POSIX_PATH "./")
if(NOT KEYBOARD_GUIDE_USE_SDL)
    set(KEYBOARD_GUIDE_LVGL_POSIX_PATH "/usr/share/Keyboard-Guide/")
endif()

target_compile_definitions(lvgl PUBLIC
    KEYBOARD_GUIDE_LVGL_USE_SDL=$<IF:$<BOOL:${KEYBOARD_GUIDE_USE_SDL}>,1,0>
    KEYBOARD_GUIDE_LVGL_FS_POSIX_PATH="${KEYBOARD_GUIDE_LVGL_POSIX_PATH}"
)

if(KEYBOARD_GUIDE_USE_SDL)
    if(PkgConfig_FOUND)
        pkg_check_modules(SDL2 QUIET IMPORTED_TARGET sdl2)
    endif()

    if(SDL2_FOUND)
        target_link_libraries(lvgl PRIVATE PkgConfig::SDL2)
        keyboard_guide_add_sdl2_parent_include(lvgl PkgConfig::SDL2)
        set(KEYBOARD_GUIDE_SDL_LIBS PkgConfig::SDL2)
    else()
        find_package(SDL2 CONFIG QUIET)
        if(TARGET SDL2::SDL2)
            target_link_libraries(lvgl PRIVATE SDL2::SDL2)
            keyboard_guide_add_sdl2_parent_include(lvgl SDL2::SDL2)
            set(KEYBOARD_GUIDE_SDL_LIBS SDL2::SDL2)
        else()
            find_package(SDL2 REQUIRED)
            target_include_directories(lvgl SYSTEM PRIVATE ${SDL2_INCLUDE_DIRS})
            foreach(sdl2_include_dir IN LISTS SDL2_INCLUDE_DIRS)
                if(sdl2_include_dir MATCHES "/SDL2$")
                    get_filename_component(sdl2_parent_include_dir "${sdl2_include_dir}" DIRECTORY)
                    target_include_directories(lvgl SYSTEM PRIVATE "${sdl2_parent_include_dir}")
                endif()
            endforeach()
            set(KEYBOARD_GUIDE_SDL_LIBS ${SDL2_LIBRARIES})
        endif()
    endif()
endif()

file(GLOB KEYBOARD_GUIDE_CPP CONFIGURE_DEPENDS
    "${KEYBOARD_GUIDE_SRC_DIR}/main.cpp"
    "${KEYBOARD_GUIDE_SRC_DIR}/assets/*.cpp"
    "${KEYBOARD_GUIDE_SRC_DIR}/audio/*.cpp"
    "${KEYBOARD_GUIDE_SRC_DIR}/core/*.cpp"
    "${KEYBOARD_GUIDE_SRC_DIR}/hal/*.cpp"
    "${KEYBOARD_GUIDE_SRC_DIR}/input/*.cpp"
    "${KEYBOARD_GUIDE_SRC_DIR}/models/*.cpp"
    "${KEYBOARD_GUIDE_SRC_DIR}/view_models/*.cpp"
    "${KEYBOARD_GUIDE_SRC_DIR}/views/*.cpp"
)
file(GLOB_RECURSE KEYBOARD_GUIDE_ASSET_C CONFIGURE_DEPENDS
    "${KEYBOARD_GUIDE_SRC_DIR}/assets/res_c/*.c"
)

add_executable(${KEYBOARD_GUIDE_BIN_NAME}
    ${KEYBOARD_GUIDE_CPP}
    ${KEYBOARD_GUIDE_ASSET_C}
    "${KEYBOARD_GUIDE_SRC_DIR}/audio/miniaudio_impl.c"
)

target_include_directories(${KEYBOARD_GUIDE_BIN_NAME} PRIVATE
    "${KEYBOARD_GUIDE_SRC_DIR}"
    "${KEYBOARD_GUIDE_SRC_DIR}/assets"
    "${KEYBOARD_GUIDE_MINIAUDIO_DIR}"
)

if(KEYBOARD_GUIDE_USE_SDL)
    set(KEYBOARD_GUIDE_AUDIO_ASSET_DIR "${KEYBOARD_GUIDE_ROOT}/assets/audio")
else()
    set(KEYBOARD_GUIDE_AUDIO_ASSET_DIR "/usr/share/Keyboard-Guide/audio")
endif()
target_compile_definitions(${KEYBOARD_GUIDE_BIN_NAME} PRIVATE
    KEYBOARD_GUIDE_USE_SDL=$<IF:$<BOOL:${KEYBOARD_GUIDE_USE_SDL}>,1,0>
    KEYBOARD_GUIDE_AUDIO_ASSET_DIR="${KEYBOARD_GUIDE_AUDIO_ASSET_DIR}"
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${KEYBOARD_GUIDE_BIN_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_link_libraries(${KEYBOARD_GUIDE_BIN_NAME} PRIVATE
    lvgl::lvgl
    smooth_ui_toolkit
    spdlog::spdlog
    Threads::Threads
    ${KEYBOARD_GUIDE_SDL_LIBS}
    ${KEYBOARD_GUIDE_SYSTEM_LIBS}
)

set_target_properties(${KEYBOARD_GUIDE_BIN_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${KEYBOARD_GUIDE_OUTPUT_DIR}"
)

if(KEYBOARD_GUIDE_BUILD_TESTS)
    enable_testing()

    add_executable(keyboard_guide_modifier_state_tests
        tests/modifier_state_parser_test.cpp
        src/input/modifier_state_parser.cpp
    )
    target_include_directories(keyboard_guide_modifier_state_tests PRIVATE "${KEYBOARD_GUIDE_SRC_DIR}")
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(keyboard_guide_modifier_state_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_test(NAME keyboard_guide_modifier_state COMMAND keyboard_guide_modifier_state_tests)
endif()
