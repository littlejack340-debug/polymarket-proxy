// Hote de test : charge les vrais binaires .vst3 compiles, comme le ferait FL Studio,
// et verifie chargement, traitement, automation, etat, programmes et editeur.
//
//   BrasierHostTest <dossier contenant les .vst3>       (ex. build/vs2022/plugins)
//
// Le code de sortie vaut le nombre d'echecs.

#include <cmath>
#include <cstdio>
#include <random>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace
{
    int failures = 0;

    void check (bool condition, const juce::String& what)
    {
        std::printf ("  [%s] %s\n", condition ? "PASS" : "FAIL", what.toRawUTF8());
        if (! condition)
            ++failures;
    }

    std::unique_ptr<juce::AudioPluginInstance> load (juce::AudioPluginFormat& format, const juce::PluginDescription& desc)
    {
        juce::String error;
        auto instance = format.createInstanceFromDescription (desc, 48000.0, 512, error);
        if (instance == nullptr)
            std::printf ("  erreur de chargement : %s\n", error.toRawUTF8());
        return instance;
    }

    void testPlugin (juce::AudioPluginFormat& format, const juce::PluginDescription& desc)
    {
        std::printf ("\n== %s (%s) ==\n", desc.name.toRawUTF8(), desc.fileOrIdentifier.toRawUTF8());

        auto plugin = load (format, desc);
        check (plugin != nullptr, "le plugin se charge");
        if (plugin == nullptr)
            return;

        check (desc.manufacturerName == "Brasier Audio", "fabricant : Brasier Audio");
        check (plugin->getParameters().size() >= 5, "parametres exposes a l'hote (" + juce::String (plugin->getParameters().size()) + ")");

        plugin->setPlayConfigDetails (2, 2, 48000.0, 512);
        plugin->prepareToPlay (48000.0, 512);
        std::printf ("  latence reportee : %d echantillons\n", plugin->getLatencySamples());

        // Traitement avec automation aleatoire de tous les parametres
        std::mt19937 rng (1);
        std::uniform_real_distribution<float> uni (0.0f, 1.0f);
        juce::AudioBuffer<float> buffer (2, 512);
        juce::MidiBuffer midi;
        bool finite = true;
        float peak = 0.0f;

        for (int block = 0; block < 300; ++block)
        {
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 512; ++i)
                    buffer.setSample (ch, i, 0.5f * (uni (rng) * 2.0f - 1.0f));

            // L'hote automatise les reglages, pas le bypass ni le selecteur de programme
            if (block % 4 == 0)
                for (auto* p : plugin->getParameters())
                    if (! p->getName (32).containsIgnoreCase ("bypass") && p->getName (32) != "Program" && uni (rng) < 0.3f)
                        p->setValue (uni (rng));

            plugin->processBlock (buffer, midi);

            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < 512; ++i)
                    finite = finite && std::isfinite (buffer.getSample (ch, i));
            peak = juce::jmax (peak, buffer.getMagnitude (0, 512));
        }

        check (finite, "traitement sous automation aleatoire : aucun NaN/Inf");
        check (peak < 500.0f, "sortie bornee (pic " + juce::String (peak, 2) + ")");

        // Etat : sauvegarde puis rechargement dans une autre instance
        juce::MemoryBlock state;
        plugin->getStateInformation (state);
        auto other = load (format, desc);
        bool stateOk = other != nullptr && state.getSize() > 0;
        if (other != nullptr)
        {
            other->setStateInformation (state.getData(), static_cast<int> (state.getSize()));
            const auto& a = plugin->getParameters();
            const auto& b = other->getParameters();
            stateOk = stateOk && a.size() == b.size();
            for (int i = 0; stateOk && i < a.size(); ++i)
            {
                // Valeur affichee identique (un choix regle a 0,746 est relu a sa position exacte)
                if (a[i]->getName (32) == "Program")
                    continue;
                const bool sameText = a[i]->getText (a[i]->getValue(), 64) == b[i]->getText (b[i]->getValue(), 64);
                if (! sameText && std::abs (a[i]->getValue() - b[i]->getValue()) > 0.02f)
                {
                    std::printf ("  parametre %s : %.3f au lieu de %.3f\n", a[i]->getName (32).toRawUTF8(),
                                 static_cast<double> (b[i]->getValue()), static_cast<double> (a[i]->getValue()));
                    stateOk = false;
                }
            }
        }
        check (stateOk, "etat sauvegarde et restaure par l'hote (" + juce::String (static_cast<int> (state.getSize())) + " octets)");

        // Programmes (presets d'usine vus par l'hote)
        const int programs = plugin->getNumPrograms();
        std::printf ("  programmes : %d (ex. \"%s\")\n", programs, plugin->getProgramName (juce::jmin (1, programs - 1)).toRawUTF8());
        check (programs >= 3, "presets d'usine exposes comme programmes");
        if (programs > 1)
        {
            plugin->setCurrentProgram (1);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (200);
            plugin->processBlock (buffer, midi);
        }

        // Editeur
        check (plugin->hasEditor(), "le plugin a une interface");
        if (plugin->hasEditor())
        {
            std::unique_ptr<juce::AudioProcessorEditor> editor (plugin->createEditorAndMakeActive());
            check (editor != nullptr && editor->getWidth() >= 600 && editor->getHeight() >= 360,
                   "l'interface s'ouvre (" + (editor != nullptr ? juce::String (editor->getWidth()) + "x" + juce::String (editor->getHeight()) : juce::String ("-")) + ")");
           #if ! JUCE_LINUX   // sous Linux sans gestionnaire de fenetres (Xvfb nu), X11 refuse certains atomes
            if (editor != nullptr)
            {
                editor->addToDesktop (0);
                editor->setVisible (true);
                juce::MessageManager::getInstance()->runDispatchLoopUntil (400);
                editor->setVisible (false);
                editor->removeFromDesktop();
            }
           #endif
        }

        plugin->releaseResources();
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    if (argc < 2)
    {
        std::printf ("usage : BrasierHostTest <dossier contenant les .vst3>\n");
        return 1;
    }

    const juce::File root (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    juce::Array<juce::File> bundles;
    for (const auto& f : root.findChildFiles (juce::File::findDirectories, true, "*.vst3"))
        if (f.getParentDirectory().getFileName() == "VST3")   // le bundle, pas le binaire interne
            bundles.add (f);
    bundles.sort();

    std::printf ("%d bundle(s) VST3 trouve(s) dans %s\n", bundles.size(), root.getFullPathName().toRawUTF8());

    juce::VST3PluginFormat format;
    int loaded = 0;

    for (const auto& bundle : bundles)
    {
        juce::OwnedArray<juce::PluginDescription> types;
        format.findAllTypesForFile (types, bundle.getFullPathName());
        check (! types.isEmpty(), bundle.getFileName() + " : le scan VST3 trouve le plugin");

        for (auto* desc : types)
        {
            testPlugin (format, *desc);
            ++loaded;
        }
    }

    check (loaded > 0, "au moins un plugin teste");
    std::printf ("\n%d plugin(s) testes, %s : %d echec(s)\n", loaded, failures == 0 ? "OK" : "ECHEC", failures);
    return failures;
}
