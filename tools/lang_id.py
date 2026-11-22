# Builds src/game/lang_id_data.inc, the Indonesian texts of the game's English language file (LANGUAGES\ENGLISH.BIN, in
# the USA disc's ZZDATA archives), keyed by the file's string hashes: lang_id.py [<iso>] [<out>]
#
# The English chunk (id 0x39000: header, charset table, 3849 sorted (hash, offset) pairs, UTF-8 strings) is found on
# the disc by its size; the translations in tools/lang_id/tr*.txt are "<index>|<text>" lines, the index being the
# string's place in the table and the first of identical English strings (the others get the same text). {xx} stands
# for the byte xx (the game's own marks: {82} the trademark sign, {a9} the copyright sign). The key hints bars
# (JOY_EVENT ... phrases) are translated from tools/lang_id/phrases.txt, the tracks' "Reverse" is "Terbalik".
import glob, mmap, os, re, struct, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CHUNK_SIZE, COUNT = 242820, 3849


def find_iso():
    for pat in ("iso/*.iso", "assets/*.iso", "Iso/*.iso"):
        hits = glob.glob(os.path.join(ROOT, pat))
        if hits:
            return hits[0]
    sys.exit("no iso found: lang_id.py <iso>")


def english_chunk(iso):
    with open(iso, "rb") as f, mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ) as m:
        key = struct.pack("<II", 0x39000, CHUNK_SIZE)
        at = -1
        while True:
            at = m.find(key, at + 1)
            if at < 0:
                sys.exit("English language chunk not found on the disc")
            head = struct.unpack_from("<IIII", m, at + 8)
            if head[1] == COUNT:
                return bytes(m[at + 8: at + 8 + CHUNK_SIZE])


def parse(d):
    a, count, table, base = struct.unpack_from("<IIII", d, 0)
    out = []
    for i in range(count):
        h, off = struct.unpack_from("<II", d, table + i * 8)
        end = d.index(b"\0", base + off)
        out.append((h, d[base + off:end].decode("latin-1")))
    return out


def load_numbered():
    tr = {}
    for path in sorted(glob.glob(os.path.join(ROOT, "tools", "lang_id", "tr*.txt"))):
        with open(path, encoding="utf-8") as f:
            for n, line in enumerate(f, 1):
                line = line.rstrip("\n")
                if not line:
                    continue
                idx, _, text = line.partition("|")
                tr[int(idx)] = text
    return tr


def load_phrases():
    ph = {}
    with open(os.path.join(ROOT, "tools", "lang_id", "phrases.txt"), encoding="utf-8") as f:
        for line in f:
            line = line.rstrip("\n")
            if line:
                a, _, b = line.partition("|")
                ph[a] = b
    return ph


TOKEN = re.compile(r"(\$[A-Z_0-9]+\$)")


def hint_bar(s, phrases, unknown):
    parts = []
    for line in s.split("^"):
        pieces = TOKEN.split(line)
        for i, p in enumerate(pieces):
            if i % 2 or not p.strip():
                continue
            lead = p[:len(p) - len(p.lstrip())]
            trail = p[len(p.rstrip()):]
            words = re.split(r"( {2,})", p.strip())
            for j, w in enumerate(words):
                if j % 2 or not w:
                    continue
                if w in phrases:
                    words[j] = phrases[w]
                else:
                    unknown.append(w)
            pieces[i] = lead + "".join(words) + trail
        parts.append("".join(pieces))
    return "^".join(parts)


def to_bytes(text):
    return re.sub(r"\{([0-9a-f]{2})\}", lambda m: chr(int(m.group(1), 16)), text)


def c_string(text):
    out = []
    for ch in text:
        c = ord(ch)
        if ch in '"\\?' or c < 0x20 or c >= 0x7F:
            out.append("\\%03o" % c)
        else:
            out.append(ch)
    return '"' + "".join(out) + '"'


def tokens(s):
    # the game's apostrophe, dashes and ellipsis marks ({81} {83} {85} {86}) may be left out of a translation
    s = re.sub(r"\{8[1356]\}", "", s)
    return sorted(re.findall(r"%[-+ #0-9.$*lh]*[a-zA-Z%]|\$[A-Z_0-9]+\$|\{[0-9a-f]{2}\}", s))


def main():
    iso = sys.argv[1] if len(sys.argv) > 1 else find_iso()
    out_path = sys.argv[2] if len(sys.argv) > 2 else os.path.join(ROOT, "src", "game", "lang_id_data.inc")
    english = parse(english_chunk(iso))
    numbered = load_numbered()
    phrases = load_phrases()
    first = {}
    for i, (h, s) in enumerate(english):
        first.setdefault(s, i)
    result, unknown, problems = {}, [], []
    for i, (h, s) in enumerate(english):
        text = None
        j = first[s]
        if "$JOY_EVENT" in s or "$ICON_SPACER$" in s:
            text = hint_bar(s, phrases, unknown)
        elif j in numbered:
            text = numbered[j]
        elif s.endswith(" Reverse") and 3120 <= i <= 3175:
            text = s[:-len(" Reverse")] + " Terbalik"
        elif re.search(r" (INCH|WATTS|LB BOTTLE|CARBON FIBRE)", s):
            # the audio and nitrous parts' units, brand names left as they are
            text = (s.replace(" INCH", " INCI").replace(" WATTS", " WATT").replace(" LB BOTTLE", " LB TABUNG")
                    .replace(" CARBON FIBRE", " SERAT KARBON"))
        if text is None:
            continue
        visible = re.sub(r"[\x80-\xff]", lambda m: "{%02x}" % ord(m.group()), s)
        if tokens(visible) != tokens(text):
            problems.append((i, visible, text))
        result[h] = to_bytes(text)
    for i, e, t in problems:
        print("token mismatch at %d:\n  %s\n  %s" % (i, e[:160], t[:160]))
    if unknown:
        print("unknown phrases:", sorted(set(unknown)))
    lines = ["// generated by tools/lang_id.py: Indonesian texts by the hash of their string in the English language file"]
    for h in sorted(result):
        lines.append("    {0x%08Xu, %s}," % (h, c_string(result[h])))
    with open(out_path, "w", encoding="ascii", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    letters = [i for i, (h, s) in enumerate(english) if re.search(r"[A-Za-z]{2}", s)]
    print("%d of %d strings translated (%d with letters)" % (len(result), len(english), len(letters)))


main()
