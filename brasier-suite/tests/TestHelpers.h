#pragma once

// Outils communs des tests hors-ligne de la suite Brasier.

#include <cmath>
#include <complex>
#include <cstdio>
#include <functional>
#include <memory>
#include <random>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_events/juce_events.h>

#include "core/EffectProcessorBase.h"

namespace brasier::tests
{
    inline constexpr double sampleRate = 48000.0;
    inline constexpr int blockSize = 512;

    //==============================================================================
    // Enregistrement des tests
    struct TestCase
    {
        const char* name;
        std::function<void()> run;
    };

    inline std::vector<TestCase>& registry()
    {
        static std::vector<TestCase> tests;
        return tests;
    }

    struct Registrar
    {
        Registrar (const char* name, std::function<void()> fn) { registry().push_back ({ name, std::move (fn) }); }
    };

    inline int& failureCount()
    {
        static int failures = 0;
        return failures;
    }

    inline void check (bool condition, const juce::String& what)
    {
        std::printf ("  [%s] %s\n", condition ? "PASS" : "FAIL", what.toRawUTF8());
        if (! condition)
            ++failureCount();
    }

    //==============================================================================
    // Parametres
    inline void setParam (juce::AudioProcessorValueTreeState& state, const juce::String& id, float value)
    {
        auto* p = state.getParameter (id);
        jassert (p != nullptr);
        p->setValueNotifyingHost (p->convertTo0to1 (value));
    }

    inline float getParam (juce::AudioProcessorValueTreeState& state, const juce::String& id)
    {
        return state.getRawParameterValue (id)->load();
    }

    template <typename Processor>
    std::unique_ptr<Processor> makeProcessor (int preparedBlock = blockSize, double rate = sampleRate)
    {
        auto p = std::make_unique<Processor>();
        p->setRateAndBufferSizeDetails (rate, preparedBlock);
        p->prepareToPlay (rate, preparedBlock);
        return p;
    }

    inline void reprepare (juce::AudioProcessor& p, int preparedBlock = blockSize, double rate = sampleRate)
    {
        p.releaseResources();
        p.setRateAndBufferSizeDetails (rate, preparedBlock);
        p.prepareToPlay (rate, preparedBlock);
    }

    //==============================================================================
    // Signaux
    inline juce::AudioBuffer<float> makeSine (int numSamples, float amplitude = 0.5f,
                                              double freqLeft = 1000.0, double freqRight = 220.0,
                                              double rate = sampleRate)
    {
        juce::AudioBuffer<float> b (2, numSamples);
        for (int i = 0; i < numSamples; ++i)
        {
            const auto t = static_cast<double> (i) / rate;
            b.setSample (0, i, amplitude * static_cast<float> (std::sin (juce::MathConstants<double>::twoPi * freqLeft * t)));
            b.setSample (1, i, amplitude * static_cast<float> (std::sin (juce::MathConstants<double>::twoPi * freqRight * t)));
        }
        return b;
    }

    inline juce::AudioBuffer<float> makeNoise (int numSamples, float amplitude, unsigned seed = 1234)
    {
        std::mt19937 rng (seed);
        std::uniform_real_distribution<float> dist (-1.0f, 1.0f);
        juce::AudioBuffer<float> b (2, numSamples);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < numSamples; ++i)
                b.setSample (ch, i, amplitude * dist (rng));
        return b;
    }

    /** Impulsions courtes espacees : utile pour mesurer des echos. */
    inline juce::AudioBuffer<float> makeImpulse (int numSamples, int position = 0, float amplitude = 1.0f)
    {
        juce::AudioBuffer<float> b (2, numSamples);
        b.clear();
        b.setSample (0, position, amplitude);
        b.setSample (1, position, amplitude);
        return b;
    }

    //==============================================================================
    // Traitement
    inline juce::AudioBuffer<float> run (juce::AudioProcessor& p, const juce::AudioBuffer<float>& input, int hostBlock = blockSize)
    {
        juce::AudioBuffer<float> out (input);
        juce::MidiBuffer midi;

        for (int start = 0; start < out.getNumSamples(); start += hostBlock)
        {
            const int n = juce::jmin (hostBlock, out.getNumSamples() - start);
            juce::AudioBuffer<float> view (out.getArrayOfWritePointers(), out.getNumChannels(), start, n);
            p.processBlock (view, midi);
        }

        return out;
    }

    //==============================================================================
    // Mesures
    inline double alignmentErrorDb (const juce::AudioBuffer<float>& in, const juce::AudioBuffer<float>& out, int delay, int skip)
    {
        double err = 0.0, ref = 0.0;
        for (int ch = 0; ch < in.getNumChannels(); ++ch)
            for (int i = skip; i < out.getNumSamples(); ++i)
            {
                const double x = (i - delay >= 0) ? in.getSample (ch, i - delay) : 0.0;
                const double y = out.getSample (ch, i);
                err += (y - x) * (y - x);
                ref += x * x;
            }
        return 10.0 * std::log10 ((err + 1.0e-30) / (ref + 1.0e-30));
    }

    inline double maxAbsDiffDelayed (const juce::AudioBuffer<float>& in, const juce::AudioBuffer<float>& out, int delay, int skip)
    {
        double m = 0.0;
        for (int ch = 0; ch < in.getNumChannels(); ++ch)
            for (int i = skip; i < out.getNumSamples(); ++i)
            {
                const double x = (i - delay >= 0) ? in.getSample (ch, i - delay) : 0.0;
                m = std::max (m, std::abs (out.getSample (ch, i) - x));
            }
        return m;
    }

    inline double rmsDb (const juce::AudioBuffer<float>& b, int start, int end = -1, int channel = -1)
    {
        if (end < 0)
            end = b.getNumSamples();
        double s = 0.0;
        int n = 0;
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
        {
            if (channel >= 0 && ch != channel)
                continue;
            for (int i = start; i < end; ++i, ++n)
                s += static_cast<double> (b.getSample (ch, i)) * b.getSample (ch, i);
        }
        return 10.0 * std::log10 (s / std::max (1, n) + 1.0e-30);
    }

    inline bool allFinite (const juce::AudioBuffer<float>& b)
    {
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
            for (int i = 0; i < b.getNumSamples(); ++i)
                if (! std::isfinite (b.getSample (ch, i)))
                    return false;
        return true;
    }

    /** Spectre de puissance (fenetre de Blackman-Harris 4 termes, lobes secondaires a -92 dB)
        d'un canal, sur 2^order echantillons a partir de 'start'. */
    inline std::vector<double> powerSpectrum (const juce::AudioBuffer<float>& b, int channel, int start, int order)
    {
        const int size = 1 << order;
        juce::dsp::FFT fft (order);
        std::vector<float> data (static_cast<size_t> (size) * 2, 0.0f);

        for (int i = 0; i < size; ++i)
        {
            const double t = juce::MathConstants<double>::twoPi * i / size;
            const double w = 0.35875 - 0.48829 * std::cos (t) + 0.14128 * std::cos (2.0 * t) - 0.01168 * std::cos (3.0 * t);
            data[static_cast<size_t> (i)] = static_cast<float> (w * b.getSample (channel, start + i));
        }

        fft.performFrequencyOnlyForwardTransform (data.data());

        std::vector<double> power (static_cast<size_t> (size / 2));
        for (size_t k = 0; k < power.size(); ++k)
            power[k] = static_cast<double> (data[k]) * data[k];
        return power;
    }

    /** Frequence fondamentale estimee par autocorrelation (pour les tests de pitch). */
    inline double estimatePitchHz (const juce::AudioBuffer<float>& b, int channel, int start, int length,
                                   double minHz = 50.0, double maxHz = 1000.0, double rate = sampleRate)
    {
        const int minLag = static_cast<int> (rate / maxHz);
        const int maxLag = static_cast<int> (rate / minHz);
        double bestScore = -1.0;
        int bestLag = minLag;
        std::vector<double> scores (static_cast<size_t> (maxLag + 2), 0.0);

        for (int lag = minLag; lag <= maxLag; ++lag)
        {
            double num = 0.0, e0 = 0.0, e1 = 0.0;
            for (int i = 0; i < length; ++i)
            {
                const double x = b.getSample (channel, start + i);
                const double y = b.getSample (channel, start + i + lag);
                num += x * y;
                e0 += x * x;
                e1 += y * y;
            }
            const double score = num / std::sqrt (e0 * e1 + 1.0e-20);
            scores[static_cast<size_t> (lag)] = score;
            if (score > bestScore) { bestScore = score; bestLag = lag; }
        }

        // Premier pic proche du meilleur (evite de prendre une sous-octave)
        for (int lag = minLag + 1; lag < maxLag; ++lag)
        {
            const auto s = scores[static_cast<size_t> (lag)];
            if (s > 0.9 * bestScore && s >= scores[static_cast<size_t> (lag - 1)] && s >= scores[static_cast<size_t> (lag + 1)])
            {
                bestLag = lag;
                break;
            }
        }

        // Interpolation parabolique
        const double a = scores[static_cast<size_t> (bestLag - 1)];
        const double c = scores[static_cast<size_t> (bestLag)];
        const double d = scores[static_cast<size_t> (bestLag + 1)];
        const double denom = a - 2.0 * c + d;
        const double offset = std::abs (denom) > 1.0e-12 ? 0.5 * (a - d) / denom : 0.0;
        return rate / (bestLag + offset);
    }

    //==============================================================================
    /**
        Batterie commune appliquee a chaque plugin :
          - automation aleatoire de tous les parametres a chaque bloc (aucun NaN, sortie bornee)
          - bypass et Mix 0 % : dry exact retarde de la latence reportee
          - blocs plus grands que prevu et blocs aleatoires
          - silence en entree -> silence en sortie (apres la queue)
          - sauvegarde / rechargement de tous les parametres
          - mono et 44,1 / 96 kHz
    */
    template <typename Processor>
    void runCommonPluginTests (const juce::String& name, double silenceSeconds = 2.0)
    {
        std::printf ("\n== %s : batterie commune ==\n", name.toRawUTF8());

        // 1. Fuzz : parametres aleatoires a chaque bloc, bruit fort en entree
        {
            auto p = makeProcessor<Processor>();
            auto input = makeNoise (static_cast<int> (sampleRate * 3.0), 0.9f, 42);
            std::mt19937 rng (3);
            std::uniform_real_distribution<float> uni (0.0f, 1.0f);
            juce::MidiBuffer midi;
            bool finite = true;
            float peak = 0.0f;

            for (int start = 0; start < input.getNumSamples(); start += blockSize)
            {
                for (auto* param : p->getParameters())
                    if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
                        if (ranged->getParameterID() != core::ids::bypass && uni (rng) < 0.3f)
                            ranged->setValueNotifyingHost (uni (rng));

                const int n = juce::jmin (blockSize, input.getNumSamples() - start);
                juce::AudioBuffer<float> view (input.getArrayOfWritePointers(), 2, start, n);
                p->processBlock (view, midi);
            }

            finite = allFinite (input);
            peak = input.getMagnitude (0, input.getNumSamples());
            std::printf ("  pic de sortie sous automation aleatoire : %.2f\n", static_cast<double> (peak));
            check (finite, name + " : automation aleatoire de tous les parametres, aucun NaN/Inf");
            // +24 dB d'effet et +24 dB de sortie au maximum : ~230 au pire sans emballement
            check (peak < 500.0f, name + " : sortie bornee (pas d'emballement)");
        }

        // 2. Bypass exact
        {
            auto p = makeProcessor<Processor>();
            setParam (p->getValueTreeState(), core::ids::bypass, 1.0f);
            reprepare (*p);
            const auto input = makeNoise (24000, 0.5f, 8);
            const auto out = run (*p, input);
            check (maxAbsDiffDelayed (input, out, p->getLatencySamples(), 0) <= 0.0,
                   name + " : bypass = entree retardee de la latence (" + juce::String (p->getLatencySamples()) + " ech.), au bit pres");
        }

        // 3. Mix 0 % exact
        {
            auto p = makeProcessor<Processor>();
            setParam (p->getValueTreeState(), core::ids::mix, 0.0f);
            reprepare (*p);
            const auto input = makeNoise (24000, 0.5f, 9);
            const auto out = run (*p, input);
            check (maxAbsDiffDelayed (input, out, p->getLatencySamples(), 0) <= 0.0, name + " : mix 0 % = dry exact");
        }

        // 4. Blocs de taille variable
        {
            auto a = makeProcessor<Processor>();
            auto b = makeProcessor<Processor>();
            const auto input = makeNoise (30000, 0.3f, 10);
            const auto outA = run (*a, input, blockSize);
            const auto outB = run (*b, input, 4096);
            check (maxAbsDiffDelayed (outA, outB, 0, 0) < 1.0e-5,
                   name + " : blocs de 4096 (> 512 prepare) = blocs de 512");

            auto c = makeProcessor<Processor>();
            juce::AudioBuffer<float> out (input);
            juce::MidiBuffer midi;
            std::mt19937 rng (11);
            for (int pos = 0; pos < out.getNumSamples();)
            {
                const int n = juce::jmin (static_cast<int> (rng() % 700) + 1, out.getNumSamples() - pos);
                juce::AudioBuffer<float> view (out.getArrayOfWritePointers(), 2, pos, n);
                c->processBlock (view, midi);
                pos += n;
            }
            check (allFinite (out) && maxAbsDiffDelayed (outA, out, 0, 0) < 1.0e-3,
                   name + " : blocs aleatoires de 1 a 700 echantillons = meme resultat");
        }

        // 5. Silence apres signal
        {
            auto p = makeProcessor<Processor>();
            const int total = static_cast<int> (sampleRate * (1.0 + silenceSeconds + 1.0));
            auto input = makeNoise (total, 0.5f, 12);
            input.clear (static_cast<int> (sampleRate), total - static_cast<int> (sampleRate));
            const auto out = run (*p, input);
            const int tailStart = total - static_cast<int> (sampleRate * 0.5);
            const float tail = out.getMagnitude (tailStart, total - tailStart);
            std::printf ("  niveau residuel apres %.1f s de silence : %.1f dBFS\n", silenceSeconds + 0.5,
                         static_cast<double> (juce::Decibels::gainToDecibels (tail, -200.0f)));
            check (tail < 1.0e-4f, name + " : le silence redevient du silence (pas d'oscillation ni de bruit)");
        }

        // 6. Sauvegarde / rechargement
        {
            auto p = makeProcessor<Processor>();
            std::mt19937 rng (13);
            std::uniform_real_distribution<float> uni (0.0f, 1.0f);
            for (auto* param : p->getParameters())
                param->setValueNotifyingHost (uni (rng));

            juce::MemoryBlock data;
            p->getStateInformation (data);
            auto q = makeProcessor<Processor>();
            q->setStateInformation (data.getData(), static_cast<int> (data.getSize()));

            bool same = true;
            const auto& pp = p->getParameters();
            const auto& qp = q->getParameters();
            for (int i = 0; i < pp.size(); ++i)
            {
                // Comparaison des valeurs "reelles" (un interrupteur a 0.86 vaut On, comme 1.0)
                auto* rp = dynamic_cast<juce::RangedAudioParameter*> (pp[i]);
                auto* rq = dynamic_cast<juce::RangedAudioParameter*> (qp[i]);
                const auto& range = rp->getNormalisableRange();
                const float vp = range.snapToLegalValue (rp->convertFrom0to1 (pp[i]->getValue()));
                const float vq = range.snapToLegalValue (rq->convertFrom0to1 (qp[i]->getValue()));
                if (std::abs (vp - vq) > 1.0e-3f * (1.0f + std::abs (vp)))
                {
                    same = false;
                    std::printf ("  parametre %s : %f au lieu de %f\n", pp[i]->getName (32).toRawUTF8(),
                                 static_cast<double> (vq), static_cast<double> (vp));
                }
            }
            check (same && pp.size() == qp.size(), name + " : tous les parametres restaures");
        }

        // 7. Mono et autres frequences d'echantillonnage
        {
            auto p = std::make_unique<Processor>();
            juce::AudioProcessor::BusesLayout mono;
            mono.inputBuses.add (juce::AudioChannelSet::mono());
            mono.outputBuses.add (juce::AudioChannelSet::mono());
            const bool accepted = p->setBusesLayout (mono);
            p->setRateAndBufferSizeDetails (44100.0, 256);
            p->prepareToPlay (44100.0, 256);
            juce::AudioBuffer<float> monoBuf (1, 44100);
            auto noise = makeNoise (44100, 0.5f, 14);
            monoBuf.copyFrom (0, 0, noise, 0, 0, 44100);
            juce::MidiBuffer midi;
            for (int pos = 0; pos < 44100; pos += 256)
            {
                juce::AudioBuffer<float> view (monoBuf.getArrayOfWritePointers(), 1, pos, juce::jmin (256, 44100 - pos));
                p->processBlock (view, midi);
            }
            check (accepted && allFinite (monoBuf), name + " : mono a 44,1 kHz");

            auto q = makeProcessor<Processor> (1024, 96000.0);
            const auto out = run (*q, makeNoise (96000, 0.5f, 15), 1024);
            check (allFinite (out), name + " : stereo a 96 kHz");
        }
    }
}

#define BRASIER_TEST(name, fn) static brasier::tests::Registrar registrar_##fn (name, fn)
