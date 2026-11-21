// The game's main heap. 0x4F600 makes EA's pool 0 from one contiguous block of [0x38EE48] bytes (0x3700000, 55 MB of
// the console's 64), read nowhere else. Each track or free roam load has the streamer (0x4419E0, from 0x184730 through
// 0x1D5360) take one block of 17 to 18.5 MB from it ([0x357854] KB); after a few loads the heap was too cut up for
// it, 0x4EE60 found no room and 0x4F640 stopped on its int3 at 0x4F7BA. Our physical memory is 128 MB (the SDK's
// kPhysicalSize): the heap gets 32 MB more, the rest left for D3D, the sound and the kernel
#include "cpu/x86_ops.h"
#include "kernel/kernel.h"

X86_EXTERN_FUNC(__imp__sub_0004F600);
X86_FUNC(sub_0004F600) {
    constexpr uint32_t kHeapSize = 0x38EE48;
    kernel::W32(kHeapSize, 0x3700000 + 0x2000000);
    __imp__sub_0004F600(ctx, base);
}
