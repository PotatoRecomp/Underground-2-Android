// the settings: their rows (app::rows) and the panel on F10 and the touch gear
#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <iterator>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_internal.h>  // ClearActiveID, the drag scroll's

#include "app/app.h"
#include "audio/output.h"
#include "common/aspect.h"
#include "common/keys.h"
#include "common/menu.h"
#include "common/mouse_keyboard.h"
#include "common/music.h"
#include "common/fps_place.h"
#include "common/options.h"
#include "common/pad_binds.h"
#include "common/translations.h"

// native/gyro.cpp: steering by tilting the phone
namespace gyro {
uint32_t TiltAngle();  // degrees for a full lock
void StepTiltAngle(int step);
}  // namespace gyro

// native/touch.cpp: the touch controls' layout
namespace touch {
uint32_t ControlsOpacity();
void StepOpacity(int step);
uint32_t ControlsSize();
void StepSize(int step);
const char* StickModeName();
void StepStickMode(int step);
const char* SteeringName();
void StepSteering(int step);
const char* ShiftingName();
void StepShifting(int step);
void EditLayout();
}  // namespace touch

// native/update.cpp: the activity asks GitHub for a newer release and says what it found
namespace host {
void CheckForUpdates();
bool PhoneVibrator();  // native/input.cpp: the phone has a vibrator Android can drive
bool HdrSupported();   // native/vulkan.cpp: the display offers an HDR10 swap chain
// native/stats.cpp: the performance overlay's numbers, percent and degrees, -1 where the device does not tell
void SamplePerf();
float CpuLoad();
float GpuLoad();
float CpuTemp();
float GpuTemp();
float BatteryTemp();
}  // namespace host

namespace app {

Settings DefaultSettings();  // native/main.cpp: a phone's first run, unlimited frame rate and vsync off

namespace {

std::atomic<bool> g_open{false};

// gold on a dark red
constexpr ImVec4 kPanelBg{0.045f, 0.02f, 0.02f, 0.94f};
constexpr ImVec4 kBorder{0.80f, 0.62f, 0.22f, 0.85f};
constexpr ImVec4 kText{0.92f, 0.88f, 0.82f, 1.0f};
constexpr ImVec4 kHighlight{0.42f, 0.07f, 0.05f, 1.0f};
constexpr ImVec4 kHeading{0.92f, 0.72f, 0.28f, 1.0f};
constexpr ImVec4 kDim{0.64f, 0.56f, 0.50f, 1.0f};
constexpr ImVec4 kTab{0.14f, 0.05f, 0.04f, 1.0f};
constexpr ImVec4 kTabHovered{0.26f, 0.07f, 0.05f, 1.0f};
constexpr ImVec4 kButton{0.22f, 0.06f, 0.04f, 1.0f};
// lighter than the highlight, so it shows on a selected row
constexpr ImVec4 kButtonHovered{0.58f, 0.12f, 0.08f, 1.0f};
constexpr ImVec4 kLine{0.92f, 0.72f, 0.28f, 0.35f};
constexpr std::pair<ImGuiCol, ImVec4> kPanelColors[] = {
    {ImGuiCol_WindowBg, kPanelBg}, {ImGuiCol_Border, kBorder}, {ImGuiCol_Text, kText},
    {ImGuiCol_Header, kHighlight}, {ImGuiCol_HeaderHovered, kHighlight}, {ImGuiCol_HeaderActive, kHighlight},
    {ImGuiCol_Button, kButton}, {ImGuiCol_ButtonHovered, kButtonHovered}, {ImGuiCol_ButtonActive, kButtonHovered},
    {ImGuiCol_FrameBg, kTab}, {ImGuiCol_FrameBgHovered, kTabHovered}, {ImGuiCol_FrameBgActive, kHighlight},
    {ImGuiCol_ScrollbarGrab, kHighlight}, {ImGuiCol_ScrollbarGrabHovered, kButtonHovered},
    {ImGuiCol_ScrollbarGrabActive, kHeading}, {ImGuiCol_Separator, kLine}, {ImGuiCol_NavHighlight, kHeading},
};

constexpr uint32_t kAnisoValues[] = {0, 2, 4, 8, 16};
const char* const kAnisoNames[] = {"GAME DEFAULT", "2x", "4x", "8x", "16x"};
constexpr uint32_t kScreenFilters[] = {0, 1, 2, 3, 4};
const char* const kScreenFilterNames[] = {"OFF", "SHARPEN", "VIVID", "SCANLINES", "CRT"};
constexpr uint32_t kAntiAliasings[] = {game::kAaOff, game::kAaFxaa, game::kAaSsaa};
const char* const kAntiAliasingNames[] = {"OFF", "FXAA", "SSAA 2X"};
// the counter's place on the screen, Options::fps_position (common/fps_place.h); top centre is under the settings
// gear, CUSTOM where the touch layout editor dragged it
using game::kFpsTop, game::kFpsTopLeft, game::kFpsTopRight, game::kFpsBottomLeft, game::kFpsBottom,
    game::kFpsBottomRight, game::kFpsCustom, game::kFpsPlaceScale;
constexpr uint32_t kFpsPositions[] = {kFpsTop, kFpsTopLeft, kFpsTopRight, kFpsBottomLeft, kFpsBottom, kFpsBottomRight,
                                      kFpsCustom};
// the counter's look, Options::fps_style
constexpr uint32_t kFpsSimple = 0, kFpsDetailed = 1;
constexpr uint32_t kFpsStyles[] = {kFpsSimple, kFpsDetailed};
const char* const kFpsStyleNames[] = {"SIMPLE", "DETAILED"};
const char* const kFpsPositionNames[] = {"TOP CENTER", "TOP LEFT", "TOP RIGHT", "BOTTOM LEFT", "BOTTOM CENTER",
                                         "BOTTOM RIGHT", "CUSTOM"};
constexpr uint32_t kAudioOutputs[] = {audio::kOutputAuto, audio::kOutputMono, audio::kOutputStereo,
                                      audio::kOutputSurround};
const char* const kAudioOutputNames[] = {"AUTOMATIC", "MONO", "2.0 STEREO", "5.1 SURROUND"};
const char* const kLanguageNames[] = {"ENGLISH", "FRENCH", "GERMAN", "ITALIAN", "SPANISH",
                                      "INDONESIAN", "DUTCH", "SWEDISH", "DANISH", "PORTUGUESE"};  // as game::kLanguages
constexpr const char* kDonateUrl = "https://sociabuzz.com/racas/tribe";
constexpr const char* kGithubUrl = "https://github.com/RanggaCasper";
constexpr const char* kYoutubeUrl = "https://youtube.com/c/@RanggaCasper";
constexpr uint32_t kFpsCaps[] = {30, 60, 75, 90, 120, 144, 165, 240, 360, kFpsCapRefresh, 0};

int StepIndex(int index, int step, int count) {
    if (count <= 0) return 0;
    index = (index + step) % count;
    return index < 0 ? index + count : index;
}

int NearestIndex(const uint32_t* values, int count, uint32_t value) {
    int best = 0;
    uint32_t best_d = UINT32_MAX;
    for (int i = 0; i < count; i++) {
        uint32_t d = values[i] > value ? values[i] - value : value - values[i];
        if (d < best_d) {
            best_d = d;
            best = i;
        }
    }
    return best;
}

// index nearest value, stepped first
template <size_t N>
int StepTable(const uint32_t (&values)[N], uint32_t& value, int step) {
    int i = NearestIndex(values, int(N), value);
    if (step) {
        i = StepIndex(i, step, int(N));
        value = values[i];
    }
    return i;
}

// value clamped to lo to hi, stepped by inc through the values StepIndex wraps over
int StepRange(int value, int lo, int hi, int inc, int step) {
    int count = (hi - lo) / inc + 1;
    return lo + inc * StepIndex((std::clamp(value, lo, hi) - lo) / inc, step, count);
}

void Section(const char* title) {
    float em = ImGui::GetFontSize();
    ImGui::Dummy(ImVec2(0, em * 0.5f));
    ImGui::PushStyleColor(ImGuiCol_Text, kHeading);
    ImGui::TextUnformatted(Ui(title).c_str());
    ImGui::PopStyleColor();
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, em * 0.2f));
}

constexpr float kValueEms = 12.0f;  // the value column's width

// a row's place and size: the label left, the value block (an arrow each side of the value) right
struct RowBox {
    float em;
    float row_w;
    float row_h;
    float value_w;
    float block_w;
    float row_x;
    float row_y;
};

RowBox MeasureRow() {
    const ImGuiStyle& style = ImGui::GetStyle();
    RowBox box;
    box.em = ImGui::GetFontSize();
    box.row_w = ImGui::GetContentRegionAvail().x;
    box.row_h = ImGui::GetFrameHeight();
    box.value_w = box.em * kValueEms;
    box.block_w = box.row_h * 2 + box.value_w + style.ItemSpacing.x * 2;
    box.row_x = ImGui::GetCursorPosX();
    box.row_y = ImGui::GetCursorPosY();
    return box;
}

void RowLabel(const RowBox& box, const char* label) {
    ImGui::SetCursorPos(ImVec2(box.row_x + ImGui::GetStyle().ItemInnerSpacing.x, box.row_y));
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(Ui(label).c_str());
}

// the cursor back to the row's start, then past the whole row
void EndRow(const RowBox& box) {
    ImGui::SetCursorPos(ImVec2(box.row_x, box.row_y));
    ImGui::Dummy(ImVec2(box.row_w, box.row_h));
}

void FillItem() {
    ImGui::GetWindowDrawList()->AddRectFilled(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(),
                                              ImGui::GetColorU32(ImGuiCol_Header));
}

// label left, arrows and value right, the row is the only nav stop
int OptionRow(const char* label, const char* value) {
    const ImGuiStyle& style = ImGui::GetStyle();
    RowBox box = MeasureRow();
    ImGui::PushID(label);
    int step = 0;
    ImGui::Selectable("##row", false, ImGuiSelectableFlags_AllowOverlap, ImVec2(box.row_w, box.row_h));
    if (ImGui::IsItemFocused()) {
        FillItem();
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadLeft)) step = -1;
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow) || ImGui::IsKeyPressed(ImGuiKey_GamepadDpadRight)) step = 1;
    }
    RowLabel(box, label);
    ImGui::PushItemFlag(ImGuiItemFlags_NoNav | ImGuiItemFlags_ButtonRepeat, true);  // held arrows keep stepping
    float block_x = box.row_x + box.row_w - box.block_w;
    ImGui::SetCursorPos(ImVec2(block_x, box.row_y));
    if (ImGui::ArrowButton("##dec", ImGuiDir_Left)) step = -1;
    float value_x = block_x + box.row_h + style.ItemSpacing.x;
    float pad = (box.value_w - ImGui::CalcTextSize(value).x) * 0.5f;
    ImGui::SetCursorPos(ImVec2(value_x + std::max(pad, 0.0f), box.row_y));
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(value);
    ImGui::SetCursorPos(ImVec2(value_x + box.value_w + style.ItemSpacing.x, box.row_y));
    if (ImGui::ArrowButton("##inc", ImGuiDir_Right)) step = 1;
    ImGui::PopItemFlag();
    ImGui::PopID();
    EndRow(box);
    return step;
}

const char* OnOff(bool on) { return on ? "ON" : "OFF"; }

void Flip(bool& on, int step) {
    if (step) on = !on;
}

// applies and saves
void Changed() {
    audio::SetOutputMode(game::Opt().audio_output);
    Save();
}

enum RowId { kRenderResolution, kMaxFps, kVsync, kFpsCounter, kAniso, kAudioOutput, kLanguage, kGyro,
             kGyroSensitivity, kTouchOpacity, kTouchSize, kTouchEdit, kCheckUpdates, kAspectRatio, kScreenFilter,
             kAntiAliasing, kFpsPosition, kFpsTemps, kStickMode, kBloom, kFog, kDonate, kGithub, kYoutube, kFpsUsage,
             kShaderPreload, kRain, kParticles, kShifting, kResetKeyboard, kResetController, kPadPreset, kCheatCash,
             kCheatNitrous, kCheatUnlockAll, kVibration, kHdr, kFpsStyle };

using rows::Kind;
using rows::Row;

// the recomp's settings in pages of their own, grouped within
constexpr Row kGraphicsRows[] = {
    {Kind::Heading, -1, "DISPLAY"},
    {Kind::Value, kRenderResolution, "RENDER RESOLUTION"},
    {Kind::Value, kAspectRatio, "ASPECT RATIO"},
    {Kind::Value, kHdr, "HDR"},
    {Kind::Heading, -1, "PERFORMANCE"},
    {Kind::Value, kVsync, "VERTICAL SYNC"},
    {Kind::Value, kMaxFps, "MAX FRAME RATE"},
    {Kind::Value, kFpsCounter, "FPS COUNTER"},
    {Kind::Value, kFpsStyle, "FPS COUNTER STYLE"},
    {Kind::Value, kFpsPosition, "FPS COUNTER POSITION"},
    {Kind::Value, kFpsUsage, "SHOW CPU / GPU USAGE"},
    {Kind::Value, kFpsTemps, "SHOW TEMPERATURES"},
    {Kind::Value, kShaderPreload, "SHADER PRELOADING"},
    {Kind::Heading, -1, "QUALITY"},
    {Kind::Value, kAniso, "ANISOTROPIC FILTERING"},
    {Kind::Value, kAntiAliasing, "ANTI-ALIASING"},
    {Kind::Value, kBloom, "BLOOM"},
    {Kind::Value, kFog, "FOG"},
    {Kind::Value, kRain, "RAIN"},
    {Kind::Value, kParticles, "PARTICLES"},
    {Kind::Value, kScreenFilter, "SCREEN FILTER"},
};

constexpr Row kTouchRows[] = {
    {Kind::Heading, -1, "TOUCH"},
    {Kind::Value, kGyro, "STEERING"},
    {Kind::Value, kGyroSensitivity, "TILT ANGLE"},
    {Kind::Value, kShifting, "SHIFTING"},
    {Kind::Value, kStickMode, "STICK MODE"},
    {Kind::Value, kVibration, "VIBRATION"},
    {Kind::Value, kTouchOpacity, "CONTROLS OPACITY"},
    {Kind::Value, kTouchSize, "CONTROLS SIZE"},
    {Kind::Link, kTouchEdit, "EDIT CONTROLS POSITION"},
};

// LANGUAGE is the settings panel's: the game takes its own from the console's language
constexpr Row kGeneralRows[] = {
    {Kind::Value, kLanguage, "LANGUAGE"},
    {Kind::Value, kAudioOutput, "AUDIO OUTPUT"},
};

// src/game/game.cpp's ApplyCheats keeps them on while they are set
constexpr Row kCheatRows[] = {
    {Kind::Heading, -1, "CHEATS"},
    {Kind::Value, kCheatCash, "MAX CASH"},
    {Kind::Value, kCheatNitrous, "INFINITE NITROUS"},
    {Kind::Value, kCheatUnlockAll, "UNLOCK ALL"},
};

// the credits' names, an info row's id; as they are, untranslated
const char* const kCreditNames[] = {"Rangga Casper", "bagasromadon", "TGP482",
                                    "SDL, glslang, toml++, Dear ImGui", "xemu, Cxbx-Reloaded"};

// a page of its own: who made the port, and the links to support it
constexpr Row kCreditRows[] = {
    {Kind::Heading, -1, "CREDITS"},
    {Kind::Info, 0, "ANDROID PORT"},
    {Kind::Info, 1, "BUTTON DESIGN"},
    {Kind::Info, 2, "RECOMPILER AND RUNTIME"},
    {Kind::Info, 3, "LIBRARIES"},
    {Kind::Info, 4, "XBOX REFERENCES"},
    {Kind::Heading, -1, "SUPPORT"},
    {Kind::Link, kDonate, "DONATE"},
    {Kind::Link, kGithub, "GITHUB"},
    {Kind::Link, kYoutube, "YOUTUBE"},
};

const std::vector<Row>& InputRows() {
    static const std::vector<Row> list = [] {
        std::vector<Row> out = {{Kind::Heading, -1, "KEYBOARD"}};
        size_t count = 0;
        const input::Action* actions = input::Actions(&count);
        for (size_t i = 0; i < count; i++) out.push_back({Kind::Bind, int(i), actions[i].label});
        return out;
    }();
    return list;
}

const std::vector<Row>& PadRows() {
    static const std::vector<Row> list = [] {
        std::vector<Row> out = {{Kind::Heading, -1, "CONTROLLER"}, {Kind::Value, kPadPreset, "PRESET"}};
        size_t count = 0;
        const input::Action* actions = input::PadActions(&count);
        for (size_t i = 0; i < count; i++) out.push_back({Kind::PadBind, int(i), actions[i].label});
        return out;
    }();
    return list;
}

// the setting an ON / OFF row flips, nullptr for any other row
bool* ToggleFlag(Settings& s, int id) {
    game::Options& o = game::Opt();
    switch (id) {
        case kVsync: return &s.vsync;
        case kFpsCounter: return &o.fps_counter;
        case kFpsTemps: return &o.fps_temps;
        case kFpsUsage: return &o.fps_usage;
        case kBloom: return &o.bloom;
        case kFog: return &o.fog;
        case kRain: return &o.rain;
        case kParticles: return &o.particles;
        case kShaderPreload: return &o.shader_preload;
        case kVibration: return &o.vibration;
        case kHdr: return &o.hdr;
        case kCheatCash: return &o.cheat_cash;
        case kCheatNitrous: return &o.cheat_nitrous;
        case kCheatUnlockAll: return &o.cheat_unlock_all;
        default: return nullptr;
    }
}

// the display rows that are no ON / OFF, stepped first; false for another row
bool StepDisplayRow(int id, int step, Settings& s, char (&text)[64]) {
    game::Options& o = game::Opt();
    switch (id) {
        case kRenderResolution: {
            // 20% to 200% in steps of 5
            if (step) s.render_percent = uint32_t(StepRange(int(s.render_percent), 20, 200, 5, step));
            Mode out = Output();
            std::snprintf(text, sizeof(text), "%u%% (%ux%u)", s.render_percent, out.width * s.render_percent / 100,
                          out.height * s.render_percent / 100);
            return true;
        }
        case kAspectRatio: {
            // 1:1 to the whole screen, an unknown saved value the whole screen's
            constexpr int count = int(std::size(game::kAspects));
            int i = count - 1;
            for (int j = 0; j < count; j++)
                if (game::kAspects[j].code == o.aspect_ratio) i = j;
            if (step) {
                i = StepIndex(i, step, count);
                o.aspect_ratio = game::kAspects[i].code;
            }
            std::snprintf(text, sizeof(text), "%s", game::kAspects[i].name);
            return true;
        }
        case kMaxFps: {
            uint32_t cap = kFpsCaps[StepTable(kFpsCaps, s.fps_cap, step)];
            if (cap == kFpsCapRefresh) std::snprintf(text, sizeof(text), "REFRESH RATE");
            else if (cap) std::snprintf(text, sizeof(text), cap == 60 ? "%u (DEFAULT)" : "%u", cap);
            else std::snprintf(text, sizeof(text), "UNLIMITED");
            return true;
        }
        default:
            return false;
    }
}

// the graphics rows that are no ON / OFF, stepped first; false for another row
bool StepGraphicsRow(int id, int step, Settings& s, char (&text)[64]) {
    game::Options& o = game::Opt();
    switch (id) {
        case kAniso:
            std::snprintf(text, sizeof(text), "%s", kAnisoNames[StepTable(kAnisoValues, s.aniso, step)]);
            return true;
        case kAntiAliasing:
            std::snprintf(text, sizeof(text), "%s", kAntiAliasingNames[StepTable(kAntiAliasings, o.anti_aliasing, step)]);
            return true;
        case kScreenFilter:
            std::snprintf(text, sizeof(text), "%s", kScreenFilterNames[StepTable(kScreenFilters, o.screen_filter, step)]);
            return true;
        case kFpsStyle:
            std::snprintf(text, sizeof(text), "%s", kFpsStyleNames[StepTable(kFpsStyles, o.fps_style, step)]);
            return true;
        case kFpsPosition:
            std::snprintf(text, sizeof(text), "%s", kFpsPositionNames[StepTable(kFpsPositions, o.fps_position, step)]);
            return true;
        default:
            return false;
    }
}

// the general and touch rows that are no ON / OFF, stepped first
void StepInputRow(int id, int step, char (&text)[64]) {
    game::Options& o = game::Opt();
    switch (id) {
        case kAudioOutput:
            std::snprintf(text, sizeof(text), "%s", kAudioOutputNames[StepTable(kAudioOutputs, o.audio_output, step)]);
            break;
        case kGyro:
            // the RACING layout's arrows, its wheel, or the phone tilted
            if (step) touch::StepSteering(step);
            std::snprintf(text, sizeof(text), "%s", touch::SteeringName());
            break;
        case kPadPreset: {
            // a step goes to the next preset (from CUSTOM, the first) and binds it
            if (step) {
                int n = input::PadPresetCount(), now = input::PadPreset();
                input::SetPadPreset(now < 0 ? (step > 0 ? 0 : n - 1) : (now + (step > 0 ? 1 : n - 1)) % n);
            }
            std::snprintf(text, sizeof(text), "%s", input::PadPresetName(input::PadPreset()));
            break;
        }
        case kShifting:
            // the RACING layout's gear lever or its X and Y buttons
            if (step) touch::StepShifting(step);
            std::snprintf(text, sizeof(text), "%s", touch::ShiftingName());
            break;
        case kStickMode:
            // floating sticks come where the thumb lands, fixed ones stay where they are drawn
            if (step) touch::StepStickMode(step);
            std::snprintf(text, sizeof(text), "%s", touch::StickModeName());
            break;
        case kTouchOpacity:
            if (step) touch::StepOpacity(step);
            std::snprintf(text, sizeof(text), "%u%%", touch::ControlsOpacity());
            break;
        case kTouchSize:
            if (step) touch::StepSize(step);
            std::snprintf(text, sizeof(text), "%u%%", touch::ControlsSize());
            break;
        case kCheckUpdates:
            // any step asks; the answer comes in a dialog over the game
            if (step) host::CheckForUpdates();
            std::snprintf(text, sizeof(text), "CHECK");
            break;
        case kGyroSensitivity:
            // the phone's tilt for a full lock, 10 to 60 degrees: less turns quicker
            if (step) gyro::StepTiltAngle(step);
            std::snprintf(text, sizeof(text), "%u\xC2\xB0", gyro::TiltAngle());
            break;
        case kLanguage: {
            int count = int(std::size(game::kLanguages));
            int i = 0;
            while (i < count - 1 && o.language != game::kLanguages[i]) i++;
            // past the languages this build does not offer
            if (step) {
                do i = StepIndex(i, step, count);
                while (!game::LanguageAvailable(game::kLanguages[i]));
                o.language = game::kLanguages[i];
            }
            const char* format = o.language == game::BootLanguage() ? "%s" : "%s (RESTART)";
            std::snprintf(text, sizeof(text), format, kLanguageNames[i]);
            break;
        }
    }
}

// steps the value first when step is not zero
std::string RowText(int id, int step, Settings& s = Get()) {
    char text[64] = "";
    if ((id == kVibration && !host::PhoneVibrator()) || (id == kHdr && !host::HdrSupported())) {
        std::snprintf(text, sizeof(text), "NOT SUPPORTED");  // nothing to step
        return text;
    }
    if (bool* on = ToggleFlag(s, id)) {
        Flip(*on, step);
        std::snprintf(text, sizeof(text), "%s", OnOff(*on));
    } else if (!StepDisplayRow(id, step, s, text) && !StepGraphicsRow(id, step, s, text)) {
        StepInputRow(id, step, text);
    }
    if (step && &s == &Get()) Changed();
    return text;
}

constexpr const char* kUnbound = "UNBOUND";

std::string KeyText(std::string keys) {
    if (keys.empty()) return kUnbound;
    for (char& c : keys) c = c == ',' ? '/' : char(std::toupper(uint8_t(c)));
    return keys;
}

// the panel's tabs
const char* const kPages[] = {"GRAPHICS", "TOUCH CONTROLS", "GENERAL", "KEYBOARD", "CONTROLLER", "CREDITS", "CHEATS"};
constexpr int kSettingsPage = 0;
constexpr int kTouchPage = 1;
constexpr int kGeneralPage = 2;
constexpr int kKeyboardPage = 3;
constexpr int kControllerPage = 4;
constexpr int kCreditsPage = 5;
constexpr int kCheatsPage = 6;

void RestoreDefaults(int page) {
    if (page == kCreditsPage) return;  // nothing to reset
    game::Options& o = game::Opt();
    if (page == kCheatsPage) {
        o.cheat_cash = game::Options{}.cheat_cash;
        o.cheat_nitrous = game::Options{}.cheat_nitrous;
        o.cheat_unlock_all = game::Options{}.cheat_unlock_all;
        Changed();
        return;
    }
    if (page == kKeyboardPage) {
        input::ResetBindings();
        return;
    }
    if (page == kControllerPage) {
        input::ResetPadBindings();
        return;
    }
    game::Options keep = o;
    Get() = DefaultSettings();
    o = game::Options{};
    o.keys = keep.keys;
    o.pad = keep.pad;
    o.cheat_cash = keep.cheat_cash;  // the cheats have their own page to reset
    o.cheat_nitrous = keep.cheat_nitrous;
    o.cheat_unlock_all = keep.cheat_unlock_all;
    Changed();
}

// the row being rebound, -1 when idle; its keys must be released before the next press binds
std::atomic<int> g_capture{-1};
int g_capture_slot = 0;  // its box: 0 the key, 1 the alternate
int g_hovered_box = -1;  // the box under the mouse or focused with a key in it, this frame
bool g_capture_armed = false;
int g_capture_done = 0;  // frame of the last bind, the key that bound must not start another
// the controller row being rebound, -1 when idle: as the keys', its inputs let go before the next press binds; Escape
// or kPadCaptureMs with no press cancels, for a player with the controller alone
std::atomic<int> g_pad_capture{-1};
bool g_pad_capture_armed = false;
std::chrono::steady_clock::time_point g_pad_capture_since;
constexpr std::chrono::seconds kPadCaptureTimeout{5};
// the panel's open page, one tab each
int g_page = kSettingsPage;
// the action's key and its alternate as their boxes show them, the alternate blank when there is none
std::vector<std::string> KeyBoxes(size_t action) {
    std::string keys = input::Binding(action);
    size_t comma = keys.find(',');
    std::string second;
    if (comma != std::string::npos) second = keys.substr(comma + 1);
    second = second.substr(0, second.find(','));
    if (!second.empty()) second = KeyText(second);
    return {KeyText(keys.substr(0, comma)), second};
}

constexpr float kKeyBoxEms = 10.0f;  // a key box's width when the row has two

// label left, a box per key right (the key and its alternate side by side), ending where the other rows' values do;
// Enter or click starts a box's capture, right click or Delete sets `cleared`; returns the box, -1 for none
int BindRow(const char* label, const std::vector<std::string>& keys, int capturing, int& cleared,
            const char* prompt = "PRESS A KEY") {
    const ImGuiStyle& style = ImGui::GetStyle();
    RowBox row = MeasureRow();
    float value_end = row.row_x + row.row_w - row.block_w + row.row_h + style.ItemSpacing.x + row.value_w;
    int n = int(keys.size());
    int pressed = -1;
    float box_w = n > 1 ? row.em * kKeyBoxEms : row.value_w;
    ImGui::PushID(label);
    RowLabel(row, label);
    for (int i = 0; i < n; i++) {
        float x = value_end - float(n - i) * box_w - float(n - 1 - i) * style.ItemSpacing.x;
        ImGui::SetCursorPos(ImVec2(x, row.row_y));
        ImGui::PushID(i);
        if (ImGui::InvisibleButton("##key", ImVec2(box_w, row.row_h))) pressed = i;
        bool deleted = ImGui::IsItemFocused() && ImGui::IsKeyPressed(ImGuiKey_Delete, false);
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right) || deleted) cleared = i;
        bool pointed = ImGui::IsItemHovered() || ImGui::IsItemFocused();
        if (pointed && !keys[i].empty() && keys[i] != kUnbound) g_hovered_box = i;
        bool lit = capturing == i || ImGui::IsItemFocused() || ImGui::IsItemHovered();
        ImGui::GetWindowDrawList()->AddRectFilled(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(),
                                                  ImGui::GetColorU32(lit ? ImGuiCol_Header : ImGuiCol_FrameBg));
        // key names as they are
        std::string value = keys[i];
        if (capturing == i) value = Ui(prompt);
        else if (value == kUnbound) value = Ui(kUnbound);
        float pad = (box_w - ImGui::CalcTextSize(value.c_str()).x) * 0.5f;
        ImGui::SetCursorPos(ImVec2(x + std::max(pad, 0.0f), row.row_y));
        ImGui::AlignTextToFramePadding();
        if (capturing == i) ImGui::PushStyleColor(ImGuiCol_Text, kHeading);
        ImGui::TextUnformatted(value.c_str());
        if (capturing == i) ImGui::PopStyleColor();
        ImGui::PopID();
    }
    ImGui::PopID();
    EndRow(row);
    return pressed;
}

// Escape cancels, any other bindable key or mouse button binds
void PollCapture() {
    if (g_capture < 0) return;
    if (std::chrono::steady_clock::now() - g_pad_capture_since > kPadCaptureTimeout) {  // as a controller's
        g_capture = -1;
        g_capture_done = ImGui::GetFrameCount();
        return;
    }
    int vk = input::PressedKey();
    if (!g_capture_armed) {
        g_capture_armed = !vk;
        return;
    }
    if (!vk) return;
    if (vk != VK_ESCAPE) input::Bind(size_t(g_capture.load()), vk, g_capture_slot);
    g_capture = -1;
    g_capture_done = ImGui::GetFrameCount();
}

// any input of a controller binds once those held when it began are let go
void PollPadCapture() {
    if (g_pad_capture < 0) return;
    std::string input = input::PressedPadInput();
    bool cancel = ImGui::IsKeyPressed(ImGuiKey_Escape, false) ||
                  std::chrono::steady_clock::now() - g_pad_capture_since > kPadCaptureTimeout;
    if (!cancel && !g_pad_capture_armed) {
        g_pad_capture_armed = input.empty();
        return;
    }
    if (!cancel && input.empty()) return;
    if (!cancel) input::PadBind(size_t(g_pad_capture.load()), input);
    g_pad_capture = -1;
    g_capture_done = ImGui::GetFrameCount();
}

// not under a bind or on the button that just bound
bool CaptureFree() { return g_capture < 0 && g_pad_capture < 0 && ImGui::GetFrameCount() > g_capture_done + 1; }

void ValueRow(const Row& r) {
    int step = OptionRow(r.label, Ui(RowText(r.id, 0)).c_str());
    if (step) RowText(r.id, step);
}

const char* LinkUrl(int id) {
    switch (id) {
        case kDonate: return kDonateUrl;
        case kGithub: return kGithubUrl;
        case kYoutube: return kYoutubeUrl;
        default: return nullptr;
    }
}

// label left, OPEN right; a tap, click, Enter or A anywhere on the row opens the page in the browser, or the touch
// layout's editor
void LinkRow(const Row& r) {
    RowBox box = MeasureRow();
    ImGui::PushID(r.label);
    bool open = ImGui::Selectable("##link", false, ImGuiSelectableFlags_AllowOverlap, ImVec2(box.row_w, box.row_h));
    if (ImGui::IsItemFocused() || ImGui::IsItemHovered()) FillItem();
    RowLabel(box, r.label);
    std::string value = Ui("OPEN");
    float value_x = box.row_x + box.row_w - box.block_w + box.row_h + ImGui::GetStyle().ItemSpacing.x;
    float pad = (box.value_w - ImGui::CalcTextSize(value.c_str()).x) * 0.5f;
    ImGui::SetCursorPos(ImVec2(value_x + std::max(pad, 0.0f), box.row_y));
    ImGui::AlignTextToFramePadding();
    ImGui::PushStyleColor(ImGuiCol_Text, kHeading);
    ImGui::TextUnformatted(value.c_str());
    ImGui::PopStyleColor();
    ImGui::PopID();
    EndRow(box);
    if (!open) return;
    if (r.id == kTouchEdit) touch::EditLayout();
    else if (const char* url = LinkUrl(r.id)) SDL_OpenURL(url);
}

// the role left, the name right-aligned at the row's end
void InfoRow(const Row& r) {
    RowBox box = MeasureRow();
    RowLabel(box, r.label);
    if (r.id >= 0 && r.id < int(std::size(kCreditNames))) {
        const char* name = kCreditNames[r.id];
        float x = box.row_x + box.row_w - ImGui::CalcTextSize(name).x - ImGui::GetStyle().ItemInnerSpacing.x;
        ImGui::SetCursorPos(ImVec2(x, box.row_y));
        ImGui::AlignTextToFramePadding();
        ImGui::PushStyleColor(ImGuiCol_Text, kHeading);
        ImGui::TextUnformatted(name);
        ImGui::PopStyleColor();
    }
    EndRow(box);
}

void KeyRow(const Row& r) {
    int cleared = -1;
    int box = BindRow(r.label, KeyBoxes(size_t(r.id)), g_capture == r.id ? g_capture_slot : -1, cleared);
    // a Delete that answered a capture is no clear
    if (cleared >= 0 && CaptureFree()) input::Bind(size_t(r.id), 0, cleared);
    if (box < 0 || !CaptureFree()) return;
    g_capture_slot = box;
    g_pad_capture_since = std::chrono::steady_clock::now();
    g_capture = r.id;
    g_capture_armed = false;
}

// the bound input, else the mapping's; a clear goes back to the mapping
void PadRow(const Row& r) {
    size_t count = 0;
    const input::Action& action = input::PadActions(&count)[r.id];
    std::string bound = input::PadBinding(size_t(r.id));
    int cleared = -1;
    int box = BindRow(r.label, {bound.empty() ? action.defaults : bound}, g_pad_capture == r.id ? 0 : -1, cleared,
                      "PRESS A BUTTON");
    if (cleared >= 0 && CaptureFree()) input::PadBind(size_t(r.id), "");
    if (box < 0 || !CaptureFree()) return;
    g_pad_capture = r.id;
    g_pad_capture_armed = false;
    g_pad_capture_since = std::chrono::steady_clock::now();
}

void DrawRows(int page) {
    size_t count = 0;
    const Row* list = rows::Page(page, &count);
    for (size_t i = 0; i < count; i++) {
        const Row& r = list[i];
        if (r.kind == Kind::Heading) Section(r.label);
        else if (r.kind == Kind::Value) ValueRow(r);
        else if (r.kind == Kind::Link) LinkRow(r);
        else if (r.kind == Kind::Info) InfoRow(r);
        else if (r.kind == Kind::PadBind) PadRow(r);
        else KeyRow(r);
    }
}

void Defaults() {
    g_capture = -1;
    g_pad_capture = -1;
    RestoreDefaults(g_page);
}

bool g_music_tab = true;  // the panel opens on My Music; its other tab is the cheats

// the panel's pages and My Music, one button each, the open one lit
void DrawTitle(float em) {
    auto tab = [&](const char* label, bool open) {
        if (open) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        bool pressed = ImGui::Button(label, ImVec2(em * 10, 0));
        if (open) ImGui::PopStyleColor();
        return pressed;
    };
    const float avail = ImGui::GetContentRegionAvail().x;
    float used = 0;
    auto place = [&](float w) {
        if (used > 0 && used + w <= avail) ImGui::SameLine();
        else if (used > 0) used = 0;
        used += w + ImGui::GetStyle().ItemSpacing.x;
    };
    for (int page = 0; page < int(std::size(kPages)); page++) {
        place(em * 10);
        if (tab(Ui(kPages[page]).c_str(), !g_music_tab && g_page == page)) {
            g_music_tab = false;
            g_page = page;
            g_capture = -1;
            g_pad_capture = -1;
        }
    }
    place(em * 10);
    if (tab("MY MUSIC", g_music_tab)) g_music_tab = true;
}

// My Music: what plays and how, then every song in the folder, with a tap to play it, where it may play and its place in the list
void DrawMusic(float em) {
    using namespace music;
    // one line: where the songs go and where their volume is set, Rescan at its end
    const char* rescan = "Rescan";
    float rescan_w = ImGui::CalcTextSize(rescan).x + ImGui::GetStyle().FramePadding.x * 2;
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(kDim, "%s", HasSongs() ? "Underground2/music  -  volume in the game's Audio options"
                                              : "No songs: copy MP3, M4A, OGG, FLAC or WAV to Underground2/music");
    ImGui::SameLine(std::max(ImGui::GetContentRegionMax().x - rescan_w, ImGui::GetCursorPosX()));
    if (ImGui::Button(rescan)) Rescan();
    if (!HasSongs()) return;
    float button = em * 7;
    bool mine = Source() == kMyMusic;
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Plays");
    ImGui::SameLine(em * 5);
    if (ImGui::Button(mine ? "My Music" : "EA Trax", ImVec2(button, 0))) SetSource(mine ? kEaTrax : kMyMusic);
    ImGui::SameLine();
    if (ImGui::Button(Shuffle() ? "Random" : "Sequential", ImVec2(button, 0))) SetShuffle(!Shuffle());
    ImGui::SameLine();
    if (ImGui::Button("Next", ImVec2(button, 0))) Next();
    ImGui::Separator();
    static const char* const kModeNames[] = {"Off", "Menus", "Racing", "All"};
    std::vector<Entry> list = List();
    int current = Current();
    float row_end = ImGui::GetContentRegionAvail().x;
    float mode_w = em * 5, arrow_w = em * 2.2f, spacing = ImGui::GetStyle().ItemSpacing.x;
    float side = mode_w + arrow_w * 2 + spacing * 3;
    for (size_t i = 0; i < list.size(); i++) {
        ImGui::PushID(int(i));
        const Song& song = list[i].song;
        std::string name = std::to_string(i + 1) + "  " + song.title;
        if (!song.artist.empty()) name += " - " + song.artist;
        if (mine && int(i) == current) ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 214, 90, 255));
        if (ImGui::Selectable(name.c_str(), false, ImGuiSelectableFlags_None, ImVec2(row_end - side, ImGui::GetFrameHeight()))) PlayNow(i);
        if (mine && int(i) == current) ImGui::PopStyleColor();
        ImGui::SameLine(row_end - side + spacing);
        if (ImGui::Button(kModeNames[list[i].mode & 3], ImVec2(mode_w, 0))) SetMode(i, uint8_t((list[i].mode + 1) & 3));
        ImGui::SameLine();
        if (ImGui::Button("^", ImVec2(arrow_w, 0))) Move(i, -1);
        ImGui::SameLine();
        if (ImGui::Button("v", ImVec2(arrow_w, 0))) Move(i, 1);
        ImGui::PopID();
    }
}

constexpr const char* kCredit = "Port by Rangga Casper";

// the hint line, then the credit on the left and Back and Defaults on the right
void DrawFooter(float em) {
    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_Text, kDim);
    const char* hint = "";
    if (g_capture >= 0) hint = "Press a key or mouse button to bind, Escape cancels";
    else if (g_pad_capture >= 0) hint = "Press a button on the controller to bind, wait 5 seconds to cancel";
    ImGui::TextUnformatted(Ui(hint, false).c_str());
    ImGui::PopStyleColor();
    float bw = em * 8;
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(kDim, "%s", kCredit);
    ImGui::SameLine(0, 0);
    float buttons_pad = ImGui::GetContentRegionAvail().x - bw * 2 - ImGui::GetStyle().ItemSpacing.x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(buttons_pad, 0.0f));
    if (ImGui::Button(Ui("Back", false).c_str(), ImVec2(bw, 0))) ToggleMenu();
    ImGui::SameLine();
    if (g_music_tab) ImGui::Dummy(ImVec2(bw, 0));
    else if (ImGui::Button(Ui("Defaults", false).c_str(), ImVec2(bw, 0))) Defaults();
}

// The rows a finger drags up or down scroll with it, as a phone's lists do: the row it came down on is let go, so the
// drag presses nothing. A drag of the scrollbar itself stays the scrollbar's.
void DragScroll() {
    static bool dragging = false;
    const ImGuiIO& io = ImGui::GetIO();
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        dragging = false;
        return;
    }
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (!dragging) {
        bool touch = io.MouseSource == ImGuiMouseSource_TouchScreen;
        bool inside = window->Rect().Contains(io.MouseClickedPos[ImGuiMouseButton_Left]);
        bool scrollbar = ImGui::GetActiveID() == ImGui::GetWindowScrollbarID(window, ImGuiAxis_Y);
        float moved = std::fabs(ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 0.0f).y);
        dragging = touch && inside && !scrollbar && moved > ImGui::GetFontSize() * 0.6f;
        if (!dragging) return;
    }
    ImGui::SetScrollY(ImGui::GetScrollY() - io.MouseDelta.y);
    ImGui::ClearActiveID();
}

void DrawPanel() {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    float em = ImGui::GetFontSize();
    ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(em * 40, em * 16), ImGuiCond_Always);
    for (const auto& [col, color] : kPanelColors) ImGui::PushStyleColor(col, color);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(em * 1.6f, em * 1.2f));
    // a scrollbar a thumb can take
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, em * 1.4f);
    ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize, em * 2.0f);
    constexpr ImGuiWindowFlags kFlags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                                        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoTitleBar;
    // the panel is the cheats and My Music: the settings are rows of the game's own OPTIONS menu (src/game/android_options.cpp)
    if (ImGui::Begin("##settings", nullptr, kFlags)) {
        auto tab = [&](const char* label, bool open) {
            if (open) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            bool pressed = ImGui::Button(Ui(label).c_str(), ImVec2(em * 10, 0));
            if (open) ImGui::PopStyleColor();
            return pressed;
        };
        if (tab("CHEATS", !g_music_tab)) {
            g_music_tab = false;
            g_page = kCheatsPage;
        }
        ImGui::SameLine();
        if (tab("MY MUSIC", g_music_tab)) g_music_tab = true;
        ImGui::Separator();
        float footer = ImGui::GetTextLineHeightWithSpacing() + ImGui::GetFrameHeightWithSpacing() + em;
        ImGui::BeginChild("##rows", ImVec2(0, -footer), ImGuiChildFlags_NavFlattened);
        g_hovered_box = -1;
        if (g_music_tab) DrawMusic(em);
        else DrawRows(kCheatsPage);
        DragScroll();
        ImGui::EndChild();
        DrawFooter(em);
    }
    ImGui::End();
    ImGui::PopStyleVar(6);
    ImGui::PopStyleColor(int(std::size(kPanelColors)));
}

// the window's display's refresh rate, 0 when unreadable
uint32_t RefreshRate() {
    SDL_Window* w = static_cast<SDL_Window*>(Window());
    const SDL_DisplayMode* m = SDL_GetCurrentDisplayMode(w ? SDL_GetDisplayForWindow(w) : SDL_GetPrimaryDisplay());
    return m && m->refresh_rate > 1 ? uint32_t(std::lround(m->refresh_rate)) : 0;
}

// as app::Pace, at this cap
void PaceAt(uint32_t cap) {
    using clock = std::chrono::steady_clock;
    static clock::time_point next = clock::now();
    auto now = clock::now();
    if (!cap) {
        next = now;
        return;
    }
    auto period = std::chrono::nanoseconds(1000000000ull / cap);
    next += period;
    if (next < now - period) next = now;
    while (clock::now() + std::chrono::milliseconds(2) < next) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    while (clock::now() < next) std::this_thread::yield();
}

}  // namespace

void PaceFrame() {
    if (Get().fps_cap != kFpsCapRefresh) {
        Pace();
        return;
    }
    // a few under, so a variable refresh display stays in its range and vsync never waits a whole refresh
    constexpr uint32_t kRefreshMargin = 3;
    uint32_t rate = RefreshRate();
    PaceAt(rate > kRefreshMargin ? rate - kRefreshMargin : 0);
}

const rows::Row* rows::Page(int page, size_t* count) {
    if (page == kKeyboardPage) {
        *count = InputRows().size();
        return InputRows().data();
    }
    if (page == kControllerPage) {
        *count = PadRows().size();
        return PadRows().data();
    }
    if (page == kTouchPage) {
        *count = std::size(kTouchRows);
        return kTouchRows;
    }
    if (page == kGeneralPage) {
        *count = std::size(kGeneralRows);
        return kGeneralRows;
    }
    if (page == kCreditsPage) {
        *count = std::size(kCreditRows);
        return kCreditRows;
    }
    if (page == kCheatsPage) {
        *count = std::size(kCheatRows);
        return kCheatRows;
    }
    *count = std::size(kGraphicsRows);
    return kGraphicsRows;
}

const char* rows::PageName(int page) {
    return page >= 0 && page < int(std::size(kPages)) ? kPages[page] : "";
}

// The game's options menu runs on the game thread, mid-frame; a setting changed there (the render resolution) moved
// under the renderer and hung it. Its steps and actions wait here for the UI's frame, as the panel's own run
std::mutex g_pending_lock;
std::vector<std::pair<Row, int>> g_pending;  // a row, its step (0 for an action)
std::atomic<uint32_t> g_applied{0};

void ApplyPending() {
    std::vector<std::pair<Row, int>> pending;
    {
        std::lock_guard lock(g_pending_lock);
        pending.swap(g_pending);
    }
    for (const auto& [r, step] : pending) {
        if (step) {
            RowText(r.id, step);
        } else if (r.kind == Kind::Link && (r.id == kResetKeyboard || r.id == kResetController)) {
            g_capture = -1;
            g_pad_capture = -1;
            RestoreDefaults(r.id == kResetKeyboard ? kKeyboardPage : kControllerPage);
        } else if (r.kind == Kind::Link) {
            if (r.id == kTouchEdit) touch::EditLayout();
            else if (const char* url = LinkUrl(r.id)) SDL_OpenURL(url);
        } else if ((r.kind == Kind::Bind || r.kind == Kind::PadBind) && !Capturing()) {
            // the game's options menu waits for the input in place, as Most Wanted's does: the row reads PRESS A KEY
            // or PRESS A BUTTON, the next one binds (its value then), Escape or kPadCaptureTimeout gives up
            g_pad_capture_since = std::chrono::steady_clock::now();
            if (r.kind == Kind::Bind) {
                g_capture_slot = 0;
                g_capture_armed = false;
                g_capture = r.id;
            } else {
                g_pad_capture_armed = false;
                g_pad_capture = r.id;
            }
        }
    }
    if (!pending.empty()) g_applied++;
}

uint32_t rows::Applied() { return g_applied; }

bool rows::ResetRow(int page, Row* row) {
    if (page != kKeyboardPage && page != kControllerPage) return false;
    *row = {Kind::Link, page == kKeyboardPage ? kResetKeyboard : kResetController, "RESET TO DEFAULTS"};
    return true;
}

std::string rows::Value(const Row& r, int step) {
    if (step && r.kind == Kind::Value) {
        std::lock_guard lock(g_pending_lock);
        g_pending.push_back({r, step});
    }
    switch (r.kind) {
        case Kind::Value: return RowText(r.id, 0);
        case Kind::Link: return r.id == kResetKeyboard || r.id == kResetController ? "RESET" : "OPEN";
        case Kind::Info: return r.id >= 0 && r.id < int(std::size(kCreditNames)) ? kCreditNames[r.id] : "";
        case Kind::Bind: return g_capture == r.id ? "PRESS A KEY" : KeyBoxes(size_t(r.id))[0];
        case Kind::PadBind: {
            if (g_pad_capture == r.id) return "PRESS A BUTTON";
            size_t count = 0;
            std::string bound = input::PadBinding(size_t(r.id));
            return bound.empty() ? input::PadActions(&count)[r.id].defaults : bound;
        }
        default: return "";
    }
}

// a link opens its page or the touch layout's editor; a binding waits for its input (ApplyPending)
void rows::Activate(const Row& r) {
    if (r.kind != Kind::Link && r.kind != Kind::Bind && r.kind != Kind::PadBind) return;
    std::lock_guard lock(g_pending_lock);
    g_pending.push_back({r, 0});
}

bool MenuOpen() { return g_open.load(std::memory_order_relaxed); }

bool Capturing() { return g_capture >= 0 || g_pad_capture >= 0; }

void ToggleMenu() {
    bool open = !MenuOpen();
    g_open.store(open, std::memory_order_relaxed);
    g_capture = -1;
    g_pad_capture = -1;
    if (open) input::ReleaseCursor();
}

// the last frames' times in ms, as the render thread draws the UI once a present; a ring, the newest before g_ft_head
constexpr int kFrameHistory = 240;
float g_ft[kFrameHistory] = {};
int g_ft_head = 0, g_ft_count = 0;
std::chrono::steady_clock::time_point g_ft_last{};

void RecordFrame() {
    auto now = std::chrono::steady_clock::now();
    float ms = std::chrono::duration<float, std::milli>(now - g_ft_last).count();
    g_ft_last = now;
    // a pause (the app in the background, a load holding the presents) is no frame: the graph starts again
    if (ms > 1000.0f) {
        g_ft_count = 0;
        return;
    }
    g_ft[g_ft_head] = ms;
    g_ft_head = (g_ft_head + 1) % kFrameHistory;
    g_ft_count = std::min(g_ft_count + 1, kFrameHistory);
}

// the i-th frame back, 0 the newest
float FrameAgo(int i) { return g_ft[(g_ft_head - 1 - i + kFrameHistory * 2) % kFrameHistory]; }

struct FrameStats {
    float avg_ms = 0, max_ms = 0, low_fps = 0;  // over the last second; the 1% low over the whole history
};

FrameStats Stats() {
    FrameStats st;
    float sum = 0;
    int n = 0;
    while (n < g_ft_count && sum < 1000.0f) {
        float ms = FrameAgo(n++);
        sum += ms;
        st.max_ms = std::max(st.max_ms, ms);
    }
    if (n) st.avg_ms = sum / float(n);
    if (g_ft_count) {
        // the frame time 1% of the history is slower than
        float sorted[kFrameHistory];
        for (int i = 0; i < g_ft_count; i++) sorted[i] = FrameAgo(i);
        int k = g_ft_count / 100;
        std::nth_element(sorted, sorted + k, sorted + g_ft_count, [](float a, float b) { return a > b; });
        st.low_fps = sorted[k] > 0 ? 1000.0f / sorted[k] : 0;
    }
    return st;
}

// a frame time's colour: 50 fps or more green, down to 30 yellow, slower red
ImU32 FrameColor(float ms, int alpha = 255) {
    if (ms <= 20.0f) return IM_COL32(110, 220, 120, alpha);
    if (ms <= 33.4f) return IM_COL32(240, 200, 80, alpha);
    return IM_COL32(240, 90, 80, alpha);
}

void DrawShadowed(ImDrawList* dl, float size, ImVec2 at, ImU32 color, const char* text) {
    float shadow = std::max(1.0f, size / 16.0f);
    dl->AddText(ImGui::GetFont(), size, ImVec2(at.x + shadow, at.y + shadow), IM_COL32(0, 0, 0, 180), text);
    dl->AddText(ImGui::GetFont(), size, at, color, text);
}

float TextWidth(float size, const char* text) { return ImGui::GetFont()->CalcTextSizeA(size, FLT_MAX, 0, text).x; }
float TextHeight(float size) { return ImGui::GetFont()->CalcTextSizeA(size, FLT_MAX, 0, "0").y; }

constexpr ImU32 kLabel = IM_COL32(160, 165, 180, 255), kValue = IM_COL32(240, 240, 245, 255);
// the credit in a warm gold, opaque: it reads over the game in both styles
constexpr ImU32 kCreditColor = IM_COL32(255, 205, 110, 255);

// a temperature's colour: warm from 55, hot from 70
ImU32 TempColor(float c) {
    if (c >= 70.0f) return IM_COL32(240, 90, 80, 255);
    if (c >= 55.0f) return IM_COL32(240, 200, 80, 255);
    return kValue;
}

// a label and its value in a small rounded box of their own
struct Chip {
    std::string label, value;
    ImU32 color;
};

// "CPU" with what the device tells of it: its load, its temperature coloured when warm; none when it tells nothing
void AddPerfChip(std::vector<Chip>& chips, const char* name, float load, float temp) {
    if (load < 0 && temp < 0) return;
    char v[32] = "";
    int n = 0;
    if (load >= 0) n += std::snprintf(v + n, sizeof(v) - n, "%.0f%%", load);
    if (temp >= 0) std::snprintf(v + n, sizeof(v) - n, "%s%.0f\xC2\xB0", n ? " " : "", temp);
    chips.push_back({name, v, temp >= 0 ? TempColor(temp) : kValue});
}

// the place Options::fps_position picks for a box of this size
ImVec2 FpsCorner(float box_w, float box_h) {
    ImVec2 screen = ImGui::GetIO().DisplaySize;
    float margin = screen.y * 0.03f;
    uint32_t pos = game::Opt().fps_position;
    bool bottom = pos == kFpsBottomLeft || pos == kFpsBottom || pos == kFpsBottomRight;
    // top centre sits under the settings gear, bottom centre over Back and Start, which the touch controls keep there
    float y = pos == kFpsTop ? screen.y * 0.14f
              : pos == kFpsBottom ? screen.y * 0.86f - box_h
              : bottom ? screen.y - margin - box_h
                       : margin;
    float x = pos == kFpsTopLeft || pos == kFpsBottomLeft     ? margin
              : pos == kFpsTopRight || pos == kFpsBottomRight ? screen.x - margin - box_w
                                                              : screen.x / 2 - box_w / 2;
    // CUSTOM: where it was dragged, kept whole on the screen whichever style or size it has now
    if (pos == kFpsCustom) {
        x = float(game::Opt().fps_x) / kFpsPlaceScale * screen.x;
        y = float(game::Opt().fps_y) / kFpsPlaceScale * screen.y;
    }
    x = std::clamp(x, 0.0f, std::max(screen.x - box_w, 0.0f));
    y = std::clamp(y, 0.0f, std::max(screen.y - box_h, 0.0f));
    // the editor picks the box up where it is drawn
    game::g_fps_box[0] = x, game::g_fps_box[1] = y, game::g_fps_box[2] = x + box_w, game::g_fps_box[3] = y + box_h;
    game::g_fps_shown = true;
    return ImVec2(x, y);
}

// the box under the counter, a bar down its left edge in the frame rate's colour
void FpsBox(ImDrawList* dl, ImVec2 at, float w, float h, float unit, ImU32 accent) {
    float round = unit * 0.35f, bar = std::max(2.0f, unit * 0.14f);
    dl->AddRectFilled(at, ImVec2(at.x + w, at.y + h), IM_COL32(12, 14, 20, 150), round);
    dl->AddRectFilled(at, ImVec2(at.x + bar, at.y + h), accent, round, ImDrawFlags_RoundCornersLeft);
}

// The frame rate counter at the place Options::fps_position picks. SIMPLE: the rate and the frame time on one line.
// DETAILED: that line with a graph of the last frames' times (the 60 fps line across it) and the credit, then the 1%
// low, the last second's slowest frame and the CPU's, GPU's and battery's load and temperatures, each in a chip
void DrawFpsCounter() {
    const game::Options& o = game::Opt();
    game::g_fps_shown = false;
    if (!o.fps_counter || MenuOpen()) return;
    bool detailed = o.fps_style == kFpsDetailed;
    if (detailed) host::SamplePerf();
    FrameStats st = Stats();
    float fps = st.avg_ms > 0 ? 1000.0f / st.avg_ms : 0;
    ImU32 accent = FrameColor(st.avg_ms);

    float unit = ImGui::GetFontSize();
    float big = unit * 1.25f, small = unit * 0.8f;
    float big_h = TextHeight(big), small_h = TextHeight(small);
    char rate[16], ms[24];
    std::snprintf(rate, sizeof(rate), "%.0f", fps);
    std::snprintf(ms, sizeof(ms), "%.1f ms", st.avg_ms);
    const char* unit_text = " FPS  ";
    float rate_w = TextWidth(big, rate), unit_w = TextWidth(small, unit_text), ms_w = TextWidth(small, ms);
    float lead = unit * 0.55f, pad = unit * 0.35f, gap = unit * 0.5f;
    // the credit, the last line in both styles
    float credit = unit * 0.78f, credit_w = TextWidth(credit, kCredit), credit_h = TextHeight(credit);
    float credit_gap = unit * 0.2f;
    ImDrawList* dl = ImGui::GetForegroundDrawList();

    // the first line's start: the rate large, its unit and the frame time on the same foot
    auto head = [&](float x, float y) {
        float foot = y + big_h;
        DrawShadowed(dl, big, ImVec2(x, y), accent, rate);
        x += rate_w;
        DrawShadowed(dl, small, ImVec2(x, foot - small_h), kLabel, unit_text);
        DrawShadowed(dl, small, ImVec2(x + unit_w, foot - small_h), kValue, ms);
        return x + unit_w + ms_w;
    };

    if (!detailed) {
        float w = lead + std::max(rate_w + unit_w + ms_w, credit_w) + pad * 1.5f;
        float h = pad * 0.6f * 2 + big_h + credit_gap + credit_h;
        ImVec2 at = FpsCorner(w, h);
        FpsBox(dl, at, w, h, unit, accent);
        head(at.x + lead, at.y + pad * 0.6f);
        DrawShadowed(dl, credit, ImVec2(at.x + lead, at.y + pad * 0.6f + big_h + credit_gap), kCreditColor, kCredit);
        return;
    }

    char low[16], max[16];
    std::snprintf(low, sizeof(low), "%.0f", st.low_fps);
    std::snprintf(max, sizeof(max), "%.0f ms", st.max_ms);
    std::vector<Chip> chips = {{"1% LOW", low, FrameColor(st.low_fps > 0 ? 1000.0f / st.low_fps : 0)},
                               {"MAX", max, FrameColor(st.max_ms)}};
    AddPerfChip(chips, "CPU", o.fps_usage ? host::CpuLoad() : -1, o.fps_temps ? host::CpuTemp() : -1);
    AddPerfChip(chips, "GPU", o.fps_usage ? host::GpuLoad() : -1, o.fps_temps ? host::GpuTemp() : -1);
    if (o.fps_temps) AddPerfChip(chips, "BAT", -1, host::BatteryTemp());
    float chip_pad = unit * 0.25f, chip_gap = unit * 0.25f, space = TextWidth(small, " ");
    float chips_w = 0;
    for (const Chip& c : chips)
        chips_w += TextWidth(small, c.label.c_str()) + space + TextWidth(small, c.value.c_str()) + chip_pad * 2 + chip_gap;
    chips_w -= chip_gap;

    float graph_w = unit * 6.0f, graph_h = big_h * 0.75f;
    // the graph takes the first line's room up to the box's width
    float inner = std::max({rate_w + unit_w + ms_w + gap + graph_w, chips_w, credit_w});
    graph_w = inner - (rate_w + unit_w + ms_w + gap);
    float chip_h = small_h + chip_pad * 0.8f;
    float w = lead + inner + pad, h = pad + big_h + unit * 0.25f + chip_h + credit_gap + credit_h + pad;
    ImVec2 at = FpsCorner(w, h);
    FpsBox(dl, at, w, h, unit, accent);

    float x = head(at.x + lead, at.y + pad) + gap, foot = at.y + pad + big_h;
    // the graph: the last frames, one bar each, newest on the right, scaled to 33 ms or the slowest up to 66
    constexpr int kShown = 120;
    int shown = std::min(g_ft_count, kShown);
    float scale = 33.4f;
    for (int i = 0; i < shown; i++) scale = std::max(scale, std::min(FrameAgo(i), 66.8f));
    ImVec2 g0(x, foot - graph_h - unit * 0.08f), g1(x + graph_w, foot - unit * 0.08f);
    dl->AddRectFilled(ImVec2(g0.x - 2, g0.y - 2), ImVec2(g1.x + 2, g1.y + 2), IM_COL32(255, 255, 255, 14), unit * 0.15f);
    float bar = graph_w / float(kShown);
    for (int i = 0; i < shown; i++) {
        float t = FrameAgo(i);
        float hgt = std::max(std::min(t / scale, 1.0f) * graph_h, 1.0f);
        float bx = g1.x - float(i + 1) * bar;
        dl->AddRectFilled(ImVec2(bx, g1.y - hgt), ImVec2(bx + std::max(bar, 1.0f), g1.y), FrameColor(t, 220));
    }
    float l60 = g1.y - 16.7f / scale * graph_h;
    dl->AddLine(ImVec2(g0.x, l60), ImVec2(g1.x, l60), IM_COL32(255, 255, 255, 80), 1.0f);
    // the chips
    float cx = at.x + lead, cy = foot + unit * 0.25f;
    for (const Chip& c : chips) {
        float lw = TextWidth(small, c.label.c_str()), vw = TextWidth(small, c.value.c_str());
        float cw = lw + space + vw + chip_pad * 2;
        dl->AddRectFilled(ImVec2(cx, cy), ImVec2(cx + cw, cy + chip_h), IM_COL32(255, 255, 255, 18), unit * 0.2f);
        float ty = cy + (chip_h - small_h) / 2;
        DrawShadowed(dl, small, ImVec2(cx + chip_pad, ty), kLabel, c.label.c_str());
        DrawShadowed(dl, small, ImVec2(cx + chip_pad + lw + space, ty), c.color, c.value.c_str());
        cx += cw + chip_gap;
    }
    DrawShadowed(dl, credit, ImVec2(at.x + lead, cy + chip_h + credit_gap), kCreditColor, kCredit);
}

void DrawUi(double) {
    ApplyPending();
    RecordFrame();
    DrawFpsCounter();
    ImGuiIO& io = ImGui::GetIO();
    if (MenuOpen()) io.ConfigFlags &= ~ImGuiConfigFlags_NoMouseCursorChange;
    else io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    io.KeyRepeatRate = 0.1f;  // held arrows step ten times a second, ImGui's twenty is too fast for settings
    // scale the font with the output height: a phone's dense screen reads 13 px text at 540 rows to the height
    float base = io.Fonts->Fonts.empty() ? 13.0f : io.Fonts->Fonts[0]->FontSize;
    io.FontGlobalScale = std::max(1.0f, io.DisplaySize.y / 540.0f) * 13.0f / base;
    // a binding taken in the game's options menu as in the panel; its end redraws the menu's rows
    PollCapture();
    PollPadCapture();
    static bool was_capturing = false;
    if (was_capturing && !Capturing()) g_applied++;
    was_capturing = Capturing();
    if (MenuOpen()) DrawPanel();
}

}  // namespace app
