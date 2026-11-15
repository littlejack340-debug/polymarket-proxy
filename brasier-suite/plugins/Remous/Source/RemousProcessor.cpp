#include "RemousProcessor.h"
#include "FactoryPresets.h"

#include "dsp/TempoSync.h"
#include "ui/BrasierEditor.h"

namespace brasier::remous
{
    namespace
    {
        core::EffectProcessorBase::Options makeOptions()
        {
            core::EffectProcessorBase::Options o;
            o.name = "Brasier Remous";
            o.common.withOversampling = true;
            o.common.defaultOversamplingIndex = 1;   // 2x
            o.mixLaw = dsp::DryWetMixer::MixLaw::linear;
            o.tailSeconds = 0.1;
            o.factoryPresets = makeFactoryPresets();
            return o;
        }

        juce::StringArray shapeNames() { return { "Sinus", "Triangle", "Scie +", "Scie -", "Carre" }; }
    }

    juce::AudioProcessorValueTreeState::ParameterLayout RemousProcessor::createParameters()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;
        layout.add (core::makeChoiceParameter (ids::mode, "Mode", dsp::AnalogModulator::getModeNames(), 0));
        layout.add (core::makeFrequencyParameter (ids::rate, "Rate", 0.02f, 12.0f, 0.8f));
        layout.add (core::makeBoolParameter (ids::sync, "Sync", false));
        layout.add (core::makeChoiceParameter (ids::division, "Division", dsp::tempo::divisionNames(), dsp::tempo::quarterIndex + 3));
        layout.add (core::makePercentParameter (ids::depth, "Depth", 50.0f));
        layout.add (core::makeFloatParameter (ids::feedback, "Feedback", { -95.0f, 95.0f, 0.1f }, 30.0f, "%", 1));
        layout.add (core::makePercentParameter (ids::spread, "Spread", 50.0f));
        layout.add (core::makeChoiceParameter (ids::shape, "Shape", shapeNames(), 0));
        layout.add (core::makePercentParameter (ids::character, "Character", 30.0f));
        return layout;
    }

    RemousProcessor::RemousProcessor()
        : core::EffectProcessorBase (makeOptions(), createParameters())
    {
        modeParam      = apvts.getRawParameterValue (ids::mode);
        rateParam      = apvts.getRawParameterValue (ids::rate);
        syncParam      = apvts.getRawParameterValue (ids::sync);
        divisionParam  = apvts.getRawParameterValue (ids::division);
        depthParam     = apvts.getRawParameterValue (ids::depth);
        feedbackParam  = apvts.getRawParameterValue (ids::feedback);
        spreadParam    = apvts.getRawParameterValue (ids::spread);
        shapeParam     = apvts.getRawParameterValue (ids::shape);
        characterParam = apvts.getRawParameterValue (ids::character);
    }

    juce::AudioProcessorEditor* RemousProcessor::createEditor()
    {
        using namespace ui;
        using S = ControlSpec::Style;
        EditorSpec e;
        e.pluginName = "REMOUS";
        e.tagline = fr ("modulation analogique");
        e.accent = juce::Colour (0xff4a90d9);
        e.rows = {
            { SectionSpec { "Effet", { { ids::mode, S::selector }, { ids::depth, S::knob }, { ids::feedback, S::knob, true } }, 1.4f },
              SectionSpec { "Vitesse", { { ids::sync, S::toggle }, { ids::rate, S::knob, false, {}, ids::sync, 0 }, { ids::division, S::selector, false, {}, ids::sync, 1 } }, 1.2f } },
            { SectionSpec { fr ("CARACTÈRE"), { { ids::shape, S::selector }, { ids::spread, S::knob }, { ids::character, S::knob } }, 1.0f } } };
        return new BrasierEditor (*this, std::move (e));
    }

    dsp::AnalogModulator::Settings RemousProcessor::readSettings (const core::ProcessContext& ctx) const noexcept
    {
        dsp::AnalogModulator::Settings s;
        s.mode = static_cast<dsp::AnalogModulator::Mode> (juce::jlimit (0, 3, static_cast<int> (modeParam->load())));
        s.rateHz = syncParam->load() > 0.5f ? dsp::tempo::divisionHz (static_cast<int> (divisionParam->load()), ctx.bpm)
                                            : static_cast<double> (rateParam->load());
        s.depth = depthParam->load() * 0.01f;
        s.feedback = feedbackParam->load() * 0.01f;
        s.spread = spreadParam->load() * 0.01f;
        s.character = characterParam->load() * 0.01f;
        s.shape = static_cast<dsp::Lfo::Shape> (juce::jlimit (0, 4, static_cast<int> (shapeParam->load())));
        return s;
    }

    void RemousProcessor::prepareEffect (const core::EffectSpec& spec)
    {
        core::ProcessContext defaults;
        modulator.setSettings (readSettings (defaults));
        modulator.prepare (spec.sampleRate, spec.maxBlockSize, spec.maxOversamplingFactor);
    }

    void RemousProcessor::resetEffect()
    {
        modulator.reset();
    }

    void RemousProcessor::processEffect (juce::dsp::AudioBlock<float> block, const core::ProcessContext& ctx)
    {
        if (ctx.oversamplingChanged)
            modulator.reset();   // les lignes de retard sont a la frequence sur-echantillonnee

        modulator.setSettings (readSettings (ctx));

        if (syncParam->load() > 0.5f && ctx.isPlaying)
        {
            const int division = juce::jlimit (0, 18, static_cast<int> (divisionParam->load()));
            modulator.syncToPpq (ctx.ppqPosition, 1.0 / dsp::tempo::divisions[static_cast<size_t> (division)].quarterNotes);
        }

        modulator.computeModulation (static_cast<int> (block.getNumSamples()), ctx.oversampledRate);
        auto up = oversampler().processUp (block);
        modulator.processOversampled (up, ctx.oversamplingFactor, ctx.oversampledRate);
        oversampler().processDown (block);
    }
}
