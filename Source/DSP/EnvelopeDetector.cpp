#include "EnvelopeDetector.h"
#include <cmath>

namespace valvane {

// ═══════════════════════════════════════════════════════════════════════════
// EnvelopeDetector implementation
//
// Coefficient derivation (standard first-order IIR smoothing):
//   For a time constant tau (seconds) and sample rate fs (Hz):
//     alpha = 1 - exp(-1 / (tau * fs))
//
//   tau = attackMs / 1000   for attack
//   tau = releaseMs / 1000  for release
//
// The attack coefficient is used when the input exceeds the current
// envelope, the release coefficient when the input falls below it.
// ═══════════════════════════════════════════════════════════════════════════

void EnvelopeDetector::prepare (double sampleRate)
{
    sr = sampleRate;
    updateCoeffs();
    reset();
}

void EnvelopeDetector::reset()
{
    peakEnv       = 0.0f;
    rmsSquared    = 0.0f;
    currentOutput = 0.0f;
}

float EnvelopeDetector::processSample (float input)
{
    const float absInput = std::abs (input);

    // ---- Peak envelope follower ----
    // env[n] = env[n-1] + (|x[n]| - env[n-1]) * alpha
    // where alpha = attackCoeff if rising, releaseCoeff if falling.
    {
        const float alpha = (absInput > peakEnv) ? attackCoeff : releaseCoeff;
        peakEnv += (absInput - peakEnv) * alpha;
    }

    // ---- RMS envelope follower ----
    // One-pole lowpass on x^2, then take square root.
    // rms_sq[n] = rms_sq[n-1] + (x[n]^2 - rms_sq[n-1]) * alpha_rms
    {
        const float sq = input * input;
        const float alpha = (sq > rmsSquared) ? attackCoeff : releaseCoeff;
        rmsSquared += (sq - rmsSquared) * alpha;
    }
    const float rmsEnv = std::sqrt (std::max (rmsSquared, 0.0f));

    // ---- Blend / select ----
    switch (detType)
    {
        case Type::Peak:
            currentOutput = peakEnv;
            break;

        case Type::RMS:
            currentOutput = rmsEnv;
            break;

        case Type::Blend:
        default:
            // Linear crossfade: (1 - blend) * peak + blend * rms
            currentOutput = (1.0f - blendAmt) * peakEnv + blendAmt * rmsEnv;
            break;
    }

    return currentOutput;
}

void EnvelopeDetector::updateCoeffs()
{
    // alpha = 1 - exp(-1 / (tau * fs))
    // tau = ms / 1000
    const float tauAttack  = attackMs  * 0.001f;
    const float tauRelease = releaseMs * 0.001f;

    attackCoeff  = 1.0f - std::exp (-1.0f / (static_cast<float>(sr) * tauAttack));
    releaseCoeff = 1.0f - std::exp (-1.0f / (static_cast<float>(sr) * tauRelease));
}

} // namespace valvane
