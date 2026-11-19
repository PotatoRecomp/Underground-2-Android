# Draws the icons of ANDROID OPTIONS (the main menu's and pause menu's) and of its sections (GRAPHICS, CONTROLS,
# GENERAL: src/game/android_options.cpp) and writes them as src/game/option_icons.inc. They are drawn as the
# game's main menu icons are (MAIN_ICON_*: 128x64, white, the picture in alpha): a rounded screen with a glare across it, see through, and the subject in front of it on its
# right, more solid, a gap cut round it.
# usage: python tools/option_icons.py [preview.png]
import os
import sys

from PIL import Image, ImageChops, ImageDraw, ImageFilter

W, H = 128, 64
SCALE = 8
OUT = os.path.join(os.path.dirname(__file__), "..", "src", "game", "option_icons.inc")

# the game's alphas, from MAIN_ICON_OPTIONS: outlines 0.8, fills about 0.45
EDGE, FILL, SUBJECT = 204, 110, 240


def canvas():
    img = Image.new("L", (W * SCALE, H * SCALE), 0)
    return img, ImageDraw.Draw(img)


def p(*xy):
    return [v * SCALE for v in xy]


def screen():
    # the screen: a rounded frame, its inside see through, a glare from its corner
    img, d = canvas()
    d.rounded_rectangle(p(14, 10, 78, 54), radius=5 * SCALE, fill=EDGE)
    d.rounded_rectangle(p(18, 14, 74, 50), radius=3 * SCALE, fill=FILL)
    d.polygon(p(18, 38, 46, 14, 54, 14, 18, 44), fill=EDGE)
    return img


def robot():
    # Android's robot: the head a half disc with antennae and eyes, the body under it, arms and legs
    img, d = canvas()
    cx = 84
    d.pieslice(p(cx - 15, 14, cx + 15, 42), 180, 360, fill=255)          # head
    d.line(p(cx - 9, 17, cx - 13, 10), fill=255, width=2 * SCALE)        # antennae
    d.line(p(cx + 9, 17, cx + 13, 10), fill=255, width=2 * SCALE)
    d.rounded_rectangle(p(cx - 15, 30, cx + 15, 50), radius=3 * SCALE, fill=255)  # body
    d.rounded_rectangle(p(cx - 22, 30, cx - 17, 46), radius=2.5 * SCALE, fill=255)  # arms
    d.rounded_rectangle(p(cx + 17, 30, cx + 22, 46), radius=2.5 * SCALE, fill=255)
    d.rounded_rectangle(p(cx - 9, 46, cx - 4, 57), radius=2.5 * SCALE, fill=255)  # legs
    d.rounded_rectangle(p(cx + 4, 46, cx + 9, 57), radius=2.5 * SCALE, fill=255)
    d.ellipse(p(cx - 8, 21, cx - 4.5, 24.5), fill=0)                       # eyes
    d.ellipse(p(cx + 4.5, 21, cx + 8, 24.5), fill=0)
    return img


def monitor():
    # a display on a stand, a mountain and a sun on it
    img, d = canvas()
    cx = 86
    d.rounded_rectangle(p(cx - 22, 14, cx + 22, 44), radius=2.5 * SCALE, fill=255)
    d.rectangle(p(cx - 18, 18, cx + 18, 40), fill=0)
    d.polygon(p(cx - 16, 39, cx - 5, 26, cx + 3, 34, cx + 8, 29, cx + 16, 39), fill=255)
    d.ellipse(p(cx + 7, 20, cx + 13, 26), fill=255)
    d.rectangle(p(cx - 3, 44, cx + 3, 51), fill=255)
    d.rounded_rectangle(p(cx - 12, 50, cx + 12, 55), radius=2 * SCALE, fill=255)
    return img


def pad():
    # a controller: the body with its grips, the dpad and four buttons cut out
    img, d = canvas()
    cx, cy = 86, 34
    d.rounded_rectangle(p(cx - 23, cy - 10, cx + 23, cy + 8), radius=8 * SCALE, fill=255)
    d.ellipse(p(cx - 25, cy - 4, cx - 9, cy + 18), fill=255)
    d.ellipse(p(cx + 9, cy - 4, cx + 25, cy + 18), fill=255)
    d.rectangle(p(cx - 17, cy - 2, cx - 7, cy + 1), fill=0)
    d.rectangle(p(cx - 13.5, cy - 5.5, cx - 10.5, cy + 4.5), fill=0)
    for bx, by in ((cx + 12, cy - 5), (cx + 16, cy - 1), (cx + 8, cy - 1), (cx + 12, cy + 3)):
        d.ellipse(p(bx - 1.8, by - 1.8, bx + 1.8, by + 1.8), fill=0)
    return img


def sliders():
    # three sliders, their knobs at different places
    img, d = canvas()
    for i, (y, k) in enumerate(((20, 0.3), (33, 0.75), (46, 0.5))):
        x0, x1 = 64, 108
        d.rounded_rectangle(p(x0, y - 1.5, x1, y + 1.5), radius=1.5 * SCALE, fill=255)
        kx = x0 + (x1 - x0) * k
        d.ellipse(p(kx - 5.5, y - 5.5, kx + 5.5, y + 5.5), fill=255)
        d.ellipse(p(kx - 2.5, y - 2.5, kx + 2.5, y + 2.5), fill=0)
    return img


# in src/game/android_options.cpp's order: ANDROID OPTIONS, then its sections
ICONS = [robot, monitor, pad, sliders]


def compose(subject):
    back, front = screen(), subject()
    # the gap round the subject, as the game's icons cut one round theirs
    gap = front.filter(ImageFilter.MaxFilter(2 * 2 * SCALE + 1))
    back = ImageChops.subtract(back, gap)
    front = front.point(lambda v: v * SUBJECT // 255)
    out = ImageChops.lighter(back, front)
    return out.resize((W, H), Image.LANCZOS)


def main():
    icons = [compose(f) for f in ICONS]
    if len(sys.argv) > 1:
        sheet = Image.new("RGBA", (W, H * len(icons)), (40, 60, 40, 255))
        for i, alpha in enumerate(icons):
            white = Image.new("RGBA", (W, H), (255, 255, 255, 255))
            white.putalpha(alpha)
            sheet.alpha_composite(white, (0, H * i))
        sheet.resize((W * 3, H * 3 * len(icons)), Image.NEAREST).save(sys.argv[1])
    lines = ["// written by tools/option_icons.py, do not edit",
             f"constexpr uint32_t kIconCount = {len(icons)}, kIconWidth = {W}, kIconHeight = {H};",
             "constexpr uint32_t kIcons[kIconCount][kIconWidth * kIconHeight] = {"]
    for alpha in icons:
        a = alpha.load()
        lines.append("    {")
        for y in range(H):
            lines.append("        " + ", ".join(f"0x{a[x, y]:02X}FFFFFF" for x in range(W)) + ",")
        lines.append("    },")
    lines.append("};")
    with open(OUT, "w", newline="\n") as f:
        f.write("\n".join(lines) + "\n")


if __name__ == "__main__":
    main()
