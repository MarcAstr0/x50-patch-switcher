#!/usr/bin/env python3
"""Build assets/fonts/PixelOperatorX50.ttf and PixelOperatorX50-Bold.ttf from upstream
Pixel Operator (CC0 1.0, Jayvee Enaguas / HarvettFox96).

Upstream source (this script downloads nothing): zip from
https://www.dafont.com/pixel-operator.font (direct: https://dl.dafont.com/dl/?f=pixel_operator),
upstream repository https://notabug.org/HarvettFox96/ttf-pixeloperator. Files needed:
PixelOperator.ttf and PixelOperator-Bold.ttf.

Changes (everything else, metrics, grid and all other glyphs, is untouched):
  * zero: dotted zero, a free-standing centre mark inside a plain oval (replaces the
    slashed zero, which read as 8 at small sizes)
  * ampersand: redrawn as a classic pixel ampersand (upstream read as 6)
  * name table: family "Pixel Operator X50" so it is never mistaken for upstream

Usage (needs fonttools and skia-pathops, install into a venv, not system-wide):
  python3 patch_font.py UPSTREAM_DIR OUT_DIR
  e.g. python3 patch_font.py ~/dl/pixel_operator assets/fonts
"""
import os
import sys
import pathops
from fontTools.ttLib import TTFont
from fontTools.pens.ttGlyphPen import TTGlyphPen

# One char = one font pixel (100 units). Column 0 is x = 0, the bottom row is y = 0.
# Rows are listed from the top (y 800..900) to the bottom (y 0..100).
REG = {
    'zero': [
        '..XXX...',
        '.X...X..',
        '.X...X..',
        '.X.X.X..',
        '.X.X.X..',
        '.X.X.X..',
        '.X...X..',
        '.X...X..',
        '..XXX...',
    ],
    'ampersand': [
        '..XX....',
        '.X..X...',
        '.X..X...',
        '.X.X....',
        '..X.....',
        '.X.X.X..',
        '.X..XX..',
        '.X..X...',
        '..XX.X..',
    ],
}
BOLD = {
    'zero': [
        '..XXXX..',
        '.XX..XX.',
        '.XX..XX.',
        '.X.XX.X.',
        '.X.XX.X.',
        '.X.XX.X.',
        '.XX..XX.',
        '.XX..XX.',
        '..XXXX..',
    ],
    'ampersand': [
        '..XXX...',
        '.XX.XX..',
        '.XX.XX..',
        '..XXX...',
        '..XXX.XX',
        '.XX.XXX.',
        '.XX.XX..',
        '.XX.XXX.',
        '..XXX.XX',
    ],
}


def outline(rows):
    path = pathops.Path()
    pen = path.getPen()
    n = len(rows)
    for i, row in enumerate(rows):
        y0 = (n - 1 - i) * 100
        for x, ch in enumerate(row):
            if ch == 'X':
                x0 = x * 100
                pen.moveTo((x0, y0))
                pen.lineTo((x0 + 100, y0))
                pen.lineTo((x0 + 100, y0 + 100))
                pen.lineTo((x0, y0 + 100))
                pen.closePath()
    path.simplify(fix_winding=True)
    tt = TTGlyphPen(None)
    path.draw(tt)
    return tt.glyph()


def patch(src, dst, glyphs, style):
    f = TTFont(src)
    gl = f['glyf']
    for name, rows in glyphs.items():
        adv, _ = f['hmtx'][name]
        assert max(len(r) for r in rows) * 100 <= adv + 100, name
        gl[name] = outline(rows)
        gl[name].recalcBounds(gl)
        f['hmtx'][name] = (adv, gl[name].xMin)
    fam = 'Pixel Operator X50'
    full = fam + (' Bold' if style == 'Bold' else '')
    ps = 'PixelOperatorX50' + ('-Bold' if style == 'Bold' else '')
    for rec in f['name'].names:
        if rec.nameID == 1:
            rec.string = fam
        elif rec.nameID == 4:
            rec.string = full
        elif rec.nameID == 6:
            rec.string = ps
        elif rec.nameID == 3:
            rec.string = ps + ':X50 patch of Pixel Operator 2018.10.04-1'
    f.save(dst)


if __name__ == '__main__':
    up, out = sys.argv[1], sys.argv[2]
    patch(os.path.join(up, 'PixelOperator.ttf'), os.path.join(out, 'PixelOperatorX50.ttf'), REG, 'Regular')
    patch(os.path.join(up, 'PixelOperator-Bold.ttf'), os.path.join(out, 'PixelOperatorX50-Bold.ttf'), BOLD, 'Bold')
