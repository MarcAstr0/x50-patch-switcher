"""Generate the four mockup SVGs and render PNGs with Inkscape.

Usage: python3 build.py [--no-render]
Fonts: a throwaway fontconfig file points at assets/fonts so the committed files are
used and nothing is installed system-wide.
"""
import os, subprocess, sys, tempfile
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import live
from lib import ROOT

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.abspath(os.path.join(HERE, '..'))


def render(name):
    conf = os.path.join(tempfile.mkdtemp(), 'fonts.conf')
    with open(conf, 'w') as f:
        f.write('<?xml version="1.0"?><!DOCTYPE fontconfig SYSTEM "fonts.dtd"><fontconfig>'
                '<dir>%s/assets/fonts</dir><include ignore_missing="yes">/etc/fonts/fonts.conf</include>'
                '</fontconfig>' % ROOT)
    env = dict(os.environ, FONTCONFIG_FILE=conf)
    subprocess.check_call(['inkscape', os.path.join(HERE, name + '.svg'), '--export-type=png',
                           '--export-dpi=96', '--export-filename=' + os.path.join(OUT, 'mockup-' + name + '.png')],
                          env=env)


if __name__ == '__main__':
    log = live.build(HERE)
    try:
        import browse, settings
        log += browse.build(HERE) + settings.build(HERE)
    except ImportError:
        pass
    print('\n'.join(log))
    if '--no-render' not in sys.argv:
        for n in ('live-8', 'live-6', 'browse', 'settings'):
            if os.path.exists(os.path.join(HERE, n + '.svg')):
                render(n)
