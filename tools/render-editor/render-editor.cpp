//------------------------------------------------------------------------
// SpyBand - draw the real editor into PNG files, with no window
//
// Builds the editor exactly as a host would - the same SpyBandEditor and
// SpyBandController, the same buildViews() - into a CFrame that is never
// opened, and draws it into VSTGUI's cairo offscreen context at 2x. The
// controller is fed what the processor would send: a sample rate, three
// loaded slots and one meter frame shaped like the mock-up's.
//
// This is how the panel is compared with
// Designs/SpyBand-Polivoks-1-BlackPanel.png on a machine with no
// display. Linux only (it uses VSTGUI's cairo back end); see build.sh.
//
//   render-editor <out-dir>
//
// writes editor-bands.png, editor-bands-drag.png, editor-matrix.png and
// editor-input.png.
//------------------------------------------------------------------------

#include "SpyBandController.h"
#include "SpyBandEditor.h"
#include "SpyBandIDs.h"

#include "public.sdk/source/main/moduleinit.h"
#include "public.sdk/source/main/pluginfactory.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "vstgui/lib/platform/linux/linuxfactory.h"

#include "vstgui/lib/cframe.h"
#include "vstgui/lib/coffscreencontext.h"
#include "vstgui/lib/events.h"
#include "vstgui/lib/platform/iplatformbitmap.h"
#include "vstgui/lib/platform/iplatformfileselector.h"
#include "vstgui/lib/platform/iplatformfont.h"
#include "vstgui/lib/platform/iplatformframe.h"
#include "vstgui/lib/platform/iplatformgradient.h"
#include "vstgui/lib/platform/iplatformgraphicsdevice.h"
#include "vstgui/lib/platform/iplatformresourceinputstream.h"
#include "vstgui/lib/platform/iplatformtimer.h"
#include "vstgui/lib/platform/iplatformstring.h"
#include "vstgui/lib/platform/linux/cairobitmap.h"
#include "vstgui/lib/platform/linux/cairogradient.h"
#include "vstgui/lib/platform/linux/cairographicscontext.h"
#include "vstgui/lib/platform/linux/linuxtaskexecutor.h"
#include "vstgui/lib/platform/platformfactory.h"
#include "vstgui/lib/vstguiinit.h"

#include <chrono>
#include <cstdlib>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace VSTGUI;
using namespace Steinberg;
using namespace Steinberg::Vst;
using namespace SpyBand;

//------------------------------------------------------------------------
// A platform factory with only what drawing needs: cairo bitmaps, paths
// and gradients. No frames, no fonts, no timers - the panel uses none.
//------------------------------------------------------------------------
namespace VSTGUI {

struct PreviewString : IPlatformString
{
	void setUTF8String (UTF8StringPtr) override {}
};

class PreviewFactory : public IPlatformFactory
{
public:
	void finalize () noexcept override { executor->waitAllTasksExecuted (); }
	uint64_t getTicks () const noexcept override
	{
		using namespace std::chrono;
		return duration_cast<milliseconds> (steady_clock::now ().time_since_epoch ()).count ();
	}
	PlatformFramePtr createFrame (IPlatformFrameCallback*, const CRect&, void*, PlatformType,
	                              IPlatformFrameConfig*) const noexcept override { return nullptr; }
	PlatformFontPtr createFont (const UTF8String&, const CCoord&, const int32_t&) const noexcept override
	{
		return nullptr;
	}
	bool getAllFontFamilies (const FontFamilyCallback&) const noexcept override { return false; }
	PlatformBitmapPtr createBitmap (const CPoint& size) const noexcept override
	{
		return makeOwned<Cairo::Bitmap> (size);
	}
	PlatformBitmapPtr createBitmap (const CResourceDescription&) const noexcept override { return nullptr; }
	PlatformBitmapPtr createBitmapFromPath (UTF8StringPtr path) const noexcept override
	{
		return Cairo::Bitmap::create (path);
	}
	PlatformBitmapPtr createBitmapFromMemory (const void* p, uint32_t size) const noexcept override
	{
		return Cairo::Bitmap::create (p, size);
	}
	PNGBitmapBuffer createBitmapMemoryPNGRepresentation (const PlatformBitmapPtr& bitmap) const noexcept override
	{
		if (auto b = dynamic_cast<Cairo::Bitmap*> (bitmap.get ()))
			return b->createMemoryPNGRepresentation ();
		return {};
	}
	PlatformResourceInputStreamPtr createResourceInputStream (const CResourceDescription&) const noexcept override
	{
		return {};
	}
	PlatformStringPtr createString (UTF8StringPtr) const noexcept override
	{
		return makeOwned<PreviewString> ();
	}
	PlatformTimerPtr createTimer (IPlatformTimerCallback*) const noexcept override { return nullptr; }
	bool setClipboard (const DataPackagePtr&) const noexcept override { return false; }
	DataPackagePtr getClipboard () const noexcept override { return nullptr; }
	PlatformGradientPtr createGradient () const noexcept override
	{
		return std::make_unique<Cairo::Gradient> ();
	}
	PlatformFileSelectorPtr createFileSelector (PlatformFileSelectorStyle, IPlatformFrame*) const noexcept override
	{
		return nullptr;
	}
	const IPlatformGraphicsDeviceFactory& getGraphicsDeviceFactory () const noexcept override
	{
		return devices;
	}
	const IPlatformTaskExecutor& getTaskExecutor () const noexcept override { return *executor; }
	bool replaceTaskExecutor (const ReplaceTaskExecFunc&) const noexcept override { return false; }
	const LinuxFactory* asLinuxFactory () const noexcept override { return nullptr; }
	const MacFactory* asMacFactory () const noexcept override { return nullptr; }
	const Win32Factory* asWin32Factory () const noexcept override { return nullptr; }

private:
	CairoGraphicsDeviceFactory devices;
	PlatformTaskExecutorPtr executor { std::make_unique<LinuxTaskExecutor> () };
};

static PlatformFactoryPtr gFactory;

void initPlatform (PlatformInstanceHandle)
{
	gFactory = std::unique_ptr<IPlatformFactory> (new PreviewFactory);
}

void exitPlatform ()
{
	gFactory->finalize ();
	gFactory = nullptr;
}

const IPlatformFactory& getPlatformFactory () { return *gFactory; }

} // namespace VSTGUI

//------------------------------------------------------------------------
// What vstguieditor.cpp expects of the module it is built into. There is
// no module here, and main() initialises VSTGUI itself.
//------------------------------------------------------------------------
void* moduleHandle = nullptr;   // moduleinit.cpp's getPlatformModuleHandle reads it

namespace Steinberg {
DEF_CLASS_IID (IPluginFactoryInternal)
namespace Linux { void setupVSTGUIRunloop (FUnknown*) {} }
} // namespace Steinberg

SMTG_EXPORT_SYMBOL Steinberg::IPluginFactory* PLUGIN_API GetPluginFactory () { return nullptr; }

/** cairobitmap.cpp asks for a resource folder when loading a bitmap by
    name; the panel has no bitmaps. */
std::string VSTGUI::LinuxFactory::getResourcePath () const noexcept { return {}; }

//------------------------------------------------------------------------
namespace {

/** A 16-bit stereo .wav of shaped noise, for the slot overviews. */
void writeWav (const std::string& path, int seed, double rateHz)
{
	const int frames = 48000 * 2;
	std::vector<int16_t> data (static_cast<std::size_t> (frames) * 2);
	uint32_t s = static_cast<uint32_t> (seed) * 2654435761u + 1u;
	for (int f = 0; f < frames; ++f)
	{
		s = s * 1664525u + 1013904223u;
		const double noise = (static_cast<double> (s >> 8) / 16777216.0) * 2.0 - 1.0;
		const double t = static_cast<double> (f) / frames;
		const double env = 0.25 + 0.75 * std::pow (std::fabs (std::sin (t * rateHz * 3.14159)), 3.0)
		                 * (0.6 + 0.4 * std::sin (t * 7.0 + seed));
		const int16_t v = static_cast<int16_t> (noise * env * 30000.0);
		data[static_cast<std::size_t> (2 * f)] = v;
		data[static_cast<std::size_t> (2 * f + 1)] = v;
	}

	FILE* file = std::fopen (path.c_str (), "wb");
	if (! file)
		return;
	auto u32 = [file] (uint32_t v) { std::fwrite (&v, 4, 1, file); };
	auto u16 = [file] (uint16_t v) { std::fwrite (&v, 2, 1, file); };
	const uint32_t bytes = static_cast<uint32_t> (data.size () * 2);
	std::fwrite ("RIFF", 1, 4, file); u32 (36 + bytes); std::fwrite ("WAVE", 1, 4, file);
	std::fwrite ("fmt ", 1, 4, file); u32 (16); u16 (1); u16 (2); u32 (48000); u32 (48000 * 4); u16 (4); u16 (16);
	std::fwrite ("data", 1, 4, file); u32 (bytes);
	std::fwrite (data.data (), 2, data.size (), file);
	std::fclose (file);
}

void send (SpyBandController* controller, IMessage* message)
{
	controller->notify (message);
	message->release ();
}

void feed (SpyBandController* controller, const std::string& dir)
{
	// The sample rate.
	{
		auto* m = new HostMessage;
		m->setMessageID (kSpyBandSampleRateMessage);
		m->getAttributes ()->setFloat (kSpyBandSampleRateAttribute, 48000.0);
		send (controller, m);
	}

	// Three slots loaded, as the mock-up has them.
	const char* const kNames[3] = { "Numbers Station.wav", "Morse 20wpm.wav", "Shortwave Voice.wav" };
	std::string blob;
	for (int s = 0; s < Vocoder::kNumSlots; ++s)
	{
		std::string path;
		if (s < 3)
		{
			path = dir + "/" + kNames[s];
			writeWav (path, s + 1, 9.0 + 4.0 * s);
		}
		blob.push_back (path.empty () ? '\0' : '\1');
		blob += path;
		blob.push_back ('\0');
	}
	{
		auto* m = new HostMessage;
		m->setMessageID (kSpyBandSlotStateMessage);
		m->getAttributes ()->setBinary (kSpyBandSlotStateAttribute, blob.data (),
		                                static_cast<uint32> (blob.size ()));
		send (controller, m);
	}

	// One meter frame: the mock-up's bars, -9 dB on the left, -21 dB on
	// the right, a correlation of 0.12, and the detector hearing a vowel.
	{
		const double bars[9] = { 0.45, 1.0, 0.68, 0.36, 0.83, 0.58, 0.49, 0.18, 0.08 };
		double frame[kSpyBandMeterHeader + 9] = { 1.0, 9.0, 0.355, 0.089, 0.12, 0.70, 0.66, 0.0 };
		for (int i = 0; i < 9; ++i)
			frame[kSpyBandMeterHeader + i] = bars[i];
		auto* m = new HostMessage;
		m->setMessageID (kSpyBandMeterDataMessage);
		m->getAttributes ()->setBinary (kSpyBandMeterAttribute, frame, sizeof (frame));
		send (controller, m);
	}
}

bool render (CFrame* frame, const std::string& path)
{
	const CRect size = frame->getViewSize ();
	auto context = COffscreenContext::create (CPoint (size.getWidth (), size.getHeight ()), 2.0);
	if (! context)
	{
		std::fprintf (stderr, "no offscreen context\n");
		return false;
	}
	context->beginDraw ();
	{
		// The cairo context draws in device pixels; the panel is laid out
		// in points, so scale by the backing factor.
		CDrawContext::Transform scale (*context, CGraphicsTransform ().scale (2.0, 2.0));
		frame->drawRect (context, size);
	}
	context->endDraw ();

	auto bitmap = context->getBitmap ();
	if (! bitmap || ! bitmap->getPlatformBitmap ())
		return false;
	const auto png = getPlatformFactory ().createBitmapMemoryPNGRepresentation (bitmap->getPlatformBitmap ());
	FILE* file = std::fopen (path.c_str (), "wb");
	if (! file)
		return false;
	std::fwrite (png.data (), 1, png.size (), file);
	std::fclose (file);
	std::printf ("%s  %d x %d (2x)\n", path.c_str (), static_cast<int> (size.getWidth ()),
	             static_cast<int> (size.getHeight ()));
	return true;
}

template <typename T>
T* findView (CFrame* frame)
{
	for (uint32_t i = 0; i < frame->getNbViews (); ++i)
		if (auto* v = dynamic_cast<T*> (frame->getView (i)))
			return v;
	return nullptr;
}

} // namespace

//------------------------------------------------------------------------
int main (int argc, char** argv)
{
	const std::string out = argc > 1 ? argv[1] : ".";

	VSTGUI::init (nullptr);

	auto* controller = new SpyBandController;
	controller->initialize (nullptr);

	// The mock-up's patch: Classic 9-Band, Repeat, the detector on, two
	// slots playing, the diagonal at full and two routings off it.
	controller->loadFactoryPreset (0);
	controller->setParamNormalized (kRepeatSamp, 1.0);
	controller->setParamNormalized (kVoicedDetect, 1.0);
	controller->setParamNormalized (kSample1, 1.0);
	controller->setParamNormalized (kSample2, 1.0);
	for (int b = 0; b < 9; ++b)
		controller->setParamNormalized (patchParam (b, b), 1.0);
	controller->setParamNormalized (patchParam (3, 5), 0.5);
	controller->setParamNormalized (patchParam (6, 7), 0.5);

	// The generated samples go in a scratch folder, not beside the PNGs.
	char scratch[] = "/tmp/spyband-render-XXXXXX";
	const std::string samples = mkdtemp (scratch) ? scratch : out;
	feed (controller, samples);

	auto* editor = new SpyBandEditor (controller);
	auto* frame = new CFrame (CRect (0, 0, SpyBandEditor::kEditorWidth, SpyBandEditor::kEditorHeight), editor);
	editor->buildViews (frame);
	editor->refresh ();

	bool ok = render (frame, out + "/editor-bands.png");

	// Hovering the Bottom handle brings up its note.
	if (auto* display = findView<SpyBandsDisplay> (frame))
	{
		const double x = display->getViewSize ().left
		               + display->getViewSize ().getWidth () * std::log (100.0 / 20.0) / std::log (1000.0);
		MouseMoveEvent move;
		move.mousePosition = CPoint (x, display->getViewSize ().top + 80.0);
		display->onMouseMoveEvent (move);
		ok = render (frame, out + "/editor-bands-drag.png") && ok;
		MouseExitEvent exit;
		display->onMouseExitEvent (exit);
	}

	editor->selectTab (0);
	ok = render (frame, out + "/editor-matrix.png") && ok;
	editor->selectTab (2);
	ok = render (frame, out + "/editor-input.png") && ok;

	frame->forget ();
	return ok ? 0 : 1;
}
