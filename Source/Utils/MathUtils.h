/*
 * MathUtils.h
 * VoicemeeterHost — 數學工具函式
 *
 * 提供幾何計算、Bezier 曲線及線性代數等輕量輔助函式，
 * 供各 UI 元件使用。主要的 Bezier 繪製邏輯在 ConnectionComponent；
 * 此標頭用於存放其他額外的通用數學輔助函式。
 */

#pragma once

#include <cmath>

namespace MathUtils
{
    /**
     * clamp: 將 val 限制在 [lo, hi] 範圍內。
     * 若 val < lo 回傳 lo；若 val > hi 回傳 hi；否則回傳 val 本身。
     * 使用 constexpr 以支援編譯期求值。
     */
    template <typename T>
    constexpr T clamp (T val, T lo, T hi)
    {
        return val < lo ? lo : (val > hi ? hi : val);
    }

    /**
     * mapRange: 將 v 從輸入範圍 [inMin, inMax] 線性映射到輸出範圍 [outMin, outMax]。
     * 若輸入範圍幾乎為零（< 1e-12），直接回傳 outMin 以防止除以零。
     */
    inline float mapRange (float v, float inMin, float inMax, float outMin, float outMax)
    {
        if (std::abs (inMax - inMin) < 1e-12f) return outMin;
        return outMin + (v - inMin) / (inMax - inMin) * (outMax - outMin);
    }
}
