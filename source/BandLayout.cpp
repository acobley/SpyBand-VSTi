//------------------------------------------------------------------------
// SpyBand - the arithmetic the DSP and the editor must agree on
//------------------------------------------------------------------------

#include "BandLayout.h"

#include <cmath>

namespace SpyBand {

//------------------------------------------------------------------------
int bandCount (double internal)
{
	int index = static_cast<int> (internal + 0.5);
	if (index < 0)
		index = 0;
	if (index > 3)
		index = 3;
	return kBandCounts[index];
}

//------------------------------------------------------------------------
double bandCentreHz (double freqInternal, double spreadInternal,
                     int bands, int index, double sampleRate)
{
	if (bands <= 0)
		return 0.0;

	const double log2 = std::log (2.0);

	const double baseFreq = bandBottomHz (freqInternal);
	const double topFreq = bandTopHz (freqInternal, spreadInternal, sampleRate);

	const double ns       = 12.0 * (std::log (topFreq / baseFreq) / log2);
	const double stepSize = ns / bands;

	return baseFreq * std::pow (2.0, (stepSize * (index + 0.5) / 12.0));
}

//------------------------------------------------------------------------
double bandBottomHz (double freqInternal)
{
	return 25.0 * std::pow (2.0, (48.0 * freqInternal / 12.0));
}

double bandTopHz (double freqInternal, double spreadInternal, double sampleRate)
{
	const double baseFreq = bandBottomHz (freqInternal);
	double topFreq = 25.0 * std::pow (2.0, (108.0 * spreadInternal / 12.0)) + baseFreq;

	if (topFreq > (sampleRate / 2.0))
		topFreq = sampleRate / 2.0;
	return topFreq;
}

//------------------------------------------------------------------------
double freqForBottomHz (double hz)
{
	if (hz <= 25.0)
		return 0.0;
	const double v = std::log2 (hz / 25.0) / 4.0;
	return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v);
}

double spreadForTopHz (double hz, double freqInternal)
{
	const double above = hz - bandBottomHz (freqInternal);
	if (above <= 25.0)
		return 0.0;
	const double v = std::log2 (above / 25.0) / 9.0;
	return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v);
}

//------------------------------------------------------------------------
double bandResponse (double hz, double centreHz, double resonanceInternal,
                     int slopes, double sampleRate)
{
	if (hz <= 0.0 || centreHz <= 0.0 || sampleRate <= 0.0 || hz >= sampleRate / 2.0)
		return 0.0;

	const double pi = 3.14159265358979323846;
	const double bandwidth = resonanceInternal / 4.0;
	const double omega = 2.0 * pi * centreHz / sampleRate;
	const double sn = std::sin (omega);
	const double cs = std::cos (omega);
	if (sn <= 0.0)
		return 0.0;
	const double alpha = sn * std::sinh (std::log (2.0) / 2.0 * bandwidth * omega / sn);
	if (alpha <= 0.0)
		return 0.0;

	// H(z) = alpha (1 - z^-2) / ((1 + alpha) - 2 cos(w0) z^-1 + (1 - alpha) z^-2)
	const double w = 2.0 * pi * hz / sampleRate;
	const double c1 = std::cos (w), s1 = std::sin (w);
	const double c2 = std::cos (2.0 * w), s2 = std::sin (2.0 * w);

	const double numRe = alpha * (1.0 - c2);
	const double numIm = alpha * s2;
	const double denRe = (1.0 + alpha) - 2.0 * cs * c1 + (1.0 - alpha) * c2;
	const double denIm = 2.0 * cs * s1 - (1.0 - alpha) * s2;

	const double den = std::sqrt (denRe * denRe + denIm * denIm);
	if (den <= 0.0)
		return 1.0;
	double m = std::sqrt (numRe * numRe + numIm * numIm) / den;
	if (m > 1.0)
		m = 1.0;

	const int sections = 1 + (slopes < 0 ? 0 : (slopes > 2 ? 2 : slopes));
	double out = 1.0;
	for (int i = 0; i < sections; ++i)
		out *= m;
	return out;
}

//------------------------------------------------------------------------
double noiseHighPassHz (double internal, double sampleRate)
{
	return sampleRate / 4.0 * internal;
}

//------------------------------------------------------------------------
double envAttackMs  (double internal) { return internal * 250.0 + 1.0; }
double envReleaseMs (double internal) { return internal * 100.0 + 1.0; }

//------------------------------------------------------------------------
double outputDecibels (double control)
{
	return kOutputMinDb + control * kOutputSpanDb;
}

double outputGain (double control)
{
	return std::pow (10.0, outputDecibels (control) / 20.0);
}

double outputControlFromDb (double db)
{
	double control = (db - kOutputMinDb) / kOutputSpanDb;
	if (control < 0.0)
		control = 0.0;
	if (control > 1.0)
		control = 1.0;
	return control;
}

double outputControlFromV1State (double v1Control)
{
	// Version 1's travel was -60..0 dB.
	return outputControlFromDb (-60.0 + v1Control * 60.0);
}

//------------------------------------------------------------------------
} // namespace SpyBand
