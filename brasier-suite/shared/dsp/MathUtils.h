#pragma once

#include <cmath>
#include <numbers>

namespace brasier::dsp
{
    inline constexpr float pi    = std::numbers::pi_v<float>;
    inline constexpr float twoPi = 2.0f * std::numbers::pi_v<float>;
    inline constexpr float halfPi = 0.5f * std::numbers::pi_v<float>;

    /** Conversion dB -> gain lineaire (en dessous de minusInfinityDb on renvoie 0). */
    inline float dbToGain (float db, float minusInfinityDb = -100.0f) noexcept
    {
        return db > minusInfinityDb ? std::pow (10.0f, db * 0.05f) : 0.0f;
    }

    /** Conversion gain lineaire -> dB (borne basse a minusInfinityDb). */
    inline float gainToDb (float gain, float minusInfinityDb = -100.0f) noexcept
    {
        return gain > 0.0f ? std::fmax (minusInfinityDb, 20.0f * std::log10 (gain)) : minusInfinityDb;
    }

    /** Coefficient d'un filtre a un pole pour une constante de temps donnee
        (atteint ~63 % de la cible apres timeSeconds). */
    inline float onePoleCoefficient (double sampleRate, double timeSeconds) noexcept
    {
        if (timeSeconds <= 0.0 || sampleRate <= 0.0)
            return 1.0f;

        return static_cast<float> (1.0 - std::exp (-1.0 / (timeSeconds * sampleRate)));
    }
}
