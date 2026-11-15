#pragma once

#include "LinearSmoother.h"
#include "Svf.h"

namespace brasier::dsp
{
    /**
        Filtre coupe-bas / coupe-haut 12 dB/oct (Butterworth) avec une vraie
        position "off" : potard en butee = signal strictement intact (aucun
        dephasage dans le sub, important pour les 808).

        Le passage on/off se fait par un fondu de 20 ms entre signal direct et
        signal filtre : pas de clic.
    */
    class CutFilter
    {
    public:
        enum class Kind { lowCut, highCut };

        void prepare (double sampleRate, Kind newKind, float hz, bool enabled) noexcept
        {
            kind = newKind;
            svf.prepare (sampleRate, kind == Kind::lowCut ? SvfType::highpass : SvfType::lowpass, hz, 0.70710678f);
            amount.prepare (sampleRate, 0.02);
            amount.setCurrentAndTarget (enabled ? 1.0f : 0.0f);
            wasActive = enabled;
        }

        void setSampleRate (double sampleRate) noexcept
        {
            svf.setSampleRate (sampleRate);
            amount.prepare (sampleRate, 0.02);
        }

        /** enabled = false : filtre neutralise (fondu vers le signal direct). */
        void setFrequency (float hz, bool enabled) noexcept
        {
            svf.setFrequency (hz);
            amount.setTarget (enabled ? 1.0f : 0.0f);
        }

        void reset() noexcept { svf.reset(); }

        bool isActive() const noexcept { return amount.isSmoothing() || amount.getTarget() > 0.0f; }

        void process (juce::dsp::AudioBlock<float> block) noexcept
        {
            if (! isActive())
            {
                wasActive = false;
                return;   // off : passe-plat exact
            }

            if (! wasActive)
                svf.reset();   // repart d'un etat propre (le fondu masque la mise en route)
            wasActive = true;

            if (! amount.isSmoothing())
            {
                svf.process (block);
                return;
            }

            const auto numChannels = juce::jmin (block.getNumChannels(), static_cast<size_t> (SmoothedSvf::maxChannels));
            for (size_t i = 0; i < block.getNumSamples(); ++i)
            {
                svf.tickParameters();
                const float a = amount.getNext();
                for (size_t ch = 0; ch < numChannels; ++ch)
                {
                    auto* data = block.getChannelPointer (ch);
                    const float filtered = svf.processSample (static_cast<int> (ch), data[i]);
                    data[i] += a * (filtered - data[i]);
                }
            }
        }

    private:
        Kind kind = Kind::lowCut;
        SmoothedSvf svf;
        LinearSmoother<float> amount;
        bool wasActive = false;
    };
}
