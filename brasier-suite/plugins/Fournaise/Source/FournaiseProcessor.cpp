#include "FournaiseProcessor.h"
#include "FactoryPresets.h"

#include "dsp/MathUtils.h"
#include "dsp/Waveshapers.h"
#include "ui/BrasierEditor.h"

namespace brasier::fournaise
{
    namespace
    {
        constexpr double rampSeconds = 0.02;
        constexpr float  punishDriveDb = 15.0f;   // drive supplementaire du mode Punish
        constexpr float  punishBellDb = 6.0f;     // bosse de presence avant saturation
        constexpr float  punishBellHz = 1800.0f;
        constexpr float  tiltPivotHz = 900.0f;
        constexpr float  tapeEmphasisHz = 3000.0f;
        constexpr float  tapeEmphasisDb = 6.0f;

        // Potards en butee = filtre coupe (signal intact)
        bool lowCutEnabled (float hz) noexcept  { return hz > 20.5f; }
        bool highCutEnabled (float hz) noexcept { return hz < 19900.0f; }

        core::EffectProcessorBase::Options makeOptions()
        {
            core::EffectProcessorBase::Options o;
            o.name = "Brasier Fournaise";
            o.common.withOversampling = true;
            o.common.defaultOversamplingIndex = 2;   // 4x
            o.mixLaw = dsp::DryWetMixer::MixLaw::linear;
            o.factoryPresets = makeFactoryPresets();
            return o;
        }
    }

    juce::AudioProcessorValueTreeState::ParameterLayout FournaiseProcessor::createParameters()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;
        layout.add (core::makeChoiceParameter (ids::model, "Model", getModelNames(), 0));
        layout.add (core::makeDecibelParameter (ids::drive, "Drive", 0.0f, 36.0f, 6.0f));
        layout.add (core::makeDecibelParameter (ids::tone, "Tone", -6.0f, 6.0f, 0.0f));
        layout.add (core::makeBoolParameter (ids::punish, "Punish", false));
        layout.add (core::makeFrequencyParameter (ids::preLowCut, "Pre Low Cut", 20.0f, 1000.0f, 20.0f));
        layout.add (core::makeFrequencyParameter (ids::preHighCut, "Pre High Cut", 1000.0f, 20000.0f, 20000.0f));
        layout.add (core::makeFrequencyParameter (ids::postLowCut, "Post Low Cut", 20.0f, 1000.0f, 20.0f));
        layout.add (core::makeFrequencyParameter (ids::postHighCut, "Post High Cut", 1000.0f, 20000.0f, 20000.0f));
        return layout;
    }

    FournaiseProcessor::FournaiseProcessor()
        : core::EffectProcessorBase (makeOptions(), createParameters())
    {
        modelParam    = apvts.getRawParameterValue (ids::model);
        driveParam    = apvts.getRawParameterValue (ids::drive);
        toneParam     = apvts.getRawParameterValue (ids::tone);
        punishParam   = apvts.getRawParameterValue (ids::punish);
        preLowParam   = apvts.getRawParameterValue (ids::preLowCut);
        preHighParam  = apvts.getRawParameterValue (ids::preHighCut);
        postLowParam  = apvts.getRawParameterValue (ids::postLowCut);
        postHighParam = apvts.getRawParameterValue (ids::postHighCut);
    }

    juce::AudioProcessorEditor* FournaiseProcessor::createEditor()
    {
        using namespace ui;
        using S = ControlSpec::Style;
        EditorSpec e;
        e.pluginName = "FOURNAISE";
        e.tagline = fr ("saturateur analogique");
        e.accent = juce::Colour (0xffe2552b);
        e.rows = {
            { SectionSpec { "Saturation", { { ids::model, S::selector }, { ids::drive, S::largeKnob }, { ids::punish, S::toggle } }, 1.5f },
              SectionSpec { fr ("TONALITÉ"), { { ids::tone, S::knob, true } }, 0.6f } },
            { SectionSpec { "Filtres avant", { { ids::preLowCut, S::knob }, { ids::preHighCut, S::knob } }, 1.0f },
              SectionSpec { fr ("FILTRES APRÈS"), { { ids::postLowCut, S::knob }, { ids::postHighCut, S::knob } }, 1.0f } } };
        return new BrasierEditor (*this, std::move (e));
    }

    //==============================================================================
    void FournaiseProcessor::prepareEffect (const core::EffectSpec& spec)
    {
        baseRate = spec.sampleRate;

        using Cut = dsp::CutFilter::Kind;
        preLow.prepare   (baseRate, Cut::lowCut,  preLowParam->load(),   lowCutEnabled (preLowParam->load()));
        preHigh.prepare  (baseRate, Cut::highCut, preHighParam->load(),  highCutEnabled (preHighParam->load()));
        postLow.prepare  (baseRate, Cut::lowCut,  postLowParam->load(),  lowCutEnabled (postLowParam->load()));
        postHigh.prepare (baseRate, Cut::highCut, postHighParam->load(), highCutEnabled (postHighParam->load()));
        tiltLow.prepare  (baseRate, dsp::SvfType::lowShelf,  tiltPivotHz, 0.5f, -toneParam->load());
        tiltHigh.prepare (baseRate, dsp::SvfType::highShelf, tiltPivotHz, 0.5f,  toneParam->load());

        for (auto& dc : dcBlockers)
            dc.prepare (baseRate, 5.0);   // assez bas pour ne pas dephaser le sub d'une 808

        const double rate = baseRate * oversampler().getFactor();
        punishBell.prepare (rate, dsp::SvfType::bell, punishBellHz, 0.8f, punishParam->load() > 0.5f ? punishBellDb : 0.0f);
        tapePreEmphasis.prepare (rate, dsp::SvfType::highShelf, tapeEmphasisHz, 0.7071f,  tapeEmphasisDb);
        tapeDeEmphasis.prepare  (rate, dsp::SvfType::highShelf, tapeEmphasisHz, 0.7071f, -tapeEmphasisDb);

        currentModel = previousModel = static_cast<Model> (juce::jlimit (0, 3, static_cast<int> (modelParam->load())));
        setOversampledRate (rate);

        driveDb.setCurrentAndTarget (driveParam->load());
        punishAmount.setCurrentAndTarget (punishParam->load() > 0.5f ? 1.0f : 0.0f);
        modelFade.setCurrentAndTarget (1.0f);
    }

    void FournaiseProcessor::setOversampledRate (double rate) noexcept
    {
        osRate = rate;
        driveDb.prepare (osRate, rampSeconds);
        punishAmount.prepare (osRate, rampSeconds);
        modelFade.prepare (osRate, rampSeconds);
        punishBell.setSampleRate (osRate);
        tapePreEmphasis.setSampleRate (osRate);
        tapeDeEmphasis.setSampleRate (osRate);
        punishBell.reset();
        tapePreEmphasis.reset();
        tapeDeEmphasis.reset();
    }

    void FournaiseProcessor::resetEffect()
    {
        for (auto* f : { &preLow, &preHigh, &postLow, &postHigh })
            f->reset();

        for (auto* f : { &tiltLow, &tiltHigh, &punishBell, &tapePreEmphasis, &tapeDeEmphasis })
            f->reset();

        for (auto& dc : dcBlockers)
            dc.reset();
    }

    void FournaiseProcessor::updateTargets() noexcept
    {
        preLow.setFrequency   (preLowParam->load(),   lowCutEnabled (preLowParam->load()));
        preHigh.setFrequency  (preHighParam->load(),  highCutEnabled (preHighParam->load()));
        postLow.setFrequency  (postLowParam->load(),  lowCutEnabled (postLowParam->load()));
        postHigh.setFrequency (postHighParam->load(), highCutEnabled (postHighParam->load()));

        const float tone = toneParam->load();
        tiltLow.setGainDb (-tone);
        tiltHigh.setGainDb (tone);

        driveDb.setTarget (driveParam->load());
        punishAmount.setTarget (punishParam->load() > 0.5f ? 1.0f : 0.0f);

        // Changement de modele : fondu enchaine de 20 ms entre l'ancien et le nouveau
        const auto requested = static_cast<Model> (juce::jlimit (0, 3, static_cast<int> (modelParam->load())));
        if (requested != currentModel)
        {
            previousModel = currentModel;
            currentModel = requested;
            modelFade.setCurrentAndTarget (0.0f);
            modelFade.setTarget (1.0f);
        }
    }

    //==============================================================================
    float FournaiseProcessor::shape (Model m, int channel, float x) noexcept
    {
        switch (m)
        {
            case Model::tube:       return dsp::shapers::tube (x);
            case Model::transistor: return dsp::shapers::transistor (x);
            case Model::diode:      return dsp::shapers::diode (x);
            case Model::tape:
            {
                // Pre-emphase des aigus avant la bande, de-emphase apres : les aigus
                // saturent plus tot, comme sur un vrai magnetophone.
                const float emphasised = tapePreEmphasis.processSample (channel, x);
                return tapeDeEmphasis.processSample (channel, dsp::shapers::tape (emphasised));
            }
        }

        return x;
    }

    void FournaiseProcessor::saturate (juce::dsp::AudioBlock<float> up) noexcept
    {
        const auto numSamples = up.getNumSamples();
        const auto numChannels = juce::jmin (up.getNumChannels(), static_cast<size_t> (2));

        // Les filtres de bande ne tournent que si le modele Bande est en jeu
        const bool tapeInUse = currentModel == Model::tape || (modelFade.isSmoothing() && previousModel == Model::tape);
        if (tapeInUse && ! tapeFiltersActive)
        {
            tapePreEmphasis.reset();
            tapeDeEmphasis.reset();
        }
        tapeFiltersActive = tapeInUse;

        auto computeDrive = [this] (float& driveGain, float& compensation)
        {
            const float totalDb = driveDb.getCurrent() + punishDriveDb * punishAmount.getCurrent();
            driveGain = dsp::dbToGain (totalDb);
            compensation = dsp::dbToGain (-0.5f * totalDb);   // le drive ajoute la moitie de ses dB
        };

        float driveGain = 1.0f, compensation = 1.0f;
        computeDrive (driveGain, compensation);
        punishBell.setGainDb (punishBellDb * punishAmount.getTarget());

        for (size_t i = 0; i < numSamples; ++i)
        {
            if (driveDb.isSmoothing() || punishAmount.isSmoothing())
            {
                driveDb.getNext();
                punishAmount.getNext();
                computeDrive (driveGain, compensation);
            }

            punishBell.tickParameters();
            const bool bellActive = punishAmount.getCurrent() > 0.0f || punishBell.isSmoothing();
            const bool fading = modelFade.isSmoothing();
            const float fade = fading ? modelFade.getNext() : 1.0f;

            for (size_t ch = 0; ch < numChannels; ++ch)
            {
                auto* data = up.getChannelPointer (ch);
                float x = data[i];

                if (bellActive)
                    x = punishBell.processSample (static_cast<int> (ch), x);

                x *= driveGain;

                float y = shape (currentModel, static_cast<int> (ch), x);
                if (fading)
                    y = fade * y + (1.0f - fade) * shape (previousModel, static_cast<int> (ch), x);

                data[i] = y * compensation;
            }
        }
    }

    void FournaiseProcessor::processEffect (juce::dsp::AudioBlock<float> block, const core::ProcessContext& ctx)
    {
        if (ctx.oversamplingChanged)
            setOversampledRate (ctx.oversampledRate);

        updateTargets();

        // 1. Filtres PRE (lineaires, frequence de base)
        preLow.process (block);
        preHigh.process (block);

        // 2. Saturation sur-echantillonnee
        auto up = oversampler().processUp (block);
        saturate (up);
        oversampler().processDown (block);

        // 3. DC blocker (les modeles asymetriques creent du continu)
        const auto numChannels = juce::jmin (block.getNumChannels(), static_cast<size_t> (2));
        for (size_t ch = 0; ch < numChannels; ++ch)
        {
            auto* data = block.getChannelPointer (ch);
            auto& dc = dcBlockers[ch];
            for (size_t i = 0; i < block.getNumSamples(); ++i)
                data[i] = dc.process (data[i]);
            dc.flush();
        }

        // 4. Tone (tilt) puis filtres POST
        tiltLow.process (block);
        tiltHigh.process (block);
        postLow.process (block);
        postHigh.process (block);
    }
}
