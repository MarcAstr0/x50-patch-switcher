"""Shared helpers for the mockup generators. Values come from docs/design/DESIGN.md.

Text is measured with the committed font files (Pillow + raqm) so the fit rule of
DESIGN 5.3 is run for real, not guessed. Output is plain SVG; build.py renders it.
"""
import math
import os
from PIL import ImageFont, features

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
FD = os.path.join(ROOT, 'assets', 'fonts')

FILES = {
    'ar': ('AtkinsonHyperlegible-Regular.ttf', 'Atkinson Hyperlegible', 400),
    'ab': ('AtkinsonHyperlegible-Bold.ttf', 'Atkinson Hyperlegible', 700),
    'bm': ('BarlowSemiCondensed-Medium.ttf', 'Barlow Semi Condensed', 500),
    'bs': ('BarlowSemiCondensed-SemiBold.ttf', 'Barlow Semi Condensed', 600),
    'po': ('PixelOperator.ttf', 'Pixel Operator', 400),
    'pb': ('PixelOperator-Bold.ttf', 'Pixel Operator', 700),
}

C = dict(
    bg_live='#0B0D0F', bar='#12171B', surface='#1A1F24', surface_pressed='#2A333A',
    line='#2A3238', line_strong='#5F6C76', text='#F2F5F7', text_dim='#AEB8C2',
    text_empty='#7E8A94', strip='#B2BBCA', cyan='#5DB0D0', cyan_text='#7CC8E4',
    on_cyan='#04141B', red='#FF8080', on_red='#04141B', amber='#F0B65A', green='#7BD39A',
    bg_browse='#121518', panel='#1B2024', panel_raised='#232A30', field='#101417',
    sel_row='#1E4A5C', border='#46525A', disabled_text='#6B7780', placeholder='#8A97A2',
    lcd_field='#E6E8E1', lcd_field_dim='#BFC3BA', lcd_ink='#111411', lcd_bezel='#3A413B',
    lcd_warn='#8A1414',
)

_fc = {}
_layout = ImageFont.Layout.RAQM if features.check('raqm') else ImageFont.Layout.BASIC


def _font(k):
    if k not in _fc:
        _fc[k] = ImageFont.truetype(os.path.join(FD, FILES[k][0]), 1000, layout_engine=_layout)
    return _fc[k]


def adv(text, k, size, trk=0.0):
    """Advance of text in px at the given pixel size and absolute letter spacing."""
    return _font(k).getlength(text) * size / 1000.0 + trk * len(text)


def asc_desc(k):
    a, d = _font(k).getmetrics()
    return a / 1000.0, d / 1000.0


def r(x):
    """Round half up (device pixels at 1x)."""
    return int(math.floor(x + 0.5))


def esc(s):
    return s.replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;')


def fmt(v):
    if isinstance(v, float):
        s = ('%.2f' % v).rstrip('0').rstrip('.')
        return s
    return str(v)


class Svg:
    def __init__(self, w, h, bg):
        self.w, self.h = w, h
        self.p = []
        self.defs = []
        self._clip = 0
        self.rect(0, 0, w, h, fill=bg)

    # primitives -------------------------------------------------------
    def rect(self, x, y, w, h, fill=None, stroke=None, sw=0, rx=0, dash=None, inner=True):
        """Rect; the stroke is drawn INSIDE the given box (DESIGN 3.5)."""
        if stroke and sw:
            if inner:
                x, y, w, h = x + sw / 2.0, y + sw / 2.0, w - sw, h - sw
                rx = max(0, rx - sw / 2.0)
        a = 'x="%s" y="%s" width="%s" height="%s"' % (fmt(x), fmt(y), fmt(w), fmt(h))
        if rx:
            a += ' rx="%s"' % fmt(rx)
        a += ' fill="%s"' % (fill or 'none')
        if stroke and sw:
            a += ' stroke="%s" stroke-width="%s"' % (stroke, fmt(sw))
            if dash:
                a += ' stroke-dasharray="%s"' % ' '.join(fmt(d) for d in dash)
        self.p.append('<rect %s/>' % a)

    def circle(self, cx, cy, rad, fill=None, stroke=None, sw=0):
        a = 'cx="%s" cy="%s" r="%s" fill="%s"' % (fmt(cx), fmt(cy), fmt(rad), fill or 'none')
        if stroke:
            a += ' stroke="%s" stroke-width="%s"' % (stroke, fmt(sw))
        self.p.append('<circle %s/>' % a)

    def poly(self, pts, fill=None, stroke=None, sw=0, close=True, round_=False):
        d = 'M' + ' L'.join('%s %s' % (fmt(x), fmt(y)) for x, y in pts)
        if close:
            d += ' Z'
        self.path(d, fill, stroke, sw, round_)

    def path(self, d, fill=None, stroke=None, sw=0, round_=False):
        a = 'd="%s" fill="%s"' % (d, fill or 'none')
        if stroke:
            a += ' stroke="%s" stroke-width="%s"' % (stroke, fmt(sw))
            if round_:
                a += ' stroke-linecap="round" stroke-linejoin="round"'
        self.p.append('<path %s/>' % a)

    def line(self, x1, y1, x2, y2, stroke, sw, cap='butt'):
        self.p.append('<line x1="%s" y1="%s" x2="%s" y2="%s" stroke="%s" stroke-width="%s" stroke-linecap="%s"/>'
                      % (fmt(x1), fmt(y1), fmt(x2), fmt(y2), stroke, fmt(sw), cap))

    def text(self, x, y, s, k, size, fill, anchor='start', trk=0.0, base=False):
        """y is the vertical CENTRE of the line box (Qt AlignVCenter) unless base=True."""
        if not base:
            a, d = asc_desc(k)
            y = y + (a - d) / 2.0 * size
        fam, wt = FILES[k][1], FILES[k][2]
        if anchor == 'end':
            x += trk
        elif anchor == 'middle':
            x += trk / 2.0
        st = "font-family:'%s';font-weight:%d;font-size:%spx;fill:%s;" % (fam, wt, fmt(float(size)), fill)
        if trk:
            st += 'letter-spacing:%spx;' % fmt(float(trk))
        self.p.append('<text x="%s" y="%s" text-anchor="%s" style="%s">%s</text>'
                      % (fmt(x), fmt(y), anchor, st, esc(s)))

    def clip_start(self, x, y, w, h, rx=0):
        self._clip += 1
        cid = 'c%d' % self._clip
        self.defs.append('<clipPath id="%s"><rect x="%s" y="%s" width="%s" height="%s" rx="%s"/></clipPath>'
                         % (cid, fmt(x), fmt(y), fmt(w), fmt(h), fmt(rx)))
        self.p.append('<g clip-path="url(#%s)">' % cid)

    def clip_end(self):
        self.p.append('</g>')

    def comment(self, s):
        self.p.append('<!-- %s -->' % s.replace('--', '-'))

    def save(self, path):
        out = ['<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" viewBox="0 0 %d %d">'
               % (self.w, self.h, self.w, self.h)]
        if self.defs:
            out.append('<defs>%s</defs>' % ''.join(self.defs))
        out += self.p
        out.append('</svg>')
        with open(path, 'w') as f:
            f.write('\n'.join(out) + '\n')


# shared painted glyphs ---------------------------------------------------
def star(s, cx, cy, rad, fill=None, stroke=None, sw=0):
    pts = []
    for i in range(10):
        ang = -math.pi / 2 + i * math.pi / 5
        rr = rad if i % 2 == 0 else rad * 0.45
        pts.append((cx + rr * math.cos(ang), cy + rr * math.sin(ang)))
    s.poly(pts, fill=fill, stroke=stroke, sw=sw, round_=True)


def led(s, cx, cy, diam, ok, ring_w, color_ok, color_bad):
    if ok:
        s.circle(cx, cy, diam / 2.0, fill=color_ok)
    else:
        rad = diam / 2.0 - ring_w / 2.0
        s.circle(cx, cy, rad, stroke=color_bad, sw=ring_w)
        d = rad * 0.7071
        s.line(cx - d, cy + d, cx + d, cy - d, color_bad, ring_w)
