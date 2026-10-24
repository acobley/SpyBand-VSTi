//------------------------------------------------------------------------
// SpyBand - the panel's controls
//
// The editor of Designs/SpyBand-Polivoks-1-BlackPanel.png: Concept 6's
// layout redressed as R2Synth's black panel, with its wear. Every control
// draws itself with paths - see SpyBandStyle.h - and none of them knows
// about the controller: what a control shows comes from the editor, and
// what it does goes back through IControlListener or one of the
// callbacks below.
//
// The first editor recreated the DXi's property page pixel for pixel; it
// is in git history (main, before the Design2 branch) along with the
// SlideSpin, PatchBoard and DrawArea notes that explained it.
//------------------------------------------------------------------------

#pragma once

#include "SpyBandStyle.h"

#include "vstgui/vstgui.h"

#include <functional>
#include <string>
#include <vector>

namespace SpyBand {

//------------------------------------------------------------------------
/** A knob: black and knurled, a white line, eleven ticks printed round
    it, and two lines of text under it - the reading and the label. Its
    wear (scuffs, grit, a polished cap) turns with it.

    Drag up or down; Shift for fine. Double-click resets to the default.
    The wheel steps by one percent. */
class SpyKnob : public VSTGUI::CControl
{
public:
	SpyKnob (const Layout::Knob& knob, VSTGUI::IControlListener* listener, const Wear::KnobWear& wear);

	/** The reading, given the normalised value. */
	void setFormatter (std::function<std::string (float)> formatter) { mFormatter = std::move (formatter); }

	/** Redraw if the reading would now read differently - for readings
	    that depend on something other than the value, like the sample
	    rate. */
	void refreshReadout ();

	void draw (VSTGUI::CDrawContext* context) override;
	void onMouseDownEvent (VSTGUI::MouseDownEvent& event) override;
	void onMouseMoveEvent (VSTGUI::MouseMoveEvent& event) override;
	void onMouseUpEvent (VSTGUI::MouseUpEvent& event) override;
	void onMouseCancelEvent (VSTGUI::MouseCancelEvent& event) override;
	void onMouseWheelEvent (VSTGUI::MouseWheelEvent& event) override;

	CLASS_METHODS (SpyKnob, VSTGUI::CControl)

	/** Pixels of travel for the whole range. */
	static constexpr double kDragPixels = 200.0;

private:
	Layout::Knob mKnob;
	Wear::KnobWear mWear;
	std::string mShown;
	std::function<std::string (float)> mFormatter;

	bool mDragging = false;
	double mAnchorY = 0.0;
	float mAnchorValue = 0.f;
};

//------------------------------------------------------------------------
/** A rotary selector for a two-state or list parameter: a chicken-head
    knob with its positions printed round it, the chosen one white with a
    red mark, the others dim, and its name under it. Click a position's
    name (or off the knob towards it) to pick it; click the knob to step
    round; the wheel steps. The value is index / (count - 1). The
    geometry is Layout::Selector's, VSTGUI-free, so the tests can check
    every name fits and that a click on one picks it. */
class SpySelector : public VSTGUI::CControl
{
public:
	SpySelector (const Layout::Selector& selector, VSTGUI::IControlListener* listener,
	             std::vector<Wear::PolarSpeck> grit);

	int currentIndex () const;
	void select (int index);

	void draw (VSTGUI::CDrawContext* context) override;
	void onMouseDownEvent (VSTGUI::MouseDownEvent& event) override;
	void onMouseUpEvent (VSTGUI::MouseUpEvent& event) override;
	void onMouseWheelEvent (VSTGUI::MouseWheelEvent& event) override;

	CLASS_METHODS (SpySelector, VSTGUI::CControl)

private:
	Layout::Selector mSel;
	std::vector<Wear::PolarSpeck> mGrit;
};

//------------------------------------------------------------------------
/** A switch drawn ON THE GLASS of a screen: its states as words, the
    chosen one in the screen's colour inside a thin box, the others grey.
    Only the Input page's MONO / STEREO uses it - everywhere on the panel
    itself a switch is a SpySelector. Click a word to choose it. The value
    is index / (count - 1). */
class SpySegment : public VSTGUI::CControl
{
public:
	SpySegment (const VSTGUI::CRect& size, VSTGUI::IControlListener* listener, int32_t tag);

	void setNames (const std::vector<std::string>& names);
	void setColour (const VSTGUI::CColor& colour) { mColour = colour; invalid (); }

	void draw (VSTGUI::CDrawContext* context) override;
	void onMouseDownEvent (VSTGUI::MouseDownEvent& event) override;
	void onMouseWheelEvent (VSTGUI::MouseWheelEvent& event) override;

	CLASS_METHODS (SpySegment, VSTGUI::CControl)

	/** Each segment's box, in view coordinates. Public for the layout
	    checks. */
	std::vector<VSTGUI::CRect> segmentRects () const;

private:
	int currentIndex () const;

	std::vector<std::string> mNames;
	VSTGUI::CColor mColour = Palette::kCarrier;
};

//------------------------------------------------------------------------
/** The three tabs over the big screen: MATRIX, BANDS, INPUT, as
    outlined buttons, the chosen one with a white edge and a red bar.
    Each tab's box is Layout::tabRect (i). The view spans the centre
    section's title row, and prints the section's title and subtitle for
    the chosen tab (Layout::kCentreTitles) - the background leaves that
    section untitled. */
class SpyTabs : public VSTGUI::CView
{
public:
	SpyTabs (const VSTGUI::CRect& size, const std::vector<std::string>& names);

	void setSelected (int index);
	int selected () const { return mSelected; }
	void setChangeHandler (std::function<void (int)> handler) { mOnChange = std::move (handler); }

	void draw (VSTGUI::CDrawContext* context) override;
	void onMouseDownEvent (VSTGUI::MouseDownEvent& event) override;

	CLASS_METHODS (SpyTabs, VSTGUI::CView)

private:
	std::vector<std::string> mNames;
	int mSelected = 1;
	std::function<void (int)> mOnChange;
};

//------------------------------------------------------------------------
/** The patch matrix as a pin board.

    484 parameters behind one view, so the editor supplies these instead
    of a tag. ON SCREEN THE ROWS ARE THE MODULATOR BANDS AND THE COLUMNS
    THE CARRIER BANDS - modulator Hz down the left, carrier Hz in
    white along the bottom. That is the transpose of the DXi's board, which
    put the modulator across; with the axes labelled, rows reading as
    "this band drives that one" is the more natural way round. The
    parameter ids are unchanged: row is still the modulator. */
struct IPatchBoardListener
{
	virtual ~IPatchBoardListener () = default;
	virtual double patchValue (int row, int column) const = 0;
	virtual void beginPatchEdit (int row, int column) = 0;
	virtual void setPatchValue (int row, int column, double value) = 0;
	virtual void endPatchEdit (int row, int column) = 0;

	/** What a cell goes back to when it is switched on again - shared by
	    every board on the panel. The DXi kept a second 22 x 22 array for
	    this (iOldPatchBoard), seeded at 0.5. */
	virtual double restoreValue (int row, int column) const = 0;
	virtual void setRestoreValue (int row, int column, double value) = 0;

	/** A label for band `index` of the current band count - its centre
	    frequency, "127" or "1.44K". */
	virtual std::string bandLabel (int index) const = 0;
};

class SpyPinBoard : public VSTGUI::CView
{
public:
	/** `labels`: draw the band figures down the left and along the
	    bottom, inside the view's own box. */
	SpyPinBoard (const VSTGUI::CRect& size, IPatchBoardListener* listener, bool labels);

	/** How many bands are in use - the grid is that square. */
	void setBands (int bands);

	/** The square the cells occupy, in view coordinates. */
	VSTGUI::CRect gridRect () const;

	void draw (VSTGUI::CDrawContext* context) override;
	void onMouseDownEvent (VSTGUI::MouseDownEvent& event) override;
	void onMouseMoveEvent (VSTGUI::MouseMoveEvent& event) override;
	void onMouseUpEvent (VSTGUI::MouseUpEvent& event) override;
	void onMouseCancelEvent (VSTGUI::MouseCancelEvent& event) override;
	void onMouseWheelEvent (VSTGUI::MouseWheelEvent& event) override;

	CLASS_METHODS (SpyPinBoard, VSTGUI::CView)

	/** Normalised travel per pixel of vertical drag: 100 pixels moves a
	    cell by half its range. The DXi needed 25 pixels per HUNDREDTH,
	    which made 50 % to 100 % a 1250-pixel drag on a 159-pixel board. */
	static constexpr double kDragPerPixel = 0.005;

private:
	bool cellAt (const VSTGUI::CPoint& where, int& row, int& column) const;
	double pitch () const;
	void set (double value);

	IPatchBoardListener* mListener = nullptr;
	bool mLabels = false;
	int mBands = 9;

	bool mDragging = false;
	bool mMoved = false;
	int mRow = 0;
	int mColumn = 0;
	double mAnchorY = 0.0;
	double mAnchorValue = 0.0;
};

//------------------------------------------------------------------------
/** The BANDS tab, on the big OLED: every band's filter drawn as the
    response it actually has (blue), the modulator envelopes as bars
    (amber), and the Bottom and Top handles (white rings), which drag.
    It draws on the glass; the cut-out and the glass's wear are the
    background's and the SpyGlass's. */
class SpyBandsDisplay : public VSTGUI::CView
{
public:
	explicit SpyBandsDisplay (const VSTGUI::CRect& size);

	struct State
	{
		int bands = 9;
		double centreHz[22] = { 0.0 };
		double bottomHz = 100.0;
		double topHz = 8000.0;
		double resonance = 0.75;    // internal, 0..1
		int slopes = 0;             // 0..2
		double sampleRate = 44100.0;
		std::string mode;           // "9 BANDS · SHALLOW"
	};

	void setState (const State& state);
	void setEnvelopes (const double* values, int count);

	/** `which` is 0 for Bottom and 1 for Top. `phase`: 0 begin, 1 move,
	    2 end. */
	void setDragHandler (std::function<void (int which, double hz, int phase)> handler)
	{
		mOnDrag = std::move (handler);
	}

	void draw (VSTGUI::CDrawContext* context) override;
	void onMouseDownEvent (VSTGUI::MouseDownEvent& event) override;
	void onMouseMoveEvent (VSTGUI::MouseMoveEvent& event) override;
	void onMouseUpEvent (VSTGUI::MouseUpEvent& event) override;
	void onMouseCancelEvent (VSTGUI::MouseCancelEvent& event) override;
	void onMouseExitEvent (VSTGUI::MouseExitEvent& event) override;

	CLASS_METHODS (SpyBandsDisplay, VSTGUI::CView)

	/** The plotted range. */
	static constexpr double kLowHz = 20.0;
	static constexpr double kHighHz = 20000.0;

private:
	VSTGUI::CRect plotRect () const;
	double xForHz (double hz) const;
	double hzForX (double x) const;
	int handleAt (const VSTGUI::CPoint& where) const;

	State mState;
	double mEnvelope[22] = { 0.0 };
	int mHover = -1;
	int mDrag = -1;
	double mDragAnchorX = 0.0;
	double mDragAnchorHz = 0.0;
	std::function<void (int, double, int)> mOnDrag;
};

//------------------------------------------------------------------------
/** One of the five sample slots: number in a ring, lamp, file name, and
    the waveform behind a little OLED glass of its own (a dashed outline
    while the slot is empty).

    Click the LAMP to switch the slot on or off - that is the parameter
    (kSample1..5). Click the NAME to choose a file, or drop a .wav on the
    row. Right-click (or Ctrl-click) for Choose and Clear. */
class SpySlotRow : public VSTGUI::CControl
{
public:
	SpySlotRow (const VSTGUI::CRect& size, VSTGUI::IControlListener* listener, int32_t tag,
	            int slot, const Wear::GlassWear& glassWear);

	/** Empty `name` means an empty slot. `missing` means the path is set
	    and the file did not load. */
	void setFile (const std::string& name, bool missing);
	void setOverview (const std::vector<float>& peaks);
	/** Greyed out, when Interlace takes the modulator from the right
	    input and the slots are not read. */
	void setInactive (bool inactive);

	void setChooseHandler (std::function<void ()> h) { mOnChoose = std::move (h); }
	void setContextHandler (std::function<void ()> h) { mOnContext = std::move (h); }
	void setDropHandler (std::function<void (const std::string&)> h) { mOnDrop = std::move (h); }

	void draw (VSTGUI::CDrawContext* context) override;
	void onMouseDownEvent (VSTGUI::MouseDownEvent& event) override;
	VSTGUI::SharedPointer<VSTGUI::IDropTarget> getDropTarget () override;

	CLASS_METHODS (SpySlotRow, VSTGUI::CControl)

private:
	friend class SlotDropTarget;
	VSTGUI::CRect lampRect () const;

	int mSlot = 0;
	Wear::GlassWear mGlassWear;
	std::string mName;
	bool mMissing = false;
	bool mInactive = false;
	bool mDropHighlight = false;
	std::vector<float> mPeaks;

	std::function<void ()> mOnChoose;
	std::function<void ()> mOnContext;
	std::function<void (const std::string&)> mOnDrop;
};

//------------------------------------------------------------------------
/** A segmented OLED level bar with a peak hold: the Input Check's L and
    R. Fed a linear peak at the editor's frame rate. */
class SpyLevelBar : public VSTGUI::CView
{
public:
	SpyLevelBar (const VSTGUI::CRect& size, const std::string& label, const VSTGUI::CColor& colour);

	void setLevel (double linearPeak);
	double displayedDb () const { return mDb; }

	void draw (VSTGUI::CDrawContext* context) override;

	CLASS_METHODS (SpyLevelBar, VSTGUI::CView)

	static constexpr double kFloorDb = -60.0;

private:
	std::string mLabel;
	VSTGUI::CColor mColour;
	double mDb = kFloorDb;
	double mPeakDb = kFloorDb;
	int mHold = 0;
	std::string mShown;
};

//------------------------------------------------------------------------
/** The OUT meter: two segmented columns on their own glass, green with
    the top three segments red. */
class SpyOutMeter : public VSTGUI::CView
{
public:
	explicit SpyOutMeter (const VSTGUI::CRect& size);
	void setLevels (double left, double right);
	void draw (VSTGUI::CDrawContext* context) override;
	CLASS_METHODS (SpyOutMeter, VSTGUI::CView)

private:
	double mLevel[2] = { 0.0, 0.0 };   // 0..1 of the bar
};

//------------------------------------------------------------------------
/** "● SAME AUDIO ON L AND R          CORR 0.12". The red lamp lights
    when the two channels have been one signal for a while. Without the
    reading (setShowReading (false)) it is the lamp and words only, and
    the editor prints the reading on the glass. */
class SpyCorrelationLamp : public VSTGUI::CView
{
public:
	explicit SpyCorrelationLamp (const VSTGUI::CRect& size);

	/** One block's coefficient, or anything below -1.5 for "too quiet to
	    tell". Smoothed here. */
	void setCorrelation (double correlation);

	bool lit () const { return mLit; }
	double smoothed () const { return mSmoothed; }
	bool haveReading () const { return mHaveReading; }
	void setShowReading (bool show) { mShowReading = show; invalid (); }
	/** On the glass (the Input page) the words are grey, not printed white. */
	void setOnGlass (bool glass) { mOnGlass = glass; invalid (); }

	void draw (VSTGUI::CDrawContext* context) override;
	CLASS_METHODS (SpyCorrelationLamp, VSTGUI::CView)

	/** Lit above this, held for kHoldFrames. */
	static constexpr double kSameAbove = 0.985;
	static constexpr int kHoldFrames = 15;

private:
	double mSmoothed = 0.0;
	bool mHaveReading = false;
	bool mLit = false;
	int mAbove = 0;
	bool mShowReading = true;
	bool mOnGlass = false;
	std::string mShown;
};

//------------------------------------------------------------------------
/** A clickable text button, black with a grey edge: the header's ‹ ›
    SAVE, and the matrix section's patterns. Highlighted - the pattern the
    matrix is now - it has a white edge and a red bar under its word. */
class SpyButton : public VSTGUI::CView
{
public:
	enum class Shape { Box, Pill };

	SpyButton (const VSTGUI::CRect& size, const std::string& text, Shape shape);

	void setClickHandler (std::function<void ()> handler) { mOnClick = std::move (handler); }
	void setHighlighted (bool on);
	void setCap (double cap) { mCap = cap; invalid (); }

	void draw (VSTGUI::CDrawContext* context) override;
	void onMouseDownEvent (VSTGUI::MouseDownEvent& event) override;
	void onMouseUpEvent (VSTGUI::MouseUpEvent& event) override;
	void onMouseCancelEvent (VSTGUI::MouseCancelEvent& event) override;

	CLASS_METHODS (SpyButton, VSTGUI::CView)

private:
	std::string mText;
	Shape mShape;
	bool mHighlighted = false;
	bool mPressed = false;
	double mCap = Layout::kTextHeader.cap;
	std::function<void ()> mOnClick;
};

//------------------------------------------------------------------------
/** The preset name in the header: "FACTORY / CLASSIC 9-BAND ⌄". Click
    opens the menu. */
class SpyPresetBox : public VSTGUI::CView
{
public:
	explicit SpyPresetBox (const VSTGUI::CRect& size);

	void setName (const std::string& bank, const std::string& name, bool modified);
	void setClickHandler (std::function<void ()> handler) { mOnClick = std::move (handler); }

	void draw (VSTGUI::CDrawContext* context) override;
	void onMouseDownEvent (VSTGUI::MouseDownEvent& event) override;

	CLASS_METHODS (SpyPresetBox, VSTGUI::CView)

private:
	std::string mBank;
	std::string mName;
	bool mModified = false;
	std::function<void ()> mOnClick;
};

//------------------------------------------------------------------------
/** A black box with a lamp and a word: the version (display only; its
    green lamp is lit while the DSP answers) and BYPASS (a two-state
    control on kBypass; its red lamp is lit while bypassed). */
class SpyPill : public VSTGUI::CControl
{
public:
	SpyPill (const VSTGUI::CRect& size, VSTGUI::IControlListener* listener, int32_t tag,
	         const std::string& text, const VSTGUI::CColor& lampColour, bool clickable);

	void draw (VSTGUI::CDrawContext* context) override;
	void onMouseDownEvent (VSTGUI::MouseDownEvent& event) override;

	CLASS_METHODS (SpyPill, VSTGUI::CControl)

private:
	std::string mText;
	VSTGUI::CColor mLamp;
	bool mClickable;
};

//------------------------------------------------------------------------
/** A red lamp on its own: the voiced detector's verdict. */
class SpyLamp : public VSTGUI::CView
{
public:
	SpyLamp (const VSTGUI::CRect& size, const VSTGUI::CColor& colour = Palette::kRed);
	void setLit (bool lit);
	bool lit () const { return mLit; }
	void draw (VSTGUI::CDrawContext* context) override;
	CLASS_METHODS (SpyLamp, VSTGUI::CView)

private:
	VSTGUI::CColor mColour;
	bool mLit = false;
};

//------------------------------------------------------------------------
/** A line of text that changes: the matrix panel's "9 × 9 OF 22" and pin
    counts. Redraws only when the text does. */
class SpyLabel : public VSTGUI::CView
{
public:
	SpyLabel (const VSTGUI::CRect& size, double cap, const VSTGUI::CColor& colour,
	          Align align = Align::Left, double weight = SciFi::kRegular);

	void setText (const std::string& text);
	void setColour (const VSTGUI::CColor& colour);
	const std::string& text () const { return mText; }

	void draw (VSTGUI::CDrawContext* context) override;
	CLASS_METHODS (SpyLabel, VSTGUI::CView)

private:
	std::string mText;
	double mCap;
	VSTGUI::CColor mColour;
	Align mAlign;
	double mWeight;
};

//------------------------------------------------------------------------
/** The glass over a screen: its dust, fibres and fingerprint, and the
    sheen. A view of its own, added after whatever draws on that screen,
    so a meter redrawing thirty times a second redraws the glass over
    itself. It takes no clicks. */
class SpyGlass : public VSTGUI::CView
{
public:
	SpyGlass (const Layout::Rect& glass, const Wear::GlassWear& wear);
	void draw (VSTGUI::CDrawContext* context) override;
	CLASS_METHODS (SpyGlass, VSTGUI::CView)

private:
	Layout::Rect mGlass;
	Wear::GlassWear mWear;
};

//------------------------------------------------------------------------
/** Everything on the panel that never changes: the ground, the header
    and wordmark, the screws, the sections, the screens' cut-outs, the
    fixed printing - and the wear over all of it. A display list the
    editor fills once. */
class SpyBackground : public VSTGUI::CView
{
public:
	explicit SpyBackground (const VSTGUI::CRect& size);

	/** Section `index` of Layout::kSections. */
	void addSection (int index);
	/** A screen's cut-out and black glass; `dots` for the dot grid. */
	void addBezel (const Layout::Rect& glass, bool dots);
	void addText (const std::string& text, double x, double centreY, const Layout::Text& style,
	              const VSTGUI::CColor& colour, Align align = Align::Left);
	void addRule (const VSTGUI::CPoint& a, const VSTGUI::CPoint& b,
	              const VSTGUI::CColor& colour = Palette::kRule);
	void addPin (const VSTGUI::CPoint& c, double radius, double value);

	void draw (VSTGUI::CDrawContext* context) override;
	CLASS_METHODS (SpyBackground, VSTGUI::CView)

	const Wear::PanelWear& wear () const { return mWear; }

private:
	struct Item
	{
		enum Kind { Section, Bezel, Text, Rule, Pin } kind = Text;
		int index = 0;               // Section: its place in Layout::kSections
		Layout::Rect rect {};
		VSTGUI::CPoint a, b;
		double radius = 0.0;
		VSTGUI::CColor colour;
		std::string text;
		Layout::Text style {};
		Align align = Align::Left;
		bool dots = false;
		double value = 0.0;
	};
	void drawItem (VSTGUI::CDrawContext* context, const Item& item);
	void drawWear (VSTGUI::CDrawContext* context);

	std::vector<Item> mItems;
	Wear::PanelWear mWear;
};

//------------------------------------------------------------------------
} // namespace SpyBand
