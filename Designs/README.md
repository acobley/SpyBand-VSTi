# SpyBand: interface concepts

Six proposed editor designs, each based on R2Synth's design work. These are
mock-ups only: nothing here is implemented, and no parameter or ID changes.
Frequencies are calculated with `bandCentreHz()` from `BandLayout.cpp`, for an
example patch with Bottom Freq 50 % and Top Freq 92 % (100 Hz to 7.87 kHz).

| File | Based on | Idea | Engine work |
|---|---|---|---|
| `SpyBand-Concept1-SignalFlow.png` | R2Synth's current panel (Concept A) | Modulator › Matrix › Carrier from left to right, with Noise and Output in a strip below. Section colours match `docs/signal-path.html`. The header shows the band span, so the near-silent defaults are visible straight away. | None |
| `SpyBand-Concept2-DisplayFirst.png` | Concept B (Display First) | One large screen with Matrix, Bands and Input tabs. Bottom and Top are draggable handles. A "same audio on L and R" lamp, a preset bar, and one-click matrix patterns. | Presets, plus a hidden live output parameter for L/R correlation (like `kCutoffLive`) |
| `SpyBand-Concept3-PinMatrix.png` | KorgMoog Patch Lab | A hardware-style panel built around the matrix as a pin board (orange pins at 100 %, teal at 50 %), with a list of off-diagonal routings, tape-slot cards and VU meters. | None. The most drawing work of the three; at 22 bands it may need S/M/L sizing |
| `SpyBand-Concept4-DisplayFirst-PinMatrix.png` | Concept 2 with Concept 3's pin matrix | Concept 2's dark look, with its Matrix tab now a pin board: amber pins at 100 % and darker amber at 50 %, rows in modulator Hz (amber) and columns in carrier Hz (blue). Beside the board: the patch list, the routing under the cursor, and pattern buttons. The right-hand column shows a thumbnail of the Bands tab instead of the matrix. | As Concept 2 |
| `SpyBand-Concept5-DisplayFirst-PinPanel.png` | Concept 2, with Concept 3's pin look | Concept 2's layout left exactly as it was: the Bands tab is still open in the main screen, and the matrix stays in its own panel on the right. Only that panel has changed. It is now a pin board in Concept 2's dark style: amber pins at 100 %, darker amber at 50 %, dark sockets for open cells, rows in modulator Hz (amber) and columns in carrier Hz (blue), with a pin legend and pattern buttons. | As Concept 2 |
| `SpyBand-Concept6-70sSciFiLettering.png` | Concept 5, in 70s sci-fi lettering | A type exploration only; the layout is Concept 5's, unchanged. All the text is in heavy, wide, rounded-square capitals drawn from scratch as stroked paths, so there is no font file or font licence involved. The wordmark has a thin cut running through the strokes. | None |

**Concept 6 is implemented** on the `Design2` branch: the editor in
`source/SpyBandEditor.cpp` is this design, drawn by the plug-in's own code.
`docs/editor-*.png` are renders of the real editor, made by
`tools/render-editor/build.sh`. Where it differs from the mock-up, it is
because the editor draws real data: the band curves are each filter's
actual response (narrower than the mock-up's bells), and the waveforms are
the loaded files.

## Black panel concepts (October 2026)

Three more designs, in R2Synth's released look: the black panel ("Polivoks")
redress with the Studio Worn wear and the 70s sci-fi lettering. Matte black,
white printing, sections boxed in thin white lines with outlined title tabs,
knurled knobs with printed 11-tick scales, chicken-head rotary selectors in
place of every segmented switch, and colour only on the OLED glass (carrier
blue, modulator amber, output green). Red marks what is *set*: a selector's
chosen position, a pin, a lit lamp. Mock-ups only; no parameter or ID changes.

| File | Idea | Engine work |
|---|---|---|
| `SpyBand-Polivoks-1-BlackPanel.png` | Concept 6's layout, redressed. Bands screen in the middle, matrix as a pin board on the right (red pin 100 %, white pin 50 %), segmented level meter on the output. | None beyond Design2 |
| `SpyBand-Polivoks-2-PatchBay.png` | Matrix first: the full 22 × 22 board fills the middle as a hardware patch bay, shown on a 22-band preset. Each carrier band's bell sits above its own column; the modulator envelopes run beside the rows. Carrier, Noise and Output stacked on the right; Input Check under the files. | A Full 22-Band factory preset. The board is about 430 px square at 19.5 px a cell |
| `SpyBand-Polivoks-3-Receiver.png` | A radio receiver: one full-width tuning dial (the Bands view) with a printed scale under the glass, big Bottom / Top / Resonance knobs, needle VU meters for input L/R and output, a compact 9 × 9 matrix. | Needle meters (paths only, as the OLED screens) |

**Black Panel (design 1) is implemented** on the `Design2` branch,
replacing the Concept 6 editor: `source/SpyBandLayout.h` holds its
positions and `source/SpyBandWear.h` its wear (R2Synth's Studio Worn).
`docs/editor-*.png` are renders of the real editor. Where it differs from
the mock-up it draws real data: the band curves are each filter's actual
response, narrower than the mock-up's bells; the centre section's title
follows the tab (MATRIX, BANDS, INPUT); and the Input page's MONO /
STEREO and the voiced lamp beside DETECT, which the mock-up left out, are
there.

The 22 band frequencies in design 2 are log-spaced approximations, not
`bandCentreHz()`. Sources: `polivoks/gen.py` (SVG + layout JSON),
`polivoks/render.py` (rbfont.js, Chromium at 2×), `polivoks/weather.py`
(R2Synth's `render-weathered.py`, Studio Worn only).
