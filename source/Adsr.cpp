//------------------------------------------------------------------------
// SpyBand - the unvoiced-noise envelope (was ADSR.cpp)
//------------------------------------------------------------------------

#include "Adsr.h"

namespace SpyBand {

//------------------------------------------------------------------------
void Adsr::setSampleRate (double sampleRate)
{
	if (sampleRate > 0.0)
		mSampleRate = sampleRate;
}

//------------------------------------------------------------------------
// The four setters, each `Rate * SamplesPerSec` with a floor of one sample,
// exactly as ADSR.cpp had them. The floor matters: the processing loop asks
// for a 0.01 s decay, and without it a zero rate divides by zero further
// down.
//------------------------------------------------------------------------
void Adsr::setAttackTime (double seconds)
{
	mAttack = seconds * mSampleRate;
	if (mAttack <= 0.0)
		mAttack = 1.0;
}

void Adsr::setDecayTime (double seconds)
{
	mDecay = seconds * mSampleRate;
	if (mDecay <= 0.0)
		mDecay = 1.0;
}

void Adsr::setSustainLevel (double level)
{
	mSustain = level;
	mReleaseInitialValue = mSustain;
}

void Adsr::setReleaseTime (double seconds)
{
	mRelease = seconds * mSampleRate;
	if (mRelease <= 0.0)
		mRelease = 1.0;
}

//------------------------------------------------------------------------
double Adsr::nextValue ()
{
	double value = mCurrentValue;

	switch (mPhase)
	{
		case kAdsrAttack:
			value = mCurrentSample / mAttack;
			if ((mCurrentSample > mAttack) || (value > 1.0))
				mPhase = kAdsrDecay;
			break;

		case kAdsrDecay:
			value = 1.0 - ((mCurrentSample - mAttack) / mDecay);
			if (value <= mSustain)
				mPhase = kAdsrSustain;
			break;

		case kAdsrSustain:
			break;

		case kAdsrRelease:
			value = mReleaseInitialValue
			      - ((mCurrentSample - mSustainSample) / mRelease);
			if (value <= 0.000001)
			{
				value = 0.000001;
				mPhase = kAdsrReleased;
			}
			break;

		case kAdsrReleased:
		default:
			break;
	}

	mCurrentValue = value;
	mCurrentSample += 1.0;

	if (! mEnvEnable)
		return 1.0;
	if (mInvert)
		return mEnvLevel * (1.0 - value);
	return mEnvLevel * value;
}

//------------------------------------------------------------------------
void Adsr::setRelease ()
{
	if (mPhase != kAdsrRelease)
	{
		mSustainSample = mCurrentSample;
		mReleaseInitialValue = mCurrentValue;
	}
	mPhase = kAdsrRelease;
}

//------------------------------------------------------------------------
void Adsr::trigger ()
{
	mPhase = kAdsrAttack;
	mEnvEnable = true;
	mCurrentSample = 0.0;
}

//------------------------------------------------------------------------
void Adsr::reset ()
{
	mPhase = kAdsrAttack;
	mCurrentValue = 0.0;
	mCurrentSample = 0.0;
	mSustainSample = 0.0;
	mReleaseInitialValue = mSustain;
	mEnvEnable = false;
}

//------------------------------------------------------------------------
} // namespace SpyBand
