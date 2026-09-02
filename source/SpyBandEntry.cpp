//------------------------------------------------------------------------
// SpyBand - plug-in factory (was PlugInApp.cpp)
//------------------------------------------------------------------------

#include "SpyBandController.h"
#include "SpyBandIDs.h"
#include "SpyBandProcessor.h"
#include "version.h"

#include "public.sdk/source/main/pluginfactory.h"

#define stringPluginName "SpyBand"

using namespace Steinberg::Vst;
using namespace SpyBand;

//------------------------------------------------------------------------
BEGIN_FACTORY_DEF (stringCompanyName, "https://github.com/", "mailto:aecobley@googlemail.com")

	// SpyBand is an EFFECT, whatever the DXi registered as. CSpyBand derived
	// from CDXi and the filter from CSoftSynth, but Process() reads its input
	// buffer and nothing reads the MIDI queue - the synth interface was how a
	// DXi got an input pin and a place in the rack, not a statement about what
	// the plug-in is. PlugType::kFxModulation is what a host reads to decide
	// which list to put it in.
	DEF_CLASS2 (INLINE_UID_FROM_FUID (kSpyBandProcessorUID),
	            PClassInfo::kManyInstances,
	            kVstAudioEffectClass,
	            stringPluginName,
	            Vst::kDistributable,
	            PlugType::kFxModulation,
	            FULL_VERSION_STR,
	            kVstVersionString,
	            SpyBandProcessor::createInstance)

	DEF_CLASS2 (INLINE_UID_FROM_FUID (kSpyBandControllerUID),
	            PClassInfo::kManyInstances,
	            kVstComponentControllerClass,
	            stringPluginName "Controller",
	            0,
	            "",
	            FULL_VERSION_STR,
	            kVstVersionString,
	            SpyBandController::createInstance)

END_FACTORY
