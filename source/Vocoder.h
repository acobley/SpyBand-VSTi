//------------------------------------------------------------------------
// SpyBand - the vocoder (was CSpyBand::Process and its helpers)
//
// This is the plug-in. Everything else in the DXi was DirectShow plumbing
// or MFC; this file is the part that was carried across line for line.
//
// No SDK types, so it compiles and runs standalone:
//
//     c++ -std=c++17 -O2 -Wall -Wextra -I../source
//         ../source/Vocoder.cpp ../source/Adsr.cpp ../source/WavFile.cpp
//         ../source/BandLayout.cpp Tests.cpp -o tests
//
// WHAT IT DOES, in the original's own words (SpyBand.cpp:742):
//
//     fLeftWav and fRightWav hold the modulator
//     fLeftSrc and fRightSrc hold the carrier (or pink noise)
//
// so the CARRIER is the live input and the MODULATOR is whichever of the
// five loaded .wav files are playing. That is the opposite way round from
// most vocoders, where you sing into the modulator; here you play the
// carrier in and the samples do the talking. With Interlace on, the right
// input channel becomes the modulator and the left stays the carrier, and
// no file is needed at all.
//
// Both signals go through the same bank of bandpass filters. The modulator
// side is then envelope-followed, and a 22 x 22 patch matrix decides which
// modulator band drives which carrier band - the diagonal being the plain
// vocoder and everything else being why the plug-in exists.
//------------------------------------------------------------------------

#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

#include "Adsr.h"
#include "BandLayout.h"
#include "WavFile.h"

namespace SpyBand {

//------------------------------------------------------------------------
class Vocoder
{
public:
	static constexpr int kMaxBands   = 22;
	static constexpr int kNumSlots   = 5;
	static constexpr int kPatchCells = kMaxBands * kMaxBands;

	/** Filter state is indexed by `filtNum`, which runs 0..2*bands-1 for
	    the carrier and 2*bands..4*bands-1 for the modulator. At 22 bands
	    that is 88, and the DXi sized every one of these arrays 90. */
	static constexpr int kFilterBanks = 90;

	//--------------------------------------------------------------------
	/** What the DSP is handed. Every field is an INTERNAL value - what
	    ParamInfo::MapToInternal produced - not a percentage and not a
	    normalised 0..1 from the host. SpyBandParams::toInternal is what
	    converts. */
	struct Params
	{
		double inLevel          = 0.2;   // -> * 5 inside, as the DXi did
		double sampLevel        = 0.2;   // -> * 5
		double sampThroughLevel = 0.0;
		double resonance        = 0.75;
		double freq             = 0.01;
		double freqSpread       = 0.02;
		double attack           = 0.05;  // -> * 250 + 1 ms
		double release          = 0.05;  // -> * 100 + 1 ms
		double envBoost         = 0.25;  // -> * 10
		double voicedSense      = 0.2;
		double noiseLevel       = 0.2;
		double noiseHighFreq    = 0.01;
		double outputTrim       = 1.0;   // NEW - control 0..1, 1.0 = the DXi's staging

		bool enable        = true;
		bool stereo        = false;
		bool interlaced    = false;
		bool repeatSamp    = false;
		bool voicedDetect  = false;
		bool noiseOverride = false;

		bool sampleEnabled[kNumSlots] = { false, false, false, false, false };

		int bands        = 9;    // 9, 12, 18 or 22
		int filterSlopes = 0;    // 0, 1 or 2 extra cascaded sections

		/** Row-major, row = modulator band, column = carrier band, each
		    0..2 (the DXi stored twice the dialog's 0..1). */
		double patch[kPatchCells] = { 0.0 };
	};

	//--------------------------------------------------------------------
	Vocoder ();

	/** Allocates. Call from setupProcessing, never from process - the DXi
	    called its equivalent from inside the processing loop. */
	void setSampleRate (double sampleRate);
	double sampleRate () const { return mSampleRate; }

	/** Clears every filter, envelope and counter and re-seeds the noise,
	    so two runs from reset() are identical. */
	void reset ();

	//--------------------------------------------------------------------
	// Sample slots
	//
	// The audio thread reads through an atomic pointer; the UI thread
	// publishes with a single store and keeps every buffer it has ever
	// published alive in mRetained, so the audio thread can never be
	// reading a freed one. Loads are human actions, so the list is short.
	//--------------------------------------------------------------------
	void setSample (int slot, std::shared_ptr<const WavData> data);
	std::shared_ptr<const WavData> sample (int slot) const;

	/** Frees retired buffers. Safe only when the audio thread is stopped -
	    call it from setActive(false). */
	void releaseRetiredSamples ();

	//--------------------------------------------------------------------
	/** One block. `in` and `out` are deinterleaved, `channels` is 1 or 2,
	    and `params` is the value at the END of the block: the smoothed
	    fields ramp linearly to it from the previous block's value.

	    `in` may be null, which is treated as silence. */
	void process (const Params& params,
	              const float* const* in, int inChannels,
	              float* const* out, int outChannels,
	              int frames);

	//--------------------------------------------------------------------
	// What the editor displays
	//--------------------------------------------------------------------

	/** The band envelopes, weighted by the patch matrix exactly as
	    CSpyBand::GetEnvData did. Writes `bands` values, or 2 * `bands`
	    when the last block ran in stereo, and returns how many. `out`
	    should hold 2 * kMaxBands.

	    Called at the END of process(), on the audio thread, into the
	    snapshot below. Do not call it from anywhere else: it walks mCells,
	    which process() clears and refills every block. */
	int envelopeData (double* out) const;

	/** The snapshot the editor reads. Copies at most 2 * kMaxBands values
	    and returns how many were written.

	    Safe to call from the UI thread while the audio thread is running:
	    it reads a fixed array and an atomic count, never a container. A
	    torn read shows one meter frame made of two, which is invisible at
	    thirty frames a second and is the whole cost of not locking the
	    audio thread. */
	int meter (double* out) const;

	/** Peak of the two input channels, taken immediately after the split
	    and BEFORE either level control: [0] is the left and [1] the right,
	    over the last block.

	    Deliberately ahead of Src Level and Wav Level, so the reading does
	    not move when a control does and two columns that track each other
	    mean the channels genuinely carry the same audio.

	    Same threading story as the band meter: two atomics written by the
	    audio thread, read by the editor's timer. */
	void inputPeaks (float& left, float& right) const;

	/** The voiced detector's last verdict. */
	bool voicedState () const { return mVoicedStore.load (std::memory_order_relaxed); }

	/** True while slot `slot` is enabled AND has a file loaded - the
	    indicator lamp on each file button. */
	bool samplePlaying (int slot) const;

	/** Peak absolute output since the last call, and resets it. */
	double peakSinceLastCall ();

private:
	//--------------------------------------------------------------------
	// The pieces, each named after the member function it came from
	//--------------------------------------------------------------------
	void   setEnvChar (double attackMs, double releaseMs);
	void   setFilterConstants ();             // was setFilterConstantsc
	void   setLowFilterConstants ();
	void   setHighFilterConstants ();
	void   setNoiseHighFilterConstants (double freqInternal);

	double bandpass (double in, int filtNum, int band);
	double envFollow (double input, int envNumber);
	double lowPass (double in);
	double highPass (double in);
	double noiseHighPass (double in);
	double lowEnvFollow (double input);
	double highEnvFollow (double input);
	double voicedDetection (double in);
	double noiseValue ();

	float  ditherNoise ();
	void   buildNoiseTables ();

	//--------------------------------------------------------------------
	double mSampleRate = 44100.0;

	// The DXi's fNoise[1000]: a table of numbers around 1e-20, added into
	// every filter state to keep it out of the denormal range. It is not
	// dither - it is inaudible by twenty orders of magnitude - and it is
	// reproduced exactly, LCG and all, so that a port test can compare
	// numbers with the original.
	static constexpr int kNoisePoints = 1000;
	float  mNoise[kNoisePoints] = { 0.0f };
	int    mNoisePos = 0;

	// The DXi's pNoiseSrc[48000]: gaussian white, made once and read in a
	// loop, which is the source the pink filter runs on.
	static constexpr int kWhitePoints = 48000;
	std::vector<double> mWhite;
	int    mWhitePos = 0;

	// Bandpass bank
	double mB0[kFilterBanks] = { 0.0 }, mB1[kFilterBanks] = { 0.0 };
	double mB2[kFilterBanks] = { 0.0 }, mB3[kFilterBanks] = { 0.0 };
	double mS10[kFilterBanks] = { 0.0 }, mS11[kFilterBanks] = { 0.0 };
	double mS12[kFilterBanks] = { 0.0 }, mS13[kFilterBanks] = { 0.0 };
	double mS20[kFilterBanks] = { 0.0 }, mS21[kFilterBanks] = { 0.0 };
	double mS22[kFilterBanks] = { 0.0 }, mS23[kFilterBanks] = { 0.0 };

	double mC0[kMaxBands] = { 0.0 }, mC2[kMaxBands] = { 0.0 };
	double mC3[kMaxBands] = { 0.0 }, mC4[kMaxBands] = { 0.0 };
	double mBandHz[kMaxBands] = { 0.0 };

	double mEnvelope[kFilterBanks] = { 0.0 };
	double mEnvAttackConst = 0.0;
	double mEnvReleaseConst = 0.0;

	// Voiced/unvoiced detection: a fixed 600 Hz low pass and 5800 Hz high
	// pass, each two cascaded biquads, and an envelope on each.
	double mCL0 = 0.0, mCL1 = 0.0, mCL2 = 0.0, mCL3 = 0.0, mCL4 = 0.0;
	double mCH0 = 0.0, mCH1 = 0.0, mCH2 = 0.0, mCH3 = 0.0, mCH4 = 0.0;
	double mLowState[8]  = { 0.0 };
	double mHighState[8] = { 0.0 };
	double mLowEnvelope = 0.0;
	double mHighEnvelope = 0.0;

	// Pink noise, and the high pass on it
	double mCNH0 = 0.0, mCNH1 = 0.0, mCNH2 = 0.0, mCNH3 = 0.0, mCNH4 = 0.0;
	double mNoiseHighState[8] = { 0.0 };
	double mPink0 = 0.0, mPink1 = 0.0, mPink2 = 0.0;
	double mNoiseHz = 0.0;

	Adsr   mNoiseEnvelope;
	bool   mNoiseEnvTriggered = false;
	std::atomic<bool> mVoicedStore { false };
	double mVoicedSensitivity = 0.2;

	// The meter snapshot, written at the end of every block and read by
	// the editor's timer.
	double mMeter[2 * kMaxBands] = { 0.0 };
	std::atomic<int> mMeterCount { 0 };
	std::atomic<float> mSrcPeakL { 0.0f };
	std::atomic<float> mSrcPeakR { 0.0f };

	// What the current coefficients were computed FROM, so they are only
	// recomputed when one of them moves - as the DXi did.
	double mCurrentResonance = -1.0;
	double mCurrentFreq = -1.0;
	double mCurrentSpread = -1.0;
	double mCurrentAttackMs = -1.0;
	double mCurrentReleaseMs = -1.0;
	double mCurrentNoiseHigh = -1.0;
	int    mCurrentBands = 0;
	int    mFilterSlopes = 0;

	// Sample slots
	std::atomic<const WavData*> mSlot[kNumSlots] = {};
	std::shared_ptr<const WavData> mSlotOwner[kNumSlots];
	std::vector<std::shared_ptr<const WavData>> mRetained;
	long   mSlotPos[kNumSlots] = { 0, 0, 0, 0, 0 };
	bool   mSlotEnabled[kNumSlots] = { false, false, false, false, false };

	// Per-band scratch, reused every sample rather than reallocated
	double mModL[kMaxBands] = { 0.0 }, mModR[kMaxBands] = { 0.0 };
	double mCarL[kMaxBands] = { 0.0 }, mCarR[kMaxBands] = { 0.0 };

	// The active patch cells for this block: the DXi kept a linked list of
	// non-zero cells and walked it per sample, and this is that list
	// flattened, plus the per-sample increment that ramps a cell the user
	// is dragging.
	struct Cell { int row; int column; double value; double step; };
	std::vector<Cell> mCells;

	Params mPrevious;
	bool   mHavePrevious = false;
	bool   mLastBlockStereo = false;
	int    mLastBlockBands = 9;
	double mPeak = 0.0;
};

//------------------------------------------------------------------------
} // namespace SpyBand
