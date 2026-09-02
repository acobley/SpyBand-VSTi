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
// The control's travel is -60 dB to 0 dB, so the TOP of it is the DXi's own
// staging and nothing is lost. 60 dB rather than the 40 the earlier ports
// used because SpyBand needs it: see the measurements in PORTING-NOTES
// section 5, where the extreme settings reach +48 dBFS.
//------------------------------------------------------------------------
constexpr double kOutputRangeDb = 60.0;

/** Trim control (0..1) -> a linear gain. 1.0 is the DXi's own staging. */
double outputGain (double control);

/** The same in decibels, for display: -kOutputRangeDb .. 0. */
double outputDecibels (double control);

/** The inverse of outputDecibels, clamped to the control's travel. */
double outputControlFromDb (double db);

//------------------------------------------------------------------------
} // namespace SpyBand
