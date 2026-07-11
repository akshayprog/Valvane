#pragma once
#include <cmath>
#include <algorithm>

namespace valvane {

// ═══════════════════════════════════════════════════════════════════════════
// GainComputer
//
// Implements the soft-knee compressor transfer curve from:
//
//   Giannoulis, D., Massberg, M., & Reiss, J. D. (2012).
//   "Digital Dynamic Range Compressor Design — A Tutorial and Analysis."
//   Journal of the Audio Engineering Society, 60(6), 399-408.
//
// All computation is in the dB domain.
//
// Let x_dB = input level (dB), T = threshold (dB), R = ratio, W = knee (dB).
//
//   if 2*(x_dB - T) < -W :  output_dB = x_dB                            (below knee)
//   if 2*(x_dB - T) >  W :  output_dB = T + (x_dB - T) / R              (above knee)
//   else (inside knee)    :  output_dB = x_dB + (1/R - 1)*(x_dB - T + W/2)^2 / (2*W)
//
// This guarantees C1 continuity (value AND first derivative) at both knee
// boundaries, eliminating click/zipper artifacts at the transition points.
//
// The gain reduction returned is:  GR(dB) = output_dB - x_dB  (always <= 0).
// ═══════════════════════════════════════════════════════════════════════════
class GainComputer
{
public:
    /// Compute the gain change (dB) to apply to an input at |inputDb|.
    /// Returns a value <= 0 representing gain reduction.
    static inline float computeGainDb (float inputDb,
                                       float thresholdDb,
                                       float ratio,
                                       float kneeWidthDb)
    {
        // Protect against ratio <= 1 (no compression)
        if (ratio <= 1.0f)
            return 0.0f;

        const float diff = inputDb - thresholdDb;
        const float halfW = kneeWidthDb * 0.5f;

        if (2.0f * diff < -kneeWidthDb)
        {
            // Below knee — no gain reduction
            return 0.0f;
        }
        else if (2.0f * diff > kneeWidthDb)
        {
            // Above knee — full compression
            // GR = (x_dB - T) * (1 - 1/R)  →  always negative when x > T
            return diff * (1.0f - 1.0f / ratio);
        }
        else
        {
            // Inside the soft-knee region
            // GR = (1/R - 1) * (x_dB - T + W/2)^2 / (2*W)
            const float t = diff + halfW;
            return (1.0f / ratio - 1.0f) * (t * t) / (2.0f * kneeWidthDb);
        }
    }

    /// Convert linear amplitude to dB (with -200 dB floor).
    static inline float linearToDb (float x)
    {
        return 20.0f * std::log10 (std::max (x, 1e-10f));
    }

    /// Convert dB to linear amplitude.
    static inline float dbToLinear (float db)
    {
        return std::pow (10.0f, db * 0.05f);
    }
};

} // namespace valvane
