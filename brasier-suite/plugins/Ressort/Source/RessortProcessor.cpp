#include "RessortProcessor.h"

#include "dsp/MathUtils.h"
#include "dsp/Waveshapers.h"
#include "ui/GenericKnobEditor.h"

namespace brasier::ressort
{
    namespace
    {
        constexpr double maxPredelayMs = 250.0;

        core::EffectProcessorBase::Options makeOptions()
        {
            core::EffectProcessorBase::Options o;
            o.name = "Brasier Ressort";
            o.common.withOversampling = true;        // pour le drive d'entree
            o.common.defaultOversamplingIndex = 1;   // 2x
            o.common.defaultMixPercent = 25.0f;
            o.mixLaw = dsp::DryWetMixer::MixLaw::equalPower;
            o.autoGainMode = core::EffectProcessorBase::AutoGainMode::fixed;
            o.tailSeconds = 12.0;
            return o;
        }

        bool lowCutEnabled (float hz) noexcept  { return hz > 20.5f; }
        bool highCutEnabled (float hz) noexcept { return hz < 19900.0f; }
    }

    juce::AudioProcessorValueTreeState::ParameterLayout RessortProcessor::createParameters()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;
        layout.add (core::makeChoiceParameter (ids::type, "Type", { "Ressort", "Plaque" }, 0));
        layout.add (core::makeFloatParameter (ids::decay, "Decay", { 0.3f, 8.0f, 0.01f, 0.5f }, 2.0f, "s", 2));
        layout.add (core::makeTimeParameter (ids::predelay, "Pre-Delay", 0.0f, static_cast<float> (maxPredelayMs), 15.0f));
        layout.add (core::makePercentParameter (ids::damping, "Damping", 40.0f));
        layout.add (core::makePercentParameter (ids::tension, "Tension", 50.0f));
        layout.add (core::makeFrequencyParameter (ids::lowCut, "Low Cut", 20.0f, 1000.0f, 150.0f));
        layout.add (core::makeFrequencyParameter (ids::highCut, "High Cut", 1000.0f, 20000.0f, 8000.0f));
        layout.add (core::makeDecibelParameter (ids::drive, "Drive", 0.0f, 18.0f, 3.0f));
        layout.add (core::makePercentParameter (ids::width, "Width", 100.0f));
        return layout;
    }

    RessortProcessor::RessortProcessor()
        : core::EffectProcessorBase (makeOptions(), createParameters())
    {
        typeParam     = apvts.getRawParameterValue (ids::type);
        decayParam    = apvts.getRawParameterValue (ids::decay);
        predelayParam = apvts.getRawParameterValue (ids::predelay);
        dampingParam  = apvts.getRawParameterValue (ids::damping);
        tensionParam  = apvts.getRawParameterValue (ids::tension);
        lowCutParam   = apvts.getRawParameterValue (ids::lowCut);
        highCutParam  = apvts.getRawParameterValue (ids::highCut);
        driveParam    = apvts.getRawParameterValue (ids::drive);
        widthParam    = apvts.getRawParameterValue (ids::width);
    }

    juce::AudioProcessorEditor* RessortProcessor::createEditor()
    {
        ui::GenericKnobEditor::Options o;
        o.title = "BRASIER  RESSORT";
        o.subtitle = "reverb ressort / plaque";
        o.footerParameterIds = core::getCommonParameterIds();
        o.hiddenParameterIds = { core::ids::bypass };
        return new ui::GenericKnobEditor (*this, apvts, o);
    }

    float RessortProcessor::getStaticAutoGainDb() const
    {
        return -0.5f * driveParam->load();
    }

    void RessortProcessor::prepareEffect (const core::EffectSpec& spec)
    {
        baseRate = spec.sampleRate;

        // Le pre-delay (le premier composant de 0 ms) a 4 ech. de garde (lecture cubique)
        predelayLine.prepare (1, static_cast<int> (std::ceil (maxPredelayMs * 0.001 * baseRate)) + 8);
        lowCut.prepare (baseRate, dsp::CutFilter::Kind::lowCut, lowCutParam->load(), lowCutEnabled (lowCutParam->load()));
        highCut.prepare (baseRate, dsp::CutFilter::Kind::highCut, highCutParam->load(), highCutEnabled (highCutParam->load()));

        for (auto* s : { &predelaySamples, &driveDb, &width })
            s->prepare (baseRate, 0.05);

        predelaySamples.setCurrentAndTarget (static_cast<float> (predelayParam->load() * 0.001 * baseRate));
        driveDb.setCurrentAndTarget (driveParam->load());
        width.setCurrentAndTarget (widthParam->load() * 0.01f);

        spring.prepare (baseRate);
        plate.prepare (baseRate);
        for (auto& dc : dcBlockers)
            dc.prepare (baseRate, 5.0);

        activeType = static_cast<ReverbType> (juce::jlimit (0, 1, static_cast<int> (typeParam->load())));
        typeGain = 1.0f;
        switching = false;
        typeStep = static_cast<float> (1.0 / (0.02 * baseRate));
    }

    void RessortProcessor::resetEffect()
    {
        lowCut.reset();
        highCut.reset();
        predelayLine.reset();
        for (auto& dc : dcBlockers)
            dc.reset();
        spring.reset();
        plate.reset();
    }

    void RessortProcessor::processEffect (juce::dsp::AudioBlock<float> block, const core::ProcessContext&)
    {
        const auto numSamples = block.getNumSamples();
        const auto numChannels = block.getNumChannels();

        // --- Reglages
        lowCut.setFrequency (lowCutParam->load(), lowCutEnabled (lowCutParam->load()));
        highCut.setFrequency (highCutParam->load(), highCutEnabled (highCutParam->load()));
        predelaySamples.setTarget (static_cast<float> (predelayParam->load() * 0.001 * baseRate));
        driveDb.setTarget (driveParam->load());
        width.setTarget (widthParam->load() * 0.01f);

        const float decay = decayParam->load();
        const float damping = dampingParam->load() * 0.01f;
        spring.setDecaySeconds (decay);
        spring.setDamping (damping);
        spring.setTension (tensionParam->load() * 0.01f);
        plate.setDecaySeconds (decay);
        plate.setDamping (damping);

        const auto requestedType = static_cast<ReverbType> (juce::jlimit (0, 1, static_cast<int> (typeParam->load())));
        if (requestedType != activeType)
            switching = true;

        // --- 1. Filtres d'entree
        lowCut.process (block);
        highCut.process (block);

        // --- 2. Drive "preampli" sur-echantillonne
        {
            auto up = oversampler().processUp (block);
            const float startDb = driveDb.getCurrent();
            const float endDb = driveDb.getTarget();
            driveDb.skip (static_cast<int> (numSamples));
            const auto numUp = up.getNumSamples();

            const bool ramp = ! juce::exactlyEqual (startDb, endDb);

            for (size_t ch = 0; ch < up.getNumChannels(); ++ch)
            {
                auto* data = up.getChannelPointer (ch);
                float g = dsp::dbToGain (startDb);
                float compensation = dsp::dbToGain (-0.5f * startDb);

                for (size_t j = 0; j < numUp; ++j)
                {
                    if (ramp && (j & 7) == 0)
                    {
                        const float db = startDb + (endDb - startDb) * static_cast<float> (j) / static_cast<float> (numUp);
                        g = dsp::dbToGain (db);
                        compensation = dsp::dbToGain (-0.5f * db);
                    }
                    data[j] = dsp::shapers::tube (data[j] * g) * compensation;
                }
            }
            oversampler().processDown (block);

            // La plaque n'a pas de passe-haut dans sa boucle : aucun continu ne doit y entrer
            for (size_t ch = 0; ch < juce::jmin (numChannels, static_cast<size_t> (2)); ++ch)
            {
                auto* data = block.getChannelPointer (ch);
                for (size_t i = 0; i < numSamples; ++i)
                    data[i] = dcBlockers[ch].process (data[i]);
                dcBlockers[ch].flush();
            }
        }

        // --- 3. Pre-delay + reverb (mono -> stereo) + largeur
        auto* left = block.getChannelPointer (0);
        auto* right = numChannels > 1 ? block.getChannelPointer (1) : nullptr;

        for (size_t i = 0; i < numSamples; ++i)
        {
            const float mono = right != nullptr ? 0.5f * (left[i] + right[i]) : left[i];
            predelayLine.write (0, mono);
            const float delayed = predelayLine.read (0, juce::jmax (4.0f, predelaySamples.getNext() + 4.0f));
            predelayLine.advance();

            float wetL = 0.0f, wetR = 0.0f;
            if (activeType == ReverbType::spring)
                spring.process (delayed, wetL, wetR);
            else
                plate.process (delayed, wetL, wetR);

            // Changement de type : fondu sortant, bascule (moteur remis a zero), fondu entrant
            if (switching)
            {
                typeGain -= typeStep;
                if (typeGain <= 0.0f)
                {
                    typeGain = 0.0f;
                    switching = false;
                    activeType = requestedType;
                    spring.reset();
                    plate.reset();
                }
            }
            else if (typeGain < 1.0f)
            {
                typeGain = juce::jmin (1.0f, typeGain + typeStep);
            }

            // Largeur : mid / side
            const float w = width.getNext();
            const float mid = 0.5f * (wetL + wetR);
            const float side = 0.5f * (wetL - wetR) * w;
            wetL = (mid + side) * typeGain;
            wetR = (mid - side) * typeGain;

            if (right != nullptr)
            {
                left[i] = wetL;
                right[i] = wetR;
            }
            else
            {
                left[i] = 0.5f * (wetL + wetR);
            }
        }
    }
}
