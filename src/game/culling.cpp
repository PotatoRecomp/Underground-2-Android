// The view's box test, the busiest function of the game's own frame (about 6% of the title thread in a race, profiled
// on an Adreno 732 phone): 0x9A330, thiscall (view, const float min[3], const float max[3], const float* matrix), ret
// 0xC. A box, through the matrix when there is one (0x9A250: each corner's axis through the rows, the smaller product
// to the new min and the larger to the max, the row 3 the start), as its centre ([0x3408BC] times min + max) and half
// extent (max - centre) against the view's six planes (*view + 0x140, 16 bytes each: normal x y z, distance):
// 0 wholly outside one, 2 inside all, 1 across one. Recompiled it runs on the x87 stack emulation; here it is plain
// floats in the same order of operations, which the callers only branch on
#include <cmath>

#include "cpu/x86_ops.h"
#include "kernel/kernel.h"

namespace {

constexpr uint32_t kHalf = 0x3408BC;   // 0.5 as the game keeps it
constexpr uint32_t kZero = 0x33FA8C;   // the planes' threshold
constexpr uint32_t kPlanes = 0x140;    // in the view the first argument points at
constexpr int kPlaneCount = 6;

float F(uint32_t va) { return kernel::G<float>(va); }

}  // namespace

X86_FUNC(sub_0009A330) {
    kernel::Args a{ctx};
    uint32_t pmin = a[0], pmax = a[1], matrix = a[2];
    float lo[3] = {F(pmin), F(pmin + 4), F(pmin + 8)};
    float hi[3] = {F(pmax), F(pmax + 4), F(pmax + 8)};
    if (matrix) {
        // 0x9A250
        double nlo[3], nhi[3];
        for (int i = 0; i < 3; i++) nlo[i] = nhi[i] = F(matrix + 0x30 + 4 * uint32_t(i));
        for (int k = 0; k < 3; k++)
            for (int i = 0; i < 3; i++) {
                float m = F(matrix + 0x10 * uint32_t(k) + 4 * uint32_t(i));
                double e = double(lo[k]) * m;
                float f = float(double(hi[k]) * m);
                if (e < f) nlo[i] += e, nhi[i] += f;
                else nlo[i] += f, nhi[i] += e;
            }
        for (int i = 0; i < 3; i++) lo[i] = float(nlo[i]), hi[i] = float(nhi[i]);
    }
    float half = F(kHalf), zero = F(kZero);
    float c[3], h[3];
    for (int i = 0; i < 3; i++) c[i] = float((double(hi[i]) + lo[i]) * half);
    for (int i = 0; i < 3; i++) h[i] = hi[i] - c[i];
    uint32_t p = kernel::R32(ctx.ecx) + kPlanes;
    bool across = false;
    for (int n = 0; n < kPlaneCount; n++, p += 16) {
        double nx = F(p), ny = F(p + 4), nz = F(p + 8), d = F(p + 12);
        double r = std::fabs(ny) * h[1] + std::fabs(nz) * h[2] + std::fabs(nx) * h[0];
        double s = c[1] * ny + c[2] * nz + c[0] * nx + d;
        if (s + r < zero) {
            kernel::Return(ctx, 0, 3);
            return;
        }
        if (s - r < zero) across = true;
    }
    kernel::Return(ctx, across ? 1 : 2, 3);
}
