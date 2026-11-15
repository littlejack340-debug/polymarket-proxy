#pragma once

#include <cmath>
#include <cstdint>

#include <juce_core/juce_core.h>

#include "MathUtils.h"

namespace brasier::dsp
{
    /** Generateur pseudo-aleatoire xorshift32 : deterministe, sans allocation,
        utilisable dans le thread audio (contrairement a std::random_device...). */
    class FastRandom
    {
    public:
        explicit FastRandom (uint32_t seed = 0x9E3779B9u) noexcept : state (seed != 0 ? seed : 1u) {}

        void seed (uint32_t s) noexcept { state = s != 0 ? s : 1u; }

        inline uint32_t nextInt() noexcept
        {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            return state;
        }

        /** Uniforme dans [-1, 1]. */
        inline float nextBipolar() noexcept
        {
            return static_cast<float> (nextInt()) * (2.0f / 4294967295.0f) - 1.0f;
        }

    private:
        uint32_t state;
    };

    /**
        LFO a phase continue. Formes : sinus, triangle, dent de scie montante et
        descendante, carre (adouci), sample & hold et aleatoire lisse.

        La phase peut etre recalee sur la position du transport de l'hote
        (syncToPpq) pour que les modulations synchronisees restent calees sur la
        grille quand on relance la lecture.
    */
    class Lfo
    {
    public:
        enum class Shape { sine = 0, triangle, sawUp, sawDown, square, sampleHold, smoothRandom };

        static juce::StringArray getShapeNames()
        {
            return { "Sinus", "Triangle", "Scie +", "Scie -", "Carre", "S&H", "Aleatoire" };
        }

        void prepare (double newSampleRate, uint32_t seed = 0x1234567u) noexcept
        {
            sampleRate = newSampleRate;
            rng.seed (seed);
            reset();
        }

        void reset (double startPhase = 0.0) noexcept
        {
            phase = startPhase - std::floor (startPhase);
            held = target = rng.nextBipolar();
            smoothed = held;
        }

        void setShape (Shape s) noexcept        { shape = s; }
        void setFrequency (double hz) noexcept   { increment = juce::jlimit (0.0, 0.5, hz / sampleRate); }
        double getPhase() const noexcept         { return phase; }
        void setPhase (double p) noexcept        { phase = p - std::floor (p); }

        /** Recale la phase sur le transport : cyclesPerQuarter = nombre de periodes par noire. */
        void syncToPpq (double ppqPosition, double cyclesPerQuarter, double phaseOffset = 0.0) noexcept
        {
            const double p = ppqPosition * cyclesPerQuarter + phaseOffset;
            phase = p - std::floor (p);
        }

        /** Echantillon suivant dans [-1, 1]. */
        inline float next() noexcept
        {
            const float value = valueAt (phase);
            phase += increment;
            if (phase >= 1.0)
            {
                phase -= 1.0;
                held = target;
                target = rng.nextBipolar();
            }
            return value;
        }

        /** Avance de n echantillons sans calculer la forme (pour un LFO lu a cadence reduite). */
        void skip (int n) noexcept
        {
            for (int i = 0; i < n; ++i)
                next();
        }

    private:
        inline float valueAt (double p) noexcept
        {
            const auto x = static_cast<float> (p);

            switch (shape)
            {
                case Shape::sine:      return std::sin (twoPi * x);
                case Shape::triangle:  return x < 0.5f ? 4.0f * x - 1.0f : 3.0f - 4.0f * x;
                case Shape::sawUp:     return 2.0f * x - 1.0f;
                case Shape::sawDown:   return 1.0f - 2.0f * x;
                case Shape::square:
                {
                    // Carre aux fronts legerement arrondis (pas de clic en modulation d'amplitude)
                    constexpr float edge = 0.02f;
                    if (x < edge)            return x / edge;
                    if (x < 0.5f - edge)     return 1.0f;
                    if (x < 0.5f + edge)     return (0.5f - x) / edge;
                    if (x < 1.0f - edge)     return -1.0f;
                    return (x - 1.0f) / edge;
                }
                case Shape::sampleHold:   return held;
                case Shape::smoothRandom:
                {
                    // Interpolation cosinus entre deux valeurs aleatoires
                    const float w = 0.5f - 0.5f * std::cos (pi * x);
                    return held + (target - held) * w;
                }
            }

            return 0.0f;
        }

        double sampleRate = 44100.0;
        double phase = 0.0, increment = 0.0;
        Shape shape = Shape::sine;
        FastRandom rng;
        float held = 0.0f, target = 0.0f, smoothed = 0.0f;
    };
}
