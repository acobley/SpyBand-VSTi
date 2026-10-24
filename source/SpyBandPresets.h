//------------------------------------------------------------------------
// SpyBand - presets, matrix patterns and the state format
//
// Everything the header's preset bar and the matrix panel's pattern
// buttons do that is not drawing. No VSTGUI and no SDK classes - only the
// parameter enum - so all of it is tested by tests/PresetTests.cpp.
//
// THE STATE FORMAT. A user preset is a .vstpreset file whose component
// chunk is byte for byte what SpyBandProcessor::getState writes, so a host
// that reads presets from the standard folder can load it too, and the
// processor's setState reads it like a saved project. encodeState and
// decodeState are that format, written out once more here because the
// controller has to produce it without the processor; PresetTests checks
// the layout field by field against the processor's description.
//------------------------------------------------------------------------

#pragma once

#include "SpyBandParams.h"
#include "Vocoder.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace SpyBand {

//------------------------------------------------------------------------
/** A whole plug-in setting: every parameter, normalised, plus bypass and
    the five sample paths. */
struct Patch
{
	double values[kNumParams];
	bool bypass = false;
	std::string paths[Vocoder::kNumSlots];

	/** Every parameter at its default, the matrix on its diagonal at
	    50 %, no samples. */
	static Patch defaults ();

	/** The 22 x 22 matrix, row-major as patchParam() lays it out. */
	double* patch () { return values + kPatchBase; }
	const double* patch () const { return values + kPatchBase; }
};

//------------------------------------------------------------------------
// The processor's state stream
//------------------------------------------------------------------------
constexpr std::int32_t kStateVersion = 2;   // must match SpyBandProcessor.cpp
constexpr std::int32_t kMaxStatePathBytes = 8192;

std::vector<std::uint8_t> encodeState (const Patch& patch);

/** Read a stream written by encodeState or SpyBandProcessor::getState.
    A short stream leaves what it does not mention at the default, as the
    processor does. False on anything malformed. */
bool decodeState (const std::uint8_t* data, std::size_t size, Patch& out);

//------------------------------------------------------------------------
// Factory presets
//
// They exist because of the factory DEFAULTS: the DXi put all nine bands
// between 26.8 Hz and 51.8 Hz, which is nearly silent, and the defaults
// are kept as the DXi had them. Classic 9-Band - Bottom 50 %, Top 92 %,
// roughly 100 Hz to 7.9 kHz - is the setting anyone should meet first.
//
// A factory preset sets the sound and leaves the SAMPLES alone: which
// files are loaded and which slots are on is the user's, not the preset's.
//------------------------------------------------------------------------
int factoryPresetCount ();
const char* factoryPresetName (int index);

/** The preset as a Patch. Its paths are empty and the slot enables are
    at their defaults; see keepsSamples(). */
Patch factoryPreset (int index);

/** True for parameters a factory preset does not touch: the five slot
    enables and the DXi's Enabled switch. */
bool keepsSamples (Steinberg::Vst::ParamID id);

//------------------------------------------------------------------------
// Matrix patterns - the four buttons under the matrix
//
// All four work on the bands in use; cells beyond them are cleared,
// because they are not read and a pin left there would reappear,
// unexplained, the next time the band count went up.
//------------------------------------------------------------------------
enum class Pattern
{
	Diagonal,   // band n to band n, at 50 % - the default matrix
	Reverse,    // band n to band (bands - 1 - n): low drives high
	Shift,      // every pin moves up one carrier band, wrapping
	Clear       // nothing patched
};

/** Apply `pattern` to a 22 x 22 matrix of normalised cell values. */
void applyPattern (Pattern pattern, int bands, double* matrix);

/** RE-FIT THE MATRIX TO A NEW BAND COUNT, when the user turns the BANDS
    selector from `from` bands to `to`. Without it, going from 9 to 22 left
    the pattern in the top-left 9 x 9 and thirteen bands unpatched - and
    silent, since a band with no pin is not heard.

    If the bands in use hold a complete pattern - every row pinned on a
    diagonal or a reverse diagonal, shifted any number of carrier bands
    (what DIAGONAL, REVERSE and SHIFT make) - that pattern is laid out
    again at the new count, with the same shift. Each new pin takes the
    level of the old pin nearest it in relative position, so a diagonal
    at 100 % stays at 100 %. Any other pins - extras beside a pattern, or
    a matrix that is no pattern at all - move to the cell at the same
    relative position (row and column scaled by (to - 1) / (from - 1)),
    keeping their level; where two land on one cell the louder wins.
    Cells beyond `to` are cleared, as the pattern buttons do. An empty
    matrix stays empty.

    The editor calls this only for the user's own turn of the selector:
    a preset or a project brings its own matrix with its band count, and
    host automation of the band count leaves the matrix alone. */
void refitMatrix (int from, int to, double* matrix);

/** The panel's "11 PINS / 2 OFF-DIAG": open cells within the bands in
    use, and how many of them are not on the diagonal. */
int pinCount (const double* matrix, int bands);
int offDiagonalCount (const double* matrix, int bands);

//------------------------------------------------------------------------
} // namespace SpyBand
