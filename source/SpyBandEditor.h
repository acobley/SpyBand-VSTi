//------------------------------------------------------------------------
// SpyBand - editor
//
// Recreates IDD_PROPPAGE from SpyBand.rc. The dialog is 378 x 217 dialog
// units in MS Sans Serif 8pt, and the scale is settled by the background
// static alone: 376 x 217 DLU holding a 564 x 353 bitmap gives
//
//     x_px = x_dlu * 1.5        y_px = (y_dlu - 1) * 1.625
//
// (the -1 because the static sits one unit down, and this editor draws the
// artwork at the origin instead of reproducing that margin). Every
// position below came out of tools/rc-geometry.py; none was placed by eye.
//
// The canvas is the bitmap: 564 x 353. The Windows dialog was 567 x 353
// and clipped the last millimetre of its own backdrop.
//
// Two things are NOT from the .rc:
//
//   * Output Trim, which is new - see PORTING-NOTES section 5. It goes in
//     the empty bottom-left corner, on the same rhythm as the column above
//     it.
//   * IDC_MOOG, which IS in the .rc and is not here: it is declared
//     NOT WS_VISIBLE, the parameter behind it is commented out of
//     Parameters.h, and the filter it selected is unreachable.
//
// The input LED columns are new too, and live in the 48-pixel channel the
// dialog left empty between its first and second columns of controls.
// Being new they have no dialog units to convert from, so they are placed
// in pixels - as ForTran's added panels are.
//------------------------------------------------------------------------

#pragma once

#include "SpyBandControls.h"
#include "SpyBandParams.h"
#include "Vocoder.h"

#include "public.sdk/source/vst/vstguieditor.h"

#include <map>
#include <string>

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

	/** The controller's setParamNormalized reaches the panel through here. */
	void updateControl (Steinberg::Vst::ParamID tag, Steinberg::Vst::ParamValue normalized);

	static constexpr int kEditorWidth  = 564;
	static constexpr int kEditorHeight = 353;

private:
	VSTGUI::CBitmap* bitmap (const char* name);

	/** Every position is given in DIALOG UNITS, exactly as the .rc has
	    them, and converted here - so a position in this file can be
	    checked against the .rc line it came from without arithmetic. */
	VSTGUI::CRect fromDlu (double x, double y, double w, double h) const;

	void registerControl (Steinberg::Vst::ParamID tag, VSTGUI::CControl* control);

	SpySlider*   addSlider (Steinberg::Vst::ParamID tag, const char* label,
	                        double x, double y, double w, double h);
	SpyToggle*   addToggle (Steinberg::Vst::ParamID tag,
	                        double x, double y, double w, double h);
	SpySelector* addSelector (Steinberg::Vst::ParamID tag, const char* const* names,
	                          int count, double x, double y, double w, double h);
	void addFileButton (int slot, double x, double y, double w, double h);

	/** The timer's work: everything the DXi's OnTimer did, which is
	    everything the panel shows that is not a parameter. */
	void refresh ();

	SpyBandController* mController = nullptr;

	std::map<std::string, VSTGUI::SharedPointer<VSTGUI::CBitmap>> mBitmaps;
	std::map<Steinberg::Vst::ParamID, VSTGUI::CControl*> mControls;

	SpyFileButton* mFileButtons[Vocoder::kNumSlots] = { nullptr };
	SpyLedColumn*  mInputMeter[2] = { nullptr, nullptr };
	SpyPatchBoard* mPatchBoard = nullptr;
	VSTGUI::CTextLabel* mAxisX = nullptr;
	VSTGUI::CTextLabel* mAxisY = nullptr;
	SpyBandMeter*  mMeter = nullptr;
	VSTGUI::CTextLabel* mVersion = nullptr;

	VSTGUI::SharedPointer<VSTGUI::CVSTGUITimer> mTimer;
};

//------------------------------------------------------------------------
} // namespace SpyBand
