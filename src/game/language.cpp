// The game's text in Indonesian, which no disc has. The text of a language is one file, LANGUAGES\ENGLISH.BIN for the console's
// English (ConsoleLanguage), whose chunk 0x39000 0x11ED10 loads: the count of its strings in [0x3FBC20], the table of their
// (hash, text) pairs, sorted by hash, in [0x3FBC28], with the offsets already made addresses (the lookup 0x1105C0 is a binary
// search of it by the hash). With the language set to Indonesian each text we have for a hash (lang_id_data.inc, made by
// tools/lang_id.py) is copied once to memory of ours and its address put in the pair: the rest of the game reads it as it
// reads its own, and what has no translation (names of cars, tracks and brands) stays English. The texts are in the game's
// own byte set (7 bits, the apostrophe and the like through its table), which Indonesian needs none of.
#include <algorithm>
#include <cstring>
#include <iterator>
#include <string_view>

#include "common/options.h"
#include "core/log.h"
#include "cpu/x86_ops.h"
#include "kernel/kernel.h"
#include "kernel/memory.h"

namespace {

struct Text {
    uint32_t hash;
    const char* text;
};
// sorted by hash
constexpr Text kTexts[] = {
#include "lang_id_data.inc"
};

constexpr uint32_t kCount = 0x3FBC20, kTable = 0x3FBC28;

// the texts' place in guest memory, made on the first chunk
uint32_t g_texts = 0;
uint32_t g_at[std::size(kTexts)];

void Apply() {
    if (!g_texts) {
        uint32_t bytes = 0;
        for (auto& t : kTexts) bytes += uint32_t(std::strlen(t.text)) + 1;
        g_texts = mem::AllocSystem(bytes, 4);
        if (!g_texts) {
            LOGR("language: no memory for %u bytes of Indonesian text", bytes);
            return;
        }
        uint32_t at = g_texts;
        for (size_t i = 0; i < std::size(kTexts); i++) {
            size_t n = std::strlen(kTexts[i].text) + 1;
            std::memcpy(kernel::HostPtr(at), kTexts[i].text, n);
            g_at[i] = at;
            at += uint32_t(n);
        }
    }
    uint32_t count = kernel::R32(kCount), table = kernel::R32(kTable);
    if (!table || count > 0x10000) return;
    uint32_t swapped = 0;
    for (uint32_t i = 0; i < count; i++) {
        uint32_t pair = table + i * 8, hash = kernel::R32(pair);
        auto it = std::lower_bound(std::begin(kTexts), std::end(kTexts), hash,
                                   [](const Text& t, uint32_t h) { return t.hash < h; });
        if (it == std::end(kTexts) || it->hash != hash) continue;
        kernel::W32(pair + 4, g_at[it - std::begin(kTexts)]);
        swapped++;
    }
    LOGR("language: %u of %u texts in Indonesian", swapped, count);
}

}  // namespace

X86_EXTERN_FUNC(__imp__sub_0011ED10);
X86_FUNC(sub_0011ED10) {
    __imp__sub_0011ED10(ctx, base);
    if (ctx.eax == 1 && game::BootLanguage() == "ind") Apply();
}

// The language the game boots in: 0x11EC80 asks 0x1C18A0 for the id of the console's language in the game's table at 0x3A0640
// (0 English, 1 French, 2 German, 3 Italian, 4 Spanish, 5 Dutch, 6 Swedish, 7 Danish; the others are Asian), which the
// console's language gives for English, French, German and Italian only, Spanish and the rest never (PAL discs ask the
// player). The language setting gives it instead, Indonesian the English one its text is swapped in.
X86_EXTERN_FUNC(__imp__sub_001C18A0);
X86_FUNC(sub_001C18A0) {
    __imp__sub_001C18A0(ctx, base);
    static constexpr struct {
        const char* code;
        uint32_t id;
    } kIds[] = {{"int", 0}, {"ind", 0}, {"fra", 1}, {"deu", 2}, {"ita", 3}, {"esp", 4}, {"nld", 5}, {"swe", 6}, {"dan", 7}};
    for (auto& l : kIds)
        if (game::BootLanguage() == l.code) ctx.eax = l.id;
}
