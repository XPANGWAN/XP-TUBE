#!/usr/bin/env python3
"""Makes the PS4 home screen art and the README logo of Playtube (needs Pillow).

    python3 scripts/make-ps4-art.py

The mark (the user's choice, 2026-11-12): a red screen with rounded corners on two small feet, a white play
triangle inside: YouTube's colours on a television, not YouTube's logo. The name "Playtube" is set in Roboto
Condensed Bold (third_party/fonts, SIL OFL; used only here, not in the package).

icon0.png (512x512): white, the mark, "Playtube" in black under it. pic1.png (1920x1080), shown by the PS4 behind
the selected app: the app's plain dark background with the mark and "Playtube" at the bottom right (the console
writes its white labels over the top and middle, so the picture stays dark). images/logo-on-dark.png and
logo-on-light.png: the mark and the name on a transparent background for the README (GitHub's dark and light
themes).
"""
import os

from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
BACKGROUND = (0x07, 0x07, 0x08)  # the app's background (src/ui/theme.cpp)
RED = (0xFF, 0x00, 0x33)
WHITE = (0xFF, 0xFF, 0xFF)
INK = (0x0F, 0x0F, 0x0F)
LIGHT_TEXT = (0xF1, 0xF1, 0xF1)
FONT = os.path.join(ROOT, "third_party", "fonts", "RobotoCondensed[wght].ttf")
WEIGHT = 700
TRACKING = -0.03  # of the font size, between letters

SCALE = 4  # drawn larger, then reduced: smooth curves

# The mark in its own units: the screen 320 x 214 with corners of 56, two feet 50 x 34 under it; 320 x 248 in all.
MARK_W, MARK_H = 320.0, 248.0


def font(size):
    face = ImageFont.truetype(FONT, size)
    face.set_variation_by_axes([WEIGHT])
    return face


def rounded_play(draw, cx, cy, size, fill):
    """A play triangle pointing right, centred on (cx, cy), with round corners (radius 8 % of its height)."""
    h, w, r = size, size * 0.9, size * 0.08
    points = [(cx - w * 0.42 + r, cy - h / 2 + 2 * r), (cx + w * 0.58 - 2 * r, cy), (cx - w * 0.42 + r, cy + h / 2 - 2 * r)]
    draw.polygon(points, fill=fill)
    for a, b in zip(points, points[1:] + points[:1]):
        draw.line([a, b], fill=fill, width=round(2 * r))
    for x, y in points:
        draw.ellipse((x - r, y - r, x + r, y + r), fill=fill)


def mark(image, left, top, width):
    """The red screen on its feet with the white triangle, `width` pixels wide, its top left at (left, top)."""
    k = width / MARK_W * SCALE
    big = Image.new("RGBA", (round(MARK_W * k), round(MARK_H * k)), (0, 0, 0, 0))
    draw = ImageDraw.Draw(big)
    for foot_x in (64, 206):
        draw.rounded_rectangle((foot_x * k, 190 * k, (foot_x + 50) * k, 248 * k), radius=14 * k, fill=RED)
    draw.rounded_rectangle((0, 0, 320 * k, 214 * k), radius=56 * k, fill=RED)
    rounded_play(draw, 160 * k, 107 * k, 92 * k, WHITE)
    small = big.resize((round(width), round(width * MARK_H / MARK_W)), Image.LANCZOS)
    image.paste(small, (round(left), round(top)), small)
    return small.height


def name_width(face, size):
    probe = ImageDraw.Draw(Image.new("RGBA", (1, 1)))
    text = "Playtube"
    return sum(probe.textlength(c, font=face) for c in text) + TRACKING * size * (len(text) - 1)


def draw_name(image, x, baseline, size, colour):
    """"Playtube" from its baseline, starting at x, with the tracking; returns its width."""
    face = font(size)
    draw = ImageDraw.Draw(image)
    pen = x
    for c in "Playtube":
        draw.text((pen, baseline), c, font=face, fill=colour, anchor="ls")
        pen += draw.textlength(c, font=face) + TRACKING * size
    return pen - TRACKING * size - x


def logo(text_colour, size=150, margin=16):
    """The mark and the name side by side, cropped to their ink plus a margin, on a transparent background."""
    face = font(size)
    probe = ImageDraw.Draw(Image.new("RGBA", (1, 1)))
    ascent = -probe.textbbox((0, 0), "P", font=face, anchor="ls")[1]  # cap height
    descent = probe.textbbox((0, 0), "y", font=face, anchor="ls")[3]
    mark_w = ascent * 1.6
    mark_h = mark_w * MARK_H / MARK_W
    gap = size * 0.12
    width = mark_w + gap + name_width(face, size)
    height = max(mark_h, ascent + descent)
    image = Image.new("RGBA", (round(width + 2 * margin), round(height + 2 * margin)), (0, 0, 0, 0))
    centre = margin + height / 2
    mark(image, margin, centre - mark_h / 2 - mark_h * 0.04, mark_w)
    draw_name(image, margin + mark_w + gap, centre + ascent / 2, size, text_colour)
    return image


def main():
    out = os.path.join(ROOT, "resources", "ps4", "sce_sys")
    icon = Image.new("RGB", (512, 512), WHITE)
    mark(icon, 96, 110, 320)
    size = 96
    draw_name(icon, (512 - name_width(font(size), size)) / 2, 452, size, INK)
    icon.save(os.path.join(out, "icon0.png"))

    picture = Image.new("RGB", (1920, 1080), BACKGROUND)
    size = 128
    mark_w = 200
    gap = 30
    text_w = name_width(font(size), size)
    left = 1800 - (mark_w + gap + text_w)
    mark_h = mark_w * MARK_H / MARK_W
    mark(picture, left, 862 - mark_h / 2, mark_w)
    draw_name(picture, left + mark_w + gap, 862 + size * 0.36, size, LIGHT_TEXT)
    picture.save(os.path.join(out, "pic1.png"))

    images = os.path.join(ROOT, "images")
    os.makedirs(images, exist_ok=True)
    logo(LIGHT_TEXT).save(os.path.join(images, "logo-on-dark.png"))
    logo(INK).save(os.path.join(images, "logo-on-light.png"))


if __name__ == "__main__":
    main()
