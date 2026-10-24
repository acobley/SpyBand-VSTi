//------------------------------------------------------------------------
// SpyBand - the panel's wear: "Studio Worn"
//
// R2Synth's wear (R2Synth-VSTi/source/PanelWear.h), brought across with
// the black panel so the two plug-ins look like they came off the same
// desk. Light wear only - nothing may cost a label its legibility:
//
//   PANEL    a patchy haze of dust, heavier towards the top and the right
//            (where hands reach least); dust drifted against the inside
//            of each section's box; a ring of dust round the foot of
//            every knob and selector; hairline scratches, some in
//            clusters, some alone; specks and the odd fibre.
//   KNOBS    the cap polished by fingers - a soft sheen at the top left -
//            and fine circular scuffs from turning; dust in the knurling.
//            Both TURN WITH THE KNOB: they are stored relative to the
//            pointer.
//   SELECTORS  grit round the chicken head's hub, turning with it.
//   SCREENS  dust specks on the glass, the odd fibre, and a fingerprint
//            on the big screen and on the Input Check.
//
// WHY DATA AND NOT A PICTURE. The panel has no bitmaps: everything is
// paths, so it is sharp at any scale. The wear keeps that rule. Every
// mark is GENERATED here, from a fixed seed, as plain numbers in panel
// units; the views draw them and tests/PanelTests.cpp holds them to the
// panel. Same seed, same marks, on every machine: its own random numbers
// (SplitMix64) and its own normal distribution, never <random>'s, whose
// distributions differ between standard libraries - and ONE random number
// per statement, never two in the arguments of one call, whose order of
// evaluation is up to the compiler (g++ and the Mac's clang disagreed on
// R2Synth, 22 Oct 2026).
//
// NO VSTGUI here, like SpyBandLayout.h.
//------------------------------------------------------------------------

#pragma once

#include "SpyBandLayout.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace SpyBand {
namespace Wear {

//------------------------------------------------------------------------
// The colours of the dirt, as RGB. Alphas are 0..255 and travel with
// each mark.
//------------------------------------------------------------------------
struct Rgb { uint8_t r, g, b; };

constexpr Rgb kDust      { 150, 146, 138 };   // haze, drifts, foot rings
constexpr Rgb kSpeck     { 175, 170, 160 };   // specks and fibres on the panel
constexpr Rgb kScratch   { 120, 120, 116 };   // hairlines through the black paint
constexpr Rgb kScuff     { 150, 150, 148 };   // turning marks on a knob's cap
constexpr Rgb kKnurlDust { 140, 136, 128 };   // dust in the knurling
constexpr Rgb kGlassDust { 170, 170, 168 };   // specks on a screen's glass
constexpr Rgb kPrint     { 200, 200, 205 };   // a fingerprint's ridges
constexpr Rgb kSheen     { 255, 255, 255 };   // a polished cap

/** The most any one mark may cover, so the wear can never hide a label. */
constexpr int kMaxHazeAlpha  = 20;
constexpr int kMaxMarkAlpha  = 130;
constexpr int kMaxPrintAlpha = 18;

//------------------------------------------------------------------------
// The marks
//------------------------------------------------------------------------
struct Pt { double x, y; };

struct Speck { double x, y, r; uint8_t alpha; };     // a filled dot
struct Blob { double x, y, r; uint8_t alpha; };      // soft haze, fading to none at r

/** Dust against a box edge: fades from the edge side (fromX/fromY: -1
    left/top, 1 right/bottom) to nothing at the other. */
struct Drift
{
	Layout::Rect r;
	int fromX, fromY;
	uint8_t alpha;
};

/** Dust round a knob's foot: peak alpha at `peak`, none at `inner` and `outer`. */
struct FootRing { double cx, cy, inner, peak, outer; uint8_t alpha; };

/** A hairline: full alpha in its middle third, half at either end. */
struct Line
{
	std::vector<Pt> pts;
	double width;
	uint8_t alpha;
};

/** A scuff on a cap: an arc about the centre. `radius` a fraction of the
    CAP's radius; `start` degrees clockwise FROM THE POINTER. */
struct Arc { double radius, start, span, width; uint8_t alpha; };

/** A speck that turns with its knob: radius a fraction of the BODY's (or
    a selector's hub's), angle in degrees from the pointer. */
struct PolarSpeck { double radius, angle, r; uint8_t alpha; };

/** A fingerprint: closed, wobbled elliptical ridges, fading outwards. */
struct Print
{
	double x, y, rx, ry, tilt;
	std::vector<Line> ridges;
};

struct PanelWear
{
	std::vector<Blob> haze;
	std::vector<Drift> drifts;
	std::vector<FootRing> rings;
	std::vector<Line> scratches;
	std::vector<Speck> specks;
	std::vector<Line> fibres;
};

struct KnobWear
{
	std::vector<Arc> scuffs;
	std::vector<PolarSpeck> grit;
	uint8_t sheen = 0;
	uint8_t knurlAlpha = 0;
};

struct GlassWear
{
	std::vector<Speck> dust;
	std::vector<Line> fibres;
	std::vector<Print> prints;
};

//------------------------------------------------------------------------
// The random numbers - the same on every machine
//------------------------------------------------------------------------
class Random
{
public:
	explicit Random (uint64_t seed) : mState (seed) {}

	uint64_t next ()
	{
		uint64_t z = (mState += 0x9E3779B97F4A7C15ull);
		z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
		z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
		return z ^ (z >> 31);
	}

	double unit () { return static_cast<double> (next () >> 11) * (1.0 / 9007199254740992.0); }
	double uniform (double lo, double hi) { return lo + (hi - lo) * unit (); }
	int below (int n) { return static_cast<int> (unit () * n); }

	double normal ()
	{
		const double u = std::max (unit (), 1e-12);
		const double v = unit ();
		return std::sqrt (-2.0 * std::log (u)) * std::cos (6.283185307179586 * v);
	}

private:
	uint64_t mState;
};

//------------------------------------------------------------------------
namespace Detail {

constexpr double kPi = 3.14159265358979323846;

inline uint8_t alpha (double a)
{
	return static_cast<uint8_t> (std::clamp (std::lround (a), 0L, 255L));
}

inline std::vector<Pt> curve (Random& rng, double x, double y, double angle, double length, double bend,
                              double step = 3.0)
{
	std::vector<Pt> pts { { x, y } };
	const int n = std::max (2, static_cast<int> (length / step));
	for (int i = 0; i < n; ++i)
	{
		angle += bend + rng.normal () * 0.0075;
		x += std::cos (angle) * step;
		y += std::sin (angle) * step;
		pts.push_back ({ x, y });
	}
	return pts;
}

/** Does a polyline come within `pad` of any screen's cut-out? Scratches
    stop at the glass; the screens get their own wear. */
inline bool crossesAScreen (const std::vector<Pt>& pts, double pad)
{
	for (int s = 0; s < Layout::kNumScreens; ++s)
	{
		const Layout::Rect b = Layout::bezelOf (Layout::screen (s));
		for (const auto& p : pts)
			if (b.contains (p.x, p.y, pad))
				return true;
	}
	return false;
}

inline bool onAScreen (double x, double y, double pad)
{
	for (int s = 0; s < Layout::kNumScreens; ++s)
		if (Layout::screen (s).contains (x, y, pad))
			return true;
	return false;
}

inline bool insidePanel (const std::vector<Pt>& pts, double margin)
{
	for (const auto& p : pts)
		if (p.x < margin || p.y < margin || p.x > Layout::kEditorWidth - margin ||
		    p.y > Layout::kEditorHeight - margin)
			return false;
	return true;
}

/** How much more dust collects here, 0..1 - the top and the right. */
inline double reach (double x, double y)
{
	return std::clamp (0.5 + 0.3 * x / Layout::kEditorWidth - 0.25 * y / Layout::kEditorHeight, 0.0, 1.0);
}

} // namespace Detail

//------------------------------------------------------------------------
// The panel
//------------------------------------------------------------------------
inline PanelWear panelWear ()
{
	using namespace Layout;
	using namespace Detail;
	Random rng (0x5370794261'6E6431ull);   // "SpyBand1"
	PanelWear w;
	const double W = kEditorWidth, H = kEditorHeight;

	// ---- the haze ------------------------------------------------------
	for (int i = 0; i < 90; ++i)
	{
		const double x = rng.uniform (0, W);
		const double y = rng.uniform (0, H);
		const double r = rng.uniform (28, 95);
		const double a = rng.uniform (4, 14) * (0.4 + 0.8 * reach (x, y));
		w.haze.push_back ({ x, y, r, alpha (std::min<double> (a, kMaxHazeAlpha)) });
	}

	// ---- dust drifted against the inside of each box -------------------
	for (const auto& s : kSections)
	{
		const Rect& b = s.r;
		const double depth = 6.0;
		auto run = [&] (bool horizontal, double from, double to, int fromX, int fromY, double fixed) {
			double at = from;
			while (at < to)
			{
				const double len = std::min (to - at, rng.uniform (18, 70));
				const uint8_t a = alpha (rng.uniform (3, 13));
				Rect r {};
				if (horizontal)
					r = { at, fromY < 0 ? fixed : fixed - depth, at + len, fromY < 0 ? fixed + depth : fixed };
				else
					r = { fromX < 0 ? fixed : fixed - depth, at, fromX < 0 ? fixed + depth : fixed, at + len };
				if (rng.unit () < 0.8)
					w.drifts.push_back ({ r, fromX, fromY, a });
				at += len;
			}
		};
		run (true, b.left + 2, b.right - 2, 0, -1, b.top + 1.5);
		run (true, b.left + 2, b.right - 2, 0, 1, b.bottom - 1.5);
		run (false, b.top + 2, b.bottom - 2, -1, 0, b.left + 1.5);
		run (false, b.top + 2, b.bottom - 2, 1, 0, b.right - 1.5);
	}

	// ---- a ring of dust round every knob's and selector's foot ---------
	for (const auto& k : kKnobs)
	{
		const double body = knobBody (k);
		const double a = rng.uniform (20, 30);
		w.rings.push_back ({ k.cx, knobCentreY (k), body - 1.0, body + 1.5, body + 5.0, alpha (a) });
	}
	for (const auto& s : kSelectors)
	{
		const double hub = s.radius * 0.8;
		const double a = rng.uniform (18, 26);
		w.rings.push_back ({ s.cx, s.cy, hub - 1.0, hub + 1.2, hub + 4.0, alpha (a) });
	}

	// ---- hairline scratches: clusters, then strays ---------------------
	auto addScratch = [&] (std::vector<Pt> pts, double width, double a) {
		if (crossesAScreen (pts, 2.0) || ! insidePanel (pts, 3.0))
			return;
		w.scratches.push_back ({ std::move (pts), width, alpha (std::min<double> (a, kMaxMarkAlpha)) });
	};
	for (int k = 0; k < 9; ++k)
	{
		const double cx = rng.uniform (0, W);
		const double cy = rng.uniform (0, H);
		const double angle = 0.15 + rng.normal () * 0.4;
		const int n = 3 + rng.below (6);
		for (int i = 0; i < n; ++i)
		{
			const double x = cx + rng.normal () * 30.0;
			const double y = cy + rng.normal () * 18.0;
			const double a = angle + rng.normal () * 0.08;
			const double length = rng.uniform (20, 110);
			const double bend = rng.normal () * 0.002;
			auto pts = curve (rng, x, y, a, length, bend);
			const double width = rng.uniform (0.25, 0.45);
			const double strength = rng.uniform (0.15, 0.4) * 0.9 * 255;
			addScratch (std::move (pts), width, strength);
		}
	}
	for (int i = 0; i < 40; ++i)
	{
		const double x = rng.uniform (0, W);
		const double y = rng.uniform (0, H);
		const double a = rng.uniform (0, 2 * kPi);
		const double length = rng.uniform (8, 45);
		const double bend = rng.normal () * 0.01;
		auto pts = curve (rng, x, y, a, length, bend);
		const double strength = rng.uniform (0.1, 0.3) * 0.9 * 255;
		addScratch (std::move (pts), 0.3, strength);
	}

	// ---- specks, and the odd fibre -------------------------------------
	for (int i = 0; i < 1100; ++i)
	{
		const double x = rng.uniform (1, W - 1);
		const double y = rng.uniform (1, H - 1);
		const double r = rng.uniform (0.15, 0.6);
		const double a = rng.uniform (0.15, 0.45) * 255;
		if (onAScreen (x, y, 1.0))
			continue;
		w.specks.push_back ({ x, y, r, alpha (a) });
	}
	for (int i = 0; i < 10; ++i)
	{
		const double x = rng.uniform (20, W - 20);
		const double y = rng.uniform (20, H - 20);
		const double a = rng.uniform (0, 2 * kPi);
		const double length = rng.uniform (10, 35);
		// Gentler than R2Synth's 0.12, which can curl a fibre into a ring.
		const double bend = rng.normal () * 0.05;
		auto pts = curve (rng, x, y, a, length, bend, 1.5);
		const uint8_t strength = alpha (rng.uniform (0.25, 0.5) * 255);
		if (! crossesAScreen (pts, 1.0) && insidePanel (pts, 2.0))
			w.fibres.push_back ({ std::move (pts), 0.35, strength });
	}
	return w;
}

//------------------------------------------------------------------------
// A knob - `index` is its place in Layout::kKnobs
//------------------------------------------------------------------------
inline KnobWear knobWear (int index)
{
	using namespace Detail;
	Random rng (0x4B6E6F62'00000000ull + static_cast<uint64_t> (index) * 0x9E37ull);   // "Knob"
	KnobWear k;
	const int n = 8 + rng.below (11);
	for (int i = 0; i < n; ++i)
	{
		const double radius = rng.uniform (0.25, 0.95);
		const double start = rng.uniform (0, 360);
		const double span = rng.uniform (17, 92);
		const double width = rng.uniform (0.3, 0.65);
		const double a = std::min (rng.uniform (0.12, 0.35) * 255.0, static_cast<double> (kMaxMarkAlpha));
		k.scuffs.push_back ({ radius, start, span, width, alpha (a) });
	}
	// grit in the knurling (between the cap, at 0.78 of the body, and the
	// teeth), and a little on the cap
	for (int i = 0; i < 22; ++i)
	{
		const double radius = rng.uniform (0.8, 0.97);
		const double angle = rng.uniform (0, 360);
		const double r = rng.uniform (0.2, 0.55);
		const double a = rng.uniform (0.2, 0.42) * 255;
		k.grit.push_back ({ radius, angle, r, alpha (a) });
	}
	for (int i = 0; i < 4; ++i)
	{
		const double radius = rng.uniform (0.1, 0.72);
		const double angle = rng.uniform (0, 360);
		const double r = rng.uniform (0.15, 0.4);
		const double a = rng.uniform (0.15, 0.35) * 255;
		k.grit.push_back ({ radius, angle, r, alpha (a) });
	}
	k.sheen = alpha (rng.uniform (12, 17));
	k.knurlAlpha = alpha (rng.uniform (0.18, 0.26) * 255 * 0.5);
	return k;
}

/** A selector's grit, round the hub's edge. `index` is its place in
    Layout::kSelectors. */
inline std::vector<PolarSpeck> selectorGrit (int index)
{
	using namespace Detail;
	Random rng (0x53656C65'00000000ull + static_cast<uint64_t> (index) * 0x9E37ull);   // "Sele"
	std::vector<PolarSpeck> g;
	for (int i = 0; i < 12; ++i)
	{
		const double radius = rng.uniform (0.82, 1.0);
		const double angle = rng.uniform (0, 360);
		const double r = rng.uniform (0.2, 0.5);
		const double a = rng.uniform (0.2, 0.4) * 255;
		g.push_back ({ radius, angle, r, alpha (a) });
	}
	return g;
}

//------------------------------------------------------------------------
// A screen's glass - `index` is a Layout::ScreenIndex
//------------------------------------------------------------------------
inline GlassWear glassWear (int index)
{
	using namespace Layout;
	using namespace Detail;
	const Rect s = screen (index);
	Random rng (0x476C6173'00000000ull + static_cast<uint64_t> (index) * 0x9E37ull);   // "Glas"
	GlassWear g;
	const int n = static_cast<int> (s.width () * s.height () / 280.0);
	for (int i = 0; i < n; ++i)
	{
		const double x = rng.uniform (s.left + 1, s.right - 1);
		const double y = rng.uniform (s.top + 1, s.bottom - 1);
		const double r = rng.uniform (0.2, 0.65);
		const double a = rng.uniform (0.15, 0.5) * 255;
		g.dust.push_back ({ x, y, r, alpha (a) });
	}
	if (index == kScreenBands || index == kScreenInput)
	{
		const double x = rng.uniform (s.left + 20, s.right - 40);
		const double y = rng.uniform (s.top + 15, s.bottom - 15);
		const double a = rng.uniform (0, 2 * kPi);
		const double length = rng.uniform (10, 22);
		const double bend = rng.normal () * 0.15;
		auto pts = curve (rng, x, y, a, length, bend, 1.5);
		const uint8_t strength = alpha (rng.uniform (0.3, 0.5) * 255);
		bool inside = true;
		for (const auto& p : pts)
			inside = inside && s.contains (p.x, p.y, -1.0);
		if (inside)
			g.fibres.push_back ({ std::move (pts), 0.35, strength });
	}

	auto print = [&] (double x, double y, double rx, double ry, double tilt) {
		Print p { x, y, rx, ry, tilt, {} };
		const double ct = std::cos (tilt * kPi / 180.0), st = std::sin (tilt * kPi / 180.0);
		const int ridges = 12;
		for (int i = 1; i <= ridges; ++i)
		{
			const double f = static_cast<double> (i) / ridges;
			Line ridge { {}, 0.45, alpha (kMaxPrintAlpha * std::exp (-f * f * 2.2)) };
			const double phase = rng.uniform (0, 2 * kPi);
			const int steps = 40;
			for (int k = 0; k <= steps; ++k)
			{
				const double t = 2 * kPi * k / steps;
				const double wobble = 1.0 + 0.04 * std::sin (3 * t + phase) + 0.025 * std::sin (5 * t + 2 * phase);
				const double u = rx * f * wobble * std::cos (t), v = ry * f * wobble * std::sin (t);
				double px = x + u * ct - v * st, py = y + u * st + v * ct;
				px = std::clamp (px, s.left + 0.5, s.right - 0.5);
				py = std::clamp (py, s.top + 0.5, s.bottom - 0.5);
				ridge.pts.push_back ({ px, py });
			}
			p.ridges.push_back (std::move (ridge));
		}
		g.prints.push_back (std::move (p));
	};
	// One on the big screen, right of centre where a hand reaches for the
	// Top handle; one on the Input Check, over the bars.
	if (index == kScreenBands)
		print (s.left + 376.0, s.top + 112.0, 13.0, 17.0, 23.0);
	if (index == kScreenInput)
		print (s.left + 176.0, s.top + 34.0, 12.0, 16.0, -17.0);
	return g;
}

//------------------------------------------------------------------------
} // namespace Wear
} // namespace SpyBand
