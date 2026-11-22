#include <atomic>
#include <cstdlib>

#include <toml++/toml.hpp>

#include "app/app.h"
#include "audio/output.h"
#include "common/menu.h"
#include "common/mouse_keyboard.h"
#include "common/options.h"
#include "common/pad_binds.h"

namespace game {

namespace {

// port 0 has the touch controls, the keyboard and mouse; the game sees no input while the panel is open, nor while a
// binding waits for its input and until what bound it is let go (B bound would have backed out of the menu too)
void Pad(int port, input::Pad& pad) {
    input::SeenPad(pad);
    if (port == 0) pad.connected = true;
    static std::atomic<bool> held{false};
    if (app::Capturing()) held = true;
    const bool panel = app::MenuOpen();
    if (!panel && port == 0) input::Keyboard(pad);
    if (held && port == 0 && !app::Capturing()) {
        bool idle = !pad.buttons && std::abs(pad.lx) < 8000 && std::abs(pad.ly) < 8000 && std::abs(pad.rx) < 8000 &&
                    std::abs(pad.ry) < 8000;
        for (uint8_t a : pad.analog) idle &= a < 30;
        if (idle) held = false;
    }
    if (panel || held) pad = input::Pad{.connected = pad.connected};
}

// no vibration while the keyboard and mouse are in use
void Rumble(int port, uint16_t& left, uint16_t& right) {
    if (port != 0 || !input::KeyboardActive()) return;
    left = 0;
    right = 0;
}

// what the options list cannot hold
void Load(const void* table) {
    const toml::table& t = *static_cast<const toml::table*>(table);
    Options& o = Opt();
    std::string language = t["language"].value_or(o.language);
    // a language this build does not offer goes back to English
    if (KnownLanguage(language) && LanguageAvailable(language)) o.language = language;
    if (const toml::table* keys = t["keys"].as_table())
        for (auto&& [id, value] : *keys)
            if (auto v = value.value<std::string>()) o.keys[std::string(id.str())] = *v;
    if (const toml::table* pad = t["pad"].as_table())
        for (auto&& [id, value] : *pad)
            if (auto v = value.value<std::string>()) o.pad[std::string(id.str())] = *v;
}

// after the settings load, before boot
void Init() {
    BootLanguage();  // latched before the options can change the setting
    input::LoadBindings();
    input::LoadPadBindings();
    if (Opt().screen_filter > 4) Opt().screen_filter = 0;  // HDR (5) was taken out: a saved one is off
    audio::SetOutputMode(Opt().audio_output);
}

void Save(void* table) {
    toml::table& t = *static_cast<toml::table*>(table);
    const Options& o = Opt();
    t.insert("language", o.language);
    toml::table keys;
    for (const auto& [id, value] : o.keys) keys.insert(id, value);
    t.insert("keys", keys);
    toml::table pad;
    for (const auto& [id, value] : o.pad) pad.insert(id, value);
    t.insert("pad", pad);
}

}  // namespace

Options& Opt() {
    static Options o;
    return o;
}

const std::string& BootLanguage() {
    static const std::string language = Opt().language;
    return language;
}

}  // namespace game

const app::GameInfo app::kGame = {
    .title = L"Need for Speed: Underground 2",
    .short_title = L"Underground 2",
    .name = L"Underground2",
    .log = "underground2",
    .settings = "underground2.toml",
    .splash = nullptr,
    .missing = L"Put the game in the Underground2 folder on the phone's storage: its disc image (.iso), or default.xbe "
               L"with the disc's NFSUNDER folder beside it.",
    .images = {{"default.xbe", "Underground2"}},
    .options = {{"fps_counter", &game::Opt().fps_counter, nullptr},
                {"hdr", &game::Opt().hdr, nullptr},
                {"bloom", &game::Opt().bloom, nullptr},
                {"fog", &game::Opt().fog, nullptr},
                {"rain", &game::Opt().rain, nullptr},
                {"particles", &game::Opt().particles, nullptr},
                {"shader_preload", &game::Opt().shader_preload, nullptr},
                {"vibration", &game::Opt().vibration, nullptr},
                {"cheat_cash", &game::Opt().cheat_cash, nullptr},
                {"cheat_nitrous", &game::Opt().cheat_nitrous, nullptr},
                {"cheat_unlock_all", &game::Opt().cheat_unlock_all, nullptr},
                {"fps_usage", &game::Opt().fps_usage, nullptr},
                {"fps_temps", &game::Opt().fps_temps, nullptr},
                {"fps_position", nullptr, &game::Opt().fps_position},
                {"fps_style", nullptr, &game::Opt().fps_style},
                {"fps_x", nullptr, &game::Opt().fps_x},
                {"fps_y", nullptr, &game::Opt().fps_y},
                {"audio_output", nullptr, &game::Opt().audio_output},
                {"aspect_ratio", nullptr, &game::Opt().aspect_ratio},
                {"screen_filter", nullptr, &game::Opt().screen_filter},
                {"anti_aliasing", nullptr, &game::Opt().anti_aliasing}},
    .load = game::Load,
    .save = game::Save,
    .init = game::Init,
    .loaded = nullptr,
    .window = nullptr,
    .message = nullptr,
    .ui = app::DrawUi,
    .pad = game::Pad,
    .rumble = game::Rumble,
};
