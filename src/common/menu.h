#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

// the F10 settings panel, drawn by DrawUi (GameInfo::ui)
namespace app {

bool MenuOpen();
bool Capturing();  // a key or controller binding waits for its input
void ToggleMenu();
void DrawUi(double fps);
inline constexpr uint32_t kFpsCapRefresh = 1;  // Settings::fps_cap: just under the display's refresh rate
void PaceFrame();  // app::Pace, with kFpsCapRefresh; game thread, after the present

// the panel's rows
namespace rows {

// a link row opens its page on a tap, an info row is a line of plain text; PadBind binds a controller's input
enum class Kind { Heading, Value, Bind, Link, Info, PadBind };
struct Row {
    Kind kind;
    int id;  // a bind row's input action
    const char* label;
};
// 0 graphics, 1 touch controls, 2 general, 3 mouse and keyboard, 4 controller, 5 credits
const Row* Page(int page, size_t* count);
const char* PageName(int page);  // the tab's, in English capitals
// the game's options menu's rows (src/game/android_options.cpp), any thread the game's: a row's value as the panel
// shows it (in English, capitals), stepped first when step is not zero, and saved; a link's or a binding's action
std::string Value(const Row& r, int step);
void Activate(const Row& r);
// the game's options menu's last row of the KEYBOARD (3) and CONTROLLER (4) pages: their bindings back to the defaults
bool ResetRow(int page, Row* row);
// those steps and actions wait for the UI's frame (the render thread), which counts each batch it applies
uint32_t Applied();

}  // namespace rows

}  // namespace app
