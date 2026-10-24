//------------------------------------------------------------------------
// SpyBand - where everything on the panel is
//
// THE BLACK PANEL (Designs/SpyBand-Polivoks-1-BlackPanel.png, October
// 2026): Concept 6's layout redressed in R2Synth's released look - matte
// black, white printing, each section boxed in a thin white line with its
// title in its own outlined box, knurled knobs with printed tick scales,
// a chicken-head rotary selector in place of every segmented switch, and
// colour only on the glass of the OLED screens. Red marks what is SET: a
// selector's chosen position, a pin, a lit lamp.
//
//   +-- header: wordmark | ‹ preset › SAVE          version   bypass ---+
//   | MODULATOR    | BANDS               [MATRIX][BANDS][INPUT]| INPUT  |
//   | source  play |  +--------------------------------------+ | CHECK  |
//   | five slots   |  |  the OLED: the tab's page            | +--------+
//   |              |  +--------------------------------------+ | MATRIX |
//   | wav    env   |  CARRIER  four knobs   bands  slope       |  pins  |
//   | atk    rel   |  NOISE    three knobs  detect carrier     +--------+
//   |              |                                           | OUTPUT |
//   +--------------+-------------------------------------------+--------+
//
// Every position is the mock-up's (Designs/polivoks/gen.py, design1()),
// in panel units - the mock-up is drawn at 2x and these are its 1x.
//
// NO VSTGUI HERE, so the layout tests and the wear generator
// (SpyBandWear.h) can read it as well as the editor. That is R2Synth's
// arrangement (R2SynthLayout.h), for the same reason: the "every label
// fits" checks have to agree with what is drawn.
//------------------------------------------------------------------------

#pragma once

#include "SciFiLettering.h"
#include "SpyBandParams.h"

#include <cmath>
#include <string>

namespace SpyBand {
namespace Layout {

//------------------------------------------------------------------------
struct Rect
{
	double left, top, right, bottom;
	double width () const { return right - left; }
	double height () const { return bottom - top; }
	bool contains (double x, double y, double pad = 0.0) const
	{
		return x >= left - pad && x <= right + pad && y >= top - pad && y <= bottom + pad;
	}
	bool overlaps (const Rect& o) const
	{
		return left < o.right && o.left < right && top < o.bottom && o.top < bottom;
	}
};

constexpr double kEditorWidth  = 1200.0;
constexpr double kEditorHeight = 690.0;
constexpr double kHeaderHeight = 52.0;

//------------------------------------------------------------------------
// The lettering's sizes: cap height and weight. The mock-up set its text
// in CSS pixels and drew capitals 0.64 of that; these are its capitals.
//------------------------------------------------------------------------
struct Text
{
	double cap;
	double weight;
	SciFi::Style style () const { SciFi::Style s; s.cap = cap; s.weight = weight; return s; }
	double width (const std::string& t) const { return SciFi::textWidth (t, style ()); }
	double height () const { return style ().inkHeight (); }
};

constexpr Text kTextTitle    { 8.3, SciFi::kBold };      // section titles, in their boxes
constexpr Text kTextSubtitle { 6.1, SciFi::kRegular };   // beside a title
constexpr Text kTextLabel    { 6.1, SciFi::kBold };      // knob and selector names, positions
constexpr Text kTextValue    { 6.4, SciFi::kRegular };   // knob readings
constexpr Text kTextTick     { 4.8, SciFi::kRegular };   // a scale's 0 and 10
constexpr Text kTextHeader   { 7.0, SciFi::kBold };      // the header's windows and buttons
constexpr Text kTextBlock    { 7.0, SciFi::kBold };      // CARRIER, NOISE
constexpr Text kTextButton   { 6.1, SciFi::kBold };      // tabs; 5.4 for the pattern buttons
constexpr Text kTextNote     { 6.1, SciFi::kRegular };   // slot names, legends
constexpr Text kTextScreen   { 5.8, SciFi::kRegular };   // OLED captions and readings
constexpr Text kTextAxis     { 5.4, SciFi::kRegular };   // OLED axis figures, matrix figures
constexpr double kWordmarkCap = 23.0;

//------------------------------------------------------------------------
// Sections
//------------------------------------------------------------------------
struct Section
{
	Rect r;
	const char* title;
	const char* subtitle;
};

constexpr Section kSections[] = {
	{ { 14, 64, 300, 676 },   "MODULATOR",   "THE .WAV FILES" },
	{ { 310, 64, 880, 676 },  "BANDS",       "CARRIER FILTERS + MODULATOR ENVELOPES" },
	{ { 890, 64, 1186, 220 }, "INPUT CHECK", "" },
	{ { 890, 228, 1186, 516 },"MATRIX",      "" },   // its subtitle is live: "9 × 9 OF 22"
	{ { 890, 524, 1186, 676 },"OUTPUT",      "" },
};
constexpr int kNumSections = sizeof (kSections) / sizeof (kSections[0]);
constexpr double kSectionRadius = 2.0;
constexpr double kSectionLine = 1.2;

/** A section's title box: 10 in from the corner, 22 tall, the title plus
    9 either side. */
inline Rect titleBox (const Section& s, const char* title = nullptr)
{
	const double w = kTextTitle.width (title ? title : s.title) + 18.0;
	return { s.r.left + 10.0, s.r.top + 10.0, s.r.left + 10.0 + w, s.r.top + 32.0 };
}

/** Where a subtitle starts: 12 past the title box. */
inline double subtitleLeft (const Section& s, const char* title = nullptr)
{
	return titleBox (s, title).right + 12.0;
}

//------------------------------------------------------------------------
// The OLED screens: black glass in a black cut-out 3 larger
//------------------------------------------------------------------------
enum ScreenIndex { kScreenBands = 0, kScreenInput, kScreenOut, kScreenSlot1 };

constexpr int kNumSlotsOnPanel = 5;
constexpr double kSlotTop = 188.0, kSlotPitch = 52.0;
constexpr double kSlotLeft = 26.0, kSlotRight = 288.0;

/** Slot `i`'s row: number, lamp, name, and its waveform's glass. */
inline Rect slotRow (int i)
{
	return { kSlotLeft, kSlotTop + kSlotPitch * i, kSlotRight, kSlotTop + kSlotPitch * (i + 1) - 2.0 };
}

/** The little glass a loaded slot's waveform sits behind. */
inline Rect slotGlass (int i)
{
	const Rect r = slotRow (i);
	return { r.left + 28.0, r.top + 22.0, r.right, r.top + 44.0 };
}

constexpr Rect kBandsScreen { 324, 108, 866, 352 };
constexpr Rect kInputScreen { 904, 108, 1172, 178 };
constexpr Rect kOutScreen   { 1112, 566, 1170, 650 };

constexpr int kNumScreens = 3 + kNumSlotsOnPanel;

inline Rect screen (int index)
{
	switch (index)
	{
		case kScreenBands: return kBandsScreen;
		case kScreenInput: return kInputScreen;
		case kScreenOut:   return kOutScreen;
		default:           return slotGlass (index - kScreenSlot1);
	}
}

inline Rect bezelOf (const Rect& s)
{
	return { s.left - 3.0, s.top - 3.0, s.right + 3.0, s.bottom + 3.0 };
}

//------------------------------------------------------------------------
// Knobs: black, knurled, a white line, eleven printed ticks
//------------------------------------------------------------------------
struct Knob
{
	Steinberg::Vst::ParamID tag;
	double cx, top, diameter;
	const char* label;
	bool scale;      // print 0 and 10 (or - and +) at the ends
	bool bipolar;
};

constexpr Knob kKnobs[] = {
	{ kSampLevel,        86, 466, 48, "WAV LEVEL",   true,  false },
	{ kEnvBoost,        228, 466, 48, "ENV LEVEL",   true,  false },
	{ kAttack,           86, 574, 44, "ATTACK",      true,  false },
	{ kRelease,         228, 574, 44, "RELEASE",     true,  false },
	{ kInLevel,         352, 410, 48, "SRC LEVEL",   false, false },
	{ kFreq,            432, 410, 48, "BOTTOM",      true,  false },
	{ kFreqSpread,      512, 410, 48, "TOP",         true,  false },
	{ kResonance,       596, 410, 48, "RESONANCE",   true,  false },
	{ kVoicedSense,     366, 552, 44, "SENSITIVITY", true,  false },
	{ kNoiseLevel,      466, 552, 44, "UNVOICED",    true,  false },
	{ kNoiseHighFreq,   566, 552, 44, "HIGH PASS",   true,  false },
	{ kSampThroughLevel,946, 566, 44, "THROUGH",     true,  false },
	{ kOutputTrim,     1040, 566, 44, "TRIM",        true,  true  },
};
constexpr int kNumKnobs = sizeof (kKnobs) / sizeof (kKnobs[0]);

/** Below the knob: a gap, the reading, the label. */
constexpr double kKnobGap = 6.0;
constexpr double kKnobValueY = 7.0;    // the reading's centre, below the gap
constexpr double kKnobLabelY = 19.5;   // the label's

inline double knobOuter (const Knob& k) { return k.diameter / 2.0; }
inline double knobBody (const Knob& k) { return k.diameter / 2.0 - (k.diameter >= 44 ? 6.0 : 5.0); }
inline double knobCentreY (const Knob& k) { return k.top + k.diameter / 2.0; }

/** The knob's view: wide enough for its widest reading, down to its label. */
inline Rect knobRect (const Knob& k)
{
	return { k.cx - 38.0, k.top - 2.0, k.cx + 38.0, k.top + k.diameter + kKnobGap + kKnobLabelY + 6.0 };
}

//------------------------------------------------------------------------
// The rotary selectors: a chicken head, its positions printed round it
//------------------------------------------------------------------------
struct Selector
{
	Steinberg::Vst::ParamID tag;
	double cx, cy, radius, labelRadius;
	int count;
	const char* names[4];
	double angle[4];       // degrees from twelve o'clock, clockwise
	const char* name;      // printed under it
	double nameDy;         // the name's centre below the pivot
};

constexpr Selector kSelectors[] = {
	{ kInterlaced,     100, 128, 15, 26, 2, { "SAMPLES", "RIGHT IN" },        { -45, 45 },           "SOURCE",   30 },
	{ kRepeatSamp,     230, 128, 15, 26, 2, { "ONCE", "REPEAT" },             { -45, 45 },           "PLAYBACK", 30 },
	{ kBands,          690, 448, 15, 26, 4, { "9", "12", "18", "22" },        { -66, -22, 22, 66 },  "BANDS",    34 },
	{ kFilterSlopes,   818, 448, 15, 22, 3, { "SHALLOW", "MED", "STEEP" },    { -60, 0, 60 },        "SLOPE",    34 },
	{ kVoicedDetect,   690, 592, 15, 26, 2, { "OFF", "ON" },                  { -45, 45 },           "DETECT",   34 },
	{ kNoiseOverride,  800, 592, 15, 26, 2, { "INPUT", "NOISE" },             { -45, 45 },           "CARRIER",  34 },
};
constexpr int kNumSelectors = sizeof (kSelectors) / sizeof (kSelectors[0]);

constexpr double kPi = 3.14159265358979323846;

/** A point `radius` out at `degrees` from twelve o'clock, clockwise. */
inline void pointFromTop (double cx, double cy, double radius, double degrees, double& x, double& y)
{
	const double t = degrees * kPi / 180.0;
	x = cx + radius * std::sin (t);
	y = cy - radius * std::cos (t);
}

/** WHERE A POSITION'S NAME IS PRINTED: hung from labelRadius along its
    angle - centred above for the positions near the top, ending short of
    the line on the left, starting past it on the right. */
inline Rect selectorLabelBox (const Selector& s, int index)
{
	const double t = s.angle[index];
	double px = 0.0, py = 0.0;
	pointFromTop (s.cx, s.cy, s.labelRadius, t, px, py);
	const double w = kTextLabel.width (s.names[index]);
	const double h = kTextLabel.height ();
	double left = 0.0, centreY = 0.0;
	if (std::fabs (t) < 30.0)
	{
		left = px - w / 2.0;
		centreY = py - 8.0;
	}
	else if (t < 0.0)
	{
		left = px - 2.0 - w;
		centreY = py + 2.0;
	}
	else
	{
		left = px + 2.0;
		centreY = py + 2.0;
	}
	return { left, centreY - h / 2.0, left + w, centreY + h / 2.0 };
}

inline Rect selectorNameBox (const Selector& s)
{
	const double w = kTextLabel.width (s.name);
	const double h = kTextLabel.height ();
	const double cy = s.cy + s.nameDy;
	return { s.cx - w / 2.0, cy - h / 2.0, s.cx + w / 2.0, cy + h / 2.0 };
}

/** Everything a selector draws - its view's size. */
inline Rect selectorRect (const Selector& s)
{
	Rect r { s.cx - s.radius - 9.0, s.cy - s.radius - 9.0, s.cx + s.radius + 9.0, s.cy + s.radius + 4.0 };
	auto grow = [&r] (const Rect& b) {
		r.left = std::fmin (r.left, b.left - 2.0);
		r.top = std::fmin (r.top, b.top - 2.0);
		r.right = std::fmax (r.right, b.right + 2.0);
		r.bottom = std::fmax (r.bottom, b.bottom + 2.0);
	};
	for (int i = 0; i < s.count; ++i)
		grow (selectorLabelBox (s, i));
	grow (selectorNameBox (s));
	return r;
}

/** THE POSITION A CLICK PICKS: on the knob, -1 (it steps on); anywhere
    else, the position whose angle is nearest the click's. */
inline int selectorPositionAt (const Selector& s, double x, double y)
{
	const double dx = x - s.cx, dy = y - s.cy;
	if (dx * dx + dy * dy <= s.radius * s.radius)
		return -1;
	// A click on a printed name picks that name, wherever it is.
	for (int i = 0; i < s.count; ++i)
		if (selectorLabelBox (s, i).contains (x, y, 2.0))
			return i;
	const double t = std::atan2 (dx, -dy) * 180.0 / kPi;
	int best = 0;
	for (int i = 1; i < s.count; ++i)
		if (std::fabs (s.angle[i] - t) < std::fabs (s.angle[best] - t))
			best = i;
	return best;
}

//------------------------------------------------------------------------
// Everything else
//------------------------------------------------------------------------
// The header
constexpr double kWordmarkLeft = 22.0;
constexpr double kHeaderDivider = 220.0;
constexpr Rect kPrevButton   { 234, 13, 262, 39 };
constexpr Rect kPresetWindow { 270, 13, 540, 39 };
constexpr Rect kNextButton   { 548, 13, 576, 39 };
constexpr Rect kSaveButton   { 584, 13, 642, 39 };
constexpr Rect kVersionBox   { 860, 13, 1060, 39 };
constexpr Rect kBypassBox    { 1070, 13, 1180, 39 };

// The centre: the tabs over the screen, and the two rows under it.
// The centre section's title follows the tab.
inline Rect tabRect (int i) { return { 676.0 + 66.0 * i, 74, 736.0 + 66.0 * i, 96 }; }
constexpr int kCentreSection = 1;
struct Titles { const char* title; const char* subtitle; };
constexpr Titles kCentreTitles[3] = {
	{ "MATRIX", "MODULATOR BAND \xE2\x80\xBA CARRIER BAND" },
	{ "BANDS",  "CARRIER FILTERS + MODULATOR ENVELOPES" },
	{ "INPUT",  "WHAT ARRIVES, BEFORE ANY LEVEL CONTROL" },
};
constexpr double kCarrierLabelY = 392.0;
constexpr double kNoiseRuleY = 512.0;
constexpr double kNoiseLabelY = 530.0;

/** The voiced detector's lamp: beside DETECT's name. */
inline void voicedLampCentre (double& x, double& y)
{
	const Selector& s = kSelectors[4];
	x = selectorNameBox (s).right + 9.0;
	y = s.cy + s.nameDy;
}

// The matrix page inside the big screen
constexpr Rect kBigBoard      { 478, 132, 720, 350 };
constexpr Rect kScreenCaption { 336, 113, 856, 125 };

// The input page inside the big screen
constexpr Rect kBigBarL       { 344, 142, 846, 156 };
constexpr Rect kBigBarR       { 344, 166, 846, 180 };
constexpr Rect kBigCorrelation{ 344, 196, 846, 210 };
constexpr double kInputNoteTop = 228.0;    // three notes, 14 apart
constexpr Rect kProcessingLabel { 344, 283, 430, 295 };
constexpr Rect kStereoSwitch    { 430, 276, 560, 302 };
constexpr Rect kRateLabel       { 640, 283, 846, 295 };

// Input check
constexpr Rect kInputBarL   { 912, 132, 1164, 146 };
constexpr Rect kInputBarR   { 912, 152, 1164, 166 };
constexpr Rect kSameAudio   { 904, 193, 1172, 207 };

// The matrix section
constexpr Rect kPanelBoard  { 911, 270, 1094, 460 };   // grid from 940, 271
constexpr double kLegendPinX = 1112.0;
constexpr double kLegendTop = 290.0;       // 100 %, 50 %, OPEN, 24 apart
constexpr Rect kPinCount    { 1106, 365, 1180, 379 };
constexpr Rect kOffDiagonal { 1106, 381, 1180, 395 };
constexpr Rect kInputCaption { 912, 114, 990, 126 };
inline Rect patternButton (int i) { return { 902.0 + 70.0 * i, 482, 966.0 + 70.0 * i, 504 }; }

// Output
constexpr double kOutLabelY = 664.0;

// The corner screws
struct Screw { double x, y; };
constexpr Screw kScrews[] = { { 8, 8 }, { 1192, 8 }, { 8, 682 }, { 1192, 682 } };

//------------------------------------------------------------------------
} // namespace Layout
} // namespace SpyBand
