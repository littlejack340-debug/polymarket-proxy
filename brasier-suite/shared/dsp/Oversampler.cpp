#include "Oversampler.h"

namespace brasier::dsp
{
    void Oversampler::prepare (int numChannels, int maxBlockSize)
    {
        using OS = juce::dsp::Oversampling<float>;

        for (size_t i = 0; i < engines.size(); ++i)
        {
            const auto stages = i + 1;   // 2^1, 2^2, 2^3

            engines[i] = std::make_unique<OS> (static_cast<size_t> (juce::jmax (1, numChannels)),
                                               stages,
                                               OS::filterHalfBandFIREquiripple,
                                               true,    // qualite max : rejection plus forte
                                               true);   // latence entiere
            engines[i]->initProcessing (static_cast<size_t> (juce::jmax (1, maxBlockSize)));
        }

        reset();
    }

    void Oversampler::reset() noexcept
    {
        for (auto& e : engines)
            if (e != nullptr)
                e->reset();
    }

    void Oversampler::setFactorIndex (int newIndex) noexcept
    {
        newIndex = juce::jlimit (0, numFactors - 1, newIndex);

        if (newIndex == index)
            return;

        index = newIndex;

        if (auto* os = current())
            os->reset();
    }

    int Oversampler::getLatencySamplesForIndex (int factorIndex) const noexcept
    {
        if (factorIndex <= 0 || factorIndex >= numFactors)
            return 0;

        const auto& e = engines[static_cast<size_t> (factorIndex - 1)];
        return e != nullptr ? static_cast<int> (std::lround (e->getLatencyInSamples())) : 0;
    }

    int Oversampler::getLatencySamples() const noexcept
    {
        return getLatencySamplesForIndex (index);
    }

    int Oversampler::getMaxLatencySamples() const noexcept
    {
        int maxLatency = 0;

        for (int i = 1; i < numFactors; ++i)
            maxLatency = juce::jmax (maxLatency, getLatencySamplesForIndex (i));

        return maxLatency;
    }

    juce::dsp::AudioBlock<float> Oversampler::processUp (juce::dsp::AudioBlock<float> block) noexcept
    {
        if (auto* os = current())
            return os->processSamplesUp (block);

        return block;
    }

    void Oversampler::processDown (juce::dsp::AudioBlock<float> block) noexcept
    {
        if (auto* os = current())
            os->processSamplesDown (block);
    }
}
