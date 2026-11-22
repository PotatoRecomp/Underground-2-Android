// INFINITE NITROUS (game::Options::cheat_nitrous). A car's nitrous is timed, kept by the component at its +0x40 in ticks
// of 1/4000 s: +0x414 the tank, +0x41C what is left of it, +0x420 the tick the burst began (0 or 0x7FFFFFFF when off).
// The fill 0x19CD60 sets the tank and what is left from the car's attribute (+0x2A0, seconds); the burst's end
// 0x19D4D0 takes the ticks it ran off what is left; the driving rewards (0x19D6xx to 0x19D8xx) add to it. What is left
// in seconds, the running burst taken off, is 0x19D450's (thiscall, x87 result): the physics (0x1AA4EB, ...) asks it to
// know when the tank is dry, the HUD's gauge (0xDF890 through 0x19D5B0) to draw it.
// Asked about a player's car with the cheat on, what is left becomes the tank plus the burst's ticks so far (taken from
// the game's own answer, so the clock it reads needs no choosing here): the tank is always full, and the burst's end
// leaves it full too. Other cars keep theirs
#include <cmath>

#include "common/options.h"
#include "cpu/x86_ops.h"
#include "kernel/kernel.h"

namespace {

constexpr uint32_t kPlayers = 0x44940C;  // the two local players; a player's car at +4, its nitrous at the car's +0x40
constexpr uint32_t kNitrous = 0x40, kTank = 0x414, kLeft = 0x41C;
constexpr double kTicks = 4000.0;  // 0x3408C4 is its inverse

bool PlayerNitrous(uint32_t nitrous) {
    for (uint32_t i = 0; i < 2; ++i) {
        uint32_t player = kernel::R32(kPlayers + i * 4);
        if (player && kernel::R32(player + 4) + kNitrous == nitrous) return true;
    }
    return false;
}

bool Valid(int32_t ticks) { return ticks > 0 && ticks != 0x7FFFFFFF; }

}  // namespace

X86_EXTERN_FUNC(__imp__sub_0019D450);
X86_FUNC(sub_0019D450) {
    uint32_t self = ctx.ecx;
    __imp__sub_0019D450(ctx, base);
    if (!game::Opt().cheat_nitrous || !self || !PlayerNitrous(self)) return;
    int32_t tank = static_cast<int32_t>(kernel::R32(self + kTank));
    int32_t left = static_cast<int32_t>(kernel::R32(self + kLeft));
    if (!Valid(tank) || left == 0x7FFFFFFF) return;
    // the burst's ticks so far: what was left less what the game says is left
    double& result = ctx.st[ctx.top];
    double ran = std::round(left - result * kTicks);
    if (!(ran > 0) || ran > 0x3FFFFFFF) ran = 0;  // no burst running (or nothing sensible to take)
    kernel::W32(self + kLeft, static_cast<uint32_t>(tank + static_cast<int32_t>(ran)));
    result = x87_prec(ctx, tank / kTicks);
}
