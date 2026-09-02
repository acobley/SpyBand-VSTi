//------------------------------------------------------------------------
// SpyBand - the unvoiced-noise envelope (was ADSR.h / ADSR.cpp)
//
// Ported line for line. SpyBand uses exactly one of these: the envelope
// that opens the pink-noise substitute when the voiced detector says the
// modulator has gone unvoiced (SpyBand.cpp:686-700).
//
// No SDK types, so it compiles and can be tested standalone.
//------------------------------------------------------------------------

#pragma once

namespace SpyBand {

//------------------------------------------------------------------------
/** The phases, and their original numbering - GetPhase() is compared
    against RELEASE by name in the processing loop, so the values matter. */
enum AdsrPhase
{
	kAdsrAttack   = 0,
	kAdsrDecay    = 1,
	kAdsrSustain  = 2,
	kAdsrRelease  = 3,
	kAdsrReleased = 4
};

//------------------------------------------------------------------------
class Adsr
{
public:
	Adsr () = default;

	/** Lengths are in SECONDS and are converted with the current sample
	    rate, so setSampleRate must come first. The DXi set the rate
	    directly on a public member and then called the four setters every
	    block (SpyBand.cpp:428-432); doing it in that order is what makes
	    the conversion right. */
	void setSampleRate (double sampleRate);

	void setAttackTime  (double seconds);
	void setDecayTime   (double seconds);
	void setSustainLevel (double level);
	void setReleaseTime (double seconds);

	/** Advances one sample and returns the level.

	    Returns 1.0 - not zero - until the envelope has been triggered for
	    the first time, which is the DXi's `if (EnvEnable == false) return
	    1.0` and is why the noise is at full level before the first voiced
	    transition rather than silent. */
	double nextValue ();

	void trigger ();
	void setRelease ();

	int  phase () const { return mPhase; }

	/** Back to the constructed state. The DXi had no equivalent - it never
	    reset anything - but VST3 hosts call setActive(false)/(true) and
	    expect a plug-in to start clean. */
	void reset ();

	double envLevel () const     { return mEnvLevel; }
	void setEnvLevel (double v)  { mEnvLevel = v; }
	void setInvert (bool v)      { mInvert = v; }

private:
	double mSampleRate   = 44100.0;

	// Lengths in SAMPLES, as in the original, where they were longs.
	double mAttack       = 0.5;
	double mDecay        = 0.0005;
	double mSustain      = 1.0;
	double mRelease      = 0.95;

	int    mPhase        = kAdsrAttack;
	double mCurrentValue = 0.0;
	double mCurrentSample = 0.0;
	double mSustainSample = 0.0;
	double mReleaseInitialValue = 0.0;

	double mEnvLevel     = 0.0;
	bool   mEnvEnable    = false;
	bool   mInvert       = false;
};

//------------------------------------------------------------------------
} // namespace SpyBand
