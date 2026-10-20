# Super Mario Galaxy for the PlayStation 5 - placeholder home-screen art until the
# real pictures are made: a starfield around a small planet, drawn here (nothing
# from the game). Writes art/icon0-source.png and art/pic-source.png.
# SPDX-License-Identifier: GPL-3.0-only
import random, sys, math
from pathlib import Path
from PIL import Image, ImageDraw, ImageFilter

art = Path(__file__).resolve().parent

def scene(width, height, seed):
    random.seed(seed)
    image = Image.new("RGB", (width, height))
    pixels = image.load()
    for y in range(height):
        t = y / height
        row = (int(8 + 20 * t), int(10 + 12 * t), int(40 + 60 * t))
        for x in range(width):
            pixels[x, y] = row
    draw = ImageDraw.Draw(image)
    for _ in range(width * height // 900):
        x, y = random.randrange(width), random.randrange(height)
        r = random.choice((1, 1, 1, 2, 2, 3)) * max(1, width // 1200)
        b = random.randint(150, 255)
        draw.ellipse((x - r, y - r, x + r, y + r), fill=(b, b, min(255, b + 30)))
    glow = Image.new("RGB", (width, height))
    g = ImageDraw.Draw(glow)
    cx, cy, radius = width * 0.68, height * 0.58, min(width, height) * 0.22
    g.ellipse((cx - radius * 1.5, cy - radius * 1.5, cx + radius * 1.5, cy + radius * 1.5), fill=(60, 90, 200))
    glow = glow.filter(ImageFilter.GaussianBlur(radius * 0.5))
    image = Image.blend(image, glow, 0.35)
    draw = ImageDraw.Draw(image)
    draw.ellipse((cx - radius, cy - radius, cx + radius, cy + radius), fill=(70, 150, 90))
    for i in range(6):
        a = random.uniform(0, 2 * math.pi)
        d = random.uniform(0, radius * 0.7)
        s = radius * random.uniform(0.15, 0.3)
        px, py = cx + math.cos(a) * d, cy + math.sin(a) * d
        draw.ellipse((px - s, py - s, px + s, py + s), fill=(90, 175, 105))
    return image

scene(1024, 1024, 7).save(art / "icon0-source.png")
scene(3840, 2160, 11).save(art / "pic-source.png")
