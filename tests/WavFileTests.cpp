//------------------------------------------------------------------------
// SpyBand - .wav loader checks
//
// The DXi's parser assumed 16-bit stereo PCM with the data chunk second,
// and did something quietly wrong with everything else. These build files
// in memory, write them to a temporary path and read them back.
//
//   c++ -std=c++17 -O2 -Wall -Wextra -I../source ../source/WavFile.cpp
//       WavFileTests.cpp -o wav-tests && ./wav-tests
//------------------------------------------------------------------------

#include "WavFile.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace SpyBand;

static int fails = 0;

static void chk (bool ok, const char* what)
{
	if (! ok)
	{
		std::printf ("  FAIL: %s\n", what);
		++fails;
	}
}

//------------------------------------------------------------------------
struct Builder
{
	std::vector<unsigned char> bytes;

	void u32 (unsigned v)
	{
		for (int i = 0; i < 4; ++i)
			bytes.push_back (static_cast<unsigned char> ((v >> (8 * i)) & 0xFF));
	}
	void u16 (unsigned v)
	{
		for (int i = 0; i < 2; ++i)
			bytes.push_back (static_cast<unsigned char> ((v >> (8 * i)) & 0xFF));
	}
	void tag (const char* t) { bytes.insert (bytes.end (), t, t + 4); }
};

/** A WAVE file with `channels` channels of `bits` bits, optionally with a
    junk chunk before the data so the chunk walk is exercised. */
static std::string writeWav (const char* name, int channels, int bits,
                             bool isFloat, const std::vector<double>& interleaved,
                             bool withJunk)
{
	Builder b;
	std::vector<unsigned char> data;

	const int bytesPer = bits / 8;
	for (double v : interleaved)
	{
		unsigned char buf[8] = { 0 };
		if (isFloat && bits == 32)
		{
			const float f = static_cast<float> (v);
			std::memcpy (buf, &f, 4);
		}
		else if (bits == 8)
		{
			buf[0] = static_cast<unsigned char> (v * 127.0 + 128.0);
		}
		else if (bits == 16)
		{
			const int s = static_cast<int> (std::lround (v * 32767.0));
			buf[0] = static_cast<unsigned char> (s & 0xFF);
			buf[1] = static_cast<unsigned char> ((s >> 8) & 0xFF);
		}
		else if (bits == 24)
		{
			const int s = static_cast<int> (std::lround (v * 8388607.0));
			buf[0] = static_cast<unsigned char> (s & 0xFF);
			buf[1] = static_cast<unsigned char> ((s >> 8) & 0xFF);
			buf[2] = static_cast<unsigned char> ((s >> 16) & 0xFF);
		}
		else
		{
			const long s = std::lround (v * 2147483647.0);
			for (int i = 0; i < 4; ++i)
				buf[i] = static_cast<unsigned char> ((s >> (8 * i)) & 0xFF);
		}
		data.insert (data.end (), buf, buf + bytesPer);
	}

	b.tag ("RIFF");
	b.u32 (0);                       // patched below
	b.tag ("WAVE");
	b.tag ("fmt ");
	b.u32 (16);
	b.u16 (isFloat ? 3 : 1);
	b.u16 (static_cast<unsigned> (channels));
	b.u32 (44100);
	b.u32 (44100u * static_cast<unsigned> (channels * bytesPer));
	b.u16 (static_cast<unsigned> (channels * bytesPer));
	b.u16 (static_cast<unsigned> (bits));

	if (withJunk)
	{
		b.tag ("LIST");
		b.u32 (5);                   // odd, so the pad byte matters
		for (int i = 0; i < 5; ++i)
			b.bytes.push_back ('x');
		b.bytes.push_back (0);       // word-align pad
	}

	b.tag ("data");
	b.u32 (static_cast<unsigned> (data.size ()));
	b.bytes.insert (b.bytes.end (), data.begin (), data.end ());

	const unsigned riffSize = static_cast<unsigned> (b.bytes.size () - 8);
	for (int i = 0; i < 4; ++i)
		b.bytes[4 + static_cast<std::size_t> (i)] =
			static_cast<unsigned char> ((riffSize >> (8 * i)) & 0xFF);

	std::string path = std::string ("/tmp/") + name;
	std::FILE* fp = std::fopen (path.c_str (), "wb");
	std::fwrite (b.bytes.data (), 1, b.bytes.size (), fp);
	std::fclose (fp);
	return path;
}

//------------------------------------------------------------------------
int main ()
{
	std::string error;

	//--------------------------------------------------------------------
	// 16-bit stereo, which is all the DXi could read.
	//--------------------------------------------------------------------
	{
		const std::vector<double> pcm = { 0.5, -0.5, 0.25, -0.25 };
		const std::string path = writeWav ("spyband-16-stereo.wav", 2, 16, false, pcm, false);
		auto w = loadWav (path, error);
		chk (w != nullptr, "16-bit stereo did not load");
		if (w)
		{
			chk (w->frameCount () == 2, "wrong frame count");
			chk (w->sampleLimit () == 3, "sampleLimit is not size - 1");
			chk (std::fabs (w->samples[0] - 0.5) < 1e-4, "left sample wrong");
			chk (std::fabs (w->samples[1] + 0.5) < 1e-4, "right sample wrong");
			chk (w->fileRate == 44100.0, "sample rate not read");
			chk (w->fileChannels == 2 && w->fileBits == 16, "format not recorded");
		}
	}

	//--------------------------------------------------------------------
	// DEVIATION: a MONO file.
	//
	// The DXi computed its frame count as dwDSize/(channels * bytes) and
	// then multiplied it back by the channel count, so a mono file became
	// a buffer the playback loop read two samples at a time as left and
	// right - playing it an octave up with alternate samples in opposite
	// channels. Here it is duplicated into both.
	//--------------------------------------------------------------------
	{
		const std::vector<double> pcm = { 0.5, 0.25, -0.75 };
		const std::string path = writeWav ("spyband-16-mono.wav", 1, 16, false, pcm, false);
		auto w = loadWav (path, error);
		chk (w != nullptr, "16-bit mono did not load");
		if (w)
		{
			chk (w->frameCount () == 3, "mono frame count wrong");
			for (int f = 0; f < 3; ++f)
				chk (w->samples[f * 2] == w->samples[f * 2 + 1],
				     "a mono file did not come out the same in both channels");
			chk (std::fabs (w->samples[4] + 0.75) < 1e-4, "mono sample wrong");
		}
	}

	//--------------------------------------------------------------------
	// The depths the DXi could not read at all, and a chunk in the way.
	//--------------------------------------------------------------------
	for (int bits : { 8, 24, 32 })
	{
		const std::vector<double> pcm = { 0.5, -0.5 };
		char name[64];
		std::snprintf (name, sizeof (name), "spyband-%d.wav", bits);
		const std::string path = writeWav (name, 2, bits, false, pcm, true);
		auto w = loadWav (path, error);
		chk (w != nullptr, "a supported bit depth did not load");
		if (w)
		{
			const double tol = (bits == 8) ? 0.01 : 1e-4;
			chk (std::fabs (w->samples[0] - 0.5) < tol, "value wrong at this depth");
			chk (std::fabs (w->samples[1] + 0.5) < tol, "value wrong at this depth");
		}
	}

	{
		const std::vector<double> pcm = { 0.5, -0.5 };
		const std::string path = writeWav ("spyband-float.wav", 2, 32, true, pcm, true);
		auto w = loadWav (path, error);
		chk (w != nullptr, "32-bit float did not load");
		if (w)
			chk (std::fabs (w->samples[0] - 0.5) < 1e-6, "float value wrong");
	}

	//--------------------------------------------------------------------
	// Failures must be failures, not silence with a plausible buffer.
	//--------------------------------------------------------------------
	{
		auto w = loadWav ("/tmp/spyband-does-not-exist.wav", error);
		chk (w == nullptr && ! error.empty (), "a missing file did not report an error");

		std::FILE* fp = std::fopen ("/tmp/spyband-garbage.wav", "wb");
		std::fputs ("this is not a wave file at all", fp);
		std::fclose (fp);
		w = loadWav ("/tmp/spyband-garbage.wav", error);
		chk (w == nullptr && ! error.empty (), "garbage loaded as audio");
	}

	std::printf (fails ? "\n%d FAILURES\n" : "\nall wav checks passed\n", fails);
	return fails ? 1 : 0;
}
