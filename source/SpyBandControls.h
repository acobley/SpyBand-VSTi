//------------------------------------------------------------------------
// SpyBand - custom VSTGUI controls
//
// The DXi's property page used five kinds of control, and NONE of them was
// a bitmap: every one drew itself with GDI rectangles and text on top of
// the panel bitmap showing through. That is unusual for a plug-in of this
// vintage and it makes the port easier, because there is no artwork to
// recover - only shapes and colours, and both are in the source.
//
//   * SlideSpin   - a horizontal drag slider with a progress bar along its
//                   bottom edge, a green label under it and, when it has a
//                   value to show, red text across the middle. Used for
//                   thirteen controls, and in three modes: slider,
//                   two-state and multi-state.
//   * PatchBoard  - the 22 x 22 matrix.
//   * DrawArea    - the band envelope display.
//   * CButtonST   - never actually used here; the file buttons are
//                   SlideSpins with their indicator turned on.
//
// The colours below are the originals, taken from SlideSpin::PaintBk,
// PatchBoard::PaintBk and DrawArea::PaintBk rather than matched by eye.
//------------------------------------------------------------------------

#pragma once

#include "vstgui/vstgui.h"

#include <functional>
#include <string>
#include <vector>

namespace SpyBand {

//------------------------------------------------------------------------
// The original's palette
//------------------------------------------------------------------------
namespace Colours {

const VSTGUI::CColor kBarLight   (200, 200, 200, 255);  // Draw3dRect top-left
const VSTGUI::CColor kBarHigh    (255, 255, 255, 255);  // Draw3dRect bottom-right
const VSTGUI::CColor kBarFill    (100, 100, 100, 255);
const VSTGUI::CColor kLabel      ( 50, 255,  50, 255);  // SetTextColor, green
const VSTGUI::CColor kValue      (192,  50,  50, 255);  // DrawTheText, red
const VSTGUI::CColor kLampOn     (255,   0,   0, 255);
const VSTGUI::CColor kLampOff    (  0,   0,   0, 255);
const VSTGUI::CColor kLampFrame  (100, 100, 100, 255);
const VSTGUI::CColor kGrid       (200, 200, 200, 255);
const VSTGUI::CColor kGridBorder (100, 255, 100, 255);
const VSTGUI::CColor kOuterBorder(100, 100, 100, 255);
const VSTGUI::CColor kPin        (255,   0,   0, 255);
const VSTGUI::CColor kTrace      (127, 200, 255, 255);  // DrawArea's polyline

} // namespace Colours

/** MFC's CreatePointFont(80) - Arial at 8 points. */
VSTGUI::CFontRef panelFont ();

/** The same face one and two sizes down, for a label too long for its
    control. Six of the DXi's labels are wider than the control they name -
    "Unvoiced Noise Level" wants 95 pixels and has 82 - and DT_WORDBREAK
    wrapped them into an 11-pixel band, which clipped the second line.
    Dropping a size instead is the porting guide's advice and it is what
    these are for. */
VSTGUI::CFontRef panelFontSmall ();
VSTGUI::CFontRef panelFontTiny ();

//------------------------------------------------------------------------
/** A SlideSpin in its ordinary mode: drag left and right.

    The DXi moved the value by ONE unit of a 0..100 range per pixel of
    horizontal movement, in either direction, with no absolute
    positioning - clicking did not jump the value to the pointer. That is
    preserved, because on a control 69 pixels wide an absolute drag would
    make every setting a coarse one. */
class SpySlider : public VSTGUI::CControl
{
public:
	SpySlider (const VSTGUI::CRect& size, VSTGUI::IControlListener* listener, int32_t tag);

	/** The green text under the bar - the DXi's SetLabel. */
	void setLabel (const std::string& label);

	/** The red text across the middle - the DXi's SetValue, which the
	    property page filled in on a timer with a frequency reading. Empty
	    means the numeric value is shown instead, which is what the DXi
	    did when m_Value was empty. */
	void setValueText (const std::string& text);

	/** The 10 x 10 lamp in the top-left corner - SetUseIndicator. */
	void setUseIndicator (bool use);
	void setIndicator (bool on);
	bool indicator () const { return mIndicator; }

	/** How the numeric value reads when there is no value text. Given the
	    NORMALISED value; the editor hands it the parameter's own
	    formatting so the panel and the host cannot disagree. */
	void setFormatter (std::function<std::string (float)> formatter);

	void draw (VSTGUI::CDrawContext* context) override;

	void onMouseDownEvent (VSTGUI::MouseDownEvent& event) override;
	void onMouseMoveEvent (VSTGUI::MouseMoveEvent& event) override;
	void onMouseUpEvent (VSTGUI::MouseUpEvent& event) override;
	void onMouseCancelEvent (VSTGUI::MouseCancelEvent& event) override;
	void onMouseWheelEvent (VSTGUI::MouseWheelEvent& event) override;

	CLASS_METHODS (SpySlider, VSTGUI::CControl)

protected:
	void drawLamp (VSTGUI::CDrawContext* context);
	void drawBar (VSTGUI::CDrawContext* context, double fraction, bool fill);
	void drawLabel (VSTGUI::CDrawContext* context, const std::string& text,
	                const VSTGUI::CColor& colour);
	/** Draw `text` centred at the TOP of `band`, dropping a font size
	    rather than letting it run past the edges. */
	void drawFitted (VSTGUI::CDrawContext* context, const std::string& text,
	                 const VSTGUI::CRect& band, const VSTGUI::CColor& colour);

	std::string mLabel;
	std::string mValueText;
	std::function<std::string (float)> mFormatter;
	bool mUseIndicator = false;
	bool mIndicator = false;

	bool mDragging = false;
	VSTGUI::CPoint mLastPoint;
};

//------------------------------------------------------------------------
/** A SlideSpin in two-state mode: a click toggles it.

    The bar fills the whole width when on and disappears when off, and the
    text under it is the name of the state rather than a label - "Use
    Sample" against "Interlace". */
class SpyToggle : public SpySlider
{
public:
	SpyToggle (const VSTGUI::CRect& size, VSTGUI::IControlListener* listener, int32_t tag);

	void setStateNames (const std::string& off, const std::string& on);

	void draw (VSTGUI::CDrawContext* context) override;
	void onMouseDownEvent (VSTGUI::MouseDownEvent& event) override;
	void onMouseMoveEvent (VSTGUI::MouseMoveEvent& event) override;
	void onMouseUpEvent (VSTGUI::MouseUpEvent& event) override;
	void onMouseWheelEvent (VSTGUI::MouseWheelEvent& event) override;

	CLASS_METHODS (SpyToggle, SpySlider)

private:
	std::string mNames[2];
};

//------------------------------------------------------------------------
/** A SlideSpin in multi-state mode: an outlined box with the name of the
    current value across it.

    LEFT CLICK STEPS DOWN, RIGHT CLICK STEPS UP, and both wrap. Ctrl-click
    counts as a right click, which is the macOS convention and a fallback
    for hosts that keep the right button to themselves.

    NOT the DXi, which had no click behaviour at all: its vertical mode
    needed the pointer to move 25 pixels before it did anything, on a
    control 18 pixels tall, so the control read as dead until you happened
    to drag it. See PORTING-NOTES section 3.

    The drag is still there and still works the original's way round -
    DOWN ADVANCES, because the DXi decremented a counter it then reported
    as `max - count`, which is the opposite of the usual convention and is
    preserved. The wheel advances upwards, one position per click. */
class SpySelector : public SpySlider
{
public:
	SpySelector (const VSTGUI::CRect& size, VSTGUI::IControlListener* listener, int32_t tag);

	void setNames (const std::vector<std::string>& names);

	void draw (VSTGUI::CDrawContext* context) override;
	void onMouseDownEvent (VSTGUI::MouseDownEvent& event) override;
	void onMouseMoveEvent (VSTGUI::MouseMoveEvent& event) override;
	void onMouseUpEvent (VSTGUI::MouseUpEvent& event) override;
	void onMouseWheelEvent (VSTGUI::MouseWheelEvent& event) override;

	CLASS_METHODS (SpySelector, SpySlider)

private:
	int currentIndex () const;

	std::vector<std::string> mNames;
	VSTGUI::CCoord mAnchorY = 0.;
	bool mMoved = false;
	bool mStepUp = false;
};

//------------------------------------------------------------------------
/** One of the five file buttons.

    Click the LAMP to enable or disable the slot; click anywhere else to
    choose a file. The DXi's dialog never wrote PARAM_SAMPLE1..5 at all -
    the buttons only opened a file dialog - so no sample could play unless
    a host automated the parameter, while the lamp and GetSamplePlayState
    that would have shown it were already written. Only the write was
    missing. See PORTING-NOTES section 6. */
class SpyFileButton : public SpySlider
{
public:
	SpyFileButton (const VSTGUI::CRect& size, VSTGUI::IControlListener* listener, int32_t tag);

	/** Called when the part of the button that is not the lamp is clicked. */
	void setChooseHandler (std::function<void ()> handler);

	void draw (VSTGUI::CDrawContext* context) override;
	void onMouseDownEvent (VSTGUI::MouseDownEvent& event) override;
	void onMouseMoveEvent (VSTGUI::MouseMoveEvent& event) override;
	void onMouseUpEvent (VSTGUI::MouseUpEvent& event) override;
	void onMouseWheelEvent (VSTGUI::MouseWheelEvent& event) override;

	CLASS_METHODS (SpyFileButton, SpySlider)

private:
	std::function<void ()> mChoose;
};

//------------------------------------------------------------------------
/** The patch matrix.

    484 parameters behind one view, so it cannot be a CControl with a
    single tag; the editor supplies these instead. Row is the MODULATOR
    band and column is the CARRIER band, and row is the HORIZONTAL axis -
    PatchBoard::OnLButtonDown read the row from x and the column from y,
    and PaintBk drew the pin at (row * step, column * step). */
struct IPatchBoardListener
{
	virtual ~IPatchBoardListener () = default;
	virtual double patchValue (int row, int column) const = 0;
	virtual void beginPatchEdit (int row, int column) = 0;
	virtual void setPatchValue (int row, int column, double value) = 0;
	virtual void endPatchEdit (int row, int column) = 0;
};

class SpyPatchBoard : public VSTGUI::CView
{
public:
	SpyPatchBoard (const VSTGUI::CRect& size, IPatchBoardListener* listener);

	/** How many bands are in use - the grid is that square, whatever the
	    matrix behind it holds. */
	void setSize (int bands);

	void draw (VSTGUI::CDrawContext* context) override;

	void onMouseDownEvent (VSTGUI::MouseDownEvent& event) override;
	void onMouseMoveEvent (VSTGUI::MouseMoveEvent& event) override;
	void onMouseUpEvent (VSTGUI::MouseUpEvent& event) override;
	void onMouseCancelEvent (VSTGUI::MouseCancelEvent& event) override;
	void onMouseWheelEvent (VSTGUI::MouseWheelEvent& event) override;

	CLASS_METHODS (SpyPatchBoard, VSTGUI::CView)

private:
	bool cellAt (const VSTGUI::CPoint& where, int& row, int& column) const;
	VSTGUI::CCoord step () const;
	VSTGUI::CCoord border () const;
	void nudge (double delta);

	IPatchBoardListener* mListener = nullptr;
	int mBands = 9;

	bool mDragging = false;
	bool mMoved = false;
	int  mRow = 0;
	int  mColumn = 0;
	VSTGUI::CCoord mAnchorY = 0.;

	/** What a cell goes back to when it is switched on again. The DXi kept
	    a whole second 22 x 22 array for this (iOldPatchBoard) and seeded
	    it at 0.5. */
	double mRestore[22 * 22];
};

//------------------------------------------------------------------------
/** A segmented LED column, for the two input taps.

    NEW - the DXi had nothing like it. It reads the signal immediately
    after Src Level, which is the point marked L and R on
    docs/signal-path.png and the point at which the two channels part
    company: the left goes on to be the carrier, the right becomes the
    modulator when Interlace is on.

    Two of them side by side answer the question that cost a whole
    afternoon: are these two channels actually carrying different audio? A
    mono source moves both columns identically, and Interlace mode then
    vocodes the signal with itself. */
class SpyLedColumn : public VSTGUI::CView
{
public:
	SpyLedColumn (const VSTGUI::CRect& size, const std::string& label);

	/** A linear peak. Converted to dB, mapped over the column's range, and
	    given the ballistics below. Call it at the editor's frame rate; the
	    release and the peak hold are both counted in frames. */
	void setLevel (double linearPeak);

	void draw (VSTGUI::CDrawContext* context) override;

	CLASS_METHODS (SpyLedColumn, VSTGUI::CView)

	/** The bottom of the scale. The top is 0 dBFS. */
	static constexpr double kFloorDb = 60.0;
	static constexpr int    kSegments = 20;

private:
	std::string mLabel;
	double mLevel = 0.0;      // 0..1 up the column
	double mPeak = 0.0;
	int    mHold = 0;
};

//------------------------------------------------------------------------
/** The band envelope display (IDC_DRAWAREA).

    A stepped polyline, one horizontal segment per band, drawn from the
    bottom up with no normalisation - DrawArea::SetData computes the
    maximum and then does not use it, the two lines that would have scaled
    by it being commented out. A band above 1.0 therefore runs off the top,
    which is the original's behaviour and a useful clip indicator. */
class SpyBandMeter : public VSTGUI::CView
{
public:
	explicit SpyBandMeter (const VSTGUI::CRect& size);

	void setData (const double* values, int count, bool stereo);

	void draw (VSTGUI::CDrawContext* context) override;

	CLASS_METHODS (SpyBandMeter, VSTGUI::CView)

private:
	double mData[44] = { 0.0 };
	int  mCount = 0;
	bool mStereo = false;
};

//------------------------------------------------------------------------
} // namespace SpyBand
