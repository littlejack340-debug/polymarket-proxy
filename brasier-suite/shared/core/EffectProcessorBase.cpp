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
          options (std::move (opts))
    {
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
        const int maxLatency = (options.common.withOversampling ? overSampler.getMaxLatencySamples() : 0)
                             + getEffectLatencySamples();

        dryWet.prepare (sampleRate, numChannels, preparedBlockSize, maxLatency);
        dryWet.setMixLaw (options.mixLaw);

        autoGain.prepare (sampleRate);
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
        const int osLatency = options.common.withOversampling ? overSampler.getLatencySamples() : 0;
        return osLatency + getEffectLatencySamples();
    }

    void EffectProcessorBase::timerCallback()
    {
        const int latency = pendingLatency.load();

        if (latency != getLatencySamples())
            setLatencySamples (latency);
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

        const int latency = computeTotalLatency();
        dryWet.setLatency (latency);
        pendingLatency.store (latency);   // reporte a l'hote par timerCallback()
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

        // Cibles des parametres communs. Le bypass se fait en douceur :
        // mix -> 0 (dry seul, deja retarde) et output -> 0 dB.
        const bool bypassed = forceBypass || bypassValue->load() > 0.5f;
        dryWet.setMix (bypassed ? 0.0f : mixParam->load() * 0.01f);
        outputGain.setTarget (bypassed ? 1.0f : dsp::dbToGain (outputParam->load()));
        autoGain.setEnabled (autoGainParam->load() > 0.5f);

        const int numChannels = juce::jmin (context.numChannels, buffer.getNumChannels());
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
        }
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

        if (auto xml = state.createXml())
            copyXmlToBinary (*xml, destData);
    }

    void EffectProcessorBase::setStateInformation (const void* data, int sizeInBytes)
    {
        if (auto xml = getXmlFromBinary (data, sizeInBytes))
            if (xml->hasTagName (apvts.state.getType()))
                apvts.replaceState (juce::ValueTree::fromXml (*xml));
    }
}
