//------------------------------------------------------------------------
// SpyBand - the sample slots' file loader
//------------------------------------------------------------------------

#include "WavFile.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace SpyBand {
namespace {

//------------------------------------------------------------------------
struct Reader
{
	const std::vector<unsigned char>& bytes;
	std::size_t pos = 0;

	bool have (std::size_t n) const { return pos + n <= bytes.size (); }

	std::uint32_t u32 ()
	{
		std::uint32_t v = static_cast<std::uint32_t> (bytes[pos])
		                | (static_cast<std::uint32_t> (bytes[pos + 1]) << 8)
		                | (static_cast<std::uint32_t> (bytes[pos + 2]) << 16)
		                | (static_cast<std::uint32_t> (bytes[pos + 3]) << 24);
		pos += 4;
		return v;
	}

	std::uint16_t u16 ()
	{
		std::uint16_t v = static_cast<std::uint16_t> (
			bytes[pos] | (static_cast<std::uint16_t> (bytes[pos + 1]) << 8));
		pos += 2;
		return v;
	}

	bool tag (const char* four) const
	{
		return std::memcmp (&bytes[pos], four, 4) == 0;
	}
};

//------------------------------------------------------------------------
bool readFile (const std::string& path, std::vector<unsigned char>& out,
               std::string& error)
{
	std::FILE* fp = std::fopen (path.c_str (), "rb");
	if (fp == nullptr)
	{
		error = "cannot open file";
		return false;
	}
	if (std::fseek (fp, 0, SEEK_END) != 0)
	{
		std::fclose (fp);
		error = "cannot seek file";
		return false;
	}
	const long size = std::ftell (fp);
	std::rewind (fp);
	if (size <= 0)
	{
		std::fclose (fp);
		error = "file is empty";
		return false;
	}
	out.resize (static_cast<std::size_t> (size));
	const std::size_t got = std::fread (out.data (), 1, out.size (), fp);
	std::fclose (fp);
	if (got != out.size ())
	{
		error = "short read";
		return false;
	}
	return true;
}

//------------------------------------------------------------------------
/** One sample, from `bits` bits at `p`, normalised to +/-1. */
double decode (const unsigned char* p, int bits, bool isFloat)
{
	if (isFloat)
	{
		if (bits == 32)
		{
			float f = 0.0f;
			std::memcpy (&f, p, 4);
			return static_cast<double> (f);
		}
		double d = 0.0;
		std::memcpy (&d, p, 8);
		return d;
	}

	switch (bits)
	{
		case 8:
			// 8-bit PCM in a .wav is UNSIGNED, centred on 128.
			return (static_cast<int> (p[0]) - 128) / 128.0;

		case 16:
		{
			const std::int16_t v = static_cast<std::int16_t> (
				p[0] | (static_cast<std::uint16_t> (p[1]) << 8));
			// 32767, not 32768: the DXi divided by 32767 and that half-bit
			// of gain is carried across.
			return v / 32767.0;
		}

		case 24:
		{
			std::int32_t v = static_cast<std::int32_t> (
				(static_cast<std::uint32_t> (p[0]) << 8)
				| (static_cast<std::uint32_t> (p[1]) << 16)
				| (static_cast<std::uint32_t> (p[2]) << 24));
			v >>= 8;                       // sign-extend from 24 bits
			return v / 8388607.0;
		}

		case 32:
		{
			std::int32_t v = 0;
			std::memcpy (&v, p, 4);
			return v / 2147483647.0;
		}

		default:
			return 0.0;
	}
}

} // namespace

//------------------------------------------------------------------------
std::shared_ptr<const WavData> loadWav (const std::string& path,
                                        std::string& error)
{
	error.clear ();

	std::vector<unsigned char> bytes;
	if (! readFile (path, bytes, error))
		return nullptr;

	Reader r { bytes, 0 };
	if (! r.have (12) || ! r.tag ("RIFF"))
	{
		error = "not a RIFF file";
		return nullptr;
	}
	r.pos += 8;
	if (! r.tag ("WAVE"))
	{
		error = "not a WAVE file";
		return nullptr;
	}
	r.pos += 4;

	int    channels = 0;
	int    bits     = 0;
	bool   isFloat  = false;
	double rate     = 0.0;
	std::size_t dataAt = 0;
	std::size_t dataSize = 0;

	// Walk the chunks. The DXi's parser assumed fmt then data and hunted
	// for "data" with a loop that read each unwanted chunk into a leaked
	// buffer; this just skips them.
	while (r.have (8))
	{
		const bool isFmt  = r.tag ("fmt ");
		const bool isData = r.tag ("data");
		r.pos += 4;
		const std::uint32_t size = r.u32 ();
		const std::size_t body = r.pos;

		if (! r.have (size))
			break;

		if (isFmt && size >= 16)
		{
			const std::uint16_t format = r.u16 ();
			channels = r.u16 ();
			rate     = static_cast<double> (r.u32 ());
			r.pos += 4;                 // bytes per second
			r.pos += 2;                 // block align
			bits     = r.u16 ();
			isFloat  = (format == 3);   // WAVE_FORMAT_IEEE_FLOAT

			if (format == 0xFFFE && size >= 40)
			{
				// EXTENSIBLE: the real format is the first two bytes of the
				// sub-format GUID, 22 bytes into the extension.
				r.pos = body + 24;
				const std::uint16_t sub = r.u16 ();
				isFloat = (sub == 3);
			}
		}
		else if (isData)
		{
			dataAt = body;
			dataSize = size;
		}

		r.pos = body + size + (size & 1u);   // chunks are word-aligned
	}

	if (channels < 1 || bits < 8 || dataSize == 0)
	{
		error = "no usable fmt or data chunk";
		return nullptr;
	}
	if (! (bits == 8 || bits == 16 || bits == 24 || bits == 32
	       || (isFloat && bits == 64)))
	{
		error = "unsupported sample size";
		return nullptr;
	}

	const int bytesPerSample = bits / 8;
	const std::size_t frameBytes =
		static_cast<std::size_t> (bytesPerSample) * static_cast<std::size_t> (channels);
	const std::size_t frames = dataSize / frameBytes;
	if (frames == 0)
	{
		error = "no frames";
		return nullptr;
	}

	auto wav = std::make_shared<WavData> ();
	wav->fileRate     = rate;
	wav->fileChannels = channels;
	wav->fileBits     = bits;
	wav->path         = path;
	wav->samples.resize (frames * 2);

	for (std::size_t f = 0; f < frames; ++f)
	{
		const unsigned char* p = bytes.data () + dataAt + f * frameBytes;
		const double left = decode (p, bits, isFloat);
		const double right = (channels > 1)
			? decode (p + bytesPerSample, bits, isFloat)
			: left;
		wav->samples[f * 2]     = static_cast<float> (left);
		wav->samples[f * 2 + 1] = static_cast<float> (right);
	}

	return wav;
}

//------------------------------------------------------------------------
} // namespace SpyBand
