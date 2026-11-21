"""Live view mockups (DESIGN.md section 5), 1920x1080 at 100% scale, u = 1.5."""
from lib import *

W, H = 1920, 1080
U = round(min(W / 1280.0, H / 720.0), 3)  # 1.5


def L(v):
    """design px at u=1 -> logical px (DESIGN 5.1: round(value*u))."""
    return r(v * U)


def wrap_words(text, k, s, wbox):
    words = text.split(' ')
    lines, cur = [], ''
    for w in words:
        t = w if not cur else cur + ' ' + w
        if adv(t, k, s) <= wbox:
            cur = t
        else:
            if not cur:
                return None
            lines.append(cur)
            cur = w
    lines.append(cur)
    return lines


def wrap_any(text, k, s, wbox):
    lines, cur = [], ''
    for ch in text:
        if adv(cur + ch, k, s) <= wbox:
            cur += ch
        else:
            lines.append(cur)
            cur = ch
    lines.append(cur)
    return lines


def fit_name(text, wbox, hbox, slots):
    """DESIGN 5.3 stages A, B, C. Returns (size_px, lines, stage, size_in_u)."""
    max1, max2 = (52, 44) if slots == 8 else (60, 52)
    for d in range(max1, 35, -2):
        s = L(d)
        if adv(text, 'ab', s) <= wbox:
            return s, [text], 'A', d
    for d in range(max2, 27, -2):
        s = L(d)
        ls = wrap_words(text, 'ab', s, wbox)
        if ls is None and d == 28:
            ls = wrap_any(text, 'ab', s, wbox)
        if ls and len(ls) <= 2 and len(ls) * 1.1 * s <= hbox:
            return s, ls, 'B', d
    for d in range(26, 13, -2):
        s = L(d)
        ls = wrap_any(text, 'ab', s, wbox)
        if len(ls) <= 2 and len(ls) * 1.1 * s <= hbox:
            return s, ls, 'C', d
    raise RuntimeError('no fit')


def fit_sound(text, wbox):
    for d in range(20, 13, -2):
        s = L(d)
        if adv(text, 'ar', s) <= wbox:
            return s
    return L(14)


def button(s, x, y, w, h, slot, n, st, slots, log):
    """st: normal | lit | next | empty. slot: dict(label, kind, addr, name) or None."""
    rad = L(14)
    pad = L(16)
    b_norm, b_lit, b_next, b_empty = L(2), L(6), L(3), L(2)
    badge = L(44)
    tagc = L(22)
    dash = (L(8), L(6))
    # fill and border
    if st == 'lit':
        s.rect(x, y, w, h, fill=C['cyan'], rx=rad)
        s.rect(x, y, w, h, stroke=C['text'], sw=b_lit, rx=rad)
    elif st == 'next':
        s.rect(x, y, w, h, fill=C['surface'], rx=rad)
        s.rect(x, y, w, h, stroke=C['strip'], sw=b_next, rx=rad)
    elif st == 'empty':
        s.rect(x, y, w, h, stroke=C['line'], sw=b_empty, rx=rad, dash=dash)
    else:
        s.rect(x, y, w, h, fill=C['surface'], rx=rad)
        s.rect(x, y, w, h, stroke=C['line_strong'], sw=b_norm, rx=rad)

    ix, iy, iw = x + pad, y + pad, w - 2 * pad
    # badge
    if st == 'lit':
        s.rect(ix, iy, badge, badge, fill=C['on_cyan'], rx=L(8))
    elif st == 'empty':
        s.rect(ix, iy, badge, badge, stroke=C['line'], sw=L(2), rx=L(8), dash=dash)
    else:
        s.rect(ix, iy, badge, badge, stroke=C['line_strong'], sw=L(2), rx=L(8))
    s.text(ix + badge / 2.0, iy + badge / 2.0, str(n), 'bs', L(28),
           C['text_empty'] if st == 'empty' else C['text'], anchor='middle')

    if st == 'empty':
        s.text(x + w / 2.0, y + h / 2.0, 'EMPTY', 'bs', tagc, C['text_empty'], anchor='middle', trk=L(2))
        return

    fg = C['on_cyan'] if st == 'lit' else C['text']
    fg2 = C['on_cyan'] if st == 'lit' else C['text_dim']

    # tag
    rowc = iy + badge / 2.0
    gap = L(8)
    if st in ('lit', 'next'):
        word = 'NOW' if st == 'lit' else 'NEXT'
        col = C['on_cyan'] if st == 'lit' else C['cyan_text']
        right = ix + iw
        s.text(right, rowc, word, 'bs', tagc, col, anchor='end', trk=L(2))
        tw = adv(word, 'bs', tagc, L(2)) - L(2)
        tx = right - tw - gap
        if st == 'lit':
            tw_, th_ = L(14), L(16)
            s.poly([(tx - tw_, rowc - th_ / 2.0), (tx, rowc), (tx - tw_, rowc + th_ / 2.0)], fill=col)
        else:
            tw_, th_ = L(10), L(16)
            s.poly([(tx - tw_, rowc - th_ / 2.0), (tx - 0.5, rowc), (tx - tw_, rowc + th_ / 2.0)],
                   stroke=col, sw=L(3), close=False, round_=False)

    # name box
    nby = iy + badge + L(8)
    nbh = h - 2 * pad - badge - L(8) - L(8) - L(48)
    big = slot['label'] or slot['name']
    size, lines, stage, d = fit_name(big, iw, nbh, slots)
    log.append('slot %d %-18s stage %s %2du = %2dpx, %d line(s), box %dx%d'
               % (n, big, stage, d, size, len(lines), iw, nbh))
    cy = nby + nbh / 2.0
    lh = 1.1 * size
    for i, ln in enumerate(lines):
        s.text(ix + iw / 2.0, cy + (i - (len(lines) - 1) / 2.0) * lh, ln, 'ab', size, fg, anchor='middle')

    # bottom block (48u high, bottom aligned)
    bot = y + h - pad
    addr = '%s %s' % (slot['kind'], slot['addr'])
    asz = L(22)
    if slot['label']:
        ssz = fit_sound(slot['name'], iw)
        s.text(ix, bot - 1.1 * ssz / 2.0, slot['name'], 'ar', ssz, fg2)
        s.text(ix, bot - 1.1 * ssz - asz / 2.0, addr, 'bs', asz, fg2, trk=L(1))
    else:
        s.text(ix, bot - asz / 2.0, addr, 'bs', asz, fg2, trk=L(1))


def top_bar(s, setname, mode, addr, name, notinset, connected, log):
    bh = L(56)
    s.rect(0, 0, W, bh, fill=C['bar'])
    s.rect(0, bh - L(2), W, L(2), fill=C['line'])
    cy = (bh - L(2)) / 2.0
    pad = L(24)
    ssz = L(24)
    s.text(pad, cy, setname, 'ab', ssz, C['text'])
    x = pad + adv(setname, 'ab', ssz) + L(24)
    # mode chip
    stat = L(22)
    chip_w = adv(mode, 'bs', stat, L(1)) - L(1) + 2 * L(10)
    chip_h = L(32)
    s.rect(x, cy - chip_h / 2.0, chip_w, chip_h, stroke=C['text_dim'], sw=L(2), rx=L(6))
    s.text(x + L(10), cy, mode, 'bs', stat, C['text_dim'], trk=L(1))
    x += chip_w + L(12)
    if addr:
        s.text(x, cy, addr, 'bs', L(22), C['text_dim'], trk=L(1))
        x += adv(addr, 'bs', L(22), L(1)) + L(12)
        s.text(x, cy, name, 'ab', ssz, C['text'])
        x += adv(name, 'ab', ssz) + L(16)
        if notinset:
            tsz = L(18)
            tw = adv('NOT IN SET', 'bs', tsz, L(2)) - L(2) + 2 * L(10)
            s.rect(x, cy - chip_h / 2.0, tw, chip_h, stroke=C['cyan_text'], sw=L(2), rx=L(6))
            s.text(x + L(10), cy, 'NOT IN SET', 'bs', tsz, C['cyan_text'], trk=L(2))
    # right zone
    label = 'X50 CONNECTED' if connected else 'NO X50'
    col = C['text'] if connected else C['red']
    right = W - pad
    s.text(right, cy, label, 'bs', stat, col, anchor='end', trk=L(1))
    tw = adv(label, 'bs', stat, L(1)) - L(1)
    led(s, right - tw - L(12) - L(16) / 2.0, cy, L(16), connected, L(3), C['cyan'], C['red'])


def hint(s):
    sz = L(18)
    trk = L(0.5)
    cy = H - L(32) / 2.0 - 0
    sp = adv(' ', 'bm', sz, trk)
    a, b = 'Hold', 'for 1 second, or press Esc twice, to leave live view'
    capw = adv('Esc', 'bm', sz, trk) - trk + 2 * L(6)
    caph = L(24)
    total = adv(a, 'bm', sz, trk) + sp + capw + sp + adv(b, 'bm', sz, trk)
    x = (W - total) / 2.0
    s.text(x, cy, a, 'bm', sz, C['text_dim'], trk=trk)
    x += adv(a, 'bm', sz, trk) + sp
    s.rect(x, cy - caph / 2.0, capw, caph, stroke=C['text_dim'], sw=L(2), rx=L(4))
    s.text(x + L(6), cy, 'Esc', 'bm', sz, C['text'], trk=trk)
    x += capw + sp
    s.text(x, cy, b, 'bm', sz, C['text_dim'], trk=trk)


def grid(slots_n):
    m, g = L(24), L(20)
    top = L(56) + L(20)
    area_h = H - L(56) - L(20) - L(20) - L(32)
    rowh = (area_h - g) // 2
    cols = 4 if slots_n == 8 else 3
    cw = (W - 2 * m - (cols - 1) * g) / float(cols)
    cell_w = r(cw)
    xs, ws = [], []
    x = m
    for c in range(cols):
        w = cell_w if c < cols - 1 else W - m - x
        xs.append(x)
        ws.append(w)
        x += w + g
    ys = [top, top + rowh + g]
    hs = [rowh, area_h - rowh - g]
    return cols, xs, ws, ys, hs


def render(path, slots, lit, setname, mode, addr, name, notinset, connected, log):
    n = len(slots)
    s = Svg(W, H, C['bg_live'])
    top_bar(s, setname, mode, addr, name, notinset, connected, log)
    cols, xs, ws, ys, hs = grid(n)
    # next-up: next non-empty slot after the lit one, no wrap
    nxt = None
    if lit is not None:
        for j in range(lit + 1, n):
            if slots[j]:
                nxt = j
                break
    for i, sl in enumerate(slots):
        c, rw = i % cols, i // cols
        st = 'empty' if not sl else ('lit' if i == lit else ('next' if i == nxt else 'normal'))
        button(s, xs[c], ys[rw], ws[c], hs[rw], sl, i + 1, st, n, log) if sl else \
            button(s, xs[c], ys[rw], ws[c], hs[rw], None, i + 1, 'empty', n, log)
    hint(s)
    s.save(path)


def sl(label, kind, addr, name):
    return dict(label=label, kind=kind, addr=addr, name=name)


SET8 = [
    sl('Intro pad', 'COMBI', 'A002', 'Vintage Pad'),
    sl('Verse EP', 'PROG', 'D104', 'Classic E.Piano'),
    sl(None, 'COMBI', 'C096', 'Piano &Strings'),
    sl(None, 'COMBI', 'A000', 'StereophonicKeys'),
    sl('Bridge organ', 'COMBI', 'A015', 'Heavy Organ'),
    None,
    sl('Solo lead', 'PROG', 'A028', 'Mono Guitar Lead'),
    sl(None, 'COMBI', 'A026', 'Waiting4Sunrise'),
]

SET6 = [
    sl(None, 'COMBI', 'B064', 'Layered A.Piano'),
    sl('Warm pad', 'PROG', 'A025', 'Overture Pad'),
    sl('Ballad piano', 'PROG', 'B001', 'Acoustic Piano'),
    sl(None, 'COMBI', 'A017', 'Detailed Strings'),
    sl('Harp intro', 'PROG', 'A092', 'Harp'),
    sl(None, 'COMBI', 'C051', 'Session Piano'),
]


def build(outdir):
    log = []
    render(outdir + '/live-8.svg', SET8, 1, 'Friday Set 2', 'PROG', 'D104', 'Classic E.Piano',
           False, True, log)
    log.append('---')
    render(outdir + '/live-6.svg', SET6, None, 'Acoustic Night', 'PROG', 'D001', 'PianoMezzoDark',
           True, True, log)
    return log
