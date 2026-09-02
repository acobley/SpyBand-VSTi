//------------------------------------------------------------------------
// SpyBand - the parameter table
//
// Generated from the DXi's CMediaParams::m_aParamInfo. Do not hand-edit a
// range or a default here without recording it in PORTING-NOTES.md - the
// whole point of this table is that the DSP receives what it always did.
//------------------------------------------------------------------------

#include "SpyBandParams.h"

#include <cmath>
#include <cstdio>

namespace SpyBand {

const char* const kBandsNames[4] =
	{ "9 Bands", "12 Bands", "18 Bands", "22 Bands" };

const char* const kFilterSlopeNames[3] =
	{ "Shallow Slope", "Medium Slope", "Steep Slope" };

const char* const kStereoNames[2]        = { "Mono", "Stereo" };
const char* const kInterlacedNames[2]    = { "Use Sample", "Interlace" };
const char* const kRepeatNames[2]        = { "Single Shot", "Repeat Sample" };
const char* const kVoicedNames[2]        = { "Voiced Det Off", "Voiced Det On" };
const char* const kNoiseOverrideNames[2] = { "Input Carrier", "Noise Carrier" };

//------------------------------------------------------------------------
// id                    title                    units type                plainMin plainMax default intMin intMax steps smoothed
const ParamDef kParams[kNumScalarParams] =
{
	{ kEnable,           "Enabled",                "",   ParamType::Bool,   0.0,   1.0,   1.0,  0.0,  1.0, 1, false },

	{ kSample1,          "Sample 1",               "",   ParamType::Bool,   0.0,   1.0,   0.0,  0.0,  1.0, 1, false },
	{ kSample2,          "Sample 2",               "",   ParamType::Bool,   0.0,   1.0,   0.0,  0.0,  1.0, 1, false },
	{ kSample3,          "Sample 3",               "",   ParamType::Bool,   0.0,   1.0,   0.0,  0.0,  1.0, 1, false },
	{ kSample4,          "Sample 4",               "",   ParamType::Bool,   0.0,   1.0,   0.0,  0.0,  1.0, 1, false },
	{ kSample5,          "Sample 5",               "",   ParamType::Bool,   0.0,   1.0,   0.0,  0.0,  1.0, 1, false },

	{ kInLevel,          "Src Level",              "%",  ParamType::Float,  0.0, 100.0,  20.0,  0.0,  1.0, 0, true  },
	{ kSampLevel,        "Wav Level",              "%",  ParamType::Float,  0.0, 100.0,  20.0,  0.0,  1.0, 0, true  },
	{ kSampThroughLevel, "Through",                "%",  ParamType::Float,  0.0, 100.0,   0.0,  0.0,  1.0, 0, true  },
	{ kResonance,        "Resonance",              "%",  ParamType::Float,  0.0, 100.0,  75.0,  0.0,  1.0, 0, false },
	{ kFreq,             "Bottom Freq",            "%",  ParamType::Float,  0.0, 100.0,   1.0,  0.0,  1.0, 0, false },
	{ kFreqSpread,       "Top Freq",               "%",  ParamType::Float,  0.0, 100.0,   2.0,  0.0,  1.0, 0, false },
	{ kAttack,           "Env Attack",             "%",  ParamType::Float,  0.0, 100.0,   5.0,  0.0,  1.0, 0, false },
	{ kRelease,          "Env Release",            "%",  ParamType::Float,  0.0, 100.0,   5.0,  0.0,  1.0, 0, false },
	{ kEnvBoost,         "Env Level",              "%",  ParamType::Float,  0.0, 100.0,  25.0,  0.0,  1.0, 0, true  },

	{ kStereo,           "Stereo",                 "",   ParamType::Bool,   0.0,   1.0,   0.0,  0.0,  1.0, 1, false },
	{ kBands,            "Bands",                  "",   ParamType::Enum,   0.0,   3.0,   0.0,  0.0,  3.0, 3, false },
	{ kInterlaced,       "Interlaced",             "",   ParamType::Bool,   0.0,   1.0,   0.0,  0.0,  1.0, 1, false },
	{ kRepeatSamp,       "Repeat Samp",            "",   ParamType::Bool,   0.0,   1.0,   0.0,  0.0,  1.0, 1, false },
	{ kVoicedDetect,     "Voiced Detect",          "",   ParamType::Bool,   0.0,   1.0,   0.0,  0.0,  1.0, 1, false },
	{ kVoicedSense,      "Voiced Sensitivity",     "%",  ParamType::Float,  0.0, 100.0,  20.0,  0.0,  1.0, 0, false },
	{ kNoiseLevel,       "Unvoiced Noise Level",   "%",  ParamType::Float,  0.0, 100.0,  20.0,  0.0,  1.0, 0, true  },
	{ kFilterSlopes,     "Filter Slopes",          "",   ParamType::Enum,   0.0,   2.0,   0.0,  0.0,  2.0, 2, false },
	{ kNoiseOverride,    "Noise Override",         "",   ParamType::Bool,   0.0,   1.0,   0.0,  0.0,  1.0, 1, false },
	{ kNoiseHighFreq,    "Noise High Pass Freq",   "%",  ParamType::Float,  0.0, 100.0,   1.0,  0.0,  1.0, 0, false },

	// NEW - see the OUTPUT TRIM note in SpyBandParams.h. Its units field is
	// empty because toString appends "dB" itself; a parameter whose value
	// string carries its own unit and ALSO declares one renders "-20 dB %".
	{ kOutputTrim,       "Output Trim",            "",   ParamType::Float,  0.0, 100.0, 100.0,  0.0,  1.0, 0, true  },
};

//------------------------------------------------------------------------
// One patch cell.
//
// The DXi's dialog drove a cell from 0.01 to 1.00 in hundredths and stored
// twice that (PatchBoard::OnMouseMove, and the `2.0 * Value` in
// CSpyBand::SetPatchValue), so the internal range is 0..2 and the external
// one reads as a percentage of a fully-open cell.
//------------------------------------------------------------------------
const ParamDef kPatchDef =
	{ kPatchBase,        "Patch",                  "%",  ParamType::Float,  0.0, 100.0,   0.0,  0.0,  2.0, 0, true  };

// Returned for an id that is neither scalar nor a patch cell, so that a
// missed range check produces a visibly empty parameter rather than a
// plausible wrong one.
static const ParamDef kInvalidDef =
	{ 0,                 "",                       "",   ParamType::Float,  0.0,   0.0,   0.0,  0.0,  0.0, 0, false };

static const ParamDef kBypassDef =
	{ kBypass,           "Bypass",                 "",   ParamType::Bool,   0.0,   1.0,   0.0,  0.0,  1.0, 1, false };

//------------------------------------------------------------------------
const ParamDef& paramDef (Steinberg::Vst::ParamID id)
{
	if (id < kNumScalarParams)
		return kParams[id];
	if (isPatchParam (id))
		return kPatchDef;
	if (id == kBypass)
		return kBypassDef;
	return kInvalidDef;
}

//------------------------------------------------------------------------
void patchTitle (Steinberg::Vst::ParamID id, char* out, std::size_t outSize)
{
	if (out == nullptr || outSize == 0)
		return;
	if (! isPatchParam (id))
	{
		out[0] = '\0';
		return;
	}
	std::snprintf (out, outSize, "Mod %d x Car %d",
	               patchRow (id) + 1, patchColumn (id) + 1);
}

//------------------------------------------------------------------------
// The diagonal is the default, at half travel.
//
// The DXi shipped with an EMPTY matrix - PatchBoard's constructor zeroes
// iPatchBoard - so the plug-in made no sound at all until the user clicked
// a cell. The diagonal at 0.5 is not an invention: it is exactly what
// CSpyBand::Initialize sets in the block that was commented out
// (SpyBand.cpp:137-147), and 0.5 is the value a cell takes when it is
// first switched on (iOldPatchBoard, PatchBoard.cpp:24). See DEVIATION 4.
//------------------------------------------------------------------------
double patchDefaultNormalized (int row, int column)
{
	return (row == column) ? 0.5 : 0.0;
}

//------------------------------------------------------------------------
const char* const* booleanNames (Steinberg::Vst::ParamID id)
{
	switch (id)
	{
		case kStereo:         return kStereoNames;
		case kInterlaced:     return kInterlacedNames;
		case kRepeatSamp:     return kRepeatNames;
		case kVoicedDetect:   return kVoicedNames;
		case kNoiseOverride:  return kNoiseOverrideNames;
		default:              return nullptr;
	}
}

//------------------------------------------------------------------------
int bandCount (double internal)
{
	int index = static_cast<int> (internal + 0.5);
	if (index < 0)
		index = 0;
	if (index > 3)
		index = 3;
	return kBandCounts[index];
}

//------------------------------------------------------------------------
// The band layout, transcribed from CSpyBand::setFilterConstantsc.
//
// The only change is that the sample rate is an argument rather than the
// literal 44100.0 the DXi used - see DEVIATION 3. At 44100 this returns
// what the DXi returned, bit for bit, which BandLayoutTests asserts.
//------------------------------------------------------------------------
double bandCentreHz (double freqInternal, double spreadInternal,
                     int bands, int index, double sampleRate)
{
	if (bands <= 0)
		return 0.0;

	const double log2 = std::log (2.0);

	double baseFreq = 25.0 * std::pow (2.0, (48.0 * freqInternal / 12.0));
	double topFreq  = 25.0 * std::pow (2.0, (108.0 * spreadInternal / 12.0)) + baseFreq;

	if (topFreq > (sampleRate / 2.0))
		topFreq = sampleRate / 2.0;

	const double ns       = 12.0 * (std::log (topFreq / baseFreq) / log2);
	const double stepSize = ns / bands;

	return baseFreq * std::pow (2.0, (stepSize * (index + 0.5) / 12.0));
}

//------------------------------------------------------------------------
double noiseHighPassHz (double internal, double sampleRate)
{
	return sampleRate / 4.0 * internal;
}

//------------------------------------------------------------------------
double envAttackMs  (double internal) { return internal * 250.0 + 1.0; }
double envReleaseMs (double internal) { return internal * 100.0 + 1.0; }

//------------------------------------------------------------------------
double outputGain (double control)
{
	if (control <= 0.0)
		return 0.0;
	return std::pow (10.0, (kOutputRangeDb * (control - 1.0)) / 20.0);
}

double outputDecibels (double control)
{
	if (control <= 0.0)
		return -1e9;
	return kOutputRangeDb * (control - 1.0);
}

double outputControlFromDb (double db)
{
	double control = 1.0 + db / kOutputRangeDb;
	if (control < 0.0)
		control = 0.0;
	if (control > 1.0)
		control = 1.0;
	return control;
}

//------------------------------------------------------------------------
} // namespace SpyBand
