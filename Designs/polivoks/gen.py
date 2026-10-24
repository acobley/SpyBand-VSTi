#!/usr/bin/env python3
"""Three SpyBand panel concepts in R2Synth's black panel style (the Polivoks
redress, "Studio Worn"), with the 70s sci-fi lettering.

Writes <name>.svg plus <name>.json (knob, selector and screen geometry in
R2Synth's layout-dump shape, for weather.py). Every <text> is later replaced
by stroked paths by rbfont.js.
"""
import json, math, random, sys
from html import escape

W, H, HDR = 1200, 690, 52

INK = '#f4f1e8'       # printing
DIM = '#8f8c85'       # unchosen selector positions, secondary printing
LINE = '#d9d6cc'      # section boxes, ticks
RED = '#e2342b'       # the one thing that is set
CARRIER = '#2fb8ff'   # OLED colours: SpyBand's code, kept on the glass only
MODUL = '#ffb23a'
NOISE = '#c27bff'
OUTC = '#38f0a0'
CAPTION = '#8d96a3'

BANDS = [127, 207, 336, 546, 887, 1440, 2340, 3800, 6180]
BLABEL = ['127', '207', '336', '546', '887', '1.44K', '2.34K', '3.80K', '6.18K']
ENV = [0.45, 0.85, 0.62, 0.30, 0.72, 0.52, 0.42, 0.18, 0.08]
FILES = [('NUMBERS STATION.WAV', True), ('MORSE 20WPM.WAV', True), ('SHORTWAVE VOICE.WAV', False), None, None]
PINS = {(i, i): 1.0 for i in range(9)}
PINS.update({(3, 5): 0.5, (6, 7): 0.5})


def pol(cx, cy, r, deg):
    t = math.radians(deg)
    return cx + r * math.sin(t), cy - r * math.cos(t)


class G:
    def __init__(s, name):
        s.name, s.p = name, []
        s.knobs, s.sels, s.screens, s.sections = [], [], [], []
        s.fp = []

    def add(s, x):
        s.p.append(x)

    def text(s, x, y, msg, size, fill=INK, anchor='start', bold=True, extra=''):
        y = y + size * 0.32
        s.add(f'<text x="{x:.1f}" y="{y:.1f}" font-size="{size}" font-weight="{700 if bold else 400}" '
              f'fill="{fill}" text-anchor="{anchor}" {extra}>{escape(msg)}</text>')

    def rect(s, x0, y0, x1, y1, fill='none', stroke='none', sw=1, rx=0, extra=''):
        s.add(f'<rect x="{x0:.1f}" y="{y0:.1f}" width="{x1-x0:.1f}" height="{y1-y0:.1f}" rx="{rx}" fill="{fill}" '
              f'stroke="{stroke}" stroke-width="{sw}" {extra}/>')

    def line(s, x0, y0, x1, y1, stroke=LINE, sw=1, extra=''):
        s.add(f'<line x1="{x0:.1f}" y1="{y0:.1f}" x2="{x1:.1f}" y2="{y1:.1f}" stroke="{stroke}" stroke-width="{sw}" '
              f'stroke-linecap="round" {extra}/>')

    def circle(s, cx, cy, r, fill='none', stroke='none', sw=1, extra=''):
        s.add(f'<circle cx="{cx:.1f}" cy="{cy:.1f}" r="{r:.1f}" fill="{fill}" stroke="{stroke}" stroke-width="{sw}" {extra}/>')

    # -----------------------------------------------------------------
    def backdrop(s):
        s.rect(0, 0, W, H, fill='#121212')
        s.rect(0, 0, W, HDR, fill='#0c0c0c')
        s.line(0, HDR - 1, W, HDR - 1, '#e9e6dc', 2)
        for x, y in [(8, 8), (W - 8, 8), (8, H - 8), (W - 8, H - 8)]:
            s.circle(x, y, 3.2, fill='#3a3a3a', stroke='#000', sw=0.6)
            s.line(x - 2.2, y - 2.2, x + 2.2, y + 2.2, '#222', 0.8)
        s.add(f'<g id="wm" transform="translate(22,{HDR / 2 - 13:.1f})"></g>')
        s.line(220, 16, 220, 38, '#5a5852', 1)

    def header(s, preset='FACTORY / CLASSIC 9-BAND'):
        # preset window: ‹ [ name ⌄ ] › SAVE
        y0, y1 = 13, 39
        s.button(234, y0, 262, y1, '‹', 13)
        s.rect(270, y0, 540, y1, fill='#050505', stroke='#3c3c3c', sw=1, rx=3)
        s.text(282, 26, preset, 11, INK)
        s.text(528, 26, '⌄', 11, DIM, anchor='end')
        s.button(548, y0, 576, y1, '›', 13)
        s.button(584, y0, 642, y1, 'SAVE', 11)
        # version window, and bypass with its lamp
        s.rect(860, y0, 1060, y1, fill='#0c0c0c', stroke='#5a5852', sw=1.2, rx=3)
        s.circle(876, 26, 4, fill='#38f0a0')
        s.text(888, 26, 'V1.0.1 · AE COBLEY', 11, INK)
        s.rect(1070, y0, 1180, y1, fill='#0c0c0c', stroke='#5a5852', sw=1.2, rx=3)
        s.circle(1086, 26, 4, fill='#3a1512', stroke='#000', sw=0.5)
        s.text(1098, 26, 'BYPASS', 11, INK)

    def button(s, x0, y0, x1, y1, label, size=10, on=False):
        s.rect(x0, y0, x1, y1, fill='#0c0c0c', stroke=LINE if on else '#5a5852', sw=1.2, rx=3)
        s.text((x0 + x1) / 2, (y0 + y1) / 2, label, size, INK if on else '#c9c6be', anchor='middle', bold=on or size > 10)
        if on:
            s.line(x0 + 6, y1 - 3, x1 - 6, y1 - 3, RED, 1.6)

    def section(s, x0, y0, x1, y1, title, sub=None, tw=None):
        s.sections.append(dict(rect=[x0, y0, x1, y1]))
        s.rect(x0, y0, x1, y1, fill='#151515', stroke=LINE, sw=1.2, rx=2)
        tw = tw or (len(title) * 9.0 + 18)
        s.rect(x0 + 10, y0 + 10, x0 + 10 + tw, y0 + 32, fill='#121212', stroke=LINE, sw=1.2, rx=1)
        s.text(x0 + 10 + tw / 2, y0 + 21, title, 13, INK, anchor='middle')
        if sub:
            s.text(x0 + 22 + tw, y0 + 21, sub, 9.5, '#b9b6ae', bold=False)

    def rule(s, x0, x1, y):
        s.line(x0, y, x1, y, '#3a3a3a', 1)

    def knob(s, cx, top, dia, v, readout, label, bipolar=False, scale=True):
        cy = top + dia / 2
        outer = dia / 2
        body = outer - (6 if dia >= 44 else 5)
        ang = -135 + 270 * v
        for i in range(11):
            t = -135 + 27 * i
            lg = i in (0, 5, 10)
            x0, y0 = pol(cx, cy, body + 1.5, t)
            x1, y1 = pol(cx, cy, outer - (0 if lg else 1.8), t)
            s.line(x0, y0, x1, y1, LINE, 1.3 if lg else 0.9)
        if scale and dia >= 44:
            lo, hi = ('-', '+') if bipolar else ('0', '10')
            x, y = pol(cx, cy, outer + 2, -138)
            s.text(x - 2, y + 4, lo, 7.5, LINE, anchor='end', bold=False)
            x, y = pol(cx, cy, outer + 2, 138)
            s.text(x + 2, y + 4, hi, 7.5, LINE, bold=False)
        teeth, pts = 28, []
        for i in range(teeth * 2):
            rr = body if i % 2 == 0 else body - 1.4
            pts.append('%.2f,%.2f' % pol(cx, cy, rr, 360 * i / (teeth * 2)))
        s.add(f'<polygon points="{" ".join(pts)}" fill="#0a0a0a" stroke="#2f2f2f" stroke-width="0.6"/>')
        s.circle(cx, cy, body * 0.78, fill='url(#capgrad)', stroke='#000', sw=0.8)
        x0, y0 = pol(cx, cy, body * 0.15, ang)
        x1, y1 = pol(cx, cy, body - 0.5, ang)
        s.line(x0, y0, x1, y1, INK, 2.2 if dia >= 44 else 1.8)
        t2 = top + dia + 6
        s.text(cx, t2 + 7, readout, 10, INK, anchor='middle', bold=False)
        s.text(cx, t2 + 19.5, label, 9.5, INK, anchor='middle')
        s.knobs.append(dict(kind='knob', rect=[cx - outer, top, cx + outer, top + dia], diameter=dia, label=label))

    def selector(s, cx, cy, names, cur, label=None, angles=None, r=15, lr=26, label_dy=34):
        n = len(names)
        angles = angles or {2: [-45, 45], 3: [-50, 0, 50], 4: [-66, -22, 22, 66]}[n]
        for i, (t, nm) in enumerate(zip(angles, names)):
            act = i == cur
            x0, y0 = pol(cx, cy, r + 2.5, t)
            x1, y1 = pol(cx, cy, r + 8, t)
            s.line(x0, y0, x1, y1, RED if act else LINE, 2 if act else 1.6)
            lx, ly = pol(cx, cy, lr, t)
            mid = abs(t) < 30
            anchor = 'middle' if mid else ('end' if t < 0 else 'start')
            dx = 0 if mid else (-2 if t < 0 else 2)
            dy = -8 if mid else 2
            s.text(lx + dx, ly + dy, nm, 9.5, INK if act else DIM, anchor=anchor)
        ang = angles[cur]
        s.circle(cx, cy, r * 0.8, fill='#0b0b0b', stroke='#444', sw=0.8)
        tip = pol(cx, cy, r + 3, ang)
        l, rg = pol(cx, cy, 8, ang - 90), pol(cx, cy, 8, ang + 90)
        tail = pol(cx, cy, r, ang + 180)
        s.add(f'<path d="M{tip[0]:.1f},{tip[1]:.1f} L{rg[0]:.1f},{rg[1]:.1f} Q{tail[0]:.1f},{tail[1]:.1f} '
              f'{l[0]:.1f},{l[1]:.1f} Z" fill="url(#capgrad)" stroke="#000" stroke-width="0.8"/>')
        x0, y0 = pol(cx, cy, 2, ang)
        s.line(x0, y0, tip[0], tip[1], INK, 1.8)
        if label:
            s.text(cx, cy + label_dy, label, 9.5, INK, anchor='middle')
        s.sels.append(dict(kind='switch', label=label or '', selector=dict(cx=cx, cy=cy, radius=r)))

    def glass(s, x0, y0, x1, y1, dots=True):
        s.screens.append(dict(rect=[x0, y0, x1, y1]))
        s.rect(x0 - 3, y0 - 3, x1 + 3, y1 + 3, fill='#050505', stroke='#3c3c3c', sw=1, rx=4)
        s.rect(x0, y0, x1, y1, fill='#000', rx=2)
        if dots:
            for x in range(int(x0) + 24, int(x1) - 1, 24):
                for y in range(int(y0) + 12, int(y1) - 1, 12):
                    s.circle(x, y, 0.6, fill='#2a2d33')
            for y in range(int(y0) + 24, int(y1) - 1, 24):
                for x in range(int(x0) + 12, int(x1) - 1, 12):
                    s.circle(x, y, 0.6, fill='#2a2d33')

    def sheen(s, x0, y0, x1, y1):
        gid = 'gl%d%d' % (x0, y0)
        s.add(f'<linearGradient id="{gid}" x1="0" y1="0" x2="0.35" y2="1"><stop offset="0" stop-color="#fff" '
              f'stop-opacity="0.07"/><stop offset="0.45" stop-color="#fff" stop-opacity="0.015"/>'
              f'<stop offset="0.46" stop-color="#fff" stop-opacity="0"/></linearGradient>')
        s.rect(x0, y0, x1, y1, fill=f'url(#{gid})', rx=2)
        s.line(x0 + 3, y0 + 0.8, x1 - 3, y0 + 0.8, '#ffffff', 0.6, extra='opacity="0.12"')

    def lamp(s, cx, cy, on, colour=RED, r=4):
        if on:
            s.circle(cx, cy, r * 2, fill=colour, extra='opacity="0.22"')
        s.circle(cx, cy, r, fill=colour if on else '#2a1210', stroke='#000', sw=0.6)

    def wave(s, x0, x1, yc, amp, seed, colour, opacity=1.0):
        rnd = random.Random(seed)
        n = int((x1 - x0) / 2)
        top, bot = [], []
        e = 0.5
        for i in range(n + 1):
            e = max(0.15, min(1.0, e + rnd.uniform(-0.18, 0.18)))
            env = math.sin(math.pi * i / n) ** 0.35
            a = amp * e * env * rnd.uniform(0.55, 1.0)
            x = x0 + (x1 - x0) * i / n
            top.append(f'{x:.1f},{yc - a:.1f}')
            bot.append(f'{x:.1f},{yc + a:.1f}')
        s.add(f'<polygon points="{" ".join(top + bot[::-1])}" fill="{colour}" opacity="{opacity}" filter="url(#bloom)"/>')

    # -----------------------------------------------------------------
    def band_view(s, x0, y0, x1, y1, fmin=40, fmax=12000, labels=True, caption=True, handles=True, tip=True,
                  axis_inside=True):
        """Carrier filter bells (blue), modulator envelopes (amber bars), Bottom/Top handles."""
        pl, pr = x0 + 8, x1 - 8
        pt = y0 + (26 if caption else 10)
        pb = y1 - (20 if axis_inside else 8)
        X = lambda f: pl + (pr - pl) * math.log(f / fmin) / math.log(fmax / fmin)
        if caption:
            s.text(x0 + 10, y0 + 12, 'AMBER = MODULATOR ENVELOPES - NOT A METER', 9, CAPTION, bold=False)
            s.text(x1 - 10, y0 + 12, '9 BANDS · SHALLOW', 9, CARRIER, anchor='end', bold=False)
        # bars
        for f, e in zip(BANDS, ENV):
            bx = X(f)
            s.rect(bx - 6, pb - e * (pb - pt) * 0.86, bx + 6, pb, fill=MODUL, extra='opacity="0.85" filter="url(#bloom)"')
        # bells
        gid = 'bell%d' % x0
        s.add(f'<linearGradient id="{gid}" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="{CARRIER}" '
              f'stop-opacity="0.30"/><stop offset="1" stop-color="{CARRIER}" stop-opacity="0.02"/></linearGradient>')
        sp = (X(BANDS[1]) - X(BANDS[0]))
        for f in BANDS:
            bx = X(f)
            pts = []
            for i in range(61):
                u = -3 + 6 * i / 60
                x = bx + u * sp * 0.42
                y = pb - (pb - pt) * 0.92 * math.exp(-u * u / 2)
                pts.append(f'{x:.1f},{y:.1f}')
            s.add(f'<polygon points="{pts[0].split(",")[0]},{pb} {" ".join(pts)} {pts[-1].split(",")[0]},{pb}" fill="url(#{gid})"/>')
            s.add(f'<polyline points="{" ".join(pts)}" fill="none" stroke="{CARRIER}" stroke-width="1.4" filter="url(#bloom)"/>')
        s.line(pl, pb, pr, pb, CARRIER, 1.6, extra='filter="url(#bloom)"')
        # axis
        if labels:
            for f, t in [(50, '50'), (100, '100'), (200, '200'), (500, '500'), (1000, '1K'), (2000, '2K'),
                         (5000, '5K'), (10000, '10K')]:
                if f < fmin or f > fmax:
                    continue
                x = X(f)
                s.line(x, pb + 1, x, pb + 4, '#3a3f47', 1)
                s.text(x + 3, pb + 11, t, 8.5, CAPTION, bold=False)
        if handles:
            for f, nm, dash in [(100, 'BOTTOM', False), (7870, 'TOP', True)]:
                x = X(f)
                s.line(x, pt - 6, x, pb, '#ffffff', 1.2, extra='stroke-dasharray="4 3"' if dash else '')
                s.circle(x, pt + 6, 6, fill='#000', stroke='#fff', sw=1.6)
                s.text(x + (10 if not dash else -10), pt + 6, nm, 9, '#ffffff', anchor='start' if not dash else 'end', bold=False)
            if tip:
                tx = X(100) + 30
                s.rect(tx, pt + 26, tx + 196, pt + 46, fill='#000', stroke='#3c4048', sw=1, rx=3)
                s.text(tx + 10, pt + 36, 'BOTTOM 100 HZ · DRAG · SHIFT = FINE', 9, '#ffffff', bold=False)
        return X

    def pinboard(s, x0, y0, cell, n=9, total=None, labels=True, rowlab=BLABEL, collab=BLABEL, hole=2.2, pin=4.6):
        """Rows = modulator band, columns = carrier band. Red pin 100 %, white pin 50 %."""
        total = total or n
        for r in range(total):
            for c in range(total):
                cx, cy = x0 + c * cell + cell / 2, y0 + r * cell + cell / 2
                live = r < n and c < n
                v = PINS.get((r, c)) if live else None
                if v:
                    s.circle(cx + 0.8, cy + 1.2, pin, fill='#000', extra='opacity="0.6"')
                    s.circle(cx, cy, pin, fill=RED if v == 1.0 else '#e9e6dc', stroke='#000', sw=0.7)
                    s.circle(cx, cy, pin, fill='url(#shine)')
                else:
                    s.circle(cx, cy, hole, fill='#050505' if live else '#0d0d0d', stroke='#4a4a4a' if live else '#262626', sw=0.8)
        # printed lines every 3 cells inside the live block
        for k in range(3, n, 3):
            s.line(x0 + k * cell, y0 + 2, x0 + k * cell, y0 + n * cell - 2, '#2e2e2e', 1)
            s.line(x0 + 2, y0 + k * cell, x0 + n * cell - 2, y0 + k * cell, '#2e2e2e', 1)
        if labels:
            for i in range(n):
                s.text(x0 - 6, y0 + i * cell + cell / 2, rowlab[i], 8.5, INK, anchor='end', bold=False)
                x = x0 + i * cell + cell / 2
                y = y0 + total * cell + 6
                s.text(x, y, collab[i], 8.5, INK, anchor='end', bold=False,
                       extra=f'transform="rotate(-60 {x:.1f} {y:.1f})"')

    def vu(s, x0, y0, x1, y1, title, needles):
        """An analogue needle meter behind glass: printed scale, black face, pivot in the base strip."""
        s.glass(x0, y0, x1, y1, dots=False)
        cid = 'vc%d%d' % (x0, y0)
        s.add(f'<clipPath id="{cid}"><rect x="{x0}" y="{y0}" width="{x1-x0}" height="{y1-y0}"/></clipPath>')
        cx, cy = (x0 + x1) / 2, y1 - 6
        R = min((y1 - y0) - 26, (x1 - x0) * 0.5 - 6)
        s.add(f'<g clip-path="url(#{cid})">')
        marks = [(-20, -48), (-10, -32), (-7, -22), (-5, -13), (-3, -3), (0, 14), (3, 34)]
        for db, t in marks:
            a0, b0 = pol(cx, cy, R - 6, t)
            a1, b1 = pol(cx, cy, R + 1, t)
            s.line(a0, b0, a1, b1, RED if db > 0 else INK, 1.2)
            lx, ly = pol(cx, cy, R + 9, t)
            s.text(lx, ly - 3, str(abs(db)), 7.5, RED if db > 0 else INK, anchor='middle', bold=False)
        p = lambda t: pol(cx, cy, R - 6, t)
        s.add(f'<path d="M{p(-48)[0]:.1f},{p(-48)[1]:.1f} A{R-6},{R-6} 0 0 1 {p(14)[0]:.1f},{p(14)[1]:.1f}" fill="none" stroke="{INK}" stroke-width="0.8"/>')
        s.add(f'<path d="M{p(14)[0]:.1f},{p(14)[1]:.1f} A{R-6},{R-6} 0 0 1 {p(34)[0]:.1f},{p(34)[1]:.1f}" fill="none" stroke="{RED}" stroke-width="2.4"/>')
        s.text(x0 + 8, y1 - 20, title, 8.5, INK)
        for t, col in needles:
            nx, ny = pol(cx, cy, R + 2, t)
            bx, by = pol(cx, cy, 10, t)
            s.line(bx, by, nx, ny, col, 1.4, extra='filter="url(#bloom)"')
        s.rect(x0, y1 - 10, x1, y1, fill='#0a0a0a')
        s.line(x0, y1 - 10, x1, y1 - 10, '#3c3c3c', 1)
        s.add('</g>')
        s.circle(cx, cy - 2, 3, fill='#222', stroke='#000', sw=0.6)
        s.sheen(x0, y0, x1, y1)

    def out(s):
        defs = ('<defs>'
                '<radialGradient id="capgrad" cx="40%" cy="35%" r="70%"><stop offset="0" stop-color="#3a3a3a"/>'
                '<stop offset="0.6" stop-color="#161616"/><stop offset="1" stop-color="#050505"/></radialGradient>'
                '<radialGradient id="shine" cx="38%" cy="30%" r="65%"><stop offset="0" stop-color="#fff" stop-opacity="0.55"/>'
                '<stop offset="0.5" stop-color="#fff" stop-opacity="0.05"/><stop offset="1" stop-color="#000" stop-opacity="0.35"/></radialGradient>'
                '<filter id="bloom" x="-10%" y="-20%" width="120%" height="140%"><feGaussianBlur stdDeviation="0.9" result="b"/>'
                '<feComponentTransfer in="b" result="c"><feFuncA type="linear" slope="0.7"/></feComponentTransfer>'
                '<feMerge><feMergeNode in="c"/><feMergeNode in="SourceGraphic"/></feMerge></filter>'
                '</defs>')
        svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}">'
               + defs + ''.join(s.p) + '</svg>')
        open(s.name + '.svg', 'w').write(svg)
        lay = dict(width=W, height=H, headerHeight=HDR, controls=s.knobs + s.sels, screens=s.screens,
                   sections=s.sections, fingerprints=[(x * 2, y * 2) for x, y in s.fp])
        json.dump(lay, open(s.name + '.json', 'w'))


# =====================================================================
# Shared blocks
# =====================================================================
def slot_rows(g, x0, x1, y0, pitch, compact=False):
    for i, f in enumerate(FILES):
        y = y0 + i * pitch
        g.circle(x0 + 11, y + 10, 9, stroke=LINE, sw=1.2)
        g.text(x0 + 11, y + 10, str(i + 1), 9.5, INK, anchor='middle')
        if f:
            g.lamp(x0 + 32, y + 10, f[1], MODUL if False else RED, 3.5)
            g.text(x0 + 44, y + 10, f[0], 10, INK)
            gy0, gy1 = y + 22, y + pitch - 8
            g.glass(x0 + 28, gy0, x1, gy1, dots=False)
            g.wave(x0 + 34, x1 - 6, (gy0 + gy1) / 2, (gy1 - gy0) / 2 - 2, 7 + i, MODUL if f[1] else '#5a5f68')
            g.sheen(x0 + 28, gy0, x1, gy1)
        else:
            g.lamp(x0 + 32, y + 10, False, RED, 3.5)
            g.text(x0 + 44, y + 10, 'EMPTY - DROP A .WAV', 10, '#6f6c66')
            g.rect(x0 + 28, y + 22, x1, y + pitch - 8, stroke='#3a3a3a', sw=1, rx=3,
                   extra='stroke-dasharray="3 3"')
        if i < 4:
            g.rule(x0, x1, y + pitch - 2)


def input_check(g, x0, y0, x1, y1):
    g.glass(x0, y0, x1, y1)
    g.text(x0 + 8, y0 + 12, 'INPUT', 9, CAPTION, bold=False)
    g.text(x1 - 8, y0 + 12, 'CORR 0.12', 9, CARRIER, anchor='end', bold=False)
    for k, (ch, v, col, db) in enumerate([('L', 0.68, CARRIER, '-9 DB'), ('R', 0.42, MODUL, '-21 DB')]):
        yy = y0 + 32 + k * 20
        g.text(x0 + 10, yy, ch, 9, CAPTION, bold=False)
        bl, br = x0 + 24, x1 - 52
        for j in range(26):
            bx = bl + (br - bl) * j / 26
            on = j / 26 < v
            g.rect(bx, yy - 4, bx + (br - bl) / 26 - 2, yy + 4, fill=col if on else '#16181c',
                   extra='filter="url(#bloom)"' if on else '')
        g.text(x1 - 8, yy, db, 9, col, anchor='end', bold=False)
    g.sheen(x0, y0, x1, y1)


def same_audio(g, x, y):
    g.lamp(x + 5, y, False)
    g.text(x + 16, y, 'SAME AUDIO ON L AND R', 9.5, INK)


def wordmark_js(first='SPY', second='BAND', col2=INK):
    return first, second, col2


# =====================================================================
# 1  BLACK PANEL - Concept 6's layout, redressed
# =====================================================================
def design1():
    g = G('SpyBand-Polivoks-1-BlackPanel')
    g.backdrop(); g.header()
    # --- modulator
    g.section(14, 64, 300, 676, 'MODULATOR', 'THE .WAV FILES')
    g.selector(100, 128, ['SAMPLES', 'R IN'], 0, 'SOURCE', label_dy=30)
    g.selector(214, 128, ['ONCE', 'REPEAT'], 1, 'PLAYBACK', label_dy=30)
    g.rule(26, 288, 178)
    slot_rows(g, 26, 288, 188, 52)
    g.rule(26, 288, 452)
    g.knob(86, 466, 48, 0.42, '20 % UNITY', 'WAV LEVEL')
    g.knob(228, 466, 48, 0.55, '× 2.5', 'ENV LEVEL')
    g.knob(86, 574, 44, 0.30, '13.5 MS', 'ATTACK')
    g.knob(228, 574, 44, 0.22, '6.0 MS', 'RELEASE')
    # --- centre: bands
    g.section(310, 64, 880, 676, 'BANDS', 'CARRIER FILTERS + MODULATOR ENVELOPES')
    for i, (nm, on) in enumerate([('MATRIX', False), ('BANDS', True), ('INPUT', False)]):
        g.button(676 + i * 66, 74, 736 + i * 66, 96, nm, 9.5, on)
    g.glass(324, 108, 866, 352)
    g.band_view(324, 108, 866, 352)
    g.sheen(324, 108, 866, 352)
    g.fp.append((700, 220))
    g.text(324, 392, 'CARRIER', 11, INK)
    g.knob(352, 410, 48, 0.40, '20 % UNITY', 'SRC LEVEL', scale=False)
    g.knob(432, 410, 48, 0.50, '100 HZ', 'BOTTOM')
    g.knob(512, 410, 48, 0.92, '7.87 KHZ', 'TOP')
    g.knob(596, 410, 48, 0.75, '75 %', 'RESONANCE')
    g.selector(690, 448, ['9', '12', '18', '22'], 0, 'BANDS', r=15)
    g.selector(818, 448, ['SHALLOW', 'MED', 'STEEP'], 0, 'SLOPE', angles=[-60, 0, 60], lr=22)
    g.rule(322, 868, 512)
    g.text(324, 530, 'NOISE', 11, INK)
    g.knob(366, 552, 44, 0.20, '20 %', 'SENSITIVITY')
    g.knob(466, 552, 44, 0.20, '20 %', 'UNVOICED')
    g.knob(566, 552, 44, 0.12, '120 HZ', 'HIGH PASS')
    g.selector(690, 592, ['OFF', 'ON'], 1, 'DETECT')
    g.selector(800, 592, ['INPUT', 'NOISE'], 0, 'CARRIER')
    # --- right column
    g.section(890, 64, 1186, 220, 'INPUT CHECK')
    input_check(g, 904, 108, 1172, 178)
    same_audio(g, 906, 200)
    g.section(890, 228, 1186, 516, 'MATRIX', '9 × 9 OF 22')
    g.pinboard(940, 276, 17)
    for i, (lab, fill) in enumerate([('100 %', RED), ('50 %', '#e9e6dc'), ('OPEN', None)]):
        y = 290 + i * 24
        if fill:
            g.circle(1112, y, 4.6, fill=fill, stroke='#000', sw=0.7); g.circle(1112, y, 4.6, fill='url(#shine)')
        else:
            g.circle(1112, y, 2.2, fill='#050505', stroke='#4a4a4a', sw=0.8)
        g.text(1124, y, lab, 9, INK, bold=False)
    g.text(1106, 372, '11 PINS', 9, DIM, bold=False)
    g.text(1106, 386, '2 OFF-DIAG', 9, DIM, bold=False)
    for i, nm in enumerate(['DIAGONAL', 'REVERSE', 'SHIFT', 'CLEAR']):
        g.button(902 + i * 70, 482, 966 + i * 70, 504, nm, 8.5, i == 0)
    g.section(890, 524, 1186, 676, 'OUTPUT')
    g.knob(946, 566, 44, 0.0, 'OFF', 'THROUGH')
    g.knob(1040, 566, 44, 0.70, '-20.0 DB', 'TRIM', bipolar=True)
    g.glass(1112, 566, 1170, 650)
    for k, v in enumerate([0.62, 0.58]):
        bx = 1124 + k * 20
        for j in range(14):
            yy = 646 - j * 5.6
            on = j / 14 < v
            g.rect(bx, yy - 4, bx + 12, yy, fill=(OUTC if j < 11 else RED) if on else '#14161a',
                   extra='filter="url(#bloom)"' if on else '')
    g.sheen(1112, 566, 1170, 650)
    g.text(1141, 664, 'OUT', 9.5, INK, anchor='middle')
    return g


# =====================================================================
# 2  PATCH BAY - the full 22 x 22 matrix in the middle, as a pin board
# =====================================================================
def design2():
    g = G('SpyBand-Polivoks-2-PatchBay')
    g.backdrop(); g.header('FACTORY / FULL 22-BAND')
    n = 22
    hz = [100 * (7870 / 100) ** ((i + 0.5) / n) for i in range(n)]
    lab = [('%.2fK' % (f / 1000) if f >= 1000 else '%d' % f).replace('.00K', 'K') for f in hz]
    lab = [l if len(l) <= 5 else l[:4] + 'K' for l in lab]
    rnd = random.Random(5)
    env = [max(0.06, min(0.95, 0.75 * math.sin(i / 3.1) ** 2 + rnd.uniform(0, 0.3) - i * 0.012)) for i in range(n)]
    pins = {(i, i): 1.0 for i in range(n)}
    pins.update({(4, 7): 0.5, (9, 11): 0.5, (14, 12): 0.5, (17, 19): 0.5, (19, 17): 0.5})
    global PINS
    keep = PINS; PINS = pins
    # --- left: modulator + input check
    g.section(14, 64, 300, 676, 'MODULATOR', 'THE .WAV FILES')
    g.selector(100, 128, ['SAMPLES', 'R IN'], 0, 'SOURCE', label_dy=30)
    g.selector(214, 128, ['ONCE', 'REPEAT'], 1, 'PLAYBACK', label_dy=30)
    g.rule(26, 288, 178)
    slot_rows(g, 26, 288, 186, 46)
    g.rule(26, 288, 420)
    for i, (v, rd, lb) in enumerate([(0.42, '20 %', 'WAV'), (0.55, '× 2.5', 'ENV'), (0.30, '13.5 MS', 'ATTACK'),
                                     (0.22, '6.0 MS', 'RELEASE')]):
        g.knob(52 + i * 66, 432, 38, v, rd, lb)
    g.rule(26, 288, 522)
    g.text(26, 540, 'INPUT CHECK', 11, INK)
    input_check(g, 26, 556, 288, 626)
    same_audio(g, 28, 652)
    # --- centre: the whole 22 x 22 matrix as a patch bay
    g.section(310, 64, 890, 676, 'MATRIX', 'ROWS: MODULATOR BAND   COLUMNS: CARRIER BAND')
    cell = 19.5
    bw = cell * n
    bx0 = 890 - 22 - bw
    by0 = 196
    # band strip above the board: each carrier band's bell over its own column
    g.glass(bx0 - 2, 100, bx0 + bw + 2, 166)
    g.text(bx0 + 6, 110, 'CARRIER BANDS', 8.5, CAPTION, bold=False)
    g.text(bx0 + bw - 6, 110, '22 BANDS · SHALLOW', 8.5, CARRIER, anchor='end', bold=False)
    g.add(f'<clipPath id="pbclip"><rect x="{bx0-2}" y="100" width="{bw+4}" height="66"/></clipPath><g clip-path="url(#pbclip)">')
    gid = 'pbbell'
    g.add(f'<linearGradient id="{gid}" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="{CARRIER}" '
          f'stop-opacity="0.30"/><stop offset="1" stop-color="{CARRIER}" stop-opacity="0.02"/></linearGradient>')
    for c in range(n):
        cx = bx0 + c * cell + cell / 2
        pts = []
        for i in range(41):
            u = -3 + 6 * i / 40
            pts.append(f'{cx + u * cell * 0.42:.1f},{162 - 40 * math.exp(-u * u / 2):.1f}')
        g.add(f'<polygon points="{cx - 3 * cell * .42:.1f},162 {" ".join(pts)} {cx + 3 * cell * .42:.1f},162" fill="url(#{gid})"/>')
        g.add(f'<polyline points="{" ".join(pts)}" fill="none" stroke="{CARRIER}" stroke-width="1.2" filter="url(#bloom)"/>')
    g.line(bx0, 162, bx0 + bw, 162, CARRIER, 1.4, extra='filter="url(#bloom)"')
    g.add('</g>')
    g.sheen(bx0 - 2, 100, bx0 + bw + 2, 166)
    g.fp.append((640, 132))
    for i in range(n):
        g.text(bx0 + i * cell + cell / 2, 180, str(i + 1), 8, INK, anchor='middle', bold=False)
    # modulator envelopes beside the rows, then the row labels
    ex0, ex1 = 324, 364
    g.glass(ex0, by0, ex1, by0 + bw)
    for r in range(n):
        y = by0 + r * cell + cell / 2
        g.rect(ex1 - 4 - env[r] * 32, y - 4, ex1 - 4, y + 4, fill=MODUL, extra='opacity="0.85" filter="url(#bloom)"')
    g.sheen(ex0, by0, ex1, by0 + bw)
    g.pinboard(bx0, by0, cell, n=n, labels=False, pin=5.2, hole=2.4)
    for i in range(n):
        g.text(bx0 - 7, by0 + i * cell + cell / 2, lab[i], 8, INK, anchor='end', bold=False)
    # legend + patterns along the bottom
    yb = 650
    for i, (lb, fill) in enumerate([('100 %', RED), ('50 %', '#e9e6dc')]):
        x = 332 + i * 66
        g.circle(x, yb, 5, fill=fill, stroke='#000', sw=0.7); g.circle(x, yb, 5, fill='url(#shine)')
        g.text(x + 11, yb, lb, 9, INK, bold=False)
    g.text(466, yb, '27 PINS', 9, DIM, bold=False)
    for i, nm in enumerate(['DIAGONAL', 'REVERSE', 'SHIFT', 'CLEAR']):
        g.button(540 + i * 86, yb - 12, 620 + i * 86, yb + 12, nm, 9, i == 0)
    PINS = keep
    # --- right: carrier, noise, output
    g.section(900, 64, 1186, 350, 'CARRIER')
    g.knob(970, 100, 44, 0.50, '100 HZ', 'BOTTOM')
    g.knob(1116, 100, 44, 0.92, '7.87 KHZ', 'TOP')
    g.knob(970, 184, 40, 0.40, '20 %', 'SRC LEVEL', scale=False)
    g.knob(1116, 184, 40, 0.75, '75 %', 'RESONANCE')
    g.selector(970, 304, ['9', '12', '18', '22'], 3, 'BANDS', r=13, lr=22, label_dy=30)
    g.selector(1112, 304, ['SHAL', 'MED', 'STEEP'], 0, 'SLOPE', r=13, lr=21, label_dy=30)
    g.section(900, 358, 1186, 530, 'NOISE')
    g.knob(944, 396, 38, 0.20, '20 %', 'SENSE')
    g.knob(1043, 396, 38, 0.20, '20 %', 'UNVOICED')
    g.knob(1142, 396, 38, 0.12, '120 HZ', 'HIGH PASS')
    g.selector(974, 498, ['OFF', 'ON'], 1, None, r=12, lr=21)
    g.selector(1110, 498, ['INPUT', 'NOISE'], 0, None, r=12, lr=21)
    g.text(1043, 494, 'DETECT', 8, DIM, anchor='middle', bold=False)
    g.text(1043, 506, 'CARRIER', 8, DIM, anchor='middle', bold=False)
    g.section(900, 538, 1186, 676, 'OUTPUT')
    g.knob(944, 580, 40, 0.0, 'OFF', 'THROUGH')
    g.knob(1026, 580, 40, 0.70, '-20.0 DB', 'TRIM', bipolar=True)
    g.vu(1072, 552, 1174, 664, 'OUT', [(-12, OUTC)])
    return g

# =====================================================================
# 3  RECEIVER - a wide tuning dial; needle meters
# =====================================================================
def design3():
    g = G('SpyBand-Polivoks-3-Receiver')
    g.backdrop(); g.header()
    # --- the dial: full width
    g.section(14, 64, 1186, 254, 'TUNING', 'CARRIER BANDS (BLUE) · MODULATOR ENVELOPES (AMBER) · DRAG BOTTOM AND TOP')
    for i, (nm, on) in enumerate([('MATRIX', False), ('BANDS', True), ('INPUT', False)]):
        g.button(966 + i * 70, 74, 1030 + i * 70, 96, nm, 9.5, on)
    g.glass(28, 106, 1172, 226)
    X = g.band_view(28, 106, 1172, 226, fmin=30, fmax=16000, caption=False, labels=False, tip=False, axis_inside=False)
    g.sheen(28, 106, 1172, 226)
    g.fp.append((300, 160))
    # radio-style printed scale under the glass
    for f in [30, 40, 50, 60, 70, 80, 90, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000, 2000, 3000, 4000,
              5000, 6000, 7000, 8000, 9000, 10000, 16000]:
        x = X(f)
        major = f in (50, 100, 200, 500, 1000, 2000, 5000, 10000)
        g.line(x, 231, x, 231 + (6 if major else 3), LINE, 1.2 if major else 0.8)
        if major:
            g.text(x + 3, 243, {1000: '1K', 2000: '2K', 5000: '5K', 10000: '10K'}.get(f, str(f)), 8.5, INK,
                   bold=False)
    g.text(1172, 243, 'HZ', 8.5, DIM, anchor='end', bold=False)
    # --- modulator
    g.section(14, 262, 392, 676, 'MODULATOR')
    g.selector(250, 292, ['SAMPLES', 'R IN'], 0, 'SOURCE', r=11, lr=19, label_dy=21)
    g.selector(346, 292, ['ONCE', 'RPT'], 1, 'PLAYBACK', r=11, lr=19, label_dy=21)
    g.rule(26, 380, 326)
    slot_rows(g, 26, 380, 334, 42)
    g.rule(26, 380, 546)
    for i, (v, rd, lb) in enumerate([(0.42, '20 %', 'WAV'), (0.55, '× 2.5', 'ENV'), (0.30, '13.5 MS', 'ATTACK'),
                                     (0.22, '6.0 MS', 'RELEASE')]):
        g.knob(70 + i * 88, 562, 44, v, rd, lb)
    # --- carrier + noise
    g.section(402, 262, 808, 676, 'CARRIER', 'THE FILTER BANK')
    g.knob(470, 304, 64, 0.50, '100 HZ', 'BOTTOM')
    g.knob(605, 304, 64, 0.92, '7.87 KHZ', 'TOP')
    g.knob(740, 304, 64, 0.75, '75 %', 'RESONANCE')
    g.knob(470, 420, 40, 0.40, '20 %', 'SRC LEVEL', scale=False)
    g.selector(606, 448, ['9', '12', '18', '22'], 0, 'BANDS', r=14)
    g.selector(740, 448, ['SHALLOW', 'MED', 'STEEP'], 0, 'SLOPE', angles=[-60, 0, 60], lr=22)
    g.rule(414, 796, 502)
    g.text(416, 520, 'NOISE', 11, INK)
    g.knob(458, 542, 40, 0.20, '20 %', 'SENSE')
    g.knob(546, 542, 40, 0.20, '20 %', 'UNVOICED')
    g.knob(634, 542, 40, 0.12, '120 HZ', 'HIGH PASS')
    g.selector(738, 560, ['OFF', 'ON'], 1, 'DETECT', r=11, lr=19, label_dy=22)
    g.selector(738, 630, ['INPUT', 'NOISE'], 0, 'CARRIER', r=11, lr=19, label_dy=22)
    # --- meters, matrix, output
    g.section(818, 262, 1186, 428, 'METERS')
    g.vu(832, 304, 1000, 400, 'INPUT L / R', [(-20, CARRIER), (-36, MODUL)])
    g.vu(1004, 304, 1172, 400, 'OUTPUT', [(-14, OUTC)])
    g.fp.append((1080, 340))
    same_audio(g, 834, 414)
    g.text(1172, 414, 'CORR 0.12', 9, DIM, anchor='end', bold=False)
    g.section(818, 436, 1186, 676, 'MATRIX', '9 × 9 OF 22')
    g.pinboard(866, 476, 15.5, pin=4.2, hole=2)
    for i, nm in enumerate(['DIAGONAL', 'REVERSE', 'SHIFT', 'CLEAR']):
        g.button(1034, 476 + i * 28, 1110, 498 + i * 28, nm, 8.5, i == 0)
    g.knob(1066, 600, 36, 0.70, '-20 DB', 'TRIM', bipolar=True)
    g.knob(1140, 600, 36, 0.0, 'OFF', 'THRU')
    return g

if __name__ == '__main__':
    for d in (design1, design2, design3):
        g = d(); g.out(); print('wrote', g.name)
