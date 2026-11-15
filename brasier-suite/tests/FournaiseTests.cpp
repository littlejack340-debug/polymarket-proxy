// Tests de Brasier Fournaise : aliasing, transparence, filtres, modeles, punish.

#include "TestHelpers.h"
#include "Fournaise/Source/FournaiseProcessor.h"

using namespace brasier::tests;
namespace fid = brasier::fournaise::ids;
using brasier::fournaise::FournaiseProcessor;

namespace
{
    /** Part d'energie (dB) qui ne tombe pas sur un harmonique de f0 : c'est l'aliasing. */
    double aliasRatioDb (const juce::AudioBuffer<float>& out, double f0)
    {
        constexpr int order = 15;   // 32768 points
        const auto power = powerSpectrum (out, 0, out.getNumSamples() - (1 << order), order);
        const double binHz = sampleRate / (1 << order);

        double total = 0.0, harmonic = 0.0;
        for (size_t k = 2; k < power.size(); ++k)
        {
            total += power[k];
            const double f = static_cast<double> (k) * binHz;
            const double n = std::round (f / f0);
            if (n >= 1.0 && std::abs (f - n * f0) <= 4.0 * binHz)
                harmonic += power[k];
        }
        return 10.0 * std::log10 ((total - harmonic + 1.0e-30) / (total + 1.0e-30));
    }

    void testAliasing()
    {
        std::printf ("\n== Fournaise : aliasing selon l'oversampling ==\n");
        // Frequence "coherente" (nombre entier de periodes dans la fenetre FFT, bin premier) :
        // aucune fuite spectrale, la mesure ne voit que le vrai repliement.
        constexpr double f0 = 3119.0 * sampleRate / 32768.0;   // ~4569 Hz
        const auto input = makeSine (static_cast<int> (sampleRate), 0.5f, f0, f0);
        double ratios[4] = {};

        for (int os = 0; os < 4; ++os)
        {
            auto p = makeProcessor<FournaiseProcessor>();
            auto& s = p->getValueTreeState();
            setParam (s, fid::model, 1.0f);    // transistor : le plus riche en harmoniques
            setParam (s, fid::drive, 24.0f);
            setParam (s, brasier::core::ids::oversampling, static_cast<float> (os));
            reprepare (*p);
            ratios[os] = aliasRatioDb (run (*p, input), f0);
            std::printf ("  %dx : energie d'aliasing %.1f dB sous le signal\n", 1 << os, -ratios[os]);
        }

        check (ratios[3] < ratios[0] - 25.0, "8x reduit l'aliasing d'au moins 25 dB par rapport a 1x");
        check (ratios[1] < ratios[0] && ratios[2] < ratios[1] && ratios[3] < ratios[2], "plus de facteur = moins d'aliasing");
        check (ratios[3] < -60.0, "8x : aliasing sous -60 dB");
    }

    void testTransparencyAtLowLevel()
    {
        std::printf ("\n== Fournaise : transparence a bas niveau ==\n");
        const char* names[] = { "Tube", "Transistor", "Bande", "Diode" };
        const auto input = makeSine (static_cast<int> (sampleRate), 0.001f, 1000.0, 220.0);   // -60 dBFS

        for (int m = 0; m < 4; ++m)
        {
            auto p = makeProcessor<FournaiseProcessor>();
            auto& s = p->getValueTreeState();
            setParam (s, fid::model, static_cast<float> (m));
            setParam (s, fid::drive, 0.0f);
            reprepare (*p);
            const auto out = run (*p, input);
            const double err = alignmentErrorDb (input, out, p->getLatencySamples(), 8000);
            std::printf ("  %s : ecart a l'entree %.1f dB\n", names[m], err);
            check (err < -30.0, juce::String (names[m]) + " : quasi transparent a -60 dBFS sans drive");
        }
    }

    void testFiltersAndTone()
    {
        std::printf ("\n== Fournaise : filtres pre/post et tone ==\n");
        const auto bass = makeSine (static_cast<int> (sampleRate), 0.01f, 50.0, 50.0);
        const auto treble = makeSine (static_cast<int> (sampleRate), 0.01f, 10000.0, 10000.0);

        auto measure = [] (const juce::AudioBuffer<float>& in, const char* id, float value)
        {
            auto p = makeProcessor<FournaiseProcessor>();
            setParam (p->getValueTreeState(), fid::drive, 0.0f);
            setParam (p->getValueTreeState(), id, value);
            reprepare (*p);
            return rmsDb (run (*p, in), 12000) - rmsDb (in, 12000);
        };

        const double preLow = measure (bass, fid::preLowCut, 500.0f);
        const double postLow = measure (bass, fid::postLowCut, 500.0f);
        const double preHigh = measure (treble, fid::preHighCut, 2000.0f);
        const double postHigh = measure (treble, fid::postHighCut, 2000.0f);
        const double toneUp = measure (treble, fid::tone, 6.0f);
        const double toneDown = measure (bass, fid::tone, 6.0f);
        std::printf ("  pre low cut 500 Hz sur 50 Hz : %.1f dB | post : %.1f dB\n", preLow, postLow);
        std::printf ("  pre high cut 2 kHz sur 10 kHz : %.1f dB | post : %.1f dB\n", preHigh, postHigh);
        std::printf ("  tone +6 : 10 kHz %+.1f dB, 50 Hz %+.1f dB\n", toneUp, toneDown);

        check (preLow < -30.0 && postLow < -30.0, "les low cut attenuent fortement le grave");
        check (preHigh < -20.0 && postHigh < -20.0, "les high cut attenuent fortement l'aigu");
        check (toneUp > 4.0 && toneDown < -4.0, "le tone incline le spectre (aigus +, graves -)");

        const auto flat = measure (makeSine (static_cast<int> (sampleRate), 0.01f, 1000.0, 15000.0), fid::preHighCut, 20000.0f);
        check (std::abs (flat) < 0.5, "high cut en butee = neutre jusqu'a 15 kHz");
    }

    void testDcAndPunish()
    {
        std::printf ("\n== Fournaise : continu, punish, changement de modele ==\n");
        const auto input = makeSine (static_cast<int> (sampleRate * 2.0), 0.5f, 100.0, 100.0);

        auto p = makeProcessor<FournaiseProcessor>();
        setParam (p->getValueTreeState(), fid::model, 3.0f);   // diode, tres asymetrique
        setParam (p->getValueTreeState(), fid::drive, 24.0f);
        const auto out = run (*p, input);
        double mean = 0.0;
        for (int i = 48000; i < 96000; ++i)
            mean += out.getSample (0, i);
        mean /= 48000.0;
        std::printf ("  diode +24 dB : composante continue %.5f\n", mean);
        check (std::abs (mean) < 1.0e-3, "pas de composante continue en sortie");

        auto a = makeProcessor<FournaiseProcessor>();
        auto b = makeProcessor<FournaiseProcessor>();
        setParam (b->getValueTreeState(), fid::punish, 1.0f);
        const auto quiet = makeSine (48000, 0.05f);
        const double gainA = rmsDb (run (*a, quiet), 12000);
        const double gainB = rmsDb (run (*b, quiet), 12000);
        std::printf ("  punish : %+.1f dB\n", gainB - gainA);
        check (gainB > gainA + 3.0, "punish pousse nettement le niveau");

        // Changement de modele en cours de lecture : pas de saut brutal
        auto c = makeProcessor<FournaiseProcessor>();
        setParam (c->getValueTreeState(), fid::drive, 18.0f);
        const auto sine = makeSine (48000, 0.5f, 200.0, 200.0);
        auto first = run (*c, sine);
        float maxStep = 0.0f;
        for (int i = 1; i < first.getNumSamples(); ++i)
            maxStep = std::max (maxStep, std::abs (first.getSample (0, i) - first.getSample (0, i - 1)));

        setParam (c->getValueTreeState(), fid::model, 3.0f);
        auto second = run (*c, sine);
        float switchStep = 0.0f;
        for (int i = 1; i < 4096; ++i)
            switchStep = std::max (switchStep, std::abs (second.getSample (0, i) - second.getSample (0, i - 1)));
        std::printf ("  pas max : regime %.4f, au changement %.4f\n", static_cast<double> (maxStep), static_cast<double> (switchStep));
        check (switchStep < maxStep * 2.0f, "changement de modele sans clic");
    }

    void testFournaiseCommon() { runCommonPluginTests<FournaiseProcessor> ("Brasier Fournaise"); }
}

BRASIER_TEST ("fournaise.common", testFournaiseCommon);
BRASIER_TEST ("fournaise.aliasing", testAliasing);
BRASIER_TEST ("fournaise.transparency", testTransparencyAtLowLevel);
BRASIER_TEST ("fournaise.filters", testFiltersAndTone);
BRASIER_TEST ("fournaise.dc-punish", testDcAndPunish);
