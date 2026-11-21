"""Prints the DESIGN 5.3 / 5.7 name-fit table for Pixel Operator X50 Bold.
Usage: python3 fit_table.py   (needs Pillow; reads assets/fonts)"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import live
from lib import r

NAMES = ['Solo lead', 'Piano &Strings', 'Bridge organ', 'Layered A.Piano',
         'Session Piano', 'Detailed Strings', 'StereophonicKeys', 'Waiting4Sunrise',
         'WWWWWWWWWWWWWWWW']


def boxes(Wd, Hd, dpr, slots):
    W, H = Wd / dpr, Hd / dpr
    u = round(min(W / 1280.0, H / 720.0), 3)
    L = lambda v: r(v * u)
    m, g = L(24), L(20)
    area = H - L(56) - L(20) - L(20) - L(32)
    rowh = (int(area) - g) // 2
    cols = 4 if slots == 8 else 3
    cw = r((W - 2 * m - (cols - 1) * g) / float(cols))
    iw = cw - 2 * L(16)
    nbh = rowh - 2 * L(16) - L(44) - L(8) - L(8) - L(48)
    return u, r(iw * dpr), r(nbh * dpr)


if __name__ == '__main__':
    for slots in (8, 6):
        for (Wd, Hd) in ((1920, 1080), (1366, 768)):
            for dpr in (1.0, 1.25, 1.5, 2.0):
                u, wb, hb = boxes(Wd, Hd, dpr, slots)
                U = u * dpr
                row = []
                for n in NAMES:
                    k, ls, rule = live.fit_pixel(n, wb, hb, U)
                    row.append('%s k%d/%dL' % (n[:6], k, len(ls)))
                print('%d slots %dx%d @%.2f u=%.3f U=%.3f cap=%d box %dx%d | %s'
                      % (slots, Wd, Hd, dpr, u, U, live.px_cap(U), wb, hb, '; '.join(row)))
