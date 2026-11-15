// Tests hors-ligne de la suite Brasier.
// Chaque test affiche PASS/FAIL ; le code de sortie vaut le nombre d'echecs.

#include <cmath>
#include <cstdio>
#include <functional>
#include <memory>
#include <random>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>

#include "Gain/Source/GainProcessor.h"

namespace
{
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;

    int failures = 0;

    void check (bool condition, const juce::String& what)
    {
        std::printf ("  [%s] %s\n", condition ? "PASS" : "FAIL", what.toRawUTF8());
        if (! condition)
            ++failures;
    }

    void setParam (juce::AudioProcessorValueTreeState& state, const juce::String& id, float value)
    {
        auto* p = state.getParameter (id);
        jassert (p != nullptr);
        p->setValueNotifyingHost (p->convertTo0to1 (value));
    }

    float getParam (juce::AudioProcessorValueTreeState& state, const juce::String& id)
    {
        return state.getRawParameterValue (id)->load();
    }

    std::unique_ptr<brasier::gain::GainProcessor> makeGain (int preparedBlock = blockSize)
    {
        auto p = std::make_unique<brasier::gain::GainProcessor>();
        p->setRateAndBufferSizeDetails (sampleRate, preparedBlock);
        p->prepareToPlay (sampleRate, preparedBlock);
        return p;
    }

    /** Re-prepare (applique un nouveau facteur d'oversampling comme le ferait l'hote). */
    void reprepare (juce::AudioProcessor& p, int preparedBlock = blockSize)
    {
        p.releaseResources();
        p.prepareToPlay (sampleRate, preparedBlock);
    }

    juce::AudioBuffer<float> makeSine (int numSamples, float amplitude = 0.5f)
    {
        juce::AudioBuffer<float> b (2, numSamples);
        for (int i = 0; i < numSamples; ++i)
        {
            const auto t = static_cast<double> (i) / sampleRate;
            b.setSample (0, i, amplitude * static_cast<float> (std::sin (2.0 * juce::MathConstants<double>::pi * 1000.0 * t)));
            b.setSample (1, i, amplitude * static_cast<float> (std::sin (2.0 * juce::MathConstants<double>::pi * 220.0 * t)));
        }
        return b;
    }

    juce::AudioBuffer<float> makeNoise (int numSamples, float amplitude, unsigned seed = 1234)
    {
        std::mt19937 rng (seed);
        std::uniform_real_distribution<float> dist (-1.0f, 1.0f);
        juce::AudioBuffer<float> b (2, numSamples);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < numSamples; ++i)
                b.setSample (ch, i, amplitude * dist (rng));
        return b;
    }

    /** Traite 'input' par blocs de hostBlock echantillons et renvoie la sortie. */
    juce::AudioBuffer<float> run (juce::AudioProcessor& p, const juce::AudioBuffer<float>& input, int hostBlock = blockSize)
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

    /** Erreur RMS (dB, relative au RMS de l'entree) entre sortie et entree retardee de 'delay'. */
    double alignmentErrorDb (const juce::AudioBuffer<float>& in, const juce::AudioBuffer<float>& out, int delay, int skip)
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

    double maxAbsDiffDelayed (const juce::AudioBuffer<float>& in, const juce::AudioBuffer<float>& out, int delay, int skip)
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

    double rmsDb (const juce::AudioBuffer<float>& b, int start)
    {
        double s = 0.0;
        int n = 0;
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
            for (int i = start; i < b.getNumSamples(); ++i, ++n)
                s += static_cast<double> (b.getSample (ch, i)) * b.getSample (ch, i);
        return 10.0 * std::log10 (s / std::max (1, n) + 1.0e-30);
    }

    bool allFinite (const juce::AudioBuffer<float>& b)
    {
        for (int ch = 0; ch < b.getNumChannels(); ++ch)
            for (int i = 0; i < b.getNumSamples(); ++i)
                if (! std::isfinite (b.getSample (ch, i)))
                    return false;
        return true;
    }

    //==============================================================================
    void testLatencyAlignment()
    {
        std::printf ("\n== Latence et alignement dry/wet (Brasier Gain) ==\n");
        const auto input = makeSine (static_cast<int> (sampleRate));
        const char* names[] = { "1x", "2x", "4x", "8x" };

        for (int os = 0; os < 4; ++os)
        {
            auto p = makeGain();
            auto& s = p->getValueTreeState();
            setParam (s, brasier::core::ids::oversampling, static_cast<float> (os));
            setParam (s, brasier::core::ids::mix, 50.0f);
            reprepare (*p);

            const int latency = p->getLatencySamples();
            const auto out = run (*p, input);
            const double e0 = alignmentErrorDb (input, out, latency, 4096);
            const double eMinus = alignmentErrorDb (input, out, latency - 1, 4096);
            const double ePlus  = alignmentErrorDb (input, out, latency + 1, 4096);

            std::printf ("  %s : latence %d ech., erreur %.1f dB (L-1 : %.1f dB, L+1 : %.1f dB)\n",
                         names[os], latency, e0, eMinus, ePlus);

            check (os == 0 ? latency == 0 : latency > 0, juce::String (names[os]) + " : latence coherente");
            check (e0 < -60.0, juce::String (names[os]) + " : mix 50 % aligne (pas de filtrage en peigne)");
            check (os == 0 || (e0 < eMinus - 10.0 && e0 < ePlus - 10.0),
                   juce::String (names[os]) + " : la latence reportee est la bonne");
        }
    }

    void testBypassAndDryAreExact()
    {
        std::printf ("\n== Bypass et Mix 0 %% : dry exact retarde ==\n");
        const auto input = makeNoise (48000, 0.5f);

        {
            auto p = makeGain();
            auto& s = p->getValueTreeState();
            setParam (s, brasier::gain::ids::gain, 12.0f);
            setParam (s, brasier::gain::ids::clip, 1.0f);
            setParam (s, brasier::core::ids::oversampling, 3.0f);
            setParam (s, brasier::core::ids::bypass, 1.0f);
            reprepare (*p);

            const auto out = run (*p, input);
            check (maxAbsDiffDelayed (input, out, p->getLatencySamples(), 0) <= 0.0,
                   "bypass : sortie = entree retardee, au bit pres");
        }
        {
            auto p = makeGain();
            auto& s = p->getValueTreeState();
            setParam (s, brasier::gain::ids::gain, 12.0f);
            setParam (s, brasier::core::ids::mix, 0.0f);
            reprepare (*p);

            const auto out = run (*p, input);
            check (maxAbsDiffDelayed (input, out, p->getLatencySamples(), 0) <= 0.0,
                   "mix 0 % : sortie = entree retardee, au bit pres");
        }
        {
            // Bypass active pendant la lecture : transition douce puis dry exact
            auto p = makeGain();
            auto& s = p->getValueTreeState();
            setParam (s, brasier::gain::ids::gain, 12.0f);
            juce::AudioBuffer<float> first (input);
            juce::MidiBuffer midi;
            p->processBlock (first, midi);   // un gros bloc (decoupe en interne)
            setParam (s, brasier::core::ids::bypass, 1.0f);
            const auto out = run (*p, input);
            check (maxAbsDiffDelayed (input, out, p->getLatencySamples(), 4800) < 1.0e-6,
                   "bypass en cours de lecture : dry exact apres la rampe");
        }
    }

    void testHostBlockSizes()
    {
        std::printf ("\n== Blocs de taille variable ==\n");
        const auto input = makeNoise (40000, 0.3f, 99);

        auto a = makeGain (512);
        auto b = makeGain (512);
        for (auto* p : { a.get(), b.get() })
        {
            setParam (p->getValueTreeState(), brasier::gain::ids::gain, 6.0f);
            setParam (p->getValueTreeState(), brasier::gain::ids::clip, 1.0f);
            setParam (p->getValueTreeState(), brasier::core::ids::mix, 70.0f);
        }

        const auto outA = run (*a, input, 512);
        const auto outB = run (*b, input, 4096);   // plus grand que le bloc prepare
        check (maxAbsDiffDelayed (outA, outB, 0, 0) <= 0.0,
               "blocs de 4096 (> 512 prepare) : resultat identique, sans debordement");

        auto c = makeGain (512);
        juce::AudioBuffer<float> out (input);
        juce::MidiBuffer midi;
        std::mt19937 rng (7);
        int pos = 0;
        while (pos < out.getNumSamples())
        {
            const int n = juce::jmin (static_cast<int> (rng() % 700) + 1, out.getNumSamples() - pos);
            juce::AudioBuffer<float> view (out.getArrayOfWritePointers(), 2, pos, n);
            c->processBlock (view, midi);
            pos += n;
        }
        check (allFinite (out), "blocs aleatoires de 1 a 700 echantillons : aucun NaN");
    }

    void testAutoGain()
    {
        std::printf ("\n== Auto-gain ==\n");
        const auto input = makeSine (static_cast<int> (sampleRate * 4.0), 0.25f);

        auto p = makeGain();
        auto& s = p->getValueTreeState();
        setParam (s, brasier::gain::ids::gain, 12.0f);
        setParam (s, brasier::core::ids::autoGain, 1.0f);

        const auto out = run (*p, input);
        const double inDb  = rmsDb (input, static_cast<int> (sampleRate * 2.0));
        const double outDb = rmsDb (out,   static_cast<int> (sampleRate * 2.0));
        std::printf ("  entree %.2f dB RMS, sortie %.2f dB RMS (gain +12 dB, auto-gain on)\n", inDb, outDb);
        check (std::abs (outDb - inDb) < 0.5, "auto-gain : niveau de sortie = niveau d'entree (+/-0.5 dB)");

        auto q = makeGain();
        setParam (q->getValueTreeState(), brasier::gain::ids::gain, 12.0f);
        const auto out2 = run (*q, input);
        const double out2Db = rmsDb (out2, static_cast<int> (sampleRate * 2.0));
        check (std::abs (out2Db - inDb - 12.0) < 0.2, "auto-gain off : +12 dB effectifs");
    }

    void testStress()
    {
        std::printf ("\n== Robustesse ==\n");
        auto input = makeNoise (48000, 1.0f, 5);
        input.setSample (0, 1000, 50.0f);   // pic absurde

        auto p = makeGain();
        auto& s = p->getValueTreeState();
        setParam (s, brasier::gain::ids::gain, 24.0f);
        setParam (s, brasier::gain::ids::clip, 1.0f);
        setParam (s, brasier::core::ids::oversampling, 3.0f);
        reprepare (*p);

        const auto out = run (*p, input);
        check (allFinite (out), "gain +24 dB, clip, 8x, bruit pleine echelle : aucun NaN/Inf");
        check (out.getMagnitude (0, out.getNumSamples()) < 2.0f, "le soft-clip borne bien la sortie");

        juce::AudioBuffer<float> silence (2, 48000);
        silence.clear();
        const auto outSilence = run (*p, silence);
        check (outSilence.getMagnitude (24000, 24000) < 1.0e-6f, "le silence reste du silence");
    }

    void testOversamplingSwitchAtRuntime()
    {
        std::printf ("\n== Changement d'oversampling en cours de lecture ==\n");
        auto p = makeGain();
        auto& s = p->getValueTreeState();
        setParam (s, brasier::core::ids::mix, 50.0f);
        const auto input = makeSine (48000);

        run (*p, input);
        const int before = p->getLatencySamples();

        setParam (s, brasier::core::ids::oversampling, 3.0f);   // 2x -> 8x sans re-prepare
        const auto out = run (*p, input);

        // La latence est reportee a l'hote par le thread message (Timer)
        juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
        const int after = p->getLatencySamples();

        std::printf ("  latence 2x = %d, 8x = %d\n", before, after);
        check (after > before, "la nouvelle latence est reportee a l'hote");
        check (alignmentErrorDb (input, out, after, 4096) < -60.0, "le dry suit la nouvelle latence");
    }

    void testStateRoundTrip()
    {
        std::printf ("\n== Sauvegarde / rechargement ==\n");
        auto p = makeGain();
        auto& s = p->getValueTreeState();
        setParam (s, brasier::gain::ids::gain, 7.5f);
        setParam (s, brasier::gain::ids::clip, 1.0f);
        setParam (s, brasier::core::ids::mix, 42.0f);
        setParam (s, brasier::core::ids::oversampling, 2.0f);

        juce::MemoryBlock data;
        p->getStateInformation (data);

        auto q = makeGain();
        q->setStateInformation (data.getData(), static_cast<int> (data.getSize()));
        auto& t = q->getValueTreeState();

        check (std::abs (getParam (t, brasier::gain::ids::gain) - 7.5f) < 0.01f, "gain restaure");
        check (getParam (t, brasier::gain::ids::clip) > 0.5f, "soft clip restaure");
        check (std::abs (getParam (t, brasier::core::ids::mix) - 42.0f) < 0.05f, "mix restaure");
        check (static_cast<int> (getParam (t, brasier::core::ids::oversampling)) == 2, "oversampling restaure");
    }
}

/** Rend l'interface d'un plugin dans un PNG (verification visuelle sans hote). */
static int renderEditorSnapshot (const juce::File& file, int width)
{
    auto p = makeGain();
    std::unique_ptr<juce::AudioProcessorEditor> editor (p->createEditor());
    editor->setSize (width, width * 9 / 16);

    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
    file.deleteFile();
    juce::FileOutputStream stream (file);
    juce::PNGImageFormat png;
    const bool ok = stream.openedOk() && png.writeImageToStream (image, stream);
    std::printf ("capture %s : %s\n", file.getFullPathName().toRawUTF8(), ok ? "OK" : "ECHEC");
    return ok ? 0 : 1;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    // BrasierTests --snapshot capture.png [largeur]
    if (argc >= 3 && juce::String (argv[1]) == "--snapshot")
        return renderEditorSnapshot (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]),
                                     argc >= 4 ? juce::String (argv[3]).getIntValue() : 640);

    testLatencyAlignment();
    testBypassAndDryAreExact();
    testHostBlockSizes();
    testAutoGain();
    testStress();
    testOversamplingSwitchAtRuntime();
    testStateRoundTrip();

    std::printf ("\n%s : %d echec(s)\n", failures == 0 ? "OK" : "ECHEC", failures);
    return failures;
}
