#include "AutoGain.h"
#include "MathUtils.h"

namespace brasier::dsp
{
    void AutoGain::prepare (double sampleRate) noexcept
    {
        powerCoeff = onePoleCoefficient (sampleRate, 0.4);
        gainCoeff  = onePoleCoefficient (sampleRate, 0.15);
        reset();
    }

    void AutoGain::reset() noexcept
    {
        refPower = wetPower = 0.0f;
        measuredGain = 1.0f;
        currentGain = 1.0f;
        counter = 0;
    }

    void AutoGain::process (const juce::dsp::AudioBlock<const float>& reference,
                            juce::dsp::AudioBlock<float>& wet) noexcept
    {
        const auto numSamples  = juce::jmin (reference.getNumSamples(), wet.getNumSamples());
        const auto numChannels = juce::jmin (reference.getNumChannels(), wet.getNumChannels());

        if (numChannels == 0)
            return;

        const float channelScale = 1.0f / static_cast<float> (numChannels);

        if (staticMode)
        {
            const float target = enabled ? staticGain : 1.0f;
            for (size_t i = 0; i < numSamples; ++i)
            {
                currentGain += gainCoeff * (target - currentGain);
                for (size_t ch = 0; ch < numChannels; ++ch)
                    wet.getChannelPointer (ch)[i] *= currentGain;
            }
            return;
        }

        for (size_t i = 0; i < numSamples; ++i)
        {
            // Puissance instantanee moyennee sur les canaux
            float refSq = 0.0f, wetSq = 0.0f;

            for (size_t ch = 0; ch < numChannels; ++ch)
            {
                const float r = reference.getSample (static_cast<int> (ch), static_cast<int> (i));
                const float w = wet.getSample (static_cast<int> (ch), static_cast<int> (i));
                refSq += r * r;
                wetSq += w * w;
            }

            refPower += powerCoeff * (refSq * channelScale - refPower);
            wetPower += powerCoeff * (wetSq * channelScale - wetPower);

            if (++counter >= updateInterval)
            {
                counter = 0;

                if (refPower > silenceThreshold && wetPower > silenceThreshold)
                    measuredGain = juce::jlimit (minGain, maxGain, std::sqrt (refPower / wetPower));
            }

            const float target = enabled ? measuredGain : 1.0f;
            currentGain += gainCoeff * (target - currentGain);

            for (size_t ch = 0; ch < numChannels; ++ch)
                wet.getChannelPointer (ch)[i] *= currentGain;
        }

        // Evite les denormaux quand le signal s'eteint
        if (refPower < 1.0e-20f) refPower = 0.0f;
        if (wetPower < 1.0e-20f) wetPower = 0.0f;
    }
}
