//------------------------------------------------------------------------
// SpyBand - the vocoder
//
// Transcribed from CSpyBand::Process and its helpers. Where this file
// departs from the original it says so and PORTING-NOTES.md explains why;
// everywhere else the arithmetic is the DXi's, including the parts that
// look wrong.
//------------------------------------------------------------------------

#include "Vocoder.h"

#include <cmath>
#include <cstring>

namespace SpyBand {
namespace {

const double kPi  = 3.14159265358979323846264338327950288419716939937510;
const double kLn2 = 0.69314718055994530942;

/** Flush a denormal or a non-finite value.

    The DXi's answer to denormals was to add its ~1e-20 noise into every
    filter state, which is kept - but nothing anywhere guarded against a
    NaN or an infinity, and a vocoder feeds its own filter bank from a
    resonant filter bank. One non-finite sample would otherwise stay in the
    recursion for good. */
inline double clean (double v)
{
	if (! std::isfinite (v))
		return 0.0;
	if (v > -1e-30 && v < 1e-30)
		return 0.0;
	return v;
}

/** The ceiling on anything written back into recursive state.

    +/-16.0 is about +24 dBFS, far above any musical level, and the guide's
    recommendation from the SpaceDub port. The DXi had no bound at all: the
    resonance control multiplies straight into the feedback term, and the
    patch matrix multiplies two filter outputs together. */
inline double bounded (double v)
{
	v = clean (v);
	if (v > 16.0)
		return 16.0;
	if (v < -16.0)
		return -16.0;
	return v;
}

} // namespace

//------------------------------------------------------------------------
Vocoder::Vocoder ()
{
	mWhite.resize (kWhitePoints, 0.0);
	reset ();
}

//------------------------------------------------------------------------
// The two noise tables
//
// fNoise[1000] is the DXi's anti-denormal noise, reproduced exactly: the
// same LCG, the same bit mask, the same reinterpret. The numbers come out
// around 6.8e-21 (exponent field 60, so 2^-67), which is why adding one to
// a filter state is inaudible and keeps it out of the denormal range.
//
// pNoiseSrc[48000] is the white source the pink filter runs on. The DXi
// filled it with `rand()`, so the sequence was Microsoft's C runtime and
// nothing else - reproduced here rather than substituted, so the pink
// noise is literally the noise the plug-in always made.
//
// TRAP, and it is the original's: `X = sqrt(-2 * log(R1)) * cos(...)`
// takes the log of a value that CAN be zero. Over the 96000 draws this
// consumes, MSVC's generator returns zero exactly twice - at draw 25199
// and draw 74833 - and BOTH are odd-numbered, so both land on R2, the
// cosine's argument, and neither reaches the logarithm. The buffer is
// finite by luck. The guard below costs nothing and removes the luck.
//------------------------------------------------------------------------
void Vocoder::buildNoiseTables ()
{
	std::uint32_t state = 1;
	for (int i = 0; i < kNoisePoints; ++i)
	{
		state = state * 1234567UL + 890123UL;
		const std::uint32_t mantissa = state & 0x807f0000u;
		const std::uint32_t bits = mantissa | 0x1E000000u;
		float f = 0.0f;
		std::memcpy (&f, &bits, sizeof (f));
		mNoise[i] = f;
	}
	mNoisePos = 0;

	// MSVC's rand(), seeded as the DXi left it: srand was never called, so
	// the seed is 1.
	std::uint32_t seed = 1;
	auto msvcRand = [&seed] () -> int
	{
		seed = seed * 214013u + 2531011u;
		return static_cast<int> ((seed >> 16) & 0x7fffu);
	};

	for (int i = 0; i < kWhitePoints; ++i)
	{
		const double r1 = static_cast<float> (msvcRand ()) / 32767.0f;
		const double r2 = static_cast<float> (msvcRand ()) / 32767.0f;
		double x = 0.0;
		if (r1 > 0.0)
			x = std::sqrt (-2.0 * std::log (r1)) * std::cos (2.0 * kPi * r2);
		mWhite[static_cast<std::size_t> (i)] = x * 0.5;
	}
	mWhitePos = 0;
}

//------------------------------------------------------------------------
float Vocoder::ditherNoise ()
{
	const float v = mNoise[mNoisePos];
	++mNoisePos;
	if (mNoisePos >= kNoisePoints)
		mNoisePos = 0;
	return v;
}

//------------------------------------------------------------------------
void Vocoder::reset ()
{
	buildNoiseTables ();

	// The DXi seeded every filter and envelope state from the noise table
	// (CSpyBand::Initialize, and again inside setFilterConstants). The
	// same is done here for the state this port actually has; the DXi also
	// seeded EnvIn[] and the moog buffers mb0..mb4[], neither of which is
	// ported - EnvIn is written nowhere and read nowhere, and the moog
	// filter is unreachable. See PORTING-NOTES section 6.
	for (int i = 0; i < kFilterBanks; ++i)
	{
		mEnvelope[i] = ditherNoise ();
		mB0[i] = ditherNoise ();
		mB1[i] = ditherNoise ();
		mB2[i] = ditherNoise ();
		mB3[i] = ditherNoise ();
		mS10[i] = ditherNoise ();
		mS11[i] = ditherNoise ();
		mS12[i] = ditherNoise ();
		mS13[i] = ditherNoise ();
		mS20[i] = ditherNoise ();
		mS21[i] = ditherNoise ();
		mS22[i] = ditherNoise ();
		mS23[i] = ditherNoise ();
	}

	for (int i = 0; i < 8; ++i)
	{
		mLowState[i] = ditherNoise ();
		mHighState[i] = ditherNoise ();
		mNoiseHighState[i] = ditherNoise ();
	}

	mPink0 = ditherNoise ();
	mPink1 = ditherNoise ();
	mPink2 = ditherNoise ();

	mLowEnvelope = 0.0;
	mHighEnvelope = 0.0;
	mVoicedStore.store (false, std::memory_order_relaxed);
	mMeterCount.store (0, std::memory_order_release);
	mNoiseEnvTriggered = false;

	mNoiseEnvelope.reset ();
	mNoiseEnvelope.setSampleRate (mSampleRate);
	mNoiseEnvelope.setEnvLevel (1.0);

	for (int s = 0; s < kNumSlots; ++s)
	{
		mSlotPos[s] = 0;
		mSlotEnabled[s] = false;
	}

	// Force every coefficient to be recomputed on the first block.
	mCurrentResonance = -1.0;
	mCurrentFreq = -1.0;
	mCurrentSpread = -1.0;
	mCurrentAttackMs = -1.0;
	mCurrentReleaseMs = -1.0;
	mCurrentNoiseHigh = -1.0;
	mCurrentBands = 0;

	setLowFilterConstants ();
	setHighFilterConstants ();

	mHavePrevious = false;
	mPeak = 0.0;
	mCells.clear ();
	mCells.reserve (kPatchCells);
}

//------------------------------------------------------------------------
void Vocoder::setSampleRate (double sampleRate)
{
	if (sampleRate <= 0.0)
		return;
	mSampleRate = sampleRate;
	mNoiseEnvelope.setSampleRate (sampleRate);

	// Everything that was computed against the old rate is now wrong.
	mCurrentResonance = -1.0;
	mCurrentAttackMs = -1.0;
	mCurrentNoiseHigh = -1.0;
	setLowFilterConstants ();
	setHighFilterConstants ();
}

//------------------------------------------------------------------------
// Sample slots
//------------------------------------------------------------------------
void Vocoder::setSample (int slot, std::shared_ptr<const WavData> data)
{
	if (slot < 0 || slot >= kNumSlots)
		return;

	if (mSlotOwner[slot])
		mRetained.push_back (mSlotOwner[slot]);

	mSlotOwner[slot] = std::move (data);
	mSlot[slot].store (mSlotOwner[slot].get (), std::memory_order_release);
}

std::shared_ptr<const WavData> Vocoder::sample (int slot) const
{
	if (slot < 0 || slot >= kNumSlots)
		return nullptr;
	return mSlotOwner[slot];
}

void Vocoder::releaseRetiredSamples ()
{
	mRetained.clear ();
}

bool Vocoder::samplePlaying (int slot) const
{
	if (slot < 0 || slot >= kNumSlots)
		return false;
	return mSlotEnabled[slot];
}

//------------------------------------------------------------------------
double Vocoder::peakSinceLastCall ()
{
	const double p = mPeak;
	mPeak = 0.0;
	return p;
}

//------------------------------------------------------------------------
// Envelope follower constants, from CSpyBand::setEnvChar.
//
// The DXi's `long SampleRate = 44100` is the sample rate here instead - see
// DEVIATION 3. At 44100 the constants are identical.
//------------------------------------------------------------------------
void Vocoder::setEnvChar (double attackMs, double releaseMs)
{
	mEnvAttackConst  = std::exp (std::log (0.01) / (attackMs * mSampleRate * 0.001));
	mEnvReleaseConst = std::exp (std::log (0.01) / (releaseMs * mSampleRate * 0.001));
}

//------------------------------------------------------------------------
double Vocoder::envFollow (double input, int envNumber)
{
	const double tmp = (input > 0.0) ? input : -input;

	if (tmp > mEnvelope[envNumber])
		mEnvelope[envNumber] = mEnvAttackConst * (mEnvelope[envNumber] - tmp) + tmp;
	else
		mEnvelope[envNumber] = mEnvReleaseConst * (mEnvelope[envNumber] - tmp) + tmp;

	mEnvelope[envNumber] = clean (mEnvelope[envNumber]);

	// The original's ceiling. It is not a safety clamp - 5.0 is reached in
	// ordinary use with Env Level up - it is part of the sound.
	if (mEnvelope[envNumber] < 5.0)
		return mEnvelope[envNumber];
	return 5.0;
}

//------------------------------------------------------------------------
// The bandpass bank, from CSpyBand::setFilterConstantsc.
//
// A textbook constant-0dB-peak bandpass biquad (the "cookbook" formulas the
// original credits to harmony-central.com), one per band, with the Q set
// from the Resonance control as a BANDWIDTH in octaves: resonance/4, so the
// control's travel is 0 to a quarter of an octave.
//------------------------------------------------------------------------
void Vocoder::setFilterConstants ()
{
	const double bandwidth = mCurrentResonance / 4.0;

	for (int i = 0; i < mCurrentBands; ++i)
	{
		// The centre frequencies come from BandLayout, which the editor
		// calls too, so a readout cannot disagree with the filter it names.
		const double freq = bandCentreHz (mCurrentFreq, mCurrentSpread,
		                                  mCurrentBands, i, mSampleRate);
		mBandHz[i] = freq;

		const double omega = 2.0 * kPi * freq / mSampleRate;
		const double sn = std::sin (omega);
		const double cs = std::cos (omega);
		const double alpha = sn * std::sinh (kLn2 / 2.0 * bandwidth * omega / sn);

		const double b0 =  alpha;
		const double b2 = -alpha;
		const double a0 =  1.0 + alpha;
		const double a1 = -2.0 * cs;
		const double a2 =  1.0 - alpha;

		mC0[i] = b0 / a0;
		mC2[i] = b2 / a0;      // c1 is not kept: b1 is zero for a bandpass
		mC3[i] = a1 / a0;
		mC4[i] = a2 / a0;
	}
}

//------------------------------------------------------------------------
// One band. `filtNum` selects the state, `band` the coefficients, so the
// same band can filter the carrier and the modulator independently.
//
// Two or three identical sections in series when Filter Slopes is up:
// 12, 24 or 36 dB/octave skirts.
//------------------------------------------------------------------------
double Vocoder::bandpass (double in, int filtNum, int band)
{
	double out = mC0[band] * in
	           + mC2[band] * mB1[filtNum]
	           - mC3[band] * mB2[filtNum]
	           - mC4[band] * mB3[filtNum];
	out = clean (out);

	mB3[filtNum] = mB2[filtNum];
	mB2[filtNum] = bounded (out + ditherNoise ());
	mB1[filtNum] = mB0[filtNum];
	mB0[filtNum] = bounded (in);

	if (mFilterSlopes > 0)
	{
		in = out;
		out = mC0[band] * in
		    + mC2[band] * mS11[filtNum]
		    - mC3[band] * mS12[filtNum]
		    - mC4[band] * mS13[filtNum];
		out = clean (out);

		mS13[filtNum] = mS12[filtNum];
		mS12[filtNum] = bounded (out + ditherNoise ());
		mS11[filtNum] = mS10[filtNum];
		mS10[filtNum] = bounded (in);
	}

	if (mFilterSlopes > 1)
	{
		in = out;
		out = mC0[band] * in
		    + mC2[band] * mS21[filtNum]
		    - mC3[band] * mS22[filtNum]
		    - mC4[band] * mS23[filtNum];
		out = clean (out);

		mS23[filtNum] = mS22[filtNum];
		mS22[filtNum] = bounded (out + ditherNoise ());
		mS21[filtNum] = mS20[filtNum];
		mS20[filtNum] = bounded (in);
	}

	// The original's 3x on every band output. It is part of the staging
	// the output trim exists to undo, not a normalisation.
	return 3.0 * out;
}

//------------------------------------------------------------------------
// The voiced/unvoiced detector's two fixed filters.
//
// 600 Hz low pass and 5800 Hz high pass, each two cascaded biquads, each
// with the same bandwidth-of-one-octave alpha the bands use. The DXi wrote
// 44100 into both; here it is the real rate, so the corner stays at 600 Hz
// and 5800 Hz at every sample rate instead of moving with it.
//------------------------------------------------------------------------
void Vocoder::setLowFilterConstants ()
{
	const double freq = 600.0;
	const double bandwidth = 1.0;
	const double omega = 2.0 * kPi * freq / mSampleRate;
	const double sn = std::sin (omega);
	const double cs = std::cos (omega);
	const double alpha = sn * std::sinh (kLn2 / 2.0 * bandwidth * omega / sn);

	const double b0 = (1.0 - cs) / 2.0;
	const double b1 =  1.0 - cs;
	const double b2 = (1.0 - cs) / 2.0;
	const double a0 =  1.0 + alpha;
	const double a1 = -2.0 * cs;
	const double a2 =  1.0 - alpha;

	mCL0 = b0 / a0;
	mCL1 = b1 / a0;
	mCL2 = b2 / a0;
	mCL3 = a1 / a0;
	mCL4 = a2 / a0;
}

void Vocoder::setHighFilterConstants ()
{
	const double freq = 5800.0;
	const double bandwidth = 1.0;
	const double omega = 2.0 * kPi * freq / mSampleRate;
	const double sn = std::sin (omega);
	const double cs = std::cos (omega);
	const double alpha = sn * std::sinh (kLn2 / 2.0 * bandwidth * omega / sn);

	const double b0 =  (1.0 + cs) / 2.0;
	const double b1 = -(1.0 + cs);
	const double b2 =  (1.0 + cs) / 2.0;
	const double a0 =   1.0 + alpha;
	const double a1 =  -2.0 * cs;
	const double a2 =   1.0 - alpha;

	mCH0 = b0 / a0;
	mCH1 = b1 / a0;
	mCH2 = b2 / a0;
	mCH3 = a1 / a0;
	mCH4 = a2 / a0;
}

//------------------------------------------------------------------------
// The pink noise high pass, from setNoiseHighFilterConstants.
//
// This one used `alpha = sin/(2Q)` with Q = 1 rather than the bandwidth
// form the other three use - the bandwidth line is commented out in the
// original. Kept: it is a different filter shape and it is the shape the
// noise always had.
//------------------------------------------------------------------------
void Vocoder::setNoiseHighFilterConstants (double freqInternal)
{
	const double freq = noiseHighPassHz (freqInternal, mSampleRate);
	mNoiseHz = freq;

	const double q = 1.0;
	const double omega = 2.0 * kPi * freq / mSampleRate;
	const double sn = std::sin (omega);
	const double cs = std::cos (omega);
	const double alpha = sn / (2.0 * q);

	const double b0 =  (1.0 + cs) / 2.0;
	const double b1 = -(1.0 + cs);
	const double b2 =  (1.0 + cs) / 2.0;
	const double a0 =   1.0 + alpha;
	const double a1 =  -2.0 * cs;
	const double a2 =   1.0 - alpha;

	mCNH0 = b0 / a0;
	mCNH1 = b1 / a0;
	mCNH2 = b2 / a0;
	mCNH3 = a1 / a0;
	mCNH4 = a2 / a0;
}

//------------------------------------------------------------------------
// The three two-section filters. Each keeps four state values per section:
// [0] x(n-1), [1] x(n-2), [2] y(n-1), [3] y(n-2).
//------------------------------------------------------------------------
namespace {

inline double biquadPair (double in, double c0, double c1, double c2,
                          double c3, double c4, double* s,
                          float noiseA, float noiseB)
{
	double out = c0 * in + c1 * s[0] + c2 * s[1] - c3 * s[2] - c4 * s[3];
	out = clean (out);
	s[3] = s[2];
	s[2] = bounded (out + noiseA);
	s[1] = s[0];
	s[0] = bounded (in);

	in = out;
	out = c0 * in + c1 * s[4] + c2 * s[5] - c3 * s[6] - c4 * s[7];
	out = clean (out);
	s[7] = s[6];
	s[6] = bounded (out + noiseB);
	s[5] = s[4];
	s[4] = bounded (in);

	return 3.0 * out;
}

} // namespace

double Vocoder::lowPass (double in)
{
	return biquadPair (in, mCL0, mCL1, mCL2, mCL3, mCL4, mLowState,
	                   ditherNoise (), ditherNoise ());
}

double Vocoder::highPass (double in)
{
	return biquadPair (in, mCH0, mCH1, mCH2, mCH3, mCH4, mHighState,
	                   ditherNoise (), ditherNoise ());
}

double Vocoder::noiseHighPass (double in)
{
	return biquadPair (in, mCNH0, mCNH1, mCNH2, mCNH3, mCNH4, mNoiseHighState,
	                   ditherNoise (), ditherNoise ());
}

//------------------------------------------------------------------------
// The detector's two envelopes.
//
// These use the MAIN envelope's attack and release constants, not the
// dedicated LowEnv/HighEnv ones - CSpyBand::setLowHighEnvChar computes a
// fixed 50 ms pair into LowEnvAttackConst and friends and is never called
// from anywhere, so the members it fills are dead and the followers read
// EnvAttackConst instead. Kept as it stands: the detector tracks the Env
// Attack and Env Release controls, which is audible behaviour, not a
// typo one can quietly correct. See PORTING-NOTES section 6.
//------------------------------------------------------------------------
double Vocoder::lowEnvFollow (double input)
{
	const double tmp = (input > 0.0) ? input : -input;
	if (tmp > mLowEnvelope)
		mLowEnvelope = mEnvAttackConst * (mLowEnvelope - tmp) + tmp;
	else
		mLowEnvelope = mEnvReleaseConst * (mLowEnvelope - tmp) + tmp;
	mLowEnvelope = clean (mLowEnvelope);
	return (mLowEnvelope < 5.0) ? mLowEnvelope : 5.0;
}

double Vocoder::highEnvFollow (double input)
{
	const double tmp = (input > 0.0) ? input : -input;
	if (tmp > mHighEnvelope)
		mHighEnvelope = mEnvAttackConst * (mHighEnvelope - tmp) + tmp;
	else
		mHighEnvelope = mEnvReleaseConst * (mHighEnvelope - tmp) + tmp;
	mHighEnvelope = clean (mHighEnvelope);
	return (mHighEnvelope < 5.0) ? mHighEnvelope : 5.0;
}

//------------------------------------------------------------------------
/** Zero when the modulator is voiced, the high-band level when it is not.

    The return value is a LEVEL, not a flag, and the processing loop uses
    it both ways: `if (!bVoiced)` treats it as a flag, and
    `PinkNoiseSample * tmp * bVoiced` multiplies by it. That is why the
    left channel's noise is scaled by the detector's output and the right
    channel's is not - see the note in process(). */
double Vocoder::voicedDetection (double in)
{
	const double lowLevel = lowEnvFollow (lowPass (in));
	const double highLevel = highEnvFollow (highPass (in));

	const bool voiced = ((lowLevel * mVoicedSensitivity) > highLevel);
	mVoicedStore.store (voiced, std::memory_order_relaxed);

	if (! voiced)
		return highLevel;
	return 0.0;
}

//------------------------------------------------------------------------
// Pink noise: Paul Kellett's three-pole approximation, then the high pass.
//
// The original's `if (NoisePtr > 48000) NoisePtr = 0;` reads one past the
// end of a 48000-element array once per cycle - the comparison wants >= .
// Fixed; it is an out-of-bounds read, not a sound.
//------------------------------------------------------------------------
double Vocoder::noiseValue ()
{
	++mWhitePos;
	if (mWhitePos >= kWhitePoints)
		mWhitePos = 0;

	const double white = mWhite[static_cast<std::size_t> (mWhitePos)];

	mPink0 = 0.99765 * mPink0 + white * 0.0990460;
	mPink1 = 0.96300 * mPink1 + white * 0.2965164;
	mPink2 = 0.57000 * mPink2 + white * 1.0526913;

	double pink = mPink0 + mPink1 + mPink2 + white * 0.1848;
	pink = noiseHighPass (pink);

	return pink * 0.5;
}

//------------------------------------------------------------------------
// One block.
//
// The DXi read every parameter once at the top of Process and then ran a
// loop over the interleaved buffer. Two things about that loop are not
// reproduced, and both are recorded in PORTING-NOTES section 6:
//
//   * it advanced its index by THREE per stereo frame - twice inside the
//     body and once in the for statement - so it processed frames 0, 1
//     then 3, 4 then 6, 7, left a zero in the output at every third
//     sample, and swapped which input channel it called "left" on
//     alternate frames. VST3 hands over deinterleaved channels, so the
//     shape of the bug cannot even be expressed here;
//
//   * it read the parameters once per block and stepped nothing, so a
//     control moved during a long block arrived as a step. The gains ramp
//     across the block instead. With a static setting the ramp is a
//     no-op, which VocoderTests asserts.
//------------------------------------------------------------------------
namespace {

/** A value that walks from where it was to where it is going. */
struct Ramp
{
	double v = 0.0;
	double inc = 0.0;

	void set (double from, double to, int frames)
	{
		v = from;
		inc = (frames > 0) ? (to - from) / frames : 0.0;
	}
	void step () { v += inc; }
};

} // namespace

void Vocoder::process (const Params& params,
                       const float* const* in, int inChannels,
                       float* const* out, int outChannels,
                       int frames)
{
	if (out == nullptr || outChannels < 1 || frames <= 0)
		return;

	if (! mHavePrevious)
		mPrevious = params;

	const float* inL = (in != nullptr && inChannels > 0) ? in[0] : nullptr;
	const float* inR = (in != nullptr && inChannels > 1) ? in[1] : inL;

	float* outL = out[0];
	float* outR = (outChannels > 1) ? out[1] : nullptr;

	//--------------------------------------------------------------------
	// Disabled, and disabled last block too: pass the input through dry.
	//
	// The DXi ignored PARAM_ENABLE entirely - nothing ever read it - so
	// there is no original behaviour to preserve. An effect that is off
	// should be its input, not silence.
	//--------------------------------------------------------------------
	const bool wasEnabled = mPrevious.enable;
	if (! params.enable && ! wasEnabled)
	{
		for (int f = 0; f < frames; ++f)
		{
			const double l = inL ? inL[f] : 0.0;
			const double r = inR ? inR[f] : l;
			outL[f] = static_cast<float> (outR ? l : 0.5 * (l + r));
			if (outR)
				outR[f] = static_cast<float> (r);
		}
		mPrevious = params;
		mHavePrevious = true;
		return;
	}

	//--------------------------------------------------------------------
	// Coefficients, recomputed only when what they were made from moves -
	// which is what the DXi did, block by block.
	//--------------------------------------------------------------------
	const int bands = (params.bands > 0 && params.bands <= kMaxBands)
		? params.bands : 9;

	if (bands != mCurrentBands
	    || params.resonance != mCurrentResonance
	    || params.freq != mCurrentFreq
	    || params.freqSpread != mCurrentSpread)
	{
		mCurrentBands = bands;
		mCurrentResonance = params.resonance;
		mCurrentFreq = params.freq;
		mCurrentSpread = params.freqSpread;
		setFilterConstants ();
	}

	const double attackMs = envAttackMs (params.attack);
	const double releaseMs = envReleaseMs (params.release);
	if (attackMs != mCurrentAttackMs || releaseMs != mCurrentReleaseMs)
	{
		mCurrentAttackMs = attackMs;
		mCurrentReleaseMs = releaseMs;
		setEnvChar (attackMs, releaseMs);
	}

	if (params.noiseHighFreq != mCurrentNoiseHigh)
	{
		mCurrentNoiseHigh = params.noiseHighFreq;
		setNoiseHighFilterConstants (params.noiseHighFreq);
	}

	mFilterSlopes = (params.filterSlopes < 0) ? 0
		: (params.filterSlopes > 2 ? 2 : params.filterSlopes);
	mVoicedSensitivity = params.voicedSense;

	// The DXi set these four every block, so the noise envelope's shape is
	// fixed and only its sample rate ever changes.
	mNoiseEnvelope.setSampleRate (mSampleRate);
	mNoiseEnvelope.setAttackTime (0.05);
	mNoiseEnvelope.setDecayTime (0.01);
	mNoiseEnvelope.setSustainLevel (1.0);
	mNoiseEnvelope.setReleaseTime (0.1);

	//--------------------------------------------------------------------
	// Sample slots
	//--------------------------------------------------------------------
	const WavData* slotData[kNumSlots] = { nullptr };
	bool anySlotActive = false;
	for (int s = 0; s < kNumSlots; ++s)
	{
		slotData[s] = mSlot[s].load (std::memory_order_acquire);
		mSlotEnabled[s] = params.sampleEnabled[s] && (slotData[s] != nullptr);
		if (! mSlotEnabled[s])
			mSlotPos[s] = 0;
		else
			anySlotActive = true;
	}

	//--------------------------------------------------------------------
	// The active patch cells.
	//
	// The DXi kept a linked list of non-zero cells and walked it per
	// sample; this is that list, flattened, with the per-sample increment
	// that ramps a cell being dragged.
	//
	// Cells outside the current band count are dropped. The DXi did not
	// drop them: its list survived a change of band count, and a cell at
	// row 15 with nine bands indexed fLeftModulatorInputValues[15], which
	// only the first nine entries of had been written that sample. That is
	// an uninitialised read, not a sound.
	//--------------------------------------------------------------------
	mCells.clear ();
	for (int row = 0; row < bands; ++row)
	{
		for (int column = 0; column < bands; ++column)
		{
			const int index = row * kMaxBands + column;
			const double to = params.patch[index];
			const double from = mPrevious.patch[index];
			if (to == 0.0 && from == 0.0)
				continue;
			Cell cell;
			cell.row = row;
			cell.column = column;
			cell.value = from;
			cell.step = (to - from) / frames;
			mCells.push_back (cell);
		}
	}

	//--------------------------------------------------------------------
	// The ramped gains. Each carries the DXi's own scaling: Src Level and
	// Wav Level are multiplied by 5 with a 1e-5 floor, Env Level by 10,
	// and the noise level gets the same floor.
	//--------------------------------------------------------------------
	Ramp inLevel, sampLevel, throughLevel, envBoost, noiseLevel, trim;
	inLevel.set     (mPrevious.inLevel * 5.0 + 0.00001,
	                 params.inLevel * 5.0 + 0.00001, frames);
	sampLevel.set   (mPrevious.sampLevel * 5.0 + 0.00001,
	                 params.sampLevel * 5.0 + 0.00001, frames);
	throughLevel.set (mPrevious.sampThroughLevel, params.sampThroughLevel, frames);
	envBoost.set    (mPrevious.envBoost * 10.0, params.envBoost * 10.0, frames);
	noiseLevel.set  (mPrevious.noiseLevel + 0.00001,
	                 params.noiseLevel + 0.00001, frames);
	trim.set        (outputGain (mPrevious.outputTrim),
	                 outputGain (params.outputTrim), frames);

	// Crossfade in or out of bypass rather than stepping.
	Ramp wet;
	wet.set (wasEnabled ? 1.0 : 0.0, params.enable ? 1.0 : 0.0, frames);

	const int bank2 = 2 * bands;
	bool blockStereo = params.stereo;

	//--------------------------------------------------------------------
	for (int f = 0; f < frames; ++f)
	{
		const double dryL = inL ? inL[f] : 0.0;
		const double dryR = inR ? inR[f] : dryL;

		double srcL = (dryL + ditherNoise ()) * inLevel.v;
		double srcR = (dryR + ditherNoise ()) * inLevel.v;

		const double pink = noiseValue () * noiseLevel.v;

		double wetL = 0.0;
		double wetR = 0.0;

		if (anySlotActive || params.interlaced)
		{
			double throughL = 0.0, throughR = 0.0;
			double wavL = 0.0, wavR = 0.0;

			if (! params.interlaced)
			{
				for (int s = 0; s < kNumSlots; ++s)
				{
					if (! mSlotEnabled[s])
						continue;
					const WavData* w = slotData[s];
					const long limit = w->sampleLimit ();
					if (mSlotPos[s] >= limit)
						continue;

					const float* d = w->samples.data ();
					const double a = d[mSlotPos[s]];
					throughL += a * throughLevel.v;
					wavL     += a * sampLevel.v;
					++mSlotPos[s];
					const double b = d[mSlotPos[s]];
					throughR += b * throughLevel.v;
					wavR     += b * sampLevel.v;
					++mSlotPos[s];
				}

				// Voiced detection. `voiced` is a LEVEL: zero when the
				// modulator is voiced, the high-band level when it is not.
				const double voiced = voicedDetection (wavL + wavR);

				if (voiced == 0.0)
				{
					if (mNoiseEnvTriggered && params.voicedDetect)
					{
						mNoiseEnvelope.setRelease ();
						mNoiseEnvTriggered = false;
					}
				}
				else if (! mNoiseEnvTriggered && params.voicedDetect)
				{
					mNoiseEnvelope.trigger ();
					mNoiseEnvTriggered = true;
				}

				if ((voiced != 0.0 && params.voicedDetect)
				    || mNoiseEnvelope.phase () == kAdsrRelease)
				{
					const double env = mNoiseEnvelope.nextValue () + ditherNoise ();
					// The asymmetry is the original's: the left channel's
					// noise is scaled by the detector's OUTPUT LEVEL as
					// well as its envelope, the right channel's only by
					// the envelope. See PORTING-NOTES section 6.
					srcL += pink * env * voiced;
					srcR += pink * env;
				}

				if (! params.stereo)
				{
					wavL += wavR;
					srcL += srcR;
					blockStereo = false;
				}
				else
				{
					blockStereo = true;
				}
			}

			// Noise Override. The DXi tested `< 0.5`, so the carrier was
			// replaced by noise when the button read "Input On" and left
			// alone when it read "Noise Only" - the two states were the
			// wrong way round, and the shipped default was noise. The
			// sense is corrected here; see DEVIATION 5.
			if (params.noiseOverride)
			{
				srcL = pink;
				srcR = pink;
			}

			if (params.interlaced)
			{
				// The right input channel becomes the modulator and the
				// left stays the carrier, so no file is needed. The DXi
				// also zeroed its stereo flag here, which forces the mono
				// vocoding path; that is preserved.
				wavL = srcR * sampLevel.v;
				throughL = srcR * throughLevel.v;
				throughR = throughL;
				blockStereo = false;
			}

			if (blockStereo)
			{
				for (int i = 0; i < bands; ++i)
				{
					const int twoi = i << 1;
					mModL[i] = envBoost.v * envFollow (bandpass (wavL, bank2 + twoi, i), twoi);
					mModR[i] = envBoost.v * envFollow (bandpass (wavR, bank2 + twoi + 1, i), twoi + 1);
					mCarL[i] = bandpass (srcL, twoi, i);
					mCarR[i] = bandpass (srcR, twoi + 1, i);
				}

				for (const Cell& cell : mCells)
				{
					wetL += mModL[cell.row] * mCarL[cell.column] * cell.value;
					wetR += mModR[cell.row] * mCarR[cell.column] * cell.value;
				}
			}
			else
			{
				for (int i = 0; i < bands; ++i)
				{
					const int twoi = i << 1;
					mModL[i] = envBoost.v * envFollow (bandpass (wavL, bank2 + twoi, i), twoi);
					mCarL[i] = bandpass (srcL, twoi, i);
				}

				for (const Cell& cell : mCells)
					wetL += mModL[cell.row] * mCarL[cell.column] * cell.value;

				wetR = wetL;
			}

			wetL += throughL;
			wetR += throughR;

			for (int s = 0; s < kNumSlots; ++s)
			{
				if (! mSlotEnabled[s])
					continue;
				if (mSlotPos[s] >= slotData[s]->sampleLimit ())
				{
					if (params.repeatSamp)
						mSlotPos[s] = 0;
				}
			}
		}

		wetL = clean (wetL) * trim.v;
		wetR = clean (wetR) * trim.v;

		const double mixL = wetL * wet.v + dryL * (1.0 - wet.v);
		const double mixR = wetR * wet.v + dryR * (1.0 - wet.v);

		const double magnitude = (std::fabs (mixL) > std::fabs (mixR))
			? std::fabs (mixL) : std::fabs (mixR);
		if (magnitude > mPeak)
			mPeak = magnitude;

		if (outR)
		{
			outL[f] = static_cast<float> (mixL);
			outR[f] = static_cast<float> (mixR);
		}
		else
		{
			outL[f] = static_cast<float> (0.5 * (mixL + mixR));
		}

		inLevel.step ();
		sampLevel.step ();
		throughLevel.step ();
		envBoost.step ();
		noiseLevel.step ();
		trim.step ();
		wet.step ();
		for (Cell& cell : mCells)
			cell.value += cell.step;
	}

	mLastBlockStereo = blockStereo;
	mLastBlockBands = bands;
	mPrevious = params;
	mHavePrevious = true;

	// The editor's meter. Taken here, on the audio thread, because
	// envelopeData walks mCells - which the next block will clear.
	const int count = envelopeData (mMeter);
	mMeterCount.store (count, std::memory_order_release);
}

//------------------------------------------------------------------------
int Vocoder::meter (double* out) const
{
	if (out == nullptr)
		return 0;
	const int count = mMeterCount.load (std::memory_order_acquire);
	for (int i = 0; i < count; ++i)
		out[i] = mMeter[i];
	return count;
}

//------------------------------------------------------------------------
int Vocoder::envelopeData (double* out) const
{
	if (out == nullptr)
		return 0;

	double processed[kFilterBanks] = { 0.0 };

	for (const Cell& cell : mCells)
	{
		processed[2 * cell.row]     += mEnvelope[2 * cell.row] * cell.value;
		processed[2 * cell.row + 1] += mEnvelope[2 * cell.row + 1] * cell.value;
	}

	const int bands = mLastBlockBands;
	if (mLastBlockStereo)
	{
		for (int x = 0; x < bands; ++x)
		{
			out[x]         = processed[2 * x];
			out[bands + x] = processed[2 * x + 1];
		}
		return bands * 2;
	}

	for (int x = 0; x < bands; ++x)
		out[x] = processed[2 * x];
	return bands;
}

//------------------------------------------------------------------------
} // namespace SpyBand
