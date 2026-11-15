#include "GainProcessor.h"
#include "FactoryPresets.h"

#include "dsp/MathUtils.h"
#include "ui/BrasierEditor.h"

namespace brasier::gain
{
    namespace
    {
        constexpr double smoothingSeconds = 0.02;
    }

    juce::AudioProcessorValueTreeState::ParameterLayout GainProcessor::createParameters()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;
        layout.add (core::makeDecibelParameter (ids::gain, "Gain", -24.0f, 24.0f, 0.0f));
        layout.add (core::makeBoolParameter (ids::clip, "Soft Clip", false));
        return layout;
    }

    static core::EffectProcessorBase::Options makeOptions()
    {
        core::EffectProcessorBase::Options o;
        o.name = "Brasier Gain";
        o.common.withOversampling = true;
        o.common.defaultOversamplingIndex = 1;   // 2x
        o.mixLaw = dsp::DryWetMixer::MixLaw::linear;
        o.factoryPresets = makeFactoryPresets();
        return o;
    }

    GainProcessor::GainProcessor()
        : core::EffectProcessorBase (makeOptions(), createParameters())
    {
        gainParam = apvts.getRawParameterValue (ids::gain);
        clipParam = apvts.getRawParameterValue (ids::clip);
    }

    juce::AudioProcessorEditor* GainProcessor::createEditor()
    {
        using namespace ui;
        using S = ControlSpec::Style;
        EditorSpec e;
        e.pluginName = "GAIN";
        e.tagline = fr ("outil de test");
        e.accent = juce::Colour (0xffe0a030);
        e.rows = { { SectionSpec { "Gain", { { ids::gain, S::largeKnob, true } }, 1.0f },
                     SectionSpec { "Couleur", { { ids::clip, S::toggle } }, 1.0f } } };
        return new BrasierEditor (*this, std::move (e));
    }

    void GainProcessor::prepareEffect (const core::EffectSpec& spec)
    {
        gainSmoother.prepare (spec.sampleRate, smoothingSeconds);
        gainSmoother.setCurrentAndTarget (dsp::dbToGain (gainParam->load()));

        // La duree de rampe du clip est recalculee quand le facteur d'oversampling change
        clipSmoother.prepare (spec.sampleRate * oversampler().getFactor(), smoothingSeconds);
        clipSmoother.setCurrentAndTarget (clipParam->load() > 0.5f ? 1.0f : 0.0f);
    }

    void GainProcessor::resetEffect()
    {
        gainSmoother.setCurrentAndTarget (gainSmoother.getTarget());
        clipSmoother.setCurrentAndTarget (clipSmoother.getTarget());
    }

    void GainProcessor::processEffect (juce::dsp::AudioBlock<float> block, const core::ProcessContext& ctx)
    {
        const auto numChannels = block.getNumChannels();
        const auto numSamples  = block.getNumSamples();

        if (ctx.oversamplingChanged)
            clipSmoother.prepare (ctx.oversampledRate, smoothingSeconds);

        gainSmoother.setTarget (dsp::dbToGain (gainParam->load()));
        clipSmoother.setTarget (clipParam->load() > 0.5f ? 1.0f : 0.0f);

        // 1. Gain (lineaire) a la frequence de base
        for (size_t i = 0; i < numSamples; ++i)
        {
            const float g = gainSmoother.getNext();
            for (size_t ch = 0; ch < numChannels; ++ch)
                block.getChannelPointer (ch)[i] *= g;
        }

        // 2. Soft-clip (non lineaire) a la frequence sur-echantillonnee
        auto up = oversampler().processUp (block);
        const auto upSamples = up.getNumSamples();

        if (clipSmoother.isSmoothing() || clipSmoother.getTarget() > 0.0f)
        {
            for (size_t i = 0; i < upSamples; ++i)
            {
                const float amount = clipSmoother.getNext();
                for (size_t ch = 0; ch < numChannels; ++ch)
                {
                    auto& x = up.getChannelPointer (ch)[i];
                    x += amount * (std::tanh (x) - x);   // fondu propre <-> tanh
                }
            }
        }

        oversampler().processDown (block);
    }
}
