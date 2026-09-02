//------------------------------------------------------------------------
// SpyBand - audio processor implementation
//------------------------------------------------------------------------

#include "SpyBandProcessor.h"
#include "SpyBandIDs.h"
#include "WavFile.h"

#include "base/source/fstreamer.h"
#include "pluginterfaces/vst/ivstmessage.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"

#include <algorithm>
#include <cmath>
#include <cstring>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace SpyBand {

namespace {

/** Bumped only if the layout below changes incompatibly. */
constexpr int32 kStateVersion = 1;

/** A file path longer than this is refused rather than trusted - the
    length comes off a stream a host handed us. */
constexpr int32 kMaxPathBytes = 8192;

} // namespace

//------------------------------------------------------------------------
SpyBandProcessor::SpyBandProcessor ()
{
	setControllerClass (kSpyBandControllerUID);

	// NO processContextRequirements.
	//
	// Since VST3 3.7 the ProcessContext is opt-in, and the trap is asking
	// for nothing when you need something. SpyBand needs nothing: unlike
	// SpaceDub, which used the DXi synth interface to reach the host's
	// tempo map, this plug-in never read the tempo, the transport state or
	// the musical position. Nothing here is sync'd, so nothing is
	// requested - which is what the interface is for.

	mParams.assign (kNumParams, 0.0);
	mParamTargets.assign (kNumParams, 0.0);

	for (ParamID id = 0; id < kNumScalarParams; ++id)
		mParams[id] = kParams[id].defaultNormalized ();

	for (int row = 0; row < kMaxBands; ++row)
		for (int column = 0; column < kMaxBands; ++column)
			mParams[patchParam (row, column)] = patchDefaultNormalized (row, column);

	mParamTargets = mParams;
}

//------------------------------------------------------------------------
tresult PLUGIN_API SpyBandProcessor::initialize (FUnknown* context)
{
	const tresult result = AudioEffect::initialize (context);
	if (result != kResultOk)
		return result;

	addAudioInput (STR16 ("Stereo In"), SpeakerArr::kStereo);
	addAudioOutput (STR16 ("Stereo Out"), SpeakerArr::kStereo);

	// No event input. The DXi registered as a soft synth and CSpyBand had
	// the three note-event overrides the wizard generates, but all three
	// are the wizard's stubs and the processing loop never looks at the
	// MIDI queue - it reads its input buffer. This is an effect.

	return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API SpyBandProcessor::terminate ()
{
	return AudioEffect::terminate ();
}

//------------------------------------------------------------------------
tresult PLUGIN_API SpyBandProcessor::setActive (TBool state)
{
	if (state)
	{
		mVocoder.reset ();
		sendSampleRateToController ();
		sendSlotStateToController ();
	}
	else
	{
		// The audio thread is stopped, so the buffers retired by a load
		// can finally go.
		mVocoder.releaseRetiredSamples ();
	}
	return AudioEffect::setActive (state);
}

//------------------------------------------------------------------------
tresult PLUGIN_API SpyBandProcessor::setupProcessing (ProcessSetup& setup)
{
	mSampleRate = setup.sampleRate;

	// The DXi recomputed its filter constants from inside Process and
	// allocated its noise buffer in Initialize; both happen here instead,
	// so the audio thread allocates nothing.
	mVocoder.setSampleRate (mSampleRate);

	const std::size_t maxFrames =
		static_cast<std::size_t> (std::max<int32> (setup.maxSamplesPerBlock, 1));
	for (int c = 0; c < 2; ++c)
	{
		mScratchIn[c].assign (maxFrames, 0.0f);
		mScratchOut[c].assign (maxFrames, 0.0f);
	}

	return AudioEffect::setupProcessing (setup);
}

//------------------------------------------------------------------------
tresult PLUGIN_API SpyBandProcessor::canProcessSampleSize (int32 symbolicSampleSize)
{
	if (symbolicSampleSize == kSample32 || symbolicSampleSize == kSample64)
		return kResultTrue;
	return kResultFalse;
}

//------------------------------------------------------------------------
tresult PLUGIN_API SpyBandProcessor::setBusArrangements (SpeakerArrangement* inputs, int32 numIns,
                                                         SpeakerArrangement* outputs, int32 numOuts)
{
	if (numIns == 1 && numOuts == 1)
	{
		// Stereo is the native format. Mono is accepted and folded, which
		// the DXi did not offer - IsValidInputFormat took two channels or
		// nothing. Whatever is accepted here must match the
		// SupportedNumChannels list in resource/au-info.plist, or auval
		// and the AU will disagree with the VST3.
		if ((inputs[0] == SpeakerArr::kStereo && outputs[0] == SpeakerArr::kStereo)
		    || (inputs[0] == SpeakerArr::kMono && outputs[0] == SpeakerArr::kMono))
		{
			return AudioEffect::setBusArrangements (inputs, numIns, outputs, numOuts);
		}
	}
	return kResultFalse;
}

//------------------------------------------------------------------------
uint32 PLUGIN_API SpyBandProcessor::getTailSamples ()
{
	// The bands are resonant and the envelope followers have a release of
	// up to 101 ms, so the line takes a moment to fall silent after the
	// input stops. Half a second is comfortably past audibility and costs
	// a host nothing but a little extra rendering at the end of a bounce.
	return static_cast<uint32> (mSampleRate * 0.5);
}

//------------------------------------------------------------------------
// Parameter changes.
//
// VST3 offers sample-accurate automation; the DXi read every parameter
// once per block. The value at the END of the block is taken as the
// target and the gains ramp to it inside the vocoder, which is the
// guide's recommendation and closer to the DXi than stepping would be.
//------------------------------------------------------------------------
void SpyBandProcessor::applyParameterChanges (IParameterChanges* changes)
{
	if (changes == nullptr)
		return;

	const int32 count = changes->getParameterCount ();
	for (int32 i = 0; i < count; ++i)
	{
		IParamValueQueue* queue = changes->getParameterData (i);
		if (queue == nullptr)
			continue;

		const int32 points = queue->getPointCount ();
		if (points <= 0)
			continue;

		int32 offset = 0;
		ParamValue value = 0.0;
		if (queue->getPoint (points - 1, offset, value) != kResultTrue)
			continue;

		const ParamID id = queue->getParameterId ();

		// RANGE-CHECK: kBypass is 1000, far past the end of the table.
		if (id == kBypass)
		{
			mBypass = (value >= 0.5);
			continue;
		}
		if (id < kNumParams)
			mParamTargets[id] = value;
	}
}

//------------------------------------------------------------------------
void SpyBandProcessor::buildVocoderParams (Vocoder::Params& p) const
{
	const auto internalOf = [this] (ParamID id)
	{
		return paramDef (id).toInternal (mParamTargets[id]);
	};

	p.inLevel          = internalOf (kInLevel);
	p.sampLevel        = internalOf (kSampLevel);
	p.sampThroughLevel = internalOf (kSampThroughLevel);
	p.resonance        = internalOf (kResonance);
	p.freq             = internalOf (kFreq);
	p.freqSpread       = internalOf (kFreqSpread);
	p.attack           = internalOf (kAttack);
	p.release          = internalOf (kRelease);
	p.envBoost         = internalOf (kEnvBoost);
	p.voicedSense      = internalOf (kVoicedSense);
	p.noiseLevel       = internalOf (kNoiseLevel);
	p.noiseHighFreq    = internalOf (kNoiseHighFreq);
	p.outputTrim       = internalOf (kOutputTrim);

	p.enable        = (internalOf (kEnable) >= 0.5) && ! mBypass;
	p.stereo        = internalOf (kStereo) >= 0.5;
	p.interlaced    = internalOf (kInterlaced) >= 0.5;
	p.repeatSamp    = internalOf (kRepeatSamp) >= 0.5;
	p.voicedDetect  = internalOf (kVoicedDetect) >= 0.5;
	p.noiseOverride = internalOf (kNoiseOverride) >= 0.5;

	p.sampleEnabled[0] = internalOf (kSample1) >= 0.5;
	p.sampleEnabled[1] = internalOf (kSample2) >= 0.5;
	p.sampleEnabled[2] = internalOf (kSample3) >= 0.5;
	p.sampleEnabled[3] = internalOf (kSample4) >= 0.5;
	p.sampleEnabled[4] = internalOf (kSample5) >= 0.5;

	p.bands        = bandCount (internalOf (kBands));
	p.filterSlopes = static_cast<int> (internalOf (kFilterSlopes));

	for (int row = 0; row < kMaxBands; ++row)
	{
		for (int column = 0; column < kMaxBands; ++column)
		{
			const ParamID id = patchParam (row, column);
			p.patch[row * kMaxBands + column] = kPatchDef.toInternal (mParamTargets[id]);
		}
	}
}

//------------------------------------------------------------------------
tresult PLUGIN_API SpyBandProcessor::process (ProcessData& data)
{
	applyParameterChanges (data.inputParameterChanges);

	if (data.numSamples <= 0 || data.numOutputs <= 0 || data.outputs == nullptr)
	{
		mParams = mParamTargets;
		return kResultOk;
	}

	const int32 frames = data.numSamples;
	const int32 outChannels = data.outputs[0].numChannels;
	const int32 inChannels =
		(data.numInputs > 0 && data.inputs != nullptr) ? data.inputs[0].numChannels : 0;

	if (outChannels <= 0)
	{
		mParams = mParamTargets;
		return kResultOk;
	}

	Vocoder::Params params;
	buildVocoderParams (params);

	if (data.symbolicSampleSize == kSample32)
	{
		const float* in[2] = { nullptr, nullptr };
		for (int32 c = 0; c < inChannels && c < 2; ++c)
			in[c] = data.inputs[0].channelBuffers32[c];

		float* out[2] = { nullptr, nullptr };
		for (int32 c = 0; c < outChannels && c < 2; ++c)
			out[c] = data.outputs[0].channelBuffers32[c];

		mVocoder.process (params, (inChannels > 0) ? in : nullptr,
		                  std::min<int32> (inChannels, 2),
		                  out, std::min<int32> (outChannels, 2), frames);
	}
	else
	{
		// 64-bit host, 32-bit line. See the note on mScratchIn.
		const std::size_t need = static_cast<std::size_t> (frames);
		for (int c = 0; c < 2; ++c)
		{
			if (mScratchIn[c].size () < need)
				mScratchIn[c].resize (need);
			if (mScratchOut[c].size () < need)
				mScratchOut[c].resize (need);
		}

		const float* in[2] = { nullptr, nullptr };
		for (int32 c = 0; c < inChannels && c < 2; ++c)
		{
			const double* src = data.inputs[0].channelBuffers64[c];
			for (int32 f = 0; f < frames; ++f)
				mScratchIn[c][static_cast<std::size_t> (f)] = static_cast<float> (src[f]);
			in[c] = mScratchIn[c].data ();
		}

		float* out[2] = { nullptr, nullptr };
		for (int32 c = 0; c < outChannels && c < 2; ++c)
			out[c] = mScratchOut[c].data ();

		mVocoder.process (params, (inChannels > 0) ? in : nullptr,
		                  std::min<int32> (inChannels, 2),
		                  out, std::min<int32> (outChannels, 2), frames);

		for (int32 c = 0; c < outChannels && c < 2; ++c)
		{
			double* dst = data.outputs[0].channelBuffers64[c];
			for (int32 f = 0; f < frames; ++f)
				dst[f] = mScratchOut[c][static_cast<std::size_t> (f)];
		}
	}

	// Anything the host did not offer a buffer for stays silent.
	for (int32 c = 2; c < outChannels; ++c)
	{
		if (data.symbolicSampleSize == kSample32)
			std::memset (data.outputs[0].channelBuffers32[c], 0,
			             sizeof (float) * static_cast<std::size_t> (frames));
		else
			std::memset (data.outputs[0].channelBuffers64[c], 0,
			             sizeof (double) * static_cast<std::size_t> (frames));
	}

	data.outputs[0].silenceFlags = 0;
	mParams = mParamTargets;
	return kResultOk;
}

//------------------------------------------------------------------------
// Sample slots
//------------------------------------------------------------------------
void SpyBandProcessor::loadSlot (int slot, const std::string& path)
{
	if (slot < 0 || slot >= Vocoder::kNumSlots)
		return;

	mSlotPath[slot] = path;
	mSlotError[slot].clear ();

	if (path.empty ())
	{
		mVocoder.setSample (slot, nullptr);
		return;
	}

	std::string error;
	std::shared_ptr<const WavData> data = loadWav (path, error);
	if (! data)
	{
		// The path is KEPT even when the load fails, so that saving the
		// project again does not quietly forget which file it wanted. The
		// DXi had no equivalent: exLoadFile stored the name whatever
		// LoadFile did, and LoadFile fell off its end without a return
		// value when the file was missing.
		mSlotError[slot] = error;
		mVocoder.setSample (slot, nullptr);
		return;
	}

	mVocoder.setSample (slot, std::move (data));
}

//------------------------------------------------------------------------
tresult PLUGIN_API SpyBandProcessor::notify (IMessage* message)
{
	if (message == nullptr)
		return kInvalidArgument;

	if (std::strcmp (message->getMessageID (), kSpyBandLoadSampleMessage) == 0)
	{
		int64 slot = -1;
		if (message->getAttributes ()->getInt (kSpyBandSlotAttribute, slot) != kResultOk)
			return kResultFalse;

		const void* raw = nullptr;
		uint32 size = 0;
		std::string path;
		if (message->getAttributes ()->getBinary (kSpyBandPathAttribute, raw, size) == kResultOk
		    && raw != nullptr && size > 0 && size <= static_cast<uint32> (kMaxPathBytes))
		{
			path.assign (static_cast<const char*> (raw), size);
		}

		loadSlot (static_cast<int> (slot), path);
		sendSlotStateToController ();
		return kResultOk;
	}

	if (std::strcmp (message->getMessageID (), kSpyBandMeterRequestMessage) == 0)
	{
		if (auto* reply = allocateMessage ())
		{
			FReleaser releaser (reply);
			reply->setMessageID (kSpyBandMeterDataMessage);

			// [0] is the voiced lamp, [1] the count, then the bands.
			double frame[2 + 2 * kMaxBands] = { 0.0 };
			const int count = mVocoder.meter (frame + 2);
			frame[0] = mVocoder.voicedState () ? 1.0 : 0.0;
			frame[1] = static_cast<double> (count);

			reply->getAttributes ()->setBinary (
				kSpyBandMeterAttribute, frame,
				static_cast<uint32> (sizeof (double) * (2 + count)));
			sendMessage (reply);
		}
		return kResultOk;
	}

	if (std::strcmp (message->getMessageID (), kSpyBandSlotStateMessage) == 0)
	{
		// The controller asking to be told again, after an editor opens.
		sendSampleRateToController ();
		sendSlotStateToController ();
		return kResultOk;
	}

	return AudioEffect::notify (message);
}

//------------------------------------------------------------------------
void SpyBandProcessor::sendSampleRateToController ()
{
	if (auto* message = allocateMessage ())
	{
		FReleaser releaser (message);
		message->setMessageID (kSpyBandSampleRateMessage);
		message->getAttributes ()->setFloat (kSpyBandSampleRateAttribute, mSampleRate);
		sendMessage (message);
	}
}

//------------------------------------------------------------------------
void SpyBandProcessor::sendSlotStateToController ()
{
	if (auto* message = allocateMessage ())
	{
		FReleaser releaser (message);
		message->setMessageID (kSpyBandSlotStateMessage);

		// One flat buffer: for each slot, a byte saying whether it loaded,
		// then the path, then a NUL. Simpler to walk than five attributes
		// and it keeps the slots in step with each other.
		std::string blob;
		for (int s = 0; s < Vocoder::kNumSlots; ++s)
		{
			blob.push_back (mVocoder.sample (s) ? '\1' : '\0');
			blob.append (mSlotPath[s]);
			blob.push_back ('\0');
		}

		message->getAttributes ()->setBinary (kSpyBandSlotStateAttribute,
		                                      blob.data (),
		                                      static_cast<uint32> (blob.size ()));
		sendMessage (message);
	}
}

//------------------------------------------------------------------------
// State
//
// The DXi's stream cannot be read: PersistSave wrote a DWORD count then
// (index, float) pairs in the DXi's EXTERNAL ranges, followed by five
// lengths and five ANSI paths - and the ranges, the parameter set and the
// path syntax have all changed. Old projects do not carry over, which was
// decided early rather than discovered late.
//
// A short stream - one written by an earlier version with fewer
// parameters - leaves everything it does not mention at its DEFAULT. Not
// doing that is how loading an old project after a new one inherits the
// new one's settings.
//------------------------------------------------------------------------
tresult PLUGIN_API SpyBandProcessor::setState (IBStream* state)
{
	if (state == nullptr)
		return kResultFalse;

	IBStreamer streamer (state, kLittleEndian);

	int32 version = 0;
	if (! streamer.readInt32 (version))
		return kResultFalse;
	if (version > kStateVersion)
		return kResultFalse;

	int32 count = 0;
	if (! streamer.readInt32 (count))
		return kResultFalse;
	if (count < 0 || count > static_cast<int32> (kNumParams))
		return kResultFalse;

	// Start from the defaults, so a short stream cannot inherit whatever
	// was loaded before it.
	for (ParamID id = 0; id < kNumScalarParams; ++id)
		mParamTargets[id] = kParams[id].defaultNormalized ();
	for (int row = 0; row < kMaxBands; ++row)
		for (int column = 0; column < kMaxBands; ++column)
			mParamTargets[patchParam (row, column)] = patchDefaultNormalized (row, column);

	for (int32 i = 0; i < count; ++i)
	{
		double value = 0.0;
		if (! streamer.readDouble (value))
			return kResultFalse;
		mParamTargets[static_cast<ParamID> (i)] = value;
	}

	int32 bypass = 0;
	if (! streamer.readInt32 (bypass))
		return kResultFalse;
	mBypass = (bypass != 0);

	int32 slots = 0;
	if (! streamer.readInt32 (slots))
		return kResultFalse;

	for (int32 s = 0; s < slots; ++s)
	{
		int32 length = 0;
		if (! streamer.readInt32 (length))
			return kResultFalse;
		if (length < 0 || length > kMaxPathBytes)
			return kResultFalse;

		std::string path;
		if (length > 0)
		{
			path.resize (static_cast<std::size_t> (length));
			if (streamer.readRaw (&path[0], length) != length)
				return kResultFalse;
		}

		if (s < Vocoder::kNumSlots)
			loadSlot (static_cast<int> (s), path);
	}

	// Any slot the stream did not mention is emptied, for the same reason
	// the parameters go back to their defaults.
	for (int32 s = slots; s < Vocoder::kNumSlots; ++s)
		loadSlot (static_cast<int> (s), std::string ());

	mParams = mParamTargets;

	// setState is documented [UI-thread], so these are legal here.
	sendSampleRateToController ();
	sendSlotStateToController ();

	return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API SpyBandProcessor::getState (IBStream* state)
{
	if (state == nullptr)
		return kResultFalse;

	IBStreamer streamer (state, kLittleEndian);

	streamer.writeInt32 (kStateVersion);
	streamer.writeInt32 (static_cast<int32> (kNumParams));

	for (ParamID id = 0; id < kNumParams; ++id)
		streamer.writeDouble (mParamTargets[id]);

	streamer.writeInt32 (mBypass ? 1 : 0);
	streamer.writeInt32 (static_cast<int32> (Vocoder::kNumSlots));

	for (int s = 0; s < Vocoder::kNumSlots; ++s)
	{
		const int32 length = static_cast<int32> (mSlotPath[s].size ());
		streamer.writeInt32 (length);
		if (length > 0)
			streamer.writeRaw (mSlotPath[s].data (), length);
	}

	return kResultOk;
}

//------------------------------------------------------------------------
} // namespace SpyBand
