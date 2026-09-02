//------------------------------------------------------------------------
// SpyBand - custom VSTGUI controls
//------------------------------------------------------------------------

#include "SpyBandControls.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

using namespace VSTGUI;

namespace SpyBand {

namespace {

/** The DXi's bar geometry, in pixels from the control's own edges
    (SlideSpin::PaintBk). */
constexpr CCoord kBarBottomInset = 3.;
constexpr CCoord kBarHeight      = 12.;   // rect.bottom-15 .. rect.bottom-3
constexpr CCoord kLabelTop       = 17.;   // rect.bottom-17
constexpr CCoord kLabelBottom    = 6.;    // rect.bottom-6
constexpr CCoord kLampSize       = 10.;

/** The DXi moved one unit of a 0..100 range per pixel. */
constexpr float kUnitsPerPixel = 1.f / 100.f;

/** The vertical selector needed 25 pixels of movement per step. */
constexpr CCoord kSelectorStep = 25.;

/** MFC's Draw3dRect: a light line down the top and left, a highlight up
    the bottom and right. */
void draw3dRect (CDrawContext* context, const CRect& r,
                 const CColor& topLeft, const CColor& bottomRight)
{
	if (r.getWidth () <= 0. || r.getHeight () <= 0.)
		return;

	context->setLineWidth (1.);
	context->setFrameColor (topLeft);
	context->drawLine (CPoint (r.left, r.top), CPoint (r.right - 1., r.top));
	context->drawLine (CPoint (r.left, r.top), CPoint (r.left, r.bottom - 1.));

	context->setFrameColor (bottomRight);
	context->drawLine (CPoint (r.left, r.bottom - 1.), CPoint (r.right - 1., r.bottom - 1.));
	context->drawLine (CPoint (r.right - 1., r.top), CPoint (r.right - 1., r.bottom - 1.));
}

} // namespace

//------------------------------------------------------------------------
CFontRef panelFont ()
{
	// MFC's CreatePointFont(80) is Arial at 8 points, which at the 96 dpi
	// the dialog was designed for is 11 pixels.
	static SharedPointer<CFontDesc> font = makeOwned<CFontDesc> ("Arial", 11);
	return font;
}

CFontRef panelFontSmall ()
{
	static SharedPointer<CFontDesc> font = makeOwned<CFontDesc> ("Arial", 10);
	return font;
}

CFontRef panelFontTiny ()
{
	static SharedPointer<CFontDesc> font = makeOwned<CFontDesc> ("Arial", 9);
	return font;
}

//------------------------------------------------------------------------
// SpySlider
//------------------------------------------------------------------------
SpySlider::SpySlider (const CRect& size, IControlListener* listener, int32_t tag)
: CControl (size, listener, tag, nullptr)
{
	setWantsFocus (true);
}

void SpySlider::setLabel (const std::string& label)
{
	if (mLabel == label)
		return;
	mLabel = label;
	invalid ();
}

void SpySlider::setValueText (const std::string& text)
{
	if (mValueText == text)
		return;
	mValueText = text;
	invalid ();
}

void SpySlider::setUseIndicator (bool use)
{
	mUseIndicator = use;
	invalid ();
}

void SpySlider::setIndicator (bool on)
{
	if (mIndicator == on)
		return;
	mIndicator = on;
	invalid ();
}

void SpySlider::setFormatter (std::function<std::string (float)> formatter)
{
	mFormatter = std::move (formatter);
}

//------------------------------------------------------------------------
void SpySlider::drawLamp (CDrawContext* context)
{
	if (! mUseIndicator)
		return;

	const CRect r = getViewSize ();
	CRect lamp (r.left, r.top, r.left + kLampSize, r.top + kLampSize);
	draw3dRect (context, lamp, Colours::kLampFrame, Colours::kLampFrame);

	lamp.inset (2., 2.);
	context->setFillColor (mIndicator ? Colours::kLampOn : Colours::kLampOff);
	context->drawRect (lamp, kDrawFilled);
}

void SpySlider::drawBar (CDrawContext* context, double fraction, bool fill)
{
	const CRect r = getViewSize ();

	CRect bar (r.left,
	           r.bottom - kBarBottomInset - kBarHeight,
	           r.left + r.getWidth () * std::clamp (fraction, 0.0, 1.0),
	           r.bottom - kBarBottomInset);

	draw3dRect (context, bar, Colours::kBarLight, Colours::kBarHigh);

	if (! fill)
		return;

	bar.inset (1., 1.);
	if (bar.getWidth () <= 0. || bar.getHeight () <= 0.)
		return;
	context->setFillColor (Colours::kBarFill);
	context->drawRect (bar, kDrawFilled);
}

void SpySlider::drawFitted (CDrawContext* context, const std::string& text,
                            const CRect& band, const CColor& colour)
{
	if (text.empty ())
		return;

	// Windows drew both of these with DT_CENTER and no DT_VCENTER, so they
	// sat at the TOP of the band they were given, not in the middle of it.
	// Centring them vertically instead puts the red value straight through
	// the green label - which is what the first render of this panel showed.
	CFontRef font = panelFont ();
	context->setFont (font);
	if (context->getStringWidth (text.c_str ()) > band.getWidth ())
	{
		font = panelFontSmall ();
		context->setFont (font);
		if (context->getStringWidth (text.c_str ()) > band.getWidth ())
		{
			font = panelFontTiny ();
			context->setFont (font);
		}
	}

	const CCoord height = font->getSize () + 2.;
	const CRect line (band.left, band.top, band.right,
	                  std::min (band.top + height, band.bottom));

	context->setFontColor (colour);
	context->drawString (text.c_str (), line, kCenterText, true);
}

void SpySlider::drawLabel (CDrawContext* context, const std::string& text,
                           const CColor& colour)
{
	const CRect r = getViewSize ();
	drawFitted (context, text,
	            CRect (r.left, r.bottom - kLabelTop, r.right, r.bottom), colour);
}

//------------------------------------------------------------------------
void SpySlider::draw (CDrawContext* context)
{
	// The panel bitmap behind the control shows through: the DXi blitted
	// its parent's pixels and drew on top, and here the frame's background
	// has already been drawn under us. Nothing is painted over it but the
	// bar, the text and the lamp.
	drawBar (context, getValueNormalized (), true);
	drawLabel (context, mLabel, Colours::kLabel);
	drawLamp (context);

	std::string value = mValueText;
	if (value.empty () && mFormatter)
		value = mFormatter (getValueNormalized ());

	// The value goes at the TOP of the control and the label at the
	// bottom, which is where DT_CENTER without DT_VCENTER put them.
	drawFitted (context, value, getViewSize (), Colours::kValue);

	setDirty (false);
}

//------------------------------------------------------------------------
void SpySlider::onMouseDownEvent (MouseDownEvent& event)
{
	if (! event.buttonState.isLeft ())
		return;

	// No absolute positioning: the DXi's slider moved by increments from
	// wherever it was, and on a control 69 pixels wide jumping to the
	// pointer would make every setting a coarse one.
	mDragging = true;
	mLastPoint = event.mousePosition;
	beginEdit ();
	event.consumed = true;
}

void SpySlider::onMouseMoveEvent (MouseMoveEvent& event)
{
	if (! mDragging)
		return;

	const CCoord dx = event.mousePosition.x - mLastPoint.x;
	if (std::fabs (dx) < 1.)
		return;

	mLastPoint = event.mousePosition;

	const float scale = event.modifiers.has (ModifierKey::Shift) ? 0.1f : 1.f;
	setValueNormalized (std::clamp (
		getValueNormalized () + static_cast<float> (dx) * kUnitsPerPixel * scale,
		0.f, 1.f));
	valueChanged ();
	invalid ();
	event.consumed = true;
}

void SpySlider::onMouseUpEvent (MouseUpEvent& event)
{
	if (! mDragging)
		return;
	mDragging = false;
	endEdit ();
	event.consumed = true;
}

void SpySlider::onMouseCancelEvent (MouseCancelEvent& event)
{
	if (mDragging)
	{
		mDragging = false;
		endEdit ();
	}
	event.consumed = true;
}

void SpySlider::onMouseWheelEvent (MouseWheelEvent& event)
{
	const float step = event.modifiers.has (ModifierKey::Shift) ? 0.002f : 0.01f;
	beginEdit ();
	setValueNormalized (std::clamp (
		getValueNormalized () + static_cast<float> (event.deltaY) * step, 0.f, 1.f));
	valueChanged ();
	endEdit ();
	invalid ();
	event.consumed = true;
}

//------------------------------------------------------------------------
// SpyToggle
//------------------------------------------------------------------------
SpyToggle::SpyToggle (const CRect& size, IControlListener* listener, int32_t tag)
: SpySlider (size, listener, tag)
{
}

void SpyToggle::setStateNames (const std::string& off, const std::string& on)
{
	mNames[0] = off;
	mNames[1] = on;
	invalid ();
}

void SpyToggle::draw (CDrawContext* context)
{
	const bool on = getValueNormalized () >= 0.5f;

	// All or nothing: the DXi's two-state scale was 1.0 or 0.0, never
	// anything between.
	drawBar (context, on ? 1.0 : 0.0, true);

	// A two-state control shows the NAME OF ITS STATE where a slider shows
	// its label, and shows no red value text at all.
	drawLabel (context, mNames[on ? 1 : 0], Colours::kLabel);
	drawLamp (context);

	setDirty (false);
}

void SpyToggle::onMouseDownEvent (MouseDownEvent& event)
{
	if (! event.buttonState.isLeft ())
		return;

	beginEdit ();
	setValueNormalized (getValueNormalized () >= 0.5f ? 0.f : 1.f);
	valueChanged ();
	endEdit ();
	invalid ();
	event.consumed = true;
}

void SpyToggle::onMouseMoveEvent (MouseMoveEvent& event)
{
	// The DXi returned early from OnMouseMove for a two-state control, so
	// a drag across one does nothing.
	event.consumed = false;
}

void SpyToggle::onMouseUpEvent (MouseUpEvent& event)
{
	event.consumed = true;
}

//------------------------------------------------------------------------
// SpySelector
//------------------------------------------------------------------------
SpySelector::SpySelector (const CRect& size, IControlListener* listener, int32_t tag)
: SpySlider (size, listener, tag)
{
}

void SpySelector::setNames (const std::vector<std::string>& names)
{
	mNames = names;
	invalid ();
}

int SpySelector::currentIndex () const
{
	if (mNames.empty ())
		return 0;
	const int last = static_cast<int> (mNames.size ()) - 1;
	if (last <= 0)
		return 0;
	return std::clamp (static_cast<int> (getValueNormalized () * last + 0.5f), 0, last);
}

void SpySelector::draw (CDrawContext* context)
{
	// A multi-state control is a box the full height of the view with the
	// name of the current value across it: PaintBk took Bar.top from
	// rect.top and skipped the fill.
	const CRect r = getViewSize ();
	draw3dRect (context, CRect (r.left, r.top, r.right, r.bottom - kBarBottomInset),
	            Colours::kBarLight, Colours::kBarHigh);

	if (! mNames.empty ())
		drawFitted (context, mNames[static_cast<std::size_t> (currentIndex ())],
		            r, Colours::kValue);

	setDirty (false);
}

void SpySelector::onMouseDownEvent (MouseDownEvent& event)
{
	if (! event.buttonState.isLeft ())
		return;
	mDragging = true;
	mAnchorY = event.mousePosition.y;
	beginEdit ();
	event.consumed = true;
}

void SpySelector::onMouseMoveEvent (MouseMoveEvent& event)
{
	if (! mDragging || mNames.size () < 2)
		return;

	const int last = static_cast<int> (mNames.size ()) - 1;
	int index = currentIndex ();

	// DOWN ADVANCES. The DXi decremented its counter when the pointer went
	// down, and reported max - counter, so down raised the value. Kept.
	if (event.mousePosition.y > mAnchorY + kSelectorStep)
	{
		mAnchorY = event.mousePosition.y;
		index = std::min (index + 1, last);
	}
	else if (event.mousePosition.y < mAnchorY - kSelectorStep)
	{
		mAnchorY = event.mousePosition.y;
		index = std::max (index - 1, 0);
	}
	else
	{
		return;
	}

	setValueNormalized (static_cast<float> (index) / static_cast<float> (last));
	valueChanged ();
	invalid ();
	event.consumed = true;
}

void SpySelector::onMouseUpEvent (MouseUpEvent& event)
{
	if (! mDragging)
		return;
	mDragging = false;
	endEdit ();
	event.consumed = true;
}

//------------------------------------------------------------------------
// SpyFileButton
//------------------------------------------------------------------------
SpyFileButton::SpyFileButton (const CRect& size, IControlListener* listener, int32_t tag)
: SpySlider (size, listener, tag)
{
	setUseIndicator (true);
}

void SpyFileButton::setChooseHandler (std::function<void ()> handler)
{
	mChoose = std::move (handler);
}

void SpyFileButton::draw (CDrawContext* context)
{
	// The lamp shows whether the slot is ENABLED and loaded; the label is
	// the file's own name. No bar: the DXi's file buttons were two-state
	// SlideSpins whose state nothing ever set, so the bar was always
	// empty.
	drawLabel (context, mLabel, Colours::kLabel);
	setIndicator (getValueNormalized () >= 0.5f);
	drawLamp (context);
	setDirty (false);
}

void SpyFileButton::onMouseDownEvent (MouseDownEvent& event)
{
	if (! event.buttonState.isLeft ())
		return;

	const CRect r = getViewSize ();
	const CRect lamp (r.left, r.top, r.left + kLampSize, r.top + kLampSize);

	if (lamp.pointInside (event.mousePosition))
	{
		// The lamp is the enable. This is the write the DXi never had.
		beginEdit ();
		setValueNormalized (getValueNormalized () >= 0.5f ? 0.f : 1.f);
		valueChanged ();
		endEdit ();
		invalid ();
	}
	else if (mChoose)
	{
		mChoose ();
	}

	event.consumed = true;
}

void SpyFileButton::onMouseMoveEvent (MouseMoveEvent& event)
{
	event.consumed = false;
}

void SpyFileButton::onMouseUpEvent (MouseUpEvent& event)
{
	event.consumed = true;
}

//------------------------------------------------------------------------
// SpyPatchBoard
//------------------------------------------------------------------------
SpyPatchBoard::SpyPatchBoard (const CRect& size, IPatchBoardListener* listener)
: CView (size), mListener (listener)
{
	setWantsFocus (true);
	// iOldPatchBoard, seeded at 0.5 in PatchBoard's constructor: what a
	// cell comes back to when it is switched on again.
	for (double& v : mRestore)
		v = 0.5;
}

void SpyPatchBoard::setSize (int bands)
{
	if (bands <= 0 || bands == mBands)
		return;
	mBands = bands;
	invalid ();
}

//------------------------------------------------------------------------
// The grid is SQUARE and centred, because the DXi took the smaller of the
// view's two dimensions: `Dimension = rect.Height(); if (width < height)
// Dimension = width;`. The .rc box is 159 x 172, so the grid is 159 across
// with the remainder as a margin.
//------------------------------------------------------------------------
CCoord SpyPatchBoard::step () const
{
	const CRect r = getViewSize ();
	const CCoord dimension = std::min (r.getWidth (), r.getHeight ());
	return std::floor (dimension / mBands);
}

CCoord SpyPatchBoard::border () const
{
	const CRect r = getViewSize ();
	const CCoord dimension = std::min (r.getWidth (), r.getHeight ());
	return std::floor ((dimension - step () * mBands) / 2.);
}

void SpyPatchBoard::draw (CDrawContext* context)
{
	if (mListener == nullptr)
		return;

	const CRect r = getViewSize ();
	const CCoord s = step ();
	const CCoord b = border ();
	const CCoord side = s * mBands;

	const CRect grid (r.left + b, r.top + b, r.left + b + side, r.top + b + side);

	context->setLineWidth (1.);
	context->setFrameColor (Colours::kGrid);
	for (int i = 1; i <= mBands; ++i)
	{
		const CCoord x = grid.left + i * s;
		const CCoord y = grid.top + i * s;
		context->drawLine (CPoint (x, grid.top), CPoint (x, grid.bottom));
		context->drawLine (CPoint (grid.left, y), CPoint (grid.right, y));
	}

	draw3dRect (context, grid, Colours::kGridBorder, Colours::kGridBorder);

	CRect outside (grid);
	outside.extend (b, b);
	draw3dRect (context, outside, Colours::kOuterBorder, Colours::kOuterBorder);

	// A pin per open cell. The DXi drew a flat red square whatever the
	// cell's value was; here the value shades it, so a matrix that has
	// been dragged rather than clicked can be read at a glance. The shape,
	// the position and the colour at full are the original's.
	for (int row = 0; row < mBands; ++row)
	{
		for (int column = 0; column < mBands; ++column)
		{
			const double value = mListener->patchValue (row, column);
			if (value <= 0.0)
				continue;

			CRect pin (grid.left + row * s + 1., grid.top + column * s + 1.,
			           grid.left + (row + 1) * s - 1., grid.top + (column + 1) * s - 1.);

			CColor colour = Colours::kPin;
			colour.alpha = static_cast<uint8_t> (
				std::clamp (60. + 195. * value, 0., 255.));
			context->setFillColor (colour);
			context->drawRect (pin, kDrawFilled);
		}
	}

	setDirty (false);
}

//------------------------------------------------------------------------
bool SpyPatchBoard::cellAt (const CPoint& where, int& row, int& column) const
{
	const CRect r = getViewSize ();
	const CCoord s = step ();
	if (s <= 0.)
		return false;
	const CCoord b = border ();

	// Row from x and column from y - PatchBoard::OnLButtonDown, which is
	// the transpose of what the names suggest.
	const CCoord x = where.x - r.left - b;
	const CCoord y = where.y - r.top - b;
	if (x < 0. || y < 0.)
		return false;

	row = static_cast<int> (x / s);
	column = static_cast<int> (y / s);
	return (row >= 0 && row < mBands && column >= 0 && column < mBands);
}

void SpyPatchBoard::onMouseDownEvent (MouseDownEvent& event)
{
	if (! event.buttonState.isLeft () || mListener == nullptr)
		return;
	if (! cellAt (event.mousePosition, mRow, mColumn))
		return;

	mDragging = true;
	mMoved = false;
	mAnchorY = event.mousePosition.y;
	mListener->beginPatchEdit (mRow, mColumn);
	event.consumed = true;
}

void SpyPatchBoard::nudge (double delta)
{
	const double now = mListener->patchValue (mRow, mColumn);
	// The DXi's floor was 0.01, not zero: dragging a cell down could not
	// switch it off, only make it very quiet.
	const double next = std::clamp (now + delta, 0.01, 1.0);
	mListener->setPatchValue (mRow, mColumn, next);
	mRestore[mRow * 22 + mColumn] = next;
	invalid ();
}

void SpyPatchBoard::onMouseMoveEvent (MouseMoveEvent& event)
{
	if (! mDragging || mListener == nullptr)
		return;

	// 25 pixels per hundredth, as PatchBoard::OnMouseMove had it, and
	// DOWN REDUCES - which is the ordinary way round, unlike the band
	// selectors.
	if (event.mousePosition.y > mAnchorY + 25.)
	{
		mAnchorY = event.mousePosition.y;
		mMoved = true;
		nudge (-0.01);
	}
	else if (event.mousePosition.y < mAnchorY - 25.)
	{
		mAnchorY = event.mousePosition.y;
		mMoved = true;
		nudge (0.01);
	}
	event.consumed = true;
}

void SpyPatchBoard::onMouseUpEvent (MouseUpEvent& event)
{
	if (! mDragging || mListener == nullptr)
		return;

	// A click that did not drag toggles the cell, restoring whatever it
	// was worth when it was switched off. The DXi decided this with a
	// hover counter incremented by a 20 ms timer - fewer than 50 ticks and
	// the click counted as a click - which is a second's grace; a drag
	// that actually moved the value is a better test and needs no timer.
	if (! mMoved)
	{
		const double now = mListener->patchValue (mRow, mColumn);
		if (now > 0.0)
		{
			mRestore[mRow * 22 + mColumn] = now;
			mListener->setPatchValue (mRow, mColumn, 0.0);
		}
		else
		{
			mListener->setPatchValue (mRow, mColumn, mRestore[mRow * 22 + mColumn]);
		}
		invalid ();
	}

	mListener->endPatchEdit (mRow, mColumn);
	mDragging = false;
	event.consumed = true;
}

void SpyPatchBoard::onMouseCancelEvent (MouseCancelEvent& event)
{
	if (mDragging && mListener)
		mListener->endPatchEdit (mRow, mColumn);
	mDragging = false;
	event.consumed = true;
}

void SpyPatchBoard::onMouseWheelEvent (MouseWheelEvent& event)
{
	if (mListener == nullptr)
		return;

	int row = 0, column = 0;
	if (! cellAt (event.mousePosition, row, column))
		return;

	mRow = row;
	mColumn = column;
	mListener->beginPatchEdit (row, column);
	nudge (event.deltaY > 0. ? 0.01 : -0.01);
	mListener->endPatchEdit (row, column);
	event.consumed = true;
}

//------------------------------------------------------------------------
// SpyLedColumn
//------------------------------------------------------------------------
SpyLedColumn::SpyLedColumn (const CRect& size, const std::string& label)
: CView (size), mLabel (label)
{
}

void SpyLedColumn::setLevel (double linearPeak)
{
	const double decibels = (linearPeak <= 1e-9)
		? -(kFloorDb + 30.0)
		: 20.0 * std::log10 (linearPeak);

	double target = (decibels + kFloorDb) / kFloorDb;
	target = std::clamp (target, 0.0, 1.0);

	const double was = mLevel;
	const double wasPeak = mPeak;

	// Instant attack, gentle release: a meter that falls as fast as it
	// rises is unreadable, and one that falls slowly hides a gate closing.
	if (target > mLevel)
		mLevel = target;
	else
		mLevel += (target - mLevel) * 0.28;

	// Peak hold, about a second at thirty frames, then it slides down to
	// meet the bar.
	if (target >= mPeak)
	{
		mPeak = target;
		mHold = 30;
	}
	else if (--mHold <= 0)
	{
		mPeak = std::max (mPeak - 0.015, mLevel);
	}

	if (std::fabs (mLevel - was) > 0.002 || std::fabs (mPeak - wasPeak) > 0.002)
		invalid ();
}

void SpyLedColumn::draw (CDrawContext* context)
{
	const CRect r = getViewSize ();

	// Room for the label under the column, on the same 11-pixel band the
	// SlideSpins use.
	const CCoord labelHeight = mLabel.empty () ? 0. : 13.;
	const CRect body (r.left, r.top, r.right, r.bottom - labelHeight);

	draw3dRect (context, body, Colours::kLampFrame, Colours::kLampFrame);

	CRect inner (body);
	inner.inset (2., 2.);

	const CCoord pitch = inner.getHeight () / kSegments;
	const int litTo = static_cast<int> (mLevel * kSegments + 0.5);
	const int peakAt = static_cast<int> (mPeak * kSegments + 0.5);

	for (int i = 0; i < kSegments; ++i)
	{
		// i counts UP from the bottom.
		const CCoord top = inner.bottom - (i + 1) * pitch;
		CRect cell (inner.left, top + 1., inner.right, top + pitch - 1.);
		if (cell.getHeight () <= 0.)
			continue;

		CColor colour;
		if (i >= kSegments - 2)
			colour = Colours::kLampOn;                      // the last 3 dB
		else if (i >= kSegments - 5)
			colour = CColor (255, 190, 40, 255);            // the last 9 dB
		else
			colour = Colours::kLabel;                       // the panel's green

		const bool lit = (i < litTo);
		const bool isPeak = (peakAt > 0 && i == peakAt - 1);

		if (! lit && ! isPeak)
			colour = CColor (26, 26, 26, 255);              // an unlit LED
		else if (! lit && isPeak)
			colour.alpha = 170;                             // the held peak

		context->setFillColor (colour);
		context->drawRect (cell, kDrawFilled);
	}

	if (! mLabel.empty ())
	{
		context->setFont (panelFont ());
		context->setFontColor (Colours::kLabel);
		context->drawString (mLabel.c_str (),
		                     CRect (r.left, r.bottom - labelHeight, r.right, r.bottom),
		                     kCenterText, true);
	}

	setDirty (false);
}

//------------------------------------------------------------------------
// SpyBandMeter
//------------------------------------------------------------------------
SpyBandMeter::SpyBandMeter (const CRect& size)
: CView (size)
{
}

void SpyBandMeter::setData (const double* values, int count, bool stereo)
{
	if (values == nullptr || count < 0)
		count = 0;
	if (count > 44)
		count = 44;

	bool changed = (count != mCount) || (stereo != mStereo);
	for (int i = 0; i < count; ++i)
	{
		if (mData[i] != values[i])
			changed = true;
		mData[i] = values[i];
	}

	mCount = count;
	mStereo = stereo;
	if (changed)
		invalid ();
}

void SpyBandMeter::draw (CDrawContext* context)
{
	const CRect r = getViewSize ();

	context->setLineWidth (1.);

	if (mStereo)
	{
		context->setFrameColor (Colours::kGrid);
		const CCoord x = r.left + r.getWidth () / 2.;
		context->drawLine (CPoint (x, r.top), CPoint (x, r.bottom));
	}

	draw3dRect (context, r, Colours::kOuterBorder, Colours::kOuterBorder);

	if (mCount <= 0)
	{
		setDirty (false);
		return;
	}

	// One horizontal segment per band, from the bottom up, unnormalised.
	context->setFrameColor (Colours::kTrace);
	for (int i = 0; i < mCount; ++i)
	{
		const CCoord x0 = r.left + (static_cast<double> (i) / mCount) * r.getWidth ();
		const CCoord x1 = r.left + (static_cast<double> (i + 1) / mCount) * r.getWidth ();
		const CCoord y = r.top + r.getHeight () - r.getHeight () * mData[i];

		const CCoord clamped = std::clamp (y, r.top, r.bottom);
		context->drawLine (CPoint (x0, clamped), CPoint (x1, clamped));

		if (i > 0)
		{
			const CCoord previous = std::clamp (
				r.top + r.getHeight () - r.getHeight () * mData[i - 1], r.top, r.bottom);
			context->drawLine (CPoint (x0, previous), CPoint (x0, clamped));
		}
	}

	setDirty (false);
}

//------------------------------------------------------------------------
} // namespace SpyBand
