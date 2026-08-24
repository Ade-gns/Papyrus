#!/usr/bin/env python3
"""Regenerates packaging/icons/papyrus-*.png (a simple scroll icon).

Run manually if the icon needs to change; the PNGs it produces are checked
into git like any other asset, so this script isn't part of the build.
"""

import os

from PIL import Image, ImageDraw

SIZES = (48, 128, 256)
OUT_DIR = os.path.join(os.path.dirname(__file__), "icons")

PARCHMENT = (233, 213, 166, 255)
PARCHMENT_SHADOW = (196, 168, 115, 255)
INK = (92, 62, 34, 255)
ROD = (139, 94, 46, 255)


def draw_icon(size: int) -> Image.Image:
    scale = 4  # supersample then downscale for smoother edges
    s = size * scale
    img = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)

    margin = s * 0.12
    body = [margin, s * 0.22, s - margin, s * 0.78]
    rod_h = s * 0.1

    # Scroll body.
    draw.rounded_rectangle(body, radius=s * 0.04, fill=PARCHMENT, outline=PARCHMENT_SHADOW, width=max(1, int(s * 0.01)))

    # Top and bottom rods (rolled parchment ends).
    draw.rounded_rectangle([margin - s * 0.03, body[1] - rod_h / 2, s - margin + s * 0.03, body[1] + rod_h / 2],
                            radius=rod_h / 2, fill=ROD)
    draw.rounded_rectangle([margin - s * 0.03, body[3] - rod_h / 2, s - margin + s * 0.03, body[3] + rod_h / 2],
                            radius=rod_h / 2, fill=ROD)

    # A few ink lines suggesting text.
    line_left = body[0] + s * 0.14
    line_right = body[2] - s * 0.14
    line_w = max(1, int(s * 0.025))
    for i in range(4):
        y = body[1] + s * 0.16 + i * s * 0.1
        right = line_right if i != 3 else line_left + (line_right - line_left) * 0.6
        draw.line([(line_left, y), (right, y)], fill=INK, width=line_w)

    return img.resize((size, size), Image.LANCZOS)


def main() -> None:
    os.makedirs(OUT_DIR, exist_ok=True)
    for size in SIZES:
        icon = draw_icon(size)
        path = os.path.join(OUT_DIR, f"papyrus-{size}.png")
        icon.save(path)
        print("wrote", path)


if __name__ == "__main__":
    main()
