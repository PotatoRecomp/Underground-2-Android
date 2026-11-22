#pragma once
#include <atomic>
#include <cstddef>

// The HUD's speedometer, minimap, race information (top right) and the drag race's tachometer, moved where the touch layout editor puts them
// (native/touch.cpp, saved in layout.toml): src/game/hud_layout.cpp moves their FEng objects by these offsets
namespace game {

enum HudItem : size_t { kHudSpeedometer, kHudMinimap, kHudRaceInfo, kHudDragTach, kHudItems };

// how far each is moved, FEng units; the editor writes them, the game thread reads them at each FEng update
inline std::atomic<float> g_hud_x[kHudItems] = {}, g_hud_y[kHudItems] = {};

// each item's box where the game puts it, in shares of the frame (0 to 1 across and down), as HUD_SingleRace.fng
// draws it in a race and in free roam (measured off a 3:2 frame): the editor's boxes
struct HudBox {
    float x0, y0, x1, y1;
};
inline constexpr HudBox kHudBoxes[kHudItems] = {
    {0.67f, 0.63f, 0.85f, 0.87f}, {0.14f, 0.61f, 0.33f, 0.87f}, {0.65f, 0.16f, 0.97f, 0.45f},
    {0.30f, 0.28f, 0.48f, 0.96f}};  // HUD_Drag.fng's arc, the gear over it
// where the game has each item now against where it had it as kHudBoxes was measured, FEng units (the game thread
// writes them as it moves the items, the editor's boxes follow): the drag race's tachometer is not always in one place
inline std::atomic<float> g_hud_base_x[kHudItems] = {}, g_hud_base_y[kHudItems] = {};

// the frame's width and height in the HUD's FEng units: an offset of 100 moved it 381 and 353 pixels on a 3200x2136
// frame (its objects are drawn scaled, not 640x480 to the frame as the menus' are) while the 4:3 layout was stretched
// over the frame. src/game/widescreen.cpp keeps its shape: across is then the frame's aspect times 3/4 of that, and
// kHudBoxes' places (measured stretched) come in towards the middle by 4/3 over the aspect
inline constexpr float kHudUnitsX = 838.0f, kHudUnitsY = 606.0f;
inline float HudUnitsAcross(float aspect) { return kHudUnitsX * 0.75f * aspect; }
inline float HudBoxX(float share, float aspect) { return 0.5f + (share - 0.5f) * (4.0f / 3.0f) / aspect; }

}  // namespace game
