//------------------------------------------------------------------------
// SpyBand - edit controller
//
// Replaces CSpyBandPropPage's half of the DXi: the parameter list the host
// sees, the state the editor reads, and the two things the DXi's dialog
// took straight off the DSP - the band meter and the voiced lamp - which
// now arrive as messages because the processor and the controller are
// separate components.
//------------------------------------------------------------------------

#pragma once

#include "SpyBandParams.h"
#include "SpyBandPresets.h"
#include "Vocoder.h"

#include "public.sdk/source/vst/vsteditcontroller.h"

#include <string>
#include <vector>

namespace SpyBand {

class SpyBandEditor;

//------------------------------------------------------------------------
class SpyBandController : public Steinberg::Vst::EditControllerEx1
{
public:
	SpyBandController () = default;
	~SpyBandController () SMTG_OVERRIDE = default;

	static Steinberg::FUnknown* createInstance (void*)
	{
		return (Steinberg::Vst::IEditController*)new SpyBandController;
	}

	Steinberg::tresult PLUGIN_API initialize (Steinberg::FUnknown* context) SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API terminate () SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API setComponentState (Steinberg::IBStream* state) SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API setState (Steinberg::IBStream* state) SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API getState (Steinberg::IBStream* state) SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API notify (Steinberg::Vst::IMessage* message) SMTG_OVERRIDE;

	Steinberg::IPlugView* PLUGIN_API createView (Steinberg::FIDString name) SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API setParamNormalized (
		Steinberg::Vst::ParamID tag, Steinberg::Vst::ParamValue value) SMTG_OVERRIDE;

	void editorAttached (Steinberg::Vst::EditorView* editor) SMTG_OVERRIDE;
	void editorRemoved (Steinberg::Vst::EditorView* editor) SMTG_OVERRIDE;
	void editorDestroyed (Steinberg::Vst::EditorView* editor) SMTG_OVERRIDE;

	//--------------------------------------------------------------------
	// What the editor needs and cannot compute for itself
	//--------------------------------------------------------------------

	/** The rate the DSP is actually running at, so a frequency readout
	    cannot disagree with the filter it names. 44100 until the processor
	    says otherwise. */
	double dspSampleRate () const { return mSampleRate; }

	/** The path in a slot, and whether it actually loaded. */
	const std::string& slotPath (int slot) const;
	bool slotLoaded (int slot) const;

	/** The file's own name, without directory or extension - what the DXi
	    put on the button (CSpyBand::GetFileName). Returns "File n" for an
	    empty slot, as it did. */
	std::string slotLabel (int slot) const;

	/** Ask the processor to load, or to empty when `path` is empty. */
	void requestSlotLoad (int slot, const std::string& path);

	/** Ask for a meter frame; the answer arrives at notify(). */
	void requestMeter ();

	/** The last meter frame: writes up to 2 * kMaxBands values and returns
	    how many. */
	int meter (double* out) const;
	bool voiced () const { return mVoiced; }

	/** The two input peaks the LED columns show, post Src Level. `which`
	    is 0 for left and 1 for right. */
	double inputPeak (int which) const
	{
		return (which >= 0 && which < 2) ? mInputPeak[which] : 0.0;
	}

	/** How alike the two input channels are - the last block's
	    correlation coefficient, or below -1.5 when it was too quiet to
	    tell. */
	double inputCorrelation () const { return mCorrelation; }

	/** The two output peaks, for the OUT meter. */
	double outputPeak (int which) const
	{
		return (which >= 0 && which < 2) ? mOutputPeak[which] : 0.0;
	}

	/** Counts meter replies, so the editor can tell a live DSP from a
	    silent one. */
	unsigned meterFrames () const { return mMeterFrames; }

	/** A slot's waveform as about 160 peaks, 0..1, scaled to the file's
	    own loudest - empty when nothing is loaded. Read from the file
	    once, when the slot's path changes. */
	const std::vector<float>& slotOverview (int slot);

	//--------------------------------------------------------------------
	// Presets
	//
	// Factory presets are in SpyBandPresets.cpp. User presets are
	// .vstpreset files in the standard VST3 preset folder, so a host that
	// browses that folder finds them too; their component chunk is
	// exactly the processor's state.
	//--------------------------------------------------------------------

	/** Every parameter as it now stands, plus the slot paths. */
	Patch currentPatch ();

	/** Make the plug-in sound like `patch`: every parameter that differs
	    is edited as the user would, so the host records it and the
	    processor follows. `withSamples` also loads its slot paths and
	    slot switches; a factory preset leaves both alone. */
	void applyPatch (const Patch& patch, bool withSamples);

	void loadFactoryPreset (int index);

	/** "FACTORY", "USER" or "" before anything has been loaded. */
	const std::string& presetBank () const { return mPresetBank; }
	const std::string& presetName () const { return mPresetName; }
	int factoryIndex () const { return mFactoryIndex; }

	/** The folder user presets live in, created if need be. */
	std::string userPresetFolder () const;
	/** The .vstpreset files in it, full paths, sorted by name. */
	std::vector<std::string> userPresets () const;

	bool saveUserPreset (const std::string& path);
	bool loadUserPreset (const std::string& path);

	/** The user moved a control: the name in the header gets its "*". */
	void markModified () { mPresetModified = true; }
	bool presetModified () const { return mPresetModified; }

private:
	void addParameters ();
	void setEdited (Steinberg::Vst::ParamID id, double value);

	double mSampleRate = 44100.0;

	std::string mSlotPath[Vocoder::kNumSlots];
	bool        mSlotLoaded[Vocoder::kNumSlots] = { false, false, false, false, false };

	std::vector<SpyBandEditor*> mEditors;

	double mMeter[2 * kMaxBands] = { 0.0 };
	int    mMeterCount = 0;
	bool   mVoiced = false;
	double mInputPeak[2] = { 0.0, 0.0 };
	double mCorrelation = -2.0;
	double mOutputPeak[2] = { 0.0, 0.0 };
	unsigned mMeterFrames = 0;

	std::string mOverviewPath[Vocoder::kNumSlots];
	std::vector<float> mOverview[Vocoder::kNumSlots];

	std::string mPresetBank;
	std::string mPresetName;
	int mFactoryIndex = -1;
	bool mPresetModified = false;
	bool mApplyingPatch = false;
};

//------------------------------------------------------------------------
} // namespace SpyBand
