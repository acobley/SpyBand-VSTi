//------------------------------------------------------------------------
// SpyBand - the Design2 panel's arithmetic
//
// Everything the new editor computes that is not drawing: the lettering's
// measurements, the band edges and responses the BANDS display plots, the
// matrix patterns, the factory presets and the state stream user presets
// carry. None of it needs a window; it needs the SDK's headers only for
// the parameter ids.
//
//   c++ -std=c++17 -Wall -Wextra -I<vst3sdk> -I../source
//       ../source/SciFiLettering.cpp ../source/SpyBandPresets.cpp
//       ../source/SpyBandParams.cpp ../source/BandLayout.cpp
//       PanelTests.cpp -o panel-tests
//------------------------------------------------------------------------

#include "BandLayout.h"
#include "SciFiLettering.h"
#include "SpyBandParams.h"
#include "SpyBandPresets.h"
#include "SpyBandLayout.h"
#include "SpyBandWear.h"

#include <algorithm>
#include <vector>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

using namespace SpyBand;

static int fails = 0;
static int checks = 0;

static void chk (bool ok, const char* what, const char* where)
{
	++checks;
	if (! ok)
	{
		std::printf ("  FAIL [%s]: %s\n", where, what);
		++fails;
	}
}

static void near (double a, double b, double tol, const char* what, const char* where)
{
	++checks;
	if (std::fabs (a - b) > tol)
	{
		std::printf ("  FAIL [%s]: %s (%.6g, wanted %.6g)\n", where, what, a, b);
		++fails;
	}
}

//------------------------------------------------------------------------
static void letteringTests ()
{
	using namespace SciFi;
	const char* where = "lettering";

	// Every label the editor draws must be in the face, or it comes out as
	// a row of dots. These are the strings with the unusual characters.
	const char* const kLabels[] = {
		"MODULATOR", "INPUT CHECK", "MATRIX", "OUTPUT", "SOURCE", "PLAYBACK",
		"SAMPLES", "RIGHT INPUT", "SINGLE SHOT", "REPEAT", "WAV LEVEL", "ENV LEVEL",
		"ATTACK", "RELEASE", "SRC LEVEL", "BOTTOM", "TOP", "RESONANCE", "SHALLOW",
		"MED", "STEEP", "SENSITIVITY", "UNVOICED", "HIGH PASS", "DETECT", "CARRIER",
		"INPUT", "NOISE", "THROUGH", "TRIM", "OUT", "DIAGONAL", "REVERSE", "SHIFT",
		"CLEAR", "CLICK = PIN", "100 %", "50 %", "OPEN", "BYPASS", "SAVE",
		"20 % UNITY", "\xC3\x97 2.5", "13.5 MS", "7.87 KHZ", "-20.0 DB",
		"9 \xC3\x97 9 OF 22", "11 PINS", "2 OFF-DIAG", "CORR 0.12", "CORR --",
		"SAME AUDIO ON L AND R", "LIGHTS WHEN L AND R CARRY ONE SIGNAL",
		"EMPTY \xE2\x80\x93 DROP A .WAV HERE", "FACTORY / CLASSIC 9-BAND \xE2\x8C\x84",
		"V1.1.0 \xC2\xB7 AE COBLEY", "\xE2\x80\xB9", "\xE2\x80\xBA",
		"AMBER = MODULATOR ENVELOPES \xC3\x97 ROW SUMS \xE2\x80\x94 NOT A METER",
		"9 BANDS \xC2\xB7 SHALLOW", "BOTTOM 100 HZ \xC2\xB7 DRAG \xC2\xB7 SHIFT = FINE",
		"ROWS: MODULATOR BAND (AMBER)", "1.44K", "6.18K", "MISSING",
		// The black panel's
		"RIGHT IN", "ONCE", "THE .WAV FILES", "CARRIER FILTERS + MODULATOR ENVELOPES",
		"MODULATOR BAND \xE2\x80\xBA CARRIER BAND", "WHAT ARRIVES, BEFORE ANY LEVEL CONTROL",
		"EMPTY \xE2\x80\x93 DROP A .WAV", "AMBER = MODULATOR ENVELOPES \xE2\x80\x94 NOT A METER",
		"ROWS: MODULATOR BAND \xC2\xB7 COLUMNS: CARRIER BAND \xC2\xB7 CLICK = PIN \xC2\xB7 DRAG = LEVEL",
		"+6.0 DB", "DSP RATE 48.0 KHZ", "MONO", "STEREO", "PROCESSING",
	};
	for (const char* label : kLabels)
		for (char32_t c : decodeUtf8 (label))
			chk (hasGlyph (c), (std::string ("no glyph in: ") + label).c_str (), where);

	chk (hasGlyph (U'a'), "lowercase is drawn as capitals", where);

	// Measurements against the lettering skill's rbPaths arithmetic.
	Style s;
	s.cap = 6.0;              // one grid unit per pixel
	s.weight = 1.0;
	s.tracking = 1.0;
	near (textWidth ("I", s), 1.0, 1e-9, "I is one stroke wide", where);
	near (textWidth ("H", s), 5.0, 1e-9, "H is 4 units plus a stroke", where);
	near (textWidth ("HH", s), 11.0, 1e-9, "two letters: 4 + 1 + 1 + 4 + 1", where);
	near (textWidth ("H H", s), 5.0 + 1.0 + kSpace + 5.0, 1e-9, "a word space: no tracking of its own", where);
	near (textWidth ("", s), 0.0, 1e-9, "empty is nothing", where);

	// The measured mock-up: MODULATOR at cap 9.3, bold, is about 90 px.
	Style heading;
	heading.cap = 9.3;
	heading.weight = kBold;
	near (textWidth ("MODULATOR", heading), 88.0, 4.0, "heading width matches the mock-up", where);

	// Layout puts ink where the measurement says.
	const auto segs = layout ("H", s, 10.0, 20.0);
	chk (! segs.empty () && segs.front ().op == Op::Move, "a glyph starts with a move", where);
	double minX = 1e9, maxX = -1e9, minY = 1e9, maxY = -1e9;
	for (const Segment& g : segs)
	{
		if (g.op == Op::Close)
			continue;
		minX = std::min (minX, g.x); maxX = std::max (maxX, g.x);
		minY = std::min (minY, g.y); maxY = std::max (maxY, g.y);
	}
	near (minX, 10.5, 1e-9, "stroke centre is half a stroke in from the left", where);
	near (maxX, 14.5, 1e-9, "and half a stroke in from the right", where);
	near (minY, 20.5, 1e-9, "and from the top", where);
	near (maxY, 26.5, 1e-9, "cap height below that", where);

	// UTF-8 decoding, including a malformed byte.
	const std::u32string d = decodeUtf8 ("A\xC3\x97\xE2\x80\x94\xFF");
	chk (d.size () == 4 && d[0] == U'A' && d[1] == U'×' && d[2] == U'—' && d[3] == U'�',
	     "UTF-8 decodes, and a bad byte is a replacement character", where);
}

//------------------------------------------------------------------------
static void bandTests ()
{
	const char* where = "bands";

	// The edges are the numbers the mock-up shows for 50 % / 92 %.
	near (bandBottomHz (0.5), 100.0, 1e-9, "Bottom 50 % is 100 Hz", where);
	near (bandTopHz (0.5, 0.92, 44100.0), 7872.0, 2.0, "Top 92 % is 7.87 kHz", where);
	near (bandTopHz (0.5, 1.0, 20000.0), 10000.0, 1e-9, "Top clamps at Nyquist", where);

	// bandCentreHz is still the DXi's: centres sit inside the edges and
	// split the span evenly in octaves.
	const double lo = bandBottomHz (0.5), hi = bandTopHz (0.5, 0.92, 44100.0);
	for (int b = 0; b < 9; ++b)
	{
		const double c = bandCentreHz (0.5, 0.92, 9, b, 44100.0);
		near (c, lo * std::pow (hi / lo, (b + 0.5) / 9.0), 1e-6, "centre is the octave midpoint", where);
	}
	near (bandCentreHz (0.5, 0.92, 9, 0, 44100.0), 127.0, 1.0, "first band 127 Hz, as labelled", where);
	near (bandCentreHz (0.5, 0.92, 9, 8, 44100.0), 6180.0, 15.0, "last band 6.18 kHz, as labelled", where);

	// The drag inverses.
	for (double v : { 0.0, 0.1, 0.5, 0.73, 1.0 })
		near (freqForBottomHz (bandBottomHz (v)), v, 1e-9, "Bottom round-trips", where);
	for (double v : { 0.05, 0.5, 0.92 })
		near (spreadForTopHz (bandTopHz (0.4, v, 96000.0), 0.4), v, 1e-9, "Top round-trips", where);
	near (freqForBottomHz (5.0), 0.0, 1e-12, "below range clamps", where);
	near (freqForBottomHz (1e6), 1.0, 1e-12, "above range clamps", where);

	// The response peaks at 1 at the centre, falls away, and is steeper
	// with each extra section.
	const double c = 1000.0;
	near (bandResponse (c, c, 0.75, 0, 44100.0), 1.0, 1e-6, "peak is unity", where);
	const double oct = bandResponse (2 * c, c, 0.75, 0, 44100.0);
	chk (oct < 0.5 && oct > 0.0, "an octave out is well down", where);
	chk (bandResponse (2 * c, c, 0.75, 1, 44100.0) < oct, "Medium is steeper", where);
	chk (bandResponse (2 * c, c, 0.75, 2, 44100.0) < bandResponse (2 * c, c, 0.75, 1, 44100.0),
	     "Steep is steeper still", where);
	chk (bandResponse (2 * c, c, 1.0, 0, 44100.0) > bandResponse (2 * c, c, 0.5, 0, 44100.0),
	     "more Resonance is a wider band (it is a bandwidth)", where);
	near (bandResponse (c, c, 0.0, 0, 44100.0), 0.0, 1e-12, "zero bandwidth passes nothing, as the DSP does", where);
}

//------------------------------------------------------------------------
static void patternTests ()
{
	const char* where = "patterns";
	double m[kPatchCells];

	for (double& v : m)
		v = 0.3;
	applyPattern (Pattern::Diagonal, 9, m);
	chk (pinCount (m, 9) == 9 && offDiagonalCount (m, 9) == 0, "diagonal: nine pins on it", where);
	chk (m[0] == 0.5 && m[8 * kMaxBands + 8] == 0.5, "at 50 %, the default", where);
	chk (m[9 * kMaxBands + 9] == 0.0, "nothing left beyond the bands in use", where);

	applyPattern (Pattern::Reverse, 9, m);
	chk (pinCount (m, 9) == 9 && offDiagonalCount (m, 9) == 8, "reverse: nine pins, one on the diagonal", where);
	chk (m[0 * kMaxBands + 8] == 0.5 && m[8 * kMaxBands + 0] == 0.5, "low drives high", where);

	applyPattern (Pattern::Diagonal, 12, m);
	m[3 * kMaxBands + 3] = 0.9;
	applyPattern (Pattern::Shift, 12, m);
	chk (m[3 * kMaxBands + 4] == 0.9, "shift moves a pin up one carrier band, keeping its value", where);
	chk (m[11 * kMaxBands + 0] == 0.5, "and wraps at the top", where);
	chk (pinCount (m, 12) == 12, "without losing any", where);

	applyPattern (Pattern::Clear, 22, m);
	chk (pinCount (m, 22) == 0, "clear empties it", where);

	// The default matrix IS the diagonal.
	Patch d = Patch::defaults ();
	double diag[kPatchCells];
	for (int i = 0; i < kPatchCells; ++i)
		diag[i] = d.patch ()[i];
	applyPattern (Pattern::Diagonal, 22, diag);
	chk (std::memcmp (diag, d.patch (), sizeof (diag)) == 0, "Diagonal at 22 is the default matrix", where);

	//--- re-fitting to a new band count --------------------------------
	auto cell = [] (const double* x, int row, int column) { return x[row * kMaxBands + column]; };

	// 9 to 22: the user's case. The diagonal carries on to every band.
	applyPattern (Pattern::Diagonal, 9, m);
	refitMatrix (9, 22, m);
	chk (pinCount (m, 22) == 22 && offDiagonalCount (m, 22) == 0, "9 -> 22: a diagonal stays a full diagonal", where);
	chk (cell (m, 21, 21) == 0.5, "the new pins at the pattern's level", where);

	// And back down: 22 to 9, still a diagonal, nothing left beyond 9.
	refitMatrix (22, 9, m);
	chk (pinCount (m, 9) == 9 && offDiagonalCount (m, 9) == 0, "22 -> 9: still the diagonal", where);
	chk (pinCount (m, 22) == 9, "and no pins left beyond the bands in use", where);

	// Reverse, and a shifted diagonal, keep their shape.
	applyPattern (Pattern::Reverse, 12, m);
	refitMatrix (12, 18, m);
	bool rev = pinCount (m, 18) == 18;
	for (int b = 0; b < 18; ++b)
		rev = rev && cell (m, b, 17 - b) > 0.0;
	chk (rev, "12 -> 18: a reverse stays a full reverse", where);

	applyPattern (Pattern::Diagonal, 9, m);
	applyPattern (Pattern::Shift, 9, m);
	applyPattern (Pattern::Shift, 9, m);
	refitMatrix (9, 12, m);
	bool shifted = pinCount (m, 12) == 12;
	for (int b = 0; b < 12; ++b)
		shifted = shifted && cell (m, b, (b + 2) % 12) > 0.0;
	chk (shifted, "a diagonal shifted twice is shifted twice at the new count", where);

	// Levels travel: a diagonal at 100 % stays at 100 %, and an extra pin
	// beside it moves to the same relative place with its own level.
	applyPattern (Pattern::Diagonal, 9, m);
	for (int b = 0; b < 9; ++b)
		m[b * kMaxBands + b] = 1.0;
	m[3 * kMaxBands + 5] = 0.25;
	refitMatrix (9, 22, m);
	chk (cell (m, 0, 0) == 1.0 && cell (m, 21, 21) == 1.0, "a diagonal at 100 % stays at 100 %", where);
	chk (offDiagonalCount (m, 22) == 1 && cell (m, 8, 13) == 0.25, "an extra pin moves with the matrix, keeping its level",
	     where);

	// No pattern at all: every pin moves to the same relative place.
	applyPattern (Pattern::Clear, 22, m);
	m[0 * kMaxBands + 8] = 0.5;
	m[8 * kMaxBands + 2] = 0.7;
	refitMatrix (9, 18, m);
	chk (pinCount (m, 18) == 2 && cell (m, 0, 17) == 0.5 && cell (m, 17, 4) == 0.7,
	     "a free patch is scaled pin by pin", where);

	// Empty stays empty; the same count changes nothing.
	applyPattern (Pattern::Clear, 22, m);
	refitMatrix (9, 22, m);
	chk (pinCount (m, 22) == 0, "an empty matrix stays empty", where);
	applyPattern (Pattern::Reverse, 12, m);
	double before[kPatchCells];
	std::memcpy (before, m, sizeof (before));
	refitMatrix (12, 12, m);
	chk (std::memcmp (before, m, sizeof (before)) == 0, "the same count changes nothing", where);
}

//------------------------------------------------------------------------
static void presetTests ()
{
	const char* where = "presets";

	chk (factoryPresetCount () >= 2, "there are factory presets", where);
	chk (std::strcmp (factoryPresetName (0), "Classic 9-Band") == 0, "the first is Classic 9-Band", where);

	const Patch classic = factoryPreset (0);
	near (classic.values[kFreq], 0.5, 1e-12, "Classic: Bottom 50 %", where);
	near (classic.values[kFreqSpread], 0.92, 1e-12, "Classic: Top 92 %", where);
	chk (pinCount (classic.patch (), 9) == 9, "Classic: diagonal", where);

	for (int i = 0; i < factoryPresetCount (); ++i)
	{
		const Patch p = factoryPreset (i);
		const char* name = factoryPresetName (i);
		chk (name && name[0], "every preset has a name", where);
		for (Steinberg::Vst::ParamID id = 0; id < kNumParams; ++id)
			chk (p.values[id] >= 0.0 && p.values[id] <= 1.0, "values normalised", name);
		for (int s = 0; s < 5; ++s)
			chk (p.paths[s].empty (), "factory presets carry no samples", name);
		const int bands = bandCount (paramDef (kBands).toInternal (p.values[kBands]));
		chk (pinCount (p.patch (), bands) > 0, "every preset makes a sound", name);
	}

	chk (keepsSamples (kSample1) && keepsSamples (kSample5) && keepsSamples (kEnable),
	     "slot switches and Enabled are left alone", where);
	chk (! keepsSamples (kFreq) && ! keepsSamples (kInterlaced), "everything else is set", where);
}

//------------------------------------------------------------------------
static void stateTests ()
{
	const char* where = "state";

	Patch p = factoryPreset (2);
	p.values[kOutputTrim] = 0.123456789;
	p.bypass = true;
	p.paths[0] = "/Users/andy/Samples/Numbers Station.wav";
	p.paths[3] = "/tmp/x.wav";

	const auto bytes = encodeState (p);

	// Field by field against SpyBandProcessor::getState's layout.
	auto int32At = [&] (std::size_t at) {
		return static_cast<std::int32_t> (bytes[at] | (bytes[at + 1] << 8) | (bytes[at + 2] << 16)
		                                  | (static_cast<std::uint32_t> (bytes[at + 3]) << 24));
	};
	chk (int32At (0) == 2, "version 2", where);
	chk (int32At (4) == static_cast<std::int32_t> (kNumParams), "then the parameter count", where);
	double first = 0.0;
	std::memcpy (&first, &bytes[8], 8);
	near (first, p.values[0], 0.0, "then the values as little-endian doubles", where);
	const std::size_t after = 8 + 8 * kNumParams;
	chk (int32At (after) == 1, "then bypass", where);
	chk (int32At (after + 4) == 5, "then five slots", where);
	chk (int32At (after + 8) == static_cast<std::int32_t> (p.paths[0].size ()), "each a length...", where);
	chk (std::memcmp (&bytes[after + 12], p.paths[0].data (), p.paths[0].size ()) == 0, "...and its bytes", where);

	Patch q;
	chk (decodeState (bytes.data (), bytes.size (), q), "it reads back", where);
	for (Steinberg::Vst::ParamID id = 0; id < kNumParams; ++id)
		chk (q.values[id] == p.values[id], "every value exactly", where);
	chk (q.bypass && q.paths[0] == p.paths[0] && q.paths[3] == p.paths[3] && q.paths[1].empty (),
	     "bypass and paths", where);

	// A short stream - an older version with fewer parameters - leaves
	// the rest at the default, as the processor does.
	std::vector<std::uint8_t> shortStream (bytes.begin (), bytes.begin () + 8 + 8 * 26);
	shortStream[4] = 26; shortStream[5] = shortStream[6] = shortStream[7] = 0;
	const std::uint8_t tail[] = { 0, 0, 0, 0, 0, 0, 0, 0 };   // bypass 0, no slots
	shortStream.insert (shortStream.end (), tail, tail + 8);
	Patch r;
	chk (decodeState (shortStream.data (), shortStream.size (), r), "a short stream reads", where);
	chk (r.values[patchParam (4, 4)] == 0.5 && r.values[patchParam (4, 5)] == 0.0,
	     "the matrix it did not carry is the default", where);

	// A version 1 stream: the trim was on a -60..0 dB travel and must keep
	// its decibels (0.5 was -30 dB, 1.0 was 0 dB).
	std::vector<std::uint8_t> v1 = bytes;
	v1[0] = 1;
	for (double old : { 0.5, 1.0 })
	{
		std::memcpy (&v1[8 + 8 * kOutputTrim], &old, 8);
		Patch s;
		chk (decodeState (v1.data (), v1.size (), s), "a version 1 stream reads", where);
		near (outputDecibels (s.values[kOutputTrim]), -60.0 + 60.0 * old, 1e-9,
		      "a version 1 trim keeps its dB", where);
		chk (s.values[kFreq] == p.values[kFreq], "and nothing else moves", where);
	}

	chk (! decodeState (bytes.data (), 6, q), "a truncated stream is refused", where);
	std::vector<std::uint8_t> bad = bytes;
	bad[0] = 9;
	chk (! decodeState (bad.data (), bad.size (), q), "a newer version is refused", where);
}


//------------------------------------------------------------------------
// The black panel's layout (SpyBandLayout.h): every printed word fits
// where it is printed, and nothing sits on anything else.
//------------------------------------------------------------------------
static bool inside (const Layout::Rect& inner, const Layout::Rect& outer, double margin = 0.0)
{
	return inner.left >= outer.left + margin && inner.right <= outer.right - margin
	    && inner.top >= outer.top + margin && inner.bottom <= outer.bottom - margin;
}

static int sectionOf (double x, double y)
{
	for (int i = 0; i < Layout::kNumSections; ++i)
		if (Layout::kSections[i].r.contains (x, y))
			return i;
	return -1;
}

static void layoutTests ()
{
	using namespace Layout;
	const char* where = "layout";
	const Rect panel { 0, kHeaderHeight, kEditorWidth, kEditorHeight };

	// Sections: on the panel, apart, and their titles and subtitles inside
	// them - for the centre, every tab's title.
	for (int i = 0; i < kNumSections; ++i)
	{
		const Section& s = kSections[i];
		chk (inside (s.r, panel, 4.0), "a section is on the panel, clear of the edge", where);
		for (int j = i + 1; j < kNumSections; ++j)
			chk (! s.r.overlaps (kSections[j].r), "sections do not overlap", where);
		chk (inside (titleBox (s), s.r, 4.0), "a title box is inside its section", where);
		if (s.subtitle && *s.subtitle)
			chk (subtitleLeft (s) + kTextSubtitle.width (s.subtitle) < s.r.right - 8.0,
			     "a subtitle fits its section", where);
	}
	for (const auto& t : kCentreTitles)
	{
		const Section& c = kSections[kCentreSection];
		chk (subtitleLeft (c, t.title) + kTextSubtitle.width (t.subtitle) < tabRect (0).left - 8.0,
		     "every centre subtitle stops short of the tabs", where);
	}
	for (int i = 0; i < 3; ++i)
		chk (kTextButton.width (kCentreTitles[i].title) < tabRect (i).width () - 10.0,
		     "a tab's word fits its button", where);

	// Screens: each inside one section, with its cut-out.
	for (int i = 0; i < kNumScreens; ++i)
	{
		const Rect b = bezelOf (screen (i));
		const int sec = sectionOf (b.left, b.top);
		chk (sec >= 0 && inside (b, kSections[sec].r, 2.0), "a screen's cut-out is inside a section", where);
	}

	// Knobs: inside their section, the widest likely reading and the label
	// within the knob's view, and no two views overlap.
	std::vector<Rect> boxes;
	for (const auto& k : kKnobs)
	{
		const Rect r = knobRect (k);
		const int sec = sectionOf (k.cx, k.top);
		chk (sec >= 0 && inside (r, kSections[sec].r, 2.0), (std::string ("knob inside its section: ") + k.label).c_str (), where);
		chk (kTextLabel.width (k.label) < r.width () - 4.0, (std::string ("knob label fits: ") + k.label).c_str (), where);
		chk (kTextValue.width ("20 % UNITY") < r.width () - 4.0, "the widest reading fits", where);
		for (const auto& b : boxes)
			chk (! r.overlaps (b), (std::string ("knob clear of the others: ") + k.label).c_str (), where);
		boxes.push_back (r);
	}

	// Selectors: every position's name and the selector's own name inside
	// the section, clear of the knob and of each other, clear of every
	// knob; and a click on a name picks it.
	for (const auto& s : kSelectors)
	{
		const int sec = sectionOf (s.cx, s.cy);
		chk (sec >= 0, "a selector is in a section", where);
		const Rect hub { s.cx - s.radius - 8.0, s.cy - s.radius - 8.0, s.cx + s.radius + 8.0, s.cy + s.radius + 8.0 };
		std::vector<Rect> labels;
		for (int i = 0; i < s.count; ++i)
		{
			const Rect b = selectorLabelBox (s, i);
			chk (sec >= 0 && inside (b, kSections[sec].r, 4.0),
			     (std::string ("position name inside its section: ") + s.names[i]).c_str (), where);
			// Clear of the chicken head's reach: its beak's tip is radius + 3
			// out, the printed marks end at radius + 8 along their own angle.
			const double nx = std::clamp (s.cx, b.left, b.right), ny = std::clamp (s.cy, b.top, b.bottom);
			chk (std::hypot (nx - s.cx, ny - s.cy) > s.radius + 4.0,
			     (std::string ("position name clear of the knob: ") + s.names[i]).c_str (), where);
			for (const auto& o : labels)
				chk (! b.overlaps (o), (std::string ("position names apart: ") + s.names[i]).c_str (), where);
			labels.push_back (b);
			const double cx = (b.left + b.right) / 2.0, cy = (b.top + b.bottom) / 2.0;
			chk (selectorPositionAt (s, cx, cy) == i, (std::string ("a click on a name picks it: ") + s.names[i]).c_str (), where);
		}
		const Rect name = selectorNameBox (s);
		chk (! name.overlaps (hub), "a selector's name is clear of its knob", where);
		chk (selectorPositionAt (s, s.cx, s.cy) == -1, "a click on the knob steps", where);
		const Rect all = selectorRect (s);
		chk (sec >= 0 && inside (all, kSections[sec].r, 1.0), (std::string ("selector inside its section: ") + s.name).c_str (), where);
		for (const auto& b : boxes)
			chk (! all.overlaps (b), (std::string ("selector clear of every knob and selector: ") + s.name).c_str (), where);
		boxes.push_back (all);
	}

	// The two selectors in the modulator's top row must not touch.
	chk (selectorLabelBox (kSelectors[0], 1).right + 6.0 < selectorLabelBox (kSelectors[1], 0).left,
	     "RIGHT IN and ONCE are apart", where);

	// The slots and the controls under them.
	for (int i = 0; i < kNumSlotsOnPanel; ++i)
		chk (inside (slotRow (i), kSections[0].r, 4.0), "a slot row is inside the modulator", where);
	chk (slotRow (kNumSlotsOnPanel - 1).bottom < 452.0, "the slots end above the rule", where);
	chk (kTextNote.width ("EMPTY \xE2\x80\x93 DROP A .WAV") < slotRow (0).width () - 44.0, "the empty slot note fits", where);

	// The matrix section: the board's grid, at 9 bands, ends before the
	// legend, and its rotated figures end above the pattern buttons.
	{
		const double pitch = std::floor (std::min (kPanelBoard.width () - 30.0, kPanelBoard.height () - 32.0) / 9);
		const double gridRight = kPanelBoard.left + 29.0 + 9 * pitch;
		chk (gridRight < kLegendPinX - 8.0, "the board stops short of the legend", where);
		chk (kPanelBoard.bottom < patternButton (0).top, "the board's figures end above the buttons", where);
		for (int i = 0; i < 4; ++i)
			chk (inside (patternButton (i), kSections[3].r, 4.0), "a pattern button is inside the matrix", where);
		chk (Layout::Text { 5.4, SciFi::kBold }.width ("DIAGONAL") < patternButton (0).width () - 6.0,
		     "DIAGONAL fits its button", where);
		chk (kTextNote.width ("2 OFF-DIAG") < kSections[3].r.right - kPinCount.left - 4.0, "the counts fit", where);
	}

	// The header's windows do not overlap, and the wordmark ends before the
	// divider.
	const Rect header[] = { kPrevButton, kPresetWindow, kNextButton, kSaveButton, kVersionBox, kBypassBox };
	for (int i = 0; i < 6; ++i)
		for (int j = i + 1; j < 6; ++j)
			chk (! header[i].overlaps (header[j]), "header windows are apart", where);
	{
		SciFi::Style wm;
		wm.cap = kWordmarkCap;
		wm.weight = SciFi::kWordmark;
		wm.tracking = 1.2;
		const double w = SciFi::textWidth ("SPY", wm) + 10.0 + SciFi::textWidth ("BAND", wm);
		chk (kWordmarkLeft + w < kHeaderDivider - 6.0, "the wordmark ends before the divider", where);
	}
	chk (kTextHeader.width ("FACTORY / CLASSIC 9-BAND") < kPresetWindow.width () - 42.0, "a preset name fits its window", where);
	chk (kTextHeader.width ("V1.1.0 \xC2\xB7 AE COBLEY") < kVersionBox.width () - 32.0, "the version fits its box", where);
}

//------------------------------------------------------------------------
// The wear (SpyBandWear.h): the same marks every time, on the panel and
// off the glass, and faint.
//------------------------------------------------------------------------
static void wearTests ()
{
	using namespace Wear;
	const char* where = "wear";

	const PanelWear a = panelWear (), b = panelWear ();
	chk (a.haze.size () == b.haze.size () && a.scratches.size () == b.scratches.size ()
	     && a.specks.size () == b.specks.size (), "the same marks every run", where);
	bool same = true;
	for (std::size_t i = 0; i < a.specks.size () && i < b.specks.size (); ++i)
		same = same && a.specks[i].x == b.specks[i].x && a.specks[i].alpha == b.specks[i].alpha;
	chk (same, "in the same places", where);
	chk (a.haze.size () == 90 && a.specks.size () > 800 && ! a.scratches.empty (), "there is some wear", where);
	chk (a.rings.size () == static_cast<std::size_t> (Layout::kNumKnobs + Layout::kNumSelectors),
	     "a foot ring for every knob and selector", where);

	// Pinned values, recorded from g++ on Linux (24 Oct 2026): the Mac's
	// clang must make the very same marks. If they differ, a statement
	// takes two random numbers in one expression somewhere. If the
	// generator changes on purpose, record them again.
	chk (a.drifts.size () == 131 && a.scratches.size () == 51 && a.specks.size () == 870 && a.fibres.size () == 7,
	     "the counts are the recorded ones", where);
	near (a.specks.empty () ? 0.0 : a.specks[0].x, 267.4001, 1e-3, "the first speck is where it was", where);
	near (a.specks.empty () ? 0.0 : a.specks[0].y, 676.6105, 1e-3, "the first speck is where it was", where);

	for (const auto& h : a.haze)
		chk (h.alpha <= kMaxHazeAlpha, "haze is faint", where);
	bool onPanel = true, offGlass = true, faint = true;
	for (const auto& l : a.scratches)
	{
		faint = faint && l.alpha <= kMaxMarkAlpha && l.width < 1.4;
		for (const auto& p : l.pts)
		{
			onPanel = onPanel && p.x >= 0 && p.y >= 0 && p.x <= Layout::kEditorWidth && p.y <= Layout::kEditorHeight;
			for (int s = 0; s < Layout::kNumScreens; ++s)
				offGlass = offGlass && ! Layout::bezelOf (Layout::screen (s)).contains (p.x, p.y);
		}
	}
	for (const auto& sp : a.specks)
	{
		faint = faint && sp.r < 0.7;
		for (int s = 0; s < Layout::kNumScreens; ++s)
			offGlass = offGlass && ! Layout::screen (s).contains (sp.x, sp.y);
	}
	chk (onPanel, "every scratch is on the panel", where);
	chk (offGlass, "no panel mark on a screen", where);
	chk (faint, "every mark is faint and fine", where);

	for (int i = 0; i < Layout::kNumKnobs; ++i)
	{
		const KnobWear k = knobWear (i);
		bool ok = ! k.scuffs.empty ();
		for (const auto& s : k.scuffs)
			ok = ok && s.radius > 0.0 && s.radius < 1.0 && s.alpha <= kMaxMarkAlpha;
		for (const auto& g : k.grit)
			ok = ok && g.radius > 0.0 && g.radius < 1.0;
		chk (ok, "a knob's marks are on its cap or knurling", where);
	}
	chk (knobWear (0).scuffs.size () != knobWear (1).scuffs.size ()
	     || knobWear (0).scuffs[0].start != knobWear (1).scuffs[0].start, "every knob is its own", where);

	for (int i = 0; i < Layout::kNumScreens; ++i)
	{
		const GlassWear g = glassWear (i);
		const Layout::Rect s = Layout::screen (i);
		bool ok = true;
		for (const auto& d : g.dust)
			ok = ok && s.contains (d.x, d.y);
		for (const auto& p : g.prints)
			for (const auto& r : p.ridges)
			{
				ok = ok && r.alpha <= kMaxPrintAlpha;
				for (const auto& pt : r.pts)
					ok = ok && s.contains (pt.x, pt.y);
			}
		chk (ok, "a screen's wear is on its own glass", where);
	}
	chk (glassWear (Layout::kScreenBands).prints.size () == 1 && glassWear (Layout::kScreenInput).prints.size () == 1,
	     "a fingerprint on the big screen and on the Input Check", where);
}

//------------------------------------------------------------------------
int main ()
{
	letteringTests ();
	bandTests ();
	patternTests ();
	presetTests ();
	stateTests ();
	layoutTests ();
	wearTests ();

	std::printf ("%d checks, %d failed\n", checks, fails);
	return fails == 0 ? 0 : 1;
}
