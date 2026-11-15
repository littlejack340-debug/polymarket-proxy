#pragma once

#include <cmath>

#include <juce_core/juce_core.h>

namespace brasier::dsp
{
    /**
        Filtre a un pole (6 dB/octave) en topologie TPT : passe-bas et passe-haut
        simultanes. Utilise pour les DC blockers, les lissages, les filtres
        "doux" dans les boucles de feedback (bande magnetique, BBD...).
    */
    class OnePole
    {
    public:
        void setCutoff (double sampleRate, double freq) noexcept
        {
            const double f = juce::jlimit (0.1, 0.49 * sampleRate, freq);
            const double g = std::tan (juce::MathConstants<double>::pi * f / sampleRate);
            G = static_cast<float> (g / (1.0 + g));
        }

        void reset() noexcept { s = 0.0f; }

        inline float processLowpass (float x) noexcept
        {
            const float v = G * (x - s);
            const float lp = v + s;
            s = lp + v;
            return lp;
        }

        inline float processHighpass (float x) noexcept
        {
            return x - processLowpass (x);
        }

        void flush() noexcept
        {
            if (std::abs (s) < 1.0e-15f)
                s = 0.0f;
        }

    private:
        float G = 0.5f;
        float s = 0.0f;
    };

    /** Coupe le continu (DC) : passe-haut ~10 Hz. Indispensable apres une
        saturation asymetrique (tube, diode) qui decale le signal. */
    class DcBlocker
    {
    public:
        void prepare (double sampleRate, double freq = 10.0) noexcept { filter.setCutoff (sampleRate, freq); filter.reset(); }
        void reset() noexcept                                        { filter.reset(); }
        inline float process (float x) noexcept                     { return filter.processHighpass (x); }
        void flush() noexcept                                        { filter.flush(); }

    private:
        OnePole filter;
    };
}
