//------------------------------------------------------------------------
// SpyBand - edit controller implementation
//------------------------------------------------------------------------

#include "SpyBandController.h"
#include "SpyBandEditor.h"
#include "SpyBandIDs.h"
#include "WavFile.h"
#include "version.h"

#include "base/source/fstreamer.h"
#include "pluginterfaces/base/ustring.h"
#include "pluginterfaces/vst/ivstmessage.h"
#include "public.sdk/source/vst/vstparameters.h"
#include "public.sdk/source/vst/vstpresetfile.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

#if defined(_WIN32)
#include <direct.h>
#include <io.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#endif

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
// The waveform overviews on the sample slots.
//
// The controller does not have the audio - the processor does - so it
// reads the file again, once, when a slot's path changes. It is the same
// reader the processor uses, so the picture is of what is playing.
//------------------------------------------------------------------------
const std::vector<float>& SpyBandController::slotOverview (int slot)
{
	static const std::vector<float> none;
	if (slot < 0 || slot >= Vocoder::kNumSlots)
		return none;

	if (! mSlotLoaded[slot] || mSlotPath[slot].empty ())
	{
		mOverview[slot].clear ();
		mOverviewPath[slot].clear ();
		return mOverview[slot];
	}

	const std::string path = mSlotPath[slot];

	if (path == mOverviewPath[slot])
		return mOverview[slot];

	mOverviewPath[slot] = path;
	mOverview[slot].clear ();

	std::string error;
	auto data = loadWav (path, error);
	if (! data || data->frameCount () <= 0)
		return mOverview[slot];

	constexpr int kColumns = 160;
	const long frames = data->frameCount ();
	std::vector<float> peaks (kColumns, 0.f);
	float loudest = 0.f;
	for (int c = 0; c < kColumns; ++c)
	{
		const long from = frames * c / kColumns;
		const long to = std::max (from + 1, frames * (c + 1) / kColumns);
		float peak = 0.f;
		for (long f = from; f < to && f < frames; ++f)
		{
			peak = std::max (peak, std::fabs (data->samples[static_cast<std::size_t> (2 * f)]));
			peak = std::max (peak, std::fabs (data->samples[static_cast<std::size_t> (2 * f + 1)]));
		}
		peaks[static_cast<std::size_t> (c)] = peak;
		loudest = std::max (loudest, peak);
	}
	if (loudest > 0.f)
		for (float& p : peaks)
			p /= loudest;

	mOverview[slot] = std::move (peaks);
	return mOverview[slot];
}

//------------------------------------------------------------------------
// Presets
//------------------------------------------------------------------------
Patch SpyBandController::currentPatch ()
{
	Patch p = Patch::defaults ();
	for (ParamID id = 0; id < kNumParams; ++id)
		p.values[id] = getParamNormalized (id);
	p.bypass = getParamNormalized (kBypass) >= 0.5;
	for (int s = 0; s < Vocoder::kNumSlots; ++s)
		p.paths[s] = mSlotPath[s];
	return p;
}

void SpyBandController::setEdited (ParamID id, double value)
{
	if (std::fabs (getParamNormalized (id) - value) < 1e-9)
		return;
	beginEdit (id);
	setParamNormalized (id, value);
	performEdit (id, value);
	endEdit (id);
}

void SpyBandController::applyPatch (const Patch& patch, bool withSamples)
{
	mApplyingPatch = true;
	// One undo step in hosts that support it, not five hundred.
	startGroupEdit ();
	for (ParamID id = 0; id < kNumParams; ++id)
	{
		if (! withSamples && keepsSamples (id))
			continue;
		setEdited (id, patch.values[id]);
	}

	if (withSamples)
		for (int s = 0; s < Vocoder::kNumSlots; ++s)
			if (patch.paths[s] != mSlotPath[s])
				requestSlotLoad (s, patch.paths[s]);
	finishGroupEdit ();
	mApplyingPatch = false;
	mPresetModified = false;
}

void SpyBandController::loadFactoryPreset (int index)
{
	if (index < 0 || index >= factoryPresetCount ())
		return;
	applyPatch (factoryPreset (index), false);
	mPresetBank = "FACTORY";
	mPresetName = factoryPresetName (index);
	mFactoryIndex = index;
	mPresetModified = false;
}

namespace {

#if defined(_WIN32)
constexpr char kSeparator = '\\';
#else
constexpr char kSeparator = '/';
#endif

void makeFolders (const std::string& path)
{
	for (std::size_t i = 1; i <= path.size (); ++i)
	{
		if (i == path.size () || path[i] == '/' || path[i] == '\\')
		{
			const std::string part = path.substr (0, i);
#if defined(_WIN32)
			_mkdir (part.c_str ());
#else
			mkdir (part.c_str (), 0755);
#endif
		}
	}
}

std::string fileStem (const std::string& path)
{
	std::size_t start = path.find_last_of ("/\\");
	start = (start == std::string::npos) ? 0 : start + 1;
	std::size_t stop = path.find_last_of ('.');
	if (stop == std::string::npos || stop < start)
		stop = path.size ();
	return path.substr (start, stop - start);
}

/** The controller's own chunk: a tag and the preset name, so a project
    reopens showing the preset it was saved with. */
constexpr int32 kControllerStateTag = 0x53504231;   // "SPB1"

} // namespace

std::string SpyBandController::userPresetFolder () const
{
	std::string base;
#if defined(_WIN32)
	if (const char* home = std::getenv ("USERPROFILE"))
		base = std::string (home) + "\\Documents\\VST3 Presets";
#elif defined(__APPLE__)
	if (const char* home = std::getenv ("HOME"))
		base = std::string (home) + "/Library/Audio/Presets";
#else
	if (const char* home = std::getenv ("HOME"))
		base = std::string (home) + "/.vst3/presets";
#endif
	if (base.empty ())
		return std::string ();

	const std::string folder = base + kSeparator + stringCompanyName + kSeparator + "SpyBand";
	makeFolders (folder);
	return folder;
}

std::vector<std::string> SpyBandController::userPresets () const
{
	std::vector<std::string> out;
	const std::string folder = userPresetFolder ();
	if (folder.empty ())
		return out;

#if defined(_WIN32)
	_finddata_t found;
	const intptr_t handle = _findfirst ((folder + "\\*.vstpreset").c_str (), &found);
	if (handle != -1)
	{
		do
			out.push_back (folder + "\\" + found.name);
		while (_findnext (handle, &found) == 0);
		_findclose (handle);
	}
#else
	if (DIR* dir = opendir (folder.c_str ()))
	{
		while (dirent* entry = readdir (dir))
		{
			const std::string name = entry->d_name;
			const std::string ext = ".vstpreset";
			if (name.size () > ext.size () && name[0] != '.'
			    && name.compare (name.size () - ext.size (), ext.size (), ext) == 0)
				out.push_back (folder + "/" + name);
		}
		closedir (dir);
	}
#endif
	std::sort (out.begin (), out.end ());
	return out;
}

bool SpyBandController::saveUserPreset (const std::string& path)
{
	if (path.empty ())
		return false;

	std::string target = path;
	const std::string ext = ".vstpreset";
	if (target.size () < ext.size () || target.compare (target.size () - ext.size (), ext.size (), ext) != 0)
		target += ext;

	IBStream* stream = FileStream::open (target.c_str (), "wb");
	if (stream == nullptr)
		return false;

	const std::vector<std::uint8_t> component = encodeState (currentPatch ());
	const std::string name = fileStem (target);

	bool ok = false;
	{
		PresetFile file (stream);
		file.setClassID (kSpyBandProcessorUID);
		std::vector<std::uint8_t> controller;
		for (int i = 0; i < 4; ++i)
			controller.push_back (static_cast<std::uint8_t> ((kControllerStateTag >> (8 * i)) & 0xFF));
		const int32 length = static_cast<int32> (name.size ());
		for (int i = 0; i < 4; ++i)
			controller.push_back (static_cast<std::uint8_t> ((length >> (8 * i)) & 0xFF));
		controller.insert (controller.end (), name.begin (), name.end ());

		ok = file.writeHeader ()
		  && file.writeChunk (component.data (), static_cast<int32> (component.size ()), kComponentState)
		  && file.writeChunk (controller.data (), static_cast<int32> (controller.size ()), kControllerState)
		  && file.writeChunkList ();
	}
	stream->release ();

	if (ok)
	{
		mPresetBank = "USER";
		mPresetName = name;
		mFactoryIndex = -1;
		mPresetModified = false;
	}
	return ok;
}

bool SpyBandController::loadUserPreset (const std::string& path)
{
	IBStream* stream = FileStream::open (path.c_str (), "rb");
	if (stream == nullptr)
		return false;

	bool ok = false;
	Patch patch;
	{
		PresetFile file (stream);
		if (file.readChunkList () && file.getClassID () == kSpyBandProcessorUID)
		{
			if (const PresetFile::Entry* e = file.getEntry (kComponentState))
			{
				std::vector<std::uint8_t> bytes (static_cast<std::size_t> (e->size));
				int32 read = 0;
				if (! bytes.empty ()
				    && stream->seek (e->offset, IBStream::kIBSeekSet, nullptr) == kResultOk
				    && stream->read (bytes.data (), static_cast<int32> (bytes.size ()), &read) == kResultOk
				    && read == static_cast<int32> (bytes.size ()))
				{
					ok = decodeState (bytes.data (), bytes.size (), patch);
				}
			}
		}
	}
	stream->release ();

	if (! ok)
		return false;

	applyPatch (patch, true);
	mPresetBank = "USER";
	mPresetName = fileStem (path);
	mFactoryIndex = -1;
	mPresetModified = false;
	return true;
}

//------------------------------------------------------------------------
// The controller's own state: the preset name, nothing else. Everything
// that makes a sound is in the processor's.
//------------------------------------------------------------------------
tresult PLUGIN_API SpyBandController::getState (IBStream* state)
{
	if (state == nullptr)
		return kResultFalse;
	IBStreamer streamer (state, kLittleEndian);
	streamer.writeInt32 (kControllerStateTag);
	const std::string text = mPresetBank + "\n" + mPresetName + "\n" + (mPresetModified ? "1" : "0");
	streamer.writeInt32 (static_cast<int32> (text.size ()));
	streamer.writeRaw (text.data (), static_cast<int32> (text.size ()));
	return kResultOk;
}

tresult PLUGIN_API SpyBandController::setState (IBStream* state)
{
	if (state == nullptr)
		return kResultFalse;
	IBStreamer streamer (state, kLittleEndian);
	int32 tag = 0, length = 0;
	if (! streamer.readInt32 (tag) || tag != kControllerStateTag)
		return kResultOk;   // an older project, with no controller state
	if (! streamer.readInt32 (length) || length < 0 || length > 4096)
		return kResultOk;

	std::string text (static_cast<std::size_t> (length), '\0');
	if (length > 0 && streamer.readRaw (&text[0], length) != length)
		return kResultOk;

	// A preset file's chunk is just the name; a project's is
	// bank, name and the modified flag on three lines.
	const std::size_t a = text.find ('\n');
	if (a == std::string::npos)
	{
		mPresetBank = "USER";
		mPresetName = text;
		mPresetModified = false;
	}
	else
	{
		const std::size_t b = text.find ('\n', a + 1);
		mPresetBank = text.substr (0, a);
		mPresetName = text.substr (a + 1, b == std::string::npos ? std::string::npos : b - a - 1);
		mPresetModified = (b != std::string::npos && text.substr (b + 1) == "1");
	}
	mFactoryIndex = -1;
	for (int i = 0; i < factoryPresetCount (); ++i)
		if (mPresetBank == "FACTORY" && mPresetName == factoryPresetName (i))
			mFactoryIndex = i;
	return kResultOk;
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
		    && raw != nullptr && size >= kSpyBandMeterHeader * sizeof (double))
		{
			const double* frame = static_cast<const double*> (raw);
			mVoiced = (frame[0] >= 0.5);
			mInputPeak[0] = frame[2];
			mInputPeak[1] = frame[3];
			mCorrelation = frame[4];
			mOutputPeak[0] = frame[5];
			mOutputPeak[1] = frame[6];
			mMeterFrames++;

			int count = static_cast<int> (frame[1]);
			const int available =
				static_cast<int> (size / sizeof (double)) - kSpyBandMeterHeader;
			if (count > available)
				count = available;
			if (count > 2 * kMaxBands)
				count = 2 * kMaxBands;
			if (count < 0)
				count = 0;
			for (int i = 0; i < count; ++i)
				mMeter[i] = frame[kSpyBandMeterHeader + i];
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
