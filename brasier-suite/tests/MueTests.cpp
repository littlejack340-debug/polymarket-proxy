// Tests de Brasier Mue : justesse du pitch, formants, robot, transparence, latence.

#include "TestHelpers.h"
#include "Mue/Source/MueProcessor.h"

using namespace brasier::tests;
namespace mid = brasier::mue::ids;
using brasier::mue::MueProcessor;

namespace
{
    /** Signal "voix" : serie harmonique avec une bosse de formant vers formantHz.
        f0 peut glisser lineairement de f0Start a f0End. */
    juce::AudioBuffer<float> makeVoice (int numSamples, double f0Start, double f0End = -1.0,
                                        double formantHz = 900.0, float amplitude = 0.3f)
    {
        if (f0End < 0.0)
            f0End = f0Start;

        juce::AudioBuffer<float> b (2, numSamples);
        b.clear();
        double phase = 0.0;
        std::vector<double> amps;

        for (int i = 0; i < numSamples; ++i)
        {
            const double f0 = f0Start + (f0End - f0Start) * i / numSamples;
            phase += f0 / sampleRate;
            phase -= std::floor (phase);

            double v = 0.0;
            for (int k = 1; k * f0 < 5000.0; ++k)
            {
                const double f = k * f0;
                const double env = (1.0 / k) * (1.0 + 4.0 * std::exp (-std::pow ((f - formantHz) / 300.0, 2.0)));
                v += env * std::sin (juce::MathConstants<double>::twoPi * k * phase);
            }
            b.setSample (0, i, static_cast<float> (amplitude * 0.25 * v));
        }

        b.copyFrom (1, 0, b, 0, 0, numSamples);
        return b;
    }

    double spectralCentroid (const juce::AudioBuffer<float>& b, int start)
    {
        const auto power = powerSpectrum (b, 0, start, 14);
        const double binHz = sampleRate / (1 << 14);
        double num = 0.0, den = 0.0;
        for (size_t k = 1; k < power.size(); ++k)
        {
            num += k * binHz * power[k];
            den += power[k];
        }
        return num / (den + 1.0e-30);
    }

    std::unique_ptr<MueProcessor> makeMue (float pitch, float formant = 0.0f, int latencyMode = 0)
    {
        auto p = makeProcessor<MueProcessor>();
        auto& s = p->getValueTreeState();
        setParam (s, mid::pitch, pitch);
        setParam (s, mid::formant, formant);
        setParam (s, mid::latency, static_cast<float> (latencyMode));
        reprepare (*p);
        return p;
    }

    void testIdentityAndLatency()
    {
        std::printf ("\n== Mue : transparence et latence ==\n");
        const auto voice = makeVoice (static_cast<int> (sampleRate * 2.0), 180.0);
        const char* names[] = { "Live", "Qualite" };

        for (int mode = 0; mode < 2; ++mode)
        {
            auto p = makeMue (0.0f, 0.0f, mode);
            const int latency = p->getLatencySamples();
            const auto out = run (*p, voice);
            const double err = alignmentErrorDb (voice, out, latency, 24000);
            std::printf ("  %s : latence %d ech. (%.1f ms), ecart au signal retarde %.1f dB\n",
                         names[mode], latency, latency * 1000.0 / sampleRate, err);
            check (err < -40.0, juce::String (names[mode]) + " : pitch 0 / formant 0 = signal intact, retarde de la latence reportee");
        }

        auto live = makeMue (0.0f, 0.0f, 0);
        auto quality = makeMue (0.0f, 0.0f, 1);
        check (live->getLatencySamples() * 1000.0 / sampleRate < 21.0, "mode Live : latence <= 21 ms");
        check (quality->getLatencySamples() > live->getLatencySamples(), "mode Qualite : latence plus longue (voix plus graves)");

        // Changement de mode en cours de lecture : la latence suit
        auto p = makeMue (0.0f, 0.0f, 0);
        run (*p, voice);
        setParam (p->getValueTreeState(), mid::latency, 1.0f);
        run (*p, voice);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (300);
        check (p->getLatencySamples() == quality->getLatencySamples(), "le changement de mode met a jour la latence reportee");
    }

    void testPitchAccuracy()
    {
        std::printf ("\n== Mue : justesse du pitch ==\n");
        constexpr double f0 = 196.0;   // sol 3
        const auto voice = makeVoice (static_cast<int> (sampleRate * 2.0), f0);

        for (float st : { -12.0f, -7.0f, -3.0f, 3.0f, 7.0f, 12.0f })
        {
            auto p = makeMue (st);
            const auto out = run (*p, voice);
            const double expected = f0 * std::pow (2.0, st / 12.0);
            const double measured = estimatePitchHz (out, 0, 48000, 4096, 60.0, 600.0);
            const double cents = 1200.0 * std::log2 (measured / expected);
            std::printf ("  %+5.1f st : %.2f Hz mesure, %.2f Hz attendu (%+.1f cents)\n", st, measured, expected, cents);
            check (std::abs (cents) < 15.0, juce::String (st, 0) + " demi-tons : juste a 15 cents pres");
        }

        // Le niveau reste comparable
        auto up = makeMue (12.0f);
        const auto out = run (*up, voice);
        const double diff = rmsDb (out, 48000) - rmsDb (voice, 48000);
        std::printf ("  niveau a +12 st : %+.1f dB\n", diff);
        check (std::abs (diff) < 4.0, "pitch +12 : niveau stable a +/-4 dB");
    }

    void testFormants()
    {
        std::printf ("\n== Mue : formants independants du pitch ==\n");
        constexpr double f0 = 150.0;
        const auto voice = makeVoice (static_cast<int> (sampleRate * 2.0), f0, -1.0, 900.0);
        const double baseCentroid = spectralCentroid (voice, 48000);

        for (float st : { -6.0f, 6.0f })
        {
            auto p = makeMue (0.0f, st);
            const auto out = run (*p, voice);
            const double pitch = estimatePitchHz (out, 0, 48000, 4096, 60.0, 600.0);
            const double centroid = spectralCentroid (out, 48000);
            std::printf ("  formant %+.0f st : hauteur %.2f Hz, centroide %.0f Hz (origine %.0f Hz)\n",
                         st, pitch, centroid, baseCentroid);
            check (std::abs (1200.0 * std::log2 (pitch / f0)) < 15.0, "formant " + juce::String (st, 0) + " : la note ne bouge pas");
            check (st > 0 ? centroid > baseCentroid * 1.2 : centroid < baseCentroid * 0.85,
                   "formant " + juce::String (st, 0) + " : l'enveloppe spectrale se deplace");
        }

        auto p = makeMue (7.0f, 0.0f);
        const auto out = run (*p, voice);
        const double centroid = spectralCentroid (out, 48000);
        std::printf ("  pitch +7 st sans formant : centroide %.0f Hz (origine %.0f Hz)\n", centroid, baseCentroid);
        check (centroid > baseCentroid * 0.8 && centroid < baseCentroid * 1.3,
               "pitch +7 : les formants restent a peu pres en place (pas d'effet chipmunk)");
    }

    void testRobot()
    {
        std::printf ("\n== Mue : mode robot ==\n");
        const auto glide = makeVoice (static_cast<int> (sampleRate * 3.0), 150.0, 260.0);

        auto p = makeProcessor<MueProcessor>();
        auto& s = p->getValueTreeState();
        setParam (s, mid::robot, 1.0f);
        setParam (s, mid::robotNote, 12.0f);   // C3 = 130.81 Hz
        const auto out = run (*p, glide);

        double worst = 0.0;
        for (int start = 24000; start + 8192 < out.getNumSamples(); start += 12000)
        {
            const double f = estimatePitchHz (out, 0, start, 4096, 60.0, 600.0);
            worst = std::max (worst, std::abs (1200.0 * std::log2 (f / 130.81)));
        }
        std::printf ("  entree 150 -> 260 Hz, sortie : ecart max %.1f cents autour de C3\n", worst);
        check (worst < 25.0, "robot : la sortie reste sur la note choisie pendant que l'entree glisse");
    }

    void testMueCommon() { runCommonPluginTests<MueProcessor> ("Brasier Mue"); }
}

BRASIER_TEST ("mue.common", testMueCommon);
BRASIER_TEST ("mue.identity", testIdentityAndLatency);
BRASIER_TEST ("mue.pitch", testPitchAccuracy);
BRASIER_TEST ("mue.formant", testFormants);
BRASIER_TEST ("mue.robot", testRobot);
