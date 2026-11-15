// Tests de l'etape 7 : presets d'usine et banc d'essai CPU.

#include "TestHelpers.h"
#include "Fournaise/Source/FournaiseProcessor.h"
#include "Gain/Source/GainProcessor.h"
#include "Mue/Source/MueProcessor.h"
#include "Remous/Source/RemousProcessor.h"
#include "Ressort/Source/RessortProcessor.h"
#include "Ruban/Source/RubanProcessor.h"
#include "Tamis/Source/TamisProcessor.h"

using namespace brasier::tests;

namespace
{
    /** Charge chaque preset d'usine et le fait jouer : identifiants valides, pas de NaN, niveaux sains. */
    template <typename Processor>
    void runPresetTests (const juce::String& name)
    {
        auto p = makeProcessor<Processor>();
        auto& presets = p->getPresetManager();
        auto& state = p->getValueTreeState();
        const int count = presets.getNumFactoryPresets();

        bool idsValid = true, allFinite = true, allBounded = true, allAudible = true;
        juce::StringArray problems;

        for (int i = 0; i < count; ++i)
        {
            const auto& preset = presets.getFactoryPreset (i);
            for (const auto& [id, value] : preset.values)
            {
                auto* param = state.getParameter (id);
                if (param == nullptr)
                {
                    idsValid = false;
                    problems.add (preset.name + " : parametre inconnu '" + id + "'");
                }
                else if (! juce::isPositiveAndNotGreaterThan (value - param->getNormalisableRange().start,
                                                               param->getNormalisableRange().end - param->getNormalisableRange().start))
                {
                    idsValid = false;
                    problems.add (preset.name + " : '" + id + "' hors plage (" + juce::String (value) + ")");
                }
            }

            presets.loadFactoryPreset (i);
            p->reset();

            auto input = makeSine (static_cast<int> (sampleRate * 2.0), 0.25f, 220.0, 330.0);
            auto noise = makeNoise (input.getNumSamples(), 0.1f, static_cast<unsigned> (100 + i));
            for (int ch = 0; ch < 2; ++ch)
                input.addFrom (ch, 0, noise, ch, 0, input.getNumSamples());

            const auto out = run (*p, input);
            const float peak = out.getMagnitude (0, out.getNumSamples());
            const double level = rmsDb (out, static_cast<int> (sampleRate));

            if (! brasier::tests::allFinite (out)) { allFinite = false; problems.add (preset.name + " : NaN/Inf"); }
            if (peak > 4.0f)                       { allBounded = false; problems.add (preset.name + " : pic " + juce::String (peak, 2)); }
            if (level < -60.0)                     { allAudible = false; problems.add (preset.name + " : sortie quasi muette"); }
        }

        std::printf ("\n== %s : %d presets d'usine ==\n", name.toRawUTF8(), count);
        for (const auto& problem : problems)
            std::printf ("  ! %s\n", problem.toRawUTF8());

        check (count >= 3, name + " : presets d'usine presents");
        check (idsValid, name + " : identifiants et plages des presets valides");
        check (allFinite && allBounded, name + " : chaque preset produit un signal sain");
        check (allAudible, name + " : aucun preset muet");
        check (p->getNumPrograms() == count, name + " : presets exposes comme programmes a l'hote");
    }

    //==============================================================================
    struct BenchResult { double defaultPercent = 0.0, eightXPercent = 0.0; };

    template <typename Processor>
    double measureCpuPercent (int oversamplingIndex)
    {
        constexpr int block = 256;
        constexpr double seconds = 6.0;
        auto p = makeProcessor<Processor> (block);
        if (oversamplingIndex >= 0)
        {
            setParam (p->getValueTreeState(), brasier::core::ids::oversampling, static_cast<float> (oversamplingIndex));
            reprepare (*p, block);
        }

        auto buffer = makeNoise (block, 0.3f, 77);
        juce::MidiBuffer midi;
        const int blocks = static_cast<int> (seconds * sampleRate / block);

        // Chauffe
        for (int i = 0; i < 50; ++i)
            p->processBlock (buffer, midi);

        const auto start = juce::Time::getHighResolutionTicks();
        for (int i = 0; i < blocks; ++i)
        {
            for (int ch = 0; ch < 2; ++ch)
                for (int s = 0; s < block; ++s)
                    buffer.setSample (ch, s, 0.3f * std::sin (0.01f * static_cast<float> (s + i * block) * (1.0f + 0.5f * static_cast<float> (ch))));
            p->processBlock (buffer, midi);
        }
        const double elapsed = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - start);
        return 100.0 * elapsed / seconds;
    }

    template <typename Processor>
    BenchResult bench (const char* name, bool hasOversampling)
    {
        BenchResult r;
        r.defaultPercent = measureCpuPercent<Processor> (-1);
        r.eightXPercent = hasOversampling ? measureCpuPercent<Processor> (3) : r.defaultPercent;
        std::printf ("  %-18s %6.2f %%      %6.2f %%\n", name, r.defaultPercent, r.eightXPercent);
        return r;
    }

    void testPresets()
    {
        runPresetTests<brasier::gain::GainProcessor> ("Brasier Gain");
        runPresetTests<brasier::fournaise::FournaiseProcessor> ("Brasier Fournaise");
        runPresetTests<brasier::ruban::RubanProcessor> ("Brasier Ruban");
        runPresetTests<brasier::mue::MueProcessor> ("Brasier Mue");
        runPresetTests<brasier::tamis::TamisProcessor> ("Brasier Tamis");
        runPresetTests<brasier::remous::RemousProcessor> ("Brasier Remous");
        runPresetTests<brasier::ressort::RessortProcessor> ("Brasier Ressort");
    }

    void testUserPresetRoundTrip()
    {
        std::printf ("\n== Presets utilisateur ==\n");
        auto p = makeProcessor<brasier::fournaise::FournaiseProcessor>();
        auto& presets = p->getPresetManager();
        auto& s = p->getValueTreeState();
        setParam (s, brasier::fournaise::ids::drive, 17.5f);
        setParam (s, brasier::fournaise::ids::model, 3.0f);

        const juce::String name = "__brasier_test_preset__";
        const bool saved = presets.saveUserPreset (name);
        setParam (s, brasier::fournaise::ids::drive, 0.0f);
        const auto file = presets.getUserDirectory().getChildFile (name + brasier::core::PresetManager::fileExtension);
        const bool loaded = presets.loadUserPreset (file);
        check (saved && loaded, "sauvegarde puis rechargement d'un preset utilisateur");
        check (std::abs (getParam (s, brasier::fournaise::ids::drive) - 17.5f) < 0.01f
                   && static_cast<int> (getParam (s, brasier::fournaise::ids::model)) == 3,
               "le preset utilisateur restaure les reglages");
        file.deleteFile();
    }

    void testCpu()
    {
        std::printf ("\n== Banc d'essai CPU (48 kHz, blocs de 256, stereo, 1 coeur) ==\n");
        std::printf ("  %-18s %-13s %s\n", "plugin", "par defaut", "oversampling 8x");
        const BenchResult results[] = {
            bench<brasier::gain::GainProcessor> ("Gain", true),
            bench<brasier::fournaise::FournaiseProcessor> ("Fournaise", true),
            bench<brasier::ruban::RubanProcessor> ("Ruban", true),
            bench<brasier::mue::MueProcessor> ("Mue", false),
            bench<brasier::tamis::TamisProcessor> ("Tamis", true),
            bench<brasier::remous::RemousProcessor> ("Remous", true),
            bench<brasier::ressort::RessortProcessor> ("Ressort", true),
        };

        bool defaultsLight = true;
        for (const auto& r : results)
            defaultsLight = defaultsLight && r.defaultPercent < 10.0;
        check (defaultsLight, "reglages par defaut : chaque plugin sous 10 % d'un coeur");
    }
}

BRASIER_TEST ("presets.factory", testPresets);
BRASIER_TEST ("presets.user", testUserPresetRoundTrip);
BRASIER_TEST ("bench.cpu", testCpu);
