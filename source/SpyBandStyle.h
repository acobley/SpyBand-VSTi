//------------------------------------------------------------------------
// SpyBand - the panel's look
//
// THE BLACK PANEL (Designs/SpyBand-Polivoks-1-BlackPanel.png): R2Synth's
// released look, so the two plug-ins match. Matte black, white printing,
// colour only on the glass of the OLED screens - the carrier blue, the
// modulator amber, the output green, which are SpyBand's colour code from
// the earlier designs - and red for what is SET.
//
// Concept 6's palette and drawing (rounded grey panels, coloured value
// arcs, amber pins) is in git history on the Design2 branch.
//
// The panel draws with paths, fills and gradients only. There is no
// bitmap and no system font anywhere in it, so it is sharp at any backing
// scale and looks the same on every host - the wear included
// (SpyBandWear.h).
//------------------------------------------------------------------------

#pragma once

#include "SciFiLettering.h"
#include "SpyBandLayout.h"
#include "SpyBandWear.h"

#include "vstgui/vstgui.h"

#include <string>

namespace SpyBand {

//------------------------------------------------------------------------
namespace Palette {

// The panel
const VSTGUI::CColor kGround      (0x12, 0x12, 0x12);   // matte black
const VSTGUI::CColor kHeader      (0x0c, 0x0c, 0x0c);
const VSTGUI::CColor kHeaderRule  (0xe9, 0xe6, 0xdc);   // the white rule under the header
const VSTGUI::CColor kSection     (0x15, 0x15, 0x15);
const VSTGUI::CColor kSectionEdge (0xd9, 0xd6, 0xcc);   // boxes
const VSTGUI::CColor kRule        (0x3a, 0x3a, 0x3a);   // thin rules inside a section
const VSTGUI::CColor kDivider     (0x5a, 0x58, 0x52);   // the header's divider, button edges
const VSTGUI::CColor kScrew       (0x3a, 0x3a, 0x3a);
const VSTGUI::CColor kWindow      (0x05, 0x05, 0x05);   // the preset window
const VSTGUI::CColor kWindowEdge  (0x3c, 0x3c, 0x3c);
const VSTGUI::CColor kButton      (0x0c, 0x0c, 0x0c);
const VSTGUI::CColor kButtonDown  (0x24, 0x24, 0x24);

// The printing
const VSTGUI::CColor kInk         (0xf4, 0xf1, 0xe8);   // everything printed
const VSTGUI::CColor kInkSoft     (0xb9, 0xb6, 0xae);   // subtitles
const VSTGUI::CColor kInkButton   (0xc9, 0xc6, 0xbe);   // a button not chosen
const VSTGUI::CColor kInkDim      (0x8f, 0x8c, 0x85);   // unchosen positions, counts
const VSTGUI::CColor kInkFaint    (0x6f, 0x6c, 0x66);   // empty slots
const VSTGUI::CColor kTick        (0xd9, 0xd6, 0xcc);

// Knobs and selectors
const VSTGUI::CColor kKnobBody    (0x0a, 0x0a, 0x0a);
const VSTGUI::CColor kKnobRim     (0x2f, 0x2f, 0x2f);
const VSTGUI::CColor kHub         (0x0b, 0x0b, 0x0b);
const VSTGUI::CColor kHubEdge     (0x44, 0x44, 0x44);

// Set: a chosen position, a pin, a lit lamp
const VSTGUI::CColor kRed         (0xe2, 0x34, 0x2b);
const VSTGUI::CColor kLampOff     (0x2a, 0x12, 0x10);
const VSTGUI::CColor kPinWhite    (0xe9, 0xe6, 0xdc);   // a 50 % pin

// The OLED screens
const VSTGUI::CColor kBezel       (0x05, 0x05, 0x05);
const VSTGUI::CColor kBezelEdge   (0x3c, 0x3c, 0x3c);
const VSTGUI::CColor kGlass       (0x00, 0x00, 0x00);
const VSTGUI::CColor kDotGrid     (0x2a, 0x2d, 0x33);
const VSTGUI::CColor kCaption     (0x8d, 0x96, 0xa3);   // grey captions on the glass
const VSTGUI::CColor kAxisTick    (0x3a, 0x3f, 0x47);
const VSTGUI::CColor kSegmentOff  (0x16, 0x18, 0x1c);   // an unlit meter segment
const VSTGUI::CColor kTooltipEdge (0x3c, 0x40, 0x48);
const VSTGUI::CColor kCarrier     (0x2f, 0xb8, 0xff);   // blue
const VSTGUI::CColor kModulator   (0xff, 0xb2, 0x3a);   // amber
const VSTGUI::CColor kOutput      (0x38, 0xf0, 0xa0);   // green
const VSTGUI::CColor kQuiet       (0x5a, 0x5f, 0x68);   // a waveform that will not play
const VSTGUI::CColor kMissing     (0xff, 0x5a, 0x4f);   // a file that did not load

/** `c` with its alpha replaced, 0..1. */
VSTGUI::CColor withAlpha (const VSTGUI::CColor& c, double alpha);

/** Straight blend of two colours, t = 0 gives `a`. */
VSTGUI::CColor mix (const VSTGUI::CColor& a, const VSTGUI::CColor& b, double t);

} // namespace Palette

//------------------------------------------------------------------------
// Lettering
//------------------------------------------------------------------------
enum class Align { Left, Centre, Right };

/** Draw `text` with its ink vertically centred on `centreY`. `x` is the
    left edge, the centre or the right edge, by `align`. Returns the
    width drawn. */
double drawText (VSTGUI::CDrawContext* context, const std::string& text,
                 double x, double centreY, double cap, const VSTGUI::CColor& colour,
                 Align align = Align::Left, double weight = SciFi::kRegular);

inline double drawText (VSTGUI::CDrawContext* context, const std::string& text, double x, double centreY,
                        const Layout::Text& style, const VSTGUI::CColor& colour, Align align = Align::Left)
{
	return drawText (context, text, x, centreY, style.cap, colour, align, style.weight);
}

/** The same, rotated by `degrees` (clockwise, y down) about `anchor` -
    the text's left-hand end, vertically centred, or its right-hand end
    when `anchorAtEnd`. */
void drawTextRotated (VSTGUI::CDrawContext* context, const std::string& text,
                      const VSTGUI::CPoint& anchor, double degrees, double cap,
                      const VSTGUI::CColor& colour, double weight = SciFi::kRegular,
                      bool anchorAtEnd = false);

double textWidth (const std::string& text, double cap, double weight = SciFi::kRegular);

inline double textWidth (const std::string& text, const Layout::Text& style)
{
	return textWidth (text, style.cap, style.weight);
}

/** The wordmark: SPY BAND, white, heavy, each stroke then cut through
    with a thin line of `background` - the inline of 70s display
    lettering. Left edge at `x`. */
void drawWordmark (VSTGUI::CDrawContext* context, double x, double centreY,
                   const VSTGUI::CColor& background);

//------------------------------------------------------------------------
// Shapes
//------------------------------------------------------------------------
VSTGUI::CRect toCRect (const Layout::Rect& r);

void fillRoundRect (VSTGUI::CDrawContext* context, const VSTGUI::CRect& r,
                    double radius, const VSTGUI::CColor& fill);
void strokeRoundRect (VSTGUI::CDrawContext* context, const VSTGUI::CRect& r,
                      double radius, const VSTGUI::CColor& colour, double width = 1.0);

void fillCircle (VSTGUI::CDrawContext* context, const VSTGUI::CPoint& centre,
                 double radius, const VSTGUI::CColor& fill);
void strokeCircle (VSTGUI::CDrawContext* context, const VSTGUI::CPoint& centre,
                   double radius, const VSTGUI::CColor& colour, double width = 1.0);

/** A point `radius` out at `degrees` from twelve o'clock, clockwise. */
VSTGUI::CPoint pointFromTop (const VSTGUI::CPoint& centre, double radius, double degrees);

/** A lamp: red (or `colour`) and haloed when lit, a dark red-black dot
    when not. */
void drawLamp (VSTGUI::CDrawContext* context, const VSTGUI::CPoint& centre,
               double radius, bool lit, const VSTGUI::CColor& colour = Palette::kRed);

/** A matrix pin - a ball with a highlight, red at 100 %, white at 50 % -
    or an open socket. `value` is the cell's normalised value; 0 is a
    socket. */
void drawPin (VSTGUI::CDrawContext* context, const VSTGUI::CPoint& centre,
              double radius, double value);

/** A black knob's cap, filled: lit from the top left. */
void fillCap (VSTGUI::CDrawContext* context, VSTGUI::CGraphicsPath* path,
              const VSTGUI::CPoint& centre, double radius);

/** An arc, stroked with round caps. Angles in degrees, clockwise from
    three o'clock (y down). */
void strokeArc (VSTGUI::CDrawContext* context, const VSTGUI::CPoint& centre,
                double radius, double fromDegrees, double toDegrees,
                const VSTGUI::CColor& colour, double width);

void drawLine (VSTGUI::CDrawContext* context, const VSTGUI::CPoint& a,
               const VSTGUI::CPoint& b, const VSTGUI::CColor& colour,
               double width = 1.0, bool round = false);

void drawDashedLine (VSTGUI::CDrawContext* context, const VSTGUI::CPoint& a,
                     const VSTGUI::CPoint& b, const VSTGUI::CColor& colour,
                     double width, double on, double off);

/** A section: the white box, and its title in a box of its own, and the
    subtitle beside it. */
void drawSection (VSTGUI::CDrawContext* context, const Layout::Section& s, bool withTitle = true);

/** A section's title box and subtitle, with the given words. */
void drawSectionTitle (VSTGUI::CDrawContext* context, const Layout::Section& s, const char* title,
                       const char* subtitle);

/** A screen's cut-out, and the black glass in it. `dots`: the OLED's
    dot grid. */
void drawBezel (VSTGUI::CDrawContext* context, const Layout::Rect& glass, bool dots);

/** The glass over a screen: its wear, then a faint sheen across the top. */
void drawGlass (VSTGUI::CDrawContext* context, const Layout::Rect& glass, const Wear::GlassWear& wear);

/** A trace on the OLED: the path stroked wide and faint for the bloom,
    then crisp. */
void strokeWithBloom (VSTGUI::CDrawContext* context, VSTGUI::CGraphicsPath* path,
                      const VSTGUI::CColor& colour, double width);

//------------------------------------------------------------------------
// The wear's marks, as paths
//------------------------------------------------------------------------
VSTGUI::CColor wearColour (const Wear::Rgb& c, uint8_t alpha);
bool touches (const VSTGUI::CRect& clip, double l, double t, double r, double b);
bool lineTouches (const VSTGUI::CRect& clip, const std::vector<Wear::Pt>& pts, double pad);
void wearLine (VSTGUI::CDrawContext* context, const Wear::Line& line, const Wear::Rgb& rgb);
void softDisc (VSTGUI::CDrawContext* context, const VSTGUI::CPoint& centre, double radius,
               const Wear::Rgb& rgb, uint8_t alpha);
void footRing (VSTGUI::CDrawContext* context, const Wear::FootRing& f);
void drift (VSTGUI::CDrawContext* context, const Wear::Drift& d);

//------------------------------------------------------------------------
} // namespace SpyBand
