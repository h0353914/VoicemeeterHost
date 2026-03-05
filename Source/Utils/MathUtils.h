/*
 * MathUtils.h
 * VoicemeeterHost — Math Utilities
 *
 * Lightweight helpers for geometry / bezier / linear-algebra
 * used across UI components.  Most bezier drawing is in
 * ConnectionComponent; this header is for any extra helpers.
 */

#pragma once

#include <cmath>

namespace MathUtils
{
    /** Clamp a value between lo and hi. */
    template <typename T>
    constexpr T clamp (T val, T lo, T hi)
    {
        return val < lo ? lo : (val > hi ? hi : val);
    }

    /** Map a value from [inMin,inMax] to [outMin,outMax]. */
    inline float mapRange (float v, float inMin, float inMax, float outMin, float outMax)
    {
        if (std::abs (inMax - inMin) < 1e-12f) return outMin;
        return outMin + (v - inMin) / (inMax - inMin) * (outMax - outMin);
    }
}
