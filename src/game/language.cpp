// The game's text in Indonesian and Portuguese, which no disc has. The text of a language is one file, LANGUAGES\ENGLISH.BIN for
// the console's English (ConsoleLanguage), whose chunk 0x39000 0x11ED10 loads: the count of its strings in [0x3FBC20], the
// table of their (hash, text) pairs, sorted by hash, in [0x3FBC28], with the offsets already made addresses (the lookup
// 0x1105C0 is a binary search of it by the hash). With the language set to one of these each text we have for a hash
// (lang_id_data.inc and lang_pt_data.inc, made by tools/lang_text.py) is copied once to memory of ours and its address put in
// the pair: the rest of the game reads it as it reads its own, and what has no translation (names of cars, tracks and
// brands) stays English. The texts are in the game's own byte set: 7 bits, and a byte over 0x7F goes through the chunk's
// charset table ([0x3FBC2C], 16 bits for each byte from +4), which the English one fills for a few marks only, so Portuguese
// sets the Latin 1 letters it needs in it (the font has them: the other languages of the disc draw them).
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
constexpr Text kIndonesian[] = {
#include "lang_id_data.inc"
};
constexpr Text kPortuguese[] = {
#include "lang_pt_data.inc"
};

constexpr uint32_t kCount = 0x3FBC20, kTable = 0x3FBC28, kCharset = 0x3FBC2C;

// a language's texts and their place in guest memory, made on the first chunk
struct Pack {
    const Text* texts;
    size_t count;
    uint32_t base = 0;
    uint32_t* at;
};

uint32_t g_indonesian_at[std::size(kIndonesian)], g_portuguese_at[std::size(kPortuguese)];
Pack g_indonesian{kIndonesian, std::size(kIndonesian), 0, g_indonesian_at};
Pack g_portuguese{kPortuguese, std::size(kPortuguese), 0, g_portuguese_at};

bool Place(Pack& p) {
    if (p.base) return true;
    uint32_t bytes = 0;
    for (size_t i = 0; i < p.count; i++) bytes += uint32_t(std::strlen(p.texts[i].text)) + 1;
    p.base = mem::AllocSystem(bytes, 4);
    if (!p.base) {
        LOGR("language: no memory for %u bytes of text", bytes);
        return false;
    }
    uint32_t at = p.base;
    for (size_t i = 0; i < p.count; i++) {
        size_t n = std::strlen(p.texts[i].text) + 1;
        std::memcpy(kernel::HostPtr(at), p.texts[i].text, n);
        p.at[i] = at;
        at += uint32_t(n);
    }
    return true;
}

void Apply(Pack& p, const char* name) {
    if (!Place(p)) return;
    uint32_t count = kernel::R32(kCount), table = kernel::R32(kTable);
    if (!table || count > 0x10000) return;
    uint32_t swapped = 0;
    const Text* end = p.texts + p.count;
    for (uint32_t i = 0; i < count; i++) {
        uint32_t pair = table + i * 8, hash = kernel::R32(pair);
        const Text* it = std::lower_bound(p.texts, end, hash, [](const Text& t, uint32_t h) { return t.hash < h; });
        if (it == end || it->hash != hash) continue;
        kernel::W32(pair + 4, p.at[it - p.texts]);
        swapped++;
    }
    LOGR("language: %u of %u texts in %s", swapped, count, name);
}

// the letters of Latin 1 (the division and multiplication signs left out) drawn as themselves
void LatinLetters() {
    uint32_t charset = kernel::R32(kCharset);
    if (!charset) return;
    for (uint32_t c = 0xC0; c <= 0xFF; c++) {
        if (c == 0xD7 || c == 0xF7) continue;
        uint16_t unit = uint16_t(c);
        std::memcpy(kernel::HostPtr(charset + 4 + c * 2), &unit, 2);
    }
}

}  // namespace

X86_EXTERN_FUNC(__imp__sub_0011ED10);
X86_FUNC(sub_0011ED10) {
    __imp__sub_0011ED10(ctx, base);
    if (ctx.eax != 1) return;
    if (game::BootLanguage() == "ind") {
        Apply(g_indonesian, "Indonesian");
    } else if (game::BootLanguage() == "por") {
        LatinLetters();
        Apply(g_portuguese, "Portuguese");
    }
}

// The language the game boots in: 0x11EC80 asks 0x1C18A0 for the id of the console's language in the game's table at 0x3A0640
// (0 English, 1 French, 2 German, 3 Italian, 4 Spanish, 5 Dutch, 6 Swedish, 7 Danish; the others are Asian), which the
// console's language gives for English, French, German and Italian only, Spanish and the rest never (PAL discs ask the
// player). The language setting gives it instead, Indonesian and Portuguese the English one their text is swapped in.
X86_EXTERN_FUNC(__imp__sub_001C18A0);
X86_FUNC(sub_001C18A0) {
    __imp__sub_001C18A0(ctx, base);
    static constexpr struct {
        const char* code;
        uint32_t id;
    } kIds[] = {{"int", 0}, {"ind", 0}, {"por", 0}, {"fra", 1}, {"deu", 2},
                {"ita", 3}, {"esp", 4}, {"nld", 5}, {"swe", 6}, {"dan", 7}};
    for (auto& l : kIds)
        if (game::BootLanguage() == l.code) ctx.eax = l.id;
}
