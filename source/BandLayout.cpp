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

	const double baseFreq = 25.0 * std::pow (2.0, (48.0 * freqInternal / 12.0));
	double topFreq = 25.0 * std::pow (2.0, (108.0 * spreadInternal / 12.0)) + baseFreq;

	if (topFreq > (sampleRate / 2.0))
		topFreq = sampleRate / 2.0;

	const double ns       = 12.0 * (std::log (topFreq / baseFreq) / log2);
	const double stepSize = ns / bands;

	return baseFreq * std::pow (2.0, (stepSize * (index + 0.5) / 12.0));
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
double outputGain (double control)
{
	return std::pow (10.0, (kOutputRangeDb * (control - 1.0)) / 20.0);
}

double outputDecibels (double control)
{
	return kOutputRangeDb * (control - 1.0);
}

double outputControlFromDb (double db)
{
	double control = 1.0 + db / kOutputRangeDb;
	if (control < 0.0)
		control = 0.0;
	if (control > 1.0)
		control = 1.0;
	return control;
}

//------------------------------------------------------------------------
} // namespace SpyBand
