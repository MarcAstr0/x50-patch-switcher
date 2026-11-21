"""LCD panels in the Pixel Operator face (DESIGN 2.2, 5.5, 7.2). Everything is in device
pixels at 1x; `k` is device pixels per font pixel. Pixel Operator is drawn at 16*k px
(small rows) and 32*k px (the sound bar), so every glyph pixel is a k x k square."""
from lib import *

# ink extents of the face, in font pixels: cap height 9, descender 3, baseline at row 13 of 16
CAP, DESC = 9, 3


def base_in(top, h, k):
    """Baseline y (device px, integer) that centres cap-top..descender-bottom in a box."""
    return top + r((h - (CAP + DESC) * k) / 2.0) + CAP * k


def mode_texts(kind):
    if kind == 'COMBI':
        return 'COMBI 0:Play', 'Combination', ':Combi Select'
    return 'PROG 0:Play', 'Program', ':Program Select'


def sound_text(addr, with_bank):
    """addr like 'C096' or 'G001'. Browse bar: number only; live bar: bank letter + number."""
    return (addr[1:] if not with_bank else addr)


def strip_panel(s, ox, oy, k, kind, addr, name, cat_idx, cat_name, log=None):
    """Browse display strip LCD panel, 480k x 74k. kind None = unknown sound."""
    W_, H_ = 480 * k, 74 * k
    b = 2 * k
    s.rect(ox, oy, W_, H_, fill=C['lcd_bezel'], rx=4 * k)
    s.rect(ox + b, oy + b, W_ - 2 * b, H_ - 2 * b, fill=C['lcd_field'], rx=2 * k)
    fx, fy, fw = ox + b, oy + b, W_ - 2 * b
    fs = 16 * k
    if kind is None:
        s.rect(fx + 4 * k, fy + 3 * k, adv('--', 'pb', fs) + 8 * k, 16 * k, fill=C['lcd_ink'])
        s.text(fx + 8 * k, fy + 14 * k, '--', 'pb', fs, C['lcd_field'], base=True)
        bt = fy + 37 * k
        s.rect(fx + 4 * k, bt, fw - 8 * k, 30 * k, stroke=C['lcd_ink'], sw=k)
        s.text(fx + 10 * k, bt + 21 * k, '--', 'po', 2 * fs, C['lcd_ink'], base=True)
        return
    mode, pg, par = mode_texts(kind)
    wl = adv(mode, 'pb', fs)
    s.rect(fx + 4 * k, fy + 3 * k, wl + 8 * k, 16 * k, fill=C['lcd_ink'])
    s.text(fx + 8 * k, fy + 14 * k, mode, 'pb', fs, C['lcd_field'], base=True)
    w1, w2 = adv(pg, 'pb', fs), adv(par, 'po', fs)
    wr = w1 + w2
    rx_ = fx + fw - 4 * k - (wr + 8 * k)
    s.rect(rx_, fy + 3 * k, wr + 8 * k, 16 * k, fill=C['lcd_ink'])
    s.text(rx_ + 4 * k, fy + 14 * k, pg, 'pb', fs, C['lcd_field'], base=True)
    s.text(rx_ + 4 * k + w1, fy + 14 * k, par, 'po', fs, C['lcd_field'], base=True)
    bank = 'Bank ' + addr[0]
    s.text(fx + 8 * k, fy + 32 * k, bank, 'pb', fs, C['lcd_ink'], base=True)
    s.text(fx + 160 * k, fy + 32 * k, 'Category %02d:%s' % (cat_idx, cat_name), 'po', fs, C['lcd_ink'], base=True)
    bt = fy + 37 * k
    s.rect(fx + 4 * k, bt, fw - 8 * k, 30 * k, fill=C['lcd_ink'])
    s.text(fx + 10 * k, bt + 21 * k, '%s: %s' % (addr[1:], name), 'po', 2 * fs, C['lcd_field'], base=True)
    if log is not None:
        log.append('strip bar text width %.0f of %d' % (adv('%s: %s' % (addr[1:], name), 'po', 2 * fs), fw - 8 * k - 12 * k))


def live_panel(s, x, cy, u, kind, addr, name, log=None):
    """Live top-bar readout. u in device px per design px (U = u * scale). Returns width."""
    kb = max(1, r(1.8 * u))
    km = max(1, r(1.2 * u))
    ph, bz, inset, sh = r(40 * u), r(2 * u), r(4 * u), r(32 * u)
    py = r(cy - ph / 2.0)
    sy = py + (ph - sh) // 2
    if kind is None:
        mode_t, bar_t = '--', '--'
    else:
        mode_t, bar_t = mode_texts(kind)[0], '%s: %s' % (addr, name)
    mw = adv(mode_t, 'pb', 16 * km) + 2 * r(8 * u)
    bfont = 'po' if kind is None else 'pb'
    bw = adv(bar_t, bfont, 16 * kb) + 2 * r(10 * u)
    gap = r(6 * u)
    pw = bz + inset + mw + gap + bw + inset + bz
    s.rect(x, py, pw, ph, fill=C['lcd_bezel'], rx=r(6 * u))
    s.rect(x + bz, py + bz, pw - 2 * bz, ph - 2 * bz, fill=C['lcd_field_dim'], rx=r(4 * u))
    mx = x + bz + inset
    s.rect(mx, sy, mw, sh, fill=C['lcd_ink'])
    s.text(mx + r(8 * u), base_in(sy, sh, km), mode_t, 'pb', 16 * km, C['lcd_field_dim'], base=True)
    bx = mx + mw + gap
    if kind is None:
        o = max(1, r(2 * u))
        s.rect(bx, sy, bw, sh, stroke=C['lcd_ink'], sw=o)
        s.text(bx + r(10 * u), base_in(sy, sh, kb), bar_t, 'po', 16 * kb, C['lcd_ink'], base=True)
    else:
        s.rect(bx, sy, bw, sh, fill=C['lcd_ink'])
        s.text(bx + r(10 * u), base_in(sy, sh, kb), bar_t, 'pb', 16 * kb, C['lcd_field_dim'], base=True)
    if log is not None:
        log.append('live panel kb=%d km=%d width %d' % (kb, km, pw))
    return pw
