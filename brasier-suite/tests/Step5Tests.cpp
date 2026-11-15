// Tests de l'etape 5 : Brasier Tamis (filtre), Remous (modulation), Ressort (reverb).

#include "TestHelpers.h"
#include "Remous/Source/RemousProcessor.h"
#include "Ressort/Source/RessortProcessor.h"
#include "Tamis/Source/TamisProcessor.h"

using namespace brasier::tests;
using brasier::remous::RemousProcessor;
using brasier::ressort::RessortProcessor;
using brasier::tamis::TamisProcessor;
namespace tid = brasier::tamis::ids;
namespace mid = brasier::remous::ids;
namespace rid = brasier::ressort::ids;

namespace
{
    double gainAtDb (juce::AudioProcessor& p, double hz)
    {
        p.reset();
        const auto in = makeSine (static_cast<int> (sampleRate * 0.5), 0.05f, hz, hz);
        const auto out = run (p, in);
        return rmsDb (out, 12000) - rmsDb (in, 12000);
    }

    std::unique_ptr<TamisProcessor> makeFilter (int type, int slope, float cutoff, float resonance)
    {
        auto p = makeProcessor<TamisProcessor>();
        auto& s = p->getValueTreeState();
        setParam (s, tid::type, static_cast<float> (type));
        setParam (s, tid::slope, static_cast<float> (slope));
        setParam (s, tid::cutoff, cutoff);
        setParam (s, tid::resonance, resonance);
        reprepare (*p);
        return p;
    }

    //==============================================================================
    void testFilterResponses()
    {
        std::printf ("\n== Tamis : reponses des filtres ==\n");

        auto lp = makeFilter (0, 0, 1000.0f, 0.0f);
        const double lpPass = gainAtDb (*lp, 200.0), lpStop = gainAtDb (*lp, 8000.0);
        auto lp24 = makeFilter (0, 1, 1000.0f, 0.0f);
        const double lp24Stop = gainAtDb (*lp24, 8000.0);
        std::printf ("  LP 1 kHz : 200 Hz %+.1f dB, 8 kHz %+.1f dB (12 dB) / %+.1f dB (24 dB)\n", lpPass, lpStop, lp24Stop);
        check (std::abs (lpPass) < 1.5 && lpStop < -25.0, "passe-bas 12 dB/oct");
        check (lp24Stop < lpStop - 20.0, "passe-bas 24 dB/oct deux fois plus raide");

        auto hp = makeFilter (1, 0, 1000.0f, 0.0f);
        const double hpStop = gainAtDb (*hp, 100.0), hpPass = gainAtDb (*hp, 8000.0);
        std::printf ("  HP 1 kHz : 100 Hz %+.1f dB, 8 kHz %+.1f dB\n", hpStop, hpPass);
        check (hpStop < -25.0 && std::abs (hpPass) < 1.5, "passe-haut");

        auto bp = makeFilter (2, 0, 1000.0f, 30.0f);
        const double bpCentre = gainAtDb (*bp, 1000.0), bpFar = gainAtDb (*bp, 8000.0);
        std::printf ("  BP 1 kHz : centre %+.1f dB, 8 kHz %+.1f dB\n", bpCentre, bpFar);
        check (std::abs (bpCentre) < 2.0 && bpFar < bpCentre - 12.0, "passe-bande");

        auto notch = makeFilter (3, 0, 1000.0f, 0.0f);
        const double notchCentre = gainAtDb (*notch, 1000.0), notchFar = gainAtDb (*notch, 150.0);
        std::printf ("  Notch 1 kHz : centre %+.1f dB, 150 Hz %+.1f dB\n", notchCentre, notchFar);
        check (notchCentre < -20.0 && std::abs (notchFar) < 1.5, "coupe-bande");
    }

    void testResonanceSaturates()
    {
        std::printf ("\n== Tamis : resonance saturee et auto-oscillation ==\n");
        auto p = makeFilter (0, 0, 1000.0f, 100.0f);
        auto& s = p->getValueTreeState();
        setParam (s, tid::drive, 24.0f);
        const auto loud = run (*p, makeNoise (48000, 1.0f, 31));
        const float peak = loud.getMagnitude (0, loud.getNumSamples());
        std::printf ("  resonance 100 %%, drive +24 dB, bruit pleine echelle : pic %.2f\n", static_cast<double> (peak));
        check (allFinite (loud) && peak < 6.0f, "la resonance sature au lieu d'exploser");

        auto q = makeFilter (0, 0, 1000.0f, 100.0f);
        const auto ring = run (*q, makeImpulse (static_cast<int> (sampleRate * 2.0), 100, 0.5f));
        const double late = rmsDb (ring, static_cast<int> (sampleRate * 1.5));
        const double hz = estimatePitchHz (ring, 0, static_cast<int> (sampleRate * 1.5), 4096, 200.0, 4000.0);
        std::printf ("  apres une impulsion : %.1f dBFS a 1,5 s, oscillation a %.0f Hz\n", late, hz);
        check (late > -30.0 && std::abs (hz / 1000.0 - 1.0) < 0.1, "resonance a fond : auto-oscillation stable vers la coupure");
    }

    void testFilterModulation()
    {
        std::printf ("\n== Tamis : LFO et enveloppe ==\n");
        auto p = makeFilter (0, 0, 800.0f, 30.0f);
        auto& s = p->getValueTreeState();
        setParam (s, tid::lfoAmount, 100.0f);
        setParam (s, tid::lfoSync, 1.0f);
        setParam (s, tid::lfoDivision, 10.0f);   // 1/4 a 120 BPM = 2 Hz
        const auto out = run (*p, makeNoise (static_cast<int> (sampleRate * 2.0), 0.3f, 32));
        double lo = 1.0e9, hi = -1.0e9;
        for (int start = 4800; start + 2400 < out.getNumSamples(); start += 2400)
        {
            const double r = rmsDb (out, start, start + 2400);
            lo = std::min (lo, r);
            hi = std::max (hi, r);
        }
        std::printf ("  LFO 100 %% : niveau entre %.1f et %.1f dB\n", lo, hi);
        check (hi - lo > 10.0, "le LFO balaie la coupure");

        auto burst = makeNoise (static_cast<int> (sampleRate * 2.0), 0.3f, 33);
        for (int i = 0; i < burst.getNumSamples(); ++i)
            if ((i / 12000) % 2 == 1)
                for (int ch = 0; ch < 2; ++ch)
                    burst.setSample (ch, i, burst.getSample (ch, i) * 0.01f);

        auto withEnv = makeFilter (0, 0, 200.0f, 20.0f);
        setParam (withEnv->getValueTreeState(), tid::envAmount, 100.0f);
        auto noEnv = makeFilter (0, 0, 200.0f, 20.0f);
        const double a = rmsDb (run (*withEnv, burst), 0, 12000);
        const double b = rmsDb (run (*noEnv, burst), 0, 12000);
        std::printf ("  pendant une salve : %+.1f dB avec enveloppe vs sans\n", a - b);
        check (a > b + 6.0, "l'envelope follower ouvre le filtre quand le son tape");
    }

    //==============================================================================
    std::unique_ptr<RemousProcessor> makeModulation (int mode, float depth, float rate = 1.0f)
    {
        auto p = makeProcessor<RemousProcessor>();
        auto& s = p->getValueTreeState();
        setParam (s, mid::mode, static_cast<float> (mode));
        setParam (s, mid::depth, depth);
        setParam (s, mid::rate, rate);
        reprepare (*p);
        return p;
    }

    void testModulationModes()
    {
        std::printf ("\n== Remous : chorus, flanger, phaser, tremolo ==\n");

        // Chorus : la hauteur ondule
        auto chorus = makeModulation (0, 100.0f, 1.0f);
        setParam (chorus->getValueTreeState(), brasier::core::ids::mix, 100.0f);
        const auto tone = makeSine (static_cast<int> (sampleRate * 3.0), 0.3f, 440.0, 440.0);
        const auto cOut = run (*chorus, tone);
        double lo = 1.0e9, hi = 0.0;
        for (int start = 24000; start + 4096 < cOut.getNumSamples(); start += 2400)
        {
            const double f = estimatePitchHz (cOut, 0, start, 2048, 200.0, 1000.0);
            lo = std::min (lo, f);
            hi = std::max (hi, f);
        }
        std::printf ("  chorus : variation de hauteur %.1f cents\n", 1200.0 * std::log2 (hi / lo));
        check (1200.0 * std::log2 (hi / lo) > 3.0, "chorus : desaccord module audible");

        // Flanger statique : peigne = deuxieme pic a ~2,75 ms
        auto flanger = makeModulation (1, 0.0f);
        setParam (flanger->getValueTreeState(), mid::feedback, 0.0f);
        setParam (flanger->getValueTreeState(), mid::character, 0.0f);
        const auto fOut = run (*flanger, makeImpulse (4800, 100, 1.0f));
        const int lat = flanger->getLatencySamples();
        int peak = 0;
        float best = 0.0f;
        for (int i = 100 + lat + 20; i < 4800; ++i)
            if (std::abs (fOut.getSample (0, i)) > best) { best = std::abs (fOut.getSample (0, i)); peak = i; }
        const double ms = (peak - 100 - lat) * 1000.0 / sampleRate;
        std::printf ("  flanger (profondeur 0) : repetition a %.2f ms\n", ms);
        check (std::abs (ms - 2.75) < 0.1, "flanger : retard court au centre de sa course");

        // Phaser statique : creux marques dans le spectre
        auto phaser = makeModulation (2, 0.0f);
        setParam (phaser->getValueTreeState(), mid::feedback, 0.0f);
        const auto noise = makeNoise (65536, 0.3f, 34);
        const auto pOut = run (*phaser, noise);
        const auto pin = powerSpectrum (noise, 0, 32768, 14);
        const auto pout = powerSpectrum (pOut, 0, 32768, 14);
        double minRatio = 0.0;
        for (size_t k = 40; k < pin.size() / 2; k += 8)
        {
            double a = 0.0, b = 0.0;
            for (size_t m = k; m < k + 8; ++m) { a += pin[m]; b += pout[m]; }
            minRatio = std::min (minRatio, 10.0 * std::log10 (b / a));
        }
        std::printf ("  phaser : creux le plus profond %.1f dB\n", minRatio);
        check (minRatio < -15.0, "phaser : creux de phase dans le spectre");

        // Tremolo : amplitude modulee, et auto-pan avec spread 100 %
        auto trem = makeModulation (3, 100.0f, 4.0f);
        setParam (trem->getValueTreeState(), mid::spread, 100.0f);
        const auto tOut = run (*trem, makeSine (static_cast<int> (sampleRate * 2.0), 0.3f, 300.0, 300.0));
        double tlo = 1.0e9, thi = -1.0e9, corr = 0.0, el = 0.0, er = 0.0;
        for (int start = 9600; start + 480 < tOut.getNumSamples(); start += 480)
        {
            const double l = rmsDb (tOut, start, start + 480, 0);
            const double r = rmsDb (tOut, start, start + 480, 1);
            tlo = std::min (tlo, l);
            thi = std::max (thi, l);
            const double gl = std::pow (10.0, l / 20.0), gr = std::pow (10.0, r / 20.0);
            corr += (gl - 0.15) * (gr - 0.15);
            el += (gl - 0.15) * (gl - 0.15);
            er += (gr - 0.15) * (gr - 0.15);
        }
        std::printf ("  tremolo : amplitude de %.1f a %.1f dB, correlation G/D %.2f\n", tlo, thi, corr / std::sqrt (el * er));
        check (thi - tlo > 15.0, "tremolo 100 % : modulation d'amplitude profonde");
        check (corr < 0.0, "tremolo spread 100 % : gauche et droite en opposition (auto-pan)");
    }

    //==============================================================================
    double measureRt60 (const juce::AudioBuffer<float>& ir, int start)
    {
        // Integration de Schroeder, pente entre -5 et -25 dB extrapolee a -60 dB
        std::vector<double> energy (static_cast<size_t> (ir.getNumSamples() - start), 0.0);
        double acc = 0.0;
        for (int i = ir.getNumSamples() - 1; i >= start; --i)
        {
            const double v = 0.5 * (ir.getSample (0, i) * ir.getSample (0, i) + ir.getSample (1, i) * ir.getSample (1, i));
            acc += v;
            energy[static_cast<size_t> (i - start)] = acc;
        }
        const double total = energy[0];
        int t5 = -1, t25 = -1;
        for (size_t i = 0; i < energy.size(); ++i)
        {
            const double db = 10.0 * std::log10 (energy[i] / total + 1.0e-30);
            if (t5 < 0 && db < -5.0)  t5 = static_cast<int> (i);
            if (t25 < 0 && db < -25.0) { t25 = static_cast<int> (i); break; }
        }
        if (t5 < 0 || t25 < 0)
            return 0.0;
        return 3.0 * (t25 - t5) / sampleRate;
    }

    std::unique_ptr<RessortProcessor> makeReverb (int type, float decay)
    {
        auto p = makeProcessor<RessortProcessor>();
        auto& s = p->getValueTreeState();
        setParam (s, rid::type, static_cast<float> (type));
        setParam (s, rid::decay, decay);
        setParam (s, rid::predelay, 0.0f);
        setParam (s, rid::drive, 0.0f);
        setParam (s, brasier::core::ids::mix, 100.0f);
        reprepare (*p);
        return p;
    }

    void testReverbs()
    {
        std::printf ("\n== Ressort : decroissance, pre-delay, largeur ==\n");
        const char* names[] = { "Ressort", "Plaque" };

        for (int type = 0; type < 2; ++type)
        {
            for (float decay : { 1.0f, 3.0f })
            {
                auto p = makeReverb (type, decay);
                const auto ir = run (*p, makeImpulse (static_cast<int> (sampleRate * (decay * 2.5f + 1.0f)), 0, 0.5f));
                const double rt = measureRt60 (ir, 0);
                std::printf ("  %s, decay %.1f s : RT60 mesure %.2f s\n", names[type], static_cast<double> (decay), rt);
                check (rt > decay * 0.6 && rt < decay * 1.5, juce::String (names[type]) + " : le temps de decroissance suit le reglage");
            }
        }

        auto p = makeReverb (1, 2.0f);
        setParam (p->getValueTreeState(), rid::predelay, 100.0f);
        reprepare (*p);
        const auto ir = run (*p, makeImpulse (static_cast<int> (sampleRate * 2.0), 0, 0.5f));
        const double before = rmsDb (ir, 0, 4500), after = rmsDb (ir, 5000, 20000);
        std::printf ("  pre-delay 100 ms : avant %.1f dB, apres %.1f dB\n", before, after);
        check (before < after - 40.0, "pre-delay : rien n'arrive avant 100 ms");

        auto mono = makeReverb (1, 2.0f);
        setParam (mono->getValueTreeState(), rid::width, 0.0f);
        reprepare (*mono);
        const auto m = run (*mono, makeNoise (24000, 0.3f, 35));
        double diff = 0.0;
        for (int i = 0; i < m.getNumSamples(); ++i)
            diff = std::max (diff, static_cast<double> (std::abs (m.getSample (0, i) - m.getSample (1, i))));
        check (diff < 1.0e-6, "largeur 0 % : reverb mono");

        // Le ressort disperse : les aigus arrivent apres les graves (chirp)
        auto spring = makeReverb (0, 2.0f);
        setParam (spring->getValueTreeState(), rid::highCut, 20000.0f);
        setParam (spring->getValueTreeState(), rid::lowCut, 20.0f);
        setParam (spring->getValueTreeState(), rid::damping, 0.0f);
        const auto sIr = run (*spring, makeImpulse (12000, 0, 0.5f));
        auto centroidAt = [&sIr] (int start, int length)
        {
            juce::AudioBuffer<float> seg (1, 1 << 10);
            seg.clear();
            for (int i = 0; i < length && i < (1 << 10); ++i)
                seg.setSample (0, i, sIr.getSample (0, start + i));
            const auto pw = powerSpectrum (seg, 0, 0, 10);
            double num = 0.0, den = 0.0;
            for (size_t k = 1; k < pw.size(); ++k) { num += static_cast<double> (k) * pw[k]; den += pw[k]; }
            return num / (den + 1.0e-30) * sampleRate / 1024.0;
        };
        const int firstArrival = static_cast<int> (0.0413 * sampleRate);
        const double early = centroidAt (firstArrival, 240);
        const double late = centroidAt (firstArrival + 240, 240);
        std::printf ("  ressort : centroide du 1er chirp %.0f Hz puis %.0f Hz\n", early, late);
        check (late > early * 1.2, "ressort : dispersion (chirp montant)");
    }

    void testTamisCommon()   { runCommonPluginTests<TamisProcessor> ("Brasier Tamis"); }
    void testRemousCommon()  { runCommonPluginTests<RemousProcessor> ("Brasier Remous"); }
    void testRessortCommon() { runCommonPluginTests<RessortProcessor> ("Brasier Ressort", 8.0); }
}

BRASIER_TEST ("tamis.common", testTamisCommon);
BRASIER_TEST ("tamis.responses", testFilterResponses);
BRASIER_TEST ("tamis.resonance", testResonanceSaturates);
BRASIER_TEST ("tamis.modulation", testFilterModulation);
BRASIER_TEST ("remous.common", testRemousCommon);
BRASIER_TEST ("remous.modes", testModulationModes);
BRASIER_TEST ("ressort.common", testRessortCommon);
BRASIER_TEST ("ressort.reverbs", testReverbs);
