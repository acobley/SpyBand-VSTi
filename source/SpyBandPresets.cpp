//------------------------------------------------------------------------
// SpyBand - presets, matrix patterns and the state format
//------------------------------------------------------------------------

#include "SpyBandPresets.h"
#include "BandLayout.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace SpyBand {

using Steinberg::Vst::ParamID;

//------------------------------------------------------------------------
Patch Patch::defaults ()
{
	Patch p;
	for (ParamID id = 0; id < kNumScalarParams; ++id)
		p.values[id] = kParams[id].defaultNormalized ();
	for (int row = 0; row < kMaxBands; ++row)
		for (int column = 0; column < kMaxBands; ++column)
			p.values[patchParam (row, column)] = patchDefaultNormalized (row, column);
	return p;
}

//------------------------------------------------------------------------
// The stream, little-endian throughout:
//
//   int32  version (1)
//   int32  count of parameter values that follow (kNumParams)
//   double value[count]           normalised, in ParamID order
//   int32  bypass (0 or 1)
//   int32  slot count (5)
//   per slot: int32 length, then that many bytes of path, no terminator
//------------------------------------------------------------------------
namespace {

void putInt32 (std::vector<std::uint8_t>& out, std::int32_t v)
{
	const std::uint32_t u = static_cast<std::uint32_t> (v);
	for (int i = 0; i < 4; ++i)
		out.push_back (static_cast<std::uint8_t> ((u >> (8 * i)) & 0xFF));
}

void putDouble (std::vector<std::uint8_t>& out, double v)
{
	std::uint64_t u = 0;
	std::memcpy (&u, &v, sizeof (u));
	for (int i = 0; i < 8; ++i)
		out.push_back (static_cast<std::uint8_t> ((u >> (8 * i)) & 0xFF));
}

struct Reader
{
	const std::uint8_t* p;
	const std::uint8_t* end;

	bool int32 (std::int32_t& v)
	{
		if (end - p < 4)
			return false;
		std::uint32_t u = 0;
		for (int i = 0; i < 4; ++i)
			u |= static_cast<std::uint32_t> (p[i]) << (8 * i);
		v = static_cast<std::int32_t> (u);
		p += 4;
		return true;
	}

	bool real (double& v)
	{
		if (end - p < 8)
			return false;
		std::uint64_t u = 0;
		for (int i = 0; i < 8; ++i)
			u |= static_cast<std::uint64_t> (p[i]) << (8 * i);
		std::memcpy (&v, &u, sizeof (v));
		p += 8;
		return true;
	}
};

} // namespace

std::vector<std::uint8_t> encodeState (const Patch& patch)
{
	std::vector<std::uint8_t> out;
	out.reserve (16 + 8 * kNumParams + 64);

	putInt32 (out, kStateVersion);
	putInt32 (out, static_cast<std::int32_t> (kNumParams));
	for (ParamID id = 0; id < kNumParams; ++id)
		putDouble (out, patch.values[id]);

	putInt32 (out, patch.bypass ? 1 : 0);
	putInt32 (out, Vocoder::kNumSlots);
	for (const std::string& path : patch.paths)
	{
		const std::size_t length =
			path.size () > static_cast<std::size_t> (kMaxStatePathBytes)
				? static_cast<std::size_t> (kMaxStatePathBytes) : path.size ();
		putInt32 (out, static_cast<std::int32_t> (length));
		out.insert (out.end (), path.begin (), path.begin () + static_cast<std::ptrdiff_t> (length));
	}
	return out;
}

bool decodeState (const std::uint8_t* data, std::size_t size, Patch& out)
{
	if (data == nullptr)
		return false;

	Reader r { data, data + size };
	Patch p = Patch::defaults ();

	std::int32_t version = 0, count = 0;
	if (! r.int32 (version) || version < 1 || version > kStateVersion)
		return false;
	if (! r.int32 (count) || count < 0 || count > static_cast<std::int32_t> (kNumParams))
		return false;

	for (std::int32_t i = 0; i < count; ++i)
	{
		double v = 0.0;
		if (! r.real (v))
			return false;
		if (v < 0.0)
			v = 0.0;
		if (v > 1.0)
			v = 1.0;
		p.values[i] = v;
	}

	// Version 1's trim stopped at 0 dB; keep its decibels.
	if (version < 2 && count > static_cast<std::int32_t> (kOutputTrim))
		p.values[kOutputTrim] = outputControlFromV1State (p.values[kOutputTrim]);

	std::int32_t bypass = 0, slots = 0;
	if (! r.int32 (bypass) || ! r.int32 (slots) || slots < 0)
		return false;
	p.bypass = (bypass != 0);

	for (std::int32_t s = 0; s < slots; ++s)
	{
		std::int32_t length = 0;
		if (! r.int32 (length) || length < 0 || length > kMaxStatePathBytes)
			return false;
		if (r.end - r.p < length)
			return false;
		if (s < Vocoder::kNumSlots)
			p.paths[s].assign (reinterpret_cast<const char*> (r.p),
			                   static_cast<std::size_t> (length));
		r.p += length;
	}

	out = p;
	return true;
}

//------------------------------------------------------------------------
// Factory presets
//------------------------------------------------------------------------
namespace {

/** One parameter, in its PLAIN units - the numbers on the panel. */
struct Setting
{
	ParamID id;
	double plain;
};

struct FactoryDef
{
	const char* name;
	Setting settings[8];
	int settingCount;
	Pattern pattern;
	int shifts;      // extra Shift presses after the pattern
};

// Bands and Filter Slopes are enums: the plain value is the index
// (9, 12, 18, 22 bands -> 0..3; Shallow, Medium, Steep -> 0..2).
const FactoryDef kFactory[] =
{
	{ "Classic 9-Band",
	  { { kFreq, 50.0 }, { kFreqSpread, 92.0 } }, 2,
	  Pattern::Diagonal, 0 },

	{ "Wide 12-Band",
	  { { kFreq, 45.0 }, { kFreqSpread, 95.0 }, { kBands, 1.0 }, { kFilterSlopes, 1.0 } }, 4,
	  Pattern::Diagonal, 0 },

	{ "Fine 22-Band",
	  { { kFreq, 42.0 }, { kFreqSpread, 96.0 }, { kBands, 3.0 }, { kFilterSlopes, 2.0 },
	    { kResonance, 90.0 } }, 5,
	  Pattern::Diagonal, 0 },

	{ "Upside Down",
	  { { kFreq, 50.0 }, { kFreqSpread, 92.0 } }, 2,
	  Pattern::Reverse, 0 },

	{ "Shifted Up",
	  { { kFreq, 50.0 }, { kFreqSpread, 92.0 }, { kBands, 1.0 } }, 3,
	  Pattern::Diagonal, 1 },

	{ "Interlace Live",
	  { { kFreq, 50.0 }, { kFreqSpread, 92.0 }, { kBands, 1.0 }, { kInterlaced, 1.0 } }, 4,
	  Pattern::Diagonal, 0 },

	{ "Noise Whisper",
	  { { kFreq, 48.0 }, { kFreqSpread, 94.0 }, { kBands, 2.0 }, { kFilterSlopes, 1.0 },
	    { kNoiseOverride, 1.0 } }, 5,
	  Pattern::Diagonal, 0 },

	// The DXi's own defaults, named for what they are. Nearly silent until
	// Bottom and Top are moved - kept so that nobody has to wonder what the
	// original sounded like out of the box.
	{ "DXi Defaults",
	  { { kFreq, 1.0 } }, 0,
	  Pattern::Diagonal, 0 },
};

constexpr int kFactoryCount = static_cast<int> (sizeof (kFactory) / sizeof (kFactory[0]));

int bandsForPatch (const Patch& p)
{
	return bandCount (paramDef (kBands).toInternal (p.values[kBands]));
}

} // namespace

int factoryPresetCount ()
{
	return kFactoryCount;
}

const char* factoryPresetName (int index)
{
	if (index < 0 || index >= kFactoryCount)
		return "";
	return kFactory[index].name;
}

Patch factoryPreset (int index)
{
	Patch p = Patch::defaults ();
	if (index < 0 || index >= kFactoryCount)
		return p;

	const FactoryDef& def = kFactory[index];
	for (int i = 0; i < def.settingCount; ++i)
		p.values[def.settings[i].id] = paramDef (def.settings[i].id).toNormalized (def.settings[i].plain);

	const int bands = bandsForPatch (p);
	applyPattern (def.pattern, bands, p.patch ());
	for (int i = 0; i < def.shifts; ++i)
		applyPattern (Pattern::Shift, bands, p.patch ());
	return p;
}

bool keepsSamples (ParamID id)
{
	return id == kEnable || (id >= kSample1 && id <= kSample5);
}

//------------------------------------------------------------------------
// Patterns
//------------------------------------------------------------------------
void applyPattern (Pattern pattern, int bands, double* m)
{
	if (m == nullptr)
		return;
	if (bands < 1)
		bands = 1;
	if (bands > kMaxBands)
		bands = kMaxBands;

	auto at = [m] (int row, int column) -> double& { return m[row * kMaxBands + column]; };
	constexpr double kHalf = 0.5;

	switch (pattern)
	{
		case Pattern::Clear:
		case Pattern::Diagonal:
		case Pattern::Reverse:
		{
			for (int i = 0; i < kPatchCells; ++i)
				m[i] = 0.0;
			if (pattern == Pattern::Diagonal)
				for (int b = 0; b < bands; ++b)
					at (b, b) = kHalf;
			else if (pattern == Pattern::Reverse)
				for (int b = 0; b < bands; ++b)
					at (b, bands - 1 - b) = kHalf;
			break;
		}

		case Pattern::Shift:
		{
			double next[kPatchCells] = { 0.0 };
			for (int row = 0; row < bands; ++row)
				for (int column = 0; column < bands; ++column)
					next[row * kMaxBands + (column + 1) % bands] = at (row, column);
			for (int i = 0; i < kPatchCells; ++i)
				m[i] = next[i];
			break;
		}
	}
}

//------------------------------------------------------------------------
// Re-fitting to a new band count
//------------------------------------------------------------------------
namespace {

/** Where row `r` of a pattern pins: diagonal or reverse, shifted `k`. */
int patternColumn (bool reverse, int shift, int row, int bands)
{
	const int base = reverse ? bands - 1 - row : row;
	return (base + shift) % bands;
}

/** The index in a count of `to` at the same relative position as `i` in
    a count of `from`. */
int scaled (int i, int from, int to)
{
	if (from <= 1 || to <= 1)
		return 0;
	const double t = static_cast<double> (i) * (to - 1) / (from - 1);
	return std::clamp (static_cast<int> (std::lround (t)), 0, to - 1);
}

} // namespace

void refitMatrix (int from, int to, double* m)
{
	if (m == nullptr)
		return;
	from = std::clamp (from, 1, kMaxBands);
	to = std::clamp (to, 1, kMaxBands);
	if (from == to)
		return;

	auto at = [m] (int row, int column) -> double& { return m[row * kMaxBands + column]; };

	// Is there a complete pattern in the old bands? Diagonals first, then
	// reverses, each from no shift upwards.
	bool found = false, reverse = false;
	int shift = 0;
	for (int r = 0; r < 2 && ! found; ++r)
		for (int k = 0; k < from && ! found; ++k)
		{
			bool all = true;
			for (int row = 0; row < from && all; ++row)
				all = at (row, patternColumn (r == 1, k, row, from)) > 0.0;
			if (all)
			{
				found = true;
				reverse = (r == 1);
				shift = k;
			}
		}

	double next[kPatchCells] = { 0.0 };
	auto put = [&next] (int row, int column, double v) {
		double& cell = next[row * kMaxBands + column];
		cell = std::max (cell, v);
	};

	if (found)
	{
		// The pattern again at the new count, each pin at the level of the
		// old pin nearest it.
		for (int row = 0; row < to; ++row)
		{
			const int old = scaled (row, to, from);
			put (row, patternColumn (reverse, shift % to, row, to),
			     at (old, patternColumn (reverse, shift, old, from)));
		}
	}

	// Every other pin, moved to the same relative position.
	for (int row = 0; row < from; ++row)
		for (int column = 0; column < from; ++column)
		{
			const double v = at (row, column);
			if (v <= 0.0)
				continue;
			if (found && column == patternColumn (reverse, shift, row, from))
				continue;
			put (scaled (row, from, to), scaled (column, from, to), v);
		}

	for (int i = 0; i < kPatchCells; ++i)
		m[i] = next[i];
}

int pinCount (const double* m, int bands)
{
	if (m == nullptr)
		return 0;
	int n = 0;
	for (int row = 0; row < bands && row < kMaxBands; ++row)
		for (int column = 0; column < bands && column < kMaxBands; ++column)
			if (m[row * kMaxBands + column] > 0.0)
				++n;
	return n;
}

int offDiagonalCount (const double* m, int bands)
{
	if (m == nullptr)
		return 0;
	int n = 0;
	for (int row = 0; row < bands && row < kMaxBands; ++row)
		for (int column = 0; column < bands && column < kMaxBands; ++column)
			if (row != column && m[row * kMaxBands + column] > 0.0)
				++n;
	return n;
}

//------------------------------------------------------------------------
} // namespace SpyBand
