//------------------------------------------------------------------------
// SpyBand - editor
//
// Implements Designs/SpyBand-Polivoks-1-BlackPanel.png: Concept 6's
// layout redressed as R2Synth's black panel - white printing on matte
// black, knurled knobs, rotary selectors, OLED screens, and the Studio
// Worn wear. 1200 x 690. The picture of the layout, and every position,
// is in SpyBandLayout.h.
//
// Nothing here maps a parameter differently from the Concept 6 editor:
// the ids, ranges and readouts are the parameter table's.
//
// The layout is built in buildViews(), separately from open(), so that
// tools/render-editor can draw the real editor into a PNG on a machine
// with no window - which is how the panel is checked against the design.
//------------------------------------------------------------------------

#pragma once

#include "SpyBandControls.h"
#include "SpyBandParams.h"
#include "SpyBandPresets.h"
#include "Vocoder.h"

#include "public.sdk/source/vst/vstguieditor.h"

#include <map>
#include <string>
#include <vector>

namespace SpyBand {

class SpyBandController;

//------------------------------------------------------------------------
class SpyBandEditor : public Steinberg::Vst::VSTGUIEditor,
                      public VSTGUI::IControlListener,
                      public IPatchBoardListener
{
public:
	explicit SpyBandEditor (SpyBandController* controller);

	bool PLUGIN_API open (void* parent, const VSTGUI::PlatformType& platformType) SMTG_OVERRIDE;
	void PLUGIN_API close () SMTG_OVERRIDE;

	// IControlListener
	void valueChanged (VSTGUI::CControl* control) SMTG_OVERRIDE;
	void controlBeginEdit (VSTGUI::CControl* control) SMTG_OVERRIDE;
	void controlEndEdit (VSTGUI::CControl* control) SMTG_OVERRIDE;

	// IPatchBoardListener
	double patchValue (int row, int column) const SMTG_OVERRIDE;
	void beginPatchEdit (int row, int column) SMTG_OVERRIDE;
	void setPatchValue (int row, int column, double value) SMTG_OVERRIDE;
	void endPatchEdit (int row, int column) SMTG_OVERRIDE;
	double restoreValue (int row, int column) const SMTG_OVERRIDE;
	void setRestoreValue (int row, int column, double value) SMTG_OVERRIDE;
	std::string bandLabel (int index) const SMTG_OVERRIDE;

	/** The controller's setParamNormalized reaches the panel through here. */
	void updateControl (Steinberg::Vst::ParamID tag, Steinberg::Vst::ParamValue normalized);

	/** Build every view into `frame`, and run one refresh. open() calls
	    this; so does tools/render-editor, with a frame it never opens. */
	void buildViews (VSTGUI::CFrame* target);

	/** Choose the display's tab: 0 Matrix, 1 Bands, 2 Input. */
	void selectTab (int index);

	/** Everything the timer does: meters, readouts, labels. */
	void refresh ();

	static constexpr int kEditorWidth  = 1200;
	static constexpr int kEditorHeight = 690;

private:
	void registerControl (Steinberg::Vst::ParamID tag, VSTGUI::CControl* control);

	/** The knob and selector for `tag`, from Layout::kKnobs and
	    kSelectors, with their wear. */
	SpyKnob* addKnob (Steinberg::Vst::ParamID tag);
	SpySelector* addSelector (Steinberg::Vst::ParamID tag);

	void buildHeader ();
	void buildModulator ();
	void buildCentre ();
	void buildRight ();

	void chooseFile (int slot);
	void showSlotMenu (int slot);
	void showPresetMenu ();
	void stepPreset (int delta);
	void savePreset ();
	void applyPattern (Pattern pattern);
	/** Carry the matrix from `from` bands to `to` - see refitMatrix. */
	void refitMatrixToBands (int from, int to);

	/** Drag on the band display: which 0 = Bottom, 1 = Top. */
	void dragBandEdge (int which, double hz, int phase);

	double paramInternal (Steinberg::Vst::ParamID id) const;
	int currentBands () const;
	void edited ();

	SpyBandController* mController = nullptr;

	std::map<Steinberg::Vst::ParamID, VSTGUI::CControl*> mControls;
	std::vector<SpyKnob*> mKnobs;

	SpySlotRow* mSlots[Vocoder::kNumSlots] = { nullptr };
	SpyTabs* mTabs = nullptr;
	SpyBandsDisplay* mBandsDisplay = nullptr;
	SpyPinBoard* mBigBoard = nullptr;
	SpyPinBoard* mPanelBoard = nullptr;
	std::vector<VSTGUI::CView*> mInputPage;
	SpyLevelBar* mInputBar[2] = { nullptr, nullptr };
	SpyLevelBar* mInputBarBig[2] = { nullptr, nullptr };
	SpyCorrelationLamp* mCorrelation = nullptr;
	SpyCorrelationLamp* mCorrelationBig = nullptr;
	SpyLabel* mRateLabel = nullptr;
	SpyOutMeter* mOutMeter = nullptr;
	SpyLabel* mMatrixSize = nullptr;
	SpyLabel* mPinCount = nullptr;
	SpyLabel* mOffDiagonal = nullptr;
	SpyButton* mPatternButtons[4] = { nullptr, nullptr, nullptr, nullptr };
	SpyPresetBox* mPresetBox = nullptr;
	SpyPill* mVersionPill = nullptr;
	SpyLamp* mVoicedLamp = nullptr;
	SpyLabel* mMatrixCaption = nullptr;
	SpyLabel* mCorrReading = nullptr;

	/** The band count when a turn of the BANDS selector began. */
	int mBandsBeforeEdit = 0;

	unsigned mLastMeterFrames = 0;
	int mFramesSinceMeter = 0;

	/** What a cell goes back to when switched on again - see
	    IPatchBoardListener::restoreValue. Shared by both boards. */
	double mRestore[kPatchCells];

	VSTGUI::SharedPointer<VSTGUI::CVSTGUITimer> mTimer;
};

//------------------------------------------------------------------------
} // namespace SpyBand
