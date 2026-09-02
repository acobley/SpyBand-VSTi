#!/usr/bin/env python3
"""Convert the DXi dialog's control geometry from dialog units to pixels.

The .rc gives every control in DIALOG UNITS, which are a function of the
dialog font rather than pixels. Two independent equations settle the scale
here, and they come from the same control: the background static is
376 x 217 DLU and holds res/background.bmp, which is 564 x 353 pixels.

    x:  564 / 376 = 1.5     exactly
    y:  353 / 217 = 1.6267  -> 1.625, and 217 * 1.625 = 352.625 -> 353

1.5 and 1.625 are the standard factors for an MS Sans Serif 8 pt dialog,
which is what the .rc declares, so the width solves x, the height solves y,
and the font agrees with both.

The static sits at DLU (0, 1) - one unit down - so a control's position
WITHIN THE ARTWORK is

    x_px = x_dlu * 1.5
    y_px = (y_dlu - 1) * 1.625

and that is what the editor uses, because it draws the artwork at (0, 0)
rather than reproducing the dialog's one-DLU top margin. The editor window
is the bitmap: 564 x 353.

Run:  python3 tools/rc-geometry.py [path/to/SpyBand.rc]
"""

import re
import sys

X_SCALE = 1.5
Y_SCALE = 1.625
Y_ORIGIN_DLU = 1          # the background static's own y

# Statements whose fields are: "text", id, x, y, w, h, [style], [exstyle]
SIMPLE = ('PUSHBUTTON', 'DEFPUSHBUTTON', 'LTEXT', 'RTEXT', 'CTEXT', 'GROUPBOX')
# CONTROL is: "text", id, "class", style, x, y, w, h, [exstyle]


def split_fields(rest):
    """Split a statement's argument list on top-level commas, keeping
    quoted strings (in which "" is an escaped quote) intact."""
    fields, buf, in_str = [], '', False
    i = 0
    while i < len(rest):
        c = rest[i]
        if c == '"':
            if in_str and i + 1 < len(rest) and rest[i + 1] == '"':
                buf += '""'
                i += 2
                continue
            in_str = not in_str
            buf += c
        elif c == ',' and not in_str:
            fields.append(buf.strip())
            buf = ''
        else:
            buf += c
        i += 1
    if buf.strip():
        fields.append(buf.strip())
    return fields


def unquote(field):
    if field.startswith('"') and field.endswith('"'):
        return field[1:-1].replace('""', '"')
    return field


def parse(path):
    """Yield (ident, text, x, y, w, h) for every control in IDD_PROPPAGE."""
    text = open(path, encoding='latin-1').read()
    body = text.split('IDD_PROPPAGE DIALOGEX', 1)[1]
    body = body.split('BEGIN', 1)[1].split('\nEND', 1)[0]

    # A statement continues onto the next line while it ends with a comma or
    # with a '|' - the resource editor breaks long style lists after either,
    # and IDC_MOOG (the removed "moog filters" checkbox) is the one control
    # that is only reachable if both are handled.
    statements, current = [], ''
    for raw in body.splitlines():
        current = (current + ' ' + raw.strip()).strip()
        if current.endswith(',') or current.endswith('|'):
            continue
        if current:
            statements.append(current)
        current = ''
    if current:
        statements.append(current)

    for st in statements:
        keyword, _, rest = st.partition(' ')
        fields = split_fields(rest)
        if keyword in SIMPLE:
            if len(fields) < 6:
                continue
            label, ident = unquote(fields[0]), fields[1]
            geom = fields[2:6]
        elif keyword == 'CONTROL':
            if len(fields) < 8:
                continue
            label, ident = unquote(fields[0]), fields[1]
            geom = fields[4:8]
        else:
            continue
        try:
            x, y, w, h = (int(g) for g in geom)
        except ValueError:
            continue
        yield ident, label, x, y, w, h


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else '../SpyBand-dxi/SpyBand.rc'
    print(f'{"control":<22} {"text":<16} {"x,y,w,h (DLU)":>17}   '
          f'{"x,y,w,h (px, artwork-relative)":>31}')
    print('-' * 92)
    for ident, label, x, y, w, h in parse(path):
        px, py = x * X_SCALE, (y - Y_ORIGIN_DLU) * Y_SCALE
        pw, ph = w * X_SCALE, h * Y_SCALE
        print(f'{ident:<22} {label[:16]:<16} '
              f'{f"{x},{y},{w},{h}":>17}   '
              f'{f"{px:.1f},{py:.1f},{pw:.1f},{ph:.1f}":>31}')


if __name__ == '__main__':
    main()
