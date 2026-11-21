// RAIN and PARTICLES (game::Options), switched off in the game's own code:
// - the rain's draw, 0x202B80 (thiscall on the rain object, one argument, ret 4): run each frame by 0x20A570 under the
//   frame's render (0xADF60), it queues the falling streaks with the RAINDROP texture (+0x4D7C, set up by 0x2021C0)
// - the drops on the screen, 0xA6530 (cdecl, the view): the pass the render list (0xAC6F0) runs last for each view,
//   named "Rain Drop" for the profiler (0x2ED000)
// - the particle system's spawn, 0xAA2A0 (thiscall on the system at 0x3F6740, three arguments, ret 0xC): an emitter's
//   new particles from the Acid pools (0x3EA4F4, groups 0x3EA4F8). Its callers (the emitters' update 0xAAFF0, the
//   cars' 0x1CC7C0, the world's 0x42210 and 0x204B2B) take nothing back. Off, no particle is made; those alive run out
#include "common/options.h"
#include "cpu/x86_ops.h"
#include "kernel/kernel.h"

X86_EXTERN_FUNC(__imp__sub_00202B80);
X86_FUNC(sub_00202B80) {
    if (game::Opt().rain) {
        __imp__sub_00202B80(ctx, base);
        return;
    }
    kernel::Return(ctx, 0, 1);
}

X86_EXTERN_FUNC(__imp__sub_000A6530);
X86_FUNC(sub_000A6530) {
    if (game::Opt().rain) {
        __imp__sub_000A6530(ctx, base);
        return;
    }
    kernel::Return(ctx, 0, 0);
}

X86_EXTERN_FUNC(__imp__sub_000AA2A0);
X86_FUNC(sub_000AA2A0) {
    if (game::Opt().particles) {
        __imp__sub_000AA2A0(ctx, base);
        return;
    }
    kernel::Return(ctx, 0, 3);
}
