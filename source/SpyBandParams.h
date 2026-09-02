//------------------------------------------------------------------------
// SpyBand - parameter definitions
//
// Generated from the DXi's own Parameters.h (both the enum and the
// CMediaParams::m_aParamInfo table), so the order, the ranges and the
// defaults are the original's by construction rather than by transcription.
//
// As in the SpaceDub and ForTran ports, every parameter carries three
// ranges:
//   * VST3 normalised, 0..1, which is what the host works in;
//   * "plain", the DXi *external* range, so the numbers on screen match;
//   * "internal", what the DSP was actually handed, via toInternal(),
//     reproducing ParamInfo::MapToInternal.
//
// Deviations from the DXi table are marked DEVIATION and explained in
// PORTING-NOTES.md section 2. There are four, plus the patch matrix.
//------------------------------------------------------------------------

#pragma once

#include <cstddef>

#include "pluginterfaces/vst/vsttypes.h"

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

//------------------------------------------------------------------------
enum Param : Steinberg::Vst::ParamID
{
	kEnable,             // PARAM_ENABLE

	kSample1,            // PARAM_SAMPLE1
	kSample2,            // PARAM_SAMPLE2
	kSample3,            // PARAM_SAMPLE3
	kSample4,            // PARAM_SAMPLE4
	kSample5,            // PARAM_SAMPLE5

	kInLevel,            // PARAM_INLEVEL           "Src Level"
	kSampLevel,          // PARAM_SAMPLEVEL         "Wav Level"
	kSampThroughLevel,   // PARAM_SAMPTHROUGHLEVEL  "Through"
	kResonance,          // PARAM_RESONANCE
	kFreq,               // PARAM_FREQ              "Bottom Freq"
	kFreqSpread,         // PARAM_FREQSPREAD        "Top Freq"
	kAttack,             // PARAM_ATTACK            "Env Attack"
	kRelease,            // PARAM_RELEASE           "Env Release"
	kEnvBoost,           // PARAM_ENVBOOST          "Env Level"

	kStereo,             // PARAM_STEREO
	kBands,              // PARAM_BANDS9            DEVIATION - enum, see below
	kInterlaced,         // PARAM_INTERLACED
	kRepeatSamp,         // PARAM_REPEATSAMP
	kVoicedDetect,       // PARAM_VOICED
	kVoicedSense,        // PARAM_VOICEDSENSE
	kNoiseLevel,         // PARAM_NOISELEVEL
	kFilterSlopes,       // PARAM_QUALITY           DEVIATION - enum, see below
	kNoiseOverride,      // PARAM_NOISEOVERRIDE
	kNoiseHighFreq,      // PARAM_NOISEHIGH

	kNumDxiParams,       // 25 - everything above came from the DXi

	//--------------------------------------------------------------------
	// NEW. Appended after the DXi's own, never inserted among them: an id
	// that moves loads a saved project's value into the wrong control.
	//--------------------------------------------------------------------
	kOutputTrim = kNumDxiParams,

	kNumScalarParams,

	//--------------------------------------------------------------------
	// The patch matrix, 484 cells, appended last for the same reason.
	//
	//     id = kPatchBase + row * kMaxBands + column
	//
	// ROW is the MODULATOR band and COLUMN is the CARRIER band - that is
	// the order SpyBand.cpp:753 multiplies them in, and the order
	// PatchBoard::OnLButtonDown reads the mouse in (row from x, column
	// from y), so row is the horizontal axis on screen.
	//--------------------------------------------------------------------
	kPatchBase = kNumScalarParams,

	kNumParams = kPatchBase + kPatchCells
};

/** The VST3 bypass, which hosts expect and the DXi had no equivalent of.
    1000 is the convention, and it is far past the end of kParams - so
    RANGE-CHECK every id before indexing the table. */
constexpr Steinberg::Vst::ParamID kBypass = 1000;

/** The id of one patch cell, and the inverse. */
inline Steinberg::Vst::ParamID patchParam (int row, int column)
{
	return static_cast<Steinberg::Vst::ParamID> (kPatchBase + row * kMaxBands + column);
}
inline bool isPatchParam (Steinberg::Vst::ParamID id)
{
	return id >= kPatchBase && id < kNumParams;
}
inline int patchRow    (Steinberg::Vst::ParamID id) { return (id - kPatchBase) / kMaxBands; }
inline int patchColumn (Steinberg::Vst::ParamID id) { return (id - kPatchBase) % kMaxBands; }

//------------------------------------------------------------------------
enum class ParamType { Float, Bool, Enum, Int };

//------------------------------------------------------------------------
struct ParamDef
{
	Steinberg::Vst::ParamID id;
	const char* title;
	const char* units;
	ParamType   type;
	double      plainMin;       // DXi "external" range
	double      plainMax;
	double      plainDefault;
	double      internalMin;    // range the DSP expects
	double      internalMax;
	int         stepCount;      // 0 = continuous
	bool        smoothed;

	//--------------------------------------------------------------------
	double toPlain (double normalized) const
	{
		return plainMin + normalized * (plainMax - plainMin);
	}

	double toNormalized (double plain) const
	{
		if (plainMax == plainMin)
			return 0.0;
		return (plain - plainMin) / (plainMax - plainMin);
	}

	/** Reproduces ParamInfo::MapToInternal from the DXi. */
	double toInternal (double normalized) const
	{
		if (type == ParamType::Bool)
			return (normalized < 0.5) ? 0.0 : 1.0;

		const double v = internalMin + normalized * (internalMax - internalMin);
		if (type == ParamType::Enum || type == ParamType::Int)
			return static_cast<double> (static_cast<long> (v + 0.5));
		return v;
	}

	double defaultNormalized () const { return toNormalized (plainDefault); }
};

//------------------------------------------------------------------------
/** The 26 scalar parameters. The patch cells are NOT in this table - they
    are all identical and are described by patchDef() instead. */
extern const ParamDef kParams[kNumScalarParams];

/** Every patch cell has the same shape; only its title and its default
    differ. */
extern const ParamDef kPatchDef;

/** The default for one cell, normalised: half travel on the diagonal and
    zero everywhere else. The DXi shipped with an EMPTY matrix and so made
    no sound at all until a cell was clicked; the diagonal at 0.5 is what
    CSpyBand::Initialize set in the block that was commented out. See
    DEVIATION 4 in PORTING-NOTES.md. */
double patchDefaultNormalized (int row, int column);

/** Look a definition up by id. Scalar ids are dense, so that half is just
    an index - but kBypass is 1000 and the patch cells are their own thing,
    so this handles all three. */
const ParamDef& paramDef (Steinberg::Vst::ParamID id);

/** "Mod 3 x Car 7". Written into a caller-supplied buffer because there
    are 484 of them and none of them wants to own storage. `outSize` should
    be at least 20. */
void patchTitle (Steinberg::Vst::ParamID id, char* out, std::size_t outSize);

//------------------------------------------------------------------------
// Enumerated parameters
//
// Both of these were MPT_FLOAT 0..100 in the DXi's table while the DSP read
// them as small integers, so the external range said "percent" and the code
// meant "one of four" (SpyBand.cpp:482-506) or "one of three"
// (SpyBand.cpp:466, nFilterSlope). The port makes them what they are.
// DEVIATION 1 and 2 - see PORTING-NOTES.md section 2.
//------------------------------------------------------------------------
extern const char* const kBandsNames[4];        // "9 Bands" .. "22 Bands"
extern const char* const kFilterSlopeNames[3];  // "Shallow" .. "Steep"

/** Band count for a control at `internal` (0..3). */
int bandCount (double internal);

//------------------------------------------------------------------------
// Two-state parameters that read as words, not as On/Off
//
// The dialog labelled each of these with a pair of AddValue() strings
// rather than a checkbox, and the words carry information a bare "On" does
// not - "Use Sample" against "Interlace", say. The host's parameter list
// shows the same words the panel does.
//------------------------------------------------------------------------
extern const char* const kStereoNames[2];         // Mono / Stereo
extern const char* const kInterlacedNames[2];     // Use Sample / Interlace
extern const char* const kRepeatNames[2];         // Single Shot / Repeat Sample
extern const char* const kVoicedNames[2];         // Voiced Det Off / On
extern const char* const kNoiseOverrideNames[2];  // see NOISE OVERRIDE below

/** The two words for a boolean parameter, or nullptr if it is a plain
    Off/On. Index 0 is the false state. */
const char* const* booleanNames (Steinberg::Vst::ParamID id);

//------------------------------------------------------------------------
// Frequency readouts
//
// The dialog showed the bottom band's centre frequency under "Bottom
// Freq", the top band's under "Top Freq" and the noise high-pass corner
// under "Noise HighPass" (SpyBandPropPage.cpp:474-487, reading
// getBassFreq/getTopFreq/getNoiseHighFreq off the DSP through a
// back-channel). VST3 splits the processor and the controller, so the
// editor has to recompute - and these are the shared functions both sides
// call, so the two cannot drift apart. They live beside the parameter
// table because they are functions OF the parameters.
//
// All three take the sample rate, because the DXi's hard-coded 44100 is
// the one thing about them the port does not preserve. See DEVIATION 3.
//------------------------------------------------------------------------

/** Centre frequency of band `index` of `bands`, in Hz, for the bottom-freq
    and spread controls at their internal (0..1) values. This is the
    formula from CSpyBand::setFilterConstantsc, and the DSP calls the same
    function rather than keeping its own copy. */
double bandCentreHz (double freqInternal, double spreadInternal,
                     int bands, int index, double sampleRate);

/** The noise high-pass corner, from CSpyBand::setNoiseHighFilterConstants:
    a quarter of the sample rate, scaled by the control. */
double noiseHighPassHz (double internal, double sampleRate);

/** Envelope follower attack and release in milliseconds, from
    SpyBand.cpp:519-530: attack is control * 250 + 1, release is
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
// 0..100 % maps to silence and then -kOutputRangeDb up to 0 dB, so the TOP
// of the control is the DXi's own staging and nothing is lost.
//------------------------------------------------------------------------
constexpr double kOutputRangeDb = 40.0;

/** Trim control (0..1) -> a linear gain. Zero is silence. */
double outputGain (double control);

/** The same in decibels, for display. Returns -1e9 at silence. */
double outputDecibels (double control);

/** The inverse of outputDecibels, clamped to the control's travel. */
double outputControlFromDb (double db);

//------------------------------------------------------------------------
} // namespace SpyBand
