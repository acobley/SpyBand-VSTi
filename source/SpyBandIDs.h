//------------------------------------------------------------------------
// SpyBand - class UIDs and message identifiers
//
// These UIDs are freshly generated for this port. NEVER CHANGE THEM once a
// build has been shipped: hosts store them in the project file, so a changed
// UID means every existing session silently loses the plug-in.
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace SpyBand {

static const Steinberg::FUID kSpyBandProcessorUID  (0x1AAF58FF, 0x9DC676FE, 0x4E7E1A9F, 0x272B46BA);
static const Steinberg::FUID kSpyBandControllerUID (0x3C43DE25, 0x3806072E, 0xF1CB1DBE, 0x430A90EC);

// The plug-in category is declared once, in SpyBandEntry.cpp, using the SDK's
// own PlugType::kFxModulation - not repeated as a string here.

//------------------------------------------------------------------------
// Processor <-> controller messages
//
// All of these travel on the UI thread. A message sent from process() is
// silently discarded by the host's connection proxy - see PORTING-GUIDE
// section 7 - so anything the DSP produces per block goes out through
// data.outputParameterChanges instead, never through a message.
//------------------------------------------------------------------------

/** Controller -> processor, from setState and from the editor's file
    buttons: "load this path into slot n", or "" to empty the slot. The
    processor owns the audio; the controller owns only the paths. */
static const char* const kSpyBandLoadSampleMessage = "SpyBandLoadSample";
static const char* const kSpyBandSlotAttribute     = "Slot";
static const char* const kSpyBandPathAttribute     = "Path";

/** Processor -> controller, from setActive and in reply to a request: the
    sample rate the DSP is actually running at, so the editor's frequency
    readouts cannot disagree with the filters. */
static const char* const kSpyBandSampleRateMessage   = "SpyBandSampleRate";
static const char* const kSpyBandSampleRateAttribute = "SampleRate";

/** The editor's band-envelope display (IDC_DRAWAREA) and its voiced lamp.
    The controller asks on a timer, the processor answers; both directions
    are UI-thread, unlike a message from process(). */
static const char* const kSpyBandMeterRequestMessage = "SpyBandMeterRequest";
static const char* const kSpyBandMeterDataMessage    = "SpyBandMeterData";
static const char* const kSpyBandMeterAttribute      = "Meter";

/** Processor -> controller, from setState and after a load: what actually
    happened to each slot, so the editor can label a button with the file
    name it really has and grey one whose file has gone missing. */
static const char* const kSpyBandSlotStateMessage = "SpyBandSlotState";
static const char* const kSpyBandSlotStateAttribute = "Slots";

//------------------------------------------------------------------------
} // namespace SpyBand
