#pragma once

#include <array>
#include <cmath>

#include "Svf.h"
#include "Waveshapers.h"

namespace brasier::dsp
{
    /**
        Filtre multimode resonant a saturation (SVF TPT non lineaire).

        Le premier etage est un SVF dont l'integrateur passe-bande est ecrete en
        douceur (tanh) : quand la resonance monte, l'amplitude du "sifflement"
        est limitee par la saturation au lieu d'exploser. A fond, le filtre
        auto-oscille proprement, comme un filtre analogique pousse.

        Pente 12 dB/oct = un etage ; 24 dB/oct = un second etage lineaire de
        meme frequence (Q Butterworth) en cascade.

        Les coefficients sont fournis par echantillon : la frequence peut etre
        modulee a l'audio (LFO, enveloppe) sans artefact.
    */
    class ResonantFilter
    {
    public:
        enum class Mode { lowpass = 0, highpass, bandpass, notch };

        static juce::StringArray getModeNames() { return { "Low Pass", "High Pass", "Band Pass", "Notch" }; }

        struct Coeffs
        {
            float k = 1.414f, a1 = 1.0f, a2 = 0.0f, a3 = 0.0f;       // etage resonant
            float k2 = 1.414f, b1 = 1.0f, b2 = 0.0f, b3 = 0.0f;      // second etage (Butterworth)
        };

        /** g = tan(pi * fc / fs) ; k = 1 / Q. */
        static inline Coeffs makeCoeffs (float g, float k) noexcept
        {
            Coeffs c;
            c.k = k;
            c.a1 = 1.0f / (1.0f + g * (g + k));
            c.a2 = g * c.a1;
            c.a3 = g * c.a2;

            c.k2 = 1.41421356f;
            c.b1 = 1.0f / (1.0f + g * (g + c.k2));
            c.b2 = g * c.b1;
            c.b3 = g * c.b2;
            return c;
        }

        void reset() noexcept
        {
            for (auto& s : first)  s = {};
            for (auto& s : second) s = {};
        }

        inline float process (int channel, const Coeffs& c, float x, Mode mode, bool steep) noexcept
        {
            auto& s = first[static_cast<size_t> (channel)];

            // Etage 1, non lineaire
            const float v3 = x - s.ic2;
            const float v1 = c.a1 * s.ic1 + c.a2 * v3;
            const float v2 = s.ic2 + c.a2 * s.ic1 + c.a3 * v3;
            s.ic1 = saturate (2.0f * v1 - s.ic1);
            s.ic2 = 2.0f * v2 - s.ic2;

            float y = output (mode, x, v1, v2, c.k);

            if (steep)
            {
                auto& t = second[static_cast<size_t> (channel)];
                const float w3 = y - t.ic2;
                const float w1 = c.b1 * t.ic1 + c.b2 * w3;
                const float w2 = t.ic2 + c.b2 * t.ic1 + c.b3 * w3;
                t.ic1 = 2.0f * w1 - t.ic1;
                t.ic2 = 2.0f * w2 - t.ic2;
                y = output (mode, y, w1, w2, c.k2);
            }

            return y;
        }

        void flush() noexcept
        {
            for (auto* bank : { &first, &second })
                for (auto& s : *bank)
                {
                    if (std::abs (s.ic1) < 1.0e-15f) s.ic1 = 0.0f;
                    if (std::abs (s.ic2) < 1.0e-15f) s.ic2 = 0.0f;
                }
        }

    private:
        struct State { float ic1 = 0.0f, ic2 = 0.0f; };

        static inline float saturate (float v) noexcept
        {
            constexpr float limit = 2.0f;
            return limit * shapers::fastTanh (v * (1.0f / limit));
        }

        static inline float output (Mode mode, float x, float v1, float v2, float k) noexcept
        {
            switch (mode)
            {
                case Mode::lowpass:  return v2;
                case Mode::highpass: return x - k * v1 - v2;
                case Mode::bandpass: return k * v1;          // gain unitaire au centre
                case Mode::notch:    return x - k * v1;
            }
            return x;
        }

        std::array<State, 2> first {}, second {};
    };
}
