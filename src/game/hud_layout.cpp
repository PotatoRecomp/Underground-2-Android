// The HUD's speedometer, minimap and race information moved where the touch layout editor puts them, out from under
// the touch controls. A race's HUD and free roam's is the FEng package HUD_SingleRace.fng, a drag race's HUD_Drag.fng (cFEng 0x3FBD08, its FEngine
// +8, the packages from +0xE4, each's name +0xC and objects +0x4C; an object's next +4, name hash +0x10, type +0x18,
// a group's (5) objects +0x64, its data +0x2C with its x and y at +0x1C and +0x20, FEng units from its group's
// middle: 0x129F40). Its pieces at the top of the package are taken by where the package puts them: the bottom left's
// (the map, its arrows and its rim) the minimap, the bottom right's (the tachometer, its gauges) the speedometer, the
// top right's (the position, the laps, the racers, the time) the race information; those in the middle (the messages,
// the countdown) and off the screen stay. The game writes some places each frame and the package's scripts others,
// so they are moved after FEngine::Update (0x163050), the last writer before the drawing, and a place found other
// than as it was left is the game's new one, moved again from there. In HUD_Drag.fng the tachometer is the arc in the
// middle (the cluster group 0x5164D4EA, with the gear over it), an item of its own; its top right (the speed) is the race
// information's, and the rest stays
#include <cmath>
#include <cstring>
#include <unordered_map>

#include "common/hud_layout.h"
#include "cpu/x86_ops.h"
#include "kernel/kernel.h"
#include "kernel/memory.h"

namespace game {

namespace {

constexpr uint32_t kCFEng = 0x3FBD08, kFEngine = 0x8, kFirstPackage = 0xE4;
constexpr uint32_t kPackageNext = 0x4, kPackageName = 0xC, kPackageObjects = 0x4C;
constexpr uint32_t kObjectNext = 0x4, kObjectHash = 0x10, kObjectFlags = 0x1C, kObjectData = 0x2C;
// an object changed: FEng works its place out again for the drawing (0x118CE0 and 0x15B2E0 set it with theirs)
constexpr uint32_t kDirty = 0x400000;
constexpr uint32_t kDataX = 0x1C, kDataY = 0x20;
constexpr int kMaxPackages = 64, kMaxObjects = 600;
constexpr int kNone = -1;

// those whose place alone would not say: the tachometer's cluster, the map's ring, the racers
struct Known {
    uint32_t hash;
    HudItem item;
};
constexpr Known kKnown[] = {{0x5164D4EA, kHudSpeedometer}, {0x382D2FC9, kHudMinimap}, {0x5ED609A1, kHudRaceInfo}};

// an object's item by the place the package gave it, FEng units (the middle of the frame 0, y down)
int ItemAt(uint32_t hash, float x, float y, bool drag) {
    if (drag) {
        if (hash == 0x5164D4EA) return kHudDragTach;
        if (std::fabs(y) >= 260 || std::fabs(x) >= 420) return kNone;
        if (x <= -120 && y >= -60) return kHudDragTach;  // beside the arc: its red marker, an icon
        return x >= 100 && y <= -60 ? kHudRaceInfo : kNone;
    }
    for (const Known& k : kKnown)
        if (k.hash == hash) return int(k.item);
    if (std::fabs(y) >= 260 || std::fabs(x) >= 420) return kNone;  // kept off the screen until used
    if (x <= -120 && y >= -40) return kHudMinimap;
    if (x >= 100 && y >= 20) return kHudSpeedometer;
    if (x >= 100 && y <= -60) return kHudRaceInfo;
    return kNone;
}

// each item's piece the editor's box follows, and its place as the box was measured
struct Anchor {
    uint32_t hash;
    bool drag;
    HudItem item;
    float x, y;
};
constexpr Anchor kAnchors[] = {{0x5164D4EA, false, kHudSpeedometer, 142, 195},
                               {0x382D2FC9, false, kHudMinimap, -223, 142},
                               {0x5ED609A1, false, kHudRaceInfo, 196, -125},
                               {0x5164D4EA, true, kHudDragTach, 16, 53}};

struct Moved {
    int item;
    float x, y;          // the place the game gave it
    float set_x, set_y;  // what was written over it
};
std::unordered_map<uint32_t, Moved> g_moved;  // by the object's data

uint32_t Peek(uint32_t va) { return va && mem::IsCommitted(va) && mem::IsCommitted(va + 3) ? kernel::R32(va) : 0; }

float Float(uint32_t va) {
    uint32_t b = kernel::R32(va);
    float f;
    std::memcpy(&f, &b, 4);
    return f;
}
void SetFloat(uint32_t va, float f) {
    uint32_t b;
    std::memcpy(&b, &f, 4);
    kernel::W32(va, b);
}

void Move(uint32_t object, bool drag, std::unordered_map<uint32_t, Moved>& seen) {
    uint32_t data = Peek(object + kObjectData);
    if (!data || !mem::IsCommitted(data + kDataY + 3)) return;
    float x = Float(data + kDataX), y = Float(data + kDataY);
    // the place is the game's own: RestoreHud put it back before the update
    Moved m{kNone, x, y, x, y};
    auto it = g_moved.find(data);
    uint32_t hash = Peek(object + kObjectHash);
    m.item = it == g_moved.end() ? ItemAt(hash, x, y, drag) : it->second.item;
    // an item's anchor: where it is now against where kHudBoxes has it, for the editor
    for (const Anchor& a : kAnchors)
        if (a.hash == hash && a.drag == drag) {
            g_hud_base_x[a.item] = x - a.x;
            g_hud_base_y[a.item] = y - a.y;
        }
    if (m.item != kNone) {
        m.set_x = m.x + g_hud_x[m.item].load();
        m.set_y = m.y + g_hud_y[m.item].load();
        if (m.set_x != x) SetFloat(data + kDataX, m.set_x);
        if (m.set_y != y) SetFloat(data + kDataY, m.set_y);
        if (m.set_x != x || m.set_y != y) kernel::W32(object + kObjectFlags, kernel::R32(object + kObjectFlags) | kDirty);
    }
    seen[data] = m;
}

// a HUD package: 1, 2 for the drag race's, 0 for none
int HudPackage(uint32_t p) {
    uint32_t name = Peek(p + kPackageName);
    char head[8] = {};
    for (uint32_t i = 0; name && i < 8 && mem::IsCommitted(name + i); i++) head[i] = char(kernel::G<uint8_t>(name + i));
    if (std::memcmp(head, "HUD_", 4) != 0) return 0;
    return std::memcmp(head, "HUD_Drag", 8) == 0 ? 2 : 1;
}

}  // namespace

// Before the update the game's own places go back where it had them: its scripts set some places outright and step
// others from where they are (the drag race's tachometer shakes at a shift); stepped from where we had put it, it took
// the offset in as its own and drifted off by it again at each. A place changed since we left it was written out of
// the update: a step if it is nearer where we put it, else a place of its own
void RestoreHud() {
    for (auto& [data, m] : g_moved) {
        if (m.item == kNone || !mem::IsCommitted(data + kDataY + 3)) continue;
        float x = Float(data + kDataX), y = Float(data + kDataY);
        float gx = m.x, gy = m.y;
        if (x != m.set_x || y != m.set_y) {
            float step = std::fabs(x - m.set_x) + std::fabs(y - m.set_y), own = std::fabs(x - m.x) + std::fabs(y - m.y);
            gx = step < own ? m.x + (x - m.set_x) : x;
            gy = step < own ? m.y + (y - m.set_y) : y;
        }
        if (gx != x) SetFloat(data + kDataX, gx);
        if (gy != y) SetFloat(data + kDataY, gy);
    }
}

// FEngine::Update runs a few hundred times a second: with nothing moved and nothing to move back it is left at once,
// and the maps are kept rather than made each time (the boot's movies started on a race of the game's own that more
// time spent here made likelier)
void ApplyHudLayout() {
    bool offsets = false;
    for (size_t i = 0; i < kHudItems; i++) offsets |= g_hud_x[i].load() != 0 || g_hud_y[i].load() != 0;
    if (!offsets && g_moved.empty()) return;
    uint32_t c = Peek(kCFEng), fe = c ? Peek(c + kFEngine) : 0;
    static std::unordered_map<uint32_t, Moved> seen;
    seen.clear();
    int n = 0;
    for (uint32_t p = fe ? Peek(fe + kFirstPackage) : 0; p && n < kMaxPackages; p = Peek(p + kPackageNext), n++) {
        int kind = HudPackage(p);
        if (!kind) continue;
        int budget = kMaxObjects;
        for (uint32_t o = Peek(p + kPackageObjects); o && budget-- > 0; o = Peek(o + kObjectNext)) Move(o, kind == 2, seen);
    }
    g_moved.swap(seen);  // a package gone forgets its objects
}

}  // namespace game

// FEngine::Update (thiscall, ret 8): the game's places back for it, then ours over what it made of them for the drawing
X86_EXTERN_FUNC(__imp__sub_00163050);
X86_FUNC(sub_00163050) {
    game::RestoreHud();
    __imp__sub_00163050(ctx, base);
    game::ApplyHudLayout();
}
