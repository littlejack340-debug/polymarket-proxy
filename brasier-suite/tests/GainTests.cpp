// Tests de Brasier Gain : latence, alignement dry/wet, auto-gain, oversampling.

#include "TestHelpers.h"
#include "Gain/Source/GainProcessor.h"

using namespace brasier::tests;

namespace
{
    std::unique_ptr<brasier::gain::GainProcessor> makeGain (int preparedBlock = blockSize)
    {
        return makeProcessor<brasier::gain::GainProcessor> (preparedBlock);
    }

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

    void testGainCommon() { runCommonPluginTests<brasier::gain::GainProcessor> ("Brasier Gain"); }
}

BRASIER_TEST ("gain.common", testGainCommon);
BRASIER_TEST ("gain.latency", testLatencyAlignment);
BRASIER_TEST ("gain.autogain", testAutoGain);
BRASIER_TEST ("gain.stress", testStress);
BRASIER_TEST ("gain.oversampling-switch", testOversamplingSwitchAtRuntime);
