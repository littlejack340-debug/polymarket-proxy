#include "RubanProcessor.h"
#include "FactoryPresets.h"

#include "dsp/MathUtils.h"
#include "dsp/TempoSync.h"
#include "dsp/Waveshapers.h"
#include "ui/BrasierEditor.h"

namespace brasier::ruban
{
    namespace
    {
        constexpr double rampSeconds = 0.03;
        constexpr double glideSeconds = 0.25;      // inertie de la tete de lecture
        constexpr float  maxDriveDb = 24.0f;       // saturation 100 % = +24 dB dans la bande
        constexpr float  maxWowMs = 4.0f;          // ~ +/-25 cents de derive a 100 %
        constexpr float  maxFlutterMs = 0.25f;     // ~ +/-1 % de vibrato rapide a 100 %
        constexpr float  maxSpreadMs = 20.0f;      // decalage du canal droit (mode stereo)
        constexpr int    readMargin = 4;           // echantillons de securite pour l'interpolation

        core::EffectProcessorBase::Options makeOptions()
        {
            core::EffectProcessorBase::Options o;
            o.name = "Brasier Ruban";
            o.common.withOversampling = true;
            o.common.defaultOversamplingIndex = 1;    // 2x : la saturation est douce
            o.common.defaultMixPercent = 30.0f;
            o.mixLaw = dsp::DryWetMixer::MixLaw::equalPower;
            o.autoGainMode = core::EffectProcessorBase::AutoGainMode::fixed;
            o.oversamplingLatencyHandledByEffect = true;
            o.tailSeconds = 10.0;
            o.factoryPresets = makeFactoryPresets();
            return o;
        }
    }

    juce::AudioProcessorValueTreeState::ParameterLayout RubanProcessor::createParameters()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;
        layout.add (core::makeBoolParameter (ids::sync, "Sync", true));
        layout.add (core::makeTimeParameter (ids::time, "Time", 10.0f, 2000.0f, 350.0f));
        layout.add (core::makeChoiceParameter (ids::division, "Division",
                                               dsp::tempo::divisionNames (16), dsp::tempo::quarterIndex));
        layout.add (core::makePercentParameter (ids::feedback, "Feedback", 35.0f, 110.0f));
        layout.add (core::makePercentParameter (ids::saturation, "Saturation", 25.0f));
        layout.add (core::makeFrequencyParameter (ids::lowCut, "Low Cut", 20.0f, 2000.0f, 150.0f));
        layout.add (core::makeFrequencyParameter (ids::highCut, "High Cut", 1000.0f, 20000.0f, 5000.0f));
        layout.add (core::makePercentParameter (ids::wow, "Wow", 15.0f));
        layout.add (core::makePercentParameter (ids::flutter, "Flutter", 10.0f));
        layout.add (core::makeChoiceParameter (ids::mode, "Mode", getModeNames(), 1));
        layout.add (core::makePercentParameter (ids::spread, "Spread", 0.0f));
        layout.add (core::makePercentParameter (ids::duck, "Ducking", 0.0f));
        layout.add (core::makeDecibelParameter (ids::duckThreshold, "Duck Threshold", -60.0f, 0.0f, -30.0f));
        return layout;
    }

    RubanProcessor::RubanProcessor()
        : core::EffectProcessorBase (makeOptions(), createParameters())
    {
        syncParam          = apvts.getRawParameterValue (ids::sync);
        timeParam          = apvts.getRawParameterValue (ids::time);
        divisionParam      = apvts.getRawParameterValue (ids::division);
        feedbackParam      = apvts.getRawParameterValue (ids::feedback);
        saturationParam    = apvts.getRawParameterValue (ids::saturation);
        lowCutParam        = apvts.getRawParameterValue (ids::lowCut);
        highCutParam       = apvts.getRawParameterValue (ids::highCut);
        wowParam           = apvts.getRawParameterValue (ids::wow);
        flutterParam       = apvts.getRawParameterValue (ids::flutter);
        modeParam          = apvts.getRawParameterValue (ids::mode);
        spreadParam        = apvts.getRawParameterValue (ids::spread);
        duckParam          = apvts.getRawParameterValue (ids::duck);
        duckThresholdParam = apvts.getRawParameterValue (ids::duckThreshold);
    }

    juce::AudioProcessorEditor* RubanProcessor::createEditor()
    {
        using namespace ui;
        using S = ControlSpec::Style;
        EditorSpec e;
        e.pluginName = "RUBAN";
        e.tagline = fr ("delay à bande");
        e.accent = juce::Colour (0xffc99a4a);
        e.rows = {
            { SectionSpec { "Temps", { { ids::sync, S::toggle }, { ids::time, S::knob, false, {}, ids::sync, 0 }, { ids::division, S::selector, false, {}, ids::sync, 1 } }, 1.3f },
              SectionSpec { "Bande", { { ids::feedback, S::knob }, { ids::saturation, S::knob }, { ids::wow, S::knob }, { ids::flutter, S::knob } }, 1.7f } },
            { SectionSpec { "Filtres de boucle", { { ids::lowCut, S::knob }, { ids::highCut, S::knob } }, 1.0f },
              SectionSpec { fr ("STÉRÉO"), { { ids::mode, S::selector }, { ids::spread, S::knob, false, {}, ids::mode, 1 } }, 1.0f },
              SectionSpec { "Ducking", { { ids::duck, S::knob }, { ids::duckThreshold, S::knob } }, 1.0f } } };
        return new BrasierEditor (*this, std::move (e));
    }

    float RubanProcessor::getStaticAutoGainDb() const
    {
        // Compense le gain de "makeup" applique au wet quand on pousse la saturation
        return -0.25f * maxDriveDb * saturationParam->load() * 0.01f;
    }

    //==============================================================================
    double RubanProcessor::computeTargetDelaySeconds (const core::ProcessContext& ctx) const noexcept
    {
        const double seconds = syncParam->load() > 0.5f
            ? dsp::tempo::divisionSeconds (static_cast<int> (divisionParam->load()), ctx.bpm)
            : timeParam->load() * 0.001;

        return juce::jlimit (minDelaySeconds, maxDelaySeconds, seconds);
    }

    void RubanProcessor::prepareEffect (const core::EffectSpec& spec)
    {
        baseRate = spec.sampleRate;
        subBlockSize = juce::jlimit (1, 16, spec.maxBlockSize);

        const auto maxModulation = (maxWowMs + maxFlutterMs + maxSpreadMs) * 0.001 * baseRate;
        tape.prepare (2, static_cast<int> (std::ceil (maxDelaySeconds * baseRate + maxModulation)) + subBlockSize + 16);
        loopBuffer.setSize (2, subBlockSize);

        loopLowCut.prepare (baseRate, dsp::SvfType::highpass, lowCutParam->load(), 0.7071f);
        loopHighCut.prepare (baseRate, dsp::SvfType::lowpass, highCutParam->load(), 0.6f);

        for (auto* s : { &feedback, &driveDb, &spreadSamples, &wowDepth, &flutterDepth })
            s->prepare (baseRate, rampSeconds);

        feedback.setCurrentAndTarget (feedbackParam->load() * 0.01f);
        driveDb.setCurrentAndTarget (maxDriveDb * saturationParam->load() * 0.01f);
        spreadSamples.setCurrentAndTarget (static_cast<float> (spreadParam->load() * 0.01f * maxSpreadMs * 0.001f * baseRate));
        wowDepth.setCurrentAndTarget (static_cast<float> (wowParam->load() * 0.01f * maxWowMs * 0.001f * baseRate));
        flutterDepth.setCurrentAndTarget (static_cast<float> (flutterParam->load() * 0.01f * maxFlutterMs * 0.001f * baseRate));

        delayGlideCoeff = dsp::onePoleCoefficient (baseRate, glideSeconds);
        snapDelayTime = true;

        for (int ch = 0; ch < 2; ++ch)
        {
            const auto seed = static_cast<uint32_t> (0xBADC0DE + 977 * ch);
            wowSine[ch].prepare (baseRate, seed);
            wowDrift[ch].prepare (baseRate, seed + 1);
            flutterA[ch].prepare (baseRate, seed + 2);
            flutterB[ch].prepare (baseRate, seed + 3);

            wowSine[ch].setShape (dsp::Lfo::Shape::sine);
            wowDrift[ch].setShape (dsp::Lfo::Shape::smoothRandom);
            flutterA[ch].setShape (dsp::Lfo::Shape::sine);
            flutterB[ch].setShape (dsp::Lfo::Shape::sine);

            wowSine[ch].setFrequency (0.55);
            wowDrift[ch].setFrequency (0.35);
            flutterA[ch].setFrequency (6.3);
            flutterB[ch].setFrequency (9.7);

            // Le canal droit est dephase : la bande "respire" differemment de chaque cote
            wowSine[ch].reset (ch * 0.25);
            flutterA[ch].reset (ch * 0.33);
            flutterB[ch].reset (ch * 0.61);
        }

        duckEnvelope.prepare (baseRate);
        duckEnvelope.setTimes (0.005, 0.25);
        duckSmoothCoeff = dsp::onePoleCoefficient (baseRate, 0.03);
        duckGain = duckTargetGain = 1.0f;
    }

    void RubanProcessor::resetEffect()
    {
        tape.reset();
        loopBuffer.clear();
        loopLowCut.reset();
        loopHighCut.reset();
        duckEnvelope.reset();
        duckGain = duckTargetGain = 1.0f;
        snapDelayTime = true;
    }

    //==============================================================================
    void RubanProcessor::saturateLoop (juce::dsp::AudioBlock<float> loop, int numLoopChannels) noexcept
    {
        // "Enregistrement" sur la bande : tanh(g.x) / g -> gain unitaire a bas niveau
        // (la boucle reste stable), les crêtes sont arrondies d'autant plus que g est grand.
        auto up = oversampler().processUp (loop);
        const auto numUp = up.getNumSamples();
        const auto upChannels = static_cast<size_t> (juce::jmin (numLoopChannels, static_cast<int> (up.getNumChannels())));

        const float startDb = driveDb.getCurrent();
        const float endDb = driveDb.getTarget();
        const bool ramp = driveDb.isSmoothing();

        for (size_t ch = 0; ch < upChannels; ++ch)
        {
            auto* data = up.getChannelPointer (ch);
            float g = dsp::dbToGain (startDb);
            float invG = 1.0f / g;

            for (size_t i = 0; i < numUp; ++i)
            {
                if (ramp && (i & 7) == 0)
                {
                    const float t = static_cast<float> (i) / static_cast<float> (numUp);
                    g = dsp::dbToGain (startDb + t * (endDb - startDb));
                    invG = 1.0f / g;
                }
                data[i] = dsp::shapers::fastTanh (g * data[i]) * invG;
            }
        }

        oversampler().processDown (loop);
    }

    void RubanProcessor::processEffect (juce::dsp::AudioBlock<float> block, const core::ProcessContext& ctx)
    {
        const auto numSamples = static_cast<int> (block.getNumSamples());
        const bool monoBus = block.getNumChannels() < 2;
        const auto mode = monoBus ? StereoMode::mono
                                  : static_cast<StereoMode> (juce::jlimit (0, 2, static_cast<int> (modeParam->load())));
        const int loopChannels = mode == StereoMode::mono ? 1 : 2;

        // --- Cibles des parametres
        const double targetDelay = computeTargetDelaySeconds (ctx) * baseRate;
        if (snapDelayTime)
        {
            delaySamples = targetDelay;   // premier bloc : pas de glissement depuis une valeur arbitraire
            snapDelayTime = false;
        }
        feedback.setTarget (feedbackParam->load() * 0.01f);
        driveDb.setTarget (maxDriveDb * saturationParam->load() * 0.01f);
        spreadSamples.setTarget (mode == StereoMode::stereo
                                     ? static_cast<float> (spreadParam->load() * 0.01f * maxSpreadMs * 0.001f * baseRate)
                                     : 0.0f);
        wowDepth.setTarget (static_cast<float> (wowParam->load() * 0.01f * maxWowMs * 0.001f * baseRate));
        flutterDepth.setTarget (static_cast<float> (flutterParam->load() * 0.01f * maxFlutterMs * 0.001f * baseRate));
        loopLowCut.setFrequency (lowCutParam->load());
        loopHighCut.setFrequency (highCutParam->load());

        const float duckAmount = duckParam->load() * 0.01f;
        const float duckThresholdDb = duckThresholdParam->load();
        const float makeup = dsp::dbToGain (0.25f * driveDb.getTarget());

        // La saturation sur-echantillonnee retarde le signal de osLatency : on lit la
        // bande d'autant plus tot pour que l'echo tombe pile au temps demande.
        const double osLatency = static_cast<double> (oversampler().getLatencySamples());
        const double minRead = static_cast<double> (subBlockSize + readMargin);

        auto* outL = block.getChannelPointer (0);
        auto* outR = monoBus ? nullptr : block.getChannelPointer (1);
        auto* loopL = loopBuffer.getWritePointer (0);
        auto* loopR = loopBuffer.getWritePointer (1);

        for (int start = 0; start < numSamples; start += subBlockSize)
        {
            const int n = juce::jmin (subBlockSize, numSamples - start);

            // 1. Lecture des tetes (retard lisse + wow + flutter), interpolation cubique
            for (int i = 0; i < n; ++i)
            {
                delaySamples += delayGlideCoeff * (targetDelay - delaySamples);
                const float wowAmount = wowDepth.getNext();
                const float flutterAmount = flutterDepth.getNext();
                const float spread = spreadSamples.getNext();

                for (int ch = 0; ch < loopChannels; ++ch)
                {
                    const float wow = wowAmount * (0.6f * wowSine[ch].next() + 0.4f * wowDrift[ch].next());
                    const float flutter = flutterAmount * (0.7f * flutterA[ch].next() + 0.3f * flutterB[ch].next());
                    const double d = delaySamples + (ch == 1 ? spread : 0.0f) + wow + flutter - osLatency;
                    loopBuffer.setSample (ch, i, tape.read (ch, juce::jmax (minRead, d), i));
                }
            }

            // 2. Filtres de la boucle (la bande perd du grave et de l'aigu a chaque passage)
            for (int i = 0; i < n; ++i)
            {
                loopLowCut.tickParameters();
                loopHighCut.tickParameters();
                for (int ch = 0; ch < loopChannels; ++ch)
                {
                    const float x = loopBuffer.getSample (ch, i);
                    loopBuffer.setSample (ch, i, loopHighCut.processSample (ch, loopLowCut.processSample (ch, x)));
                }
            }

            // 3. Saturation sur-echantillonnee
            juce::dsp::AudioBlock<float> loopBlock (loopBuffer.getArrayOfWritePointers(),
                                                    static_cast<size_t> (loopChannels), 0, static_cast<size_t> (n));
            saturateLoop (loopBlock, loopChannels);
            driveDb.skip (n);

            // 4. Ecriture sur la bande (entree + feedback), sortie wet et ducking
            for (int i = 0; i < n; ++i)
            {
                const int s = start + i;
                const float inL = outL[s];
                const float inR = outR != nullptr ? outR[s] : inL;
                const float fb = feedback.getNext();
                const float yL = loopL[i];
                const float yR = loopChannels > 1 ? loopR[i] : yL;

                switch (mode)
                {
                    case StereoMode::mono:
                        tape.write (0, 0.5f * (inL + inR) + fb * yL, i);
                        break;
                    case StereoMode::stereo:
                        tape.write (0, inL + fb * yL, i);
                        tape.write (1, inR + fb * yR, i);
                        break;
                    case StereoMode::pingPong:
                        // L'entree part a gauche, chaque repetition change de cote
                        tape.write (0, 0.5f * (inL + inR) + fb * yR, i);
                        tape.write (1, fb * yL, i);
                        break;
                }

                // Ducking : le wet baisse quand la voix (l'entree) joue
                const float env = duckEnvelope.process (juce::jmax (std::abs (inL), std::abs (inR)));
                if (i == n - 1)
                {
                    const float overDb = dsp::gainToDb (env) - duckThresholdDb;
                    const float reductionDb = duckAmount * juce::jlimit (0.0f, 36.0f, overDb * 1.5f);
                    duckTargetGain = dsp::dbToGain (-reductionDb);
                }
                duckGain += duckSmoothCoeff * (duckTargetGain - duckGain);

                const float gain = makeup * duckGain;
                outL[s] = yL * gain;
                if (outR != nullptr)
                    outR[s] = yR * gain;
            }

            tape.advance (n);
        }
    }
}
