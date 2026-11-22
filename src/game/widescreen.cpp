// The picture at the frame's own shape. The frame is shown at the aspect ratio setting's shape (native/main.cpp), but
// the game draws for a 4:3 one, or 16:9 with the console's widescreen flag (bit 0 of the video flags it keeps at
// 0x3EA5DC, set from 3:2 on): a 3:2 tablet or a 20:9 phone saw the road and the menus stretched across.
//
// The 3D views' projection (0x0A33F0, thiscall, the view's matrices in ecx, the eView first of 4 arguments, as Most
// Wanted's 0x0C7740): the camera's horizontal angle ([[view + 0x40] + 0xC4]) gives the x scale, cot of its half, at
// +0x40; the vertical is that angle times 3/4 (9/16 with the flag), its scale at +0x54. The x scale is made the
// frame's, keeping the vertical (more of the road on the sides), before the matrix product (0x2213EA at 0x0A35C7)
// gives the view projection the frustum's planes are cut from, so what is culled widens with it. Only the views drawn
// over the frame, those the game gives its wider angle too: the players' (1, 2), the road's reflections (4, 5) and
// glows (6, 7).
//
// The front end's and the HUD's screen transform (0x0AE590): 576 over the frame's width across (the width 640, or
// 853, 480 * 16 / 9 at 0x3996AC, with the flag), 448 over 480 down. It takes the flag's branch with the frame's own
// width, 480 times its aspect, so the 640x480 layout keeps its shape in the middle of a wider frame
#include <cstring>

#include "common/aspect.h"
#include "cpu/x86_ops.h"
#include "kernel/kernel.h"

namespace game {
namespace {

constexpr uint32_t kVideoFlags = 0x3EA5DC, kWideWidth = 0x3996AC;
constexpr uint32_t kViewType = 0x4, kProjection = 0x40;
uint32_t g_widen_view = 0;  // the view's matrices while its projection is made, game thread

bool Widescreen() { return kernel::R32(kVideoFlags) & 1; }
bool WideView(uint32_t type) { return type == 1 || type == 2 || (type >= 4 && type <= 7); }

}  // namespace
}  // namespace game

X86_EXTERN_FUNC(__imp__sub_000A33F0);
X86_FUNC(sub_000A33F0) {
    uint32_t view = kernel::R32(ctx.esp + 4);
    game::g_widen_view = view && game::WideView(kernel::R32(view + game::kViewType)) ? ctx.ecx : 0;
    __imp__sub_000A33F0(ctx, base);
    game::g_widen_view = 0;
}

// the matrix product (out, a, b; cdecl): for the view being made, its x scale goes from the game's shape to the frame's
X86_EXTERN_FUNC(__imp__sub_002213EA);
X86_FUNC(sub_002213EA) {
    uint32_t m = game::g_widen_view;
    if (m && kernel::R32(ctx.esp + 8) == m && kernel::R32(ctx.esp + 12) == m + game::kProjection) {
        game::g_widen_view = 0;
        float drawn = game::Widescreen() ? 16.0f / 9.0f : 4.0f / 3.0f;
        kernel::G<float>(m + game::kProjection) *= drawn / game::Aspect();
    }
    __imp__sub_002213EA(ctx, base);
}

X86_EXTERN_FUNC(__imp__sub_000AE590);
X86_FUNC(sub_000AE590) {
    uint32_t flags = kernel::R32(game::kVideoFlags);
    float wide = 480.0f * game::Aspect();
    uint32_t bits;
    std::memcpy(&bits, &wide, 4);
    kernel::W32(game::kWideWidth, bits);
    kernel::W32(game::kVideoFlags, flags | 1);
    __imp__sub_000AE590(ctx, base);
    kernel::W32(game::kVideoFlags, flags);
}
