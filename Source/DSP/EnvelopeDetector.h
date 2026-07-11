#pragma once
#include <JuceHeader.h>

namespace valvane {

// ═══════════════════════════════════════════════════════════════════════════
// EnvelopeDetector
//
// Switchable Peak / RMS / Blend (crossfade) envelope follower.
//
// Peak detector:
//   env[n] = env[n-1] + (|x[n]| - env[n-1]) * alpha
//   alpha  = 1 - exp(-1 / (tau * fs))
//
// RMS detector:
//   rms_sq[n] = rms_sq[n-1] + (x[n]^2 - rms_sq[n-1]) * alpha_rms
//   envelope  = sqrt(rms_sq[n])
//
// Blend: linear crossfade between Peak and RMS envelopes:
//   output = (1 - blend) * peak + blend * rms
//
// All time constants are derived from getSampleRate(), never hardcoded.
// ═══════════════════════════════════════════════════════════════════════════
class EnvelopeDetector
{
public:
    enum class Type : int { Peak = 0, RMS = 1, Blend = 2 };

    EnvelopeDetector() = default;

    void prepare (double sampleRate);
    void reset();

    // Process a single sample; returns the current envelope level (linear).
    float processSample (float input);

    // ---- Setters ----
    void setType       (Type t)       { detType = t; }
    void setAttackMs   (float ms)     { attackMs  = std::max (ms, 0.001f); updateCoeffs(); }
    void setReleaseMs  (float ms)     { releaseMs = std::max (ms, 0.001f); updateCoeffs(); }
    void setBlend      (float b)      { blendAmt = juce::jlimit (0.0f, 1.0f, b); }

    // ---- Getters ----
    float getEnvelope() const         { return currentOutput; }
    Type  getType() const             { return detType; }

private:
    void updateCoeffs();

    double sr         = 44100.0;
    Type   detType    = Type::Peak;

    float attackMs    = 1.0f;
    float releaseMs   = 100.0f;
    float blendAmt    = 0.0f;      // 0 = pure peak, 1 = pure RMS

    float attackCoeff  = 0.0f;
    float releaseCoeff = 0.0f;

    float peakEnv      = 0.0f;     // instantaneous peak envelope
    float rmsSquared   = 0.0f;     // running mean of x^2
    float currentOutput = 0.0f;    // final blended output
};

} // namespace valvane
