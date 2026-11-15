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

        // Acces direct aux canaux (mono / stereo)
        const float* ref0 = reference.getChannelPointer (0);
        const float* ref1 = numChannels > 1 ? reference.getChannelPointer (1) : ref0;
        float* wet0 = wet.getChannelPointer (0);
        float* wet1 = numChannels > 1 ? wet.getChannelPointer (1) : nullptr;

        for (size_t i = 0; i < numSamples; ++i)
        {
            // Puissance instantanee moyennee sur les canaux
            float refSq = ref0[i] * ref0[i];
            float wetSq = wet0[i] * wet0[i];
            if (wet1 != nullptr)
            {
                refSq += ref1[i] * ref1[i];
                wetSq += wet1[i] * wet1[i];
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

            wet0[i] *= currentGain;
            if (wet1 != nullptr)
                wet1[i] *= currentGain;
        }

        // Evite les denormaux quand le signal s'eteint
        if (refPower < 1.0e-20f) refPower = 0.0f;
        if (wetPower < 1.0e-20f) wetPower = 0.0f;
    }
}
