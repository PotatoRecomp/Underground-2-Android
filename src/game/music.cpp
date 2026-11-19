// Underground 2's EA Trax bus volume, before it is mixed with engines and effects.
#include <atomic>
#include "cpu/x86_ops.h"
#include "kernel/kernel.h"
#include "kernel/memory.h"

namespace game {
extern std::atomic<bool> g_replace_music, g_own_banner;
extern std::atomic<uint32_t> g_music_level, g_music_handle;
}

X86_EXTERN_FUNC(__imp__sub_00273C65);
X86_FUNC(sub_00273C65) {
    // 0x8FEAB and 0x8FF03 set the EA Trax bus's channel volume: [0x3E5AFC]+0x18.
    // Keep its requested gain for switching back; never change the saved Audio Options sliders.
    uint32_t bus = kernel::R32(0x3E5AFC);
    uint32_t handle = kernel::R32(ctx.esp + 4);
    uint32_t level_at = ctx.esp + 12;
    uint32_t level = kernel::R32(level_at);
    bool music = bus && mem::IsCommitted(bus + 0x18) && handle == kernel::R32(bus + 0x18);
    if (music) {
        game::g_music_handle = handle;
        game::g_music_level = level;
        if (game::g_replace_music) kernel::W32(level_at, 0);
    }
    __imp__sub_00273C65(ctx, base);
    if (music) kernel::W32(level_at, level);
}

// The banner with a song's title, artist and album, 0xC6830(&entry) (cdecl). The game calls it for its own EA Trax songs, which
// play muted while My Music replaces them: those names are held back, and MusicFrame shows My Music's own song through it.
X86_EXTERN_FUNC(__imp__sub_000C6830);
X86_FUNC(sub_000C6830) {
    if (game::g_replace_music && !game::g_own_banner) {
        ctx.esp += 4;  // ret
        return;
    }
    __imp__sub_000C6830(ctx, base);
}
