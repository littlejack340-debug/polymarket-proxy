#include "Reverbs.h"
#include "MathUtils.h"

namespace brasier::dsp
{
    //==============================================================================
    // Plaque (Dattorro). Longueurs d'origine donnees pour 29761 Hz.

    int PlateReverb::scaled (int samplesAt29761) const noexcept
    {
        return juce::jmax (1, static_cast<int> (std::lround (samplesAt29761 * sampleRate / 29761.0)));
    }

    void PlateReverb::prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;

        const int inputLengths[] = { 142, 107, 379, 277 };
        const float inputCoeffs[] = { 0.75f, 0.75f, 0.625f, 0.625f };
        for (size_t i = 0; i < inputDiffusers.size(); ++i)
            inputDiffusers[i].prepare (scaled (inputLengths[i]), inputCoeffs[i]);

        excursion = static_cast<float> (16.0 * sampleRate / 29761.0);
        modulatedLeftLength = static_cast<float> (scaled (672));
        modulatedRightLength = static_cast<float> (scaled (908));
        modulatedLeft.prepare (scaled (672), -0.7f, static_cast<int> (excursion) + 4);
        modulatedRight.prepare (scaled (908), -0.7f, static_cast<int> (excursion) + 4);
        allpassLeft.prepare (scaled (1800), 0.5f);
        allpassRight.prepare (scaled (2656), 0.5f);

        lengthLeft1 = scaled (4453);  delayLeft1.prepare (lengthLeft1);
        lengthLeft2 = scaled (3720);  delayLeft2.prepare (lengthLeft2);
        lengthRight1 = scaled (4217); delayRight1.prepare (lengthRight1);
        lengthRight2 = scaled (3163); delayRight2.prepare (lengthRight2);

        tapsLeft  = { scaled (266), scaled (2974), scaled (1913), scaled (1996), scaled (1990), scaled (187), scaled (1066) };
        tapsRight = { scaled (353), scaled (3627), scaled (1228), scaled (2673), scaled (2111), scaled (335), scaled (121) };

        modLeft.prepare (sampleRate, 0xAB12u);
        modRight.prepare (sampleRate, 0xCD34u);
        modLeft.setFrequency (0.93);
        modRight.setFrequency (0.77);
        modRight.reset (0.25);

        reset();
    }

    void PlateReverb::reset() noexcept
    {
        for (auto& d : inputDiffusers) d.reset();
        for (auto* a : { &modulatedLeft, &modulatedRight, &allpassLeft, &allpassRight })
            a->reset();
        for (auto* d : { &delayLeft1, &delayLeft2, &delayRight1, &delayRight2 })
            d->reset();
        dampLeft = dampRight = 0.0f;
    }

    void PlateReverb::setDecaySeconds (float rt60) noexcept
    {
        // Un tour complet du reservoir (~0,73 s a 29761 Hz) applique le gain "decay" 4 fois
        // (2 par branche) : g^4 doit valoir -60 dB apres rt60 secondes.
        constexpr double loopSeconds = (672.0 + 4453.0 + 1800.0 + 3720.0 + 908.0 + 4217.0 + 2656.0 + 3163.0) / 29761.0;
        decay = static_cast<float> (juce::jmin (0.995, std::pow (10.0, -3.0 * (loopSeconds / 4.0) / juce::jmax (0.1f, rt60))));
    }

    void PlateReverb::process (float input, float& outLeft, float& outRight) noexcept
    {
        float x = input;
        for (auto& d : inputDiffusers)
            x = d.process (x);

        const float leftIn  = x + decay * delayRight2.tap (lengthRight2);
        const float rightIn = x + decay * delayLeft2.tap (lengthLeft2);

        // Branche gauche
        float t = modulatedLeft.processWithDelay (leftIn, modulatedLeftLength + excursion * modLeft.next());
        delayLeft1.push (t);
        t = delayLeft1.tap (lengthLeft1);
        dampLeft = t + damping * (dampLeft - t);
        t = allpassLeft.process (dampLeft * decay);
        delayLeft2.push (t);

        // Branche droite
        t = modulatedRight.processWithDelay (rightIn, modulatedRightLength + excursion * modRight.next());
        delayRight1.push (t);
        t = delayRight1.tap (lengthRight1);
        dampRight = t + damping * (dampRight - t);
        t = allpassRight.process (dampRight * decay);
        delayRight2.push (t);

        if (std::abs (dampLeft) < 1.0e-15f)  dampLeft = 0.0f;
        if (std::abs (dampRight) < 1.0e-15f) dampRight = 0.0f;

        outLeft = 0.6f * (  delayRight1.tap (tapsLeft[0]) + delayRight1.tap (tapsLeft[1])
                          - allpassRight.tap (tapsLeft[2]) + delayRight2.tap (tapsLeft[3])
                          - delayLeft1.tap (tapsLeft[4])  - allpassLeft.tap (tapsLeft[5])
                          - delayLeft2.tap (tapsLeft[6]));

        outRight = 0.6f * (  delayLeft1.tap (tapsRight[0]) + delayLeft1.tap (tapsRight[1])
                           - allpassLeft.tap (tapsRight[2]) + delayLeft2.tap (tapsRight[3])
                           - delayRight1.tap (tapsRight[4]) - allpassRight.tap (tapsRight[5])
                           - delayRight2.tap (tapsRight[6]));
    }

    //==============================================================================
    // Ressort

    void SpringReverb::prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        stretch = juce::jmax (1, static_cast<int> (std::lround (sampleRate / 12000.0)));

        const double lengthsMs[numSprings] = { 41.3, 47.9, 55.1 };
        for (int s = 0; s < numSprings; ++s)
        {
            auto& spring = springs[static_cast<size_t> (s)];
            spring.delaySamples = static_cast<float> (lengthsMs[s] * 0.001 * sampleRate);
            spring.loop.prepare (static_cast<int> (spring.delaySamples + 0.002 * sampleRate) + 8);
            spring.stageMemory.assign (static_cast<size_t> (numStages * stretch), 0.0f);
            spring.highpass.setCutoff (sampleRate, 70.0);
            spring.wander.prepare (sampleRate, static_cast<uint32_t> (0x5151 + 77 * s));
            spring.wander.setShape (Lfo::Shape::smoothRandom);
            spring.wander.setFrequency (0.5 + 0.13 * s);
        }

        setDamping (0.4f);
        setDecaySeconds (2.0f);
        reset();
    }

    void SpringReverb::reset() noexcept
    {
        for (auto& s : springs)
        {
            s.loop.reset();
            std::fill (s.stageMemory.begin(), s.stageMemory.end(), 0.0f);
            s.lastOutput = 0.0f;
            s.lowpass.reset();
            s.highpass.reset();
        }
        stagePosition = 0;
    }

    void SpringReverb::setDecaySeconds (float rt60) noexcept
    {
        for (auto& s : springs)
        {
            const double loopSeconds = s.delaySamples / sampleRate;
            s.feedback = static_cast<float> (juce::jmin (0.985, std::pow (10.0, -3.0 * loopSeconds / juce::jmax (0.1f, rt60))));
        }
    }

    void SpringReverb::setDamping (float amount01) noexcept
    {
        const double cutoff = 1800.0 + (1.0 - juce::jlimit (0.0f, 1.0f, amount01)) * 6000.0;
        for (auto& s : springs)
            s.lowpass.setCutoff (sampleRate, cutoff);
    }

    void SpringReverb::process (float input, float& outLeft, float& outRight) noexcept
    {
        const float a = coefficient;
        const float wanderDepth = static_cast<float> (0.0002 * sampleRate);   // +/-0,2 ms
        float outs[numSprings];

        for (int s = 0; s < numSprings; ++s)
        {
            auto& spring = springs[static_cast<size_t> (s)];

            spring.loop.push (0.5f * input + spring.feedback * spring.lastOutput);
            float y = spring.loop.tapFractional (spring.delaySamples + wanderDepth * spring.wander.next());

            // Dispersion : chaine de passe-tout etires (forme directe II, memoire K par etage)
            float* memory = spring.stageMemory.data();
            for (int stage = 0; stage < numStages; ++stage)
            {
                float& slot = memory[stage * stretch + stagePosition];
                const float wDelayed = slot;           // w[n-K]
                const float w = y - a * wDelayed;
                y = a * w + wDelayed;
                slot = w;
            }

            y = spring.highpass.processHighpass (spring.lowpass.processLowpass (y));
            spring.lowpass.flush();
            spring.highpass.flush();
            if (std::abs (y) < 1.0e-15f)
                y = 0.0f;

            spring.lastOutput = y;
            outs[s] = y;
        }

        stagePosition = (stagePosition + 1) % stretch;

        outLeft  = 0.7f * (outs[0] + 0.6f * outs[2]);
        outRight = 0.7f * (outs[1] + 0.6f * outs[2]);
    }
}
