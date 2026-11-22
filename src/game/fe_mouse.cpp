// The front end's menus by touch (and a mouse). Underground 2's FEng kept the PC's mouse: FEngine::Update (0x163050)
// asks the game's interface for the pointer (GetMouseInfo, cFEng's vtable 0x355A08 slot 0x44) while its mouse is on
// (FEngine + 2), and each package taking input (+0x24 bit 0) then hovers its buttons (0x15ABF0): the pointer moving
// onto a button of the package's list (+0x90, count +0x94) makes it the current one (+0x8C) with the highlight
// messages, and the left button pressed on the current one sends it Accept (0x0C407210), as the pad's A. The console
// leaves the mouse off and GetMouseInfo an empty function (0x15B80, shared), so the slot points at a host function
// here and the mouse is on while the screen is being touched: a tap moves the pointer onto the button first, then
// presses, so one tap picks and accepts it.
#include <atomic>
#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <vector>
#include <string>

#include "common/aspect.h"
#include "common/fe_mouse.h"
#include "common/options.h"
#include "core/log.h"
#include "cpu/x86_context.h"
#include "kernel/kernel.h"
#include "kernel/memory.h"

namespace dispatch {
uint32_t HostThunk(X86Func* fn);  // sdk/runtime/cpu/dispatch.cpp
}

namespace game {

namespace {

constexpr uint32_t kCFEng = 0x3FBD08;  // cFEng*, made by 0x1441D0; its FEngine at +8
constexpr uint32_t kFEngine = 0x8;
// FEngine: its mouse on, the first package, the game's interface (cFEng)
constexpr uint32_t kMouseOn = 0x2, kFirstPackage = 0xE4, kInterface = 0x10C;
constexpr uint32_t kGetMouseInfoSlot = 0x44;
// FEPackage: the next, its name, takes input (bit 0), current button, buttons, their count
constexpr uint32_t kNext = 0x4, kName = 0xC, kInput = 0x24, kCurrentButton = 0x8C, kButtons = 0x90, kButtonCount = 0x94;
constexpr int kMaxPackages = 64;
constexpr uint32_t kObjectRect = 0x12A720;  // (object, float rect[4]) cdecl: left, top, right, bottom as hit tested

// the finger in shares of the frame; each touch counts, so one shorter than an update is still seen
std::atomic<float> g_u{0}, g_v{0};
std::atomic<bool> g_down{false};
std::atomic<uint32_t> g_presses{0};
std::atomic<int> g_mouse_frames{0};  // presents the mouse stays on after a touch
constexpr int kMouseFrames = 30;
std::atomic<bool> g_menu{false}, g_intro{false};

// The race's HUD (HUD_SingleRace.fng, races and free roam alike) is loaded before it shows: over the loading screen and
// the opening camera the touch controls waited for nothing. Its tachometer's dial (the image 0x05D19F25 in the cluster
// group 0x5164D4EA, its texture's hash at +0x24) drawn says it is up: the dial's texture is kind 6 to the draw hook,
// and the controls wait for its first drawing after the package came, then stay for as long as it is there. A HUD
// without that dial (another package) waits for nothing, and none waits past kHudWaitMs (the HUD turned off)
constexpr uint32_t kCluster = 0x5164D4EA, kDial = 0x05D19F25, kImageTexture = 0x24;
constexpr uint32_t kObjectHashAt = 0x10, kObjectTypeAt = 0x18, kGroupCountAt = 0x60, kGroupObjectsAt = 0x64;
constexpr uint32_t kPackageObjectsAt = 0x4C;
constexpr int64_t kHudWaitMs = 30000;
std::atomic<uint32_t> g_dial_data{0};  // the dial texture's data address, 0 for none
bool g_dial_drawing = false;          // drawn in the frame being drawn (game thread)
std::atomic<bool> g_hud_waiting{false};

// the arrows drawn: each as the box it covers on the 640x480 frame and the way it points; those of the frame being
// drawn, and of the last one, which a tap is tested against
enum Dir { kRight, kLeft, kDown, kUp };
struct Arrow {
    float x0, y0, x1, y1;
    Dir dir;
};
std::vector<Arrow> g_arrows_building, g_arrows;  // game thread
struct Dot {
    float x, y;
};
struct KeyBox {
    float x0, y0, x1, y1;
};
std::vector<KeyBox> g_keys_building, g_keys;  // the keyboard's keys, as drawn
std::vector<Dot> g_dots_building, g_dots, g_rings_building, g_rings;  // the colours' dots and the selected one's ring
// the arrows' textures, by hash: the circle with an arrow in it, which a quad's corners turn and flip: kind 1 points
// along the texture's u (the icon rows' and the options' lists' arrows, a right one), kind 2 along its v (a shop list
// column's, a down one, told from an up one by its place over or under the screen's middle); kind 3 is a colour's
// dot of a shop's row of them and kind 4 the ring round the selected one; kind 5 a key of the on-screen keyboard (a
// small one, a wide one); a data address of each, found again as screens load them
struct ArrowTexture {
    uint32_t hash;
    int kind;
    uint32_t data;
};
std::vector<ArrowTexture> g_arrow_textures = {{0x367B3E04, 1, 0}, {0xA79FA3B1, 2, 0}, {0x0B114ADA, 3, 0}, {0x9BA6D658, 4, 0}, {0xE6BD94A4, 5, 0}, {0x8C755A15, 5, 0}};

uint32_t Peek(uint32_t va) { return va && mem::IsCommitted(va) && mem::IsCommitted(va + 3) ? kernel::R32(va) : 0; }

uint32_t FEngine() {
    uint32_t c = Peek(kCFEng);
    return c ? Peek(c + kFEngine) : 0;
}

// a point of the frame in FEng's units: its 640x480 over the whole frame, the origin in the middle (MC_Main.fng's
// dialog has its Yes at -163 to -107 and its No at 138 to 180, 154 to 178 down)
void ToFeng(float u, float v, int16_t& x, int16_t& y) {
    x = int16_t(std::lround((u - 0.5f) * 640.0f));
    y = int16_t(std::lround((v - 0.5f) * 480.0f));
}

std::string Name(uint32_t p) {
    std::string out;
    uint32_t s = Peek(p + kName);
    for (uint32_t i = 0; s && i < 48 && mem::IsCommitted(s + i); i++) {
        char c = char(kernel::G<uint8_t>(s + i));
        if (!c) break;
        out += c;
    }
    return out;
}

// The hit test (0x12A720's box) is a button's text alone, much smaller than the button drawn round it: a tap goes to
// the middle of the button nearest it, within a button's reach, so a finger anywhere on it picks it. Disabled buttons
// (flag 0x4000000, which the hover skips too) are left out
constexpr uint32_t kObjectFlags = 0x1C, kDisabled = 0x4000000;
constexpr float kReachX = 120.0f, kReachY = 36.0f;
bool Snap(X86Context& ctx, uint32_t p, int16_t& x, int16_t& y) {
    static uint32_t rect = 0;
    if (!rect) rect = mem::AllocSystem(16);
    uint32_t list = Peek(p + kButtons), n = Peek(p + kButtonCount);
    float best = 1e9f, bx = 0, by = 0;
    for (uint32_t i = 0; rect && list && i < n && i < 64; i++) {
        uint32_t b = Peek(list + i * 4);
        if (!b || (Peek(b + kObjectFlags) & kDisabled)) continue;
        X86Context saved = ctx;
        uint8_t ok = uint8_t(kernel::CallGuest(ctx, kObjectRect, {b, rect}));
        ctx = saved;
        if (!ok) continue;
        float r[4];
        std::memcpy(r, kernel::HostPtr(rect), 16);
        float dx = std::max({r[0] - x, x - r[2], 0.0f}), dy = std::max({r[1] - y, y - r[3], 0.0f});
        if (dx > kReachX || dy > kReachY) continue;
        float d = dx + dy * 3.0f;
        if (d < best) best = d, bx = (r[0] + r[2]) / 2, by = (r[1] + r[3]) / 2;
    }
    if (best > 1e8f) return false;
    x = int16_t(std::lround(bx));
    y = int16_t(std::lround(by));
    return true;
}

constexpr uint32_t kGetTextureInfo = 0xA1580;

// the arrow texture's data address: GetTextureInfo's TextureInfo, its platform record (+0), the D3D texture in that
// (+0x10) and its data (+4); 0 while no screen has loaded it
uint32_t FindArrowTexture(X86Context& ctx, uint32_t hash) {
    X86Context saved = ctx;
    uint32_t info = kernel::CallGuest(ctx, kGetTextureInfo, {hash, 0, 0});
    ctx = saved;
    uint32_t plat = Peek(info), phys = plat ? Peek(plat + 0x10 + 4) : 0;
    return phys ? mem::VirtualFromPhysical(phys & 0x0FFFFFFF) : 0;
}

// The on-screen keyboard (UI_VirtualKeyboard.fng): its keys are buttons whose rects (0x12A720) are in their groups' own
// units, each row of them in a group of its own, so a tap by a rect picks a neighbour. The keys as drawn are known
// (their images, g_keys): the tapped key's row and place in its row, from the top and the left, is the same button's in
// the buttons by their rects; its rect's middle is where the pointer goes
struct Cell {
    float x, y;
};
std::vector<std::vector<Cell>> InRows(std::vector<Cell> cells) {
    std::sort(cells.begin(), cells.end(), [](const Cell& a, const Cell& b) { return a.y < b.y; });
    std::vector<std::vector<Cell>> rows;
    for (const Cell& c : cells) {
        if (rows.empty() || c.y - rows.back().front().y > 6) rows.emplace_back();
        rows.back().push_back(c);
    }
    for (auto& r : rows)
        std::sort(r.begin(), r.end(), [](const Cell& a, const Cell& b) { return a.x < b.x; });
    return rows;
}

// the drawn cells of a row to the buttons of it, by the best straight line between their places across; a row may
// have a key drawn that is no button (a decoration at an end) or the reverse: -1 for those. false when no line fits
bool MatchRow(const std::vector<Cell>& d, const std::vector<Cell>& o, std::vector<int>& to_object) {
    size_t n = d.size(), m = o.size();
    if (n > m + 1 || m > n + 1) return false;
    double best = 1e18;
    std::vector<int> best_map;
    // dropped: a drawn cell (first or last), an object (first or last), or none when the counts are the same
    for (int drop = 0; drop < (n == m ? 1 : 2); drop++) {
        std::vector<int> map(n, -1);
        std::vector<std::pair<double, double>> pairs;
        for (size_t i = 0; i < n; i++) {
            long j = long(i);
            if (n == m + 1 && drop == 0) j--;       // the first drawn one is no button
            else if (n == m + 1 && drop == 1) j = long(i);  // the last
            else if (m == n + 1 && drop == 0) j++;   // the first button is not drawn
            if (j < 0 || j >= long(m) || (n == m + 1 && drop == 1 && i == n - 1)) continue;
            map[i] = int(j);
            pairs.push_back({o[size_t(j)].x, d[i].x});
        }
        if (pairs.size() < 3) continue;
        double sx = 0, sy = 0, sxx = 0, sxy = 0, k = double(pairs.size());
        for (auto& [ox, dx] : pairs) sx += ox, sy += dx, sxx += ox * ox, sxy += ox * dx;
        double den = k * sxx - sx * sx;
        if (std::fabs(den) < 1e-6) continue;
        double scale = (k * sxy - sx * sy) / den, shift = (sy - scale * sx) / k, sse = 0;
        for (auto& [ox, dx] : pairs) sse += (dx - (scale * ox + shift)) * (dx - (scale * ox + shift));
        if (sse < best) best = sse, best_map = map;
    }
    if (best_map.empty() || best / double(n) > 4.0) return false;
    to_object = best_map;
    return true;
}

// where the pointer goes for a tap on the keyboard: the middle of the tapped key's button (its rect's, the units the
// game tests the pointer in); -2000 for a key that is no button; false when the keys are not told apart (the buttons
// nearest the tap then)
bool KeyAt(X86Context& ctx, uint32_t package, float x, float y, int16_t& ox, int16_t& oy) {
    if (g_keys.size() < 20) return false;
    std::vector<Cell> drawn, objects;
    for (const KeyBox& k : g_keys) {
        Cell c{(k.x0 + k.x1) / 2, (k.y0 + k.y1) / 2};
        // the highlighted key is drawn twice, its box and its highlight
        bool twice = std::any_of(drawn.begin(), drawn.end(),
                                 [&](const Cell& d) { return std::fabs(d.x - c.x) < 3 && std::fabs(d.y - c.y) < 3; });
        if (!twice) drawn.push_back(c);
    }
    static uint32_t rect = 0;
    if (!rect) rect = mem::AllocSystem(16);
    uint32_t list = Peek(package + kButtons), n = Peek(package + kButtonCount);
    for (uint32_t i = 0; rect && list && i < n && i < 80; i++) {
        uint32_t b = Peek(list + i * 4);
        if (!b) continue;
        X86Context saved = ctx;
        uint8_t ok = uint8_t(kernel::CallGuest(ctx, kObjectRect, {b, rect}));
        ctx = saved;
        float r[4];
        std::memcpy(r, kernel::HostPtr(rect), 16);
        if (ok) objects.push_back({(r[0] + r[2]) / 2, (r[1] + r[3]) / 2});
    }
    auto dr = InRows(drawn), orows = InRows(objects);
    if (orows.size() < dr.size() || orows.size() > dr.size() + 1) return false;
    // the tapped key: a drawn box with the tap in it
    for (size_t r = 0; r < dr.size(); r++)
        for (size_t c = 0; c < dr[r].size(); c++) {
            bool inside = false;
            for (const KeyBox& k : g_keys)
                if (std::fabs((k.x0 + k.x1) / 2 - dr[r][c].x) < 3 && std::fabs((k.y0 + k.y1) / 2 - dr[r][c].y) < 3 &&
                    x >= k.x0 && x <= k.x1 && y >= k.y0 && y <= k.y1)
                    inside = true;
            if (!inside) continue;
            std::vector<int> to_object;
            if (!MatchRow(dr[r], orows[r], to_object)) return false;
            int j = to_object[c];
            if (j < 0) {
                ox = oy = -2000;
            } else {
                ox = int16_t(std::lround(orows[r][size_t(j)].x));
                oy = int16_t(std::lround(orows[r][size_t(j)].y));
            }
            return true;
        }
    // under the last row of drawn keys, the row the images left out (the space bar)
    if (orows.size() == dr.size() + 1 && y > dr.back().front().y + 8) {
        ox = int16_t(std::lround(orows.back().front().x));
        oy = int16_t(std::lround(orows.back().front().y));
        return true;
    }
    return false;
}

// A colour dot under a finger, of a row or a grid of them (the shop's colours, the paint's): the presses it takes from
// the selected one, the one with the ring round it: rows down (negative up) and columns across (negative left); both 0
// for the selected one itself (A takes it); false for no dot there. The dots lie one step apart, a row's and a
// column's read from its neighbours
bool SwatchAt(float x, float y, int& across, int& down) {
    if (g_dots.size() < 5 || g_rings.empty()) return false;
    const Dot* hit = nullptr;
    float best = 1e9f;
    for (const Dot& d : g_dots) {
        float ex = std::fabs(d.x - x), ey = std::fabs(d.y - y);
        if (ex <= 8 && ey <= 8 && ex + ey < best) best = ex + ey, hit = &d;
    }
    if (!hit) return false;
    float step_x = 1e9f, step_y = 1e9f;
    for (const Dot& d : g_dots) {
        float ex = std::fabs(d.x - hit->x), ey = std::fabs(d.y - hit->y);
        if (ey < 3 && ex > 3) step_x = std::min(step_x, ex);
        if (ex < 3 && ey > 3) step_y = std::min(step_y, ey);
    }
    if (step_x > 40) return false;
    const Dot* ring = &g_rings[0];
    for (const Dot& r : g_rings)
        if (std::fabs(r.x - hit->x) + std::fabs(r.y - hit->y) < std::fabs(ring->x - hit->x) + std::fabs(ring->y - hit->y))
            ring = &r;
    across = int(std::lround((hit->x - ring->x) / step_x));
    down = step_y < 40 ? int(std::lround((hit->y - ring->y) / step_y)) : 0;
    return true;
}

// the arrow under a point of the 640x480 frame, a finger's width round it; -1 for none
constexpr float kArrowReach = 12.0f;
int ArrowAt(float x, float y) {
    for (const Arrow& a : g_arrows)
        if (x >= a.x0 - kArrowReach && x <= a.x1 + kArrowReach && y >= a.y0 - kArrowReach && y <= a.y1 + kArrowReach)
            return int(a.dir);
    return -1;
}

// Touch as a phone's own lists take it: a finger down points at what it is on (the hover makes that button current);
// lifted where it came down it is a tap, which acts; moved past a step first it is a drag, which scrolls, each step
// the pad's dpad (a list's row up or down, an icon scroller's icon left or right), and acts on nothing.
//
// What a tap does, by package:
// - the icon scrollers (the main menu's, the options'): their notification (0x14FE80) answers Accept and the dpad
//   alone, their icons no buttons: on their row the middle icon is A, one beside it left or right as many times as it
//   is icons away, an arrow at the row's end one step
// - the options' rows (UI_Options.fng): a row's value takes the dpad, which its arrows show (the widget's Act steps
//   on left and right, 0x0CECB0): on the value's left half (its left arrow) left, on its right half right; the
//   list's arrows at the top and bottom of its bar up and down; on a row's name the mouse's click, Accept
// - anything else: the mouse's click on the button the finger is on
constexpr int kPadA = 0, kPadUp = 8 + 0, kPadDown = 8 + 1, kPadLeft = 8 + 2, kPadRight = 8 + 3;

// Where the game's menus keep their arrows, as measured on the device (FEng's units, the middle of the frame the
// origin): an icon row's arrows at 183 either side of its middle, the row itself at -125 in the main menu and the
// options', -141 in the screens with a title band over it (track and car select, the shop's categories and parts), -13
// in the pause menu's panel; an options list's own, below. Any screen but a list or a dialog is taken to have such a
// row in its top band: the pointer would click nothing there, the icons being no buttons
struct Scroller {
    const char* package;
    float y;  // the row's middle
};
constexpr Scroller kScrollers[] = {{"UI_Main.fng", -125.0f},        {"UI_OptionsMain.fng", -125.0f},
                                   {"UI_Pause.fng", -13.0f},        {"UI_PauseOptionsMain.fng", -13.0f}};
constexpr float kRowHalf = 50.0f, kIconStep = 105.0f, kArrowX = 160.0f;
constexpr float kBandTop = -185.0f, kBandBottom = -95.0f;

// the game names its packages in no one case (UI_pause.fng, UI_PauseOptions.fng): compared without it
std::string Lower(std::string s) {
    for (char& c : s) c = char(std::tolower(uint8_t(c)));
    return s;
}
bool Contains(const std::string& name, const char* part) { return Lower(name).find(Lower(part)) != std::string::npos; }
bool SameName(const std::string& a, const char* b) { return Lower(a) == Lower(b); }

// the screens after a race and before one (results, stats, rewards, a sponsor's offer): no button to pick, their
// prompt "A Next" is the pad's alone; a tap anywhere on them is A. The pointer clicked nothing there, and with the
// race's controls hidden under the menu the screen could not be left
bool NextScreen(const std::string& name) {
    return Contains(name, "PostRace") || Contains(name, "PreRaceStats") || Contains(name, "Reward") ||
           Contains(name, "SponsorPopup") || Contains(name, "TournResults") || Contains(name, "TournStandings");
}

bool ScrollerAt(const std::string& name, int16_t y) {
    for (const Scroller& s : kScrollers)
        if (SameName(name, s.package)) return std::fabs(y - s.y) <= kRowHalf;
    if (Contains(name, "Options") || Contains(name, "Dialog") || name.rfind("MC_", 0) == 0 || NextScreen(name)) return false;
    return y >= kBandTop && y <= kBandBottom;
}

// the car selects (quick race, career, online, the shop's): the car stands in the middle of the screen, a row of
// brands or cars over it whose selected icon is at the left, not in the middle, so a tap in the row is the arrows' or
// A, never a step by distance; L and R (the category filter, "All Cars") are icons at the top, 56 and 185
bool CarSelect(const std::string& name) {
    return name == "UI_QRCarSelect.fng" || name == "UI_CareerCarSelect.fng" || name == "UI_OLCarSelect.fng" ||
           name == "UI_CareerCarLot.fng" || name == "MU_QuickRaceCarSelect.fng";
}
constexpr int kPadLT = 6, kPadRT = 7;

void ScrollerTap(const std::string& name, int16_t x) {
    if (CarSelect(name)) {
        if (x <= -kArrowX) touch::QueuePad(kPadLeft, 1);
        else if (x >= kArrowX) touch::QueuePad(kPadRight, 1);
        else touch::QueuePad(kPadA, 1);
        return;
    }
    int steps = int(std::lround(x / kIconStep));
    if (std::fabs(float(x)) >= kArrowX) steps = x > 0 ? 1 : -1;
    steps = std::clamp(steps, -3, 3);
    if (steps == 0) touch::QueuePad(kPadA, 1);
    else touch::QueuePad(steps > 0 ? kPadRight : kPadLeft, std::abs(steps));
}

// UI_Options.fng and the race's UI_PauseOptions.fng, measured on the device: a row's left arrow at 4 (-2 in the race's),
// its value's middle at 86, its right arrow at 166 (170, to 177 at its edge); the list's bar at 185 with its up arrow
// at -153 and its down arrow at 156
constexpr float kValueLeft = -30.0f, kValueMiddle = 86.0f, kBarX = 181.0f, kBarArrowY = 135.0f;

bool OptionsList(const std::string& name) { return SameName(name, "UI_Options.fng") || SameName(name, "UI_PauseOptions.fng"); }

// a vertical list down the screen's left (the shop's parts): its arrows at -183, the up one near the list's top
// (-65), the down one near its foot (157)
constexpr float kColumnLeft = -205.0f, kColumnRight = -160.0f;
// a tap on a car select outside its row: the car or the platform is A (select), the L and R icons over the row the
// shoulder buttons
bool CarSelectTap(const std::string& name, int16_t x, int16_t y) {
    if (!CarSelect(name)) return false;
    if (y >= -225 && y < kBandTop) {
        if (x >= 30 && x <= 85) touch::QueuePad(kPadLT, 1);
        else if (x >= 160 && x <= 210) touch::QueuePad(kPadRT, 1);
        else return false;
    } else if (y > kBandBottom && y < 195) {
        touch::QueuePad(kPadA, 1);
    } else {
        return false;
    }
    return true;
}

// The prompts under the screens ("A Install", "Y Help"; the pause menu's "B Back", "A Select" in its panel): their
// icons are glyphs of the text, no images, so their places are the layouts': the footer's left slot A, its right slot Y
constexpr int kPadB = 1, kPadY = 3;
bool PromptTap(const std::string& name, int16_t x, int16_t y) {
    if (name == "UI_Paint.fng" && y >= 140 && y <= 172) {  // the finish's "L Gloss R", over the colours' grid's left
        if (x >= -205 && x <= -160) touch::QueuePad(kPadLT, 1);
        else if (x >= -95 && x <= -50) touch::QueuePad(kPadRT, 1);
        else return false;
        return true;
    }
    if (Contains(name, "HelpDialog")) {  // its "B Back" and the rest of it alike
        touch::QueuePad(kPadB, 1);
        return true;
    }
    if (Contains(name, "Pause")) {
        if (y < 80 || y > 120) return false;
        if (x >= 35 && x <= 95) touch::QueuePad(kPadB, 1);
        else if (x >= 100 && x <= 175) touch::QueuePad(kPadA, 1);
        else return false;
        return true;
    }
    if (y < 168 || y > 222) return false;
    if (x >= -75 && x <= 60) touch::QueuePad(kPadA, 1);
    else if (x >= 130 && x <= 210) touch::QueuePad(kPadY, 1);
    else return false;
    return true;
}

bool ColumnTap(const std::string& name, int16_t x, int16_t y) {
    if (name != "UI_GenericParts_Browser.fng" || x < kColumnLeft || x > kColumnRight) return false;
    if (y >= 130) touch::QueuePad(kPadDown, 1);
    else if (y >= -90 && y <= -45) touch::QueuePad(kPadUp, 1);
    else return false;
    return true;
}

// a tap's pad presses on the options' list; false for the mouse's click
// the bar's arrows a finger's width round them: the rows end before them (-118 to 119), so on the bar's ends the right
// arrows of the rows are not in reach
constexpr float kBarArrowX = 165.0f;
bool OptionsTap(int16_t x, int16_t y) {
    if (x >= kBarArrowX && std::fabs(float(y)) >= kBarArrowY) {
        touch::QueuePad(y < 0 ? kPadUp : kPadDown, 1);
        return true;
    }
    if (x < kValueLeft || x >= kBarX) {
        LOGR("fe: options tap %d,%d is no arrow", x, y);
        return false;
    }
    LOGR("fe: options tap %d,%d -> %s", x, y, x < kValueMiddle ? "left" : "right");
    touch::QueuePad(x < kValueMiddle ? kPadLeft : kPadRight, 1);
    return true;
}

// a drag's step, FEng's units: a list's row, an icon's share of the scroller; and how far a finger goes before it
// drags rather than taps
constexpr float kDragRow = 34.0f, kDragIcon = 90.0f, kDragStart = 14.0f;

// where the finger came down, shares of the frame; the gesture's state for the game thread
std::atomic<float> g_u0{0}, g_v0{0};
std::atomic<bool> g_dragged{false};
std::atomic<uint32_t> g_taps{0};
std::atomic<int> g_drag_x{0}, g_drag_y{0};  // the drag's steps so far, signed

uint32_t g_menu_package = 0;           // game thread

// FEngine::Update's GetMouseInfo(FEMouseInfo&), thiscall: x, y, wheel and the buttons, int16 each. A finger down
// puts the pointer where it came down (away for one update first, as the hover only runs on a move), which makes the
// button there current; a tap clicks it, the button down two updates there; then the pointer goes away, so a resting
// pointer holds no button against the pad. A drag takes the pointer away at once
void GetMouseInfo(X86Context& ctx, uint8_t*) {
    constexpr int kHoldUpdates = 2, kStayUpdates = 4;
    constexpr int16_t kAway = -2000;
    static uint32_t seen_press = 0, seen_tap = 0;
    static int away = 0, hold = 0, stay = 0;
    static bool scroller_press = false, arrow_press = false;
    static int arrow_dir = -1, swatch_across = 0, swatch_down = 0;
    static bool swatch_press = false, key_press = false;
    static bool row_click = false;  // a tap on an options row that opens something: the mouse's click on it
    static int key_a_in = 0;  // updates until the keyboard's key is pressed: its pointer has made it the current key
    static int16_t sx = 0, sy = 0;  // where the finger came down, snapped to its button
    uint32_t info = kernel::Args{ctx}[0];
    std::string name = g_menu_package ? Name(g_menu_package) : std::string();
    if (uint32_t presses = g_presses; presses != seen_press) {
        seen_press = presses;
        ToFeng(g_u0, g_v0, sx, sy);
        // a row that opens something takes the tap anywhere on it, its value's arrows too, but for the list's bar
        int16_t row_x = 0, row_y = 0;
        bool steps = true;
        row_click = OptionsList(name) && g_menu_package && sx < kBarArrowX &&
                    OptionsRowAt(ctx, sy, row_x, row_y, steps) && !steps;
        arrow_dir = row_click ? -1 : ArrowAt(float(sx) + 320.0f, float(sy) + 240.0f);
        swatch_press = arrow_dir < 0 && SwatchAt(float(sx) + 320.0f, float(sy) + 240.0f, swatch_across, swatch_down);
        arrow_press = arrow_dir >= 0 || swatch_press;
        scroller_press = !arrow_press && ScrollerAt(name, sy);
        away = scroller_press || arrow_press ? 0 : 1;
        hold = 0;
        key_press = false;
        if (!scroller_press && !arrow_press && g_menu_package) {
            if (row_click)
                sx = row_x, sy = row_y;  // the row's title, which the click accepts (OPEN, a binding)
            else if (name == "UI_VirtualKeyboard.fng" && KeyAt(ctx, g_menu_package, float(sx) + 320.0f, float(sy) + 240.0f, sx, sy))
                key_press = true;
            else
                Snap(ctx, g_menu_package, sx, sy);
        }
    }
    if (uint32_t taps = g_taps; taps != seen_tap) {
        seen_tap = taps;
        int16_t x0, y0;
        ToFeng(g_u0, g_v0, x0, y0);
        LOGR("fe: tap %d,%d in %s", x0, y0, name.c_str());
        if (swatch_press) {
            if (swatch_down) touch::QueuePad(swatch_down > 0 ? kPadDown : kPadUp, std::abs(swatch_down));
            if (swatch_across) touch::QueuePad(swatch_across > 0 ? kPadRight : kPadLeft, std::abs(swatch_across));
            if (!swatch_down && !swatch_across) touch::QueuePad(kPadA, 1);
        } else if (arrow_press) {
            static constexpr int kPress[] = {kPadRight, kPadLeft, kPadDown, kPadUp};
            touch::QueuePad(kPress[arrow_dir], 1);
        } else if (scroller_press) {
            ScrollerTap(name, x0);
        } else if (key_press) {
            // the pointer made the key the current one; pressed with the pad's A (the mouse's click took the key
            // before it was current and did nothing), unless it is no button
            if (sx != -2000) key_a_in = 8;
        }
        else if (NextScreen(name) && !Peek(g_menu_package + kButtonCount)) {
            touch::QueuePad(kPadA, 1);
        }
        else if (!(OptionsList(name) && !row_click && OptionsTap(x0, y0)) && !ColumnTap(name, x0, y0) && !CarSelectTap(name, x0, y0) && !PromptTap(name, x0, y0)) {
            if (!key_press) hold = kHoldUpdates;
        }
    }
    if (key_a_in > 0 && --key_a_in == 0) touch::QueuePad(kPadA, 1);
    bool pointing = !scroller_press && !arrow_press && !g_dragged && (g_down || hold > 0 || key_a_in > 0);
    bool down = false;
    if (away > 0) {
        away--;
        pointing = false;
    } else if (hold > 0 && !g_down) {
        down = true;
        hold--;
    }
    if (pointing) stay = kStayUpdates;
    else if (stay > 0 && !g_dragged && !scroller_press && !arrow_press) stay--;
    else stay = 0;
    bool here = pointing || stay > 0;
    kernel::W16(info, uint16_t(here ? sx : kAway));
    kernel::W16(info + 2, uint16_t(here ? sy : kAway));
    kernel::W16(info + 4, 0);
    kernel::W16(info + 6, down ? 1 : 0);  // the left button
    kernel::Return(ctx, 0, 1);
}

// the HUD package, its tachometer's dial (the cluster group's image), the dial's texture: 0 for each it lacks
uint32_t DialTextureHash(uint32_t package) {
    int budget = 600;
    for (uint32_t o = Peek(package + kPackageObjectsAt); o && budget-- > 0; o = Peek(o + kNext)) {
        if (Peek(o + kObjectHashAt) != kCluster || Peek(o + kObjectTypeAt) != 5) continue;
        uint32_t n = Peek(o + kGroupCountAt);
        for (uint32_t c = Peek(o + kGroupObjectsAt); c && n-- > 0; c = Peek(c + kNext))
            if (Peek(c + kObjectHashAt) == kDial) return Peek(c + kImageTexture);
        return 0;
    }
    return 0;
}

int64_t NowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

// at each present: whether the controls wait for the HUD, from the frame just drawn
void HudFrame(X86Context& ctx, uint32_t fe) {
    static uint32_t package = 0, hash = 0;
    static int64_t since = 0;
    static bool seen = false;
    static uint32_t retry = 0;
    bool drawn = g_dial_drawing;
    g_dial_drawing = false;
    uint32_t hud = 0;
    int n = 0;
    for (uint32_t p = Peek(fe + kFirstPackage); p && n++ < kMaxPackages; p = Peek(p + kNext))
        if (Name(p).rfind("HUD_", 0) == 0) hud = p;
    if (hud != package) {
        package = hud;
        hash = hud ? DialTextureHash(hud) : 0;
        g_dial_data = hash ? FindArrowTexture(ctx, hash) : 0;
        since = NowMs();
        seen = false;
        LOGR("fe: hud %08X dial texture %08X at %08X", hud, hash, g_dial_data.load());
    } else if (hud && !g_dial_data && ++retry % 30 == 0) {
        // the package's objects and the dial's texture load after the package is listed
        if (!hash) hash = DialTextureHash(hud);
        if (hash) g_dial_data = FindArrowTexture(ctx, hash);
        if (g_dial_data) LOGR("fe: hud %08X dial texture %08X at %08X", hud, hash, g_dial_data.load());
    }
    if (drawn && !seen) LOGR("fe: hud up");
    seen |= drawn;
    g_hud_waiting = package && g_dial_data && !seen && NowMs() - since < kHudWaitMs;
}

}  // namespace

void FeFrame(X86Context& ctx) {
    uint32_t fe = FEngine();
    if (!fe) {
        g_menu = false;
        return;
    }
    static const uint32_t thunk = dispatch::HostThunk(GetMouseInfo);
    if (uint32_t iface = Peek(fe + kInterface)) {
        uint32_t slot = Peek(iface) + kGetMouseInfoSlot;
        if (Peek(iface) && kernel::R32(slot) != thunk) kernel::W32(slot, thunk);
    }
    // a package taking input with buttons to pick: a menu
    uint32_t menu = 0;
    int n = 0;
    for (uint32_t p = Peek(fe + kFirstPackage); p && n < kMaxPackages; p = Peek(p + kNext), n++)
        if ((kernel::G<uint8_t>(p + kInput) & 1) && Peek(p + kButtonCount) > 0) {
            menu = p;
            break;
        }
    // a package taking input with no buttons (a help screen, a cutscene's): its name, once each, for the screens
    // that need a rule of their own
    if (!menu) {
        static std::string quiet;
        n = 0;
        for (uint32_t p = Peek(fe + kFirstPackage); p && n < kMaxPackages; p = Peek(p + kNext), n++)
            if (kernel::G<uint8_t>(p + kInput) & 1) {
                std::string who = Name(p);
                if (who != quiet) {
                    quiet = who;
                    LOGR("fe: input without buttons %08X %s", p, who.c_str());
                }
                // the help over a screen, a tap closes it; a screen of the front end with no buttons to pick, a shop's
                // colours: a menu still, its pointer the finger
                if (who.rfind("HelpDialog", 0) == 0 || who.rfind("UI_", 0) == 0) menu = p;
                break;
            }
    }
    // a race's intro: the caption over its opening camera (Chyron_IG.fng) is up and takes the pad (Start skips it).
    // The same captions come up in free roam over the driving (a place's name, a message) without taking input: those
    // hid the race's controls mid-drive, and a pedal held under them was let go
    bool intro = false;
    {
        int count = 0;
        for (uint32_t p = Peek(fe + kFirstPackage); p && count++ < kMaxPackages; p = Peek(p + kNext))
            if ((kernel::G<uint8_t>(p + kInput) & 1) && Name(p).rfind("Chyron", 0) == 0) intro = true;
    }
    if (intro != g_intro) LOGR("fe: intro %s", intro ? "up" : "gone");
    g_intro = intro;
    static uint32_t logged = ~0u;
    std::string name = menu ? Name(menu) : std::string();
    if (menu != logged) {
        logged = menu;
        LOGR("fe: menu %08X %s", menu, name.c_str());
    }
    g_menu_package = menu;
    g_menu = menu != 0;
    g_arrows = std::move(g_arrows_building);
    g_dots = std::move(g_dots_building);
    g_dots_building.clear();
    g_rings = std::move(g_rings_building);
    g_rings_building.clear();
    g_keys = std::move(g_keys_building);
    g_keys_building.clear();
    g_arrows_building.clear();
    HudFrame(ctx, fe);
    static uint32_t scanned = 0;
    if (!(scanned++ % 30))
        for (ArrowTexture& t : g_arrow_textures) t.data = FindArrowTexture(ctx, t.hash);
    int frames = g_mouse_frames;
    if (g_down) frames = kMouseFrames;
    else if (frames > 0) frames--;
    g_mouse_frames = frames;
    kernel::G<uint8_t>(fe + kMouseOn) = menu && frames > 0;
}

bool FeMenuShown() { return g_menu; }

bool FeIntro() { return g_intro; }

bool FeHudWaiting() { return g_hud_waiting; }


bool FeOnPackage(const char* name) { return g_menu_package && Name(g_menu_package) == name; }

int FeArrowKind(uint32_t data) {
    if (data && data == g_dial_data) return 6;
    for (const ArrowTexture& t : g_arrow_textures)
        if (t.data && t.data == data) return t.kind;
    return 0;
}

// the quad's corners: the way the arrow points is where the texture's right edge (its u past a half) lies from its left
void FeArrowQuad(int kind, const float* px, const float* py, const float* u, const float* v) {
    if (kind == 6) {
        g_dial_drawing = true;
        return;
    }
    if (kind == 5) {
        g_keys_building.push_back({*std::min_element(px, px + 4), *std::min_element(py, py + 4),
                                   *std::max_element(px, px + 4), *std::max_element(py, py + 4)});
        return;
    }
    if (kind >= 3) {
        (kind == 3 ? g_dots_building : g_rings_building)
            .push_back({(px[0] + px[1] + px[2] + px[3]) / 4, (py[0] + py[1] + py[2] + py[3]) / 4});
        return;
    }
    float lx = 0, ly = 0, rx = 0, ry = 0, x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
    int nl = 0, nr = 0;
    const float* axis = kind == 2 ? v : u;
    for (int i = 0; i < 4; i++) {
        (axis[i] < 0.5f ? lx : rx) += px[i];
        (axis[i] < 0.5f ? ly : ry) += py[i];
        (axis[i] < 0.5f ? nl : nr)++;
        x0 = std::min(x0, px[i]), y0 = std::min(y0, py[i]), x1 = std::max(x1, px[i]), y1 = std::max(y1, py[i]);
    }
    if (!nl || !nr) return;
    float dx = rx / nr - lx / nl, dy = ry / nr - ly / nl;
    Dir dir = std::fabs(dx) >= std::fabs(dy) ? (dx > 0 ? kRight : kLeft) : (dy > 0 ? kDown : kUp);
    if (kind == 2) dir = (y0 + y1) < 480.0f ? kUp : kDown;
    g_arrows_building.push_back({x0, y0, x1, y1, dir});
}

void FeTouch(bool down, float u, float v) {
    g_u = u;
    g_v = v;
    if (down && !g_down) {
        g_u0 = u, g_v0 = v;
        g_dragged = false;
        g_drag_x = 0, g_drag_y = 0;
        g_presses++;
    } else if (down) {
        // a drag: the list's rows up or down, the scroller's icons left or right, the content following the finger
        float dx = (u - g_u0) * 640.0f, dy = (v - g_v0) * 480.0f;
        if (!g_dragged && std::max(std::fabs(dx), std::fabs(dy)) >= kDragStart) g_dragged = true;
        if (g_dragged && std::fabs(dx) > std::fabs(dy)) {
            int steps = int(dx / kDragIcon);
            for (int done = g_drag_x; done != steps; done += steps > done ? 1 : -1)
                touch::QueuePad(steps > done ? kPadLeft : kPadRight, 1);
            g_drag_x = steps;
        } else if (g_dragged) {
            int steps = int(dy / kDragRow);
            for (int done = g_drag_y; done != steps; done += steps > done ? 1 : -1)
                touch::QueuePad(steps > done ? kPadUp : kPadDown, 1);
            g_drag_y = steps;
        }
    } else if (g_down && !g_dragged) {
        g_taps++;
    }
    g_down = down;
    g_mouse_frames = kMouseFrames;
}

}  // namespace game
