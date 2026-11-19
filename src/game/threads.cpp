// The console had one core: the game's threads took turns, and state they share with no lock was only ever caught
// half written when the scheduler cut in between two instructions. On a phone the threads run at once, so the
// places they meet are locked here, the rest left as the game wrote it.
#include <atomic>
#include <mutex>
#include <thread>

#include "cpu/x86_ops.h"
#include "kernel/kernel.h"

namespace {

// The bank module at 0x282000..0x286300: a call selects a bank (0x284290: the bank's record, 0x46B038 + n * 0x928, in
// [0x46D4D8], its mask in [0x46D4E8]), works on [0x46D4D8] and clears both on the way out (0x28284C). Each entry point
// is a try lock of the game's own: the module's ready byte [0x46D4E4] set and nothing selected ([0x46D4E8] 0), else it
// returns at once. Its test and its claim are apart, a window the console's one core never hit: two threads in it at
// once and one cleared the other's record mid-call, SIGSEGV at 0x282886 ([0 + 0x38]) under the main loop
// (0x18A8C0 > 0x91760 > 0x8FF50 > 0x282730).
// A plain lock is wrong here: the update of every bank (0x2844E0) claims the module, then stops the stream thread
// (0x79A00, waiting for it), and the stream thread's own call into the module (0x2845A0) must find it claimed and
// return, as on the console; waiting for the lock instead hung both on the EA logo. So a thread that finds the lock
// held waits only until the holder has made its claim, then runs the call unlocked: the game's own test sees the
// claim and returns.
std::recursive_mutex g_banks;
constexpr uint32_t kSelected = 0x46D4E8;

template <void (*Original)(X86Context&, uint8_t*)>
void Banked(X86Context& ctx, uint8_t* base) {
    for (;;) {
        if (g_banks.try_lock()) {
            Original(ctx, base);
            g_banks.unlock();
            return;
        }
        if (kernel::R32(kSelected) != 0) {
            Original(ctx, base);
            return;
        }
        std::this_thread::yield();
    }
}

}  // namespace

#define BANKED(addr)                                                                    \
    X86_EXTERN_FUNC(__imp__sub_##addr);                                                 \
    X86_FUNC(sub_##addr) { Banked<__imp__sub_##addr>(ctx, base); }

BANKED(00282630)
BANKED(00282730)
BANKED(00282960)
BANKED(00282E70)
BANKED(002835A0)
BANKED(00283750)
BANKED(002839D0)
BANKED(00283D40)
BANKED(00283FA0)
BANKED(002844E0)

#undef BANKED

// EA's slot pools (bSlotPool: free list +0x10, flags +0x14, count +0x18, peak +0x1C, Most Wanted's layout): no lock of
// their own. Two threads popping one pool's list at once got one slot both, and a string written into it took the
// list's next pointer: SIGSEGV at 0x4FFC0 reading "CARS" (0x53524143) as the next slot, under the front end's car
// loading (0x188D00 > 0x186DF0 > 0x186A00 > 0x253B0E > 0x25386A > 0x1839D0). Alloc (0x4FFC0 thiscall, growing by
// 0x4FE60; 0x4EC70 cdecl), free (0x4C0C0 thiscall, 0x4ECA0 cdecl), make (0x4FDF0, 0x50070: linked into the pools'
// list at 0x3E4398) and teardown (0x50020, its free list walk 0x4FEE0) take one lock. A pool that grows takes the
// heap's own section inside it, and the heap never calls into a pool, so the two never wait on each other
namespace {
std::recursive_mutex g_pools;
}

#define LOCKED(lock, addr)                                \
    X86_EXTERN_FUNC(__imp__sub_##addr);                   \
    X86_FUNC(sub_##addr) {                                \
        std::lock_guard<std::recursive_mutex> hold(lock); \
        __imp__sub_##addr(ctx, base);                     \
    }

LOCKED(g_pools, 0004FFC0)
LOCKED(g_pools, 0004EC70)
LOCKED(g_pools, 0004C0C0)
LOCKED(g_pools, 0004ECA0)
LOCKED(g_pools, 0004FDF0)
LOCKED(g_pools, 00050070)
LOCKED(g_pools, 00050020)
LOCKED(g_pools, 0004FEE0)

// File reads: a read through a file's raw device (vtable at 0x366BF0: read +0x14, 0x254BD4, ReadFile with no
// OVERLAPPED; seek +0x1C, 0x254C39, SetFilePointer) is two calls, seek then read from where the handle is, and the
// archive's handle is one for every thread. Another thread's seek in between and the read took another file's bytes,
// as in Most Wanted, whose loader then walked garbage. Each seek and its transfer is held under one lock: the
// archive reader (0x185900) and the file system jobs' read and write (0x254337, 0x254229); the disc reads one at a
// time anyway
namespace {
std::recursive_mutex g_io;
}

LOCKED(g_io, 00185900)
LOCKED(g_io, 00254229)
LOCKED(g_io, 00254337)

#undef LOCKED
