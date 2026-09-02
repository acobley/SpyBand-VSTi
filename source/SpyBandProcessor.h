//------------------------------------------------------------------------
// SpyBand - audio processor
//
// Replaces CSpyBand::Process from the DXi. The vocoder itself is in
// Vocoder.h and knows nothing about VST3; everything here is host
// plumbing.
//------------------------------------------------------------------------

#pragma once

#include "SpyBandParams.h"
#include "Vocoder.h"

#include "public.sdk/source/vst/vstaudioeffect.h"

#include <string>
#include <vector>

namespace SpyBand {

//------------------------------------------------------------------------
class SpyBandProcessor : public Steinberg::Vst::AudioEffect
{
public:
	SpyBandProcessor ();
	~SpyBandProcessor () SMTG_OVERRIDE = default;

	static Steinberg::FUnknown* createInstance (void*)
	{
		return (Steinberg::Vst::IAudioProcessor*)new SpyBandProcessor;
	}

	Steinberg::tresult PLUGIN_API initialize (Steinberg::FUnknown* context) SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API terminate () SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API setActive (Steinberg::TBool state) SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API setupProcessing (Steinberg::Vst::ProcessSetup& setup) SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API canProcessSampleSize (Steinberg::int32 symbolicSampleSize) SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API setBusArrangements (Steinberg::Vst::SpeakerArrangement* inputs,
	                                                  Steinberg::int32 numIns,
	                                                  Steinberg::Vst::SpeakerArrangement* outputs,
	                                                  Steinberg::int32 numOuts) SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API process (Steinberg::Vst::ProcessData& data) SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API notify (Steinberg::Vst::IMessage* message) SMTG_OVERRIDE;

	Steinberg::tresult PLUGIN_API setState (Steinberg::IBStream* state) SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API getState (Steinberg::IBStream* state) SMTG_OVERRIDE;

	Steinberg::uint32 PLUGIN_API getTailSamples () SMTG_OVERRIDE;

private:
	void applyParameterChanges (Steinberg::Vst::IParameterChanges* changes);
	void buildVocoderParams (Vocoder::Params& out) const;

	/** Load, or clear when `path` is empty, and tell the controller what
	    happened. UI thread only - it reads a file. */
	void loadSlot (int slot, const std::string& path);

	/** Processor -> controller, from setActive / setState / notify, all of
	    which VST3 documents as UI-thread. A message sent from process()
	    would be dropped by the host's connection proxy. */
	void sendSampleRateToController ();
	void sendSlotStateToController ();

	Vocoder mVocoder;

	// Normalised parameter values: where the block starts and where it
	// ends. Only the smoothed ones actually ramp between the two; the rest
	// take the target, as the DXi's per-block read did.
	std::vector<double> mParams;
	std::vector<double> mParamTargets;
	bool   mBypass = false;

	std::string mSlotPath[Vocoder::kNumSlots];
	std::string mSlotError[Vocoder::kNumSlots];

	double mSampleRate = 44100.0;

	// 64-bit processing goes through these. The DXi's line was 32-bit
	// float and IsValidInputFormat rejected everything else; the line
	// stays float here, but a 64-bit host is accepted rather than refused.
	std::vector<float> mScratchIn[2];
	std::vector<float> mScratchOut[2];
};

//------------------------------------------------------------------------
} // namespace SpyBand
