#include "AnalogModulator.h"
#include "MathUtils.h"
#include "Waveshapers.h"

namespace brasier::dsp
{
    namespace
    {
        constexpr float chorusBaseMs = 12.0f;
        constexpr float chorusDepthMs = 6.0f;
        constexpr float flangerMinMs = 0.25f;
        constexpr float flangerDepthMs = 5.0f;
        constexpr float phaserLowHz = 150.0f;
        constexpr float phaserSpanOctaves = 5.5f;
        constexpr double switchSeconds = 0.005;

        inline float saturate (float x, float drive) noexcept
        {
            return shapers::fastTanh (x * drive) / drive;
        }
    }

    void AnalogModulator::prepare (double baseSampleRate, int maxBlockSize, int maxOversamplingFactor)
    {
        baseRate = baseSampleRate;

        for (size_t s = 0; s < lfos.size(); ++s)
            lfos[s].prepare (baseRate, static_cast<uint32_t> (0xC0FFEE + 131 * s));
        driftLfo.prepare (baseRate, 0xD21F7u);
        driftLfo.setShape (Lfo::Shape::smoothRandom);
        driftLfo.setFrequency (0.4);

        for (auto& t : lfoTable)
            t.assign (static_cast<size_t> (maxBlockSize + 1), 0.0f);

        const double maxDelayMs = chorusBaseMs + 2.0 * 1.3 + chorusDepthMs + 2.0;
        delay.prepare (2, static_cast<int> (std::ceil (maxDelayMs * 0.001 * baseRate * maxOversamplingFactor)) + 16);

        for (auto* s : { &depth, &feedback, &character })
            s->prepare (baseRate, 0.02);

        depth.setCurrentAndTarget (settings.depth);
        feedback.setCurrentAndTarget (settings.feedback);
        character.setCurrentAndTarget (settings.character);

        opticalAttack = onePoleCoefficient (baseRate, 0.003);
        opticalRelease = onePoleCoefficient (baseRate, 0.015);
        coeffRate = 0.0;
        activeMode = settings.mode;
        reset();
    }

    void AnalogModulator::reset() noexcept
    {
        delay.reset();
        for (auto& f : bbdFilters) f.reset();
        for (auto& st : phaserStates) st.fill (0.0f);
        feedbackSample.fill (0.0f);
        opticalGain.fill (1.0f);
        modeGain = 1.0f;
        switching = false;
        switchReady = false;
        appliedSpread = settings.spread;

        for (size_t s = 0; s < lfos.size(); ++s)
        {
            const int voice = static_cast<int> (s) % numVoices;
            const int channel = static_cast<int> (s) / numVoices;
            lfos[s].reset (voice / static_cast<double> (numVoices) + channel * 0.5 * settings.spread);
        }

        for (auto& t : lfoTable)
            std::fill (t.begin(), t.end(), 0.0f);
        tableLength = 0;
    }

    void AnalogModulator::setSettings (const Settings& s) noexcept
    {
        settings = s;
        depth.setTarget (juce::jlimit (0.0f, 1.0f, s.depth));
        feedback.setTarget (juce::jlimit (-0.95f, 0.95f, s.feedback));
        character.setTarget (juce::jlimit (0.0f, 1.0f, s.character));

        for (auto& l : lfos)
        {
            l.setShape (s.shape);
            l.setFrequency (s.rateHz);
        }

        // Spread modifie en cours de lecture : on recale le LFO droit sur le gauche
        if (std::abs (s.spread - appliedSpread) > 1.0e-4f)
        {
            for (int v = 0; v < numVoices; ++v)
                lfos[static_cast<size_t> (numVoices + v)].setPhase (lfos[static_cast<size_t> (v)].getPhase() + 0.5 * s.spread);
            appliedSpread = s.spread;
        }

        if (s.mode != activeMode && ! switchReady)
            switching = true;
    }

    void AnalogModulator::syncToPpq (double ppqPosition, double cyclesPerQuarter) noexcept
    {
        for (size_t s = 0; s < lfos.size(); ++s)
        {
            const int voice = static_cast<int> (s) % numVoices;
            const int channel = static_cast<int> (s) / numVoices;
            lfos[s].syncToPpq (ppqPosition, cyclesPerQuarter,
                               voice / static_cast<double> (numVoices) + channel * 0.5 * settings.spread);
        }
    }

    void AnalogModulator::computeModulation (int numSamples, double oversampledRate) noexcept
    {
        // Bascule de mode a la frontiere de bloc : les tables du bloc seront calculees pour le nouveau mode
        if (switchReady)
        {
            switchReady = false;
            activeMode = settings.mode;
            delay.reset();
            for (auto& f : bbdFilters) f.reset();
            for (auto& st : phaserStates) st.fill (0.0f);
            feedbackSample.fill (0.0f);
            opticalGain.fill (1.0f);
            for (auto& t : lfoTable)
                std::fill (t.begin(), t.end(), 0.0f);
            tableLength = 0;
        }

        // L'entree 0 de chaque table reprend la derniere valeur du bloc precedent (interpolation continue)
        for (auto& t : lfoTable)
            t[0] = t[static_cast<size_t> (tableLength)];

        for (int i = 1; i <= numSamples; ++i)
        {
            const float d = depth.getNext();
            const float drift = driftLfo.next() * character.getNext() * 0.08f;
            const auto idx = static_cast<size_t> (i);

            for (size_t s = 0; s < lfos.size(); ++s)
                lfoTable[s][idx] = juce::jlimit (-1.0f, 1.0f, lfos[s].next() + drift);

            if (activeMode == Mode::tremolo)
            {
                // Gain du tremolo avec l'inertie d'une cellule optique
                for (int ch = 0; ch < 2; ++ch)
                {
                    auto& g = opticalGain[static_cast<size_t> (ch)];
                    const float target = 1.0f - d * (0.5f - 0.5f * lfoTable[static_cast<size_t> (ch * numVoices)][idx]);
                    g += (target < g ? opticalAttack : opticalRelease) * (target - g);
                    lfoTable[static_cast<size_t> (ch * numVoices + 2)][idx] = g;   // slot 2 = gain optique
                }
            }
            else
            {
                // Profondeur appliquee directement aux valeurs utilisees par le chorus / flanger / phaser
                for (size_t s = 0; s < lfos.size(); ++s)
                    lfoTable[s][idx] *= d;

                if (activeMode == Mode::phaser)
                {
                    // Coefficient G du passe-tout (1 tangente par echantillon de base et par canal)
                    const auto maxHz = static_cast<float> (0.45 * oversampledRate);
                    for (int ch = 0; ch < 2; ++ch)
                    {
                        const float lfo = lfoTable[static_cast<size_t> (ch * numVoices)][idx];
                        const float hz = juce::jmin (maxHz, phaserLowHz * std::exp2 (phaserSpanOctaves * (0.5f + 0.5f * lfo)));
                        const float g = std::tan (pi * hz / static_cast<float> (oversampledRate));
                        lfoTable[static_cast<size_t> (ch * numVoices + 1)][idx] = g / (1.0f + g);
                    }
                }
            }
        }

        feedback.skip (numSamples);
        tableLength = numSamples;
    }

    inline float AnalogModulator::lfoValue (int slot, size_t upIndex, int factor) const noexcept
    {
        const auto i = upIndex / static_cast<size_t> (factor);
        const float frac = static_cast<float> (upIndex % static_cast<size_t> (factor) + 1) / static_cast<float> (factor);
        const auto& t = lfoTable[static_cast<size_t> (slot)];
        return t[i] + (t[i + 1] - t[i]) * frac;
    }

    void AnalogModulator::updateCharacter (double oversampledRate) noexcept
    {
        const float c = character.getCurrent();
        if (juce::exactlyEqual (oversampledRate, coeffRate) && std::abs (c - coeffCharacter) < 0.005f)
            return;

        coeffRate = oversampledRate;
        coeffCharacter = c;
        // Puce BBD : bande passante qui se reduit avec le "caractere"
        bbdCoeffs = makeSvfCoeffs (SvfType::lowpass, oversampledRate, 18000.0f * (1.0f - c) + 4000.0f * c, 0.6f);
    }

    void AnalogModulator::processOversampled (juce::dsp::AudioBlock<float> up, int factor, double oversampledRate) noexcept
    {
        const auto numUp = up.getNumSamples();
        const int channels = juce::jmin (2, static_cast<int> (up.getNumChannels()));
        const auto msToSamples = static_cast<float> (oversampledRate * 0.001);
        const float c = character.getCurrent();
        const float drive = 1.0f + 3.0f * c;
        const float fb = feedback.getCurrent();
        const float switchStep = static_cast<float> (1.0 / (switchSeconds * oversampledRate));

        updateCharacter (oversampledRate);

        for (size_t j = 0; j < numUp; ++j)
        {
            // Fondu lors d'un changement de mode
            if (switching)
            {
                modeGain -= switchStep;
                if (modeGain <= 0.0f)
                {
                    modeGain = 0.0f;
                    switching = false;
                    switchReady = true;   // la bascule aura lieu au debut du prochain bloc
                }
            }
            else if (modeGain < 1.0f && ! switchReady)
            {
                modeGain = juce::jmin (1.0f, modeGain + switchStep);
            }

            for (int ch = 0; ch < channels; ++ch)
            {
                auto& sample = up.getChannelPointer (static_cast<size_t> (ch))[j];
                const float x = sample;
                float y = x;

                switch (activeMode)
                {
                    case Mode::chorus:
                    {
                        delay.write (ch, saturate (x, drive));
                        float sum = 0.0f;
                        for (int v = 0; v < numVoices; ++v)
                        {
                            const float lfo = lfoValue (ch * numVoices + v, j, factor);
                            const float ms = chorusBaseMs + 1.3f * static_cast<float> (v) + chorusDepthMs * lfo;
                            const float voice = delay.read (ch, juce::jmax (3.0f, ms * msToSamples));
                            sum += bbdFilters[static_cast<size_t> (ch * numVoices + v)].process (bbdCoeffs, voice);
                        }
                        y = 0.75f * x + 0.45f * sum;
                        break;
                    }

                    case Mode::flanger:
                    {
                        const float lfo = lfoValue (ch * numVoices, j, factor);   // deja multiplie par la profondeur
                        const float ms = flangerMinMs + flangerDepthMs * (0.5f + 0.5f * lfo);
                        const float delayed = bbdFilters[static_cast<size_t> (ch * numVoices)].process (
                            bbdCoeffs, delay.read (ch, juce::jmax (3.0f, ms * msToSamples)));
                        delay.write (ch, x + fb * saturate (delayed, drive));
                        y = 0.5f * (x + delayed);
                        break;
                    }

                    case Mode::phaser:
                    {
                        // Coefficient precalcule a la frequence de base (slot 1), interpole
                        const float G = juce::jlimit (0.0f, 0.999f, lfoValue (ch * numVoices + 1, j, factor));

                        float in = x + fb * saturate (feedbackSample[static_cast<size_t> (ch)], drive);
                        for (auto& s : phaserStates[static_cast<size_t> (ch)])
                        {
                            const float v = (in - s) * G;
                            const float lp = v + s;
                            s = lp + v;
                            in = 2.0f * lp - in;   // passe-tout 1er ordre
                        }
                        feedbackSample[static_cast<size_t> (ch)] = in;
                        y = 0.5f * (x + in);
                        break;
                    }

                    case Mode::tremolo:
                    {
                        const float gain = lfoValue (ch * numVoices + 2, j, factor);
                        y = saturate (x * gain, 1.0f + 2.0f * c);
                        break;
                    }
                }

                // Fondu effet -> signal direct -> nouvel effet lors d'un changement de mode
                sample = x + modeGain * (y - x);
            }

            delay.advance();
        }

        for (auto& f : bbdFilters) f.flush();
    }
}
