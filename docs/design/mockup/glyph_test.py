#!/usr/bin/env python3
"""Render the glyph test strip (Pixel Operator X50 at 16, 32, 48 px) to glyph-test.png.
Usage: python3 glyph_test.py [FONT_DIR] [OUT_PNG]  (needs Pillow)"""
import os, sys
from PIL import Image, ImageDraw, ImageFont
HERE = os.path.dirname(os.path.abspath(__file__))
fd = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..', '..', '..', 'assets', 'fonts')
out = sys.argv[2] if len(sys.argv) > 2 else os.path.join(HERE, 'glyph-test.png')
TXT = '0 8 O D 104 184 D104 D184 0:Play 8:Play & Piano &Strings Bass & PianoPad'
BG, INK = (230, 232, 225), (17, 20, 17)
rows = []
for fn in ('PixelOperatorX50.ttf', 'PixelOperatorX50-Bold.ttf'):
    for px in (16, 32, 48):
        rows.append((fn, px))
W = 2400
H = sum(int(px * 1.6) + 12 for _, px in rows) + 24
im = Image.new('RGB', (W, H), BG)
d = ImageDraw.Draw(im)
y = 12
for fn, px in rows:
    f = ImageFont.truetype(os.path.join(fd, fn), px)
    d.fontmode = 'L'
    d.text((12, y), TXT, font=f, fill=INK)
    y += int(px * 1.6) + 12
im.save(out)
