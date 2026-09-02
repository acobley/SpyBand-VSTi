//------------------------------------------------------------------------
// SpyBand - vocoder checks
//
// The DSP holds no SDK type, so this needs no SDK, no host and no window
// server:
//
//   c++ -std=c++17 -O2 -Wall -Wextra -I../source ../source/Vocoder.cpp
//       ../source/Adsr.cpp ../source/WavFile.cpp ../source/BandLayout.cpp
//       VocoderTests.cpp -o vocoder-tests && ./vocoder-tests
//
// Each of the deliberate changes to the DXi's behaviour is pinned here,
// and each was run against the ORIGINAL arithmetic first to watch it fail -
// where "the original" is written out in the test rather than described,
// so the two can be compared rather than taken on trust.
//------------------------------------------------------------------------

#include "BandLayout.h"
#include "Vocoder.h"

#include <cmath>
#include <cstdio>
#include <algorithm>
#include <limits>
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

static void close (double a, double b, double tol, const char* what)
{
	if (! (std::fabs (a - b) <= tol))
	{
		std::printf ("  FAIL: %s (%.10g vs %.10g)\n", what, a, b);
		++fails;
	}
}

//------------------------------------------------------------------------
// A run of the vocoder, for comparing runs against each other.
//------------------------------------------------------------------------
struct Run
{
	std::vector<float> left, right;
	double peak = 0.0;
};

static Vocoder::Params musicalParams ()
{
	Vocoder::Params p;
	p.interlaced = true;          // right channel modulates left, no file needed
	p.freq = 0.50;                // bottom band near 100 Hz
	p.freqSpread = 0.75;          // top band near 1.5 kHz
	p.outputTrim = 1.0;           // the DXi's own staging, so nothing is hidden
	for (int i = 0; i < kMaxBands; ++i)
		p.patch[i * kMaxBands + i] = 1.0;
	return p;
}

/** Deterministic test signal: a tone in the left (carrier) and a slower
    tone in the right (modulator). No noise, so two runs are comparable. */
static void fillInput (std::vector<float>& l, std::vector<float>& r,
                       int frames, double sampleRate)
{
	l.resize (frames);
	r.resize (frames);
	for (int i = 0; i < frames; ++i)
	{
		const double t = i / sampleRate;
		l[i] = static_cast<float> (0.6 * std::sin (2.0 * M_PI * 220.0 * t));
		r[i] = static_cast<float> (0.6 * std::sin (2.0 * M_PI * 3.0 * t));
	}
}

static Run render (const Vocoder::Params& p, int frames, int blockSize,
                   double sampleRate)
{
	Vocoder v;
	v.setSampleRate (sampleRate);
	v.reset ();

	std::vector<float> inL, inR;
	fillInput (inL, inR, frames, sampleRate);

	Run run;
	run.left.assign (frames, 0.0f);
	run.right.assign (frames, 0.0f);

	for (int start = 0; start < frames; start += blockSize)
	{
		const int n = (start + blockSize <= frames) ? blockSize : (frames - start);
		const float* in[2] = { inL.data () + start, inR.data () + start };
		float* out[2] = { run.left.data () + start, run.right.data () + start };
		v.process (p, in, 2, out, 2, n);
	}
	run.peak = v.peakSinceLastCall ();
	return run;
}

/** A render with a probe signal of its own, and a settling period cut off
    the front.

    `fillInput`'s modulator is a 3 Hz sine, which suits the determinism
    checks - where what the signal IS does not matter - and is useless for
    measuring anything: less than one cycle fits in the buffer, so its rms
    is not what a sine's rms should be, and its spectral leakage swamps a
    Goertzel a couple of hundred hertz away. The through and carrier checks
    below need a real modulator, so they bring their own. */
static Run renderProbe (const Vocoder::Params& p, double carrierHz, double modulatorHz,
                        int frames, double sampleRate, int settle)
{
	Vocoder v;
	v.setSampleRate (sampleRate);
	v.reset ();

	const int block = 512;
	std::vector<float> inL (block), inR (block), outL (block), outR (block);
	const float* in[2] = { inL.data (), inR.data () };
	float* out[2] = { outL.data (), outR.data () };

	Run run;
	for (int start = 0; start < frames; start += block)
	{
		for (int i = 0; i < block; ++i)
		{
			const double t = (start + i) / sampleRate;
			inL[i] = static_cast<float> (0.6 * std::sin (2.0 * M_PI * carrierHz * t));
			inR[i] = static_cast<float> (0.6 * std::sin (2.0 * M_PI * modulatorHz * t));
		}
		v.process (p, in, 2, out, 2, block);
		if (start < settle)
			continue;
		for (int i = 0; i < block; ++i)
		{
			run.left.push_back (outL[i]);
			run.right.push_back (outR[i]);
		}
	}
	return run;
}

static bool identical (const Run& a, const Run& b)
{
	if (a.left.size () != b.left.size ())
		return false;
	for (std::size_t i = 0; i < a.left.size (); ++i)
		if (a.left[i] != b.left[i] || a.right[i] != b.right[i])
			return false;
	return true;
}

/** Energy at `hz`, over the whole buffer, by Goertzel. */
static double energyAt (const std::vector<float>& x, double hz, double sampleRate)
{
	const double w = 2.0 * M_PI * hz / sampleRate;
	const double coeff = 2.0 * std::cos (w);
	double s1 = 0.0, s2 = 0.0;
	for (float v : x)
	{
		const double s0 = v + coeff * s1 - s2;
		s2 = s1;
		s1 = s0;
	}
	return std::sqrt (s1 * s1 + s2 * s2 - coeff * s1 * s2) / x.size ();
}

static double db (double x) { return (x <= 1e-30) ? -300.0 : 20.0 * std::log10 (x); }

//------------------------------------------------------------------------
int main ()
{
	const double sr = 44100.0;

	//--------------------------------------------------------------------
	// DEVIATION 3: the hard-coded 44100.
	//
	// Every filter and envelope constant in the DXi divided by a literal
	// 44100 whatever the host was running at, so a band whose CENTRE was
	// computed as 500 Hz was realised at 500 * rate / 44100 - over an
	// octave up at 96 k. The port passes the real rate.
	//
	// `oldRealisedHz` is the original's behaviour, written out. At 44100
	// the two must agree to the last bit; anywhere else the old one moves
	// and the new one does not.
	//--------------------------------------------------------------------
	const auto oldRealisedHz = [] (double freq, double spread, int bands,
	                               int index, double actualRate)
	{
		// The DXi computed the layout against 44100 and then used those
		// numbers as if they were radians at the real rate.
		const double nominal = bandCentreHz (freq, spread, bands, index, 44100.0);
		// Written as a ratio so that at 44100 it is a multiplication by
		// exactly 1.0, and the bit-identical claim below means what it says.
		return nominal * (actualRate / 44100.0);
	};

	for (int i = 0; i < 9; ++i)
	{
		const double now = bandCentreHz (0.5, 0.75, 9, i, 44100.0);
		chk (now == oldRealisedHz (0.5, 0.75, 9, i, 44100.0),
		     "at 44100 the fix is not bit-identical to the DXi");

		const double at48 = bandCentreHz (0.5, 0.75, 9, i, 48000.0);
		close (at48, now, 1e-9, "the band moved when the sample rate changed");

		const double old48 = oldRealisedHz (0.5, 0.75, 9, i, 48000.0);
		chk (std::fabs (old48 - now) > now * 0.05,
		     "the 48 k check would pass against the old code too");

		const double old96 = oldRealisedHz (0.5, 0.75, 9, i, 96000.0);
		chk (old96 > now * 2.0, "the DXi's 96 k error was not over an octave");
	}

	// The top of the range is clamped to Nyquist, so the layout at a high
	// rate is allowed to differ where the DXi's clamp was biting.
	chk (bandCentreHz (0.0, 1.0, 9, 8, 44100.0) < 22050.0, "a band sits above Nyquist");
	chk (bandCentreHz (0.0, 1.0, 9, 8, 96000.0) >= bandCentreHz (0.0, 1.0, 9, 8, 44100.0),
	     "the wider Nyquist at 96 k did not free the top band");

	// The noise high pass, and the envelope times.
	close (noiseHighPassHz (0.01, 44100.0), 110.25, 1e-9, "noise corner at the default");
	close (noiseHighPassHz (0.01, 96000.0), 240.0, 1e-9, "noise corner does not follow the rate");
	close (envAttackMs (0.05), 13.5, 1e-12, "attack default");
	close (envReleaseMs (0.05), 6.0, 1e-12, "release default");

	//--------------------------------------------------------------------
	// Determinism, and block-size invariance.
	//
	// The second is the real test of the ramps added in place of the DXi's
	// per-block parameter read: with a static setting a ramp's increment
	// is zero, so splitting a run into different block sizes must not
	// change one sample of it. It also catches any per-block state that
	// should have been per-sample.
	//--------------------------------------------------------------------
	const Vocoder::Params p = musicalParams ();
	const Run a = render (p, 8192, 8192, sr);
	const Run b = render (p, 8192, 8192, sr);
	chk (identical (a, b), "two runs from reset() differ");

	const Run c = render (p, 8192, 512, sr);
	const Run d = render (p, 8192, 173, sr);
	chk (identical (a, c), "512-frame blocks differ from one 8192-frame block");
	chk (identical (a, d), "173-frame blocks differ from one 8192-frame block");

	chk (a.peak > 0.0, "the vocoder produced silence - the rest of these prove nothing");

	//--------------------------------------------------------------------
	// DEVIATION: the stride-3 interleave.
	//
	// The DXi's loop advanced its index twice inside the body and once in
	// the for statement, so it wrote frames 0, 1 then 3, 4 then 6, 7 and
	// left every third interleaved sample at the zero the buffer was
	// memset to. `damaged` is that output: the port's, with the DXi's
	// pattern of holes punched back into it.
	//
	// A hole every third sample is a multiplication by a 3-sample square
	// wave, which folds the signal to fs/3 either side of itself. The
	// bands here stop below 1.5 kHz, so energy up at 14 kHz is that
	// artefact and nothing else.
	//--------------------------------------------------------------------
	{
		std::vector<float> interleaved (a.left.size () * 2);
		for (std::size_t i = 0; i < a.left.size (); ++i)
		{
			interleaved[i * 2] = a.left[i];
			interleaved[i * 2 + 1] = a.right[i];
		}

		std::vector<float> damaged = interleaved;
		for (std::size_t i = 2; i < damaged.size (); i += 3)
			damaged[i] = 0.0f;

		// The fold lands at fs/3 offset by whatever the signal holds, so
		// probe fs/3 itself and the carrier's images either side of it.
		// Probing a round 14 kHz instead finds the skirt and understates
		// the artefact by 25 dB - which is how this test first passed
		// against a threshold it should have failed.
		const double probes[3] = { sr / 3.0 - 220.0, sr / 3.0, sr / 3.0 + 220.0 };
		double clean = 0.0, broken = 0.0;
		for (double hz : probes)
		{
			const double a1 = energyAt (interleaved, hz, sr);
			const double b1 = energyAt (damaged, hz, sr);
			if (a1 > clean)
				clean = a1;
			if (b1 > broken)
				broken = b1;
		}

		std::printf ("  stride artefact around fs/3: port %.1f dB, DXi %.1f dB\n",
		             db (clean), db (broken));
		chk (broken > clean * 100.0,
		     "the DXi's stride would not have shown up at fs/3 - the test proves nothing");
		chk (db (clean) < -100.0, "the port has artefact energy where it should have none");
	}

	//--------------------------------------------------------------------
	// A patch cell that is open by an unmeasurable amount must do
	// unmeasurably little.
	//
	// This compares the two BRANCHES against each other rather than
	// trusting that they were typed the same way: at 1e-30 the cell is in
	// the active list and every multiply and add runs, but the sum it
	// contributes is far below the last bit of the accumulator.
	//--------------------------------------------------------------------
	{
		Vocoder::Params off = musicalParams ();
		Vocoder::Params tiny = off;
		tiny.patch[3 * kMaxBands + 7] = 1e-30;

		const Run r1 = render (off, 4096, 512, sr);
		const Run r2 = render (tiny, 4096, 512, sr);
		chk (identical (r1, r2), "a cell open by 1e-30 changed the output");

		// And one open for real must not.
		Vocoder::Params real = off;
		real.patch[3 * kMaxBands + 7] = 1.0;
		const Run r3 = render (real, 4096, 512, sr);
		chk (! identical (r1, r3), "opening a cell for real changed nothing either");
	}

	//--------------------------------------------------------------------
	// Bypass is the input, exactly.
	//
	// The DXi never read PARAM_ENABLE, so there is no original behaviour
	// to match - but an effect that is off has to be its input and not an
	// approximation of it.
	//--------------------------------------------------------------------
	{
		Vocoder::Params offParams = musicalParams ();
		offParams.enable = false;

		Vocoder v;
		v.setSampleRate (sr);
		v.reset ();

		std::vector<float> inL, inR;
		fillInput (inL, inR, 1024, sr);
		std::vector<float> outL (1024), outR (1024);
		const float* in[2] = { inL.data (), inR.data () };
		float* out[2] = { outL.data (), outR.data () };

		// The first block crossfades out of the enabled state, so it is
		// the SECOND that must be the input untouched.
		v.process (offParams, in, 2, out, 2, 1024);
		v.process (offParams, in, 2, out, 2, 1024);

		bool same = true;
		for (int i = 0; i < 1024; ++i)
			if (outL[i] != inL[i] || outR[i] != inR[i])
				same = false;
		chk (same, "a disabled vocoder is not its input");
	}

	//--------------------------------------------------------------------
	// A non-finite input must not stay in the recursion.
	//
	// The bands are resonant and their outputs are multiplied together by
	// the matrix; one infinity would otherwise circulate for good. The DXi
	// had no guard of any kind.
	//--------------------------------------------------------------------
	{
		Vocoder v;
		v.setSampleRate (sr);
		v.reset ();

		std::vector<float> inL (512, 0.0f), inR (512, 0.0f);
		inL[10] = std::numeric_limits<float>::infinity ();
		inR[11] = std::nanf ("");
		std::vector<float> outL (512), outR (512);
		const float* in[2] = { inL.data (), inR.data () };
		float* out[2] = { outL.data (), outR.data () };

		v.process (p, in, 2, out, 2, 512);

		std::fill (inL.begin (), inL.end (), 0.0f);
		std::fill (inR.begin (), inR.end (), 0.0f);
		v.process (p, in, 2, out, 2, 512);

		bool finite = true;
		for (int i = 0; i < 512; ++i)
			if (! std::isfinite (outL[i]) || ! std::isfinite (outR[i]))
				finite = false;
		chk (finite, "an infinity fed in one block was still there the next");
	}

	//--------------------------------------------------------------------
	// THE THROUGH CONTROL
	//
	// Added after "I think it might be going through regardless of
	// setting". It is not, and these say so three ways.
	//
	// The first is the decisive one: with the patch matrix EMPTY the
	// vocoder can contribute nothing at all, so the through path is the
	// only thing that can make a sound. At zero it must be digital
	// silence - not "quiet", zero - and above it the level must be the
	// modulator times Src Level times the control, with nothing else in it.
	//--------------------------------------------------------------------
	{
		Vocoder::Params empty = musicalParams ();
		for (double& cell : empty.patch)
			cell = 0.0;

		Vocoder::Params silent = empty;
		silent.sampThroughLevel = 0.0;
		const Run r0 = renderProbe (silent, 220.0, 1000.0, 16384, sr, 0);

		bool allZero = true;
		for (std::size_t i = 0; i < r0.left.size (); ++i)
			if (r0.left[i] != 0.0f || r0.right[i] != 0.0f)
				allZero = false;
		chk (allZero, "Through at zero is not silent with an empty matrix");

		// In interlace mode the through path is the right input channel,
		// scaled by Src Level (control * 5, so unity at the default 0.2)
		// and then by the control. Nothing else is in it - not the
		// carrier, not the filters.
		for (double level : { 0.25, 1.0 })
		{
			Vocoder::Params p2 = empty;
			p2.sampThroughLevel = level;
			const Run r2 = renderProbe (p2, 220.0, 1000.0, 16384, sr, 0);

			double sum = 0.0;
			for (float v : r2.left)
				sum += double (v) * v;
			const double rms = std::sqrt (sum / r2.left.size ());

			const double expected = (0.6 / std::sqrt (2.0)) * (0.2 * 5.0) * level;
			close (rms, expected, expected * 0.01,
			       "the through path is not the modulator at the level asked for");
		}
	}

	//--------------------------------------------------------------------
	// And the other half of the same question: the CARRIER reaches the
	// output through the VOCODER, not through the through path, so moving
	// Through must not change how much of it comes out.
	//
	// This is what "going through regardless of setting" actually is. With
	// Noise Override corrected (DEVIATION 5) the live input is the carrier,
	// so it is always in the output - which it never was in the DXi,
	// because its inverted test replaced the carrier with pink noise at the
	// shipped default. The last check below is that difference, measured.
	//--------------------------------------------------------------------
	{
		Vocoder::Params none = musicalParams ();
		none.sampThroughLevel = 0.0;
		Vocoder::Params full = musicalParams ();
		full.sampThroughLevel = 1.0;

		const Run rn = renderProbe (none, 220.0, 1000.0, 44100, sr, 8192);
		const Run rf = renderProbe (full, 220.0, 1000.0, 44100, sr, 8192);

		const double carrierNone = energyAt (rn.left, 220.0, sr);
		const double carrierFull = energyAt (rf.left, 220.0, sr);

		std::printf ("  carrier at 220 Hz: Through 0 %.1f dB, Through 1 %.1f dB\n",
		             db (carrierNone), db (carrierFull));
		chk (carrierNone > 0.0, "no carrier reaches the output at all");
		close (db (carrierFull), db (carrierNone), 0.5,
		       "Through changed how much carrier comes out - it must not touch it");

		// Through DOES add the modulator, which is the whole of its job.
		const double modNone = energyAt (rn.left, 1000.0, sr);
		const double modFull = energyAt (rf.left, 1000.0, sr);
		std::printf ("  modulator at 1 kHz: Through 0 %.1f dB, Through 1 %.1f dB\n",
		             db (modNone), db (modFull));
		chk (modFull > modNone * 4.0, "Through did not add the modulator");

		// With the DXi's inverted sense the carrier was replaced by noise,
		// so the input's own frequency all but vanished. This is the
		// difference the fix makes, and it is why the input is audible now
		// and was not then.
		Vocoder::Params asShipped = none;
		asShipped.noiseOverride = true;      // the DXi's default, corrected sense
		const Run rs = renderProbe (asShipped, 220.0, 1000.0, 44100, sr, 8192);
		const double carrierNoise = energyAt (rs.left, 220.0, sr);

		std::printf ("  the DXi's shipped default put the carrier at %.1f dB\n",
		             db (carrierNoise));
		chk (carrierNoise < carrierNone * 0.2,
		     "a noise carrier is not much quieter at the input's own frequency");
	}

	//--------------------------------------------------------------------
	// The input LED columns.
	//
	// They read the taps immediately after Src Level, so the peak must be
	// the input times that control and nothing else - and, the reason they
	// exist, a MONO source must move both columns identically.
	//--------------------------------------------------------------------
	{
		Vocoder::Params p2 = musicalParams ();

		Vocoder v;
		v.setSampleRate (sr);
		v.reset ();

		std::vector<float> inL (2048), inR (2048), outL (2048), outR (2048);
		for (int i = 0; i < 2048; ++i)
		{
			const double t = i / sr;
			inL[i] = static_cast<float> (0.5 * std::sin (2.0 * M_PI * 220.0 * t));
			inR[i] = static_cast<float> (0.25 * std::sin (2.0 * M_PI * 1000.0 * t));
		}
		const float* in[2] = { inL.data (), inR.data () };
		float* out[2] = { outL.data (), outR.data () };
		v.process (p2, in, 2, out, 2, 2048);

		float peakL = 0.f, peakR = 0.f;
		v.inputPeaks (peakL, peakR);

		// Src Level defaults to 0.2, which the DSP takes as 0.2 * 5 = unity.
		close (peakL, 0.5, 0.01, "the left column does not read the left input");
		close (peakR, 0.25, 0.01, "the right column does not read the right input");

		// DEVIATION 6: with Interlace on the controls are SPLIT. Halving
		// Src Level halves the left and leaves the right alone; halving
		// Wav Level does the opposite. If either of these ever fails,
		// Src Level has crept back onto the modulator.
		Vocoder::Params quiet = p2;
		quiet.inLevel = 0.1;
		Vocoder v2;
		v2.setSampleRate (sr);
		v2.reset ();
		v2.process (quiet, in, 2, out, 2, 2048);
		v2.inputPeaks (peakL, peakR);
		close (peakL, 0.25, 0.01, "Src Level does not scale the left column");
		close (peakR, 0.25, 0.01, "Src Level still reaches the right channel");

		Vocoder::Params quietR = p2;
		quietR.sampLevel = 0.1;
		Vocoder v2b;
		v2b.setSampleRate (sr);
		v2b.reset ();
		v2b.process (quietR, in, 2, out, 2, 2048);
		v2b.inputPeaks (peakL, peakR);
		close (peakL, 0.5, 0.01, "Wav Level reaches the left channel");
		close (peakR, 0.125, 0.01, "Wav Level does not scale the right column");

		// THE POINT OF THEM: a mono source moves both columns the same,
		// which is what says "these two channels are the same audio" -
		// and in Interlace mode that means the signal vocodes itself.
		Vocoder v3;
		v3.setSampleRate (sr);
		v3.reset ();
		const float* mono[2] = { inL.data (), inL.data () };
		v3.process (p2, mono, 2, out, 2, 2048);
		v3.inputPeaks (peakL, peakR);
		chk (peakL == peakR, "a mono source did not read identically on both columns");
	}

	//--------------------------------------------------------------------
	// DEVIATION 6, where it is audible: Src Level is no longer squared.
	//
	// The DXi scaled both channels before the split, so with Interlace on
	// one control sat on both sides of the vocoder's multiply and the
	// output went as Src SQUARED - halving it cost 12 dB. `asDxi` is that
	// behaviour written out: the same gain applied to the modulator by
	// hand, which is what the port used to do implicitly.
	//--------------------------------------------------------------------
	{
		const auto rmsOf = [&] (const Run& run)
		{
			double sum = 0.0;
			for (float v : run.left)
				sum += double (v) * v;
			return std::sqrt (sum / run.left.size ());
		};

		double previous = 0.0, previousDxi = 0.0;
		for (int step = 0; step < 3; ++step)
		{
			const double src = 0.2 / (1 << step);          // 0.2, 0.1, 0.05

			Vocoder::Params now = musicalParams ();
			now.inLevel = src;

			// The DXi's coupling: Src Level on the modulator as well.
			Vocoder::Params asDxi = musicalParams ();
			asDxi.inLevel = src;
			asDxi.sampLevel = 0.2 * (src * 5.0 + 0.00001);

			const double a = rmsOf (renderProbe (now, 220.0, 1000.0, 44100, sr, 8192));
			const double b = rmsOf (renderProbe (asDxi, 220.0, 1000.0, 44100, sr, 8192));

			if (step > 0)
			{
				const double drop = db (previous) - db (a);
				const double dropDxi = db (previousDxi) - db (b);
				std::printf ("  halving Src Level: port %.1f dB, the DXi's coupling %.1f dB\n",
				             drop, dropDxi);
				close (drop, 6.02, 0.3, "Src Level is not 6 dB per halving");
				close (dropDxi, 12.04, 0.4, "the DXi's coupling was not 12 dB per halving");
			}
			previous = a;
			previousDxi = b;
		}
	}

	//--------------------------------------------------------------------
	// The meter the editor reads.
	//--------------------------------------------------------------------
	{
		Vocoder v;
		v.setSampleRate (sr);
		v.reset ();

		std::vector<float> inL, inR;
		fillInput (inL, inR, 512, sr);
		std::vector<float> outL (512), outR (512);
		const float* in[2] = { inL.data (), inR.data () };
		float* out[2] = { outL.data (), outR.data () };

		Vocoder::Params meterParams = musicalParams ();
		meterParams.bands = 12;
		v.process (meterParams, in, 2, out, 2, 512);

		double frame[2 * kMaxBands] = { 0.0 };
		const int count = v.meter (frame);
		chk (count == 12, "the meter did not report one value per band");

		double sum = 0.0;
		for (int i = 0; i < count; ++i)
			sum += frame[i];
		chk (sum > 0.0, "every band envelope read zero");
	}

	//--------------------------------------------------------------------
	// Band counts
	//--------------------------------------------------------------------
	for (int i = 0; i < 4; ++i)
		chk (bandCount (i) == kBandCounts[i], "bandCount does not match the table");
	chk (bandCount (-5.0) == 9, "an out-of-range band control did not clamp low");
	chk (bandCount (99.0) == 22, "an out-of-range band control did not clamp high");

	std::printf (fails ? "\n%d FAILURES\n" : "\nall vocoder checks passed\n", fails);
	return fails ? 1 : 0;
}
