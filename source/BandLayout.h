//------------------------------------------------------------------------
// SpyBand - the arithmetic the DSP and the editor must agree on
//
// A DXi could let its property page read the DSP's state directly, because
// the two were one in-process object: the dialog asked CSpyBand for
// getBassFreq(), getTopFreq() and getNoiseHighFreq() and displayed what it
// got. VST3 splits the processor and the controller into components that
// may not share an address space, so the editor has to RECOMPUTE anything
// it shows - and a private copy of the arithmetic will drift from the DSP's
// at some sample rate or setting nobody tests.
//
// So it lives here, once, and both sides call it. No SDK types, so the DSP
// stays testable standalone.
//------------------------------------------------------------------------

#pragma once

namespace SpyBand {

//------------------------------------------------------------------------
// Band counts
//
// The DXi offered 9, 12, 18 or 22 bands (SpyBand.cpp:482-506, and the four
// AddValue strings on m_bands9 in SpyBandPropPage.cpp:851-855). 22 is the
// largest, so the patch matrix is 22 x 22 whatever the current setting;
// cells past the current band count are simply not read.
//------------------------------------------------------------------------
constexpr int kBandCounts[4] = { 9, 12, 18, 22 };
constexpr int kMaxBands      = 22;
constexpr int kPatchCells    = kMaxBands * kMaxBands;   // 484

/** Band count for a control at `internal` (0..3). */
int bandCount (double internal);

//------------------------------------------------------------------------
/** Centre frequency of band `index` of `bands`, in Hz, for the bottom-freq
    and spread controls at their internal (0..1) values.

    Transcribed from CSpyBand::setFilterConstantsc. The only change is that
    the sample rate is an argument rather than the literal 44100.0 the DXi
    used - see DEVIATION 3 in PORTING-NOTES.md. At 44100 it returns what
    the DXi returned, bit for bit, which BandLayoutTests asserts. */
double bandCentreHz (double freqInternal, double spreadInternal,
                     int bands, int index, double sampleRate);

/** The two ends of the band span, in Hz: where the lowest band's lower
    edge and the highest band's upper edge sit. These are the numbers the
    Bottom and Top controls name - 100 Hz and 7.87 kHz at 50 % and 92 % -
    and bandCentreHz() divides the span between them. Same arithmetic,
    so they cannot disagree with it. */
double bandBottomHz (double freqInternal);
double bandTopHz (double freqInternal, double spreadInternal, double sampleRate);

/** The inverses, for the band display's drag handles: the control value
    that puts the bottom edge at `hz`, and the spread that puts the top
    edge at `hz` for a given bottom. Both clamped to 0..1. */
double freqForBottomHz (double hz);
double spreadForTopHz (double hz, double freqInternal);

/** Magnitude, 0..1, of one band's filter at `hz` - the response the DSP
    actually has, for drawing. The band is the bandpass biquad in
    Vocoder::setFilterConstants (bandwidth = resonance / 4 octaves) at
    `centreHz`, cascaded once, twice or three times by Filter Slopes
    (`slopes` 0..2). It peaks at exactly 1 at the centre. */
double bandResponse (double hz, double centreHz, double resonanceInternal,
                     int slopes, double sampleRate);

/** The noise high-pass corner, from CSpyBand::setNoiseHighFilterConstants:
    a quarter of the sample rate, scaled by the control. */
double noiseHighPassHz (double internal, double sampleRate);

/** Envelope follower attack and release in milliseconds, from
    SpyBand.cpp:529-530: attack is control * 250 + 1, release is
    control * 100 + 1. */
double envAttackMs  (double internal);
double envReleaseMs (double internal);

//------------------------------------------------------------------------
// Output trim
//
// NOT from the DXi. See PORTING-NOTES.md section 5 for the measurement that
// decided its default; the short version is that the DXi multiplies its
// input by 5, its envelopes by 10, every filter output by 3 and every patch
// value by 2, and none of that is compensated anywhere.
//
// The control's travel is -60 dB to +6 dB. 0 dB is the DXi's own staging,
// so nothing is lost, and the top 6 dB is make-up gain for thin settings
// (few bands, low Src/Wav Level). 60 dB of cut rather than the 40 the
// earlier ports used because SpyBand needs it: see the measurements in
// PORTING-NOTES section 5, where the extreme settings reach +48 dBFS.
//
// Before state version 2 the travel stopped at 0 dB; see
// outputControlFromV1State.
//------------------------------------------------------------------------
constexpr double kOutputMinDb = -60.0;
constexpr double kOutputMaxDb =   6.0;
constexpr double kOutputSpanDb = kOutputMaxDb - kOutputMinDb;

/** Trim control (0..1) -> a linear gain. 0 dB is the DXi's own staging. */
double outputGain (double control);

/** The same in decibels, for display: kOutputMinDb .. kOutputMaxDb. */
double outputDecibels (double control);

/** The inverse of outputDecibels, clamped to the control's travel. */
double outputControlFromDb (double db);

/** A trim value saved by state version 1 (travel -60..0 dB) -> the same
    decibels on today's control, so old projects and presets sound the same. */
double outputControlFromV1State (double v1Control);

//------------------------------------------------------------------------
} // namespace SpyBand
