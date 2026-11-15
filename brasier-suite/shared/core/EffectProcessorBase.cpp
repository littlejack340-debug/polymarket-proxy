#include "EffectProcessorBase.h"
#include "dsp/MathUtils.h"

namespace brasier::core
{
    namespace
    {
        constexpr int stateVersion = 1;
    }

    juce::AudioProcessor::BusesProperties EffectProcessorBase::makeBuses()
    {
        return BusesProperties()
            .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
            .withOutput ("Output", juce::AudioChannelSet::stereo(), true);
    }

    juce::AudioProcessorValueTreeState::ParameterLayout
    EffectProcessorBase::buildLayout (juce::AudioProcessorValueTreeState::ParameterLayout pluginParameters,
                                      const CommonParameterOptions& common)
    {
        addCommonParameters (pluginParameters, common);
        return pluginParameters;
    }

    EffectProcessorBase::EffectProcessorBase (Options opts,
                                              juce::AudioProcessorValueTreeState::ParameterLayout pluginParameters)
        : juce::AudioProcessor (makeBuses()),
          apvts (*this, nullptr, "BrasierState", buildLayout (std::move (pluginParameters), opts.common)),
          options (std::move (opts)),
          presets (apvts, options.name, options.factoryPresets)
    {
        presets.refreshUserPresets();

        mixParam      = apvts.getRawParameterValue (ids::mix);
        outputParam   = apvts.getRawParameterValue (ids::output);
        autoGainParam = apvts.getRawParameterValue (ids::autoGain);
        bypassValue   = apvts.getRawParameterValue (ids::bypass);
        bypassParameter = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (ids::bypass));

        if (options.common.withOversampling)
            oversamplingParam = apvts.getRawParameterValue (ids::oversampling);

        jassert (mixParam != nullptr && outputParam != nullptr && autoGainParam != nullptr
                 && bypassValue != nullptr && bypassParameter != nullptr);

        // Le thread message verifie regulierement si la latence a change
        // (changement d'oversampling) pour la reporter a l'hote.
        startTimerHz (10);
    }

    EffectProcessorBase::~EffectProcessorBase()
    {
        stopTimer();
    }

    //==============================================================================
    bool EffectProcessorBase::isBusesLayoutSupported (const BusesLayout& layouts) const
    {
        const auto& in  = layouts.getMainInputChannelSet();
        const auto& out = layouts.getMainOutputChannelSet();

        if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
            return false;

        return in == out;
    }

    void EffectProcessorBase::prepareToPlay (double sampleRate, int samplesPerBlock)
    {
        preparedBlockSize = juce::jmax (1, samplesPerBlock);
        const int numChannels = juce::jlimit (1, 2, getMainBusNumOutputChannels());

        context.sampleRate = sampleRate;
        context.numChannels = numChannels;

        // 1. Oversampling : les trois moteurs sont alloues ici
        overSampler.prepare (numChannels, preparedBlockSize);

        if (oversamplingParam != nullptr)
            overSampler.setFactorIndex (static_cast<int> (oversamplingParam->load()));
        else
            overSampler.setFactorIndex (0);

        context.oversamplingFactor = overSampler.getFactor();
        context.oversampledRate = sampleRate * context.oversamplingFactor;
        context.oversamplingChanged = false;

        // 2. DSP propre au plugin
        EffectSpec spec;
        spec.sampleRate = sampleRate;
        spec.maxBlockSize = preparedBlockSize;
        spec.numChannels = numChannels;
        spec.maxOversamplingFactor = options.common.withOversampling ? dsp::Oversampler::maxFactor : 1;
        prepareEffect (spec);

        // 3. Chemin dry : retard maximal = pire cas d'oversampling + latence de l'effet
        const bool osAddsLatency = options.common.withOversampling && ! options.oversamplingLatencyHandledByEffect;
        const int maxLatency = (osAddsLatency ? overSampler.getMaxLatencySamples() : 0)
                             + getMaxEffectLatencySamples();

        dryWet.prepare (sampleRate, numChannels, preparedBlockSize, maxLatency);
        dryWet.setMixLaw (options.mixLaw);

        autoGain.prepare (sampleRate);
        autoGain.setStaticMode (options.autoGainMode == AutoGainMode::fixed);
        outputGain.prepare (sampleRate, 0.03);

        // 4. Valeurs initiales sans rampe
        const bool bypassed = bypassValue->load() > 0.5f;
        dryWet.setMixImmediate (bypassed ? 0.0f : mixParam->load() * 0.01f);
        outputGain.setCurrentAndTarget (bypassed ? 1.0f : dsp::dbToGain (outputParam->load()));
        autoGain.setEnabled (autoGainParam->load() > 0.5f);

        // 5. Latence reportee a l'hote
        const int latency = computeTotalLatency();
        dryWet.setLatency (latency);
        pendingLatency.store (latency);
        setLatencySamples (latency);

        resetEffect();
        isPrepared = true;
    }

    void EffectProcessorBase::reset()
    {
        overSampler.reset();
        dryWet.reset();
        autoGain.reset();
        resetEffect();
    }

    int EffectProcessorBase::computeTotalLatency() const noexcept
    {
        const bool osAddsLatency = options.common.withOversampling && ! options.oversamplingLatencyHandledByEffect;
        const int osLatency = osAddsLatency ? overSampler.getLatencySamples() : 0;
        return osLatency + getEffectLatencySamples();
    }

    void EffectProcessorBase::timerCallback()
    {
        const int latency = pendingLatency.load();

        if (latency != getLatencySamples())
            setLatencySamples (latency);

        const int program = pendingProgram.exchange (-1);
        if (program >= 0)
            presets.loadFactoryPreset (program);
    }

    //==============================================================================
    int EffectProcessorBase::getNumPrograms()
    {
        return juce::jmax (1, presets.getNumFactoryPresets());
    }

    int EffectProcessorBase::getCurrentProgram()
    {
        return presets.isFactory (presets.getCurrentIndex()) ? presets.getCurrentIndex() : 0;
    }

    void EffectProcessorBase::setCurrentProgram (int index)
    {
        if (! juce::isPositiveAndBelow (index, presets.getNumFactoryPresets()))
            return;

        // Les presets passent par les parametres : on ne le fait que sur le thread message
        if (juce::MessageManager::existsAndIsCurrentThread())
            presets.loadFactoryPreset (index);
        else
            pendingProgram.store (index);
    }

    const juce::String EffectProcessorBase::getProgramName (int index)
    {
        if (! juce::isPositiveAndBelow (index, presets.getNumFactoryPresets()))
            return "Init";

        const auto& p = presets.getFactoryPreset (index);
        return p.category.isNotEmpty() ? p.category + " - " + p.name : p.name;
    }

    //==============================================================================
    void EffectProcessorBase::measureLevels (const juce::AudioBuffer<float>& buffer, int numChannels, bool input) noexcept
    {
        const int n = buffer.getNumSamples();
        if (n == 0)
            return;

        for (int ch = 0; ch < 2; ++ch)
        {
            const int src = juce::jmin (ch, numChannels - 1);
            const float* data = buffer.getReadPointer (src);
            float sum = 0.0f;
            for (int i = 0; i < n; ++i)
                sum += data[i] * data[i];
            (input ? inputLevel : outputLevel)[ch].store (sum / static_cast<float> (n), std::memory_order_relaxed);
        }

        if (! input)
            outputPeak.store (buffer.getMagnitude (0, n), std::memory_order_relaxed);
    }

    EffectProcessorBase::MeterLevels EffectProcessorBase::getMeterLevels() const noexcept
    {
        MeterLevels m;
        for (int ch = 0; ch < 2; ++ch)
        {
            m.inputMeanSquare[ch] = inputLevel[ch].load (std::memory_order_relaxed);
            m.outputMeanSquare[ch] = outputLevel[ch].load (std::memory_order_relaxed);
        }
        m.outputPeak = outputPeak.load (std::memory_order_relaxed);
        return m;
    }

    float EffectProcessorBase::getEditorScale() const
    {
        return static_cast<float> (apvts.state.getProperty ("editorScale", 1.0f));
    }

    void EffectProcessorBase::setEditorScale (float scale)
    {
        apvts.state.setProperty ("editorScale", scale, nullptr);
    }

    //==============================================================================
    void EffectProcessorBase::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
    {
        processInternal (buffer, false);
    }

    void EffectProcessorBase::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
    {
        // Bypass demande par l'hote : on sort le dry retarde de la latence,
        // pour que les autres pistes restent alignees.
        processInternal (buffer, true);
    }

    void EffectProcessorBase::updateHostInfo() noexcept
    {
        if (auto* hostPlayHead = getPlayHead())
        {
            if (const auto position = hostPlayHead->getPosition())
            {
                if (const auto bpm = position->getBpm())
                    context.bpm = juce::jlimit (20.0, 999.0, *bpm);

                if (const auto ppq = position->getPpqPosition())
                    context.ppqPosition = *ppq;

                context.isPlaying = position->getIsPlaying();
            }
        }
    }

    void EffectProcessorBase::updateOversampling() noexcept
    {
        context.oversamplingChanged = false;

        if (oversamplingParam == nullptr)
            return;

        const int requested = static_cast<int> (oversamplingParam->load());

        if (requested == overSampler.getFactorIndex())
            return;

        overSampler.setFactorIndex (requested);

        context.oversamplingFactor = overSampler.getFactor();
        context.oversampledRate = context.sampleRate * context.oversamplingFactor;
        context.oversamplingChanged = true;
    }

    void EffectProcessorBase::updateLatency() noexcept
    {
        updateEffectLatency();

        const int latency = computeTotalLatency();
        if (latency != dryWet.getLatency())
        {
            dryWet.setLatency (latency);
            pendingLatency.store (latency);   // reporte a l'hote par timerCallback()
        }
    }

    void EffectProcessorBase::processInternal (juce::AudioBuffer<float>& buffer, bool forceBypass) noexcept
    {
        juce::ScopedNoDenormals noDenormals;

        const int numSamples = buffer.getNumSamples();
        const int numInputs  = getTotalNumInputChannels();
        const int numOutputs = getTotalNumOutputChannels();

        for (int ch = numInputs; ch < numOutputs; ++ch)
            buffer.clear (ch, 0, numSamples);

        if (! isPrepared || numSamples == 0)
            return;

        updateHostInfo();
        updateOversampling();
        updateLatency();

        // Cibles des parametres communs. Le bypass se fait en douceur :
        // mix -> 0 (dry seul, deja retarde) et output -> 0 dB.
        const bool bypassed = forceBypass || bypassValue->load() > 0.5f;
        dryWet.setMix (bypassed ? 0.0f : mixParam->load() * 0.01f);
        outputGain.setTarget (bypassed ? 1.0f : dsp::dbToGain (outputParam->load()));
        autoGain.setEnabled (autoGainParam->load() > 0.5f);
        if (options.autoGainMode == AutoGainMode::fixed)
            autoGain.setStaticGain (dsp::dbToGain (getStaticAutoGainDb()));

        const int numChannels = juce::jmin (context.numChannels, buffer.getNumChannels());
        measureLevels (buffer, numChannels, true);

        juce::dsp::AudioBlock<float> fullBlock (buffer.getArrayOfWritePointers(),
                                                static_cast<size_t> (numChannels),
                                                static_cast<size_t> (numSamples));

        // Certains hotes envoient parfois des blocs plus grands qu'annonce :
        // on decoupe pour ne jamais depasser les buffers pre-alloues.
        for (int start = 0; start < numSamples; start += preparedBlockSize)
        {
            const int length = juce::jmin (preparedBlockSize, numSamples - start);
            processChunk (fullBlock.getSubBlock (static_cast<size_t> (start), static_cast<size_t> (length)));
            context.oversamplingChanged = false;

            // La position du transport avance avec chaque morceau (LFO synchronises)
            context.ppqPosition += static_cast<double> (length) / context.sampleRate * context.bpm / 60.0;
        }

        measureLevels (buffer, numChannels, false);
    }

    void EffectProcessorBase::processChunk (juce::dsp::AudioBlock<float> block) noexcept
    {
        // 1. Memorise le dry (retarde de la latence du chemin wet)
        dryWet.pushDry (block);

        // 2. Effet (wet), en place
        processEffect (block, context);

        // 3. Auto-gain : le wet est ramene au niveau de l'entree
        autoGain.process (dryWet.getAlignedDry(), block);

        // 4. Melange dry / wet
        dryWet.mixWet (block);

        // 5. Gain de sortie lisse
        const auto numSamples  = block.getNumSamples();
        const auto numChannels = block.getNumChannels();

        if (! outputGain.isSmoothing())
        {
            const float g = outputGain.getTarget();
            if (! juce::exactlyEqual (g, 1.0f))
                block.multiplyBy (g);
            return;
        }

        for (size_t i = 0; i < numSamples; ++i)
        {
            const float g = outputGain.getNext();
            for (size_t ch = 0; ch < numChannels; ++ch)
                block.getChannelPointer (ch)[i] *= g;
        }
    }

    //==============================================================================
    void EffectProcessorBase::getStateInformation (juce::MemoryBlock& destData)
    {
        auto state = apvts.copyState();
        state.setProperty ("stateVersion", stateVersion, nullptr);
        state.setProperty ("presetIndex", presets.getCurrentIndex(), nullptr);

        if (auto xml = state.createXml())
            copyXmlToBinary (*xml, destData);
    }

    void EffectProcessorBase::setStateInformation (const void* data, int sizeInBytes)
    {
        if (auto xml = getXmlFromBinary (data, sizeInBytes))
            if (xml->hasTagName (apvts.state.getType()))
            {
                apvts.replaceState (juce::ValueTree::fromXml (*xml));
                presets.setCurrentIndexSilently (static_cast<int> (apvts.state.getProperty ("presetIndex", -1)));
            }
    }
}
