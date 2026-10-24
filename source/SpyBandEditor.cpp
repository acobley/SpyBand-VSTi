//------------------------------------------------------------------------
// SpyBand - editor implementation
//------------------------------------------------------------------------

#include "SpyBandEditor.h"
#include "SpyBandController.h"
#include "version.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <functional>
#include <string>

using namespace Steinberg;
using namespace Steinberg::Vst;
using namespace VSTGUI;

namespace SpyBand {

namespace {

/** Thirty frames a second - nothing on this panel moves faster than an
    envelope follower can be seen to. */
constexpr uint32_t kTimerMs = 33;

std::string upper (std::string s)
{
	for (char& c : s)
		c = static_cast<char> (std::toupper (static_cast<unsigned char> (c)));
	return s;
}

std::string formatHz (double hz)
{
	char buffer[32];
	if (hz >= 10000.0)
		std::snprintf (buffer, sizeof (buffer), "%.1f KHZ", hz / 1000.0);
	else if (hz >= 1000.0)
		std::snprintf (buffer, sizeof (buffer), "%.2f KHZ", hz / 1000.0);
	else
		std::snprintf (buffer, sizeof (buffer), "%.0f HZ", hz);
	return buffer;
}

std::string formatPercent (double plain)
{
	char buffer[32];
	std::snprintf (buffer, sizeof (buffer), "%.0f %%", plain);
	return buffer;
}

/** Src Level and Wav Level are multiplied by five in the DSP, so 20 % is
    unity gain - which is worth saying, because it is the default and it
    looks like a fifth. */
std::string formatLevel (double plain)
{
	std::string s = formatPercent (plain);
	if (std::fabs (plain - 20.0) < 0.5)
		s += " UNITY";
	return s;
}

} // namespace

//------------------------------------------------------------------------
SpyBandEditor::SpyBandEditor (SpyBandController* controller)
: VSTGUIEditor (controller)
, mController (controller)
{
	ViewRect rect (0, 0, kEditorWidth, kEditorHeight);
	setRect (rect);
	for (double& v : mRestore)
		v = 0.5;
}

//------------------------------------------------------------------------
double SpyBandEditor::paramInternal (ParamID id) const
{
	if (mController == nullptr)
		return paramDef (id).toInternal (paramDef (id).defaultNormalized ());
	return paramDef (id).toInternal (mController->getParamNormalized (id));
}

int SpyBandEditor::currentBands () const
{
	return bandCount (paramInternal (kBands));
}

void SpyBandEditor::edited ()
{
	if (mController)
		mController->markModified ();
}

//------------------------------------------------------------------------
void SpyBandEditor::registerControl (ParamID tag, CControl* control)
{
	mControls[tag] = control;
	if (mController)
		control->setValueNormalized (static_cast<float> (mController->getParamNormalized (tag)));
	control->setDefaultValue (static_cast<float> (paramDef (tag).defaultNormalized ()));
	frame->addView (control);
}

SpyKnob* SpyBandEditor::addKnob (ParamID tag)
{
	for (int i = 0; i < Layout::kNumKnobs; ++i)
	{
		if (Layout::kKnobs[i].tag != tag)
			continue;
		auto* knob = new SpyKnob (Layout::kKnobs[i], this, Wear::knobWear (i));
		// The default reading is the parameter's own plain value.
		const ParamDef& def = paramDef (tag);
		knob->setFormatter ([def] (float normalized) { return formatPercent (def.toPlain (normalized)); });
		registerControl (tag, knob);
		mKnobs.push_back (knob);
		return knob;
	}
	return nullptr;
}

SpySelector* SpyBandEditor::addSelector (ParamID tag)
{
	for (int i = 0; i < Layout::kNumSelectors; ++i)
	{
		if (Layout::kSelectors[i].tag != tag)
			continue;
		auto* selector = new SpySelector (Layout::kSelectors[i], this, Wear::selectorGrit (i));
		registerControl (tag, selector);
		return selector;
	}
	return nullptr;
}

//------------------------------------------------------------------------
bool PLUGIN_API SpyBandEditor::open (void* parent, const PlatformType& platformType)
{
	if (frame != nullptr)
		return false;

	auto* target = new CFrame (CRect (0, 0, kEditorWidth, kEditorHeight), this);
	buildViews (target);

	mTimer = makeOwned<CVSTGUITimer> ([this] (CVSTGUITimer*) { refresh (); }, kTimerMs, true);

	frame->open (parent, platformType);
	return true;
}

//------------------------------------------------------------------------
void SpyBandEditor::buildViews (CFrame* target)
{
	using namespace Layout;
	frame = target;
	frame->setBackgroundColor (Palette::kGround);

	auto* background = new SpyBackground (CRect (0, 0, kEditorWidth, kEditorHeight));
	for (int i = 0; i < kNumSections; ++i)
		background->addSection (i);

	// The screens' cut-outs. The slots draw their own small glasses, as
	// only a loaded slot has one.
	background->addBezel (kBandsScreen, true);
	background->addBezel (kInputScreen, true);
	background->addBezel (kOutScreen, false);

	// Modulator: rules between the slots, and one above and below them.
	background->addRule (CPoint (26, 178.5), CPoint (288, 178.5));
	for (int i = 0; i < kNumSlotsOnPanel - 1; ++i)
	{
		const double y = slotRow (i).bottom + 0.5;
		background->addRule (CPoint (26, y), CPoint (288, y));
	}
	background->addRule (CPoint (26, 452.5), CPoint (288, 452.5));

	// Centre: the two blocks under the screen.
	background->addText ("CARRIER", 324, kCarrierLabelY, kTextBlock, Palette::kInk);
	background->addRule (CPoint (322, kNoiseRuleY + 0.5), CPoint (868, kNoiseRuleY + 0.5));
	background->addText ("NOISE", 324, kNoiseLabelY, kTextBlock, Palette::kInk);

	// The matrix legend: what a red pin and a white pin mean.
	const char* const kLegend[3] = { "100 %", "50 %", "OPEN" };
	const double kLegendValue[3] = { 1.0, 0.5, 0.0 };
	for (int i = 0; i < 3; ++i)
	{
		const double y = kLegendTop + 24.0 * i;
		background->addPin (CPoint (kLegendPinX, y), 4.6, kLegendValue[i]);
		background->addText (kLegend[i], kLegendPinX + 12.0, y, kTextNote, Palette::kInk);
	}

	// Output
	background->addText ("OUT", (kOutScreen.left + kOutScreen.right) / 2.0, kOutLabelY, kTextLabel,
	                     Palette::kInk, Align::Centre);

	frame->addView (background);

	buildHeader ();
	buildModulator ();
	buildCentre ();
	buildRight ();

	// The glass over each screen, last, so it is over what is drawn on it.
	frame->addView (new SpyGlass (kBandsScreen, Wear::glassWear (kScreenBands)));
	frame->addView (new SpyGlass (kInputScreen, Wear::glassWear (kScreenInput)));
	frame->addView (new SpyGlass (kOutScreen, Wear::glassWear (kScreenOut)));

	selectTab (1);
	refresh ();
}

//------------------------------------------------------------------------
void SpyBandEditor::buildHeader ()
{
	using namespace Layout;
	auto* prev = new SpyButton (toCRect (kPrevButton), "\xE2\x80\xB9", SpyButton::Shape::Box);
	prev->setCap (9.0);
	prev->setClickHandler ([this] () { stepPreset (-1); });
	frame->addView (prev);

	mPresetBox = new SpyPresetBox (toCRect (kPresetWindow));
	mPresetBox->setClickHandler ([this] () { showPresetMenu (); });
	frame->addView (mPresetBox);

	auto* next = new SpyButton (toCRect (kNextButton), "\xE2\x80\xBA", SpyButton::Shape::Box);
	next->setCap (9.0);
	next->setClickHandler ([this] () { stepPreset (1); });
	frame->addView (next);

	auto* save = new SpyButton (toCRect (kSaveButton), "SAVE", SpyButton::Shape::Box);
	save->setClickHandler ([this] () { savePreset (); });
	frame->addView (save);

	// The version, and the maker. The green lamp is lit while the DSP
	// answers the meter - so a dark lamp means the processor is not running.
	mVersionPill = new SpyPill (toCRect (kVersionBox), nullptr, -1,
	                            upper ("V" VERSION_STR " \xC2\xB7 " stringCompanyName),
	                            Palette::kOutput, false);
	frame->addView (mVersionPill);

	auto* bypass = new SpyPill (toCRect (kBypassBox), this, static_cast<int32_t> (kBypass),
	                            "BYPASS", Palette::kRed, true);
	registerControl (kBypass, bypass);
}

//------------------------------------------------------------------------
void SpyBandEditor::buildModulator ()
{
	using namespace Layout;
	// Interlace takes the modulator from the right input instead of the
	// samples - which is the switch's whole meaning, so it is named for
	// it rather than for the DXi's "Use Sample" / "Interlace".
	addSelector (kInterlaced);
	addSelector (kRepeatSamp);

	for (int slot = 0; slot < Vocoder::kNumSlots; ++slot)
	{
		const ParamID tag = static_cast<ParamID> (kSample1 + slot);
		auto* row = new SpySlotRow (toCRect (slotRow (slot)), this, static_cast<int32_t> (tag), slot,
		                            Wear::glassWear (kScreenSlot1 + slot));
		row->setChooseHandler ([this, slot] () { chooseFile (slot); });
		row->setContextHandler ([this, slot] () { showSlotMenu (slot); });
		row->setDropHandler ([this, slot] (const std::string& path) {
			if (mController)
				mController->requestSlotLoad (slot, path);
			edited ();
		});
		mSlots[slot] = row;
		registerControl (tag, row);
	}

	addKnob (kSampLevel)->setFormatter ([] (float v) { return formatLevel (paramDef (kSampLevel).toPlain (v)); });

	addKnob (kEnvBoost)->setFormatter ([] (float v) {
		char buffer[32];
		std::snprintf (buffer, sizeof (buffer), "\xC3\x97 %.1f", paramDef (kEnvBoost).toInternal (v) * 10.0);
		return std::string (buffer);
	});

	addKnob (kAttack)->setFormatter ([] (float v) {
		char buffer[32];
		std::snprintf (buffer, sizeof (buffer), "%.1f MS", envAttackMs (paramDef (kAttack).toInternal (v)));
		return std::string (buffer);
	});

	addKnob (kRelease)->setFormatter ([] (float v) {
		char buffer[32];
		std::snprintf (buffer, sizeof (buffer), "%.1f MS", envReleaseMs (paramDef (kRelease).toInternal (v)));
		return std::string (buffer);
	});
}

//------------------------------------------------------------------------
void SpyBandEditor::buildCentre ()
{
	using namespace Layout;
	const Section& centre = kSections[kCentreSection];
	mTabs = new SpyTabs (CRect (centre.r.left + 8, centre.r.top + 8, tabRect (2).right, tabRect (0).bottom),
	                     { "MATRIX", "BANDS", "INPUT" });
	mTabs->setChangeHandler ([this] (int index) { selectTab (index); });
	frame->addView (mTabs);

	//--- BANDS ----------------------------------------------------------
	mBandsDisplay = new SpyBandsDisplay (toCRect (kBandsScreen));
	mBandsDisplay->setDragHandler ([this] (int which, double hz, int phase) {
		dragBandEdge (which, hz, phase);
	});
	frame->addView (mBandsDisplay);

	//--- MATRIX: the same board as the section's, large -----------------
	mMatrixCaption = new SpyLabel (toCRect (kScreenCaption), kTextScreen.cap, Palette::kCaption);
	mMatrixCaption->setText ("ROWS: MODULATOR BAND \xC2\xB7 COLUMNS: CARRIER BAND \xC2\xB7 CLICK = PIN \xC2\xB7 DRAG = LEVEL");
	frame->addView (mMatrixCaption);
	mBigBoard = new SpyPinBoard (toCRect (kBigBoard), this, true);
	frame->addView (mBigBoard);
	mInputPage.clear ();

	//--- INPUT -----------------------------------------------------------
	auto addPage = [this] (CView* view) {
		frame->addView (view);
		mInputPage.push_back (view);
	};
	auto* inputCaption = new SpyLabel (toCRect (kScreenCaption), kTextScreen.cap, Palette::kCaption);
	inputCaption->setText ("WHAT ARRIVES \xE2\x80\x94 MEASURED BEFORE EVERY LEVEL CONTROL");
	addPage (inputCaption);

	mInputBarBig[0] = new SpyLevelBar (toCRect (kBigBarL), "L", Palette::kCarrier);
	mInputBarBig[1] = new SpyLevelBar (toCRect (kBigBarR), "R", Palette::kModulator);
	addPage (mInputBarBig[0]);
	addPage (mInputBarBig[1]);

	mCorrelationBig = new SpyCorrelationLamp (toCRect (kBigCorrelation));
	mCorrelationBig->setOnGlass (true);
	addPage (mCorrelationBig);

	const char* const kNotes[] = {
		"WITH RIGHT INPUT AS THE SOURCE, L IS THE CARRIER AND R THE MODULATOR,",
		"SO THE TWO MUST CARRY DIFFERENT SIGNALS. ONE MONO SOURCE ON BOTH",
		"SIDES VOCODES THE SIGNAL WITH ITSELF - THE LAMP ABOVE LIGHTS.",
	};
	for (int i = 0; i < 3; ++i)
	{
		auto* note = new SpyLabel (CRect (344, kInputNoteTop + 14 * i - 6, 860, kInputNoteTop + 14 * i + 6),
		                           kTextScreen.cap, Palette::kCaption);
		note->setText (kNotes[i]);
		addPage (note);
	}

	auto* channels = new SpyLabel (toCRect (kProcessingLabel), kTextScreen.cap, Palette::kCaption);
	channels->setText ("PROCESSING");
	addPage (channels);
	auto* stereo = new SpySegment (toCRect (kStereoSwitch), this, static_cast<int32_t> (kStereo));
	stereo->setNames ({ "MONO", "STEREO" });
	stereo->setColour (Palette::kCarrier);
	registerControl (kStereo, stereo);
	mInputPage.push_back (stereo);

	mRateLabel = new SpyLabel (toCRect (kRateLabel), kTextScreen.cap, Palette::kCaption, Align::Right);
	addPage (mRateLabel);

	//--- CARRIER ----------------------------------------------------------
	addKnob (kInLevel)->setFormatter ([] (float v) { return formatLevel (paramDef (kInLevel).toPlain (v)); });

	addKnob (kFreq)->setFormatter ([] (float v) { return formatHz (bandBottomHz (paramDef (kFreq).toInternal (v))); });

	addKnob (kFreqSpread)->setFormatter ([this] (float v) {
		const double rate = mController ? mController->dspSampleRate () : 44100.0;
		return formatHz (bandTopHz (paramInternal (kFreq), paramDef (kFreqSpread).toInternal (v), rate));
	});

	addKnob (kResonance);

	addSelector (kBands);
	addSelector (kFilterSlopes);

	//--- NOISE ------------------------------------------------------------
	addKnob (kVoicedSense);
	addKnob (kNoiseLevel);
	addKnob (kNoiseHighFreq)->setFormatter ([this] (float v) {
		const double rate = mController ? mController->dspSampleRate () : 44100.0;
		return formatHz (noiseHighPassHz (paramDef (kNoiseHighFreq).toInternal (v), rate));
	});

	addSelector (kVoicedDetect);
	// The detector's verdict: lit while it hears a voiced sound.
	double lx = 0.0, ly = 0.0;
	voicedLampCentre (lx, ly);
	mVoicedLamp = new SpyLamp (CRect (lx - 9.0, ly - 9.0, lx + 9.0, ly + 9.0));
	frame->addView (mVoicedLamp);

	// Noise Override: what the carrier IS, the input or the noise. The DXi
	// had this the wrong way round - see DEVIATION 5.
	addSelector (kNoiseOverride);
}

//------------------------------------------------------------------------
void SpyBandEditor::buildRight ()
{
	using namespace Layout;
	mInputBar[0] = new SpyLevelBar (toCRect (kInputBarL), "L", Palette::kCarrier);
	mInputBar[1] = new SpyLevelBar (toCRect (kInputBarR), "R", Palette::kModulator);
	frame->addView (mInputBar[0]);
	frame->addView (mInputBar[1]);

	auto* caption = new SpyLabel (toCRect (kInputCaption), kTextScreen.cap, Palette::kCaption);
	caption->setText ("INPUT");
	frame->addView (caption);

	mCorrReading = new SpyLabel (CRect (kInputScreen.right - 100, kInputScreen.top + 6, kInputScreen.right - 8,
	                                    kInputScreen.top + 18), kTextScreen.cap, Palette::kCarrier, Align::Right);
	frame->addView (mCorrReading);

	mCorrelation = new SpyCorrelationLamp (toCRect (kSameAudio));
	mCorrelation->setShowReading (false);
	frame->addView (mCorrelation);

	// The matrix's live subtitle: "9 × 9 OF 22".
	const Section& matrix = kSections[3];
	const double sub = subtitleLeft (matrix);
	mMatrixSize = new SpyLabel (CRect (sub, matrix.r.top + 15, sub + 110, matrix.r.top + 27),
	                            kTextSubtitle.cap, Palette::kInkSoft);
	frame->addView (mMatrixSize);

	mPanelBoard = new SpyPinBoard (toCRect (kPanelBoard), this, true);
	frame->addView (mPanelBoard);

	mPinCount = new SpyLabel (toCRect (kPinCount), kTextNote.cap, Palette::kInkDim);
	mOffDiagonal = new SpyLabel (toCRect (kOffDiagonal), kTextNote.cap, Palette::kInkDim);
	frame->addView (mPinCount);
	frame->addView (mOffDiagonal);

	const struct { const char* name; Pattern pattern; } kButtons[4] = {
		{ "DIAGONAL", Pattern::Diagonal },
		{ "REVERSE", Pattern::Reverse },
		{ "SHIFT", Pattern::Shift },
		{ "CLEAR", Pattern::Clear },
	};
	for (int i = 0; i < 4; ++i)
	{
		auto* button = new SpyButton (toCRect (patternButton (i)), kButtons[i].name, SpyButton::Shape::Box);
		button->setCap (5.4);
		const Pattern pattern = kButtons[i].pattern;
		button->setClickHandler ([this, pattern] () { applyPattern (pattern); });
		mPatternButtons[i] = button;
		frame->addView (button);
	}

	addKnob (kSampThroughLevel)->setFormatter ([] (float v) {
		const double plain = paramDef (kSampThroughLevel).toPlain (v);
		return plain < 0.5 ? std::string ("OFF") : formatPercent (plain);
	});

	addKnob (kOutputTrim)->setFormatter ([] (float v) {
		char buffer[32];
		std::snprintf (buffer, sizeof (buffer), "%+.1f DB", outputDecibels (v));
		return std::string (buffer);
	});

	mOutMeter = new SpyOutMeter (toCRect (kOutScreen));
	frame->addView (mOutMeter);
}

//------------------------------------------------------------------------
void SpyBandEditor::selectTab (int index)
{
	if (mTabs)
		mTabs->setSelected (index);

	if (mBandsDisplay)
		mBandsDisplay->setVisible (index == 1);
	if (mBigBoard)
		mBigBoard->setVisible (index == 0);
	if (mMatrixCaption)
		mMatrixCaption->setVisible (index == 0);
	for (CView* view : mInputPage)
		view->setVisible (index == 2);

	if (frame)
		frame->invalidRect (toCRect (Layout::bezelOf (Layout::kBandsScreen)));
}

//------------------------------------------------------------------------
void PLUGIN_API SpyBandEditor::close ()
{
	mTimer = nullptr;

	mControls.clear ();
	mKnobs.clear ();
	for (auto*& slot : mSlots)
		slot = nullptr;
	mTabs = nullptr;
	mBandsDisplay = nullptr;
	mBigBoard = nullptr;
	mPanelBoard = nullptr;
	mInputPage.clear ();
	mInputBar[0] = mInputBar[1] = nullptr;
	mInputBarBig[0] = mInputBarBig[1] = nullptr;
	mCorrelation = mCorrelationBig = nullptr;
	mRateLabel = nullptr;
	mOutMeter = nullptr;
	mMatrixSize = mPinCount = mOffDiagonal = nullptr;
	for (auto*& b : mPatternButtons)
		b = nullptr;
	mPresetBox = nullptr;
	mVersionPill = nullptr;
	mVoicedLamp = nullptr;
	mMatrixCaption = nullptr;
	mCorrReading = nullptr;

	if (frame)
	{
		frame->forget ();
		frame = nullptr;
	}
}

//------------------------------------------------------------------------
// Everything on the panel that is not a parameter.
//------------------------------------------------------------------------
void SpyBandEditor::refresh ()
{
	if (mController == nullptr || frame == nullptr)
		return;

	mController->requestMeter ();

	const double sampleRate = mController->dspSampleRate ();
	const double freq = paramInternal (kFreq);
	const double spread = paramInternal (kFreqSpread);
	const int bands = currentBands ();
	const int slopes = static_cast<int> (paramInternal (kFilterSlopes));

	for (SpyKnob* knob : mKnobs)
		knob->refreshReadout ();

	//--- the band display -------------------------------------------------
	if (mBandsDisplay)
	{
		SpyBandsDisplay::State state;
		state.bands = bands;
		for (int b = 0; b < bands; ++b)
			state.centreHz[b] = bandCentreHz (freq, spread, bands, b, sampleRate);
		state.bottomHz = bandBottomHz (freq);
		state.topHz = bandTopHz (freq, spread, sampleRate);
		state.resonance = paramInternal (kResonance);
		state.slopes = slopes;
		state.sampleRate = sampleRate;
		static const char* const kSlopeNames[3] = { "SHALLOW", "MEDIUM", "STEEP" };
		state.mode = std::to_string (bands) + " BANDS \xC2\xB7 " + kSlopeNames[std::clamp (slopes, 0, 2)];
		mBandsDisplay->setState (state);

		// The envelopes. In stereo the meter carries both sides; the
		// display shows the louder of the two.
		double meter[2 * kMaxBands] = { 0.0 };
		const int count = mController->meter (meter);
		double env[kMaxBands] = { 0.0 };
		if (count >= 2 * bands)
			for (int b = 0; b < bands; ++b)
				env[b] = std::max (meter[b], meter[bands + b]);
		else
			for (int b = 0; b < bands && b < count; ++b)
				env[b] = meter[b];
		mBandsDisplay->setEnvelopes (env, bands);
	}

	//--- the matrix --------------------------------------------------------
	if (mPanelBoard)
		mPanelBoard->setBands (bands);
	if (mBigBoard)
		mBigBoard->setBands (bands);

	double matrix[kPatchCells];
	for (int i = 0; i < kPatchCells; ++i)
		matrix[i] = mController->getParamNormalized (static_cast<ParamID> (kPatchBase + i));

	if (mMatrixSize)
		mMatrixSize->setText (std::to_string (bands) + " \xC3\x97 " + std::to_string (bands) + " OF 22");
	const int pins = pinCount (matrix, bands);
	const int off = offDiagonalCount (matrix, bands);
	if (mPinCount)
		mPinCount->setText (std::to_string (pins) + (pins == 1 ? " PIN" : " PINS"));
	if (mOffDiagonal)
		mOffDiagonal->setText (std::to_string (off) + " OFF-DIAG");

	// Outline the pattern the matrix is now, if it is one.
	{
		bool diagonal = pins == bands, reverse = pins == bands;
		for (int row = 0; row < bands; ++row)
			for (int column = 0; column < bands; ++column)
			{
				const bool pinned = matrix[row * kMaxBands + column] > 0.0;
				if (pinned != (row == column))
					diagonal = false;
				if (pinned != (column == bands - 1 - row))
					reverse = false;
			}
		if (mPatternButtons[0])
			mPatternButtons[0]->setHighlighted (diagonal);
		if (mPatternButtons[1])
			mPatternButtons[1]->setHighlighted (reverse && bands > 1);
		if (mPatternButtons[3])
			mPatternButtons[3]->setHighlighted (pins == 0);
	}

	//--- the slots ---------------------------------------------------------
	const bool interlaced = paramInternal (kInterlaced) >= 0.5;
	for (int slot = 0; slot < Vocoder::kNumSlots; ++slot)
	{
		if (mSlots[slot] == nullptr)
			continue;
		const std::string& path = mController->slotPath (slot);
		std::string name;
		if (! path.empty ())
		{
			std::size_t start = path.find_last_of ("/\\");
			name = upper (path.substr (start == std::string::npos ? 0 : start + 1));
		}
		mSlots[slot]->setFile (name, ! path.empty () && ! mController->slotLoaded (slot));
		mSlots[slot]->setOverview (mController->slotOverview (slot));
		mSlots[slot]->setInactive (interlaced);
	}

	//--- the meters --------------------------------------------------------
	for (int i = 0; i < 2; ++i)
	{
		if (mInputBar[i])
			mInputBar[i]->setLevel (mController->inputPeak (i));
		if (mInputBarBig[i])
			mInputBarBig[i]->setLevel (mController->inputPeak (i));
	}
	if (mCorrelation)
	{
		mCorrelation->setCorrelation (mController->inputCorrelation ());
		if (mCorrReading)
		{
			char buffer[24];
			if (mCorrelation->haveReading ())
				std::snprintf (buffer, sizeof (buffer), "CORR %.2f", mCorrelation->smoothed ());
			else
				std::snprintf (buffer, sizeof (buffer), "CORR --");
			mCorrReading->setText (buffer);
		}
	}
	if (mCorrelationBig)
		mCorrelationBig->setCorrelation (mController->inputCorrelation ());
	if (mOutMeter)
		mOutMeter->setLevels (mController->outputPeak (0), mController->outputPeak (1));

	if (mVoicedLamp)
	{
		mVoicedLamp->setLit (paramInternal (kVoicedDetect) >= 0.5 && mController->voiced ());
	}

	// The version lamp: lit while meter replies keep arriving.
	if (mVersionPill)
	{
		const unsigned frames = mController->meterFrames ();
		mFramesSinceMeter = (frames != mLastMeterFrames) ? 0 : mFramesSinceMeter + 1;
		mLastMeterFrames = frames;
		const float alive = (frames > 0 && mFramesSinceMeter < 30) ? 1.f : 0.f;
		if (alive != mVersionPill->getValueNormalized ())
		{
			mVersionPill->setValueNormalized (alive);
			mVersionPill->invalid ();
		}
	}

	if (mRateLabel)
	{
		char buffer[48];
		std::snprintf (buffer, sizeof (buffer), "DSP RATE %.1f KHZ", sampleRate / 1000.0);
		mRateLabel->setText (buffer);
	}

	//--- the preset name ---------------------------------------------------
	if (mPresetBox)
	{
		if (mController->presetName ().empty ())
			mPresetBox->setName ("NO PRESET", "DXI DEFAULTS", mController->presetModified ());
		else
			mPresetBox->setName (mController->presetBank (), upper (mController->presetName ()),
			                     mController->presetModified ());
	}
}

//------------------------------------------------------------------------
// Files
//------------------------------------------------------------------------
void SpyBandEditor::chooseFile (int slot)
{
	if (frame == nullptr || mController == nullptr)
		return;

	auto* selector = CNewFileSelector::create (frame, CNewFileSelector::kSelectFile);
	if (selector == nullptr)
		return;

	selector->setTitle ("Choose a wave file");
	selector->addFileExtension (CFileExtension ("Wave", "wav"));
	selector->setAllowMultiFileSelection (false);
	selector->run ([this, slot] (CNewFileSelector* s) {
		if (s->getNumSelectedFiles () > 0 && mController)
		{
			if (UTF8StringPtr path = s->getSelectedFile (0))
			{
				mController->requestSlotLoad (slot, std::string (path));
				edited ();
			}
		}
	});
	selector->forget ();
}

void SpyBandEditor::showSlotMenu (int slot)
{
	if (frame == nullptr || mController == nullptr || mSlots[slot] == nullptr)
		return;

	auto menu = makeOwned<COptionMenu> ();
	menu->addEntry ("Choose File...");
	auto* clear = menu->addEntry ("Clear Slot");
	if (mController->slotPath (slot).empty ())
		clear->setEnabled (false);

	const CRect r = mSlots[slot]->getViewSize ();
	menu->popup (frame, CPoint (r.left + 44, r.top + 20), [this, slot] (COptionMenu* m) {
		const int32_t index = m->getLastResult ();
		if (index == 0)
			chooseFile (slot);
		else if (index == 1 && mController)
		{
			mController->requestSlotLoad (slot, std::string ());
			edited ();
		}
	});
}

//------------------------------------------------------------------------
// Presets
//------------------------------------------------------------------------
void SpyBandEditor::stepPreset (int delta)
{
	if (mController == nullptr)
		return;
	const int count = factoryPresetCount ();
	int index = mController->factoryIndex ();
	index = (index < 0) ? (delta > 0 ? 0 : count - 1) : (index + delta + count) % count;
	mController->loadFactoryPreset (index);
	refresh ();
}

void SpyBandEditor::showPresetMenu ()
{
	if (frame == nullptr || mController == nullptr || mPresetBox == nullptr)
		return;

	auto menu = makeOwned<COptionMenu> ();
	std::vector<std::function<void ()>> actions;

	menu->addEntry ("Factory", -1, CMenuItem::kTitle);
	actions.emplace_back ();
	for (int i = 0; i < factoryPresetCount (); ++i)
	{
		auto* item = menu->addEntry (factoryPresetName (i));
		if (i == mController->factoryIndex ())
			item->setChecked (true);
		actions.emplace_back ([this, i] () { mController->loadFactoryPreset (i); });
	}

	const auto user = mController->userPresets ();
	menu->addSeparator ();
	actions.emplace_back ();
	menu->addEntry ("User", -1, CMenuItem::kTitle);
	actions.emplace_back ();
	if (user.empty ())
	{
		menu->addEntry ("(none saved yet)", -1, CMenuItem::kDisabled);
		actions.emplace_back ();
	}
	for (const std::string& path : user)
	{
		std::size_t start = path.find_last_of ("/\\");
		std::string name = path.substr (start == std::string::npos ? 0 : start + 1);
		if (name.size () > 10)
			name = name.substr (0, name.size () - 10);   // ".vstpreset"
		auto* item = menu->addEntry (name.c_str ());
		if (mController->presetBank () == "USER" && mController->presetName () == name)
			item->setChecked (true);
		actions.emplace_back ([this, path] () { mController->loadUserPreset (path); });
	}

	menu->addSeparator ();
	actions.emplace_back ();
	menu->addEntry ("Save As...");
	actions.emplace_back ([this] () { savePreset (); });
	menu->addEntry ("Load From File...");
	actions.emplace_back ([this] () {
		auto* selector = CNewFileSelector::create (frame, CNewFileSelector::kSelectFile);
		if (selector == nullptr)
			return;
		selector->setTitle ("Load a SpyBand preset");
		selector->addFileExtension (CFileExtension ("VST3 Preset", "vstpreset"));
		selector->setInitialDirectory (mController->userPresetFolder ().c_str ());
		selector->run ([this] (CNewFileSelector* s) {
			if (s->getNumSelectedFiles () > 0 && mController)
				if (UTF8StringPtr path = s->getSelectedFile (0))
					mController->loadUserPreset (std::string (path));
		});
		selector->forget ();
	});

	const CRect r = mPresetBox->getViewSize ();
	menu->popup (frame, CPoint (r.left, r.bottom + 2), [this, actions] (COptionMenu* m) {
		const int32_t index = m->getLastResult ();
		if (index >= 0 && index < static_cast<int32_t> (actions.size ()) && actions[static_cast<std::size_t> (index)])
			actions[static_cast<std::size_t> (index)] ();
		refresh ();
	});
}

void SpyBandEditor::savePreset ()
{
	if (frame == nullptr || mController == nullptr)
		return;

	auto* selector = CNewFileSelector::create (frame, CNewFileSelector::kSelectSaveFile);
	if (selector == nullptr)
		return;
	selector->setTitle ("Save SpyBand preset");
	selector->addFileExtension (CFileExtension ("VST3 Preset", "vstpreset"));
	selector->setDefaultExtension (CFileExtension ("VST3 Preset", "vstpreset"));
	selector->setInitialDirectory (mController->userPresetFolder ().c_str ());
	const std::string suggested = mController->presetName ().empty ()
		? std::string ("My Preset") : mController->presetName ();
	selector->setDefaultSaveName ((suggested + ".vstpreset").c_str ());
	selector->run ([this] (CNewFileSelector* s) {
		if (s->getNumSelectedFiles () > 0 && mController)
			if (UTF8StringPtr path = s->getSelectedFile (0))
				mController->saveUserPreset (std::string (path));
		refresh ();
	});
	selector->forget ();
}

//------------------------------------------------------------------------
// Matrix patterns
//------------------------------------------------------------------------
void SpyBandEditor::applyPattern (Pattern pattern)
{
	if (mController == nullptr)
		return;

	double matrix[kPatchCells];
	for (int i = 0; i < kPatchCells; ++i)
		matrix[i] = mController->getParamNormalized (static_cast<ParamID> (kPatchBase + i));

	SpyBand::applyPattern (pattern, currentBands (), matrix);

	mController->startGroupEdit ();
	for (int i = 0; i < kPatchCells; ++i)
	{
		const ParamID id = static_cast<ParamID> (kPatchBase + i);
		if (std::fabs (mController->getParamNormalized (id) - matrix[i]) < 1e-9)
			continue;
		mController->beginEdit (id);
		mController->setParamNormalized (id, matrix[i]);
		mController->performEdit (id, matrix[i]);
		mController->endEdit (id);
	}
	mController->finishGroupEdit ();
	edited ();
	if (mPanelBoard)
		mPanelBoard->invalid ();
	if (mBigBoard)
		mBigBoard->invalid ();
	refresh ();
}

//------------------------------------------------------------------------
// The band display's handles move Bottom Freq and Top Freq. Top is a
// SPREAD above Bottom, so moving Bottom alone moves Top too - which is
// what the knobs do as well, and is the DXi's arithmetic.
//------------------------------------------------------------------------
void SpyBandEditor::dragBandEdge (int which, double hz, int phase)
{
	if (mController == nullptr)
		return;

	const ParamID id = which == 0 ? kFreq : kFreqSpread;
	if (phase == 0)
	{
		mController->beginEdit (id);
		return;
	}

	const double value = which == 0 ? freqForBottomHz (hz) : spreadForTopHz (hz, paramInternal (kFreq));
	if (std::fabs (value - mController->getParamNormalized (id)) > 1e-9)
	{
		mController->setParamNormalized (id, value);
		mController->performEdit (id, value);
		edited ();
	}

	if (phase == 2)
		mController->endEdit (id);
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
	if (tag != kBypass)
		edited ();
}

void SpyBandEditor::controlBeginEdit (CControl* control)
{
	if (mController == nullptr || control == nullptr)
		return;
	const ParamID tag = static_cast<ParamID> (control->getTag ());
	// A turn of BANDS re-fits the matrix when it ends: one group edit for
	// the count and every cell it moves, so a host undoes them together.
	if (tag == kBands)
	{
		mBandsBeforeEdit = currentBands ();
		mController->startGroupEdit ();
	}
	mController->beginEdit (tag);
}

void SpyBandEditor::controlEndEdit (CControl* control)
{
	if (mController == nullptr || control == nullptr)
		return;
	const ParamID tag = static_cast<ParamID> (control->getTag ());
	mController->endEdit (tag);
	if (tag == kBands)
	{
		refitMatrixToBands (mBandsBeforeEdit, currentBands ());
		mController->finishGroupEdit ();
	}
}

//------------------------------------------------------------------------
// The band count changed under the user's hand: carry the pattern to the
// new count (refitMatrix in SpyBandPresets.cpp says how).
//------------------------------------------------------------------------
void SpyBandEditor::refitMatrixToBands (int from, int to)
{
	if (mController == nullptr || from == to)
		return;

	double matrix[kPatchCells];
	for (int i = 0; i < kPatchCells; ++i)
		matrix[i] = mController->getParamNormalized (static_cast<ParamID> (kPatchBase + i));

	SpyBand::refitMatrix (from, to, matrix);

	for (int i = 0; i < kPatchCells; ++i)
	{
		const ParamID id = static_cast<ParamID> (kPatchBase + i);
		if (std::fabs (mController->getParamNormalized (id) - matrix[i]) < 1e-9)
			continue;
		mController->beginEdit (id);
		mController->setParamNormalized (id, matrix[i]);
		mController->performEdit (id, matrix[i]);
		mController->endEdit (id);
	}
	edited ();
	if (mPanelBoard)
		mPanelBoard->invalid ();
	if (mBigBoard)
		mBigBoard->invalid ();
}

//------------------------------------------------------------------------
// The patch matrix: 484 parameters behind two views.
//------------------------------------------------------------------------
double SpyBandEditor::patchValue (int row, int column) const
{
	if (mController == nullptr)
		return patchDefaultNormalized (row, column);
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
	edited ();
}

void SpyBandEditor::endPatchEdit (int row, int column)
{
	if (mController)
		mController->endEdit (patchParam (row, column));
}

double SpyBandEditor::restoreValue (int row, int column) const
{
	return mRestore[row * kMaxBands + column];
}

void SpyBandEditor::setRestoreValue (int row, int column, double value)
{
	mRestore[row * kMaxBands + column] = value;
}

std::string SpyBandEditor::bandLabel (int index) const
{
	const double rate = mController ? mController->dspSampleRate () : 44100.0;
	const double hz = bandCentreHz (paramInternal (kFreq), paramInternal (kFreqSpread),
	                                currentBands (), index, rate);
	char buffer[16];
	if (hz >= 10000.0)
		std::snprintf (buffer, sizeof (buffer), "%.1fK", hz / 1000.0);
	else if (hz >= 1000.0)
		std::snprintf (buffer, sizeof (buffer), "%.2fK", hz / 1000.0);
	else
		std::snprintf (buffer, sizeof (buffer), "%.0f", hz);
	return buffer;
}

//------------------------------------------------------------------------
void SpyBandEditor::updateControl (ParamID tag, ParamValue normalized)
{
	if (frame == nullptr)
		return;

	if (isPatchParam (tag))
	{
		if (mPanelBoard)
			mPanelBoard->invalid ();
		if (mBigBoard)
			mBigBoard->invalid ();
		return;
	}

	// The band figures on both boards follow Bottom, Top and the count.
	if ((tag == kFreq || tag == kFreqSpread || tag == kBands))
	{
		if (mPanelBoard)
			mPanelBoard->invalid ();
		if (mBigBoard)
			mBigBoard->invalid ();
	}

	auto it = mControls.find (tag);
	if (it == mControls.end () || it->second == nullptr)
		return;

	it->second->setValueNormalized (static_cast<float> (normalized));
	it->second->invalid ();
}

//------------------------------------------------------------------------
} // namespace SpyBand
