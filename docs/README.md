# docs

**signal-path.html / signal-path.png** — the routing, transcribed from
`CSpyBand::Process` rather than drawn from memory. Blue is the carrier,
amber the modulator, purple the noise, dashed green the Through path. The
HTML is the source; the PNG is rendered from it with headless Chromium, so
the picture stays regenerable rather than replaceable.

It is worth having because three things about this plug-in's routing are
the opposite of what the names suggest, and all three are easier to see
than to read:

* the **live input is the carrier** and the **samples are the modulator**,
  which is the reverse of the vocoder everyone has used;
* **Through is on the modulator side** — it never touches the carrier;
* the **band display taps the modulator envelopes before Env Level** and
  weights them by the patch matrix's row sums, so it is not a level meter.

**panel-preview.png** — the editor's layout, rendered by
`tools/preview-panel.py` straight from the dialog units in the DXi's
`SpyBand.rc`. It is not a screenshot and it is not a mock-up: the script
runs the same two scale factors the editor does and draws each control the
way `SlideSpin::PaintBk`, `PatchBoard::PaintBk` and `DrawArea::PaintBk`
drew theirs, so a control that is in the wrong place here is in the wrong
place in the plug-in.

It also carries the two input LED columns, which are new — see
PORTING-NOTES §3.

Looking at it caught three faults that reading the code had not:

* the red value text was drawn straight through the green label, because
  Windows' `DT_CENTER` without `DT_VCENTER` puts text at the TOP of its
  rectangle and the port had been centring it;
* six labels are wider than the control they name — "Unvoiced Noise Level"
  wants 95 pixels and has 82 — which `DT_WORDBREAK` wrapped into an
  11-pixel band and then clipped;
* the version label sits inside the patch board, where the Windows z-order
  hid it completely.

The script also reports any two controls whose boxes overlap by more than
the resource editor's own one-unit rounding. Run it after moving anything:

    python3 tools/preview-panel.py

**panel-artwork-contrast.png** — `res/background.bmp` with its contrast
stretched six times about the mean.

Worth keeping because the artwork looks blank at normal contrast and is
not: it has a full mean of 68 with a standard deviation of 8.6, and hidden
in that is ghost lettering — "File 1", "Attack", "Src (left)",
"wave (right)", "Through", the three band counts — and faint outline boxes
where the controls sit. None of it is legible in use, and some of it names
controls that are no longer above it, so the port does not try to align
anything to it. It is here so that the next person to open the panel knows
the marks are real rather than compression artefacts.
