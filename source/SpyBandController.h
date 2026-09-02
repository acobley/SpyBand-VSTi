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
#include "Vocoder.h"

#include "public.sdk/source/vst/vsteditcontroller.h"

#include <string>

namespace SpyBand {

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
	Steinberg::tresult PLUGIN_API notify (Steinberg::Vst::IMessage* message) SMTG_OVERRIDE;

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

private:
	void addParameters ();

	double mSampleRate = 44100.0;

	std::string mSlotPath[Vocoder::kNumSlots];
	bool        mSlotLoaded[Vocoder::kNumSlots] = { false, false, false, false, false };

	double mMeter[2 * kMaxBands] = { 0.0 };
	int    mMeterCount = 0;
	bool   mVoiced = false;
};

//------------------------------------------------------------------------
} // namespace SpyBand
