#pragma once
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <iterator>
#include <map>
#include <string>

struct X86Context;

// the game's own settings, in the settings file through app::kGame.options
namespace game {

// the languages, saved by these codes: English, French, German, Italian, Spanish, Indonesian, Dutch, Swedish, Danish,
// Portuguese. The disc has the game's text in all of them but Indonesian and Portuguese, which src/game/language.cpp
// translates from the English
inline constexpr const char* kLanguages[] = {"int", "fra", "deu", "ita", "esp", "ind", "nld", "swe", "dan", "por"};
inline bool KnownLanguage(const std::string& code) {
    return std::find(std::begin(kLanguages), std::end(kLanguages), code) != std::end(kLanguages);
}

// SSAA renders at twice the height, the presenter averages it down
inline constexpr uint32_t kAaOff = 0, kAaFxaa = 1, kAaSsaa = 2;

struct Options {
    bool fps_counter = false;
    uint32_t fps_position = 0;  // the counter's place, src/common/menu.cpp's kFpsPositionNames
    bool fps_usage = true;      // the counter's CPU and GPU load
    bool fps_temps = true;      // the counter's CPU, GPU and battery temperatures
    uint32_t screen_filter = 0;  // the present's look: 0 none, 1 sharpen, 2 vivid, 3 scanlines, 4 CRT
    uint32_t anti_aliasing = 0;  // kAaOff, kAaFxaa or kAaSsaa
    uint32_t aspect_ratio = 0;  // the frame's shape in hundredths (133 is 4:3), 0 the whole screen; common/aspect.h
    bool bloom = true;  // the glow added over the frame, native/vulkan.cpp skips it when off
    bool fog = true;    // the distance fog, native/vulkan.cpp draws without it when off
    bool rain = true;       // the falling rain and its drops on the screen, src/game/effects.cpp skips them when off
    bool particles = true;  // the particle effects (smoke, sparks, spray), src/game/effects.cpp spawns none when off
    bool shader_preload = false;  // native/vulkan.cpp makes earlier runs' shaders and pipelines at the start
    uint32_t audio_output = 0;  // audio::kOutputAuto, kOutputStereo, kOutputSurround or kOutputMono
    std::string language = "int";  // one of kLanguages
    std::map<std::string, std::string> keys;  // action id to key names, absent is the default
    std::map<std::string, std::string> pad;   // action id to a controller's input, absent is SDL's mapping
};
Options& Opt();
// the language setting as this run started, the language the game boots in
const std::string& BootLanguage();
bool LanguageAvailable(const std::string& code);  // src/game/game.cpp: the LANGUAGE row offers it
inline std::atomic<bool> g_focus_paused{false};  // the game thread waits while the app is in the background
inline std::atomic<uint32_t> g_focus_pauses{0};  // focus pauses ended so far
void WaitWhileUnfocused();  // native/game_host.cpp: game thread, before the present
bool Racing();    // src/game/game.cpp, any thread: the game flow is in a race (free roam too), not loading or the front end
void MusicFrame(X86Context& ctx);  // src/game/game.cpp: game thread, at each present
// src/game/android_options.cpp: game thread, at each present: our options rows redrawn once the UI applied a change
void OptionsFrame(X86Context& ctx);

}  // namespace game
