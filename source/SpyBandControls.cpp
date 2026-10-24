//------------------------------------------------------------------------
// SpyBand - the panel's controls
//------------------------------------------------------------------------

#include "SpyBandControls.h"
#include "BandLayout.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>

using namespace VSTGUI;

namespace SpyBand {

namespace {

constexpr double kPi = 3.14159265358979323846;

/** Right button, or Ctrl-click - the macOS habit, and a fallback for
    hosts that keep the right button to themselves. On macOS VSTGUI
    reports the Control key as Super. */
bool isContextClick (const MouseDownEvent& event)
{
	return event.buttonState.isRight ()
	    || (event.buttonState.isLeft () && event.modifiers.has (ModifierKey::Super));
}

double toDb (double linear)
{
	return (linear > 1e-9) ? 20.0 * std::log10 (linear) : -200.0;
}

} // namespace

//========================================================================
// SpyKnob
//========================================================================
SpyKnob::SpyKnob (const Layout::Knob& knob, IControlListener* listener, const Wear::KnobWear& wear)
: CControl (toCRect (Layout::knobRect (knob)), listener, static_cast<int32_t> (knob.tag), nullptr)
, mKnob (knob)
, mWear (wear)
{
	setWantsFocus (true);
}

void SpyKnob::refreshReadout ()
{
	if (! mFormatter)
		return;
	const std::string now = mFormatter (getValueNormalized ());
	if (now != mShown)
		invalid ();
}

void SpyKnob::draw (CDrawContext* context)
{
	using namespace Layout;
	context->setDrawMode (kAntiAliasing | kNonIntegralMode);

	const double v = std::clamp (static_cast<double> (getValueNormalized ()), 0.0, 1.0);
	const CPoint centre (mKnob.cx, knobCentreY (mKnob));
	const double outer = knobOuter (mKnob);
	const double body = knobBody (mKnob);
	const bool big = mKnob.diameter >= 44;
	// The pointer, degrees from twelve o'clock: -135 at the bottom of the
	// range to +135 at the top.
	const double a = -135.0 + 270.0 * v;

	// ---- the printed scale: eleven ticks, long at the ends and the middle
	for (int i = 0; i <= 10; ++i)
	{
		const double t = -135.0 + 27.0 * i;
		const bool longTick = (i == 0 || i == 5 || i == 10);
		drawLine (context, pointFromTop (centre, body + 1.5, t),
		          pointFromTop (centre, outer - (longTick ? 0.0 : 1.8), t), Palette::kTick,
		          longTick ? 1.3 : 0.9, true);
	}
	if (mKnob.scale && big)
	{
		const CPoint lo = pointFromTop (centre, outer + 2.0, -138.0);
		const CPoint hi = pointFromTop (centre, outer + 2.0, 138.0);
		drawText (context, mKnob.bipolar ? "-" : "0", lo.x - 2.0, lo.y + 4.0, kTextTick, Palette::kTick,
		          Align::Right);
		drawText (context, mKnob.bipolar ? "+" : "10", hi.x + 2.0, hi.y + 4.0, kTextTick, Palette::kTick);
	}

	// ---- the knurled body, and the cap -------------------------------------
	auto rim = owned (context->createGraphicsPath ());
	if (rim)
	{
		constexpr int kTeeth = 28;
		for (int i = 0; i < kTeeth * 2; ++i)
		{
			const CPoint p = pointFromTop (centre, (i % 2 == 0) ? body : body - 1.4, 360.0 * i / (kTeeth * 2));
			if (i == 0)
				rim->beginSubpath (p);
			else
				rim->addLine (p);
		}
		rim->closeSubpath ();
		context->setFillColor (Palette::kKnobBody);
		context->drawGraphicsPath (rim, CDrawContext::kPathFilled);
		context->setFrameColor (Palette::kKnobRim);
		context->setLineWidth (0.6);
		context->setLineStyle (kLineSolid);
		context->drawGraphicsPath (rim, CDrawContext::kPathStroked);
	}
	const double capR = body * 0.78;
	auto cap = owned (context->createGraphicsPath ());
	if (cap)
	{
		cap->addEllipse (CRect (centre.x - capR, centre.y - capR, centre.x + capR, centre.y + capR));
		fillCap (context, cap, centre, capR);
		strokeCircle (context, centre, capR, CColor (0, 0, 0, 255), 0.8);
	}

	// ---- the wear: dust in the knurling, the polished cap, turning marks.
	// All but the sheen is placed relative to the pointer, so it turns with
	// the knob; the sheen is where the light is, so it stays.
	if (mWear.knurlAlpha > 0)
		strokeCircle (context, centre, (capR + body) / 2.0, wearColour (Wear::kKnurlDust, mWear.knurlAlpha),
		              body - capR);
	if (mWear.sheen > 0)
		softDisc (context, CPoint (centre.x - capR * 0.25, centre.y - capR * 0.3), capR * 0.55, Wear::kSheen,
		          mWear.sheen);
	for (const auto& s : mWear.scuffs)
	{
		// strokeArc measures from three o'clock; the pointer from twelve.
		const double start = a + s.start - 90.0;
		strokeArc (context, centre, capR * s.radius, start, start + s.span, wearColour (Wear::kScuff, s.alpha),
		           s.width);
	}
	for (const auto& g : mWear.grit)
		fillCircle (context, pointFromTop (centre, body * g.radius, a + g.angle), g.r,
		            wearColour (Wear::kKnurlDust, g.alpha));

	// ---- the white line -------------------------------------------------------
	drawLine (context, pointFromTop (centre, body * 0.15, a), pointFromTop (centre, body - 0.5, a), Palette::kInk,
	          big ? 2.2 : 1.8, true);

	// ---- the reading and the label --------------------------------------------
	const double textTop = mKnob.top + mKnob.diameter + kKnobGap;
	mShown = mFormatter ? mFormatter (static_cast<float> (v)) : std::string ();
	drawText (context, mShown, centre.x, textTop + kKnobValueY, kTextValue, Palette::kInk, Align::Centre);
	drawText (context, mKnob.label, centre.x, textTop + kKnobLabelY, kTextLabel, Palette::kInk, Align::Centre);

	setDirty (false);
}

void SpyKnob::onMouseDownEvent (MouseDownEvent& event)
{
	if (! event.buttonState.isLeft ())
		return;

	if (event.clickCount >= 2)
	{
		beginEdit ();
		setValueNormalized (getDefaultValue ());
		valueChanged ();
		endEdit ();
		invalid ();
		event.consumed = true;
		return;
	}

	mDragging = true;
	mAnchorY = event.mousePosition.y;
	mAnchorValue = getValueNormalized ();
	beginEdit ();
	event.consumed = true;
}

void SpyKnob::onMouseMoveEvent (MouseMoveEvent& event)
{
	if (! mDragging)
		return;

	double scale = 1.0 / kDragPixels;
	if (event.modifiers.has (ModifierKey::Shift))
		scale *= 0.1;

	// Fine control re-anchors so that pressing or releasing Shift
	// mid-drag does not make the value jump.
	const double dy = mAnchorY - event.mousePosition.y;
	const float next = static_cast<float> (std::clamp (mAnchorValue + dy * scale, 0.0, 1.0));
	mAnchorY = event.mousePosition.y;
	mAnchorValue = next;

	if (next != getValueNormalized ())
	{
		setValueNormalized (next);
		valueChanged ();
		invalid ();
	}
	event.consumed = true;
}

void SpyKnob::onMouseUpEvent (MouseUpEvent& event)
{
	if (! mDragging)
		return;
	mDragging = false;
	endEdit ();
	event.consumed = true;
}

void SpyKnob::onMouseCancelEvent (MouseCancelEvent& event)
{
	if (mDragging)
		endEdit ();
	mDragging = false;
	event.consumed = true;
}

void SpyKnob::onMouseWheelEvent (MouseWheelEvent& event)
{
	if (event.deltaY == 0.)
		return;
	const float step = event.modifiers.has (ModifierKey::Shift) ? 0.001f : 0.01f;
	beginEdit ();
	setValueNormalized (std::clamp (getValueNormalized () + (event.deltaY > 0. ? step : -step), 0.f, 1.f));
	valueChanged ();
	endEdit ();
	invalid ();
	event.consumed = true;
}

//========================================================================
// SpySelector
//========================================================================
SpySelector::SpySelector (const Layout::Selector& selector, IControlListener* listener,
                          std::vector<Wear::PolarSpeck> grit)
: CControl (toCRect (Layout::selectorRect (selector)), listener, static_cast<int32_t> (selector.tag), nullptr)
, mSel (selector)
, mGrit (std::move (grit))
{
}

int SpySelector::currentIndex () const
{
	if (mSel.count < 2)
		return 0;
	const int i = static_cast<int> (std::lround (getValueNormalized () * (mSel.count - 1)));
	return std::clamp (i, 0, mSel.count - 1);
}

void SpySelector::select (int index)
{
	if (mSel.count < 2)
		return;
	index = std::clamp (index, 0, mSel.count - 1);
	const float value = static_cast<float> (index) / static_cast<float> (mSel.count - 1);
	if (value == getValueNormalized ())
		return;
	beginEdit ();
	setValueNormalized (value);
	valueChanged ();
	endEdit ();
	invalid ();
}

void SpySelector::draw (CDrawContext* context)
{
	using namespace Layout;
	context->setDrawMode (kAntiAliasing | kNonIntegralMode);

	const CPoint centre (mSel.cx, mSel.cy);
	const int current = currentIndex ();

	// ---- the printed positions: a mark and a name for each ----------------
	for (int i = 0; i < mSel.count; ++i)
	{
		const bool set = (i == current);
		const double t = mSel.angle[i];
		drawLine (context, pointFromTop (centre, mSel.radius + 2.5, t), pointFromTop (centre, mSel.radius + 8.0, t),
		          set ? Palette::kRed : Palette::kTick, set ? 2.0 : 1.6, true);
		const Rect b = selectorLabelBox (mSel, i);
		drawText (context, mSel.names[i], b.left, (b.top + b.bottom) / 2.0, kTextLabel,
		          set ? Palette::kInk : Palette::kInkDim);
	}

	// ---- the chicken head: a round hub and a beak that points ------------
	const double a = mSel.angle[current];
	const double hub = mSel.radius * 0.8;
	fillCircle (context, centre, hub, Palette::kHub);
	strokeCircle (context, centre, hub, Palette::kHubEdge, 0.8);
	for (const auto& s : mGrit)   // grit round the hub, turning with the beak
		fillCircle (context, pointFromTop (centre, hub * s.radius, a + s.angle), s.r,
		            wearColour (Wear::kKnurlDust, s.alpha));

	auto beak = owned (context->createGraphicsPath ());
	if (beak)
	{
		const CPoint tip = pointFromTop (centre, mSel.radius + 3.0, a);
		const CPoint left = pointFromTop (centre, 8.0, a - 90.0);
		const CPoint right = pointFromTop (centre, 8.0, a + 90.0);
		const CPoint tail = pointFromTop (centre, mSel.radius, a + 180.0);
		beak->beginSubpath (tip);
		beak->addLine (right);
		// The tail, a quadratic through `tail`, as a cubic.
		const CPoint c1 (right.x + 2.0 / 3.0 * (tail.x - right.x), right.y + 2.0 / 3.0 * (tail.y - right.y));
		const CPoint c2 (left.x + 2.0 / 3.0 * (tail.x - left.x), left.y + 2.0 / 3.0 * (tail.y - left.y));
		beak->addBezierCurve (c1, c2, left);
		beak->closeSubpath ();
		fillCap (context, beak, centre, mSel.radius);
		context->setFrameColor (CColor (0, 0, 0, 255));
		context->setLineWidth (0.8);
		context->setLineStyle (CLineStyle (CLineStyle::kLineCapRound, CLineStyle::kLineJoinRound));
		context->drawGraphicsPath (beak, CDrawContext::kPathStroked);
		drawLine (context, pointFromTop (centre, 2.0, a), tip, Palette::kInk, 1.8, true);
	}

	// ---- its name, under it ---------------------------------------------------
	if (mSel.name && *mSel.name)
		drawText (context, mSel.name, mSel.cx, mSel.cy + mSel.nameDy, kTextLabel, Palette::kInk, Align::Centre);

	setDirty (false);
}

void SpySelector::onMouseDownEvent (MouseDownEvent& event)
{
	if (! event.buttonState.isLeft () || mSel.count < 2)
		return;
	const int picked = Layout::selectorPositionAt (mSel, event.mousePosition.x, event.mousePosition.y);
	select (picked >= 0 ? picked : (currentIndex () + 1) % mSel.count);
	event.consumed = true;
}

void SpySelector::onMouseUpEvent (MouseUpEvent& event)
{
	event.consumed = true;
}

void SpySelector::onMouseWheelEvent (MouseWheelEvent& event)
{
	if (event.deltaY > 0.)
		select (currentIndex () + 1);
	else if (event.deltaY < 0.)
		select (currentIndex () - 1);
	event.consumed = true;
}

//========================================================================
// SpySegment
//========================================================================
SpySegment::SpySegment (const CRect& size, IControlListener* listener, int32_t tag)
: CControl (size, listener, tag, nullptr)
{
}

void SpySegment::setNames (const std::vector<std::string>& names)
{
	mNames = names;
	invalid ();
}

int SpySegment::currentIndex () const
{
	const int count = static_cast<int> (mNames.size ());
	if (count <= 1)
		return 0;
	const int index = static_cast<int> (std::lround (getValueNormalized () * (count - 1)));
	return std::clamp (index, 0, count - 1);
}

std::vector<CRect> SpySegment::segmentRects () const
{
	std::vector<CRect> out;
	const CRect r = getViewSize ();
	const double padding = 9.0, gap = 6.0;
	double x = r.left;
	for (const std::string& name : mNames)
	{
		const double w = textWidth (name, Layout::kTextScreen) + 2.0 * padding;
		out.emplace_back (x, r.top + 2.0, x + w, r.bottom - 2.0);
		x += w + gap;
	}
	return out;
}

void SpySegment::draw (CDrawContext* context)
{
	const auto rects = segmentRects ();
	const int selected = currentIndex ();
	for (std::size_t i = 0; i < rects.size (); ++i)
	{
		const bool on = static_cast<int> (i) == selected;
		if (on)
			strokeRoundRect (context, rects[i], 2.0, mColour, 1.0);
		drawText (context, mNames[i], rects[i].getCenter ().x, rects[i].getCenter ().y, Layout::kTextScreen,
		          on ? mColour : Palette::kCaption, Align::Centre);
	}
	setDirty (false);
}


void SpySegment::onMouseDownEvent (MouseDownEvent& event)
{
	if (! event.buttonState.isLeft () || mNames.size () < 2)
		return;

	const auto rects = segmentRects ();
	for (std::size_t i = 0; i < rects.size (); ++i)
	{
		CRect hit = rects[i];
		hit.top = getViewSize ().top;
		hit.bottom = getViewSize ().bottom;
		if (! hit.pointInside (event.mousePosition))
			continue;

		const float value = static_cast<float> (i) / static_cast<float> (mNames.size () - 1);
		if (value != getValueNormalized ())
		{
			beginEdit ();
			setValueNormalized (value);
			valueChanged ();
			endEdit ();
			invalid ();
		}
		break;
	}
	event.consumed = true;
}

void SpySegment::onMouseWheelEvent (MouseWheelEvent& event)
{
	const int count = static_cast<int> (mNames.size ());
	if (count < 2 || event.deltaY == 0.)
		return;
	const int next = std::clamp (currentIndex () + (event.deltaY > 0. ? 1 : -1), 0, count - 1);
	beginEdit ();
	setValueNormalized (static_cast<float> (next) / static_cast<float> (count - 1));
	valueChanged ();
	endEdit ();
	invalid ();
	event.consumed = true;
}

//========================================================================
// SpyTabs
//========================================================================
SpyTabs::SpyTabs (const CRect& size, const std::vector<std::string>& names)
: CView (size), mNames (names)
{
}

void SpyTabs::setSelected (int index)
{
	index = std::clamp (index, 0, static_cast<int> (mNames.size ()) - 1);
	if (index == mSelected)
		return;
	mSelected = index;
	invalid ();
}

void SpyTabs::draw (CDrawContext* context)
{
	const auto& t = Layout::kCentreTitles[std::clamp (mSelected, 0, 2)];
	drawSectionTitle (context, Layout::kSections[Layout::kCentreSection], t.title, t.subtitle);
	for (std::size_t i = 0; i < mNames.size (); ++i)
	{
		const bool on = static_cast<int> (i) == mSelected;
		const CRect r = toCRect (Layout::tabRect (static_cast<int> (i)));
		fillRoundRect (context, r, 3.0, Palette::kButton);
		strokeRoundRect (context, r, 3.0, on ? Palette::kSectionEdge : Palette::kDivider, 1.2);
		drawText (context, mNames[i], r.getCenter ().x, r.getCenter ().y - 0.5, Layout::kTextButton.cap,
		          on ? Palette::kInk : Palette::kInkButton, Align::Centre,
		          on ? SciFi::kBold : SciFi::kRegular);
		if (on)
			drawLine (context, CPoint (r.left + 6.0, r.bottom - 3.0), CPoint (r.right - 6.0, r.bottom - 3.0),
			          Palette::kRed, 1.6);
	}
	setDirty (false);
}

void SpyTabs::onMouseDownEvent (MouseDownEvent& event)
{
	if (! event.buttonState.isLeft ())
		return;
	for (std::size_t i = 0; i < mNames.size (); ++i)
	{
		if (! Layout::tabRect (static_cast<int> (i)).contains (event.mousePosition.x, event.mousePosition.y))
			continue;
		if (static_cast<int> (i) != mSelected)
		{
			setSelected (static_cast<int> (i));
			if (mOnChange)
				mOnChange (static_cast<int> (i));
		}
		break;
	}
	event.consumed = true;
}

//========================================================================
// SpyPinBoard
//========================================================================
namespace {
constexpr double kRowLabelWidth = 28.0;
constexpr double kColumnLabelHeight = 30.0;
} // namespace

SpyPinBoard::SpyPinBoard (const CRect& size, IPatchBoardListener* listener, bool labels)
: CView (size), mListener (listener), mLabels (labels)
{
	setWantsFocus (true);
}

void SpyPinBoard::setBands (int bands)
{
	if (bands <= 0 || bands > 22 || bands == mBands)
		return;
	mBands = bands;
	invalid ();
}

double SpyPinBoard::pitch () const
{
	const CRect r = getViewSize ();
	const double w = r.getWidth () - (mLabels ? kRowLabelWidth : 0.0) - 2.0;
	const double h = r.getHeight () - (mLabels ? kColumnLabelHeight : 0.0) - 2.0;
	return std::floor (std::min (w, h) / mBands);
}

CRect SpyPinBoard::gridRect () const
{
	const CRect r = getViewSize ();
	const double side = pitch () * mBands;
	const double left = r.left + (mLabels ? kRowLabelWidth : 0.0) + 1.0;
	return CRect (left, r.top + 1.0, left + side, r.top + 1.0 + side);
}

void SpyPinBoard::draw (CDrawContext* context)
{
	if (mListener == nullptr)
		return;

	const CRect grid = gridRect ();
	const double p = pitch ();

	// A printed rule every three bands, so a cell can be found by counting.
	for (int i = 3; i < mBands; i += 3)
	{
		const double x = grid.left + i * p;
		const double y = grid.top + i * p;
		drawLine (context, CPoint (x, grid.top + 2.0), CPoint (x, grid.bottom - 2.0), CColor (0x2e, 0x2e, 0x2e, 255));
		drawLine (context, CPoint (grid.left + 2.0, y), CPoint (grid.right - 2.0, y), CColor (0x2e, 0x2e, 0x2e, 255));
	}

	const double radius = std::clamp (p * 0.27, 2.0, 6.0);
	for (int row = 0; row < mBands; ++row)
	{
		for (int column = 0; column < mBands; ++column)
		{
			// Rows are the modulator, down the board; columns the carrier.
			const CPoint c (grid.left + (column + 0.5) * p, grid.top + (row + 0.5) * p);
			drawPin (context, c, radius, mListener->patchValue (row, column));
		}
	}

	if (mLabels)
	{
		const double cap = std::clamp (p * 0.32, 3.6, Layout::kTextAxis.cap);
		for (int i = 0; i < mBands; ++i)
		{
			const std::string label = mListener->bandLabel (i);
			drawText (context, label, grid.left - 6.0, grid.top + (i + 0.5) * p, cap, Palette::kInk, Align::Right);
			drawTextRotated (context, label, CPoint (grid.left + (i + 0.5) * p - cap * 0.4, grid.bottom + 6.0),
			                 60.0, cap, Palette::kInk);
		}
	}
	setDirty (false);
}

bool SpyPinBoard::cellAt (const CPoint& where, int& row, int& column) const
{
	const CRect grid = gridRect ();
	const double p = pitch ();
	if (p <= 0.0 || ! grid.pointInside (where))
		return false;
	column = static_cast<int> ((where.x - grid.left) / p);
	row = static_cast<int> ((where.y - grid.top) / p);
	return row >= 0 && row < mBands && column >= 0 && column < mBands;
}

void SpyPinBoard::set (double value)
{
	mListener->setPatchValue (mRow, mColumn, value);
	if (value > 0.0)
		mListener->setRestoreValue (mRow, mColumn, value);
	invalid ();
}

void SpyPinBoard::onMouseDownEvent (MouseDownEvent& event)
{
	if (! event.buttonState.isLeft () || mListener == nullptr)
		return;
	if (! cellAt (event.mousePosition, mRow, mColumn))
		return;

	mDragging = true;
	mMoved = false;
	mAnchorY = event.mousePosition.y;
	mAnchorValue = mListener->patchValue (mRow, mColumn);
	mListener->beginPatchEdit (mRow, mColumn);
	event.consumed = true;
}

void SpyPinBoard::onMouseMoveEvent (MouseMoveEvent& event)
{
	if (! mDragging || mListener == nullptr)
		return;

	const double dy = mAnchorY - event.mousePosition.y;
	if (! mMoved && std::fabs (dy) < 4.0)
	{
		event.consumed = true;
		return;
	}

	if (! mMoved)
	{
		// A drag on an open cell starts from where a click would put it.
		mMoved = true;
		if (mAnchorValue <= 0.0)
			mAnchorValue = mListener->restoreValue (mRow, mColumn);
	}

	double scale = kDragPerPixel;
	if (event.modifiers.has (ModifierKey::Shift))
		scale *= 0.1;
	// The DXi's floor was 0.01, not zero: dragging a cell down never
	// switched it off, only made it very quiet. A click does that.
	const double next = std::clamp (mAnchorValue + dy * scale, 0.01, 1.0);
	mAnchorValue = next;
	mAnchorY = event.mousePosition.y;
	set (next);
	event.consumed = true;
}

void SpyPinBoard::onMouseUpEvent (MouseUpEvent& event)
{
	if (! mDragging || mListener == nullptr)
		return;

	// A click that did not drag toggles the cell, restoring whatever it
	// was worth when it was switched off.
	if (! mMoved)
	{
		const double now = mListener->patchValue (mRow, mColumn);
		if (now > 0.0)
		{
			mListener->setRestoreValue (mRow, mColumn, now);
			mListener->setPatchValue (mRow, mColumn, 0.0);
		}
		else
		{
			mListener->setPatchValue (mRow, mColumn, mListener->restoreValue (mRow, mColumn));
		}
		invalid ();
	}

	mListener->endPatchEdit (mRow, mColumn);
	mDragging = false;
	event.consumed = true;
}

void SpyPinBoard::onMouseCancelEvent (MouseCancelEvent& event)
{
	if (mDragging && mListener)
		mListener->endPatchEdit (mRow, mColumn);
	mDragging = false;
	event.consumed = true;
}

void SpyPinBoard::onMouseWheelEvent (MouseWheelEvent& event)
{
	if (mListener == nullptr || event.deltaY == 0.)
		return;
	if (! cellAt (event.mousePosition, mRow, mColumn))
		return;

	const double now = mListener->patchValue (mRow, mColumn);
	mListener->beginPatchEdit (mRow, mColumn);
	set (std::clamp (now + (event.deltaY > 0. ? 0.01 : -0.01), 0.01, 1.0));
	mListener->endPatchEdit (mRow, mColumn);
	event.consumed = true;
}

//========================================================================
// SpyBandsDisplay
//========================================================================
SpyBandsDisplay::SpyBandsDisplay (const CRect& size)
: CView (size)
{
}

void SpyBandsDisplay::setState (const State& state)
{
	bool same = state.bands == mState.bands && state.bottomHz == mState.bottomHz
	         && state.topHz == mState.topHz && state.resonance == mState.resonance
	         && state.slopes == mState.slopes && state.sampleRate == mState.sampleRate
	         && state.mode == mState.mode;
	mState = state;
	if (! same)
		invalid ();
}

void SpyBandsDisplay::setEnvelopes (const double* values, int count)
{
	bool changed = false;
	for (int i = 0; i < 22; ++i)
	{
		const double v = (values && i < count) ? values[i] : 0.0;
		if (std::fabs (v - mEnvelope[i]) > 1e-4)
			changed = true;
		mEnvelope[i] = v;
	}
	if (changed)
		invalid ();
}

CRect SpyBandsDisplay::plotRect () const
{
	const CRect r = getViewSize ();
	return CRect (r.left + 1.0, r.top + 26.0, r.right - 1.0, r.bottom - 20.0);
}

double SpyBandsDisplay::xForHz (double hz) const
{
	const CRect p = plotRect ();
	const double t = std::log (std::max (hz, 1.0) / kLowHz) / std::log (kHighHz / kLowHz);
	return p.left + t * p.getWidth ();
}

double SpyBandsDisplay::hzForX (double x) const
{
	const CRect p = plotRect ();
	const double t = (x - p.left) / p.getWidth ();
	return kLowHz * std::pow (kHighHz / kLowHz, t);
}

int SpyBandsDisplay::handleAt (const CPoint& where) const
{
	const CRect p = plotRect ();
	if (where.y < p.top - 6.0 || where.y > p.bottom)
		return -1;
	const double xb = xForHz (mState.bottomHz);
	const double xt = xForHz (mState.topHz);
	const double db = std::fabs (where.x - xb), dt = std::fabs (where.x - xt);
	if (std::min (db, dt) > 9.0)
		return -1;
	return (db <= dt) ? 0 : 1;
}

void SpyBandsDisplay::draw (CDrawContext* context)
{
	using namespace Layout;
	const CRect r = getViewSize ();
	const CRect p = plotRect ();

	// The axis: a tick and a figure for each, along the foot of the glass.
	static const double kTicks[] = { 50, 100, 200, 500, 1000, 2000, 5000, 10000 };
	static const char* const kTickNames[] = { "50", "100", "200", "500", "1K", "2K", "5K", "10K" };
	for (int i = 0; i < 8; ++i)
	{
		const double x = std::floor (xForHz (kTicks[i])) + 0.5;
		drawLine (context, CPoint (x, p.bottom + 1.0), CPoint (x, p.bottom + 4.0), Palette::kAxisTick);
		drawText (context, kTickNames[i], x + 3.0, p.bottom + 11.0, kTextAxis, Palette::kCaption);
	}

	drawText (context, "AMBER = MODULATOR ENVELOPES \xE2\x80\x94 NOT A METER", r.left + 10.0, r.top + 12.0,
	          kTextScreen, Palette::kCaption);
	drawText (context, mState.mode, r.right - 10.0, r.top + 12.0, kTextScreen, Palette::kCarrier, Align::Right);

	context->saveGlobalState ();
	context->setClipRect (CRect (p.left, p.top - 2.0, p.right, p.bottom + 2.0));

	const double base = p.bottom;
	const double height = p.getHeight () * 0.88;
	const int bands = std::clamp (mState.bands, 1, 22);

	// The modulator envelopes, one bar per band, behind the curves. Not
	// normalised: a band at 1.0 reaches the top of its curve and a hotter
	// one runs off the plot, as the DXi's display did.
	double spacing = p.getWidth ();
	if (bands > 1 && mState.centreHz[0] > 0.0 && mState.centreHz[bands - 1] > 0.0)
		spacing = (xForHz (mState.centreHz[bands - 1]) - xForHz (mState.centreHz[0])) / (bands - 1);
	const double barWidth = std::clamp (spacing * 0.22, 3.0, 12.0);
	for (int b = 0; b < bands; ++b)
	{
		const double v = std::clamp (mEnvelope[b], 0.0, 1.15);
		if (v <= 0.002 || mState.centreHz[b] <= 0.0)
			continue;
		const double x = xForHz (mState.centreHz[b]);
		const CRect bar (x - barWidth / 2.0, base - v * height, x + barWidth / 2.0, base);
		CRect glow (bar);
		glow.extend (1.0, 1.0);
		context->setFillColor (Palette::withAlpha (Palette::kModulator, 0.25));
		context->drawRect (glow, kDrawFilled);
		context->setFillColor (Palette::withAlpha (Palette::kModulator, 0.88));
		context->drawRect (bar, kDrawFilled);
	}

	// Each band's response, sampled every two pixels: a blue trace with a
	// little bloom, filled with a gradient from the line's colour to black.
	auto gradient = owned (CGradient::create (0.0, 1.0, Palette::withAlpha (Palette::kCarrier, 0.30),
	                                          Palette::withAlpha (Palette::kCarrier, 0.02)));
	for (int b = 0; b < bands; ++b)
	{
		const double centre = mState.centreHz[b];
		if (centre <= 0.0)
			continue;

		auto path = owned (context->createGraphicsPath ());
		auto fill = owned (context->createGraphicsPath ());
		if (! path || ! fill)
			continue;

		bool started = false;
		for (double x = p.left; x <= p.right; x += 2.0)
		{
			const double m = bandResponse (hzForX (x), centre, mState.resonance,
			                               mState.slopes, mState.sampleRate);
			const CPoint pt (x, base - m * height);
			if (! started)
			{
				path->beginSubpath (pt);
				fill->beginSubpath (CPoint (x, base));
				fill->addLine (pt);
				started = true;
			}
			else
			{
				path->addLine (pt);
				fill->addLine (pt);
			}
		}
		fill->addLine (CPoint (p.right, base));
		fill->closeSubpath ();

		context->setDrawMode (kAntiAliasing | kNonIntegralMode);
		if (gradient)
			context->fillLinearGradient (fill, *gradient, CPoint (p.left, base - height), CPoint (p.left, base));
		strokeWithBloom (context, path, Palette::kCarrier, 1.4);
	}

	context->restoreGlobalState ();

	drawLine (context, CPoint (p.left, base + 0.5), CPoint (p.right, base + 0.5), Palette::kCarrier, 1.6);

	// The handles: white lines with a ring at the top, Top's dashed.
	const double xb = xForHz (mState.bottomHz);
	const double xt = xForHz (mState.topHz);
	const double handleY = p.top + 6.0;

	drawLine (context, CPoint (xb, handleY + 6.0), CPoint (xb, base), Palette::kInk, 1.2);
	fillCircle (context, CPoint (xb, handleY), 6.0, Palette::kGlass);
	strokeCircle (context, CPoint (xb, handleY), 6.0, Palette::kInk, 1.6);
	drawText (context, "BOTTOM", xb + 10.0, handleY, kTextScreen, Palette::kInk);

	drawDashedLine (context, CPoint (xt, handleY + 6.0), CPoint (xt, base), Palette::kInk, 1.2, 4.0, 3.0);
	fillCircle (context, CPoint (xt, handleY), 6.0, Palette::kGlass);
	strokeCircle (context, CPoint (xt, handleY), 6.0, Palette::kInk, 1.6);
	drawText (context, "TOP", xt - 10.0, handleY, kTextScreen, Palette::kInk, Align::Right);

	// The tooltip, while a handle is under the pointer or being dragged.
	const int which = (mDrag >= 0) ? mDrag : mHover;
	if (which >= 0)
	{
		const double hz = which == 0 ? mState.bottomHz : mState.topHz;
		char buffer[32];
		if (hz >= 1000.0)
			std::snprintf (buffer, sizeof (buffer), "%.2f KHZ", hz / 1000.0);
		else
			std::snprintf (buffer, sizeof (buffer), "%.0f HZ", hz);

		const std::string name = which == 0 ? "BOTTOM " : "TOP ";
		const std::string tail = " \xC2\xB7 DRAG \xC2\xB7 SHIFT = FINE";
		const Text& style = kTextScreen;
		const double w = textWidth (name, style) + textWidth (buffer, style) + textWidth (tail, style) + 20.0;
		double left = which == 0 ? xb + 30.0 : xt - 30.0 - w;
		left = std::clamp (left, r.left + 6.0, r.right - 6.0 - w);
		const CRect box (left, handleY + 20.0, left + w, handleY + 40.0);
		fillRoundRect (context, box, 3.0, Palette::kGlass);
		strokeRoundRect (context, box, 3.0, Palette::kTooltipEdge, 1.0);

		double x = box.left + 10.0;
		const double cy = box.getCenter ().y;
		x += drawText (context, name, x, cy, style, Palette::kInk) + textWidth (" ", style);
		x += drawText (context, buffer, x, cy, style, Palette::kCarrier);
		drawText (context, tail, x, cy, style, Palette::kInk);
	}

	setDirty (false);
}

void SpyBandsDisplay::onMouseDownEvent (MouseDownEvent& event)
{
	if (! event.buttonState.isLeft ())
		return;
	const int which = handleAt (event.mousePosition);
	if (which < 0)
		return;
	mDrag = which;
	mDragAnchorX = event.mousePosition.x;
	mDragAnchorHz = which == 0 ? mState.bottomHz : mState.topHz;
	if (mOnDrag)
		mOnDrag (which, mDragAnchorHz, 0);
	invalid ();
	event.consumed = true;
}

void SpyBandsDisplay::onMouseMoveEvent (MouseMoveEvent& event)
{
	if (mDrag < 0)
	{
		const int hover = handleAt (event.mousePosition);
		if (hover != mHover)
		{
			mHover = hover;
			invalid ();
		}
		return;
	}

	double scale = 1.0;
	if (event.modifiers.has (ModifierKey::Shift))
		scale = 0.1;
	const double x = xForHz (mDragAnchorHz) + (event.mousePosition.x - mDragAnchorX) * scale;
	mDragAnchorHz = hzForX (x);
	mDragAnchorX = event.mousePosition.x;
	if (mOnDrag)
		mOnDrag (mDrag, mDragAnchorHz, 1);
	invalid ();
	event.consumed = true;
}

void SpyBandsDisplay::onMouseUpEvent (MouseUpEvent& event)
{
	if (mDrag < 0)
		return;
	if (mOnDrag)
		mOnDrag (mDrag, mDragAnchorHz, 2);
	mDrag = -1;
	invalid ();
	event.consumed = true;
}

void SpyBandsDisplay::onMouseCancelEvent (MouseCancelEvent& event)
{
	if (mDrag >= 0 && mOnDrag)
		mOnDrag (mDrag, mDragAnchorHz, 2);
	mDrag = -1;
	invalid ();
	event.consumed = true;
}

void SpyBandsDisplay::onMouseExitEvent (MouseExitEvent& event)
{
	if (mHover >= 0)
	{
		mHover = -1;
		invalid ();
	}
	event.consumed = true;
}

//========================================================================
// SpySlotRow
//========================================================================
class SlotDropTarget : public DropTargetAdapter, public NonAtomicReferenceCounted
{
public:
	explicit SlotDropTarget (SpySlotRow* row) : mRow (row) {}

	DragOperation onDragEnter (DragEventData data) override
	{
		mPath = wavPath (data.drag);
		mRow->mDropHighlight = ! mPath.empty ();
		mRow->invalid ();
		return mPath.empty () ? DragOperation::None : DragOperation::Copy;
	}

	DragOperation onDragMove (DragEventData) override
	{
		return mPath.empty () ? DragOperation::None : DragOperation::Copy;
	}

	void onDragLeave (DragEventData) override
	{
		mRow->mDropHighlight = false;
		mRow->invalid ();
	}

	bool onDrop (DragEventData) override
	{
		mRow->mDropHighlight = false;
		mRow->invalid ();
		if (mPath.empty () || ! mRow->mOnDrop)
			return false;
		mRow->mOnDrop (mPath);
		return true;
	}

private:
	static std::string wavPath (IDataPackage* drag)
	{
		if (drag == nullptr)
			return {};
		for (uint32_t i = 0; i < drag->getCount (); ++i)
		{
			const void* buffer = nullptr;
			IDataPackage::Type type;
			const uint32_t size = drag->getData (i, buffer, type);
			if (type != IDataPackage::kFilePath || buffer == nullptr || size == 0)
				continue;
			std::string path (static_cast<const char*> (buffer), size);
			while (! path.empty () && path.back () == '\0')
				path.pop_back ();
			if (path.size () > 4)
			{
				std::string ext = path.substr (path.size () - 4);
				for (char& ch : ext)
					ch = static_cast<char> (std::tolower (static_cast<unsigned char> (ch)));
				if (ext == ".wav")
					return path;
			}
		}
		return {};
	}

	SpySlotRow* mRow;
	std::string mPath;
};

SpySlotRow::SpySlotRow (const CRect& size, IControlListener* listener, int32_t tag, int slot,
                        const Wear::GlassWear& glassWear)
: CControl (size, listener, tag, nullptr), mSlot (slot), mGlassWear (glassWear)
{
}

void SpySlotRow::setFile (const std::string& name, bool missing)
{
	if (name == mName && missing == mMissing)
		return;
	mName = name;
	mMissing = missing;
	invalid ();
}

void SpySlotRow::setOverview (const std::vector<float>& peaks)
{
	if (peaks == mPeaks)
		return;
	mPeaks = peaks;
	invalid ();
}

void SpySlotRow::setInactive (bool inactive)
{
	if (inactive == mInactive)
		return;
	mInactive = inactive;
	invalid ();
}

CRect SpySlotRow::lampRect () const
{
	const CRect r = getViewSize ();
	const double cy = r.top + 10.0;
	return CRect (r.left + 24.0, cy - 8.0, r.left + 40.0, cy + 8.0);
}

SharedPointer<IDropTarget> SpySlotRow::getDropTarget ()
{
	return makeOwned<SlotDropTarget> (this);
}

void SpySlotRow::draw (CDrawContext* context)
{
	using namespace Layout;
	const CRect r = getViewSize ();
	const bool empty = mName.empty ();
	const bool on = getValueNormalized () >= 0.5;
	const double cy = r.top + 10.0;

	if (mDropHighlight)
		strokeRoundRect (context, CRect (r.left - 4.0, r.top - 2.0, r.right + 4.0, r.bottom), 3.0,
		                 Palette::kSectionEdge, 1.0);

	// The number, in a printed ring.
	const CPoint badge (r.left + 11.0, cy);
	strokeCircle (context, badge, 9.0, Palette::kSectionEdge, 1.2);
	char number[2] = { static_cast<char> ('1' + mSlot), 0 };
	drawText (context, number, badge.x, badge.y, kTextLabel, Palette::kInk, Align::Centre);

	// The lamp: lit only when the slot is on AND its file is loaded - what
	// will actually play.
	const bool lit = on && ! empty && ! mMissing && ! mInactive;
	drawLamp (context, lampRect ().getCenter (), 3.5, lit);

	const double textX = r.left + 44.0;
	const Rect glass = { r.left + 28.0, r.top + 22.0, r.right, r.top + 44.0 };
	if (empty)
	{
		drawText (context, "EMPTY \xE2\x80\x93 DROP A .WAV", textX, cy, kTextNote, Palette::kInkFaint);
		const CCoord dashes[] = { 3.0, 3.0 };
		auto path = owned (context->createRoundRectGraphicsPath (toCRect (glass), 3.0));
		if (path)
		{
			context->setLineStyle (CLineStyle (CLineStyle::kLineCapButt, CLineStyle::kLineJoinMiter, 0.0, 2, dashes));
			context->setLineWidth (1.0);
			context->setFrameColor (Palette::kRule);
			context->drawGraphicsPath (path, CDrawContext::kPathStroked);
		}
		setDirty (false);
		return;
	}

	std::string label = mName;
	if (mMissing)
		label += " \xE2\x80\x94 MISSING";
	const CColor colour = mMissing ? Palette::kMissing : (mInactive ? Palette::kInkDim : Palette::kInk);
	// Long names are cut to the row rather than running under the panel edge.
	while (label.size () > 4 && textWidth (label, kTextNote) > r.right - textX)
		label = label.substr (0, label.size () - 4) + "...";
	drawText (context, label, textX, cy, Layout::Text { kTextNote.cap, SciFi::kBold }, colour);

	// The waveform, behind its own little glass: each column a peak,
	// mirrored about the centre line, amber if it will play.
	drawBezel (context, glass, false);
	if (! mPeaks.empty ())
	{
		const double left = glass.left + 6.0, right = glass.right - 6.0;
		const double mid = (glass.top + glass.bottom) / 2.0, half = (glass.bottom - glass.top) / 2.0 - 2.0;
		const std::size_t n = mPeaks.size ();
		auto path = owned (context->createGraphicsPath ());
		if (path)
		{
			for (std::size_t i = 0; i < n; ++i)
			{
				const double x = left + (right - left) * i / (n - 1 > 0 ? n - 1 : 1);
				const double y = mid - std::max (0.6, half * mPeaks[i]);
				if (i == 0)
					path->beginSubpath (CPoint (x, y));
				else
					path->addLine (CPoint (x, y));
			}
			for (std::size_t i = n; i-- > 0;)
			{
				const double x = left + (right - left) * i / (n - 1 > 0 ? n - 1 : 1);
				path->addLine (CPoint (x, mid + std::max (0.6, half * mPeaks[i])));
			}
			path->closeSubpath ();
			const CColor wave = lit ? Palette::kModulator : Palette::kQuiet;
			context->setDrawMode (kAntiAliasing | kNonIntegralMode);
			context->setFrameColor (Palette::withAlpha (wave, 0.3));
			context->setLineWidth (1.6);
			context->setLineStyle (CLineStyle (CLineStyle::kLineCapRound, CLineStyle::kLineJoinRound));
			context->drawGraphicsPath (path, CDrawContext::kPathStroked);
			context->setFillColor (wave);
			context->drawGraphicsPath (path, CDrawContext::kPathFilled);
		}
	}
	drawGlass (context, glass, mGlassWear);
	setDirty (false);
}

void SpySlotRow::onMouseDownEvent (MouseDownEvent& event)
{
	if (isContextClick (event))
	{
		if (mOnContext)
			mOnContext ();
		event.consumed = true;
		return;
	}
	if (! event.buttonState.isLeft ())
		return;

	CRect lamp = lampRect ();
	lamp.left = getViewSize ().left;
	if (lamp.pointInside (event.mousePosition))
	{
		beginEdit ();
		setValueNormalized (getValueNormalized () >= 0.5 ? 0.f : 1.f);
		valueChanged ();
		endEdit ();
		invalid ();
	}
	else if (mOnChoose)
	{
		mOnChoose ();
	}
	event.consumed = true;
}

//========================================================================
// SpyLevelBar
//========================================================================
SpyLevelBar::SpyLevelBar (const CRect& size, const std::string& label, const CColor& colour)
: CView (size), mLabel (label), mColour (colour)
{
}

void SpyLevelBar::setLevel (double linearPeak)
{
	const double db = std::max (kFloorDb, toDb (linearPeak));
	const double before = mDb, beforePeak = mPeakDb;

	// Instant attack, 24 dB a second of fall at thirty frames, and a peak
	// marker held for a second.
	mDb = (db > mDb) ? db : std::max (db, mDb - 0.8);
	if (db >= mPeakDb)
	{
		mPeakDb = db;
		mHold = 30;
	}
	else if (mHold > 0)
	{
		--mHold;
	}
	else
	{
		mPeakDb = std::max (kFloorDb, mPeakDb - 0.8);
	}

	if (std::fabs (before - mDb) > 0.05 || std::fabs (beforePeak - mPeakDb) > 0.05)
		invalid ();
}

void SpyLevelBar::draw (CDrawContext* context)
{
	using namespace Layout;
	const CRect r = getViewSize ();
	const double cy = r.getCenter ().y;

	drawText (context, mLabel, r.left, cy, kTextScreen, Palette::kCaption);

	// Twenty-six segments from -60 dB to 0, and the peak's segment lit
	// white when it is above the bar.
	constexpr int kSegments = 26;
	const double left = r.left + 14.0, right = r.right - 44.0;
	const double pitch = (right - left) / kSegments;
	const double t = std::clamp ((mDb - kFloorDb) / -kFloorDb, 0.0, 1.0);
	const double tp = std::clamp ((mPeakDb - kFloorDb) / -kFloorDb, 0.0, 1.0);
	const int peakSeg = (tp > 0.0) ? std::min (kSegments - 1, static_cast<int> (tp * kSegments)) : -1;
	for (int j = 0; j < kSegments; ++j)
	{
		const CRect seg (left + j * pitch, cy - 4.0, left + (j + 1) * pitch - 2.0, cy + 4.0);
		const bool lit = (j + 0.5) / kSegments < t;
		if (lit)
		{
			CRect glow (seg);
			glow.extend (1.0, 1.0);
			context->setFillColor (Palette::withAlpha (mColour, 0.25));
			context->drawRect (glow, kDrawFilled);
		}
		context->setFillColor (lit ? mColour : (j == peakSeg ? Palette::withAlpha (mColour, 0.55) : Palette::kSegmentOff));
		context->drawRect (seg, kDrawFilled);
	}

	char buffer[24];
	if (mDb <= kFloorDb + 0.05)
		std::snprintf (buffer, sizeof (buffer), "-60 DB");
	else
		std::snprintf (buffer, sizeof (buffer), "%.0f DB", mDb);
	mShown = buffer;
	drawText (context, mShown, r.right, cy, kTextScreen, mColour, Align::Right);
	setDirty (false);
}

//========================================================================
// SpyOutMeter
//========================================================================
SpyOutMeter::SpyOutMeter (const CRect& size)
: CView (size)
{
	setMouseEnabled (false);
}

void SpyOutMeter::setLevels (double left, double right)
{
	const double in[2] = { left, right };
	bool changed = false;
	for (int i = 0; i < 2; ++i)
	{
		const double db = std::max (-60.0, toDb (in[i]));
		const double t = (db + 60.0) / 60.0;
		const double next = (t > mLevel[i]) ? t : std::max (t, mLevel[i] - 0.8 / 60.0);
		if (std::fabs (next - mLevel[i]) > 0.002)
			changed = true;
		mLevel[i] = next;
	}
	if (changed)
		invalid ();
}

void SpyOutMeter::draw (CDrawContext* context)
{
	// Fourteen segments a column, 4 dB each from -56 to 0; the top three
	// (above -12 dB) are red. Two columns, 12 wide, 8 apart.
	constexpr int kSegments = 14;
	const CRect r = getViewSize ();
	const double colW = 12.0, gap = 8.0;
	const double left = r.getCenter ().x - colW - gap / 2.0;
	const double pitch = (r.getHeight () - 8.0) / kSegments;
	for (int c = 0; c < 2; ++c)
	{
		const double x = left + c * (colW + gap);
		for (int j = 0; j < kSegments; ++j)
		{
			const double bottom = r.bottom - 4.0 - j * pitch;
			const CRect seg (x, bottom - pitch + 1.6, x + colW, bottom);
			const bool lit = (static_cast<double> (j) + 0.5) / kSegments < mLevel[c];
			const CColor colour = (j >= kSegments - 3) ? Palette::kRed : Palette::kOutput;
			if (lit)
			{
				CRect glow (seg);
				glow.extend (1.2, 1.2);
				context->setFillColor (Palette::withAlpha (colour, 0.25));
				context->drawRect (glow, kDrawFilled);
			}
			context->setFillColor (lit ? colour : Palette::kSegmentOff);
			context->drawRect (seg, kDrawFilled);
		}
	}
	setDirty (false);
}

//========================================================================
// SpyCorrelationLamp
//========================================================================
SpyCorrelationLamp::SpyCorrelationLamp (const CRect& size)
: CView (size)
{
}

void SpyCorrelationLamp::setCorrelation (double c)
{
	const bool wasLit = mLit;
	const bool hadReading = mHaveReading;
	const double before = mSmoothed;

	if (c < -1.5)
	{
		mHaveReading = false;
		mAbove = 0;
		mLit = false;
	}
	else
	{
		mSmoothed = mHaveReading ? mSmoothed + 0.25 * (c - mSmoothed) : c;
		mHaveReading = true;
		mAbove = (mSmoothed > kSameAbove) ? std::min (mAbove + 1, kHoldFrames) : 0;
		mLit = mAbove >= kHoldFrames;
	}

	if (wasLit != mLit || hadReading != mHaveReading || std::fabs (before - mSmoothed) > 0.004)
		invalid ();
}

void SpyCorrelationLamp::draw (CDrawContext* context)
{
	using namespace Layout;
	const CRect r = getViewSize ();
	const double cy = r.getCenter ().y;
	drawLamp (context, CPoint (r.left + 5.0, cy), 4.0, mLit);
	if (mOnGlass)
		drawText (context, "SAME AUDIO ON L AND R", r.left + 16.0, cy, kTextScreen,
		          mLit ? Palette::kInk : Palette::kCaption);
	else
		drawText (context, "SAME AUDIO ON L AND R", r.left + 16.0, cy, kTextLabel, Palette::kInk);

	if (mShowReading)
	{
		char buffer[24];
		if (mHaveReading)
			std::snprintf (buffer, sizeof (buffer), "CORR %.2f", mSmoothed);
		else
			std::snprintf (buffer, sizeof (buffer), "CORR --");
		mShown = buffer;
		drawText (context, mShown, r.right, cy, kTextScreen, mOnGlass ? Palette::kCarrier : Palette::kInkDim,
		          Align::Right);
	}
	setDirty (false);
}

//========================================================================
// SpyButton
//========================================================================
SpyButton::SpyButton (const CRect& size, const std::string& text, Shape shape)
: CView (size), mText (text), mShape (shape)
{
}

void SpyButton::setHighlighted (bool on)
{
	if (on == mHighlighted)
		return;
	mHighlighted = on;
	invalid ();
}

void SpyButton::draw (CDrawContext* context)
{
	const CRect r = getViewSize ();
	fillRoundRect (context, r, 3.0, mPressed ? Palette::kButtonDown : Palette::kButton);
	strokeRoundRect (context, r, 3.0, mHighlighted ? Palette::kSectionEdge : Palette::kDivider, 1.2);
	const bool bold = mHighlighted || mCap >= Layout::kTextHeader.cap;
	drawText (context, mText, r.getCenter ().x, r.getCenter ().y - (mHighlighted ? 0.5 : 0.0), mCap,
	          mHighlighted || bold ? Palette::kInk : Palette::kInkButton, Align::Centre,
	          bold ? SciFi::kBold : SciFi::kRegular);
	if (mHighlighted)
		drawLine (context, CPoint (r.left + 6.0, r.bottom - 3.0), CPoint (r.right - 6.0, r.bottom - 3.0),
		          Palette::kRed, 1.6);
	setDirty (false);
}

void SpyButton::onMouseDownEvent (MouseDownEvent& event)
{
	if (! event.buttonState.isLeft ())
		return;
	mPressed = true;
	invalid ();
	event.consumed = true;
}

void SpyButton::onMouseUpEvent (MouseUpEvent& event)
{
	if (! mPressed)
		return;
	mPressed = false;
	invalid ();
	if (getViewSize ().pointInside (event.mousePosition) && mOnClick)
		mOnClick ();
	event.consumed = true;
}

void SpyButton::onMouseCancelEvent (MouseCancelEvent& event)
{
	mPressed = false;
	invalid ();
	event.consumed = true;
}

//========================================================================
// SpyPresetBox
//========================================================================
SpyPresetBox::SpyPresetBox (const CRect& size)
: CView (size)
{
}

void SpyPresetBox::setName (const std::string& bank, const std::string& name, bool modified)
{
	if (bank == mBank && name == mName && modified == mModified)
		return;
	mBank = bank;
	mName = name;
	mModified = modified;
	invalid ();
}

void SpyPresetBox::draw (CDrawContext* context)
{
	const CRect r = getViewSize ();
	fillRoundRect (context, r, 3.0, Palette::kWindow);
	strokeRoundRect (context, r, 3.0, Palette::kWindowEdge, 1.0);

	const Layout::Text& style = Layout::kTextHeader;
	std::string text = mBank + " / " + mName + (mModified ? " *" : "");
	// A long user preset name is cut to the window rather than running
	// under the arrow.
	const double room = r.getWidth () - 12.0 - 30.0;
	while (text.size () > 4 && textWidth (text, style) > room)
		text = text.substr (0, text.size () - 4) + "...";
	const double cy = r.getCenter ().y;
	drawText (context, text, r.left + 12.0, cy, style, Palette::kInk);
	drawText (context, "\xE2\x8C\x84", r.right - 12.0, cy, style, Palette::kInkDim, Align::Right);
	setDirty (false);
}

void SpyPresetBox::onMouseDownEvent (MouseDownEvent& event)
{
	if (! event.buttonState.isLeft () && ! event.buttonState.isRight ())
		return;
	if (mOnClick)
		mOnClick ();
	event.consumed = true;
}

//========================================================================
// SpyPill
//========================================================================
SpyPill::SpyPill (const CRect& size, IControlListener* listener, int32_t tag,
                  const std::string& text, const CColor& lampColour, bool clickable)
: CControl (size, listener, tag, nullptr), mText (text), mLamp (lampColour), mClickable (clickable)
{
}

void SpyPill::draw (CDrawContext* context)
{
	const CRect r = getViewSize ();
	const bool on = getValueNormalized () >= 0.5;
	fillRoundRect (context, r, 3.0, Palette::kButton);
	strokeRoundRect (context, r, 3.0, (on && mClickable) ? mLamp : Palette::kDivider, 1.2);

	const double cy = r.getCenter ().y;
	drawLamp (context, CPoint (r.left + 16.0, cy), 4.0, on, mLamp);
	drawText (context, mText, r.left + 28.0, cy, Layout::kTextHeader, Palette::kInk);
	setDirty (false);
}

void SpyPill::onMouseDownEvent (MouseDownEvent& event)
{
	if (! mClickable || ! event.buttonState.isLeft ())
		return;
	beginEdit ();
	setValueNormalized (getValueNormalized () >= 0.5 ? 0.f : 1.f);
	valueChanged ();
	endEdit ();
	invalid ();
	event.consumed = true;
}

//========================================================================
// SpyLamp
//========================================================================
SpyLamp::SpyLamp (const CRect& size, const CColor& colour)
: CView (size), mColour (colour)
{
	setMouseEnabled (false);
}

void SpyLamp::setLit (bool lit)
{
	if (lit == mLit)
		return;
	mLit = lit;
	invalid ();
}

void SpyLamp::draw (CDrawContext* context)
{
	const CRect r = getViewSize ();
	const double radius = std::min (r.getWidth (), r.getHeight ()) / 4.5;
	drawLamp (context, r.getCenter (), radius, mLit, mColour);
	setDirty (false);
}

//========================================================================
// SpyLabel
//========================================================================
SpyLabel::SpyLabel (const CRect& size, double cap, const CColor& colour, Align align, double weight)
: CView (size), mCap (cap), mColour (colour), mAlign (align), mWeight (weight)
{
	setMouseEnabled (false);
}

void SpyLabel::setText (const std::string& text)
{
	if (text == mText)
		return;
	mText = text;
	invalid ();
}

void SpyLabel::setColour (const CColor& colour)
{
	if (colour == mColour)
		return;
	mColour = colour;
	invalid ();
}

void SpyLabel::draw (CDrawContext* context)
{
	const CRect r = getViewSize ();
	const double x = mAlign == Align::Left ? r.left : (mAlign == Align::Right ? r.right : r.getCenter ().x);
	drawText (context, mText, x, r.getCenter ().y, mCap, mColour, mAlign, mWeight);
	setDirty (false);
}

//========================================================================
// SpyGlass
//========================================================================
SpyGlass::SpyGlass (const Layout::Rect& glass, const Wear::GlassWear& wear)
: CView (toCRect (glass)), mGlass (glass), mWear (wear)
{
	setMouseEnabled (false);
}

void SpyGlass::draw (CDrawContext* context)
{
	drawGlass (context, mGlass, mWear);
	setDirty (false);
}

//========================================================================
// SpyBackground
//========================================================================
SpyBackground::SpyBackground (const CRect& size)
: CView (size)
, mWear (Wear::panelWear ())
{
	setMouseEnabled (false);
}

void SpyBackground::addSection (int index)
{
	Item item;
	item.kind = Item::Section;
	item.index = index;
	item.rect = Layout::kSections[index].r;
	mItems.push_back (item);
}

void SpyBackground::addBezel (const Layout::Rect& glass, bool dots)
{
	Item item;
	item.kind = Item::Bezel;
	item.rect = glass;
	item.dots = dots;
	mItems.push_back (item);
}

void SpyBackground::addText (const std::string& text, double x, double centreY, const Layout::Text& style,
                             const CColor& colour, Align align)
{
	Item item;
	item.kind = Item::Text;
	item.text = text;
	item.a = CPoint (x, centreY);
	item.style = style;
	item.colour = colour;
	item.align = align;
	mItems.push_back (item);
}

void SpyBackground::addRule (const CPoint& a, const CPoint& b, const CColor& colour)
{
	Item item;
	item.kind = Item::Rule;
	item.a = a;
	item.b = b;
	item.colour = colour;
	mItems.push_back (item);
}

void SpyBackground::addPin (const CPoint& c, double radius, double value)
{
	Item item;
	item.kind = Item::Pin;
	item.a = c;
	item.radius = radius;
	item.value = value;
	mItems.push_back (item);
}

void SpyBackground::drawItem (CDrawContext* context, const Item& item)
{
	switch (item.kind)
	{
		case Item::Section:
			// The centre's title follows the tab: SpyTabs prints it.
			drawSection (context, Layout::kSections[item.index], item.index != Layout::kCentreSection);
			break;
		case Item::Bezel:
			drawBezel (context, item.rect, item.dots);
			break;
		case Item::Text:
			drawText (context, item.text, item.a.x, item.a.y, item.style, item.colour, item.align);
			break;
		case Item::Rule:
			drawLine (context, item.a, item.b, item.colour);
			break;
		case Item::Pin:
			drawPin (context, item.a, item.radius, item.value);
			break;
	}
}

void SpyBackground::drawWear (CDrawContext* context)
{
	CRect clip;
	context->getClipRect (clip);

	// ---- the haze and the drifts: soft, underneath the sharper marks ---
	for (const auto& b : mWear.haze)
		if (touches (clip, b.x - b.r, b.y - b.r, b.x + b.r, b.y + b.r))
			softDisc (context, CPoint (b.x, b.y), b.r, Wear::kDust, b.alpha);
	for (const auto& d : mWear.drifts)
		if (touches (clip, d.r.left, d.r.top, d.r.right, d.r.bottom))
			drift (context, d);
	for (const auto& f : mWear.rings)
		if (touches (clip, f.cx - f.outer, f.cy - f.outer, f.cx + f.outer, f.cy + f.outer))
			footRing (context, f);

	// ---- hairlines through the paint and the printing ------------------
	for (const auto& l : mWear.scratches)
		if (lineTouches (clip, l.pts, l.width))
			wearLine (context, l, Wear::kScratch);

	// ---- specks and fibres ---------------------------------------------
	for (const auto& sp : mWear.specks)
		if (touches (clip, sp.x - sp.r, sp.y - sp.r, sp.x + sp.r, sp.y + sp.r))
			fillCircle (context, CPoint (sp.x, sp.y), sp.r, wearColour (Wear::kSpeck, sp.alpha));
	for (const auto& l : mWear.fibres)
		if (lineTouches (clip, l.pts, l.width))
			wearLine (context, l, Wear::kSpeck);
}

void SpyBackground::draw (CDrawContext* context)
{
	using namespace Layout;
	const CRect r = getViewSize ();
	context->setDrawMode (kAntiAliasing | kNonIntegralMode);

	// The meters redraw thirty times a second, and the background under
	// them is redrawn with them: only what touches the area being drawn
	// is drawn. (The clip is the update rectangle.)
	CRect clip;
	context->getClipRect (clip);
	auto inClip = [&clip] (double l, double t, double rr, double b) { return touches (clip, l - 3, t - 3, rr + 3, b + 3); };

	// ---- the ground, the header band and the white rule under it --------
	context->setFillColor (Palette::kGround);
	context->drawRect (r, kDrawFilled);
	if (inClip (r.left, r.top, r.right, kHeaderHeight))
	{
		context->setFillColor (Palette::kHeader);
		context->drawRect (CRect (r.left, r.top, r.right, r.top + kHeaderHeight), kDrawFilled);
		context->setFillColor (Palette::kHeaderRule);
		context->drawRect (CRect (r.left, kHeaderHeight - 2.0, r.right, kHeaderHeight), kDrawFilled);
		drawWordmark (context, kWordmarkLeft, kHeaderHeight / 2.0, Palette::kHeader);
		context->setFillColor (Palette::kDivider);
		context->drawRect (CRect (kHeaderDivider, 16, kHeaderDivider + 1, 38), kDrawFilled);
	}

	// ---- the corner screws ----------------------------------------------
	for (const auto& s : kScrews)
	{
		const CPoint c (s.x, s.y);
		fillCircle (context, c, 3.2, Palette::kScrew);
		strokeCircle (context, c, 3.2, CColor (0, 0, 0, 255), 0.6);
		drawLine (context, CPoint (c.x - 2.2, c.y - 2.2), CPoint (c.x + 2.2, c.y + 2.2), CColor (34, 34, 34, 255), 0.8);
	}

	// Two passes: the printing, then the wear over it, then the screens'
	// cut-outs - so the haze does not fog the glass.
	for (int pass = 0; pass < 2; ++pass)
	{
	if (pass == 1)
		drawWear (context);
	for (const Item& item : mItems)
	{
		if ((item.kind == Item::Bezel) != (pass == 1))
			continue;
		double l = item.rect.left, t = item.rect.top, rr = item.rect.right, b = item.rect.bottom;
		switch (item.kind)
		{
			case Item::Section: break;
			case Item::Bezel:
			{
				const Layout::Rect z = bezelOf (item.rect);
				l = z.left; t = z.top; rr = z.right; b = z.bottom;
				break;
			}
			case Item::Text:
			{
				const double w = item.style.width (item.text);
				l = item.align == Align::Left ? item.a.x : (item.align == Align::Right ? item.a.x - w : item.a.x - w / 2.0);
				rr = l + w;
				t = item.a.y - item.style.cap;
				b = item.a.y + item.style.cap;
				break;
			}
			case Item::Rule:
				l = std::min (item.a.x, item.b.x); rr = std::max (item.a.x, item.b.x);
				t = std::min (item.a.y, item.b.y); b = std::max (item.a.y, item.b.y);
				break;
			case Item::Pin:
				l = item.a.x - item.radius - 2; rr = item.a.x + item.radius + 2;
				t = item.a.y - item.radius - 2; b = item.a.y + item.radius + 2;
				break;
		}
		if (inClip (l, t, rr, b))
			drawItem (context, item);
	}
	}

	setDirty (false);
}

//------------------------------------------------------------------------
} // namespace SpyBand
