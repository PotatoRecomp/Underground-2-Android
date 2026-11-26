// AUTO SHIFT IN DRAG (game::Options::cheat_drag_shift): the player's shift up pressed for them at each gear's best rpm.
// The first local player (0x44940C) keeps its car at +4. The controls that drive a car (classes 0x341578 and 0x341600:
// 0x350B0 builds one with the car, also at a human driver's +0x470) take the pad's events: the car +0x2A8, the gear
// asked for +0x1E4, the transmission mode +0x2C0 (0x16BE0: 1 and 3 manual, 3 with the clutch, 0 and 2 automatic). The
// game reaches them from the car's +0x30, directly or at +0x470 (seen on a phone: the car's own at the start line, the
// driver's once racing), so they are looked for there and in the player, checked by their class and car.
// JOY_EVENT_SHIFTUP (0x9C) is 0x179E0 (cdecl: two event words, the press, the controls), which asks for the next gear
// by the mode. The car: the gear +0x434 (1 neutral, 2 first), the rpm +0x440 (float), its tuning +0x20: the gear count
// +0x1C8, idle +0x1F0, the shift light +0x1F4 (0xDEE20 at 0xDF051) and the top of the rpm +0x1F8 (the drag
// tachometer's full scale, 0xC5010). A human's car (+0x28 1) has its drag info at [[car+0x34]+0x4C]: each gear's best
// rpm at +0x84 + gear * 4 (0x1DCF50 > 0x19F850), which the shift is graded against (0x19FFF0: below it 1 - (best -
// rpm) / (K * window), 1 at the best rpm; past it by more than (1 - K) * window, over revved; 0.5 and up the tachometer
// shows green, 0xDF630). The press is made kEarlyRpm before it, about a frame's climb; without one (another kind of
// car), at kShiftAt of the top. A drag race is the byte
// 0x3CC498 or the dword 0x449468 set, as the driver's own start (0x16C70) and throttle (0x1771E) tell it.
// Once in a forward gear below the top the press is made once; again if the gear has not changed a while later (a
// shift the game refused). A shift takes about half a second (the car's +0x34 > +0x4C > +0x48 set), the gear changing
// at its end
#include <chrono>
#include <cstring>

#include "common/options.h"
#include "core/log.h"
#include "cpu/x86_ops.h"
#include "kernel/kernel.h"
#include "kernel/memory.h"

namespace {

constexpr uint32_t kPlayers = 0x44940C, kPlayerCar = 4;
constexpr uint32_t kDriverControls = 0x470, kControlsCar = 0x2A8, kControlsMode = 0x2C0;
constexpr uint32_t kControlsVtables[] = {0x341578, 0x341600};
constexpr uint32_t kCarControls = 0x30, kCarGear = 0x434, kCarRpm = 0x440, kCarTuning = 0x20;
constexpr uint32_t kCarKind = 0x28, kCarHuman = 1, kCarDrag = 0x34, kDragInfo = 0x4C, kInfoBestRpm = 0x84;
constexpr uint32_t kTuningGears = 0x1C8, kTuningShiftLight = 0x1F4, kTuningTopRpm = 0x1F8;
constexpr uint32_t kDragByte = 0x3CC498, kDragWord = 0x449468;
constexpr uint32_t kShiftUp = 0x179E0, kEventShiftUp = 0x9C;
constexpr uint32_t kFirstGear = 2;
constexpr float kShiftAt = 0.985f, kEarlyRpm = 60.0f;
constexpr uint64_t kRetryMs = 800, kRescanMs = 1000;
constexpr uint32_t kScanBytes = 0xA00;

uint64_t SteadyMs() {
    using namespace std::chrono;
    return uint64_t(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

float F32(uint32_t va) {
    uint32_t v = kernel::R32(va);
    float f;
    std::memcpy(&f, &v, sizeof(f));
    return f;
}

bool Readable(uint32_t va, uint32_t size) { return va && mem::IsCommitted(va) && mem::IsCommitted(va + size - 1); }

bool IsControls(uint32_t va, uint32_t car) {
    if (!Readable(va, kControlsMode + 4) || kernel::R32(va + kControlsCar) != car) return false;
    uint32_t vt = kernel::R32(va);
    for (uint32_t v : kControlsVtables)
        if (vt == v) return true;
    return false;
}

// the car's controls: the car's +0x30 first, else any pointer in the car or the player, or a driver's +0x470 there
uint32_t FindControls(uint32_t player, uint32_t car) {
    uint32_t direct = kernel::R32(car + kCarControls);
    for (uint32_t at : {direct + kDriverControls, direct})
        if (IsControls(at, car)) return at;
    for (uint32_t base : {car, player}) {
        if (!Readable(base, kScanBytes)) continue;
        for (uint32_t off = 0; off < kScanBytes; off += 4) {
            uint32_t v = kernel::R32(base + off);
            for (uint32_t at : {v + kDriverControls, v})
                if (IsControls(at, car)) return at;
        }
    }
    return 0;
}

}  // namespace

namespace game {

void DragShiftFrame(X86Context& ctx) {
    static uint32_t last_gear = ~0u, controls = 0;
    static uint64_t pressed_ms = 0, scanned_ms = 0;
    if (!Opt().cheat_drag_shift || !Racing()) return;
    if (kernel::G<uint8_t>(kDragByte) == 0 && kernel::R32(kDragWord) == 0) return;
    uint32_t player = kernel::R32(kPlayers);
    if (!Readable(player, kPlayerCar + 4)) return;
    uint32_t car = kernel::R32(player + kPlayerCar);
    if (!Readable(car, kCarRpm + 4)) return;
    uint64_t now = SteadyMs();
    // the controls change once racing (the car's own, then the driver's): looked for again when they no longer fit
    if (!IsControls(controls, car) || kernel::R32(car + kCarControls) + kDriverControls != controls) {
        uint32_t found = IsControls(controls, car) && now - scanned_ms < kRescanMs ? controls : 0;
        if (!found) {
            scanned_ms = now;
            found = FindControls(player, car);
            if (found && found != controls) LOGR("drag shift: controls %08X for car %08X", found, car);
        }
        controls = found;
    }
    if (!controls) return;
    uint32_t mode = kernel::R32(controls + kControlsMode);
    if (mode < 1 || mode > 3) return;  // automatic already shifts; 0's press opens something else (0x12CD80)
    uint32_t tuning = kernel::R32(car + kCarTuning);
    if (!Readable(tuning, kTuningTopRpm + 4)) return;
    uint32_t gear = kernel::R32(car + kCarGear);
    if (gear != last_gear) {
        last_gear = gear;
        pressed_ms = 0;
    }
    uint32_t top = kernel::R32(tuning + kTuningGears) + 1;  // the top gear, as 0x179E0 caps it
    if (gear < kFirstGear || gear >= top) return;
    float rpm = F32(car + kCarRpm), top_rpm = F32(tuning + kTuningTopRpm);
    if (!(top_rpm > 0)) top_rpm = F32(tuning + kTuningShiftLight) / kShiftAt;
    // the gear's best rpm, where the game grades a shift perfect
    float shift_at = top_rpm * kShiftAt;
    if (kernel::R32(car + kCarKind) == kCarHuman) {
        uint32_t holder = Readable(car, kCarDrag + 4) ? kernel::R32(car + kCarDrag) : 0;
        uint32_t info = Readable(holder, kDragInfo + 4) ? kernel::R32(holder + kDragInfo) : 0;
        float best = Readable(info, kInfoBestRpm + 4 * (gear + 1)) ? F32(info + kInfoBestRpm + gear * 4) : 0;
        if (best > kEarlyRpm && best <= top_rpm * 1.1f) shift_at = best - kEarlyRpm;
    }
    if (!(shift_at > 0) || !(rpm >= shift_at)) return;
    if (pressed_ms && now - pressed_ms < kRetryMs) return;
    pressed_ms = now;
    X86Context saved = ctx;
    kernel::CallGuest(ctx, kShiftUp, {0, kEventShiftUp, 1, controls});
    ctx = saved;
    LOGR("drag shift: up from gear %u at rpm %.0f, from %.0f", gear, rpm, shift_at);
}

}  // namespace game
