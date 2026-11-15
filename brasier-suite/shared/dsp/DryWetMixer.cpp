#include "DryWetMixer.h"
#include "MathUtils.h"

namespace brasier::dsp
{
    void DryWetMixer::prepare (double sampleRate, int numChannels, int maxBlockSize, int maxLatencySamples)
    {
        numChans   = juce::jmax (1, numChannels);
        maxLatency = juce::jmax (0, maxLatencySamples);
        delaySize  = maxLatency + 1;

        delayBuffer.setSize (numChans, delaySize);
        alignedDry.setSize (numChans, juce::jmax (1, maxBlockSize));

        mixSmoother.prepare (sampleRate, 0.03);
        reset();
    }

    void DryWetMixer::reset() noexcept
    {
        delayBuffer.clear();
        alignedDry.clear();
        writePos = 0;
        lastBlockSize = 0;
        mixSmoother.setCurrentAndTarget (mixSmoother.getTarget());
    }

    void DryWetMixer::setLatency (int samples) noexcept
    {
        latency = juce::jlimit (0, maxLatency, samples);
    }

    void DryWetMixer::pushDry (const juce::dsp::AudioBlock<const float>& input) noexcept
    {
        const auto numSamples  = static_cast<int> (input.getNumSamples());
        const auto numChannels = juce::jmin (numChans, static_cast<int> (input.getNumChannels()));

        jassert (numSamples <= alignedDry.getNumSamples());
        lastBlockSize = juce::jmin (numSamples, alignedDry.getNumSamples());

        int pos = writePos;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const auto* in = input.getChannelPointer (static_cast<size_t> (ch));
            auto* delay    = delayBuffer.getWritePointer (ch);
            auto* out      = alignedDry.getWritePointer (ch);

            pos = writePos;
            int readPos = pos - latency;
            if (readPos < 0)
                readPos += delaySize;

            for (int i = 0; i < lastBlockSize; ++i)
            {
                delay[pos] = in[i];        // ecriture avant lecture : latence 0 = passe-plat
                out[i] = delay[readPos];

                if (++pos == delaySize)     pos = 0;
                if (++readPos == delaySize) readPos = 0;
            }
        }

        writePos = pos;
    }

    juce::dsp::AudioBlock<const float> DryWetMixer::getAlignedDry() const noexcept
    {
        return juce::dsp::AudioBlock<const float> (alignedDry).getSubBlock (0, static_cast<size_t> (lastBlockSize));
    }

    void DryWetMixer::computeGains (float mix, float& dryGain, float& wetGain) const noexcept
    {
        if (law == MixLaw::equalPower)
        {
            dryGain = std::cos (mix * halfPi);
            wetGain = std::sin (mix * halfPi);
        }
        else
        {
            dryGain = 1.0f - mix;
            wetGain = mix;
        }
    }

    void DryWetMixer::mixWet (juce::dsp::AudioBlock<float>& wet) noexcept
    {
        const auto numSamples  = juce::jmin (lastBlockSize, static_cast<int> (wet.getNumSamples()));
        const auto numChannels = juce::jmin (numChans, static_cast<int> (wet.getNumChannels()));

        if (! mixSmoother.isSmoothing())
        {
            float dryGain, wetGain;
            computeGains (mixSmoother.getTarget(), dryGain, wetGain);

            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto* w = wet.getChannelPointer (static_cast<size_t> (ch));
                const auto* d = alignedDry.getReadPointer (ch);

                if (wetGain <= 0.0f)
                {
                    std::copy (d, d + numSamples, w);   // 100 % dry : copie exacte
                }
                else
                {
                    for (int i = 0; i < numSamples; ++i)
                        w[i] = d[i] * dryGain + w[i] * wetGain;
                }
            }

            return;
        }

        // Rampe de mix en cours : gains calcules a chaque echantillon,
        // identiques pour tous les canaux.
        auto* w0 = wet.getChannelPointer (0);
        auto* w1 = numChannels > 1 ? wet.getChannelPointer (1) : nullptr;
        const auto* d0 = alignedDry.getReadPointer (0);
        const auto* d1 = numChannels > 1 ? alignedDry.getReadPointer (1) : nullptr;

        for (int i = 0; i < numSamples; ++i)
        {
            float dryGain, wetGain;
            computeGains (mixSmoother.getNext(), dryGain, wetGain);

            w0[i] = d0[i] * dryGain + w0[i] * wetGain;

            if (w1 != nullptr)
                w1[i] = d1[i] * dryGain + w1[i] * wetGain;
        }
    }
}
