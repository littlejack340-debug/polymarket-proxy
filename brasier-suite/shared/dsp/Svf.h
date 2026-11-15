#pragma once

#include <array>
#include <cmath>

#include <juce_dsp/juce_dsp.h>

#include "LinearSmoother.h"

namespace brasier::dsp
{
    /**
        Filtre a variables d'etat (SVF) en topologie TPT / trapezoidale
        (methode d'Andrew Simper, d'apres Zavalishin).

        Avantages par rapport a un biquad classique :
          - reste stable et sans artefact quand on module la frequence a chaque
            echantillon (filtre balaye par LFO / enveloppe) ;
          - une seule structure donne LP, HP, BP, notch, peak, allpass, cloche
            et etageres, selon trois coefficients de melange m0, m1, m2.

            v3 = x - ic2 ; v1 = a1*ic1 + a2*v3 ; v2 = ic2 + a2*ic1 + a3*v3
            ic1 = 2*v1 - ic1 ; ic2 = 2*v2 - ic2
            y  = m0*x + m1*v1 + m2*v2
    */
    enum class SvfType { lowpass, highpass, bandpass, notch, peak, allpass, bell, lowShelf, highShelf };

    struct SvfCoeffs
    {
        float g = 0.0f, k = 1.4142f;
        float a1 = 1.0f, a2 = 0.0f, a3 = 0.0f;
        float m0 = 1.0f, m1 = 0.0f, m2 = 0.0f;   // par defaut : passe-tout neutre (y = x)
    };

    /** Calcule les coefficients. freq est bornee juste sous Nyquist. */
    inline SvfCoeffs makeSvfCoeffs (SvfType type, double sampleRate, float freq, float q, float gainDb = 0.0f) noexcept
    {
        SvfCoeffs c;
        const double nyquistLimit = 0.4999 * sampleRate;
        const double f = juce::jlimit (1.0, nyquistLimit, static_cast<double> (freq));
        double g = std::tan (juce::MathConstants<double>::pi * f / sampleRate);
        double k = 1.0 / juce::jmax (0.025, static_cast<double> (q));
        const double A = std::pow (10.0, gainDb / 40.0);

        switch (type)
        {
            case SvfType::lowpass:   c.m0 = 0.0f; c.m1 = 0.0f; c.m2 = 1.0f; break;
            case SvfType::bandpass:  c.m0 = 0.0f; c.m1 = 1.0f; c.m2 = 0.0f; break;
            case SvfType::highpass:  c.m0 = 1.0f; c.m1 = static_cast<float> (-k); c.m2 = -1.0f; break;
            case SvfType::notch:     c.m0 = 1.0f; c.m1 = static_cast<float> (-k); c.m2 = 0.0f; break;
            case SvfType::peak:      c.m0 = 1.0f; c.m1 = static_cast<float> (-k); c.m2 = -2.0f; break;
            case SvfType::allpass:   c.m0 = 1.0f; c.m1 = static_cast<float> (-2.0 * k); c.m2 = 0.0f; break;
            case SvfType::bell:
                k = 1.0 / (juce::jmax (0.025, static_cast<double> (q)) * A);
                c.m0 = 1.0f; c.m1 = static_cast<float> (k * (A * A - 1.0)); c.m2 = 0.0f;
                break;
            case SvfType::lowShelf:
                g /= std::sqrt (A);
                c.m0 = 1.0f; c.m1 = static_cast<float> (k * (A - 1.0)); c.m2 = static_cast<float> (A * A - 1.0);
                break;
            case SvfType::highShelf:
                g *= std::sqrt (A);
                c.m0 = static_cast<float> (A * A); c.m1 = static_cast<float> (k * (1.0 - A) * A); c.m2 = static_cast<float> (1.0 - A * A);
                break;
        }

        const double a1 = 1.0 / (1.0 + g * (g + k));
        c.g = static_cast<float> (g);
        c.k = static_cast<float> (k);
        c.a1 = static_cast<float> (a1);
        c.a2 = static_cast<float> (g * a1);
        c.a3 = static_cast<float> (g * g * a1);
        return c;
    }

    /** Etat d'un canal de SVF. */
    struct SvfState
    {
        float ic1 = 0.0f, ic2 = 0.0f;

        void reset() noexcept { ic1 = ic2 = 0.0f; }

        /** Sorties brutes (v1 = passe-bande "normalise k", v2 = passe-bas). */
        inline void tick (const SvfCoeffs& c, float x, float& v1, float& v2) noexcept
        {
            const float v3 = x - ic2;
            v1 = c.a1 * ic1 + c.a2 * v3;
            v2 = ic2 + c.a2 * ic1 + c.a3 * v3;
            ic1 = 2.0f * v1 - ic1;
            ic2 = 2.0f * v2 - ic2;
        }

        inline float process (const SvfCoeffs& c, float x) noexcept
        {
            float v1, v2;
            tick (c, x, v1, v2);
            return c.m0 * x + c.m1 * v1 + c.m2 * v2;
        }

        /** Anti-denormaux : a appeler en fin de bloc. */
        void flush() noexcept
        {
            if (std::abs (ic1) < 1.0e-15f) ic1 = 0.0f;
            if (std::abs (ic2) < 1.0e-15f) ic2 = 0.0f;
        }
    };

    /**
        SVF stereo dont la frequence, le Q et le gain sont lisses (frequence lissee
        en echelle logarithmique) : aucun zipper noise quand on tourne un potard.
        Les coefficients ne sont recalcules que pendant les rampes.
    */
    class SmoothedSvf
    {
    public:
        static constexpr int maxChannels = 2;

        void prepare (double newSampleRate, SvfType newType, float freq, float q, float gainDb = 0.0f) noexcept
        {
            type = newType;
            sampleRate = newSampleRate;
            logFreq.prepare (sampleRate, 0.02);
            resonance.prepare (sampleRate, 0.02);
            gain.prepare (sampleRate, 0.02);
            logFreq.setCurrentAndTarget (std::log2 (juce::jmax (1.0f, freq)));
            resonance.setCurrentAndTarget (q);
            gain.setCurrentAndTarget (gainDb);
            updateCoeffs();
            reset();
        }

        /** Change la frequence d'echantillonnage (oversampling) en gardant les reglages. Temps reel. */
        void setSampleRate (double newSampleRate) noexcept
        {
            sampleRate = newSampleRate;
            logFreq.prepare (sampleRate, 0.02);
            resonance.prepare (sampleRate, 0.02);
            gain.prepare (sampleRate, 0.02);
            updateCoeffs();
        }

        void setType (SvfType newType) noexcept  { if (newType != type) { type = newType; updateCoeffs(); } }
        void setFrequency (float hz) noexcept    { logFreq.setTarget (std::log2 (juce::jmax (1.0f, hz))); }
        void setQ (float q) noexcept             { resonance.setTarget (q); }
        void setGainDb (float db) noexcept       { gain.setTarget (db); }

        void reset() noexcept
        {
            for (auto& s : states)
                s.reset();
        }

        bool isSmoothing() const noexcept
        {
            return logFreq.isSmoothing() || resonance.isSmoothing() || gain.isSmoothing();
        }

        /** Avance les rampes d'un echantillon (a appeler une fois par echantillon, avant processSample). */
        inline void tickParameters() noexcept
        {
            if (isSmoothing())
            {
                logFreq.getNext();
                resonance.getNext();
                gain.getNext();
                updateCoeffs();
            }
        }

        inline float processSample (int channel, float x) noexcept
        {
            return states[static_cast<size_t> (channel)].process (coeffs, x);
        }

        void process (juce::dsp::AudioBlock<float> block) noexcept
        {
            const auto numChannels = juce::jmin (block.getNumChannels(), static_cast<size_t> (maxChannels));
            const auto numSamples = block.getNumSamples();

            if (! isSmoothing())
            {
                for (size_t ch = 0; ch < numChannels; ++ch)
                {
                    auto* data = block.getChannelPointer (ch);
                    auto& st = states[ch];
                    for (size_t i = 0; i < numSamples; ++i)
                        data[i] = st.process (coeffs, data[i]);
                }
            }
            else
            {
                for (size_t i = 0; i < numSamples; ++i)
                {
                    tickParameters();
                    for (size_t ch = 0; ch < numChannels; ++ch)
                    {
                        auto* data = block.getChannelPointer (ch);
                        data[i] = states[ch].process (coeffs, data[i]);
                    }
                }
            }

            for (auto& s : states)
                s.flush();
        }

        const SvfCoeffs& getCoeffs() const noexcept { return coeffs; }
        float getFrequency() const noexcept         { return std::exp2 (logFreq.getCurrent()); }

    private:
        void updateCoeffs() noexcept
        {
            coeffs = makeSvfCoeffs (type, sampleRate, std::exp2 (logFreq.getCurrent()),
                                    resonance.getCurrent(), gain.getCurrent());
        }

        SvfType type = SvfType::lowpass;
        double sampleRate = 44100.0;
        LinearSmoother<float> logFreq, resonance, gain;
        SvfCoeffs coeffs;
        std::array<SvfState, maxChannels> states;
    };
}
