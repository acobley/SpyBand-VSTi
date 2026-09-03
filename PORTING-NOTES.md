# SpyBand: DXi to VST3 and AUv2

What was decided, what was measured, and what was deliberately left alone.
The method is in `PORTING-GUIDE.md`; this is what happened when it met this
particular plug-in.

Sections are referred to by number from the source comments.

---

## 1. What SpyBand is

A **vocoder**, and an **audio effect** — whatever it registered as.

`CSpyBand` derives from `CDXi`, the filter from `CSoftSynth`, and the class
has the three note-event overrides the Cakewalk wizard generates. All three
are the wizard's stubs. `Process()` reads its input buffer on its first
line and nothing anywhere reads the MIDI queue, so the synth interface was
how a DXi got an input pin and a place in the rack, not a statement about
what the plug-in is. It is registered here as `PlugType::kFxModulation`
and `aufx`.

The signal path, in the original's own words at `SpyBand.cpp:742`:

> fLeftWav and fRightWav to hold the modulator
> fleftSrc and fRightSrc to hold the carrier (or pink noise)

so the **carrier is the live input** and the **modulator is whichever of the
five loadable .wav files are playing**. That is the opposite way round from
most vocoders, where you sing into the modulator; here you play the carrier
in and the samples do the talking. With **Interlace** on, the right input
channel becomes the modulator and the left stays the carrier, and no file
is needed at all — which is the mode almost everything below was measured
in, because it needs nothing but a stereo input.

Both signals go through the same bank of 9, 12, 18 or 22 bandpass filters.
The modulator side is envelope-followed, and a **22 x 22 patch matrix**
decides which modulator band drives which carrier band. The diagonal is a
plain vocoder; everything off it is why the plug-in exists.

A voiced/unvoiced detector watches the modulator and substitutes pink noise
for the carrier while the modulator is unvoiced, which is how a vocoder
gets its consonants.

### What survived

| DXi | VST3 | Survived? |
|---|---|---|
| `CSpyBand::Process` and its helpers | `Vocoder` | **Yes** — this is the port |
| `ADSR` | `Adsr` | Yes, line for line |
| `CSpyBand::LoadFile` | `WavFile` | Output format only; see §6 |
| `CMediaParams` / `CParamEnvelope` | `SpyBandParams` + the SDK | Replaced |
| `PersistLoad` / `PersistSave` | `get/setState` | Rewritten; old projects cannot be read |
| `CSpyBandPropPage` | `SpyBandEditor` | Rewritten from the `.rc` |
| `SlideSpin`, `PatchBoard`, `DrawArea` | `SpyBandControls` | Redrawn, same shapes and colours |
| `MemDC`, `BtnST`, `VMBitmap`, `StringMap`, `MySliderControl` | — | Deleted, all Win32 scaffolding |
| The moog filter path | — | Not ported; unreachable, see §6 |

---

## 2. Parameters

**510 of them**: the DXi's 25, an appended output trim, and 484 patch
cells. Plus `kBypass` at 1000, which is **not** an index into the table —
range-check before every lookup.

Ids are **appended, never inserted**. An id that moves loads a saved
project's value into the wrong control.

### The DXi's 25

Every one carries three ranges: VST3 normalised, the DXi *external* range
so the displayed numbers match, and the *internal* range the DSP was handed
via `ParamInfo::MapToInternal`. For every `MPT_FLOAT` in this plug-in the
internal range is 0..1 and the external is 0..100, so `toInternal` is a
division by a hundred — but the table carries it explicitly rather than
assuming, because the next plug-in's will not be.

| id | name | plain | default | what the DSP gets |
|---:|---|---|---:|---|
| 0 | Enabled | bool | 1 | nothing — see below |
| 1–5 | Sample 1–5 | bool | 0 | slot enables |
| 6 | Src Level | 0–100 % | 20 | `× 5 + 1e-5` → 1.0 |
| 7 | Wav Level | 0–100 % | 20 | `× 5 + 1e-5` → 1.0 |
| 8 | Through | 0–100 % | 0 | 0.0 |
| 9 | Resonance | 0–100 % | 75 | 0.75, used as bandwidth/4 in octaves |
| 10 | Bottom Freq | 0–100 % | 1 | 0.01 → 26.8 Hz |
| 11 | Top Freq | 0–100 % | 2 | 0.02 → 51.8 Hz |
| 12 | Env Attack | 0–100 % | 5 | `× 250 + 1` → 13.5 ms |
| 13 | Env Release | 0–100 % | 5 | `× 100 + 1` → 6 ms |
| 14 | Env Level | 0–100 % | 25 | `× 10` → 2.5 |
| 15 | Stereo | Mono/Stereo | Mono | |
| 16 | Bands | 4 steps | 9 Bands | **DEVIATION 1** |
| 17 | Interlaced | Use Sample/Interlace | Use Sample | |
| 18 | Repeat Samp | Single Shot/Repeat | Single Shot | |
| 19 | Voiced Detect | Off/On | Off | |
| 20 | Voiced Sensitivity | 0–100 % | 20 | 0.2 |
| 21 | Unvoiced Noise Level | 0–100 % | 20 | `+ 1e-5` |
| 22 | Filter Slopes | 3 steps | Shallow | **DEVIATION 2** |
| 23 | Noise Override | 2 words | Input Carrier | **DEVIATION 5**, §6 |
| 24 | Noise High Pass Freq | 0–100 % | 1 | 0.01 → 110.25 Hz |
| 25 | Output Trim | −60..0 dB | −20 dB | **NEW**, §5 |
| 26–509 | Patch, Mod *r* × Car *c* | 0–100 % | diagonal at 50 | **DEVIATION 4** |
| 1000 | Bypass | | off | **NEW** |

`ParamTableTests` asserts every number in the "what the DSP gets" column
against the arithmetic at `SpyBand.cpp:519-531`, so the table cannot drift
from what the DXi handed its DSP without a test failing.

**`Enabled` is read by nothing in the DXi.** It is the wizard's default
parameter and `Process()` never looks at it. It is kept, with its original
default of 1, and here it does what its name says: gate the processing,
alongside — not instead of — the new `Bypass`. There was no behaviour to
preserve, and a parameter that does nothing is worse than one that does the
obvious thing.

**Old presets cannot be read.** `PersistSave` wrote a `DWORD` count, then
`(index, float)` pairs in the external ranges, then five lengths and five
ANSI Windows paths. The ranges, the parameter set and the path syntax have
all changed. Decided early rather than discovered late.

### DEVIATION 1 — Bands is an enum, not a percentage

`Parameters.h` declares it `MPT_FLOAT`, 0–100, default 20. The DSP compares
it against 0, 1, 2 and 3 (`SpyBand.cpp:482-506`) and the dialog gave it
four `AddValue` strings. It is a four-step enum here.

The interesting part is the default. **20 matches none of the four cases**,
so none of the branches fired and `NumberOfBands` kept the 9 that
`Initialize` set — the DXi's effective default was **9 bands**, which is
what step 0 gives. `ParamTableTests` asserts exactly that, because it is
the kind of thing that looks like an off-by-one when you meet it later.

### DEVIATION 2 — Filter Slopes is an enum, not a percentage

The same story: `MPT_FLOAT` 0–100 in the table, read as
`GetParamValue(...) * 100 + 0.5` and truncated (`SpyBand.cpp:466`), and
only 0, 1 and 2 do anything. Three steps: Shallow, Medium, Steep — one, two
or three cascaded biquad sections, so 12, 24 or 36 dB per octave.

### DEVIATION 4 — the patch matrix defaults to its diagonal

`PatchBoard`'s constructor zeroes `iPatchBoard`, and `PersistSave` never
saved the matrix at all, so **the DXi started every session with an empty
matrix and made no sound whatever until a cell was clicked** — and lost the
matrix again on save.

The diagonal at half travel is not an invention. It is exactly what
`CSpyBand::Initialize` sets, in the block at `SpyBand.cpp:137-147` that was
commented out:

```c++
if (Row!=Column)
    iPatchBoard[Row][Column]=0.0f;
else
    iPatchBoard[Row][Column]=0.5f;
```

and 0.5 is also the value a cell takes when it is first switched on
(`iOldPatchBoard`, `PatchBoard.cpp:24`). Half travel reaches the DSP as
**1.0**, because `SetPatchValue` stored `2.0 * Value`; that doubling is the
patch parameters' internal range of 0..2.

The cells are hidden from the host's generic parameter list — a mixer strip
with 484 identical percentage sliders in it helps nobody — but they
automate, save and load like anything else.

---

## 3. The interface

### Geometry

The dialog is 378 x 217 dialog units in MS Sans Serif 8 pt. Two independent
equations settle the scale, and both come from the background static: it is
376 x 217 DLU and holds `res/background.bmp`, which is 564 x 353 pixels.

```
x:  564 / 376 = 1.5     exactly
y:  353 / 217 = 1.6267  -> 1.625, and 217 * 1.625 = 352.625 -> 353
```

1.5 and 1.625 are the standard factors for an MS Sans Serif 8 pt dialog,
which is what the `.rc` declares, so the width solves x, the height solves
y, and the font agrees with both. The static sits at DLU (0, 1), so a
control's position within the artwork is `x * 1.5` and `(y - 1) * 1.625`.
`tools/rc-geometry.py` prints the whole table; nothing was placed by eye.

### The artwork is nearly blank, and that is not a conversion error

`background.bmp` has a mean of 68 and a standard deviation of **8.6**. Hidden
in that are ghost lettering — "File 1", "Attack", "Src (left)",
"wave (right)", "Through", the three band counts — and faint outline boxes
where the controls sit. None of it is legible in use, and some of it names
controls that are no longer above it. `docs/panel-artwork-contrast.png` is
the same image stretched six times about the mean, kept so the next person
knows the marks are real.

Nothing is aligned to it. The controls carry their own lettering, which is
the next point.

### The controls draw themselves

Unusually for 2003, **none of this panel's controls is a bitmap**.
`SlideSpin` blits the parent's pixels and then paints rectangles and text
on top: a progress bar along the bottom edge, a green label over it, red
text at the top, and a 10 x 10 lamp in the corner. `PatchBoard` and
`DrawArea` do the same. So there is no artwork to recover — only shapes and
colours, and every one of them is in the source rather than matched by eye.
They are listed at the top of `SpyBandControls.h`.

### Rendering the panel found three faults that reading it had not

`tools/preview-panel.py` draws the layout from the same dialog units and
the same rules, saves a PNG, and reports overlaps. It is worth the hour.

1. **The value text was drawn through the label.** `DrawTheText` ends with
   `DrawText(..., DT_WORDBREAK | DT_CENTER)` — and `DT_CENTER` without
   `DT_VCENTER` is *horizontal* centring only, so Windows put the text at
   the **top** of the rectangle. The port had been centring it vertically,
   which put the red value straight through the green label on every
   slider. Both now sit where Windows put them.

2. **Six labels are wider than the control they name.** "Unvoiced Noise
   Level" wants 95 pixels and has 82. `DT_WORDBREAK` wrapped them into an
   11-pixel band and clipped the second line, so the DXi showed
   "Unvoiced Noise" and nothing else. The port drops a font size instead,
   twice if it must, which is the porting guide's advice for exactly this.

3. **The version label was invisible.** `IDC_VERSIONLABEL` is at 312, 179 —
   inside `IDC_PATCHBOARD`, which spans 385 to 544 across and 143 to about
   300 down. Windows created the board later, so it covered the label
   completely. Moved eleven units down, into the clear space beneath the
   board. It now reads the sample rate the DSP is running at, which is the
   one number on the panel a reader cannot work out for themselves.

The overlap check reports four more pairs that share **one dialog unit** of
edge — 1.6 pixels — which is the resource editor's own rounding and not a
fault: a `SlideSpin` draws its value at the top and its label and bar at the
bottom, so nothing of either is within 1.6 pixels of the boundary.

### The patch board is square

The `.rc` box is 106 x 106 DLU, which is **159 x 172 pixels** — the two
scale factors differ — and `PaintBk` took `min(width, height)`, so the grid
it drew was 159 square with the bottom 13 pixels of the box unused. The
view is that square, so nothing else has to steer around 13 pixels of
nothing.

### Direction of travel, preserved

* **Sliders**: one unit of a 0..100 range per pixel of horizontal movement,
  in either direction, with **no absolute positioning** — clicking does not
  jump the value to the pointer. On a control 69 pixels wide an absolute
  drag would make every setting a coarse one. Shift is a fine drag; the
  wheel works.
* **The two selectors (Bands, Filter Slopes): drag DOWN to advance.** The
  DXi's vertical mode decremented its counter when the pointer moved down
  by 25 pixels and reported `max - count`, so down raised the value. That is
  the opposite of the usual convention and it is kept — the porting guide's
  warning about vertical sliders, in reverse. The wheel is the normal way
  round, as it was.
* **The patch board**: 25 pixels per hundredth, down reduces, floor 0.01 —
  a cell cannot be dragged to silence, only made very quiet. A click that
  did not drag toggles the cell, restoring what it was worth when it was
  switched off.

  The DXi decided click-versus-drag with a hover counter incremented by a
  20 ms timer: fewer than 50 ticks and the click counted as a click, which
  is a **full second** of grace. Whether the drag actually moved the value
  is a better test and needs no timer.

### The file buttons

Click the **lamp** to enable or disable the slot; click anywhere else to
choose a file. See §6 — the enable is the write the DXi never had.

### Channel prefixes on the two level controls

With Interlace on the split is real, so the labels say so: **L Src Level**
and **R Wav Level**. With it off both channels are the carrier and Wav
Level is scaling the sample slots rather than a channel, so they read
**L+R Src Level** and **Smp Wav Level**. Set in `refresh()` rather than at
construction, since they depend on a parameter.

### The input LED columns

NEW, and they earned their place by hindsight. They read the two taps
immediately after Src Level — the point at which the left goes on to be
the carrier and the right becomes the modulator with Interlace on — and
they sit in the 48-pixel channel the dialog left empty between its first
and second columns of controls. Placed in pixels, since there is nothing in
the `.rc` to convert.

They exist because of an afternoon spent chasing a sound that was
"not right" and turned out to be a **mono source**. With identical audio in
both channels, Interlace mode makes the signal its own modulator, every
band is multiplied by its own envelope, and the result is a per-band
squaring: fed a pure 220 Hz sine it puts a third harmonic only **20 dB**
below the fundamental, against 65 dB below with a genuine stereo source.
Two columns side by side answer "are these two channels actually different
audio?" at a glance, which nothing else on the panel does.

They tap **immediately after the split and before either level control**,
which is the whole point: their reading cannot be moved by Src Level or Wav
Level, so two columns that track each other mean the channels genuinely
carry the same audio rather than merely being set to the same gain.
`VocoderTests` sweeps both controls across their range and asserts neither
moves either column.

Scale is 0 dBFS down to −60, twenty segments, green with amber for the last
9 dB and red for the last 3. Instant attack, gentle release, and a held
peak that slides down to meet the bar after about a second — a meter that
falls as fast as it rises cannot be read, and one that falls slowly hides a
gate closing.

---

## 4. Testing

Three suites, `tests/run-tests.sh`, none of which needs a host or a window
server and only one of which needs the SDK's headers.

* **`VocoderTests`** — the DSP. Every deliberate change is pinned against
  the original arithmetic **written out in the test beside it**, so the two
  can be compared rather than taken on trust.
* **`WavFileTests`** — builds .wav files of each bit depth in memory,
  writes them, reads them back.
* **`ParamTableTests`** — the table, and the values it hands the DSP.

Two of these earned their place by failing first:

**The block-size invariance test.** The port ramps its gains across a block
where the DXi read them once and stepped; with a static setting the ramp's
increment is zero, so splitting a run into 8192, 512 and 173-frame blocks
must not change one sample of it. It also catches any per-block state that
should have been per-sample.

**The stride test measured the wrong frequency and passed anyway.** The
DXi's decimation (§6) folds the signal to `fs/3` either side of itself, and
the first version of the test probed a round 14 kHz — 700 Hz off the fold —
found the skirt, and reported only 13.6 dB of difference against a
threshold of 20. Probing `fs/3` and the carrier's images either side of it
gives the real number:

```
stride artefact around fs/3:  port -138.6 dB,  DXi -94.7 dB
```

44 dB. The test now asserts a factor of 100 and that the port's own
artefact energy is below -100 dB. **A threshold set from the wrong metric
is the failure mode the guide warns about**, and this is what it looks like
from the inside.

Also asserted: two runs from `reset()` are identical; a patch cell open by
1e-30 changes nothing while one open for real does; a disabled vocoder is
its input bit for bit; an infinity fed in one block is gone by the next.

**The input taps**, which the LED columns read: the peak is the input times
Src Level and nothing else, halving Src Level halves both, and a mono
source reads identically on both columns — which is the whole reason they
are there.

**The through path**, added after a report that it might be passing signal
regardless of its setting. The decisive check empties the patch matrix, so
the vocoder can contribute nothing and the through path is the only thing
that can make a sound: at zero it is **digital silence**, not "quiet", and
above it the output is the modulator times Src Level times the control to
within 1 %. The rest of that story is under DEVIATION 5 in §6.

Both of those needed a probe signal of their own. `fillInput`'s modulator
is a 3 Hz sine, which suits the determinism checks — where what the signal
*is* does not matter — and is useless for measuring: less than one cycle
fits in the buffer, so its rms is not a sine's rms, and its spectral
leakage swamps a Goertzel a couple of hundred hertz away. The first version
of these checks used it and failed on all four assertions, none of which
was the code's fault. **A test signal chosen for one property is not
automatically fit for another.**

---

## 5. Output level

Measured before anyone played it, with the trim at the top of its travel —
that is, with the DXi's own staging and nothing hidden. Interlace mode,
Bottom Freq 50 %, Top Freq 75 %, the diagonal open at its default, two
seconds of full-scale noise in:

| condition | peak dBFS | rms dBFS |
|---|---:|---:|
| 9 bands, defaults | −3.4 | −15.8 |
| 12 bands, defaults | −1.7 | −14.2 |
| 18 bands, defaults | **+0.4** | −11.7 |
| 22 bands, defaults | **+1.7** | −10.2 |
| 9 bands, Src + Wav Level at 100 % | **+38.3** | +25.9 |
| 9 bands, Env Level at 100 % | +8.7 | −3.8 |
| 9 bands, steep slopes | −8.7 | −23.7 |
| 9 bands, whole matrix open | +10.6 | −1.4 |
| 22 bands, everything up | **+48.1** | +34.5 |

So it clips at the factory defaults on 18 and 22 bands, and has 48 dB more
than it needs at the extremes. There is no single culprit; it is
accumulation, and none of it is compensated anywhere:

* Src Level and Wav Level are each `control * 5`, so unity is at 20 % and
  the top of each control is **+14 dB**;
* Env Level is `control * 10`;
* every band output is multiplied by `3.0f` on its way out of `bandpassc`;
* every patch cell is stored as `2.0 * value`;
* and the bands are summed, so more bands is more level.

**Output Trim** is new. Its travel is −60 dB to 0 dB, so the **top of it is
the DXi's own staging** — nothing is lost, and it is one turn away. It
defaults to **−20 dB**, which puts the default patch between −18 and
−23 dBFS peak. 60 dB rather than the 40 the earlier ports used because this
one needs it.

Nothing clips inside a float plug-in, so the trim costs nothing; the reason
to have it is that a level over the ceiling **masks other faults**. Every
setting above it comes out at the same level, which reads as a control that
has stopped working rather than as clipping.

### There is no bleed path

Asked to look into signal apparently getting through in sample mode. There
is none. With a slot enabled and holding **digital zero**, and a full-level
carrier going in, the output is −338 dBFS — nothing at all. And across the
whole range the output tracks the modulator **1 : 1**:

| modulator | output rms |
|---:|---:|
| silence | −338.5 dB |
| −80 dBFS | −80.5 dB |
| −60 dBFS | −60.5 dB |
| −40 dBFS | −40.5 dB |
| −20 dBFS | −20.5 dB |
| 0 dBFS | −0.5 dB |

Twenty decibels of modulator buys exactly twenty decibels of output, with
no constant term anywhere. So what is heard as bleed is the carrier passing
under the modulator's own envelope — the vocoder working — and its floor is
**the sample's noise floor**. A .wav with hiss at −60 dBFS holds every band
open at −60 dB for as long as it plays. A cleaner sample, or a gate on the
modulator, is what removes it; there is nothing in the code to fix.

---

## 6. What was actually wrong with the DXi

Each of these was left alone or changed on evidence, not on taste. The
things that look wrong and **are the sound** — the `3.0f` on every filter,
the envelope follower's ceiling of 5.0, the detector reading the main
envelope's constants — are kept and are listed at the end.

### The stride: every third sample was dropped

`Process`'s loop increments `ix` **twice in the body and once in the `for`
statement**, so it advanced three interleaved samples per stereo frame. It
processed frames 0 and 1, then 3 and 4, then 6 and 7; every third sample of
the output stayed at the zero the buffer was `memset` to; and which input
channel it called "left" alternated.

This is not a reading of the source. `Release/SpyBand.asm`, dated July 2004
with line numbers matching the current `SpyBand.cpp`, has the extra
`add eax, 1` at the loop tail:

```
$L145602:
    mov  eax, DWORD PTR _ix$145600[esp+216]
    add  eax, 1
    cmp  eax, DWORD PTR _cFloats$145582[esp+216]
```

**The bug shipped.** A hole every third sample is a multiplication by a
three-sample square wave, which folds the whole signal to `fs/3` either
side of itself: 44 dB of artefact where the port has none (§4).

Fixed, and the shape of the bug cannot even be expressed in the port —
VST3 hands over deinterleaved channels.

### DEVIATION 3 — the hard-coded 44100

Every filter and envelope constant divided by a literal `44100.0` whatever
the host was running at: the band coefficients, the voiced detector's
600 Hz and 5800 Hz filters, the noise high pass, and
`exp(log(0.01)/(attack * SampleRate * 0.001))`. A band whose centre was
computed as 500 Hz was realised at `500 * rate / 44100` — 8 % up at 48 k
and **over an octave up at 96 k**, so the same patch was a different
instrument at a different session rate.

The rate is a parameter throughout. `VocoderTests` asserts that at 44100
the result is **bit-identical** to the original — the test writes the DXi's
behaviour out as `nominal * (actualRate / 44100.0)`, as a ratio so that at
44100 it is a multiplication by exactly 1.0 and the claim means what it
says — and that the old code would have failed the 48 k and 96 k checks.

The band layout itself lives in `BandLayout.h`, which the DSP and the
editor both call, so a frequency readout cannot disagree with the filter it
names.

### DEVIATION 5 — Noise Override was inverted

```c++
if (dNoiseOverride < 0.5f){
    fLeftSrc=PinkNoiseSample;
    fRightSrc=PinkNoiseSample;
}
```

fires when the button reads **"Input On"**, and leaves the carrier alone
when it reads "Noise Only". The two states were the wrong way round, and
since the parameter defaults to 0 the **shipped default replaced the live
input with pink noise**. Corrected; the states are named "Input Carrier"
and "Noise Carrier" here.

**This changes what the plug-in sounds like at its default**, and it is the
first thing anyone notices. The parameter's default value is unchanged — it
is still 0 — but 0 now means what the label always said it meant, so the
live input is the carrier and is therefore always present in the output.
In the DXi it was not, because 0 substituted noise. Measured, with a 220 Hz
carrier in the left and a 1 kHz modulator in the right:

| | 220 Hz (the carrier) |
|---|---:|
| the port, Through at 0 | −20.0 dB |
| the port, Through at 1 | −20.0 dB |
| the DXi's shipped default (noise carrier) | −45.5 dB |

Two things to read off that. The carrier comes out **25 dB louder than it
did in the DXi** — which is the fix working, not a leak. And the Through
control does not move it by so much as a tenth of a decibel, because the
carrier reaches the output through the vocoder and Through is not on that
path at all. What Through does is add the modulator: at 1 kHz it goes from
−93.3 dB to −10.5 dB across the control's travel, which is the whole of its
job. `VocoderTests` asserts all four of those numbers.

### The sample enables were never written

`PARAM_SAMPLE1..5` gate the five slots. **Nothing in the dialog ever wrote
them.** The file buttons were two-state `SlideSpin`s whose click handler
only opened a file dialog; their `m_bState` was never read back, and
`SetUseIndicator(true)` lit a lamp from `GetSamplePlayState`, which is
`enabled && loaded` — a lamp that could not light. Unless a host automated
the parameter, no sample could ever play, and the whole sample-slot half of
the plug-in was unreachable through its own interface.

Only the write was missing. The lamp is now the enable: click it to toggle
the slot, click the rest of the button to choose a file.

### The white noise buffer is finite by luck

`Initialize` fills 48000 gaussian samples with

```c++
double X = sqrt(-2.0f * log(R1)) * cos(2.0f * dPi * R2);
```

and takes the **logarithm of a value that can be zero**. Over the 96000
draws this consumes, Microsoft's `rand()` from the seed of 1 that `srand`
was never called to change returns zero exactly twice — at draw **25199**
and draw **74833** — and both are odd-numbered, so both land on `R2`, the
cosine's argument, and neither reaches the logarithm. One infinity in that
buffer would have produced a burst of NaN every time the read pointer came
round. Guarded; the guard costs nothing and removes the luck.

The generator itself is reproduced rather than replaced — MSVC's
`seed = seed * 214013 + 2531011; return (seed >> 16) & 0x7fff` — so the
pink noise is literally the noise the plug-in always made.

### DEVIATION 6 — Src Level moved to after the split

The DXi applied Src Level to both channels at the top of the loop, before
anything else:

```c++
fLeftSrc  = (pfSrc[ix]   + noise) * fInLevel;
fRightSrc = (pfSrc[ix+1] + noise) * fInLevel;
```

which is right when both channels are the carrier — but with **Interlace**
on the right channel is the *modulator*, and Wav Level then scaled it a
second time. One control ended up on **both sides of the vocoder's
multiply**, so the output went as Src Level *squared*.

That is measurable and it is not subtle. Halving Src Level:

| | per halving |
|---|---:|
| the DXi's coupling | **−12.0 dB** |
| the port | **−6.0 dB** |

`VocoderTests` asserts both numbers, with the DXi's behaviour written out
beside the port's — the same gain applied to the modulator by hand — so
the two can be compared rather than taken on trust.

With Interlace on, Src Level now scales the left channel alone and Wav
Level scales the right, exactly as Wav Level scales the sample slots.
Without Interlace both channels **are** the carrier, so Src Level still
scales both and nothing changes. Sample mode is untouched by this in
either case, because the samples never saw Src Level.

At the factory default the change is inaudible — Src Level defaults to
20 %, which the DSP takes as unity, so the factor removed from the
modulator is 1.00001. It only bites once the control is moved, which is
exactly when the old behaviour was most confusing.

The LED columns sit ahead of both controls, so they are unaffected by this
and by anything else on the panel — see §3.

### Smaller ones

* **One past the end.** `if (NoisePtr > 48000) NoisePtr = 0;` reads
  `pNoiseSrc[48000]` of a 48000-element array once per cycle. The
  comparison wants `>=`.
* **Uninitialised state.** `Lowenvelope`, `Highenvelope`, `bStereo` and
  `dCurrentNoiseHighFreq` are never initialised. The last one is compared
  against the parameter before it is first written, so whether the noise
  high pass had coefficients at all depended on stack contents.
* **Patch cells past the band count.** The DXi's cell list survived a
  change of band count, so a cell at row 15 with nine bands indexed
  `fLeftModulatorInputValues[15]` — of which only the first nine entries
  had been written that sample. An uninitialised read, not a sound. Cells
  outside the current count are dropped.
* **No bound on anything recursive.** Resonance multiplies into the
  feedback term and the matrix multiplies two filter outputs together.
  Recursive state is clamped to ±16.0, about +24 dBFS, far above any
  musical level; a default-settings render is bit-identical with and
  without it.
* **No non-finite handling.** A vocoder feeds its own filter bank from a
  resonant filter bank; one infinity would stay in the recursion for good.
* **Mono .wav files played an octave up.** `LoadFile` computed its frame
  count as `dwDSize/(channels * bytes)` and then multiplied it back by the
  channel count, so a mono file became a buffer the playback loop read two
  samples at a time as left and right. Mono files are duplicated into both
  channels now, which is what the code meant. Stereo files at the session
  rate behave exactly as before.
* **The loader read 16-bit only.** 8, 24 and 32-bit PCM and 32-bit float
  now load, and chunks may come in any order. Nothing about a stereo
  16-bit file changed.
* **Leaks, and a breakpoint.** `pNoiseSrc`, `pSnd1`, the skipped-chunk
  buffer and `fWave1..5` on reload were all leaked, and the sample loop
  still contains `if (i > 211000) int A=1;`.

### Deliberately kept

* **The `3.0f` on every band output** and the `× 5`, `× 10` and `× 2`
  elsewhere. This is the gain staging §5 measures; the trim exists so that
  none of it has to change.
* **The envelope follower's ceiling of 5.0.** Not a safety clamp — it is
  reached in ordinary use with Env Level up.
* **The detector reads the wrong envelope constants.**
  `setLowHighEnvChar` computes a fixed 50 ms pair into `LowEnvAttackConst`
  and friends **and is never called from anywhere**, so those members are
  dead and `Lowenvfollow`/`Highenvfollow` read `EnvAttackConst` instead.
  The detector therefore tracks the Env Attack and Env Release controls.
  That is audible behaviour, not a typo one can quietly correct.
* **The noise is louder in the right channel.** `VoicedDetection` returns a
  **level**, not a flag — zero when voiced, the high-band level when not —
  and the loop uses it both ways:

  ```c++
  fLeftSrc  += PinkNoiseSample * tmp * bVoiced;
  fRightSrc += PinkNoiseSample * tmp;
  ```

  so the left channel's noise is scaled by the detector's output and the
  right channel's is not. Kept.
* **The 1e-20 anti-denormal noise** added into every filter state, LCG and
  bit mask reproduced exactly. The numbers come out at 2^-67, about
  6.8e-21 — inaudible by twenty orders of magnitude, and it is the
  original's denormal strategy.
* **No resampling.** The DXi played every file back at the host's rate
  whatever the header said. `libsamplerate` and `libsndfile` tarballs sit
  unopened in the DXi's folder, which is where that intention stopped.
  The file's own rate is recorded in `WavData` if this is ever revisited.

### Fingerprints of things that were never finished

Worth knowing about, none of it ported:

* **The moog filter.** `setFilterConstants` and `bandpass` implement a
  four-pole Stilson/Smith ladder, `IDC_MOOG` exists in the `.rc` as
  `NOT WS_VISIBLE`, `PARAM_MOOG` is commented out of `Parameters.h`, the
  code that read it is commented out of `Process`, and `bMoog` is set false
  in `Initialize` and never set again. Both branches of every
  `if (dMoog<0.5)` therefore take the same one.
* **`EnvIn[90]`** is allocated, seeded from the noise table, and read by
  nothing.
* **`mb0..mb4[90]`**, the moog filter's state, likewise.
* **`m_dwChannelMask` and the surround path.** `Process` has a branch for
  more than two channels in which the rear pair becomes the modulator. Not
  ported — the port is mono and stereo — but it is a real feature and it is
  written down here in case it is wanted.

---

## 7. Traps from the guide, and which ones bit

* **`processContextRequirements`** — not needed, and that is worth saying
  out loud: SpyBand reads no tempo, no transport state and no musical
  position, so it asks for nothing. Unlike SpaceDub, which used the DXi
  synth interface precisely to reach the host's tempo map.
* **`sendMessage` from the audio thread is discarded.** The meter goes out
  in reply to a request, and both directions run on the UI thread. The
  audio thread writes a fixed array and an atomic count at the end of every
  block; `notify` reads that. It never touches `mCells`, which `process`
  clears and refills every block — walking that from the UI thread would be
  a crash, not a torn read.
* **Null title or units segfaults `RangeParameter`** — every parameter goes
  through `USTRING(...)`, and `ParamTableTests` asserts no null or empty
  title and no null units.
* **A `StringListParameter`'s default must be set after its strings**, or
  it is quantised against a step count of zero.
* **`dynamic_cast` is useless in `~EditorView()`** — `editorDestroyed`
  compares upcast pointers.
* **Append, never insert**, and `setState` resets anything a short stream
  does not mention back to its default.
* **The source-manifest guard will stop the first build** after these files
  were added, with the names of what changed and the command that fixes it.
  Build again.

---

## 8. Identity — never change these

Once a build has shipped, a changed identifier means every existing session
silently loses the plug-in.

```
Processor  UID   0x1AAF58FF 0x9DC676FE 0x4E7E1A9F 0x272B46BA
Controller UID   0x3C43DE25 0x3806072E 0xF1CB1DBE 0x430A90EC

AU type          aufx
AU subtype       SpyB
AU manufacturer  AECo        (shared with ForTran and SpaceDub)

VST3 bundle id   audio.spyband.vst3
AU bundle id     audio.spyband.audiounit
```

The channel layouts accepted by `setBusArrangements` — stereo/stereo and
mono/mono — must stay in step with `AudioUnit SupportedNumChannels` in
`resource/au-info.plist`. `auval` is strict about it.

---

## 9. Where it got to, and what is left

**It builds, loads and works.** Confirmed in Reaper, where the vocoder
behaves as this document describes. It also loads and runs in Logic, which
is a second result in itself: Logic validates an Audio Unit on scan and
quarantines anything that fails, so the AU wrapper, the four-character
codes and the channel layouts in `resource/au-info.plist` are all good.

One caveat recorded because it cost an afternoon and will cost the next
one too: a **Logic project** gave results that read as a plug-in fault and
were not. The same build in a fresh Reaper project was correct. Suspected
routing in that particular project — most likely both channels carrying
the same audio, which makes Interlace mode vocode the signal with itself
(§6). **The input LED columns exist because of this**, and they answer it
in about ten seconds: if the two columns track each other, the channels are
the same audio and no plug-in setting will make them behave.

Still to do:

* Run Steinberg's validator explicitly, and `auval -v aufx SpyB AECo`, for
  the record rather than by inference.
* Factory presets. The DXi's own defaults put all nine bands between
  **26.8 Hz and 51.8 Hz**, so the plug-in is very nearly silent until
  Bottom Freq and Top Freq are moved — 50 % and 92 % gives roughly 100 Hz
  to 8 kHz. The defaults are kept as the DXi had them, deliberately, so a
  couple of presets are what stop anyone meeting the 27 Hz version first.
* The `.component` that CMake installs contains a **symlink** into the
  build tree. Replace it with the real bundle before handing the plug-in to
  anyone.
* The surround path, if it is wanted (§6).
