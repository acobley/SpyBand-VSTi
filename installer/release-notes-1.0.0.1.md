# SpyBand 1.0.0.1

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

## Installing

Download `SpyBand-1.0.0.1.pkg` and open it. The installer offers the two
formats separately:

    /Library/Audio/Plug-Ins/VST3/SpyBand.vst3
    /Library/Audio/Plug-Ins/Components/SpyBand.component

Quit your DAW first — a running host holds the old copy open. Signed with a
Developer ID and notarised by Apple.

Requires macOS 10.13 or later.

## Three deliberate behaviours

All three are on purpose, all three are the opposite of what the names
suggest, and between them they account for every "this is faulty" moment
during the port:

* **Your input is the CARRIER, not the modulator.** In most vocoders you sing
  into the modulator and a synth is the carrier. Here it is the other way
  round: your signal is the thing being *shaped*, and the .wav files — or the
  right input channel — do the shaping. So your input stays audible in the
  output. That is the plug-in working, not leaking. Measured: with a silent
  modulator the output is −338 dBFS, which is digital silence, and there is no
  bleed path at all.
* **It is nearly silent at its factory defaults.** Bottom Freq and Top Freq
  start at 1 % and 2 %, putting all nine bands between **26.8 Hz and
  51.8 Hz**. These are the DXi's own defaults, kept deliberately rather than
  quietly improved. Bottom Freq around 50 % and Top Freq around 92 % spans
  roughly 100 Hz to 8 kHz.
* **Interlace mode needs a genuinely stereo input.** It makes the left channel
  the carrier and the right the modulator. Fed a mono source — or a mono
  channel strip — the signal becomes its own modulator, a per-band squaring
  that sounds like harsh distortion. The **two LED columns** exist to answer
  exactly this: they read the two inputs before any level control, so if they
  move identically the source is mono and no setting will help.

## What the port changed

Two of the original's behaviours could not be kept, and both are documented
with the measurement that settled them in `PORTING-NOTES.md`:

* **A loop that processed every third sample has been fixed.** The DXi
  advanced its band loop by three and wrote one sample in three, which puts a
  large artefact at a third of the sample rate — 44 dB of it. That shipped in
  2004 and is confirmed in the original's own disassembly.
* **Src Level now scales only the carrier**, not the carrier and the modulator
  together. In the DXi it sat before the channel split, so halving it dropped
  the output 12 dB instead of 6. It is now after the split, matching Wav Level
  on the other channel.

**Output Trim** is new, and defaults to **−20 dB**: the original's gain
staging clips at anything past nine bands. The top of the control is the
original's own level, so nothing is lost.

## Known gaps

* **No factory presets.** With the DXi's defaults kept as they are, the first
  thing to do after loading is move the two frequency controls.
* **No resampling of loaded files.** As in the DXi, a .wav plays back at the
  host's sample rate whatever its header says.
* **Mono, stereo and Interlace only** — the DXi's unfinished surround path was
  not brought across.

## Uninstalling

A `.pkg` never will, so:

```sh
sudo rm -rf /Library/Audio/Plug-Ins/VST3/SpyBand.vst3
sudo rm -rf /Library/Audio/Plug-Ins/Components/SpyBand.component
```

## Checksum

Taken from the finished, notarised and **stapled** package — stapling changes
the bytes, so a checksum taken before it does not match what you downloaded.

```
126095b5f46904b22c519e8bcf0e51cd7236a421291850cdacb4d662c657696a  SpyBand-1.0.0.1.pkg
```

---

Copyright 2004, 2026 A. E. Cobley. CC BY-SA 4.0.
VST is a trademark of Steinberg Media Technologies GmbH.
