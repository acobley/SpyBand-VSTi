# SpyBand 1.1.0.0

A **band vocoder** for macOS, as a **VST3** and an **Audio Unit**. Stereo in,
stereo out. A port of the 2004 Cakewalk DXi of the same name, rebuilt from its
own sources rather than reimplemented from its behaviour.

It splits one signal into 9, 12, 18 or 22 bandpass filters, follows the
envelope of each band of a second signal, and multiplies the two together
through a **22 × 22 patch matrix**. The diagonal is an ordinary vocoder. Every
cell off the diagonal routes one part of the spectrum to control another — low
bands opening high ones, spectral inversion, and anything else the grid allows.
That matrix is why the plug-in exists and is what it is named after.

The modulator can come from up to **five loaded .wav files** or, in *Interlace*
mode, from the **right input channel**. A voiced/unvoiced detector substitutes
pink noise for consonants, and a **Through** control passes the modulator to
the output dry.

## What changed since 1.0.1.0

### A new panel

The editor is no longer a copy of the DXi's dialog. It is a black hardware
panel, matching R2Synth: white printing on matte black, knurled knobs with
printed scales, a rotary selector for every switch, small OLED screens, and
a little wear from years on a desk. All the lettering is drawn by the
plug-in itself, so it is sharp at any size and the same in every host.

- **A display with three pages.** BANDS shows each band's filter as the
  response it really has, with the modulator's envelopes as bars;
  **drag Bottom and Top** to move the band span (Shift for fine). MATRIX
  shows the patch matrix large. INPUT shows the two inputs before any
  level control, and Mono / Stereo processing.
- **The matrix as a pin board.** A red pin is 100 %, a white pin 50 %.
  Click to pin or unpin; drag up or down to set a cell's level. Rows are
  the modulator's bands, columns the carrier's. **DIAGONAL, REVERSE, SHIFT
  and CLEAR** set a pattern in one click.
- **Changing the band count keeps the pattern.** Going from 9 bands to 22
  carries a diagonal, reverse or shifted pattern on to every band, and
  moves any other pins to the same place relative to the bands.
- **Input Check** replaces the two LED columns: the left and right inputs
  as meters, and a **SAME AUDIO ON L AND R** lamp that lights when the two
  are one signal - the mono-into-Interlace mistake, answered at a glance.
- **The sample slots** show each file's waveform. Drop a .wav on a slot to
  load it, or right-click for Choose and Clear. The lamp shows what will
  actually play.

### Presets

**Eight factory presets**, led by **Classic 9-Band**, which spans about
100 Hz to 8 kHz and is the quickest way past the near-silent defaults:
Classic 9-Band, Wide 12-Band, Fine 22-Band, Upside Down, Shifted Up,
Interlace Live, Noise Whisper and DXi Defaults. They leave your loaded
files alone. **SAVE** writes your own as `.vstpreset` files to
`~/Library/Audio/Presets/AE Cobley/SpyBand`, where hosts that browse that
folder find them too. A project reopens showing the preset it was on.

### Output Trim reaches +6 dB

It ran from −60 dB to 0 dB; it now runs to **+6 dB**. The default is still
−20 dB, and projects and presets saved with earlier versions keep the trim
they had.

**Projects saved with 1.1.0.0 will not open their SpyBand settings in an
earlier version** - the saved state has a new version number for the trim's
range. Earlier projects open in 1.1.0.0 unchanged. Nothing else in the
sound changed, and the plug-in's identity codes are the same, so SpyBand
stays in place in your projects.

## Installing

Download `SpyBand-1.1.0.0.pkg` and open it. The installer offers the two
formats separately:

    /Library/Audio/Plug-Ins/VST3/SpyBand.vst3
    /Library/Audio/Plug-Ins/Components/SpyBand.component

Quit your DAW first - a running host holds the old copy open. Signed with a
Developer ID and notarised by Apple. Some hosts keep a cached copy of an
Audio Unit; the version number has moved so that they notice the new one.

Requires macOS 10.13 or later.

## Three deliberate behaviours

All three are on purpose, all three are the opposite of what the names
suggest, and between them they account for every "this is faulty" moment
during the port:

* **Your input is the CARRIER, not the modulator.** In most vocoders you sing
  into the modulator and a synth is the carrier. Here it is the other way
  round: your signal is the thing being *shaped*, and the .wav files - or the
  right input channel - do the shaping. So your input stays audible in the
  output. That is the plug-in working, not leaking.
* **It is nearly silent at its DXi defaults.** Bottom Freq and Top Freq
  start at 1 % and 2 %, putting all nine bands between **26.8 Hz and
  51.8 Hz** - the DXi's own defaults, kept as the **DXi Defaults** preset.
  Load **Classic 9-Band** instead.
* **Interlace needs a genuinely stereo input.** It makes the left channel
  the carrier and the right the modulator. Fed a mono source, the signal
  becomes its own modulator and sounds like harsh distortion. The **SAME
  AUDIO** lamp lights when that is happening.

## What the port changed

Two of the original's behaviours could not be kept, and both are documented
with the measurement that settled them in `PORTING-NOTES.md`:

* **A loop that processed every third sample has been fixed.** The DXi
  advanced its band loop by three and wrote one sample in three, which puts a
  large artefact at a third of the sample rate - 44 dB of it.
* **Src Level now scales only the carrier**, not the carrier and the modulator
  together, matching Wav Level on the other channel.

**Output Trim** is not in the DXi, and defaults to **−20 dB**: the original's
gain staging clips at anything past nine bands. 0 dB is the original's own
level.

## Known gaps

* **No resampling of loaded files.** As in the DXi, a .wav plays back at the
  host's sample rate whatever its header says.
* **Mono, stereo and Interlace only** - the DXi's unfinished surround path was
  not brought across.

## Uninstalling

A `.pkg` never will, so:

```sh
sudo rm -rf /Library/Audio/Plug-Ins/VST3/SpyBand.vst3
sudo rm -rf /Library/Audio/Plug-Ins/Components/SpyBand.component
```

## Checksum

Taken from the finished, notarised and **stapled** package - stapling changes
the bytes, so a checksum taken before it does not match what you downloaded.

```
SHA-256: <paste from: shasum -a 256 installer/SpyBand-1.1.0.0.pkg>
```

---

Copyright 2004, 2026 A. E. Cobley. CC BY-SA 4.0.
VST is a trademark of Steinberg Media Technologies GmbH.
