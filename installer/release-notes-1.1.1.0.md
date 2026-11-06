# SpyBand 1.1.1.0

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

## What changed since 1.1.0.0

- **Flush-to-zero on the audio thread.** Each processing block now runs with
  the CPU's flush-to-zero mode on, and puts the host's setting back
  afterwards, so a signal decaying towards silence can never slow the
  plug-in down. SpyBand already cleared its own filters and envelopes of
  these tiny "denormal" numbers; this covers everything else, and anything
  added later.

The sound, the presets, the parameters and saved projects are exactly as in
1.1.0.0.

## Installing

Download `SpyBand-1.1.1.0.pkg` and open it. The installer offers the two
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
SHA-256: <paste from: shasum -a 256 installer/SpyBand-1.1.1.0.pkg>
```

---

Copyright 2004, 2026 A. E. Cobley. CC BY-SA 4.0.
VST is a trademark of Steinberg Media Technologies GmbH.
