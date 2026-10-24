//------------------------------------------------------------------------
// SpyBand - the panel's look
//------------------------------------------------------------------------

#include "SpyBandStyle.h"

#include <algorithm>
#include <cmath>

using namespace VSTGUI;

namespace SpyBand {

namespace Palette {

CColor withAlpha (const CColor& c, double alpha)
{
	CColor out = c;
	out.alpha = static_cast<uint8_t> (std::clamp (alpha, 0.0, 1.0) * 255.0 + 0.5);
	return out;
}

CColor mix (const CColor& a, const CColor& b, double t)
{
	t = std::clamp (t, 0.0, 1.0);
	auto lerp = [t] (uint8_t x, uint8_t y) {
		return static_cast<uint8_t> (x + (y - x) * t + 0.5);
	};
	return CColor (lerp (a.red, b.red), lerp (a.green, b.green),
	               lerp (a.blue, b.blue), lerp (a.alpha, b.alpha));
}

} // namespace Palette

namespace {

constexpr double kPi = 3.14159265358979323846;

/** Build a path from lettering segments, passing every point through
    `map` - the identity, or a rotation. */
template <typename Map>
SharedPointer<CGraphicsPath> pathFrom (CDrawContext* context,
                                       const std::vector<SciFi::Segment>& segments, Map map)
{
	auto path = owned (context->createGraphicsPath ());
	if (! path)
		return path;

	CPoint current;
	for (const SciFi::Segment& s : segments)
	{
		switch (s.op)
		{
			case SciFi::Op::Move:
				current = map (s.x, s.y);
				path->beginSubpath (current);
				break;
			case SciFi::Op::Line:
				current = map (s.x, s.y);
				path->addLine (current);
				break;
			case SciFi::Op::Quad:
			{
				// A quadratic as a cubic: each control point two thirds of
				// the way from its end to the quadratic's control.
				const CPoint q = map (s.cx, s.cy);
				const CPoint end = map (s.x, s.y);
				const CPoint c1 (current.x + 2.0 / 3.0 * (q.x - current.x),
				                 current.y + 2.0 / 3.0 * (q.y - current.y));
				const CPoint c2 (end.x + 2.0 / 3.0 * (q.x - end.x),
				                 end.y + 2.0 / 3.0 * (q.y - end.y));
				path->addBezierCurve (c1, c2, end);
				current = end;
				break;
			}
			case SciFi::Op::Close:
				path->closeSubpath ();
				break;
		}
	}
	return path;
}

CLineStyle roundLine ()
{
	return CLineStyle (CLineStyle::kLineCapRound, CLineStyle::kLineJoinRound);
}

void strokePath (CDrawContext* context, CGraphicsPath* path, const CColor& colour, double width)
{
	if (path == nullptr)
		return;
	context->setDrawMode (kAntiAliasing | kNonIntegralMode);
	context->setLineStyle (roundLine ());
	context->setLineWidth (width);
	context->setFrameColor (colour);
	context->drawGraphicsPath (path, CDrawContext::kPathStroked);
}

CRect circleRect (const CPoint& c, double r)
{
	return CRect (c.x - r, c.y - r, c.x + r, c.y + r);
}

} // namespace

//------------------------------------------------------------------------
double textWidth (const std::string& text, double cap, double weight)
{
	SciFi::Style style;
	style.cap = cap;
	style.weight = weight;
	return SciFi::textWidth (text, style);
}

double drawText (CDrawContext* context, const std::string& text, double x, double centreY,
                 double cap, const CColor& colour, Align align, double weight)
{
	if (context == nullptr || text.empty ())
		return 0.0;

	SciFi::Style style;
	style.cap = cap;
	style.weight = weight;

	const double width = SciFi::textWidth (text, style);
	double left = x;
	if (align == Align::Centre)
		left = x - width / 2.0;
	else if (align == Align::Right)
		left = x - width;

	const double top = centreY - style.inkHeight () / 2.0;
	const auto segments = SciFi::layout (text, style, left, top);
	auto path = pathFrom (context, segments, [] (double px, double py) { return CPoint (px, py); });
	strokePath (context, path, colour, style.lineWidth ());
	return width;
}

void drawTextRotated (CDrawContext* context, const std::string& text, const CPoint& anchor,
                      double degrees, double cap, const CColor& colour, double weight,
                      bool anchorAtEnd)
{
	if (context == nullptr || text.empty ())
		return;

	SciFi::Style style;
	style.cap = cap;
	style.weight = weight;

	const double width = SciFi::textWidth (text, style);
	const double height = style.inkHeight ();
	const auto segments = SciFi::layout (text, style, anchorAtEnd ? -width : 0.0, -height / 2.0);

	const double a = degrees * kPi / 180.0;
	const double ca = std::cos (a), sa = std::sin (a);
	auto path = pathFrom (context, segments, [&] (double px, double py) {
		return CPoint (anchor.x + px * ca - py * sa, anchor.y + px * sa + py * ca);
	});
	strokePath (context, path, colour, style.lineWidth ());
}

//------------------------------------------------------------------------
void drawWordmark (CDrawContext* context, double x, double centreY, const CColor& background)
{
	SciFi::Style style;
	style.cap = Layout::kWordmarkCap;
	style.weight = SciFi::kWordmark;
	style.tracking = 1.2;

	const double top = centreY - style.inkHeight () / 2.0;
	const double gap = 10.0;

	auto word = [&] (const char* text, double left) {
		const auto segments = SciFi::layout (text, style, left, top);
		auto path = pathFrom (context, segments, [] (double px, double py) { return CPoint (px, py); });
		strokePath (context, path, Palette::kInk, style.lineWidth ());
		// The inline: the same strokes again, thin, in the background.
		strokePath (context, path, background, 0.32 * style.cap / 6.0);
		return SciFi::textWidth (text, style);
	};

	const double spy = word ("SPY", x);
	word ("BAND", x + spy + gap);
}

//------------------------------------------------------------------------
CRect toCRect (const Layout::Rect& r)
{
	return CRect (r.left, r.top, r.right, r.bottom);
}

void fillRoundRect (CDrawContext* context, const CRect& r, double radius, const CColor& fill)
{
	auto path = owned (context->createRoundRectGraphicsPath (r, radius));
	if (! path)
		return;
	context->setDrawMode (kAntiAliasing | kNonIntegralMode);
	context->setFillColor (fill);
	context->drawGraphicsPath (path, CDrawContext::kPathFilled);
}

void strokeRoundRect (CDrawContext* context, const CRect& r, double radius,
                      const CColor& colour, double width)
{
	// Inset by half the line so the stroke sits inside `r`.
	CRect inner (r);
	inner.inset (width / 2.0, width / 2.0);
	auto path = owned (context->createRoundRectGraphicsPath (inner, std::max (0.0, radius - width / 2.0)));
	if (! path)
		return;
	context->setDrawMode (kAntiAliasing | kNonIntegralMode);
	context->setLineStyle (kLineSolid);
	context->setLineWidth (width);
	context->setFrameColor (colour);
	context->drawGraphicsPath (path, CDrawContext::kPathStroked);
}

void fillCircle (CDrawContext* context, const CPoint& c, double radius, const CColor& fill)
{
	context->setDrawMode (kAntiAliasing | kNonIntegralMode);
	context->setFillColor (fill);
	context->drawEllipse (circleRect (c, radius), kDrawFilled);
}

void strokeCircle (CDrawContext* context, const CPoint& c, double radius,
                   const CColor& colour, double width)
{
	context->setDrawMode (kAntiAliasing | kNonIntegralMode);
	context->setLineStyle (kLineSolid);
	context->setLineWidth (width);
	context->setFrameColor (colour);
	context->drawEllipse (circleRect (c, radius), kDrawStroked);
}

CPoint pointFromTop (const CPoint& centre, double radius, double degrees)
{
	double x = 0.0, y = 0.0;
	Layout::pointFromTop (centre.x, centre.y, radius, degrees, x, y);
	return CPoint (x, y);
}

void drawLamp (CDrawContext* context, const CPoint& c, double radius, bool lit, const CColor& colour)
{
	if (lit)
	{
		fillCircle (context, c, radius * 2.0, Palette::withAlpha (colour, 0.22));
		fillCircle (context, c, radius, colour);
	}
	else
	{
		fillCircle (context, c, radius, colour == Palette::kRed ? Palette::kLampOff
		                                                        : Palette::mix (Palette::kGlass, colour, 0.18));
	}
	strokeCircle (context, c, radius, CColor (0, 0, 0, 255), 0.6);
}

void drawPin (CDrawContext* context, const CPoint& c, double radius, double value)
{
	if (value <= 0.0)
	{
		const double r = std::max (1.6, radius * 0.46);
		fillCircle (context, c, r, Palette::kWindow);
		strokeCircle (context, c, r, CColor (0x4a, 0x4a, 0x4a, 255), 0.8);
		return;
	}

	// 100 % is a red pin and 50 % a white one, as on a hardware pin
	// board; in between blends, and below 50 % the white dims towards the
	// panel so a nearly-closed cell looks it.
	const CColor colour = (value >= 0.5)
		? Palette::mix (Palette::kPinWhite, Palette::kRed, (value - 0.5) / 0.5)
		: Palette::mix (CColor (0x4a, 0x48, 0x44, 255), Palette::kPinWhite, value / 0.5);

	fillCircle (context, CPoint (c.x + radius * 0.17, c.y + radius * 0.26), radius,
	            Palette::withAlpha (CColor (0, 0, 0), 0.6));
	fillCircle (context, c, radius, colour);
	strokeCircle (context, c, radius, CColor (0, 0, 0, 255), 0.7);
	// The shine: a highlight at the top left, a shade at the bottom right.
	auto path = owned (context->createGraphicsPath ());
	auto shine = owned (CGradient::create (0.0, 1.0, CColor (255, 255, 255, 140), CColor (0, 0, 0, 90)));
	if (path && shine)
	{
		shine->addColorStop (0.5, CColor (255, 255, 255, 13));
		path->addEllipse (circleRect (c, radius));
		context->fillRadialGradient (path, *shine, c, radius * 1.3, CPoint (-radius * 0.24, -radius * 0.4));
	}
}

void fillCap (CDrawContext* context, CGraphicsPath* path, const CPoint& centre, double radius)
{
	auto gradient = owned (CGradient::create (0.0, 1.0, CColor (58, 58, 58, 255), CColor (5, 5, 5, 255)));
	if (! gradient || ! path)
		return;
	gradient->addColorStop (0.6, CColor (22, 22, 22, 255));
	context->setDrawMode (kAntiAliasing | kNonIntegralMode);
	context->fillRadialGradient (path, *gradient, centre, radius * 1.4,
	                             CPoint (-radius * 0.25, -radius * 0.35));
}

void strokeArc (CDrawContext* context, const CPoint& centre, double radius,
                double fromDegrees, double toDegrees, const CColor& colour, double width)
{
	if (toDegrees < fromDegrees)
		std::swap (fromDegrees, toDegrees);
	const double span = toDegrees - fromDegrees;
	if (span <= 0.01)
		return;

	auto path = owned (context->createGraphicsPath ());
	if (! path)
		return;

	// Short straight steps rather than addArc, whose angle convention is
	// not the same on every platform.
	const int steps = std::max (2, static_cast<int> (span / 3.0));
	for (int i = 0; i <= steps; ++i)
	{
		const double a = (fromDegrees + span * i / steps) * kPi / 180.0;
		const CPoint p (centre.x + radius * std::cos (a), centre.y + radius * std::sin (a));
		if (i == 0)
			path->beginSubpath (p);
		else
			path->addLine (p);
	}
	strokePath (context, path, colour, width);
}

void drawLine (CDrawContext* context, const CPoint& a, const CPoint& b, const CColor& colour,
               double width, bool round)
{
	context->setDrawMode (kAntiAliasing | kNonIntegralMode);
	context->setLineStyle (round ? roundLine ()
	                             : CLineStyle (CLineStyle::kLineCapButt, CLineStyle::kLineJoinMiter));
	context->setLineWidth (width);
	context->setFrameColor (colour);
	context->drawLine (a, b);
}

void drawDashedLine (CDrawContext* context, const CPoint& a, const CPoint& b, const CColor& colour,
                     double width, double on, double off)
{
	const CCoord dashes[] = { on, off };
	context->setDrawMode (kAntiAliasing | kNonIntegralMode);
	context->setLineStyle (CLineStyle (CLineStyle::kLineCapButt, CLineStyle::kLineJoinMiter, 0.0, 2, dashes));
	context->setLineWidth (width);
	context->setFrameColor (colour);
	context->drawLine (a, b);
}

//------------------------------------------------------------------------
void drawSection (CDrawContext* context, const Layout::Section& s, bool withTitle)
{
	using namespace Layout;
	CRect r = toCRect (s.r);
	fillRoundRect (context, r, kSectionRadius, Palette::kSection);
	strokeRoundRect (context, r, kSectionRadius, Palette::kSectionEdge, kSectionLine);
	if (withTitle)
		drawSectionTitle (context, s, s.title, s.subtitle);
}

void drawSectionTitle (CDrawContext* context, const Layout::Section& s, const char* title, const char* subtitle)
{
	using namespace Layout;
	const CRect box = toCRect (titleBox (s, title));
	fillRoundRect (context, box, 1.0, Palette::kGround);
	strokeRoundRect (context, box, 1.0, Palette::kSectionEdge, kSectionLine);
	const double cy = box.getCenter ().y;
	drawText (context, title, box.getCenter ().x, cy, kTextTitle, Palette::kInk, Align::Centre);
	if (subtitle && *subtitle)
		drawText (context, subtitle, subtitleLeft (s, title), cy, kTextSubtitle, Palette::kInkSoft);
}

void drawBezel (CDrawContext* context, const Layout::Rect& glass, bool dots)
{
	const CRect cut = toCRect (Layout::bezelOf (glass));
	fillRoundRect (context, cut, 4.0, Palette::kBezel);
	strokeRoundRect (context, cut, 4.0, Palette::kBezelEdge, 1.0);
	const CRect g = toCRect (glass);
	fillRoundRect (context, g, 2.0, Palette::kGlass);
	if (! dots)
		return;

	// The dot grid: every 24 across and 12 down, and every 12 across on
	// the rows 24 apart - a pegboard of faint points rather than lines.
	CRect clip;
	context->getClipRect (clip);
	context->setDrawMode (kAntiAliasing | kNonIntegralMode);
	context->setFillColor (Palette::kDotGrid);
	auto dot = [&] (double x, double y) {
		if (x >= clip.left - 1 && x <= clip.right + 1 && y >= clip.top - 1 && y <= clip.bottom + 1)
			context->drawEllipse (circleRect (CPoint (x, y), 0.6), kDrawFilled);
	};
	for (double x = g.left + 24; x < g.right - 1; x += 24)
		for (double y = g.top + 12; y < g.bottom - 1; y += 12)
			dot (x, y);
	for (double y = g.top + 24; y < g.bottom - 1; y += 24)
		for (double x = g.left + 12; x < g.right - 1; x += 24)
			dot (x, y);
}

void drawGlass (CDrawContext* context, const Layout::Rect& glass, const Wear::GlassWear& wear)
{
	CRect clip;
	context->getClipRect (clip);
	const CRect g = toCRect (glass);

	for (const auto& d : wear.dust)
		if (touches (clip, d.x - d.r, d.y - d.r, d.x + d.r, d.y + d.r))
			fillCircle (context, CPoint (d.x, d.y), d.r, wearColour (Wear::kGlassDust, d.alpha));
	for (const auto& f : wear.fibres)
		if (lineTouches (clip, f.pts, f.width))
			wearLine (context, f, Wear::kGlassDust);
	for (const auto& p : wear.prints)
		if (touches (clip, p.x - p.ry * 1.2, p.y - p.ry * 1.2, p.x + p.ry * 1.2, p.y + p.ry * 1.2))
			for (const auto& ridge : p.ridges)
			{
				auto path = owned (context->createGraphicsPath ());
				if (! path || ridge.pts.size () < 2)
					continue;
				path->beginSubpath (CPoint (ridge.pts[0].x, ridge.pts[0].y));
				for (std::size_t i = 1; i < ridge.pts.size (); ++i)
					path->addLine (CPoint (ridge.pts[i].x, ridge.pts[i].y));
				path->closeSubpath ();
				strokePath (context, path, wearColour (Wear::kPrint, ridge.alpha), ridge.width);
			}

	// The sheen: brighter at the top left, gone before halfway down.
	auto path = owned (context->createRoundRectGraphicsPath (g, 2.0));
	auto sheen = owned (CGradient::create (0.0, 1.0, CColor (255, 255, 255, 18), CColor (255, 255, 255, 0)));
	if (path && sheen)
	{
		sheen->addColorStop (0.45, CColor (255, 255, 255, 4));
		sheen->addColorStop (0.46, CColor (255, 255, 255, 0));
		context->fillLinearGradient (path, *sheen, g.getTopLeft (),
		                             CPoint (g.left + g.getWidth () * 0.35, g.bottom));
	}
	drawLine (context, CPoint (g.left + 3.0, g.top + 0.8), CPoint (g.right - 3.0, g.top + 0.8),
	          CColor (255, 255, 255, 31), 0.6);
}

void strokeWithBloom (CDrawContext* context, CGraphicsPath* path, const CColor& colour, double width)
{
	strokePath (context, path, Palette::withAlpha (colour, 0.18), width * 3.2);
	strokePath (context, path, Palette::withAlpha (colour, 0.30), width * 1.9);
	strokePath (context, path, colour, width);
}

//------------------------------------------------------------------------
// The wear
//------------------------------------------------------------------------
CColor wearColour (const Wear::Rgb& c, uint8_t alpha)
{
	return CColor (c.r, c.g, c.b, alpha);
}

bool touches (const CRect& clip, double l, double t, double r, double b)
{
	return r >= clip.left && l <= clip.right && b >= clip.top && t <= clip.bottom;
}

bool lineTouches (const CRect& clip, const std::vector<Wear::Pt>& pts, double pad)
{
	if (pts.empty ())
		return false;
	double l = pts[0].x, t = pts[0].y, r = l, b = t;
	for (const auto& p : pts)
	{
		l = std::min (l, p.x); r = std::max (r, p.x);
		t = std::min (t, p.y); b = std::max (b, p.y);
	}
	return touches (clip, l - pad, t - pad, r + pad, b + pad);
}

namespace {
void polyline (CDrawContext* context, const std::vector<Wear::Pt>& pts, std::size_t from, std::size_t to,
               const CColor& colour, double width)
{
	if (to <= from + 1 || to > pts.size ())
		return;
	auto path = owned (context->createGraphicsPath ());
	if (! path)
		return;
	path->beginSubpath (CPoint (pts[from].x, pts[from].y));
	for (std::size_t i = from + 1; i < to; ++i)
		path->addLine (CPoint (pts[i].x, pts[i].y));
	strokePath (context, path, colour, width);
}
} // namespace

void wearLine (CDrawContext* context, const Wear::Line& line, const Wear::Rgb& rgb)
{
	const std::size_t n = line.pts.size ();
	if (n < 2)
		return;
	const std::size_t a = std::max<std::size_t> (1, n / 3), b = std::max (a + 1, n - n / 3);
	const uint8_t half = static_cast<uint8_t> (line.alpha / 2);
	polyline (context, line.pts, 0, a + 1, wearColour (rgb, half), line.width);
	polyline (context, line.pts, a, std::min (b + 1, n), wearColour (rgb, line.alpha), line.width);
	if (b < n - 1)
		polyline (context, line.pts, b, n, wearColour (rgb, half), line.width);
}

void softDisc (CDrawContext* context, const CPoint& centre, double radius, const Wear::Rgb& rgb, uint8_t alpha)
{
	auto path = owned (context->createGraphicsPath ());
	auto gradient = owned (CGradient::create (0.0, 1.0, wearColour (rgb, alpha), wearColour (rgb, 0)));
	if (! path || ! gradient)
		return;
	gradient->addColorStop (0.5, wearColour (rgb, static_cast<uint8_t> (alpha * 0.55)));
	path->addEllipse (circleRect (centre, radius));
	context->fillRadialGradient (path, *gradient, centre, radius);
}

void footRing (CDrawContext* context, const Wear::FootRing& f)
{
	auto path = owned (context->createGraphicsPath ());
	const CColor clear = wearColour (Wear::kDust, 0);
	auto gradient = owned (CGradient::create (0.0, 1.0, clear, clear));
	if (! path || ! gradient)
		return;
	gradient->addColorStop (f.inner / f.outer, clear);
	gradient->addColorStop (f.peak / f.outer, wearColour (Wear::kDust, f.alpha));
	const CPoint centre (f.cx, f.cy);
	path->addEllipse (circleRect (centre, f.outer));
	context->fillRadialGradient (path, *gradient, centre, f.outer);
}

void drift (CDrawContext* context, const Wear::Drift& d)
{
	auto path = owned (context->createGraphicsPath ());
	auto gradient = owned (CGradient::create (0.0, 1.0, wearColour (Wear::kDust, d.alpha), wearColour (Wear::kDust, 0)));
	if (! path || ! gradient)
		return;
	const CRect r = toCRect (d.r);
	path->addRect (r);
	const CPoint mid = r.getCenter ();
	CPoint from = mid, to = mid;
	if (d.fromX < 0) { from.x = r.left;  to.x = r.right; }
	if (d.fromX > 0) { from.x = r.right; to.x = r.left; }
	if (d.fromY < 0) { from.y = r.top;    to.y = r.bottom; }
	if (d.fromY > 0) { from.y = r.bottom; to.y = r.top; }
	context->fillLinearGradient (path, *gradient, from, to);
}

//------------------------------------------------------------------------
} // namespace SpyBand
