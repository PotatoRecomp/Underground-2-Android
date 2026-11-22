#pragma once
#include <atomic>
#include <cstdint>

// The FPS counter's place: one of the presets of FPS COUNTER POSITION, or CUSTOM where the touch layout editor
// (native/touch.cpp) dragged it, Options::fps_x and fps_y (src/common/menu.cpp draws it there)
namespace game {

enum FpsPosition : uint32_t { kFpsTop, kFpsTopLeft, kFpsTopRight, kFpsBottomLeft, kFpsBottom, kFpsBottomRight,
                              kFpsCustom };
// fps_x and fps_y: the box's top left corner in shares of the screen, as ten thousandths
inline constexpr float kFpsPlaceScale = 10000.0f;

// the box as it was last drawn, pixels (left, top, right, bottom), and whether it was: the editor picks it up there
inline std::atomic<float> g_fps_box[4] = {};
inline std::atomic<bool> g_fps_shown{false};

}  // namespace game
