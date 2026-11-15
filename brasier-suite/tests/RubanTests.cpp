// Tests de Brasier Ruban : temps d'echo, sync, feedback, ping-pong, ducking, stabilite.

#include "TestHelpers.h"
#include "Ruban/Source/RubanProcessor.h"

using namespace brasier::tests;
namespace rid = brasier::ruban::ids;
using brasier::ruban::RubanProcessor;

namespace
{
    /** Delay "propre" : sans modulation ni saturation ni filtrage, 100 % wet. */
    std::unique_ptr<RubanProcessor> makeCleanDelay()
    {
        auto p = makeProcessor<RubanProcessor>();
        auto& s = p->getValueTreeState();
        setParam (s, rid::wow, 0.0f);
        setParam (s, rid::flutter, 0.0f);
        setParam (s, rid::saturation, 0.0f);
        setParam (s, rid::lowCut, 20.0f);
        setParam (s, rid::highCut, 20000.0f);
        setParam (s, rid::feedback, 0.0f);
        setParam (s, brasier::core::ids::mix, 100.0f);
        reprepare (*p);
        return p;
    }

    int peakIndex (const juce::AudioBuffer<float>& b, int channel, int from = 0, int to = -1)
    {
        if (to < 0)
            to = b.getNumSamples();
        int best = from;
        float bestValue = 0.0f;
        for (int i = from; i < to; ++i)
            if (std::abs (b.getSample (channel, i)) > bestValue)
            {
                bestValue = std::abs (b.getSample (channel, i));
                best = i;
            }
        return best;
    }

    void testEchoTiming()
    {
        std::printf ("\n== Ruban : temps d'echo ==\n");

        for (int os = 0; os < 4; ++os)
        {
            auto p = makeCleanDelay();
            auto& s = p->getValueTreeState();
            setParam (s, rid::sync, 0.0f);
            setParam (s, rid::time, 250.0f);
            setParam (s, brasier::core::ids::oversampling, static_cast<float> (os));
            reprepare (*p);

            const auto out = run (*p, makeImpulse (24000, 100, 0.5f));
            const int peakL = peakIndex (out, 0);
            const int peakR = peakIndex (out, 1);
            std::printf ("  %dx : echo a %d / %d ech. (attendu %d), latence reportee %d\n",
                         1 << os, peakL - 100, peakR - 100, 12000, p->getLatencySamples());
            check (std::abs (peakL - 100 - 12000) <= 1 && std::abs (peakR - 100 - 12000) <= 1,
                   juce::String (1 << os) + "x : echo a 250 ms pile (oversampling de boucle compense)");
            check (p->getLatencySamples() == 0, juce::String (1 << os) + "x : aucune latence reportee");
        }

        // Synchro tempo : sans hote, tempo par defaut 120 BPM -> noire = 500 ms
        auto p = makeCleanDelay();
        setParam (p->getValueTreeState(), rid::sync, 1.0f);
        setParam (p->getValueTreeState(), rid::division, 7.0f);   // 1/8
        const auto out = run (*p, makeImpulse (24000, 0, 0.5f));
        const int peak = peakIndex (out, 0);
        std::printf ("  sync 1/8 a 120 BPM : echo a %d ech. (attendu 12000)\n", peak);
        check (std::abs (peak - 12000) <= 1, "sync 1/8 a 120 BPM = 250 ms");
    }

    void testFeedbackAndPingPong()
    {
        std::printf ("\n== Ruban : feedback et ping-pong ==\n");
        constexpr int period = 9600;   // 200 ms

        auto burst = makeSine (period * 6, 0.25f, 1000.0, 1000.0);
        burst.clear (2400, burst.getNumSamples() - 2400);   // salve de 50 ms

        auto p = makeCleanDelay();
        auto& s = p->getValueTreeState();
        setParam (s, rid::sync, 0.0f);
        setParam (s, rid::time, 200.0f);
        setParam (s, rid::feedback, 50.0f);
        reprepare (*p);
        const auto out = run (*p, burst);
        const double e1 = rmsDb (out, period, period + 2400, 0);
        const double e2 = rmsDb (out, 2 * period, 2 * period + 2400, 0);
        const double e3 = rmsDb (out, 3 * period, 3 * period + 2400, 0);
        std::printf ("  repetitions : %.1f / %.1f / %.1f dB\n", e1, e2, e3);
        check (std::abs ((e2 - e1) + 6.0) < 1.0 && std::abs ((e3 - e2) + 6.0) < 1.0,
               "feedback 50 % : chaque repetition perd ~6 dB");

        auto q = makeCleanDelay();
        auto& t = q->getValueTreeState();
        setParam (t, rid::sync, 0.0f);
        setParam (t, rid::time, 200.0f);
        setParam (t, rid::feedback, 70.0f);
        setParam (t, rid::mode, 2.0f);   // ping-pong
        reprepare (*q);
        const auto pp = run (*q, burst);
        const double l1 = rmsDb (pp, period, period + 2400, 0), r1 = rmsDb (pp, period, period + 2400, 1);
        const double l2 = rmsDb (pp, 2 * period, 2 * period + 2400, 0), r2 = rmsDb (pp, 2 * period, 2 * period + 2400, 1);
        std::printf ("  ping-pong : 1er echo G %.1f / D %.1f dB, 2e echo G %.1f / D %.1f dB\n", l1, r1, l2, r2);
        check (l1 > r1 + 40.0 && r2 > l2 + 40.0, "ping-pong : les repetitions alternent gauche / droite");
    }

    void testDucking()
    {
        std::printf ("\n== Ruban : ducking ==\n");
        const auto voice = makeSine (static_cast<int> (sampleRate * 3.0), 0.3f, 300.0, 300.0);

        auto measure = [&voice] (float duck)
        {
            auto p = makeCleanDelay();
            auto& s = p->getValueTreeState();
            setParam (s, rid::feedback, 40.0f);
            setParam (s, rid::duck, duck);
            setParam (s, rid::duckThreshold, -40.0f);
            reprepare (*p);
            return rmsDb (run (*p, voice), static_cast<int> (sampleRate * 1.5));
        };

        const double free = measure (0.0f);
        const double ducked = measure (100.0f);
        std::printf ("  wet pendant la voix : %.1f dB sans ducking, %.1f dB avec\n", free, ducked);
        check (ducked < free - 15.0, "ducking 100 % : le delay s'efface sous la voix (> 15 dB)");

        // Apres la voix, le wet revient
        auto p = makeCleanDelay();
        auto& s = p->getValueTreeState();
        setParam (s, rid::feedback, 60.0f);
        setParam (s, rid::duck, 100.0f);
        setParam (s, rid::duckThreshold, -40.0f);
        setParam (s, rid::sync, 0.0f);
        setParam (s, rid::time, 300.0f);
        reprepare (*p);
        auto phrase = makeSine (static_cast<int> (sampleRate * 3.0), 0.3f, 300.0, 300.0);
        phrase.clear (static_cast<int> (sampleRate), phrase.getNumSamples() - static_cast<int> (sampleRate));
        const auto out = run (*p, phrase);
        const double during = rmsDb (out, 24000, 48000);
        const double after = rmsDb (out, 60000, 90000);
        std::printf ("  pendant la phrase %.1f dB, apres %.1f dB\n", during, after);
        check (after > during + 10.0, "les repetitions remontent quand la voix s'arrete");
    }

    void testSelfOscillationIsBounded()
    {
        std::printf ("\n== Ruban : auto-oscillation bornee ==\n");
        auto p = makeProcessor<RubanProcessor>();
        auto& s = p->getValueTreeState();
        setParam (s, rid::feedback, 110.0f);
        setParam (s, rid::saturation, 100.0f);
        setParam (s, rid::lowCut, 20.0f);
        setParam (s, rid::highCut, 20000.0f);
        setParam (s, brasier::core::ids::mix, 100.0f);
        auto input = makeNoise (static_cast<int> (sampleRate * 10.0), 0.8f, 21);
        input.clear (static_cast<int> (sampleRate * 0.5), input.getNumSamples() - static_cast<int> (sampleRate * 0.5));
        const auto out = run (*p, input);
        const float peak = out.getMagnitude (static_cast<int> (sampleRate * 8.0), static_cast<int> (sampleRate * 2.0));
        std::printf ("  pic apres 8 s a 110 %% de feedback : %.2f\n", static_cast<double> (peak));
        check (allFinite (out) && peak < 4.0f, "feedback 110 % : la bande sature mais ne s'emballe pas");
        check (peak > 0.05f, "feedback 110 % : la boucle s'auto-entretient (comme une vraie bande)");
    }

    void testWowModulatesPitch()
    {
        std::printf ("\n== Ruban : wow & flutter ==\n");
        const auto tone = makeSine (static_cast<int> (sampleRate * 4.0), 0.3f, 1000.0, 1000.0);

        auto spread = [&tone] (float wow, float flutter)
        {
            auto p = makeCleanDelay();
            setParam (p->getValueTreeState(), rid::wow, wow);
            setParam (p->getValueTreeState(), rid::flutter, flutter);
            reprepare (*p);
            const auto out = run (*p, tone);
            double lo = 1.0e9, hi = 0.0;
            for (int start = static_cast<int> (sampleRate); start + 4096 < out.getNumSamples(); start += 2400)
            {
                const double f = estimatePitchHz (out, 0, start, 2048, 500.0, 2000.0);
                lo = std::min (lo, f);
                hi = std::max (hi, f);
            }
            return 1200.0 * std::log2 (hi / lo);
        };

        const double still = spread (0.0f, 0.0f);
        const double wobbly = spread (100.0f, 100.0f);
        std::printf ("  ecart de hauteur : %.1f cents sans modulation, %.1f cents a 100 %%\n", still, wobbly);
        check (still < 2.0, "sans wow/flutter la hauteur est stable");
        check (wobbly > 10.0 && wobbly < 120.0, "wow/flutter a 100 % : derive audible mais musicale");
    }

    void testRubanCommon() { runCommonPluginTests<RubanProcessor> ("Brasier Ruban", 8.0); }
}

BRASIER_TEST ("ruban.common", testRubanCommon);
BRASIER_TEST ("ruban.timing", testEchoTiming);
BRASIER_TEST ("ruban.feedback", testFeedbackAndPingPong);
BRASIER_TEST ("ruban.ducking", testDucking);
BRASIER_TEST ("ruban.self-oscillation", testSelfOscillationIsBounded);
BRASIER_TEST ("ruban.wow", testWowModulatesPitch);
