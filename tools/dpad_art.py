# Makes the dpad's art from the arrows' (app/src/main/assets/touch/{up,down,left,right}.png): the same round glass
# body with the arrow taken out, and the icon of what the dpad does in Underground 2 drawn on it in white, as its
# controller screen gives them: up the shop or a challenge, left the world map, right the career's statistics, down
# the SMS. The arrows stay: the steering arrows use them. Also the menus' OK and BACK, a face button's body (Y's, from
# shift_up.png) with A's or B's badge over it and a tick or a back arrow on it.
# usage: python tools/dpad_art.py
import os

import numpy

from PIL import Image, ImageChops, ImageDraw

DIR = os.path.join(os.path.dirname(__file__), "..", "app", "src", "main", "assets", "touch")
S = 4  # drawn this many times bigger, then reduced: smooth edges


def body(src, badge=None):
    # the art is white, its shading in its alpha, which inside the ring changes only from row to row. The icon (all
    # within 100 pixels of the body's centre, the bottom of the image less half its width) is covered by its row's
    # alpha 102 pixels out on the left. A badge's art over the body's top comes from another button's
    im = Image.open(os.path.join(DIR, src + ".png")).convert("RGBA")
    px = numpy.asarray(im).copy()
    h, w = px.shape[:2]
    cx, cy = w / 2, h - w / 2
    for y in range(h):
        dy = y + 0.5 - cy
        if abs(dy) >= 100:
            continue
        x0 = int(cx - (102 ** 2 - dy * dy) ** 0.5)
        half = int((100 ** 2 - dy * dy) ** 0.5)
        px[y, int(cx) - half:int(cx) + half + 1] = px[y, x0]
    if badge:
        other = numpy.asarray(Image.open(os.path.join(DIR, badge + ".png")).convert("RGBA"))
        px[:62, 92:164] = other[:62, 92:164]
    return Image.fromarray(px, "RGBA"), (int(cx), int(cy))


def shop(d, c):  # a shopping cart
    x, y = c
    w = 14 * S
    d.line([(x - 62 * S, y - 46 * S), (x - 44 * S, y - 46 * S), (x - 26 * S, y + 22 * S), (x + 44 * S, y + 22 * S)],
           fill=255, width=w, joint="curve")
    d.polygon([(x - 36 * S, y - 30 * S), (x + 60 * S, y - 30 * S), (x + 48 * S, y + 8 * S), (x - 26 * S, y + 8 * S)],
              fill=255)
    for cx in (x - 18 * S, x + 36 * S):
        d.ellipse([cx - 12 * S, y + 36 * S, cx + 12 * S, y + 60 * S], fill=255)


def world_map(d, c):  # a folded map, its three panels apart
    x, y = c
    top, bot, step = y - 50 * S, y + 50 * S, 10 * S
    xs = [x - 62 * S, x - 21 * S, x + 21 * S, x + 62 * S]
    gap = 4 * S
    for i in range(3):
        a, b = xs[i] + (gap if i else 0), xs[i + 1] - (gap if i < 2 else 0)
        # the panels slant up and down in turn, as a folded map's do
        la, lb = (0, step) if i % 2 == 0 else (step, 0)
        d.polygon([(a, top + la), (b, top + lb), (b, bot - step + lb), (a, bot - step + la)], fill=255)
    # a route across it, cut out of the white
    d.line([(x - 48 * S, y + 22 * S), (x - 20 * S, y - 6 * S), (x + 14 * S, y + 14 * S), (x + 46 * S, y - 26 * S)],
           fill=0, width=7 * S, joint="curve")


def stats(d, c):  # three bars rising over a base line
    x, y = c
    base = y + 52 * S
    for i, h in enumerate((44, 74, 104)):
        bx = x - 54 * S + i * 40 * S
        d.rounded_rectangle([bx, base - h * S, bx + 28 * S, base - 10 * S], radius=4 * S, fill=255)
    d.rounded_rectangle([x - 64 * S, base - 4 * S, x + 64 * S, base + 6 * S], radius=5 * S, fill=255)


def sms(d, c):  # a speech bubble with three dots
    x, y = c
    d.rounded_rectangle([x - 64 * S, y - 48 * S, x + 64 * S, y + 32 * S], radius=26 * S, fill=255)
    d.polygon([(x - 34 * S, y + 22 * S), (x - 4 * S, y + 22 * S), (x - 44 * S, y + 58 * S)], fill=255)
    for dx in (-32, 0, 32):
        d.ellipse([x + (dx - 10) * S, y - 18 * S, x + (dx + 10) * S, y + 2 * S], fill=0)


def tick(d, c):  # OK
    x, y = c
    d.line([(x - 46 * S, y + 2 * S), (x - 14 * S, y + 34 * S), (x + 48 * S, y - 32 * S)], fill=255, width=22 * S,
           joint="curve")


def back_arrow(d, c):  # BACK: an arrow turning back to the left
    x, y = c
    w = 18 * S
    d.arc([x - 40 * S, y - 34 * S, x + 50 * S, y + 46 * S], 270, 90, fill=255, width=w)
    d.line([(x + 5 * S, y + 46 * S - w // 2), (x - 30 * S, y + 46 * S - w // 2)], fill=255, width=w)
    d.line([(x + 5 * S, y - 34 * S + w // 2), (x - 18 * S, y - 34 * S + w // 2)], fill=255, width=w)
    d.polygon([(x - 56 * S, y - 25 * S), (x - 14 * S, y - 58 * S), (x - 14 * S, y + 8 * S)], fill=255)


# its name: its icon, the art its body is from, the art its badge is from
ICONS = {
    "dpad_shop": (shop, "up", None),
    "dpad_map": (world_map, "up", None),
    "dpad_stats": (stats, "up", None),
    "dpad_sms": (sms, "up", None),
    "menu_ok": (tick, "shift_up", "a"),
    "menu_back": (back_arrow, "shift_up", "b"),
}


def main():
    for name, (draw, src, badge) in ICONS.items():
        base, c = body(src, badge)
        w, h = base.size
        mask = Image.new("L", (w * S, h * S), 0)
        draw(ImageDraw.Draw(mask), (c[0] * S, c[1] * S))
        mask = mask.resize((w, h), Image.LANCZOS)
        icon = Image.new("RGBA", (w, h), (255, 255, 255, 0))
        icon.putalpha(ImageChops.multiply(mask, base.getchannel("A").point(lambda v: 255 if v else 0)))
        im = Image.alpha_composite(base, icon)
        im.save(os.path.join(DIR, name + ".png"), optimize=True)
        print(f"{name}: {im.width}x{im.height}")


if __name__ == "__main__":
    main()
