#pragma once
#include <cstdint>
// the front end's menus by touch and mouse (src/game/fe_mouse.cpp)
struct X86Context;

namespace game {

void FeFrame(X86Context& ctx);  // game thread, at each present
bool FeIntro();      // any thread: a race's intro (the caption over its opening camera) is up
bool FeMenuShown();  // any thread: a menu that takes the pointer is up
// the arrow images the front end draws (its one texture, the circle with an arrow in it, turned and flipped by the
// quad's corners): the texture's data address, 0 until the screen has loaded it, and each such quad drawn, by the
// draw hook (src/d3d/d3d8.cpp), as the corners' places on the 640x480 frame and texture coordinates
int FeArrowKind(uint32_t data);  // 0 not an arrow's texture, else what FeArrowQuad takes
void FeArrowQuad(int kind, const float* px, const float* py, const float* u, const float* v);
bool FeOnPackage(const char* name);  // game thread: that menu is this package (UI_Options.fng)
// any thread: the finger (or the mouse) in shares of the frame as the presenter letterboxes it, down or lifted
void FeTouch(bool down, float u, float v);
// game thread: our options row at a tap's FEng y, its title's middle and whether it steps (src/game/android_options.cpp)
bool OptionsRowAt(X86Context& ctx, int16_t y, int16_t& tx, int16_t& ty, bool& steps);

}  // namespace game

namespace touch {
// any thread: presses of port 0's pad queued, one after another (native/touch.cpp): 0-7 an analog button (A is 0),
// 8 + n the buttons' bit n (the dpad's up down left right, start, back)
void QueuePad(int press, int count);
}  // namespace touch

namespace game {

}  // namespace game
