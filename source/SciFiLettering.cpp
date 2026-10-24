//------------------------------------------------------------------------
// SpyBand - 70s sci-fi lettering
//------------------------------------------------------------------------

#include "SciFiLettering.h"

#include <cctype>
#include <cstdlib>
#include <map>

namespace SpyBand {
namespace SciFi {

namespace {

//------------------------------------------------------------------------
// The face. {code point, advance in grid units, path}.
//
// The path language is a subset of SVG's: M, L, Q and Z, absolute
// coordinates, y down. Z closes to the start of the CURRENT subpath, as
// SVG's does, so a glyph made of several subpaths closes each of them
// with an explicit L where it means "back to the glyph's start" - which
// is how B first came out as a 3.
//------------------------------------------------------------------------
struct GlyphDef
{
	char32_t code;
	double advance;
	const char* path;
};

const GlyphDef kGlyphs[] =
{
	{ U'A', 4, "M0,6 L0,2 Q0,0 2,0 Q4,0 4,2 L4,6 M0,3.6 L4,3.6" },
	{ U'B', 4, "M0,0 L2.8,0 Q4,0 4,1.5 Q4,3 2.8,3 L0,3 M2.8,3 Q4,3 4,4.5 Q4,6 2.8,6 L0,6 L0,0" },
	{ U'C', 4, "M4,0 L1.6,0 Q0,0 0,1.6 L0,4.4 Q0,6 1.6,6 L4,6" },
	{ U'D', 4, "M0,0 L2,0 Q4,0 4,2 L4,4 Q4,6 2,6 L0,6 Z" },
	{ U'E', 4, "M4,0 L0,0 L0,6 L4,6 M0,3 L3.2,3" },
	{ U'F', 4, "M4,0 L0,0 L0,6 M0,3 L3.2,3" },
	{ U'G', 4, "M4,0 L1.6,0 Q0,0 0,1.6 L0,4.4 Q0,6 1.6,6 L4,6 L4,3.2 L2.2,3.2" },
	{ U'H', 4, "M0,0 L0,6 M4,0 L4,6 M0,3 L4,3" },
	{ U'I', 0, "M0,0 L0,6" },
	{ U'J', 4, "M4,0 L4,4.4 Q4,6 2.4,6 L0,6" },
	{ U'K', 4, "M0,0 L0,6 M0,3 L1.8,3 L4,0 M1.8,3 L4,6" },
	{ U'L', 4, "M0,0 L0,6 L4,6" },
	{ U'M', 5, "M0,6 L0,1.4 Q0,0 1.25,0 Q2.5,0 2.5,1.4 L2.5,4.2 M2.5,1.4 Q2.5,0 3.75,0 Q5,0 5,1.4 L5,6" },
	{ U'N', 4, "M0,6 L0,0 L4,6 L4,0" },
	{ U'O', 4, "M1.6,0 L2.4,0 Q4,0 4,1.6 L4,4.4 Q4,6 2.4,6 L1.6,6 Q0,6 0,4.4 L0,1.6 Q0,0 1.6,0 Z" },
	{ U'P', 4, "M0,6 L0,0 L2.8,0 Q4,0 4,1.5 Q4,3 2.8,3 L0,3" },
	{ U'Q', 4, "M1.6,0 L2.4,0 Q4,0 4,1.6 L4,4.4 Q4,6 2.4,6 L1.6,6 Q0,6 0,4.4 L0,1.6 Q0,0 1.6,0 Z M2.6,4.4 L4.3,6.2" },
	{ U'R', 4, "M0,6 L0,0 L2.8,0 Q4,0 4,1.5 Q4,3 2.8,3 L0,3 M2.4,3 L4,6" },
	{ U'S', 4, "M4,0 L1.4,0 Q0,0 0,1.5 Q0,3 1.5,3 L2.5,3 Q4,3 4,4.5 Q4,6 2.6,6 L0,6" },
	{ U'T', 4, "M0,0 L4,0 M2,0 L2,6" },
	{ U'U', 4, "M0,0 L0,4 Q0,6 2,6 Q4,6 4,4 L4,0" },
	{ U'V', 4, "M0,0 L0,3.4 L2,6 L4,3.4 L4,0" },
	{ U'W', 5, "M0,0 L0,4.6 Q0,6 1.25,6 Q2.5,6 2.5,4.6 L2.5,1.8 M2.5,4.6 Q2.5,6 3.75,6 Q5,6 5,4.6 L5,0" },
	{ U'X', 4, "M0,0 L4,6 M4,0 L0,6" },
	{ U'Y', 4, "M0,0 L0,1.4 Q0,3 2,3 Q4,3 4,1.4 L4,0 M2,3 L2,6" },
	{ U'Z', 4, "M0,0 L4,0 L0,6 L4,6" },

	{ U'0', 4, "M1.6,0 L2.4,0 Q4,0 4,1.6 L4,4.4 Q4,6 2.4,6 L1.6,6 Q0,6 0,4.4 L0,1.6 Q0,0 1.6,0 Z" },
	{ U'1', 1.2, "M0,1 L1.2,0 L1.2,6" },
	{ U'2', 4, "M0,0 L2.5,0 Q4,0 4,1.5 Q4,3 2.5,3 L1.5,3 Q0,3 0,4.5 L0,6 L4,6" },
	{ U'3', 4, "M0,0 L2.8,0 Q4,0 4,1.5 Q4,3 2.8,3 L1.2,3 M2.8,3 Q4,3 4,4.5 Q4,6 2.8,6 L0,6" },
	{ U'4', 4, "M3,6 L3,0 L0,4 L4,4" },
	{ U'5', 4, "M4,0 L0,0 L0,3 L2.5,3 Q4,3 4,4.5 Q4,6 2.5,6 L0,6" },
	{ U'6', 4, "M3.6,0 L1.6,0 Q0,0 0,1.6 L0,4.5 Q0,6 1.5,6 L2.5,6 Q4,6 4,4.5 Q4,3 2.5,3 L0,3" },
	{ U'7', 4, "M0,0 L4,0 L1.6,6" },
	{ U'8', 4, "M1.5,3 Q0,3 0,1.5 Q0,0 1.5,0 L2.5,0 Q4,0 4,1.5 Q4,3 2.5,3 Z M1.5,3 L2.5,3 Q4,3 4,4.5 Q4,6 2.5,6 L1.5,6 Q0,6 0,4.5 Q0,3 1.5,3" },
	{ U'9', 4, "M0.4,6 L2.4,6 Q4,6 4,4.4 L4,1.5 Q4,0 2.5,0 L1.5,0 Q0,0 0,1.5 Q0,3 1.5,3 L4,3" },

	{ U'.', 0, "M0,6 L0,6.01" },
	{ U',', 0, "M0,6 L-0.4,7" },
	{ U':', 0, "M0,2 L0,2.01 M0,5.5 L0,5.51" },
	{ U'·', 0, "M0,3 L0,3.01" },            // middle dot
	{ U'•', 0, "M0,3 L0,3.01" },            // bullet
	{ U'●', 1.6, "M0.8,3 L0.8,3.01" },      // black circle
	{ U'%', 4, "M0,0 L1.1,0 L1.1,1.3 L0,1.3 Z M4,0 L0,6 M2.9,4.7 L4,4.7 L4,6 L2.9,6 Z" },
	{ U'-', 3, "M0,3 L3,3" },
	{ U'−', 3, "M0,3 L3,3" },               // minus
	{ U'–', 3, "M0,3 L3,3" },               // en dash
	{ U'—', 4, "M0,3 L4,3" },               // em dash
	{ U'+', 3, "M0,3 L3,3 M1.5,1.5 L1.5,4.5" },
	{ U'×', 3, "M0,1.6 L3,4.6 M3,1.6 L0,4.6" },  // multiplication sign
	{ U'/', 3, "M0,6 L3,0" },
	{ U'=', 3, "M0,2 L3,2 M0,4 L3,4" },
	{ U'→', 4.4, "M0,3 L4.4,3 M2.8,1.4 L4.4,3 L2.8,4.6" },  // right arrow
	{ U'↓', 3, "M1.5,0 L1.5,6 M0,4.5 L1.5,6 L3,4.5" },     // down arrow
	{ U'‹', 2.2, "M2.2,1 L0,3 L2.2,5" },    // single left angle quote
	{ U'›', 2.2, "M0,1 L2.2,3 L0,5" },      // single right angle quote
	{ U'⌄', 3.4, "M0,2.4 L1.7,4 L3.4,2.4" }, // down arrowhead
	{ U'┆', 0, "M0,0 L0,1.4 M0,2.3 L0,3.7 M0,4.6 L0,6" },
	{ U'\'', 0, "M0,0 L0,1.6" },
	{ U'(', 1.2, "M1.2,0 Q0,1 0,3 Q0,5 1.2,6" },
	{ U')', 1.2, "M0,0 Q1.2,1 1.2,3 Q1.2,5 0,6" },
};

//------------------------------------------------------------------------
struct Glyph
{
	double advance = 0.0;
	std::vector<Segment> segments;
};

/** Read "x,y" from `p`, advancing it. */
bool readPoint (const char*& p, double& x, double& y)
{
	char* end = nullptr;
	x = std::strtod (p, &end);
	if (end == p || *end != ',')
		return false;
	p = end + 1;
	y = std::strtod (p, &end);
	if (end == p)
		return false;
	p = end;
	return true;
}

std::vector<Segment> parse (const char* path)
{
	std::vector<Segment> out;
	const char* p = path;
	while (*p)
	{
		while (*p == ' ')
			++p;
		const char command = *p;
		if (command == '\0')
			break;
		++p;

		Segment s { Op::Move, 0.0, 0.0, 0.0, 0.0 };
		switch (command)
		{
			case 'M':
				s.op = Op::Move;
				readPoint (p, s.x, s.y);
				break;
			case 'L':
				s.op = Op::Line;
				readPoint (p, s.x, s.y);
				break;
			case 'Q':
				s.op = Op::Quad;
				readPoint (p, s.cx, s.cy);
				while (*p == ' ')
					++p;
				readPoint (p, s.x, s.y);
				break;
			case 'Z':
				s.op = Op::Close;
				break;
			default:
				// Not part of the language - stop rather than guess.
				return out;
		}
		out.push_back (s);
	}
	return out;
}

const std::map<char32_t, Glyph>& table ()
{
	static const std::map<char32_t, Glyph> glyphs = [] {
		std::map<char32_t, Glyph> m;
		for (const GlyphDef& def : kGlyphs)
			m[def.code] = Glyph { def.advance, parse (def.path) };
		return m;
	} ();
	return glyphs;
}

char32_t upper (char32_t c)
{
	if (c >= U'a' && c <= U'z')
		return c - U'a' + U'A';
	return c;
}

const Glyph& glyphFor (char32_t c)
{
	const auto& t = table ();
	auto it = t.find (upper (c));
	if (it != t.end ())
		return it->second;
	return t.at (U'·');
}

} // namespace

//------------------------------------------------------------------------
std::u32string decodeUtf8 (const std::string& utf8)
{
	std::u32string out;
	const unsigned char* p = reinterpret_cast<const unsigned char*> (utf8.data ());
	const unsigned char* end = p + utf8.size ();

	while (p < end)
	{
		const unsigned char b = *p;
		int extra = 0;
		char32_t c = 0;
		if (b < 0x80)       { c = b; extra = 0; }
		else if ((b & 0xE0) == 0xC0) { c = b & 0x1F; extra = 1; }
		else if ((b & 0xF0) == 0xE0) { c = b & 0x0F; extra = 2; }
		else if ((b & 0xF8) == 0xF0) { c = b & 0x07; extra = 3; }
		else
		{
			out.push_back (U'�');
			++p;
			continue;
		}

		if (end - p <= extra)
		{
			out.push_back (U'�');
			break;
		}

		bool ok = true;
		for (int i = 1; i <= extra; ++i)
		{
			if ((p[i] & 0xC0) != 0x80)
			{
				ok = false;
				break;
			}
			c = (c << 6) | (p[i] & 0x3F);
		}

		if (! ok)
		{
			out.push_back (U'�');
			++p;
			continue;
		}

		out.push_back (c);
		p += extra + 1;
	}
	return out;
}

//------------------------------------------------------------------------
bool hasGlyph (char32_t c)
{
	return c == U' ' || table ().count (upper (c)) > 0;
}

//------------------------------------------------------------------------
// The arithmetic is the lettering skill's rbPaths: each glyph is placed at
// x + w/2 so its leftmost stroke's ink starts at x, and advances by its
// own width plus the stroke plus the tracking. The last tracking is not
// part of the width.
//------------------------------------------------------------------------
double textWidth (const std::string& utf8, const Style& style)
{
	const double s = style.cap / 6.0;
	const double w = style.weight;
	double x = 0.0;
	bool any = false;

	for (char32_t c : decodeUtf8 (utf8))
	{
		any = true;
		if (c == U' ')
		{
			x += kSpace;
			continue;
		}
		x += glyphFor (c).advance + w + style.tracking;
	}

	if (! any)
		return 0.0;
	const double width = (x - style.tracking) * s;
	return width > 0.0 ? width : 0.0;
}

//------------------------------------------------------------------------
std::vector<Segment> layout (const std::string& utf8, const Style& style,
                             double x0, double yTop)
{
	std::vector<Segment> out;
	const double s = style.cap / 6.0;
	const double w = style.weight;
	double x = 0.0;

	for (char32_t c : decodeUtf8 (utf8))
	{
		if (c == U' ')
		{
			x += kSpace;
			continue;
		}

		const Glyph& g = glyphFor (c);
		const double ox = x0 + (x + w / 2.0) * s;
		const double oy = yTop + (w / 2.0) * s;

		for (const Segment& seg : g.segments)
		{
			Segment t = seg;
			t.x = ox + seg.x * s;
			t.y = oy + seg.y * s;
			t.cx = ox + seg.cx * s;
			t.cy = oy + seg.cy * s;
			out.push_back (t);
		}

		x += g.advance + w + style.tracking;
	}
	return out;
}

//------------------------------------------------------------------------
} // namespace SciFi
} // namespace SpyBand
