#include "TamisProcessor.h"
#include "FactoryPresets.h"

#include "dsp/MathUtils.h"
#include "dsp/TempoSync.h"
#include "dsp/Waveshapers.h"
#include "ui/BrasierEditor.h"

namespace brasier::tamis
{
    namespace
    {
        constexpr float maxModOctaves = 4.0f;
        constexpr double rampSeconds = 0.02;
        constexpr double switchSeconds = 0.005;

        /** Resonance 0..1 -> amortissement k = 1/Q. De 0 a ~85 % : Q de 0,5 a ~150 ;
            au-dela, k devient legerement negatif : le filtre auto-oscille, son
            amplitude etant tenue par la saturation de l'integrateur. */
        float resonanceToK (float r) noexcept
        {
            r = juce::jlimit (0.0f, 1.0f, r);
            const float open = 1.0f - r;
            return 2.0f * open * open * open - 0.02f * std::pow (r, 20.0f);
        }

        core::EffectProcessorBase::Options makeOptions()
        {
            core::EffectProcessorBase::Options o;
            o.name = "Brasier Tamis";
            o.common.withOversampling = true;
            o.common.defaultOversamplingIndex = 1;   // 2x
            o.mixLaw = dsp::DryWetMixer::MixLaw::linear;
            o.factoryPresets = makeFactoryPresets();
            return o;
        }
    }

    juce::AudioProcessorValueTreeState::ParameterLayout TamisProcessor::createParameters()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;
        layout.add (core::makeChoiceParameter (ids::type, "Type", dsp::ResonantFilter::getModeNames(), 0));
        layout.add (core::makeChoiceParameter (ids::slope, "Slope", { "12 dB", "24 dB" }, 0));
        layout.add (core::makeFrequencyParameter (ids::cutoff, "Cutoff", 20.0f, 20000.0f, 1200.0f));
        layout.add (core::makePercentParameter (ids::resonance, "Resonance", 30.0f));
        layout.add (core::makeDecibelParameter (ids::drive, "Drive", 0.0f, 24.0f, 0.0f));
        layout.add (core::makeFloatParameter (ids::lfoAmount, "LFO Amount", { -100.0f, 100.0f, 0.1f }, 0.0f, "%", 1));
        layout.add (core::makeChoiceParameter (ids::lfoShape, "LFO Shape", dsp::Lfo::getShapeNames(), 0));
        layout.add (core::makeBoolParameter (ids::lfoSync, "LFO Sync", true));
        layout.add (core::makeFrequencyParameter (ids::lfoRate, "LFO Rate", 0.05f, 20.0f, 1.0f));
        layout.add (core::makeChoiceParameter (ids::lfoDivision, "LFO Division", dsp::tempo::divisionNames(), dsp::tempo::quarterIndex + 3));
        layout.add (core::makeFloatParameter (ids::envAmount, "Env Amount", { -100.0f, 100.0f, 0.1f }, 0.0f, "%", 1));
        layout.add (core::makeTimeParameter (ids::envAttack, "Env Attack", 0.5f, 200.0f, 5.0f));
        layout.add (core::makeTimeParameter (ids::envRelease, "Env Release", 10.0f, 2000.0f, 150.0f));
        return layout;
    }

    TamisProcessor::TamisProcessor()
        : core::EffectProcessorBase (makeOptions(), createParameters())
    {
        typeParam        = apvts.getRawParameterValue (ids::type);
        slopeParam       = apvts.getRawParameterValue (ids::slope);
        cutoffParam      = apvts.getRawParameterValue (ids::cutoff);
        resonanceParam   = apvts.getRawParameterValue (ids::resonance);
        driveParam       = apvts.getRawParameterValue (ids::drive);
        lfoAmountParam   = apvts.getRawParameterValue (ids::lfoAmount);
        lfoShapeParam    = apvts.getRawParameterValue (ids::lfoShape);
        lfoSyncParam     = apvts.getRawParameterValue (ids::lfoSync);
        lfoRateParam     = apvts.getRawParameterValue (ids::lfoRate);
        lfoDivisionParam = apvts.getRawParameterValue (ids::lfoDivision);
        envAmountParam   = apvts.getRawParameterValue (ids::envAmount);
        envAttackParam   = apvts.getRawParameterValue (ids::envAttack);
        envReleaseParam  = apvts.getRawParameterValue (ids::envRelease);
    }

    juce::AudioProcessorEditor* TamisProcessor::createEditor()
    {
        using namespace ui;
        using S = ControlSpec::Style;
        EditorSpec e;
        e.pluginName = "TAMIS";
        e.tagline = fr ("filtre multimode");
        e.accent = juce::Colour (0xff3fb8a6);
        e.rows = {
            { SectionSpec { "Filtre", { { ids::type, S::selector }, { ids::slope, S::selector }, { ids::cutoff, S::knob },
                                        { ids::resonance, S::knob }, { ids::drive, S::knob } }, 1.0f } },
            { SectionSpec { "LFO", { { ids::lfoAmount, S::knob, true, "Amount" }, { ids::lfoShape, S::selector, false, "Shape" },
                                     { ids::lfoSync, S::toggle, false, "Sync" }, { ids::lfoRate, S::knob, false, "Rate", ids::lfoSync, 0 },
                                     { ids::lfoDivision, S::selector, false, "Division", ids::lfoSync, 1 } }, 1.6f },
              SectionSpec { "Enveloppe", { { ids::envAmount, S::knob, true, "Amount" }, { ids::envAttack, S::smallKnob, false, "Attack" },
                                           { ids::envRelease, S::smallKnob, false, "Release" } }, 1.0f } } };
        return new BrasierEditor (*this, std::move (e));
    }

    void TamisProcessor::prepareEffect (const core::EffectSpec& spec)
    {
        baseRate = spec.sampleRate;
        coeffs.assign (static_cast<size_t> (spec.maxBlockSize), {});

        for (auto* s : { &logCutoff, &resonance, &driveDb, &lfoDepth, &envDepth })
            s->prepare (baseRate, rampSeconds);

        logCutoff.setCurrentAndTarget (std::log2 (cutoffParam->load()));
        resonance.setCurrentAndTarget (resonanceParam->load() * 0.01f);
        driveDb.setCurrentAndTarget (driveParam->load());
        lfoDepth.setCurrentAndTarget (lfoAmountParam->load() * 0.01f * maxModOctaves);
        envDepth.setCurrentAndTarget (envAmountParam->load() * 0.01f * maxModOctaves);

        lfo.prepare (baseRate, 0xF17E2u);
        envelope.prepare (baseRate);

        activeMode = static_cast<dsp::ResonantFilter::Mode> (juce::jlimit (0, 3, static_cast<int> (typeParam->load())));
        activeSteep = slopeParam->load() > 0.5f;
        switchGain = 1.0f;
        switching = false;
        switchStep = static_cast<float> (1.0 / (switchSeconds * baseRate));
    }

    void TamisProcessor::resetEffect()
    {
        filter.reset();
        lfo.reset();
        envelope.reset();
    }

    void TamisProcessor::processEffect (juce::dsp::AudioBlock<float> block, const core::ProcessContext& ctx)
    {
        const auto numSamples = block.getNumSamples();
        const auto numChannels = juce::jmin (block.getNumChannels(), static_cast<size_t> (2));
        const double osRate = ctx.oversampledRate;
        const int factor = ctx.oversamplingFactor;

        // --- Cibles
        logCutoff.setTarget (std::log2 (cutoffParam->load()));
        resonance.setTarget (resonanceParam->load() * 0.01f);
        driveDb.setTarget (driveParam->load());
        lfoDepth.setTarget (lfoAmountParam->load() * 0.01f * maxModOctaves);
        envDepth.setTarget (envAmountParam->load() * 0.01f * maxModOctaves);
        envelope.setTimes (envAttackParam->load() * 0.001, envReleaseParam->load() * 0.001);
        lfo.setShape (static_cast<dsp::Lfo::Shape> (juce::jlimit (0, 6, static_cast<int> (lfoShapeParam->load()))));

        if (lfoSyncParam->load() > 0.5f)
        {
            const int division = static_cast<int> (lfoDivisionParam->load());
            lfo.setFrequency (dsp::tempo::divisionHz (division, ctx.bpm));
            if (ctx.isPlaying)   // LFO cale sur la grille de l'hote
                lfo.syncToPpq (ctx.ppqPosition, 1.0 / dsp::tempo::divisions[static_cast<size_t> (juce::jlimit (0, 18, division))].quarterNotes);
        }
        else
        {
            lfo.setFrequency (lfoRateParam->load());
        }

        // Changement de type ou de pente : on baisse, on bascule, on remonte
        const auto requestedMode = static_cast<dsp::ResonantFilter::Mode> (juce::jlimit (0, 3, static_cast<int> (typeParam->load())));
        const bool requestedSteep = slopeParam->load() > 0.5f;
        if ((requestedMode != activeMode || requestedSteep != activeSteep) && ! switching)
            switching = true;

        // --- 1. Modulation et coefficients a la frequence de base
        const float maxHz = static_cast<float> (juce::jmin (22000.0, 0.45 * osRate));
        for (size_t i = 0; i < numSamples; ++i)
        {
            float peak = 0.0f;
            for (size_t ch = 0; ch < numChannels; ++ch)
                peak = juce::jmax (peak, std::abs (block.getSample (static_cast<int> (ch), static_cast<int> (i))));

            const float env = envelope.process (peak);
            const float envNorm = juce::jlimit (0.0f, 1.0f, (dsp::gainToDb (env) + 48.0f) / 48.0f);
            const float octaves = lfoDepth.getNext() * lfo.next() + envDepth.getNext() * envNorm;
            const float hz = juce::jlimit (16.0f, maxHz, std::exp2 (logCutoff.getNext() + octaves));
            const auto g = static_cast<float> (std::tan (juce::MathConstants<double>::pi * hz / osRate));
            coeffs[i] = dsp::ResonantFilter::makeCoeffs (g, resonanceToK (resonance.getNext()));
        }

        // --- 2. Drive + filtre, sur-echantillonnes
        auto up = oversampler().processUp (block);
        const auto numUp = up.getNumSamples();
        const float startDb = driveDb.getCurrent();
        const float endDb = driveDb.getTarget();
        driveDb.skip (static_cast<int> (numSamples));

        const bool driveRamp = ! juce::exactlyEqual (startDb, endDb);
        float driveGain = dsp::dbToGain (startDb);
        float compensation = dsp::dbToGain (-0.5f * startDb);

        for (size_t j = 0; j < numUp; ++j)
        {
            const auto& c = coeffs[j / static_cast<size_t> (factor)];

            if (driveRamp && (j & 7) == 0)
            {
                const float db = startDb + (static_cast<float> (j) / static_cast<float> (numUp)) * (endDb - startDb);
                driveGain = dsp::dbToGain (db);
                compensation = dsp::dbToGain (-0.5f * db);
            }

            if (switching)
            {
                switchGain -= switchStep / static_cast<float> (factor);
                if (switchGain <= 0.0f)
                {
                    switchGain = 0.0f;
                    activeMode = requestedMode;
                    activeSteep = requestedSteep;
                    filter.reset();
                    switching = false;
                }
            }
            else if (switchGain < 1.0f)
            {
                switchGain = juce::jmin (1.0f, switchGain + switchStep / static_cast<float> (factor));
            }

            for (size_t ch = 0; ch < numChannels; ++ch)
            {
                auto& x = up.getChannelPointer (ch)[j];
                const float driven = dsp::shapers::fastTanh (x * driveGain) * compensation;
                x = filter.process (static_cast<int> (ch), c, driven, activeMode, activeSteep) * switchGain;
            }
        }

        filter.flush();
        oversampler().processDown (block);
    }
}
