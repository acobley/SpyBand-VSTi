//------------------------------------------------------------------------
// SpyBand - parameter table checks
//
// The table is generated from the DXi's own Parameters.h, so what these
// assert is that the generation produced something VST3 can live with:
// dense ids, no null strings (see PORTING-NOTES on RangeParameter), ranges
// that contain their defaults, normalisation that round-trips - and that
// the values handed to the DSP are still the values the DXi handed it.
//
//   c++ -std=c++17 -Wall -Wextra -I<vst3sdk> -I../source
//       ../source/SpyBandParams.cpp ../source/BandLayout.cpp
//       ParamTableTests.cpp -o param-tests
//------------------------------------------------------------------------

#include "SpyBandParams.h"

#include <cmath>
#include <cstdio>
#include <string>

using namespace SpyBand;

static int fails = 0;

static void chk (bool ok, const char* what, const char* where)
{
	if (! ok)
	{
		std::printf ("  FAIL [%s]: %s\n", where, what);
		++fails;
	}
}

static void close (double a, double b, const char* what, const char* where)
{
	chk (std::fabs (a - b) < 1e-9, what, where);
}

int main ()
{
	std::printf ("scalar parameters: %d, patch cells: %d, total: %d\n",
	             (int) kNumScalarParams, (int) kPatchCells, (int) kNumParams);

	//--------------------------------------------------------------------
	// Shape
	//--------------------------------------------------------------------
	chk (kNumDxiParams == 25, "the DXi had 25 automated parameters", "count");
	chk (kNumScalarParams == 26, "25 DXi parameters plus the output trim", "count");
	chk (kNumParams == 26 + 484, "26 scalars plus a 22 x 22 matrix", "count");

	for (int i = 0; i < (int) kNumScalarParams; ++i)
	{
		const ParamDef& d = kParams[i];
		const char* where = d.title;

		chk (d.id == (Steinberg::Vst::ParamID) i, "id does not match its index", where);
		chk (d.title && d.title[0], "null or empty title", where);
		chk (d.units != nullptr, "null units (RangeParameter would segfault)", where);
		chk (d.plainMax > d.plainMin, "empty plain range", where);
		chk (d.plainDefault >= d.plainMin && d.plainDefault <= d.plainMax,
		     "default outside plain range", where);

		const double n = d.defaultNormalized ();
		chk (n >= 0.0 && n <= 1.0, "default normalises outside 0..1", where);
		close (d.toPlain (n), d.plainDefault, "toPlain(toNormalized(x)) != x", where);

		if (d.type == ParamType::Bool)
			chk (d.stepCount == 1, "bool without stepCount 1", where);
		if (d.type == ParamType::Float)
			chk (d.stepCount == 0, "float with a stepCount", where);
		if (d.type == ParamType::Enum)
			chk (d.stepCount > 1, "enum without steps", where);

		close (d.toInternal (0.0), d.internalMin, "toInternal(0) != internalMin", where);
	}

	//--------------------------------------------------------------------
	// kBypass is 1000. Indexing kParams with it walks off the end, so
	// paramDef must handle it rather than the caller getting lucky.
	//--------------------------------------------------------------------
	chk (kBypass == 1000, "bypass is not at the conventional id", "bypass");
	chk (! isPatchParam (kBypass), "bypass reads as a patch cell", "bypass");
	chk (paramDef (kBypass).id == kBypass, "paramDef(kBypass) is not the bypass", "bypass");
	chk (paramDef (99999).plainMax == 0.0,
	     "an unknown id returns something that looks usable", "bypass");

	//--------------------------------------------------------------------
	// Patch ids: dense, and round-tripping through row and column.
	//--------------------------------------------------------------------
	for (int row = 0; row < kMaxBands; ++row)
	{
		for (int column = 0; column < kMaxBands; ++column)
		{
			const Steinberg::Vst::ParamID id = patchParam (row, column);
			chk (isPatchParam (id), "patch id not recognised", "patch");
			chk (patchRow (id) == row, "row does not round-trip", "patch");
			chk (patchColumn (id) == column, "column does not round-trip", "patch");
			chk (id >= kNumScalarParams && id < kNumParams, "patch id out of range", "patch");
		}
	}
	chk (patchParam (0, 0) == kNumScalarParams, "the matrix does not start after the scalars", "patch");
	chk (patchParam (kMaxBands - 1, kMaxBands - 1) == kNumParams - 1,
	     "the matrix does not end at the last id", "patch");

	// The diagonal defaults in, everything else out - the block that was
	// commented out in CSpyBand::Initialize.
	for (int row = 0; row < kMaxBands; ++row)
		for (int column = 0; column < kMaxBands; ++column)
			close (patchDefaultNormalized (row, column), (row == column) ? 0.5 : 0.0,
			       "patch default is not the diagonal at half travel", "patch");

	// Half travel must reach the DSP as 1.0: the dialog stored 0.5 and
	// CSpyBand::SetPatchValue doubled it.
	close (kPatchDef.toInternal (0.5), 1.0, "a half-open cell is not 1.0 internally", "patch");
	close (kPatchDef.toInternal (1.0), 2.0, "a fully open cell is not 2.0 internally", "patch");

	//--------------------------------------------------------------------
	// THE VALUES THE DSP RECEIVES
	//
	// This is the check the whole table exists for. Each number on the
	// right is what CSpyBand::Process computed from the DXi's default,
	// worked from Parameters.h and the arithmetic at SpyBand.cpp:519-531.
	//--------------------------------------------------------------------
	const auto internalDefault = [] (Steinberg::Vst::ParamID id)
	{
		const ParamDef& d = paramDef (id);
		return d.toInternal (d.defaultNormalized ());
	};

	close (internalDefault (kInLevel) * 5.0 + 0.00001, 1.00001,
	       "Src Level default is not unity into the DSP", "defaults");
	close (internalDefault (kSampLevel) * 5.0 + 0.00001, 1.00001,
	       "Wav Level default is not unity into the DSP", "defaults");
	close (internalDefault (kSampThroughLevel), 0.0, "Through default is not 0", "defaults");
	close (internalDefault (kResonance), 0.75, "Resonance default is not 0.75", "defaults");
	close (internalDefault (kFreq), 0.01, "Bottom Freq default is not 0.01", "defaults");
	close (internalDefault (kFreqSpread), 0.02, "Top Freq default is not 0.02", "defaults");
	close (internalDefault (kEnvBoost) * 10.0, 2.5, "Env Level default is not 2.5", "defaults");
	close (internalDefault (kVoicedSense), 0.2, "Voiced Sensitivity default is not 0.2", "defaults");
	close (internalDefault (kNoiseLevel), 0.2, "Noise Level default is not 0.2", "defaults");
	close (internalDefault (kNoiseHighFreq), 0.01, "Noise HighPass default is not 0.01", "defaults");
	close (envAttackMs (internalDefault (kAttack)), 13.5,
	       "Env Attack default is not 13.5 ms", "defaults");
	close (envReleaseMs (internalDefault (kRelease)), 6.0,
	       "Env Release default is not 6 ms", "defaults");

	chk (internalDefault (kEnable) >= 0.5, "Enabled does not default on", "defaults");
	chk (internalDefault (kStereo) < 0.5, "Stereo does not default off", "defaults");
	for (Steinberg::Vst::ParamID id : { kSample1, kSample2, kSample3, kSample4, kSample5 })
		chk (internalDefault (id) < 0.5, "a sample slot defaults on", "defaults");

	//--------------------------------------------------------------------
	// DEVIATION 1 and 2: the two enums.
	//
	// The DXi declared both as MPT_FLOAT 0..100 while the DSP read them as
	// small integers. Bands defaulted to 20, which matched none of the
	// four cases, so NumberOfBands kept the 9 its constructor set - and 9
	// is what the enum's default must therefore give.
	//--------------------------------------------------------------------
	chk (bandCount (internalDefault (kBands)) == 9,
	     "Bands does not default to the 9 the DXi actually ran", "enums");
	close (internalDefault (kFilterSlopes), 0.0, "Filter Slopes does not default shallow", "enums");

	for (int i = 0; i < 4; ++i)
	{
		const double norm = kParams[kBands].toNormalized (i);
		chk (bandCount (kParams[kBands].toInternal (norm)) == kBandCounts[i],
		     "a Bands step does not select its band count", "enums");
		chk (kBandsNames[i] != nullptr && kBandsNames[i][0] != '\0',
		     "empty Bands name", "enums");
	}
	for (int i = 0; i < 3; ++i)
		chk (kFilterSlopeNames[i] != nullptr && kFilterSlopeNames[i][0] != '\0',
		     "empty Filter Slopes name", "enums");

	// Every step must be reachable and distinct, or automation lands
	// between two of them.
	for (int i = 0; i < 4; ++i)
	{
		const double norm = kParams[kBands].toNormalized (i);
		close (kParams[kBands].toInternal (norm), i, "a Bands step does not round-trip", "enums");
	}

	//--------------------------------------------------------------------
	// The five two-word switches
	//--------------------------------------------------------------------
	for (Steinberg::Vst::ParamID id :
	     { kStereo, kInterlaced, kRepeatSamp, kVoicedDetect, kNoiseOverride })
	{
		const char* const* names = booleanNames (id);
		chk (names != nullptr, "a switch that had two words in the dialog lost them", "switches");
		if (names)
		{
			chk (names[0] && names[0][0], "empty false-state name", "switches");
			chk (names[1] && names[1][0], "empty true-state name", "switches");
			chk (std::string (names[0]) != std::string (names[1]),
			     "both states read the same", "switches");
		}
	}
	chk (booleanNames (kEnable) == nullptr, "Enabled invented a word pair", "switches");

	//--------------------------------------------------------------------
	// The output trim.
	//
	// The top of the travel must be EXACTLY the DXi's staging, or the port
	// is quieter than the original at full and something has been lost.
	//--------------------------------------------------------------------
	close (outputGain (1.0), 1.0, "the top of the trim is not unity", "trim");
	close (outputDecibels (1.0), 0.0, "the top of the trim is not 0 dB", "trim");
	close (outputGain (0.0), std::pow (10.0, -kOutputRangeDb / 20.0),
	       "the bottom of the trim is not -60 dB", "trim");

	const ParamDef& trim = kParams[kOutputTrim];
	close (trim.plainDefault, -20.0, "the trim does not default to -20 dB", "trim");
	close (outputDecibels (trim.toInternal (trim.defaultNormalized ())), -20.0,
	       "the trim's default does not reach the DSP as -20 dB", "trim");
	close (outputGain (trim.toInternal (trim.defaultNormalized ())), 0.1,
	       "-20 dB is not a tenth", "trim");

	for (double db = -60.0; db <= 0.0; db += 7.5)
		close (outputDecibels (outputControlFromDb (db)), db,
		       "dB does not round-trip through the control", "trim");

	std::printf (fails ? "\n%d FAILURES\n" : "\nall parameter checks passed\n", fails);
	return fails ? 1 : 0;
}
