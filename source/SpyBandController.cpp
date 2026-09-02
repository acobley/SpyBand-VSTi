//------------------------------------------------------------------------
// SpyBand - edit controller implementation
//------------------------------------------------------------------------

#include "SpyBandController.h"
#include "SpyBandEditor.h"
#include "SpyBandIDs.h"

#include "base/source/fstreamer.h"
#include "pluginterfaces/base/ustring.h"
#include "pluginterfaces/vst/ivstmessage.h"
#include "public.sdk/source/vst/vstparameters.h"

#include <algorithm>
#include <cstring>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace SpyBand {

namespace {

constexpr int32 kMaxPathBytes = 8192;

/** Add one StringListParameter from a list of names.

    NEVER pass a null title or units to a parameter: the constructor hands
    both to UString::assign, which reads src[0] with no null check, and the
    crash surfaces as the VALIDATOR segfaulting during the post-build step
    - a long way from anything that looks parameter-shaped. */
void addList (ParameterContainer& container, ParamID id, const char* title,
              const char* const* names, int count, double defaultNormalized)
{
	auto* p = new StringListParameter (USTRING (title), id, USTRING (""),
	                                   ParameterInfo::kCanAutomate | ParameterInfo::kIsList);
	for (int i = 0; i < count; ++i)
		p->appendString (USTRING (names[i]));

	// The strings must exist before either of these: a StringListParameter
	// takes its step count from how many it holds, so a default set on an
	// empty one is quantised against a step count of zero.
	p->getInfo ().defaultNormalizedValue = defaultNormalized;
	p->setNormalized (defaultNormalized);
	container.addParameter (p);
}

const char* const kOffOn[2] = { "Off", "On" };

} // namespace

//------------------------------------------------------------------------
const std::string& SpyBandController::slotPath (int slot) const
{
	static const std::string empty;
	if (slot < 0 || slot >= Vocoder::kNumSlots)
		return empty;
	return mSlotPath[slot];
}

bool SpyBandController::slotLoaded (int slot) const
{
	if (slot < 0 || slot >= Vocoder::kNumSlots)
		return false;
	return mSlotLoaded[slot];
}

//------------------------------------------------------------------------
// The DXi's CSpyBand::GetFileName: strip the directory, strip the
// extension, and fall back to "File n" when the slot is empty. The
// separator is '/' here rather than '\\'.
//------------------------------------------------------------------------
std::string SpyBandController::slotLabel (int slot) const
{
	if (slot < 0 || slot >= Vocoder::kNumSlots)
		return std::string ();

	const std::string& path = mSlotPath[slot];
	if (path.empty ())
		return std::string ("File ") + static_cast<char> ('1' + slot);

	std::size_t start = path.find_last_of ("/\\");
	start = (start == std::string::npos) ? 0 : start + 1;

	std::size_t stop = path.find_last_of ('.');
	if (stop == std::string::npos || stop < start)
		stop = path.size ();

	return path.substr (start, stop - start);
}

//------------------------------------------------------------------------
void SpyBandController::addParameters ()
{
	//--------------------------------------------------------------------
	// The scalar parameters, in the DXi's own order.
	//--------------------------------------------------------------------
	for (ParamID id = 0; id < kNumScalarParams; ++id)
	{
		const ParamDef& def = kParams[id];

		if (def.type == ParamType::Bool)
		{
			// Five of the switches were labelled in the dialog with a PAIR
			// OF WORDS rather than a tick - "Use Sample" against
			// "Interlace" - and the words say more than On would. The host
			// gets the same words the panel does.
			const char* const* names = booleanNames (id);
			addList (parameters, id, def.title, names ? names : kOffOn, 2,
			         def.defaultNormalized ());
			continue;
		}

		if (def.type == ParamType::Enum)
		{
			const char* const* names =
				(id == kBands) ? kBandsNames : kFilterSlopeNames;
			addList (parameters, id, def.title, names, def.stepCount + 1,
			         def.defaultNormalized ());
			continue;
		}

		// Everything else is a plain range. RangeParameter renders the
		// number and appends the units itself, and reads both back, so
		// there is no custom toString to disagree with a custom
		// fromString - and nothing for the validator's round-trip check
		// to catch.
		auto* p = new RangeParameter (USTRING (def.title), id, USTRING (def.units),
		                              def.plainMin, def.plainMax, def.plainDefault,
		                              def.stepCount, ParameterInfo::kCanAutomate);
		parameters.addParameter (p);
	}

	//--------------------------------------------------------------------
	// The patch matrix.
	//
	// 484 of them, appended after every DXi parameter so that no existing
	// id moves. They are hidden from the host's generic list - a mixer
	// strip with 484 identical percentage sliders in it helps nobody - but
	// they automate, save and load like any other parameter.
	//--------------------------------------------------------------------
	for (int row = 0; row < kMaxBands; ++row)
	{
		for (int column = 0; column < kMaxBands; ++column)
		{
			const ParamID id = patchParam (row, column);

			char title[32];
			patchTitle (id, title, sizeof (title));

			auto* p = new RangeParameter (
				USTRING (title), id, USTRING (kPatchDef.units),
				kPatchDef.plainMin, kPatchDef.plainMax,
				kPatchDef.toPlain (patchDefaultNormalized (row, column)),
				kPatchDef.stepCount,
				ParameterInfo::kCanAutomate | ParameterInfo::kIsHidden);
			parameters.addParameter (p);
		}
	}

	//--------------------------------------------------------------------
	// Bypass. VST3 hosts expect one; the DXi had none, and its own
	// "Enabled" parameter is kept separately with its original default
	// because it is a different switch that happens to do the same thing.
	//--------------------------------------------------------------------
	auto* bypass = new RangeParameter (USTRING ("Bypass"), kBypass, USTRING (""),
	                                   0, 1, 0, 1,
	                                   ParameterInfo::kCanAutomate | ParameterInfo::kIsBypass);
	parameters.addParameter (bypass);
}

//------------------------------------------------------------------------
tresult PLUGIN_API SpyBandController::initialize (FUnknown* context)
{
	const tresult result = EditControllerEx1::initialize (context);
	if (result != kResultOk)
		return result;

	addParameters ();
	return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API SpyBandController::terminate ()
{
	return EditControllerEx1::terminate ();
}

//------------------------------------------------------------------------
// Reads exactly what SpyBandProcessor::getState wrote. A short stream
// leaves the rest at its default, for the same reason the processor does.
//------------------------------------------------------------------------
tresult PLUGIN_API SpyBandController::setComponentState (IBStream* state)
{
	if (state == nullptr)
		return kResultFalse;

	IBStreamer streamer (state, kLittleEndian);

	int32 version = 0;
	if (! streamer.readInt32 (version))
		return kResultFalse;

	int32 count = 0;
	if (! streamer.readInt32 (count))
		return kResultFalse;
	if (count < 0 || count > static_cast<int32> (kNumParams))
		return kResultFalse;

	for (ParamID id = 0; id < kNumScalarParams; ++id)
		setParamNormalized (id, kParams[id].defaultNormalized ());
	for (int row = 0; row < kMaxBands; ++row)
		for (int column = 0; column < kMaxBands; ++column)
			setParamNormalized (patchParam (row, column),
			                    patchDefaultNormalized (row, column));

	for (int32 i = 0; i < count; ++i)
	{
		double value = 0.0;
		if (! streamer.readDouble (value))
			return kResultFalse;
		setParamNormalized (static_cast<ParamID> (i), value);
	}

	int32 bypass = 0;
	if (! streamer.readInt32 (bypass))
		return kResultFalse;
	setParamNormalized (kBypass, bypass ? 1.0 : 0.0);

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
			mSlotPath[static_cast<std::size_t> (s)] = path;
	}

	for (int32 s = slots; s < Vocoder::kNumSlots; ++s)
		mSlotPath[static_cast<std::size_t> (s)].clear ();

	return kResultOk;
}

//------------------------------------------------------------------------
void SpyBandController::requestSlotLoad (int slot, const std::string& path)
{
	if (slot < 0 || slot >= Vocoder::kNumSlots)
		return;

	mSlotPath[slot] = path;

	if (auto* message = allocateMessage ())
	{
		FReleaser releaser (message);
		message->setMessageID (kSpyBandLoadSampleMessage);
		message->getAttributes ()->setInt (kSpyBandSlotAttribute, slot);
		message->getAttributes ()->setBinary (kSpyBandPathAttribute,
		                                      path.data (),
		                                      static_cast<uint32> (path.size ()));
		sendMessage (message);
	}
}

//------------------------------------------------------------------------
void SpyBandController::requestMeter ()
{
	if (auto* message = allocateMessage ())
	{
		FReleaser releaser (message);
		message->setMessageID (kSpyBandMeterRequestMessage);
		sendMessage (message);
	}
}

//------------------------------------------------------------------------
int SpyBandController::meter (double* out) const
{
	if (out == nullptr)
		return 0;
	for (int i = 0; i < mMeterCount; ++i)
		out[i] = mMeter[i];
	return mMeterCount;
}

//------------------------------------------------------------------------
tresult PLUGIN_API SpyBandController::notify (IMessage* message)
{
	if (message == nullptr)
		return kInvalidArgument;

	if (std::strcmp (message->getMessageID (), kSpyBandSampleRateMessage) == 0)
	{
		double rate = 0.0;
		if (message->getAttributes ()->getFloat (kSpyBandSampleRateAttribute, rate) == kResultOk
		    && rate > 0.0)
		{
			mSampleRate = rate;
		}
		return kResultOk;
	}

	if (std::strcmp (message->getMessageID (), kSpyBandMeterDataMessage) == 0)
	{
		const void* raw = nullptr;
		uint32 size = 0;
		if (message->getAttributes ()->getBinary (kSpyBandMeterAttribute, raw, size) == kResultOk
		    && raw != nullptr && size >= 2 * sizeof (double))
		{
			const double* frame = static_cast<const double*> (raw);
			mVoiced = (frame[0] >= 0.5);
			int count = static_cast<int> (frame[1]);
			const int available = static_cast<int> (size / sizeof (double)) - 2;
			if (count > available)
				count = available;
			if (count > 2 * kMaxBands)
				count = 2 * kMaxBands;
			if (count < 0)
				count = 0;
			for (int i = 0; i < count; ++i)
				mMeter[i] = frame[2 + i];
			mMeterCount = count;
		}
		return kResultOk;
	}

	if (std::strcmp (message->getMessageID (), kSpyBandSlotStateMessage) == 0)
	{
		const void* raw = nullptr;
		uint32 size = 0;
		if (message->getAttributes ()->getBinary (kSpyBandSlotStateAttribute, raw, size) == kResultOk
		    && raw != nullptr)
		{
			const char* p = static_cast<const char*> (raw);
			const char* end = p + size;
			for (int s = 0; s < Vocoder::kNumSlots && p < end; ++s)
			{
				mSlotLoaded[s] = (*p++ != '\0');
				const char* stop = p;
				while (stop < end && *stop != '\0')
					++stop;
				mSlotPath[s].assign (p, static_cast<std::size_t> (stop - p));
				p = (stop < end) ? stop + 1 : end;
			}
		}
		return kResultOk;
	}

	return EditControllerEx1::notify (message);
}

//------------------------------------------------------------------------
IPlugView* PLUGIN_API SpyBandController::createView (FIDString name)
{
	if (name && FIDStringsEqual (name, ViewType::kEditor))
		return new SpyBandEditor (this);
	return nullptr;
}

//------------------------------------------------------------------------
tresult PLUGIN_API SpyBandController::setParamNormalized (ParamID tag, ParamValue value)
{
	const tresult result = EditControllerEx1::setParamNormalized (tag, value);
	if (result != kResultOk)
		return result;

	for (auto* editor : mEditors)
		editor->updateControl (tag, value);

	return result;
}

//------------------------------------------------------------------------
void SpyBandController::editorAttached (EditorView* editor)
{
	if (auto* e = dynamic_cast<SpyBandEditor*> (editor))
		if (std::find (mEditors.begin (), mEditors.end (), e) == mEditors.end ())
			mEditors.push_back (e);
}

//------------------------------------------------------------------------
void SpyBandController::editorRemoved (EditorView* editor)
{
	editorDestroyed (editor);
}

//------------------------------------------------------------------------
void SpyBandController::editorDestroyed (EditorView* editor)
{
	// Do NOT dynamic_cast here. EditorView::~EditorView() is one of the two
	// callers, and by then the SpyBandEditor sub-object is gone, so the cast
	// yields null and the entry survives as a dangling pointer - which the
	// next setParamNormalized above would then walk. Comparing upcast
	// pointers is well defined at every point in the destruction sequence.
	// See PORTING-GUIDE section 7; this one cost real time on SpaceDub.
	mEditors.erase (std::remove_if (mEditors.begin (), mEditors.end (),
	                                [editor] (SpyBandEditor* e) {
		                                return static_cast<EditorView*> (e) == editor;
	                                }),
	                mEditors.end ());
}

//------------------------------------------------------------------------
} // namespace SpyBand
