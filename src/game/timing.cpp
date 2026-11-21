// The frame's time step. 0x1890C0 turns the time the frame really took (the 0x4ADA0 clock) into the dt the whole game runs on
// (0x428A0C, in 1/3600 s), and never lets it fall under a minimum: [0x428EE8], 1/60 s from 0x180AA0/0x180AC0 (NTSC), or the 1/50 s
// constant at 0x341750 while the mode at 0x3A4C50 is 0. The console was held to its 60 Hz vertical blank, so no frame took less;
// here a frame can, and every one above 60 fps still counted for a full 1/60 s, the game running fps/60 times too fast (the
// remainder 0x4289BC does not give the shortfall back). With no minimum the dt is the real time, and the 60 Hz sim ticks the
// game takes from it (0x184200) still step at 60 Hz, only on the frames they are due.
// It also never lets the dt over a maximum, [0x3A4C64] frames of 1/60 s (3, read nowhere else): the console never
// fell under 20 fps, but a phone does, and every slower frame lost the rest of its time, the game in slow motion.
// The maximum is raised to 1/10 s; a longer frame (a hitch, a load) is still cut there rather than jump the cars.
#include "cpu/x86_ops.h"
#include "core/log.h"
#include "kernel/kernel.h"

X86_EXTERN_FUNC(__imp__sub_001890C0);
X86_FUNC(sub_001890C0) {
    constexpr uint32_t kMinDt = 0x428EE8, kMode = 0x3A4C50, kMaxFrames = 0x3A4C64;
    kernel::W32(kMinDt, 0);  // 0.0f
    kernel::G<float>(kMaxFrames) = 6.0f;  // 1/10 s
    // mode 0 takes its minimum from a constant instead: run it as the other, which differs only in the longest dt allowed
    uint32_t mode = kernel::R32(kMode);
    if (!mode) kernel::W32(kMode, 2);
    __imp__sub_001890C0(ctx, base);
    if (!mode) kernel::W32(kMode, 0);
    static uint32_t n = 0;
    if (logx::kVerbose && ++n % 120 == 0) LOGR("timing: dt %.5f", kernel::G<float>(0x428A0C));
}
