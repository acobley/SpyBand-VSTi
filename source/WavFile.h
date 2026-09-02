//------------------------------------------------------------------------
// SpyBand - the sample slots' file loader (was CSpyBand::LoadFile)
//
// The DXi read a .wav with a hand-rolled RIFF parser that assumed 16-bit
// PCM, assumed two channels, leaked every buffer it allocated and left a
// `if (i > 211000) int A = 1;` breakpoint in the sample loop. What survives
// is its OUTPUT format, because the playback loop depends on it: one flat
// interleaved stereo array of floats, counted in SAMPLES rather than
// frames.
//
// No SDK types.
//------------------------------------------------------------------------

#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace SpyBand {

//------------------------------------------------------------------------
/** One loaded file: interleaved stereo, normalised to +/-1.

    Immutable once built, so the audio thread can read one through a
    pointer that the UI thread publishes with a single atomic store. */
struct WavData
{
	std::vector<float> samples;      // interleaved L,R,L,R...
	double             fileRate = 0.0;   // the rate in the FILE's header
	int                fileChannels = 0;
	int                fileBits = 0;
	std::string        path;

	long frameCount () const
	{
		return static_cast<long> (samples.size () / 2);
	}

	/** The DXi's NumberofSamplesN: the total count of interleaved samples,
	    less one. The playback loop compares its counter against this
	    before reading a PAIR, so the last odd sample of an odd-length
	    buffer is never reached - which is why it is minus one and not the
	    size. */
	long sampleLimit () const
	{
		return static_cast<long> (samples.size ()) - 1;
	}
};

//------------------------------------------------------------------------
/** Read a RIFF/WAVE file.

    Handles what the DXi could not: 8, 16, 24 and 32-bit PCM and 32/64-bit
    float, mono as well as stereo, WAVE_FORMAT_EXTENSIBLE, and chunks in
    any order. A mono file is duplicated into both channels rather than
    being read as alternating L and R samples at double speed, which is
    what the DXi's parser did with one - see PORTING-NOTES section 6.

    The file's own sample rate is recorded but NOT resampled: the DXi
    played every file back at the host's rate whatever the header said, and
    that is preserved. `libsamplerate` and `libsndfile` tarballs sit
    unopened in the DXi's folder, which is where that intention stopped.

    Returns nullptr and sets `error` if the file cannot be read. */
std::shared_ptr<const WavData> loadWav (const std::string& path,
                                        std::string& error);

//------------------------------------------------------------------------
} // namespace SpyBand
