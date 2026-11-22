// Need for Speed: Underground 2 (USA, title 4541005A) boots with the XDK it calls all on the host; no patch of its own
// code is needed for that.
//
// This file is the game layer the host calls at each present (src/d3d/d3d8.cpp's Swap) and from its touch and
// settings code. Add Underground 2's own hooks here: [[hook]] entries in config/game.toml, or X86_FUNC replacements of
// recompiled functions (X86_EXTERN_FUNC(__imp__sub_XXXXXXXX) is the original), with the address checked in this xbe.
#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>

#include "app/app.h"
#include "common/fe_mouse.h"
#include "common/music.h"
#include "common/options.h"
#include "core/log.h"
#include "cpu/x86_ops.h"
#include "kernel/kernel.h"
#include "kernel/memory.h"

namespace game {

// --- at each present, game thread -------------------------------------------------------------------------------

std::atomic<bool> g_replace_music{false};
std::atomic<uint32_t> g_music_level{0}, g_music_handle{0};
std::atomic<bool> g_own_banner{false};  // src/game/music.cpp lets the banner through only while this is set

// the game's fonts are Latin-1
static std::string MusicLatin1(const std::string& text) {
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

// text for the game, kept for good: one copy of each string
static uint32_t MusicText(const std::string& text) {
    static std::unordered_map<std::string, uint32_t> copies;
    uint32_t& va = copies[text];
    if (!va) {
        std::string latin = MusicLatin1(text);
        va = mem::AllocSystem(uint32_t(latin.size() + 1));
        if (va) std::memcpy(kernel::HostPtr(va), latin.c_str(), latin.size() + 1);
    }
    return va;
}

// A song's entry in EA Trax's table (0x3924C0, 16 bytes each: title, artist, album, genre), for the banner, 0xC6830 (its callers
// 0x88AAF, 0x88B95, 0x88C8F pass &table[song] as a new EA Trax song starts). One entry for each song, kept
static uint32_t MusicEntry(const music::Song& song) {
    static std::unordered_map<std::string, uint32_t> entries;
    uint32_t& va = entries[song.title + '\n' + song.artist + '\n' + song.album];
    if (!va) {
        va = mem::AllocSystem(16);
        if (va) {
            kernel::W32(va, MusicText(song.title));
            kernel::W32(va + 4, MusicText(song.artist));
            kernel::W32(va + 8, MusicText(song.album));
            kernel::W32(va + 12, MusicText(""));
        }
    }
    return va;
}

void MusicFrame(X86Context& ctx) {
    uint32_t flow = kernel::R32(0x428E34);
    // Audio Options sliders: AOFEMusicVol then AOIGMusicVol (0xCE680); their callbacks
    // 0xB4940 / 0xB49F0 write these floats and pass the settings to 0x76A70.
    float level = kernel::G<float>(flow == 6 ? 0x406CF4 : 0x406CF0);
    if (!std::isfinite(level)) level = 0;
    // My Music starts at the main menu, not over the boot's logos, movies and memory card screens, and in a race once
    // its HUD is up, not over the loading screen and the opening camera
    static bool main_menu = false;
    main_menu |= FeOnPackage("UI_Main.fng");
    bool playing = main_menu && (flow == 3 || (flow == 6 && FeHudShown())) && !g_focus_paused;
    music::SetContext(playing, flow == 6, std::clamp(level, 0.0f, 1.0f));
    bool replace = music::HasSongs() && music::Source() == music::kMyMusic;
    uint32_t bus = kernel::R32(0x3E5AFC);
    bool changed = g_replace_music.exchange(replace) != replace;
    if (changed && g_music_handle.load() && bus && mem::IsCommitted(bus + 0x18) &&
        kernel::R32(bus + 0x18) == g_music_handle.load()) {
        // apply a source change immediately, even when the game's volume has not changed
        X86Context saved = ctx;
        kernel::CallGuest(ctx, 0x273C65, {g_music_handle.load(), 0xFFFFFFFFu, g_music_level.load()});
        ctx = saved;
    }
    // a My Music song began: its name on EA Trax's banner, as the game shows its own songs' (the game's own is held, music.cpp)
    music::Song song;
    if (replace && music::TakeStarted(&song)) {
        uint32_t entry = MusicEntry(song);
        if (entry) {
            X86Context saved = ctx;
            g_own_banner = true;
            kernel::CallGuest(ctx, 0xC6830, {entry});
            g_own_banner = false;
            ctx = saved;
        }
    }
}

// GameFlowManager's state (0x428E34, the object at 0x428E10): GAMEFLOW_STATE_* in the order of their names at
// 0x359BD4, 3 the front end, 6 racing (set by BeginGameFlowRacing, 0x182206), loading between; the frame's own
// present (0x0AEDA1) reads it too. The touch screen's race controls show while it is 6
constexpr uint32_t kGameFlowState = 0x428E34, kGameFlowRacing = 6;
bool Racing() { return kernel::R32(kGameFlowState) == kGameFlowRacing; }

// only English and Indonesian are offered; Indonesian translates the host's own panels and, from language.cpp, the game's text
bool LanguageAvailable(const std::string& code) { return code == "int" || code == "ind"; }

}  // namespace game

namespace app {

// the console's XC_LANGUAGE the game reads at boot: English, for Indonesian too, which the disc lacks
uint32_t ConsoleLanguage() { return 1; }

}  // namespace app
