// ANDROID OPTIONS: the settings panel's pages (src/common/menu.cpp), but for My Music, in the game's own options
// menu, drawn as its own options are.
//
// The main menu's Setup (0x0C7110, thiscall on the icon scroller) makes its seven icons (MMCareer ... MMOptions) as
// IconOptions: 0x48 bytes from operator new (0x3ED50, cdecl), the constructor 0x12C0C0 (thiscall: texture hash at
// +0xC, name hash at +0x2C, description hash at +0x30), a vtable of two (the destructor 0x12DB10, React), each added
// by the scroller's vtable slot 0x18. ANDROID OPTIONS is one more after OPTIONS, whose React is OPTIONS' own
// (0x0C7010: UI_OptionsMain.fng) and marks the options it opens as ours. That UIOptionsMain's Setup (0x0CE3A0, its
// eight icons OMAudio ... OMOnline, their React setting the category [0x406CC4]) then makes our sections' icons in
// place of the game's. A section's React picks category 0, which the fade after Accept opens UI_Options.fng for
// (0x0CE278), and the section; UIOptionsScreen then sets up category 0 by 0x0CE680 (the Audio page's), which makes
// our rows instead when a section is picked. A row is a widget as the Player page's (0x0CF4F0): 0x54 bytes, the
// constructor 0x12AF20 (the screen's +0x100, +0x110, +0x118, +0x120, enabled), added by 0x15B2E0 (thiscall on the
// screen: widget, 1), which hands it the package's next title and value strings. Its vtable is the Player page's
// first (0x34F4A0) with Act (+0x4: thiscall, the message second; the dpad's left and right step it, then the arrows
// blink, +0x40, and it draws, +0xC) and Draw (+0xC: its title [+0x2C] and value [+0x30] set to a string hash by
// 0x118BF0) as host functions.
//
// The texts are strings the string table's lookup (SearchForString, 0x1105C0, the hash in edx) answers for hashes of
// ours. The icons' pictures are the main menu's OPTIONS icon's TextureInfo copied (GetTextureInfo, 0xA1580, answers
// our hashes with them), our pixels in its D3D texture (tools/option_icons.py). The pause menu gets ANDROID OPTIONS
// too, which opens the settings panel there.
#include <algorithm>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <SDL3/SDL.h>

#include "common/fe_mouse.h"
#include "common/menu.h"
#include "common/options.h"
#include "common/translations.h"
#include "core/log.h"
#include "cpu/x86_ops.h"
#include "kernel/kernel.h"
#include "kernel/memory.h"

namespace dispatch {
uint32_t HostThunk(X86Func* fn);  // sdk/runtime/cpu/dispatch.cpp
}

namespace game {

namespace {

using app::rows::Kind;
using app::rows::Row;
using kernel::CallGuest;
using kernel::R32;
using kernel::W32;

constexpr uint32_t kNew = 0x3ED50, kIconCtor = 0x12C0C0, kIconDtor = 0x12DB10, kIconBytes = 0x48;
constexpr uint32_t kAddIconSlot = 0x18, kStaysIcon = 0x45;
constexpr uint32_t kGetTextureInfo = 0xA1580, kSetString = 0x118BF0, kFindObject = 0x144360, kSelectedIcon = 0x118120;
constexpr uint32_t kAccept = 0x0C407210, kLeft = 0x9120409E, kRight = 0xB5971BF1;
constexpr uint32_t kOptionsReact = 0x0C7010;  // MMOptions'
// the pause menu's: its OPTIONS' React, UIPauseOptionsMain's Setup (its three icons, by 0x0DA950) and the Audio page it
// opens for category 0 (0x0DAC80). Each sets its texts first: Header_text, Header_text2, Back_button, Select_button
constexpr uint32_t kPauseOptionsReact = 0x0D9FB0;
constexpr uint32_t kCategory = 0x406CC4, kAudioCategory = 0;
// a row: the widget, its constructor and vtable (the Player page's), the screen's add; its title and value strings
constexpr uint32_t kRowBytes = 0x54, kRowCtor = 0x12AF20, kRowVtable = 0x34F4A0, kRowVtableBytes = 0x44;
constexpr uint32_t kAddRow = 0x15B2E0, kRowTitle = 0x2C, kRowValue = 0x30;
constexpr uint32_t kActSlot = 0x4, kDrawSlot = 0xC, kShowSlot = 0x14, kBlinkSlot = 0x40;
// its value's left and right arrows (images, set by 0x15B2E0); an FEng object's flags (+0x1C, bit 0 hidden) and
// FEngSetInvisible and FEngSetVisible (cdecl, the object), which the widget's Show and Hide (+0x14, +0x18) call on each
constexpr uint32_t kRowArrows[] = {0x4C, 0x50};
constexpr uint32_t kObjectFlags = 0x1C, kHidden = 0x1, kSetInvisible = 0x118CE0, kSetVisible = 0x118D30;
// UIOptionsScreen: its package +4, its fields the rows are made with, the header's title (set to the category's name
// by 0x0E6753 before the page's Setup), the screen's own string (+0x2C, 0x0CF943) and the Player page's flag +0x152
constexpr uint32_t kScreenPackage = 0x4, kHeaderTitle = 0x42ADB44C, kScreenString = 0x2C, kScreenFlag = 0x152;
constexpr uint32_t kScreenFields[] = {0x100, 0x110, 0x118, 0x120};

// the main menu's OPTIONS icon, or in a race (its textures alone loaded) the pause menu's
constexpr uint32_t kTemplates[] = {0x3058FE37, 0x520DE4E3, 0xB34993C9};
// ours: hashes nothing of the game's has ("RC" and a number); icon 0 ANDROID OPTIONS, then the sections
constexpr uint32_t kTextureHashes = 0x52430A10, kNameHashes = 0x52430A20;

#ifdef __ANDROID__
constexpr const char* kHostOptions = "Android Options";  // as the game's "Options"
#else
constexpr const char* kHostOptions = "Linux Options";
#endif

// the icons' pictures, as tools/option_icons.py draws them: ANDROID OPTIONS, graphics, the controller, general, the main
// menu's EXIT GAME, the touch controls, the keyboard
enum Picture : uint32_t { kAndroidPicture, kGraphicsPicture, kControllerPicture, kGeneralPicture, kExitPicture,
                          kTouchPicture, kKeyboardPicture, kPictures };
constexpr uint32_t kExitName = 0x40;  // past the tabs' names

#include "game/option_icons.inc"  // kIconCount, kIconWidth, kIconHeight, kIcons (tools/option_icons.py)
static_assert(kIconCount == kPictures, "tools/option_icons.py draws a picture for each of Picture");

// The scroller's tabs: graphics, the touch controls, the controller, the keyboard, general and the credits, each a list
// of its rows (headings left out) with its section's picture. The options screens show 11 rows (UI_PauseOptions.fng:
// a title and a value object per row, found by the row's number; past them a row has neither): a longer list scrolls
// through those 11 (the hook of 0x15E170 below)
struct Tab {
    const char* name;  // English, translated as it is shown
    std::vector<int> pages;
    Picture picture;
};
#ifdef __ANDROID__
const Tab kTabs[] = {{"GRAPHICS", {0}, kGraphicsPicture}, {"TOUCH CONTROLS", {1}, kTouchPicture},
                     {"CONTROLLER", {4}, kControllerPicture}, {"KEYBOARD", {3}, kKeyboardPicture},
                     {"GENERAL", {2, 5}, kGeneralPicture}};
#else
const Tab kTabs[] = {{"GRAPHICS", {0}, kGraphicsPicture}, {"KEYBOARD", {3}, kKeyboardPicture},
                     {"CONTROLLER", {4}, kControllerPicture}, {"TOUCH CONTROLS", {1}, kTouchPicture},
                     {"GENERAL", {2, 5}, kGeneralPicture}};
#endif
constexpr int kTabCount = int(std::size(kTabs));

std::vector<app::rows::Row> TabRows(int tab) {
    std::vector<app::rows::Row> out;
    for (int page : kTabs[tab].pages) {
        size_t count = 0;
        const app::rows::Row* list = app::rows::Page(page, &count);
        for (size_t i = 0; i < count; i++)
            if (list[i].kind != app::rows::Kind::Heading) out.push_back(list[i]);
        app::rows::Row reset;
        if (app::rows::ResetRow(page, &reset)) out.push_back(reset);
    }
    return out;
}

std::string TabName(int tab) { return app::Ui(kTabs[tab].name); }

// ---- text

// the game's fonts are Latin-1: UTF-8 to it, what it lacks as '?'
std::string Latin1(const std::string& text) {
    std::string out;
    for (size_t i = 0; i < text.size(); i++) {
        uint8_t c = uint8_t(text[i]);
        if (c < 0x80) {
            out += char(c);
        } else if ((c & 0xE0) == 0xC0 && i + 1 < text.size()) {
            uint32_t cp = (c & 0x1Fu) << 6 | (uint8_t(text[++i]) & 0x3Fu);
            out += cp < 0x100 ? char(cp) : '?';
        } else {
            while (i + 1 < text.size() && (uint8_t(text[i + 1]) & 0xC0) == 0x80) i++;
            out += '?';
        }
    }
    return out;
}

// the game's options read "Sound Effects Volume", "On": each word's first letter in capitals, but for the short
// forms and anything with a digit in it
std::string TitleCase(std::string text) {
    static const char* const kKeep[] = {"FPS", "CPU", "GPU", "FXAA", "SSAA", "HDR", "CRT", "EA", "UI", "TV", "LT",
                                        "RT", "LB", "RB", "USB", "SDL", "XBOX"};
    size_t start = 0;
    while (start < text.size()) {
        size_t end = text.find_first_of(" /()-,", start);
        if (end == std::string::npos) end = text.size();
        std::string word = text.substr(start, end - start);
        bool keep = std::any_of(word.begin(), word.end(), [](char c) { return std::isdigit(uint8_t(c)); });
        for (const char* k : kKeep) keep |= word == k;
        // the rest of the word in lower case: ASCII, and the capital letters of Latin 1 (U+00C0 to U+00DE but the
        // multiplication sign, UTF-8 C3 80 to C3 9E) as their small ones 0x20 higher
        size_t i = start + 1;
        while (i < end && (uint8_t(text[i]) & 0xC0) == 0x80) i++;  // a first letter of two bytes
        for (; !keep && i < end; i++) {
            uint8_t c = uint8_t(text[i]);
            if (c < 0x80) {
                text[i] = char(std::tolower(c));
            } else if (c == 0xC3 && i + 1 < end) {
                uint8_t d = uint8_t(text[++i]);
                if (d >= 0x80 && d <= 0x9E && d != 0x97) text[i] = char(d + 0x20);
            }
        }
        start = end + 1;
    }
    return text;
}

// the panel's English, in the boot language, as the game writes its own
std::string GameText(const std::string& english) { return Latin1(TitleCase(app::Ui(english))); }

// our strings by hash, each text in guest memory once; the lookup runs on the game's threads
std::mutex g_strings_lock;
std::unordered_map<uint32_t, uint32_t> g_strings;

uint32_t PutString(uint32_t hash, const std::string& text) {
    std::lock_guard lock(g_strings_lock);
    uint32_t& va = g_strings[hash];
    if (!va) {
        va = mem::AllocSystem(uint32_t(text.size() + 1));
        if (va) std::memcpy(kernel::HostPtr(va), text.c_str(), text.size() + 1);
    }
    return hash;
}

// a hash of the text's own, so a changed value is a changed hash (0x118BF0 redraws a string only then)
uint32_t TextHash(const std::string& text) {
    uint32_t h = 2166136261u;
    for (char c : text) h = (h ^ uint8_t(c)) * 16777619u;
    return PutString(0x7E000000u | (h & 0x00FFFFFFu), text);
}

uint32_t LookUp(uint32_t hash) {
    std::lock_guard lock(g_strings_lock);
    auto it = g_strings.find(hash);
    return it == g_strings.end() ? 0 : it->second;
}

// ---- the icons' pictures

std::atomic<uint32_t> g_textures[kIconCount] = {};

// the NV2A's swizzle: x's and y's bits interleaved, x first, while both have them; the longer side's rest above
uint32_t Swizzle(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    uint32_t out = 0, bit = 0;
    for (uint32_t m = 1; m < w || m < h; m <<= 1) {
        if (m < w) out |= (x & m ? 1u : 0u) << bit++;
        if (m < h) out |= (y & m ? 1u : 0u) << bit++;
    }
    return out;
}

uint32_t Log2(uint32_t v) {
    uint32_t n = 0;
    while ((1u << n) < v) n++;
    return n;
}

// TextureInfo (MAIN_ICON_OPTIONS's): its platform record +0, its hash +0x24, itself +0x6C, its pixels +0x70. The
// record's D3D texture (+0x10: data +4, format +12, size +16) is a 128x64 DXT3; ours is A8R8G8B8, swizzled
uint32_t MakeTexture(X86Context& ctx, uint32_t icon) {
    uint32_t tmpl = 0;
    for (uint32_t hash : kTemplates) {
        X86Context saved = ctx;
        tmpl = CallGuest(ctx, kGetTextureInfo, {hash, 0, 0});
        ctx = saved;
        if (tmpl && mem::IsCommitted(tmpl) && R32(tmpl) && mem::IsCommitted(R32(tmpl))) break;
        tmpl = 0;
    }
    if (!tmpl) return 0;
    constexpr uint32_t kInfoBytes = 0x80, kPlatBytes = 0x40, kInfoHash = 0x24, kInfoSelf = 0x6C, kInfoPixels = 0x70;
    constexpr uint32_t kD3D = 0x10;
    uint32_t p = R32(tmpl);
    uint32_t info = mem::AllocSystem(kInfoBytes), plat = mem::AllocSystem(kPlatBytes);
    uint32_t data = mem::AllocContiguous(kIconWidth * kIconHeight * 4, 0, 0xFFFFFFFF, 0x1000);
    if (!info || !plat || !data) return 0;
    std::memcpy(kernel::HostPtr(info), kernel::HostPtr(tmpl), kInfoBytes);
    std::memcpy(kernel::HostPtr(plat), kernel::HostPtr(p), kPlatBytes);
    for (uint32_t y = 0; y < kIconHeight; y++)
        for (uint32_t x = 0; x < kIconWidth; x++)
            W32(data + Swizzle(x, y, kIconWidth, kIconHeight) * 4, kIcons[icon][y * kIconWidth + x]);
    W32(info, plat);
    W32(info + kInfoHash, kTextureHashes + icon);
    if (R32(info + kInfoSelf) == tmpl) W32(info + kInfoSelf, info);
    if (R32(info + kInfoPixels) == mem::VirtualFromPhysical(R32(p + kD3D + 4) & 0x0FFFFFFF)) W32(info + kInfoPixels, data);
    uint32_t d3d = plat + kD3D;
    uint32_t format = (R32(d3d + 12) & 0xFF) | 0x06u << 8 | 1u << 16 | Log2(kIconWidth) << 20 | Log2(kIconHeight) << 24;
    W32(d3d + 4, mem::PhysicalFromVirtual(data));
    W32(d3d + 8, 0);
    W32(d3d + 12, format);
    W32(d3d + 16, 0);
    return info;
}

// ---- the icons

// the options the next UIOptionsMain shows are ours; the section picked in it, -1 for the game's own
bool g_android = false;
int g_section = -1;

std::unordered_map<uint32_t, int> g_section_icons;  // an icon of a section, the section

uint32_t Vtable(X86Func* react) {
    uint32_t vt = mem::AllocSystem(8);
    W32(vt, kIconDtor);
    W32(vt + 4, dispatch::HostThunk(react));
    return vt;
}

// an icon added last to a scroller (thiscall Setup's this): icon 0 ANDROID OPTIONS, then the sections
uint32_t AddIcon(X86Context& ctx, uint32_t menu, uint32_t picture, uint32_t vtable, bool stays, uint32_t name = ~0u) {
    if (name == ~0u) name = picture;
    X86Context saved = ctx;
    uint32_t icon = CallGuest(ctx, kNew, {kIconBytes});
    if (icon) {
        ctx.ecx = icon;
        CallGuest(ctx, kIconCtor, {kTextureHashes + picture, kNameHashes + name, 0});
        W32(icon, vtable);
        // Accept on an icon stops the scroller's input and fades it out for the screen the icon opens (0x14FF5B);
        // one with +0x45 set takes it in place (0x14FF34), as one that opens no screen must
        kernel::G<uint8_t>(icon + kStaysIcon) = stays ? 1 : 0;
        ctx.ecx = menu;
        CallGuest(ctx, R32(R32(menu) + kAddIconSlot), {icon});
    }
    ctx = saved;
    return icon;
}

// IconOption::React(package, message, ...), thiscall, five arguments
void MainMenuReact(X86Context& ctx, uint8_t*) {
    uint32_t esp = ctx.esp, self = ctx.ecx;
    if (R32(esp + 8) == kAccept) g_android = true;
    X86Context saved = ctx;
    ctx.ecx = self;
    CallGuest(ctx, kOptionsReact, {R32(esp + 4), R32(esp + 8), R32(esp + 12), R32(esp + 16), R32(esp + 20)});
    ctx = saved;
    ctx.esp = esp + 4 + 0x14;
}

// the pause menu's: as its OPTIONS (0x0D9FB0: UI_PauseOptionsMain.fng)
void PauseReact(X86Context& ctx, uint8_t*) {
    uint32_t esp = ctx.esp, self = ctx.ecx;
    if (R32(esp + 8) == kAccept) g_android = true;
    X86Context saved = ctx;
    ctx.ecx = self;
    CallGuest(ctx, kPauseOptionsReact, {R32(esp + 4), R32(esp + 8), R32(esp + 12), R32(esp + 16), R32(esp + 20)});
    ctx = saved;
    ctx.esp = esp + 4 + 0x14;
}

// EXIT GAME: Accept closes the app as the system closing it would (native/main.cpp's Dispatch on SDL_EVENT_QUIT)
void ExitReact(X86Context& ctx, uint8_t*) {
    uint32_t esp = ctx.esp;
    if (R32(esp + 8) == kAccept) {
        LOGR("options: exit game");
        SDL_Event e{};
        e.type = SDL_EVENT_QUIT;
        SDL_PushEvent(&e);
    }
    ctx.esp = esp + 4 + 0x14;
}

// The pause menus' Setups add QUIT TO MAIN MENU (and a race's other ways out, the icons with the texture 0x4C9E34E6)
// last: ANDROID OPTIONS goes in before the first of them. While the game's Setup runs the screen has a copy of its
// vtable whose add (+0x18) is PauseAdd, which adds ours ahead of that icon; a menu without one gets ours at its end.
// Moving our icon in the scroller's list afterwards did not do: the scroller keeps more of its order than the list
constexpr uint32_t kQuitTexture = 0x4C9E34E6, kIconTexture = 0xC;
uint32_t g_pause_add = 0;   // the screen's own add while its Setup runs
bool g_pause_ours = false;  // ANDROID OPTIONS is in

void AddPauseIcon(X86Context& ctx, uint32_t menu) {
    static const uint32_t vt = Vtable(PauseReact);
    g_pause_ours = true;
    AddIcon(ctx, menu, 0, vt, false);
}

// add(icon), thiscall
void PauseAdd(X86Context& ctx, uint8_t*) {
    uint32_t esp = ctx.esp, self = ctx.ecx, icon = R32(esp + 4);
    X86Context saved = ctx;
    if (!g_pause_ours && icon && R32(icon + kIconTexture) == kQuitTexture) AddPauseIcon(ctx, self);
    ctx = saved;
    ctx.ecx = self;
    CallGuest(ctx, g_pause_add, {icon});
    ctx = saved;
    ctx.esp = esp + 4 + 4;
}

// a pause menu's Setup (thiscall on its screen) with PauseAdd as the screen's add
void PauseSetup(X86Context& ctx, uint8_t* base, X86Func* setup) {
    static std::unordered_map<uint32_t, uint32_t> copies;  // the screen's vtable, our copy of it
    constexpr uint32_t kVtableBytes = 0x40;
    uint32_t menu = ctx.ecx, vt = R32(menu);
    uint32_t& copy = copies[vt];
    if (!copy && (copy = mem::AllocSystem(kVtableBytes))) {
        std::memcpy(kernel::HostPtr(copy), kernel::HostPtr(vt), kVtableBytes);
        W32(copy + kAddIconSlot, dispatch::HostThunk(PauseAdd));
    }
    g_pause_ours = false;
    g_pause_add = R32(vt + kAddIconSlot);
    if (copy) W32(menu, copy);
    setup(ctx, base);
    W32(menu, vt);
    if (!g_pause_ours) AddPauseIcon(ctx, menu);
}

void SectionReact(X86Context& ctx, uint8_t*) {
    uint32_t esp = ctx.esp;
    auto it = g_section_icons.find(ctx.ecx);
    if (R32(esp + 8) == kAccept && it != g_section_icons.end()) {
        W32(kCategory, kAudioCategory);
        g_section = it->second;
    }
    ctx.esp = esp + 4 + 0x14;
}

// ---- the rows

std::unordered_map<uint32_t, Row> g_rows;  // a widget of ours, its row: the options screen up now's alone

// A tab's rows through the screen's 11 places: the widgets show the rows from g_first on, and the list's up and down
// at its first and last place move that window instead of the cursor (0x15E170 below)
constexpr int kBlankId = -1001;
std::vector<Row> g_section_rows;  // the tab's rows, headings left out
std::vector<uint32_t> g_widgets;  // the widgets with a place, in order
size_t g_first = 0;               // the row the first widget shows
uint32_t g_screen = 0;            // the screen they are on
// a list longer than its places keeps its RESET TO DEFAULTS (the KEYBOARD and CONTROLLER tabs' last row) in the last
// place, the rest scrolling through the places over it
bool g_pinned = false;

bool IsReset(const Row& r) {
    Row reset;
    for (int page : {3, 4})
        if (app::rows::ResetRow(page, &reset) && r.kind == reset.kind && r.id == reset.id) return true;
    return false;
}

// the places the rows scroll through, and those rows
size_t Places() { return g_widgets.size() - (g_pinned ? 1 : 0); }
size_t ScrollRows() { return g_section_rows.size() - (g_pinned ? 1 : 0); }

void MapRows() {
    for (size_t i = 0; i < Places(); i++) {
        size_t at = g_first + i;
        g_rows[g_widgets[i]] = at < ScrollRows() ? g_section_rows[at] : Row{Kind::Info, kBlankId, ""};
    }
    if (g_pinned) g_rows[g_widgets.back()] = g_section_rows.back();
}

void DrawRow(X86Context& ctx, uint32_t self) {
    auto it = g_rows.find(self);
    if (it == g_rows.end()) return;
    const Row& r = it->second;
    std::string shown;
    if (r.id != kBlankId) shown = GameText(app::rows::Value(r, 0));
    uint32_t title = TextHash(r.id == kBlankId ? std::string() : GameText(r.label)), value = TextHash(shown);
    X86Context saved = ctx;
    CallGuest(ctx, kSetString, {R32(self + kRowTitle), title});
    CallGuest(ctx, kSetString, {R32(self + kRowValue), value});
    // the arrows say left and right step the value: a row that opens something (OPEN, a binding) or shows one has none
    bool arrows = r.id != kBlankId && r.kind == Kind::Value;
    for (uint32_t at : kRowArrows) {
        uint32_t arrow = R32(self + at);
        if (!arrow || !mem::IsCommitted(arrow)) continue;
        bool hidden = (R32(arrow + kObjectFlags) & kHidden) != 0;
        if (hidden == arrows) CallGuest(ctx, arrows ? kSetVisible : kSetInvisible, {arrow});
    }
    ctx = saved;
}

// Draw(), thiscall
void RowDraw(X86Context& ctx, uint8_t*) {
    DrawRow(ctx, ctx.ecx);
    ctx.esp += 4;
}

// Show(), thiscall: the widget's, which shows its arrows too; then ours as the row has them
void RowShow(X86Context& ctx, uint8_t*) {
    uint32_t self = ctx.ecx;
    X86Context saved = ctx;
    ctx.ecx = self;
    CallGuest(ctx, R32(kRowVtable + kShowSlot), {});
    ctx = saved;
    DrawRow(ctx, self);
    ctx.esp += 4;
}

// Act(?, message), thiscall: left and right step a value, Accept opens a link or a binding
void RowAct(X86Context& ctx, uint8_t*) {
    uint32_t esp = ctx.esp, self = ctx.ecx, message = R32(esp + 8);
    auto it = g_rows.find(self);
    if (it != g_rows.end() && it->second.id != kBlankId) {
        const Row r = it->second;
        if (r.kind == Kind::Value && (message == kLeft || message == kRight)) app::rows::Value(r, message == kLeft ? -1 : 1);
        else if (message == kAccept) app::rows::Activate(r);
        if (r.kind == Kind::Value) {  // the arrows' blink: the others have none
            X86Context saved = ctx;
            ctx.ecx = self;
            CallGuest(ctx, R32(kRowVtable + kBlinkSlot), {message});
            ctx = saved;
        }
        DrawRow(ctx, self);
    }
    ctx.esp = esp + 4 + 8;
}

uint32_t RowVtable() {
    static uint32_t vt = 0;
    if (!vt) {
        vt = mem::AllocSystem(kRowVtableBytes);
        std::memcpy(kernel::HostPtr(vt), kernel::HostPtr(kRowVtable), kRowVtableBytes);
        W32(vt + kActSlot, dispatch::HostThunk(RowAct));
        W32(vt + kDrawSlot, dispatch::HostThunk(RowDraw));
        W32(vt + kShowSlot, dispatch::HostThunk(RowShow));
    }
    return vt;
}

// the section's rows on the options screen (thiscall's this), as the Player page makes its own; headings are left
// out, as the game's pages have none
void SetupRows(X86Context& ctx, uint32_t screen, int section, bool pause = false) {
    g_rows.clear();
    g_widgets.clear();
    g_section_rows.clear();
    g_first = 0;
    g_pinned = false;
    g_screen = screen;
    g_section_rows = TabRows(section);
    X86Context saved = ctx;
    // a widget a row, until the rows are all in or one finds no place
    for (size_t i = 0; i < g_section_rows.size(); i++) {
        uint32_t w = CallGuest(ctx, kNew, {kRowBytes});
        if (!w) break;
        ctx.ecx = w;
        CallGuest(ctx, kRowCtor, {screen + kScreenFields[0], screen + kScreenFields[1], screen + kScreenFields[2],
                                  screen + kScreenFields[3], 1});
        W32(w, RowVtable());
        g_rows[w] = Row{Kind::Info, kBlankId, ""};
        ctx.ecx = screen;
        CallGuest(ctx, kAddRow, {w, 1});
        if (!R32(w + kRowTitle) || !R32(w + kRowValue)) {
            // no place: out of the screen's list again, or the cursor would step on to it, a blank under the last
            // row. AddRow linked it at the list's end (head +0x50 {next, prev}, the widget's link its +4 {next, prev})
            uint32_t node = w + 4, next = R32(node), prev = R32(node + 4);
            W32(prev, next);
            W32(next + 4, prev);
            g_rows.erase(w);
            break;
        }
        g_widgets.push_back(w);
    }
    if (g_widgets.size() < g_section_rows.size())
        LOGR("options: %d rows through %d places", int(g_section_rows.size()), int(g_widgets.size()));
    g_pinned = g_widgets.size() > 1 && g_widgets.size() < g_section_rows.size() && IsReset(g_section_rows.back());
    MapRows();
    for (uint32_t w : g_widgets) DrawRow(ctx, w);
    uint32_t name = TextHash(Latin1(TitleCase(TabName(section))));
    if (!pause) {
        if (uint32_t header = CallGuest(ctx, kFindObject, {R32(screen + kScreenPackage), kHeaderTitle}))
            CallGuest(ctx, kSetString, {header, name});
        kernel::G<uint8_t>(screen + kScreenFlag) = 1;
        W32(screen + kScreenString, name);
    }
    ctx = saved;
}

// SetString(the package's object by the name's hash, the string's hash): the texts a page sets first
void SetText(X86Context& ctx, uint32_t package, uint32_t name, uint32_t string_hash) {
    constexpr uint32_t kHash = 0x1160C0;  // bStringHash(text), cdecl
    X86Context saved = ctx;
    uint32_t object = CallGuest(ctx, kFindObject, {package, CallGuest(ctx, kHash, {name})});
    CallGuest(ctx, kSetString, {object, string_hash});
    ctx = saved;
}
constexpr uint32_t kHeaderText = 0x34CE4C, kHeaderText2 = 0x3506A0, kBackButton = 0x350644, kSelectButton = 0x350634;
// what the pause options pages say over their rows and in their prompts (the game's own pages' texts)
void PauseTexts(X86Context& ctx, uint32_t package, uint32_t title) {
    SetText(ctx, package, kHeaderText, title);
    SetText(ctx, package, kBackButton, 0x933DD695);
    SetText(ctx, package, kSelectButton, 0x8202C364);
}

void PutNames() {
    static bool done = false;
    if (done) return;
    done = true;
    PutString(kNameHashes, Latin1(kHostOptions));
    PutString(kNameHashes + kExitName, Latin1(TitleCase(app::Ui("EXIT GAME"))));
    for (int t = 0; t < kTabCount; t++) PutString(kNameHashes + 1 + uint32_t(t), Latin1(TitleCase(TabName(t))));
}

}  // namespace

// a tap on the options screen at FEng's y (the middle of the frame 0): the row of ours there, the middle of its title
// (where the pointer clicks it) and whether it steps; the rows that open something (OPEN, a binding) take Accept
// alone, which a tap on their value as the dpad's left or right never gave them
bool OptionsRowAt(X86Context& ctx, int16_t y, int16_t& tx, int16_t& ty, bool& steps) {
    constexpr uint32_t kObjectRect = 0x12A720;  // (object, float rect[4]) cdecl
    constexpr float kRowReach = 17.0f;          // half a row's step
    static uint32_t rect = 0;
    if (!rect) rect = mem::AllocSystem(16);
    float best = kRowReach;
    bool found = false;
    for (uint32_t w : g_widgets) {
        auto it = g_rows.find(w);
        uint32_t title = R32(w + kRowTitle);
        if (!rect || it == g_rows.end() || it->second.id == kBlankId || !title) continue;
        X86Context saved = ctx;
        uint8_t ok = uint8_t(CallGuest(ctx, kObjectRect, {title, rect}));
        ctx = saved;
        if (!ok) continue;
        float r[4];
        std::memcpy(r, kernel::HostPtr(rect), 16);
        float d = std::fabs((r[1] + r[3]) / 2 - float(y));
        if (d > best) continue;
        best = d;
        found = true;
        tx = int16_t(std::lround((r[0] + r[2]) / 2));
        ty = int16_t(std::lround((r[1] + r[3]) / 2));
        steps = it->second.kind == Kind::Value;
    }
    return found;
}

// The widgets of the options screen up now, ours or the game's own pages': each added by 0x15B2E0 (below), the list
// begun again by the first of a new screen's
uint32_t g_widgets_screen = 0;
std::vector<uint32_t> g_screen_widgets;
std::unordered_map<uint32_t, bool> g_sliders;  // those of the widgets that are sliders (0x15B5F0's, the Audio page's)

void NoteWidget(uint32_t screen, uint32_t widget, bool slider) {
    if (screen != g_widgets_screen) {
        g_widgets_screen = screen;
        g_screen_widgets.clear();
        g_sliders.clear();
    }
    g_screen_widgets.push_back(widget);
    if (slider) g_sliders[widget] = true;
}

// an object's rect as hit tested, shown or not: the game's own pages show the arrows of the current row alone
bool ObjectRect(X86Context& ctx, uint32_t object, float r[4]) {
    constexpr uint32_t kObjectRect = 0x12A720;
    static uint32_t rect = 0;
    if (!rect) rect = mem::AllocSystem(16);
    if (!rect || !object || !mem::IsCommitted(object)) return false;
    X86Context saved = ctx;
    uint8_t ok = uint8_t(CallGuest(ctx, kObjectRect, {object, rect}));
    ctx = saved;
    if (ok) std::memcpy(r, kernel::HostPtr(rect), 16);
    return ok != 0;
}

bool OptionsArrowAt(X86Context& ctx, int16_t x, int16_t y, bool& right) {
    constexpr float kRowReach = 17.0f, kArrowReach = 24.0f;
    float best = kRowReach;
    uint32_t row = 0;
    for (uint32_t w : g_screen_widgets) {
        float t[4];
        if (!mem::IsCommitted(w + kRowValue) || !ObjectRect(ctx, R32(w + kRowTitle), t)) continue;
        float d = std::fabs((t[1] + t[3]) / 2 - float(y));
        if (d < best) best = d, row = w;
    }
    // a row of ours that does not step (OPEN, a binding) has its arrows hidden for good
    if (auto it = g_rows.find(row); it != g_rows.end() && it->second.kind != Kind::Value) return false;
    float l[4] = {}, r[4] = {};
    bool lok = false, rok = false;
    if (row && g_sliders.count(row)) {
        // a slider's pictures (0x15B5F0: +0x60 to +0x74 but the value's string at +0x68): its arrows the outermost
        for (uint32_t at : {0x60u, 0x64u, 0x6Cu, 0x70u, 0x74u}) {
            float o[4];
            if (!ObjectRect(ctx, R32(row + at), o)) continue;
            if (!lok || o[0] < l[0]) std::memcpy(l, o, sizeof o), lok = true;
            if (!rok || o[2] > r[2]) std::memcpy(r, o, sizeof o), rok = true;
        }
    } else if (row) {
        lok = ObjectRect(ctx, R32(row + kRowArrows[0]), l);
        rok = ObjectRect(ctx, R32(row + kRowArrows[1]), r);
    }
    if (!row || !lok || !rok) return false;
    if (x < l[0] - kArrowReach || x > r[2] + kArrowReach) return false;
    right = x >= (l[2] + r[0]) / 2;  // the value's halves between them
    return true;
}

// a change the UI applied (src/common/menu.cpp's ApplyPending): our rows on the screen show it
void OptionsFrame(X86Context& ctx) {
    static uint32_t seen = 0;
    uint32_t applied = app::rows::Applied();
    if (applied == seen) return;
    seen = applied;
    if (g_rows.empty() || !(FeOnPackage("UI_Options.fng") || FeOnPackage("UI_PauseOptions.fng"))) return;
    for (uint32_t widget : g_widgets) DrawRow(ctx, widget);
}

}  // namespace game

using namespace game;

// the main menu's Setup (thiscall, the scroller): its icons, then ANDROID OPTIONS and EXIT GAME
X86_EXTERN_FUNC(__imp__sub_000C7110);
X86_FUNC(sub_000C7110) {
    uint32_t menu = ctx.ecx;
    g_android = false;
    g_rows.clear();
    g_widgets.clear();
    PutNames();
    __imp__sub_000C7110(ctx, base);
    static const uint32_t vt = Vtable(MainMenuReact);
    AddIcon(ctx, menu, 0, vt, false);
    static const uint32_t exit_vt = Vtable(ExitReact);
    AddIcon(ctx, menu, kExitPicture, exit_vt, true, kExitName);
}

// the pause menu's Setups, in a race (0x0DA4D0) and in free roam (0x0DA230): theirs with ANDROID OPTIONS before QUIT
// TO MAIN MENU (PauseSetup), which opens the same menu as the main menu's
X86_EXTERN_FUNC(__imp__sub_000DA4D0);
X86_FUNC(sub_000DA4D0) {
    g_android = false;
    PutNames();
    PauseSetup(ctx, base, __imp__sub_000DA4D0);
}
X86_EXTERN_FUNC(__imp__sub_000DA230);
X86_FUNC(sub_000DA230) {
    g_android = false;
    PutNames();
    PauseSetup(ctx, base, __imp__sub_000DA230);
}

// UIOptionsMain's Setup (thiscall, the scroller): ANDROID OPTIONS' has our sections in place of the game's, then
// starts where the game's does (0x0CE641: the icon last picked in the package, then the scroller's own start)
X86_EXTERN_FUNC(__imp__sub_000CE3A0);
X86_FUNC(sub_000CE3A0) {
    g_section = -1;
    g_rows.clear();  // the options screen is gone, its widgets with it
    g_widgets.clear();
    if (!g_android) {
        __imp__sub_000CE3A0(ctx, base);
        return;
    }
    uint32_t menu = ctx.ecx;
    static const uint32_t vt = Vtable(SectionReact);
    for (int t = 0; t < kTabCount; t++)
        if (uint32_t icon = AddIcon(ctx, menu, uint32_t(kTabs[t].picture), vt, false, 1 + uint32_t(t))) g_section_icons[icon] = t;
    X86Context saved = ctx;
    uint32_t start = CallGuest(ctx, kSelectedIcon, {R32(menu + 4)});
    ctx.ecx = menu + 0x50;
    CallGuest(ctx, R32(R32(menu + 0x50) + 0x1C), {start});
    ctx.ecx = menu;
    CallGuest(ctx, R32(R32(menu) + 0x10), {});
    ctx = saved;
    ctx.esp += 4;
}

// UIPauseOptionsMain's Setup (thiscall, the scroller): the main menu's, for the pause menu's options
X86_EXTERN_FUNC(__imp__sub_000DA950);
X86_FUNC(sub_000DA950) {
    g_section = -1;
    g_rows.clear();
    g_widgets.clear();
    if (!g_android) {
        __imp__sub_000DA950(ctx, base);
        return;
    }
    uint32_t menu = ctx.ecx;
    PauseTexts(ctx, R32(menu + 4), 0x1D7BB6C9);
    static const uint32_t vt = Vtable(SectionReact);
    for (int t = 0; t < kTabCount; t++)
        if (uint32_t icon = AddIcon(ctx, menu, uint32_t(kTabs[t].picture), vt, false, 1 + uint32_t(t))) g_section_icons[icon] = t;
    X86Context saved = ctx;
    uint32_t start = CallGuest(ctx, kSelectedIcon, {R32(menu + 4)});
    ctx.ecx = menu + 0x50;
    CallGuest(ctx, R32(R32(menu + 0x50) + 0x1C), {start});
    ctx.ecx = menu;
    CallGuest(ctx, R32(R32(menu) + 0x10), {});
    ctx = saved;
    ctx.esp += 4;
}

// the pause options screen's page of category 0 (its Audio page's): a section's rows when one was picked
X86_EXTERN_FUNC(__imp__sub_000DAC80);
X86_FUNC(sub_000DAC80) {
    if (g_section < 0 || g_section >= kTabCount) {
        __imp__sub_000DAC80(ctx, base);
        return;
    }
    uint32_t screen = ctx.ecx;
    PauseTexts(ctx, R32(screen + 4), TextHash(Latin1(TitleCase(TabName(g_section)))));
    SetText(ctx, R32(screen + 4), kHeaderText2, TextHash(Latin1(TabName(g_section))));
    SetupRows(ctx, screen, g_section, true);
    ctx.esp += 4;
}

// UIOptionsScreen's Setup of category 0 (the Audio page's, thiscall): a section's rows when one was picked
X86_EXTERN_FUNC(__imp__sub_000CE680);
X86_FUNC(sub_000CE680) {
    if (g_section < 0 || g_section >= kTabCount) {
        __imp__sub_000CE680(ctx, base);
        return;
    }
    SetupRows(ctx, ctx.ecx, g_section);
    ctx.esp += 4;
}

// The list screens' messages, 0x1613A0 (thiscall on the screen: the message, the object, two more; ret 0x10), which
// the options screen's (0xB5AD0) runs first: up and down step its current widget (+0x58) through its list (head
// +0x50). On our tab, up at the first place with rows above and down at the last place with rows below move the rows
// through the places and the cursor stays; down at the last place at the end stays there
X86_EXTERN_FUNC(__imp__sub_001613A0);
X86_FUNC(sub_001613A0) {
    constexpr uint32_t kUp = 0x72619778, kDown = 0x911C0A4B, kCurrentWidget = 0x58;
    uint32_t screen = ctx.ecx, message = R32(ctx.esp + 4);
    if (screen != g_screen || g_widgets.empty() || (message != kUp && message != kDown)) {
        __imp__sub_001613A0(ctx, base);
        return;
    }
    // with RESET TO DEFAULTS pinned in the last place the rows scroll through the places over it: down at the last of
    // those moves the cursor onto it once they are at their end
    uint32_t current = R32(screen + kCurrentWidget), last_place = g_widgets[Places() - 1];
    if (message == kUp && current == g_widgets.front() && g_first > 0) {
        g_first--;
    } else if (message == kDown && current == last_place && g_first + Places() < ScrollRows()) {
        g_first++;
    } else if (message == kDown && current == g_widgets.back()) {
        kernel::Return(ctx, 0, 4);
        return;
    } else {
        __imp__sub_001613A0(ctx, base);
        return;
    }
    MapRows();
    for (uint32_t w : g_widgets) DrawRow(ctx, w);
    kernel::Return(ctx, 0, 4);
}

// GetTextureInfo(hash, default, unloaded), cdecl: our icons' hashes get our pictures
X86_EXTERN_FUNC(__imp__sub_000A1580);
X86_FUNC(sub_000A1580) {
    uint32_t icon = R32(ctx.esp + 4) - kTextureHashes;
    if (icon >= kIconCount) {
        __imp__sub_000A1580(ctx, base);
        return;
    }
    if (!g_textures[icon]) g_textures[icon] = MakeTexture(ctx, icon);
    ctx.eax = g_textures[icon];
    ctx.esp += 4;
}

// SearchForString (hash in edx): our strings
X86_EXTERN_FUNC(__imp__sub_001105C0);
X86_FUNC(sub_001105C0) {
    if (uint32_t va = LookUp(ctx.edx)) {
        ctx.eax = va;
        ctx.esp += 4;
        return;
    }
    __imp__sub_001105C0(ctx, base);
}

// a widget into an options screen (thiscall on the screen: the widget, 1): the screen's widgets, for the taps on their
// arrows (OptionsArrowAt)
X86_EXTERN_FUNC(__imp__sub_0015B2E0);
X86_FUNC(sub_0015B2E0) {
    uint32_t screen = ctx.ecx, widget = R32(ctx.esp + 4);
    __imp__sub_0015B2E0(ctx, base);
    NoteWidget(screen, widget, false);
}

// a slider into an options screen (the Audio page's, thiscall on the screen: the widget, one more; ret 8)
X86_EXTERN_FUNC(__imp__sub_0015B5F0);
X86_FUNC(sub_0015B5F0) {
    uint32_t screen = ctx.ecx, widget = R32(ctx.esp + 4);
    __imp__sub_0015B5F0(ctx, base);
    NoteWidget(screen, widget, true);
}
