//------------------------------------------------------------------------
// SpyBand - 70s sci-fi lettering
//
// Every word on the panel is drawn in this face: heavy, wide,
// rounded-square capitals and figures, each glyph drawn on a 4 x 6 grid
// (y down) and STROKED with round caps and joins. It was drawn from
// scratch as paths, so there is no font file and no font licence; the
// panel uses no bitmaps and no system font at all.
//
// This half knows nothing about VSTGUI. It turns a string into a list of
// segments in pixel coordinates, and it measures, so the editor and the
// layout tests agree on how wide a label is. SpyBandStyle.cpp turns the
// segments into a CGraphicsPath and strokes it.
//
// The table is the one in Designs/SpyBand-Concept6-70sSciFiLettering.png
// and the lettering skill, transcribed glyph for glyph.
//------------------------------------------------------------------------

#pragma once

#include <string>
#include <vector>

namespace SpyBand {
namespace SciFi {

//------------------------------------------------------------------------
/** Stroke weights, in GRID units (a glyph is 6 units tall). */
constexpr double kRegular  = 0.95;
constexpr double kBold     = 1.25;
constexpr double kWordmark = 1.7;

/** Space between letters, in grid units, on top of the stroke width. */
constexpr double kTracking = 1.05;

/** A word space, in grid units. */
constexpr double kSpace = 2.6;

//------------------------------------------------------------------------
enum class Op { Move, Line, Quad, Close };

/** One path segment. For Quad, (cx, cy) is the control point and (x, y)
    the end point; Close ignores both. */
struct Segment
{
	Op op;
	double x, y;
	double cx, cy;
};

//------------------------------------------------------------------------
/** How a run of text is drawn: the height of a capital in pixels, the
    stroke weight in grid units, and the tracking. */
struct Style
{
	double cap = 8.0;
	double weight = kRegular;
	double tracking = kTracking;

	/** The stroke width in pixels. */
	double lineWidth () const { return weight * cap / 6.0; }

	/** Ink height: a capital plus half a stroke above and below. */
	double inkHeight () const { return cap + lineWidth (); }
};

/** Width in pixels of `utf8` drawn in `style`, from the left edge of the
    first stroke's ink to the right edge of the last. Lowercase is drawn
    as capitals; a character the face does not have is drawn as a dot. */
double textWidth (const std::string& utf8, const Style& style);

/** The segments for `utf8`, in pixels, with the top-left of the ink box at
    (x, yTop). Every glyph starts with a Move. */
std::vector<Segment> layout (const std::string& utf8, const Style& style,
                             double x, double yTop);

/** True if the face has a glyph for this code point (after upper-casing).
    For the tests, and for anyone adding a label in a new character. */
bool hasGlyph (char32_t c);

/** Decode UTF-8 into code points. Malformed bytes become U+FFFD. */
std::u32string decodeUtf8 (const std::string& utf8);

//------------------------------------------------------------------------
} // namespace SciFi
} // namespace SpyBand
