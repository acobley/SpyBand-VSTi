//------------------------------------------------------------------------
// SpyBand - editor implementation
//------------------------------------------------------------------------

#include "SpyBandEditor.h"
#include "SpyBandController.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

using namespace Steinberg;
using namespace Steinberg::Vst;
using namespace VSTGUI;

namespace SpyBand {

namespace {

/** The dialog-unit scale, derived in tools/rc-geometry.py. */
constexpr double kXScale = 1.5;
constexpr double kYScale = 1.625;
constexpr double kYOriginDlu = 1.0;

/** Thirty frames a second. The DXi's SetTimer(0, 10, NULL) asked for a
    hundred, which redrew the whole property page every 10 ms whether
    anything had moved or not; nothing on this panel changes faster than
    an envelope follower can be seen to. */
constexpr uint32_t kTimerMs = 33;

std::string formatHz (double hz)
{
	char buffer[32];
	std::snprintf (buffer, sizeof (buffer), "%.0f Hz", hz);
	return buffer;
}

} // namespace

//------------------------------------------------------------------------
SpyBandEditor::SpyBandEditor (SpyBandController* controller)
: VSTGUIEditor (controller)
, mController (controller)
{
	ViewRect rect (0, 0, kEditorWidth, kEditorHeight);
	setRect (rect);
}

//------------------------------------------------------------------------
CBitmap* SpyBandEditor::bitmap (const char* name)
{
	auto it = mBitmaps.find (name);
	if (it != mBitmaps.end ())
		return it->second;
	auto bmp = makeOwned<CBitmap> (name);
	mBitmaps[name] = bmp;
	return bmp;
}

//------------------------------------------------------------------------
CRect SpyBandEditor::fromDlu (double x, double y, double w, double h) const
{
	const double px = x * kXScale;
	const double py = (y - kYOriginDlu) * kYScale;
	return CRect (px, py, px + w * kXScale, py + h * kYScale);
}

//------------------------------------------------------------------------
void SpyBandEditor::registerControl (ParamID tag, CControl* control)
{
	mControls[tag] = control;
	if (mController)
		control->setValueNormalized (
			static_cast<float> (mController->getParamNormalized (tag)));
	// Z-order is the order views are added, and every control here is a
	// direct child of the frame - so a control's getViewSize() is already
	// in frame coordinates and nothing needs a parent chain walked.
	frame->addView (control);
}

//------------------------------------------------------------------------
SpySlider* SpyBandEditor::addSlider (ParamID tag, const char* label,
                                     double x, double y, double w, double h)
{
	auto* control = new SpySlider (fromDlu (x, y, w, h), this, static_cast<int32_t> (tag));
	control->setLabel (label);

	// The number under the pointer is the parameter's own plain value, so
	// the panel and the host cannot disagree about what a control says.
	const ParamDef& def = paramDef (tag);
	control->setFormatter ([def] (float normalized)
	{
		char buffer[32];
		std::snprintf (buffer, sizeof (buffer), "%.0f", def.toPlain (normalized));
		return std::string (buffer);
	});

	registerControl (tag, control);
	return control;
}

//------------------------------------------------------------------------
SpyToggle* SpyBandEditor::addToggle (ParamID tag, double x, double y, double w, double h)
{
	auto* control = new SpyToggle (fromDlu (x, y, w, h), this, static_cast<int32_t> (tag));

	// The two words the dialog gave this switch - "Use Sample" against
	// "Interlace" - rather than a tick.
	if (const char* const* names = booleanNames (tag))
		control->setStateNames (names[0], names[1]);
	else
		control->setStateNames ("Off", "On");

	registerControl (tag, control);
	return control;
}

//------------------------------------------------------------------------
SpySelector* SpyBandEditor::addSelector (ParamID tag, const char* const* names, int count,
                                         double x, double y, double w, double h)
{
	auto* control = new SpySelector (fromDlu (x, y, w, h), this, static_cast<int32_t> (tag));

	std::vector<std::string> list;
	for (int i = 0; i < count; ++i)
		list.emplace_back (names[i]);
	control->setNames (list);

	registerControl (tag, control);
	return control;
}

//------------------------------------------------------------------------
void SpyBandEditor::addFileButton (int slot, double x, double y, double w, double h)
{
	const ParamID tag = static_cast<ParamID> (kSample1 + slot);
	auto* control = new SpyFileButton (fromDlu (x, y, w, h), this, static_cast<int32_t> (tag));

	control->setChooseHandler ([this, slot] ()
	{
		if (frame == nullptr || mController == nullptr)
			return;

		auto* selector = CNewFileSelector::create (frame, CNewFileSelector::kSelectFile);
		if (selector == nullptr)
			return;

		selector->setTitle ("Choose a wave file");
		selector->addFileExtension (CFileExtension ("Wave", "wav"));
		selector->setAllowMultiFileSelection (false);

		selector->run ([this, slot] (CNewFileSelector* s)
		{
			if (s->getNumSelectedFiles () > 0)
			{
				if (UTF8StringPtr path = s->getSelectedFile (0))
					mController->requestSlotLoad (slot, std::string (path));
			}
		});
		selector->forget ();
	});

	mFileButtons[slot] = control;
	registerControl (tag, control);
}

//------------------------------------------------------------------------
bool PLUGIN_API SpyBandEditor::open (void* parent, const PlatformType& platformType)
{
	if (frame != nullptr)
		return false;

	const CRect frameSize (0, 0, kEditorWidth, kEditorHeight);
	frame = new CFrame (frameSize, this);

	// A colour under the artwork, so a missing bitmap is a dark panel
	// rather than whatever the host left in the window.
	frame->setBackgroundColor (CColor (64, 64, 64, 255));
	if (CBitmap* background = bitmap ("background.png"))
		frame->setBackground (background);

	//--------------------------------------------------------------------
	// Left column: the envelope and the levels.
	// SpyBand.rc lines for IDC_RELEASE, IDC_ATTACK, IDC_ENVBOOST,
	// IDC_SAMPLEVEL, IDC_INLEVEL, IDC_SAMPTHROUGHLEVEL.
	//--------------------------------------------------------------------
	addSlider (kRelease,          "Env  Release", 12,  14, 46, 17);
	addSlider (kAttack,           "Env  Attack",  12,  30, 46, 17);
	addSlider (kEnvBoost,         "Env Level",    12,  47, 46, 17);
	addSlider (kSampLevel,        "Wav Level",    12,  89, 46, 17);
	addSlider (kInLevel,          "Src Level",    12, 106, 46, 17);
	addSlider (kSampThroughLevel, "Through",      12, 122, 46, 17);

	// NEW: the output trim, in the empty corner under the level controls,
	// on the same 46 x 17 rhythm as the column above it.
	addSlider (kOutputTrim, "Output Trim", 12, 156, 46, 17);
	if (auto* trim = mControls[kOutputTrim])
	{
		static_cast<SpySlider*> (trim)->setFormatter ([] (float normalized)
		{
			char buffer[32];
			std::snprintf (buffer, sizeof (buffer), "%.0f dB", outputDecibels (normalized));
			return std::string (buffer);
		});
	}

	//--------------------------------------------------------------------
	// Middle column: the bands, then the voiced detector and its noise.
	//--------------------------------------------------------------------
	addSlider (kFreq,       "Bottom Freq", 90, 13, 46, 17);
	addSlider (kFreqSpread, "Top Freq",    90, 30, 46, 17);
	addSlider (kResonance,  "Resonance",   90, 47, 46, 17);

	addToggle (kVoicedDetect, 90, 89, 55, 17)->setUseIndicator (true);
	addSlider (kVoicedSense,   "Voiced Sensitivity",   90, 106, 55, 17);
	addSlider (kNoiseLevel,    "Unvoiced Noise Level", 90, 123, 55, 17);
	addToggle (kNoiseOverride, 90, 139, 55, 18);
	addSlider (kNoiseHighFreq, "Noise HighPass",       90, 156, 55, 17);

	//--------------------------------------------------------------------
	// Right of centre: the mode switches and the two selectors.
	//--------------------------------------------------------------------
	addToggle (kStereo,     186,  89, 55, 17);
	addToggle (kInterlaced, 186, 106, 55, 17);
	addToggle (kRepeatSamp, 186, 123, 55, 17);

	addSelector (kBands,        kBandsNames,       4, 186, 140, 55, 11);
	addSelector (kFilterSlopes, kFilterSlopeNames, 3, 186, 151, 55, 11);

	//--------------------------------------------------------------------
	// The input LED columns.
	//
	// NEW. They read the taps marked L and R on docs/signal-path.png -
	// straight out of Src Level, where the two channels part company - and
	// they go in the empty channel between the .rc's first column of
	// controls (which ends at x 87) and its second (which starts at 135).
	// Pixels, not dialog units: there is nothing in the .rc to convert.
	//--------------------------------------------------------------------
	mInputMeter[0] = new SpyLedColumn (CRect (93, 143, 109, 300), "L");
	mInputMeter[1] = new SpyLedColumn (CRect (115, 143, 131, 300), "R");
	frame->addView (mInputMeter[0]);
	frame->addView (mInputMeter[1]);

	//--------------------------------------------------------------------
	// The band meter and the patch matrix.
	//--------------------------------------------------------------------
	mMeter = new SpyBandMeter (fromDlu (171, 12, 111, 55));
	frame->addView (mMeter);

	// The .rc box is 106 x 106 DLU, which is 159 x 172 PIXELS - the two
	// scale factors differ - and PatchBoard::PaintBk took the smaller of
	// the two dimensions, so the grid it drew was 159 square with the
	// bottom 13 pixels of the box unused. The view is that square, so that
	// nothing else has to steer around 13 pixels of nothing.
	CRect boardBox = fromDlu (257, 89, 106, 106);
	boardBox.setHeight (boardBox.getWidth ());
	mPatchBoard = new SpyPatchBoard (boardBox, this);
	frame->addView (mPatchBoard);

	//--------------------------------------------------------------------
	// The five file buttons. IDC_FILE1 is 14 units tall in the .rc where
	// the other four are 17; kept, because it is what the dialog said.
	//--------------------------------------------------------------------
	addFileButton (0, 312,  6, 50, 14);
	addFileButton (1, 312, 20, 50, 17);
	addFileButton (2, 312, 37, 50, 17);
	addFileButton (3, 312, 54, 50, 17);
	addFileButton (4, 312, 71, 50, 17);

	//--------------------------------------------------------------------
	// The version label. The DXi's read "V B1-MC1"; this one carries the
	// sample rate the DSP is actually running at, which is the one number
	// on the panel that a reader cannot work out for themselves.
	//
	// MOVED: the .rc puts it at 312, 179, which is INSIDE the patch board -
	// 385 to 544 across and 143 to about 300 down, whatever the band count.
	// In Windows the board was created later and so covered it, and the
	// label was invisible. Eleven units down clears the board's outer
	// border with room to spare. See PORTING-NOTES section 3.
	//--------------------------------------------------------------------
	mVersion = new CTextLabel (fromDlu (312, 190, 51, 17));
	mVersion->setFont (panelFont ());
	mVersion->setFontColor (Colours::kLabel);
	mVersion->setBackColor (kTransparentCColor);
	mVersion->setFrameColor (kTransparentCColor);
	mVersion->setStyle (CParamDisplay::kNoFrame);
	frame->addView (mVersion);

	refresh ();

	mTimer = makeOwned<CVSTGUITimer> ([this] (CVSTGUITimer*) { refresh (); },
	                                  kTimerMs, true);

	frame->open (parent, platformType);
	return true;
}

//------------------------------------------------------------------------
void PLUGIN_API SpyBandEditor::close ()
{
	mTimer = nullptr;

	mControls.clear ();
	mBitmaps.clear ();
	for (auto*& button : mFileButtons)
		button = nullptr;
	for (auto*& column : mInputMeter)
		column = nullptr;
	mPatchBoard = nullptr;
	mMeter = nullptr;
	mVersion = nullptr;

	if (frame)
	{
		frame->forget ();
		frame = nullptr;
	}
}

//------------------------------------------------------------------------
// Everything the DXi's OnTimer did, which is everything on the panel that
// is not a parameter.
//------------------------------------------------------------------------
void SpyBandEditor::refresh ()
{
	if (mController == nullptr || frame == nullptr)
		return;

	mController->requestMeter ();

	const double sampleRate = mController->dspSampleRate ();
	const double freq = paramDef (kFreq).toInternal (mController->getParamNormalized (kFreq));
	const double spread =
		paramDef (kFreqSpread).toInternal (mController->getParamNormalized (kFreqSpread));
	const int bands =
		bandCount (paramDef (kBands).toInternal (mController->getParamNormalized (kBands)));

	// The frequency readouts. These come from BandLayout, which the DSP
	// calls too - a private copy of the arithmetic here would drift from
	// the filters at some sample rate nobody tests.
	if (auto* control = mControls[kFreq])
		static_cast<SpySlider*> (control)->setValueText (
			formatHz (bandCentreHz (freq, spread, bands, 0, sampleRate)));

	if (auto* control = mControls[kFreqSpread])
		static_cast<SpySlider*> (control)->setValueText (
			formatHz (bandCentreHz (freq, spread, bands, bands - 1, sampleRate)));

	if (auto* control = mControls[kNoiseHighFreq])
	{
		const double internal = paramDef (kNoiseHighFreq)
			.toInternal (mController->getParamNormalized (kNoiseHighFreq));
		static_cast<SpySlider*> (control)->setValueText (
			formatHz (noiseHighPassHz (internal, sampleRate)));
	}

	// The voiced lamp.
	if (auto* control = mControls[kVoicedDetect])
		static_cast<SpySlider*> (control)->setIndicator (mController->voiced ());

	// The file buttons: the label is the file's own name, and the lamp is
	// lit only when the slot is BOTH enabled and loaded - which is what
	// CSpyBand::GetSamplePlayState reported and nothing could ever make
	// true, because the enable was never written.
	for (int slot = 0; slot < Vocoder::kNumSlots; ++slot)
	{
		if (mFileButtons[slot] == nullptr)
			continue;
		mFileButtons[slot]->setLabel (mController->slotLabel (slot));
		mFileButtons[slot]->setIndicator (
			mController->slotLoaded (slot)
			&& mController->getParamNormalized (static_cast<ParamID> (kSample1 + slot)) >= 0.5);
	}

	// The input columns. They are fed from the same message as the band
	// meter, so they cost nothing extra.
	for (int i = 0; i < 2; ++i)
		if (mInputMeter[i])
			mInputMeter[i]->setLevel (mController->inputPeak (i));

	// The meter, and the grid that has to match the band count.
	if (mMeter)
	{
		double frame32[2 * kMaxBands] = { 0.0 };
		const int count = mController->meter (frame32);
		mMeter->setData (frame32, count, count > bands);
	}

	if (mPatchBoard)
		mPatchBoard->setSize (bands);

	if (mVersion)
	{
		char buffer[64];
		std::snprintf (buffer, sizeof (buffer), "SpyBand  %.1f k",
		               sampleRate / 1000.0);
		mVersion->setText (buffer);
	}
}

//------------------------------------------------------------------------
void SpyBandEditor::valueChanged (CControl* control)
{
	if (mController == nullptr || control == nullptr)
		return;

	const ParamID tag = static_cast<ParamID> (control->getTag ());
	const ParamValue value = control->getValueNormalized ();

	mController->setParamNormalized (tag, value);
	mController->performEdit (tag, value);
}

void SpyBandEditor::controlBeginEdit (CControl* control)
{
	if (mController && control)
		mController->beginEdit (static_cast<ParamID> (control->getTag ()));
}

void SpyBandEditor::controlEndEdit (CControl* control)
{
	if (mController && control)
		mController->endEdit (static_cast<ParamID> (control->getTag ()));
}

//------------------------------------------------------------------------
// The patch matrix. 484 parameters behind one view, so it talks to the
// controller through here rather than through a tag.
//------------------------------------------------------------------------
double SpyBandEditor::patchValue (int row, int column) const
{
	if (mController == nullptr)
		return 0.0;
	return mController->getParamNormalized (patchParam (row, column));
}

void SpyBandEditor::beginPatchEdit (int row, int column)
{
	if (mController)
		mController->beginEdit (patchParam (row, column));
}

void SpyBandEditor::setPatchValue (int row, int column, double value)
{
	if (mController == nullptr)
		return;
	const ParamID tag = patchParam (row, column);
	mController->setParamNormalized (tag, value);
	mController->performEdit (tag, value);
}

void SpyBandEditor::endPatchEdit (int row, int column)
{
	if (mController)
		mController->endEdit (patchParam (row, column));
}

//------------------------------------------------------------------------
void SpyBandEditor::updateControl (ParamID tag, ParamValue normalized)
{
	if (frame == nullptr)
		return;

	if (isPatchParam (tag))
	{
		if (mPatchBoard)
			mPatchBoard->invalid ();
		return;
	}

	auto it = mControls.find (tag);
	if (it == mControls.end () || it->second == nullptr)
		return;

	it->second->setValueNormalized (static_cast<float> (normalized));
	it->second->invalid ();
}

//------------------------------------------------------------------------
} // namespace SpyBand
