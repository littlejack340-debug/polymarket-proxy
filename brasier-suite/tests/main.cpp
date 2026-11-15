// Point d'entree des tests hors-ligne de la suite Brasier.
//
//   BrasierTests                      lance tous les tests
//   BrasierTests fournaise            lance les tests dont le nom contient "fournaise"
//   BrasierTests --snapshot <plugin> capture.png [largeur]   rend l'interface d'un plugin en PNG
//
// Le code de sortie vaut le nombre d'echecs.

#include "TestHelpers.h"
#include "Fournaise/Source/FournaiseProcessor.h"
#include "Gain/Source/GainProcessor.h"
#include "Mue/Source/MueProcessor.h"
#include "Ruban/Source/RubanProcessor.h"

namespace
{
    std::unique_ptr<juce::AudioProcessor> createByName (const juce::String& name)
    {
        if (name.equalsIgnoreCase ("gain"))      return std::make_unique<brasier::gain::GainProcessor>();
        if (name.equalsIgnoreCase ("fournaise")) return std::make_unique<brasier::fournaise::FournaiseProcessor>();
        if (name.equalsIgnoreCase ("ruban"))     return std::make_unique<brasier::ruban::RubanProcessor>();
        if (name.equalsIgnoreCase ("mue"))       return std::make_unique<brasier::mue::MueProcessor>();
        return nullptr;
    }

    int renderEditorSnapshot (const juce::String& pluginName, const juce::File& file, int width)
    {
        auto p = createByName (pluginName);
        if (p == nullptr)
        {
            std::printf ("plugin inconnu : %s\n", pluginName.toRawUTF8());
            return 1;
        }

        p->setRateAndBufferSizeDetails (brasier::tests::sampleRate, brasier::tests::blockSize);
        p->prepareToPlay (brasier::tests::sampleRate, brasier::tests::blockSize);

        std::unique_ptr<juce::AudioProcessorEditor> editor (p->createEditorAndMakeActive());
        editor->setSize (width, width * 9 / 16);

        const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
        file.deleteFile();
        juce::FileOutputStream stream (file);
        juce::PNGImageFormat png;
        const bool ok = stream.openedOk() && png.writeImageToStream (image, stream);
        std::printf ("capture %s : %s\n", file.getFullPathName().toRawUTF8(), ok ? "OK" : "ECHEC");
        return ok ? 0 : 1;
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    if (argc >= 4 && juce::String (argv[1]) == "--snapshot")
        return renderEditorSnapshot (argv[2],
                                     juce::File::getCurrentWorkingDirectory().getChildFile (argv[3]),
                                     argc >= 5 ? juce::String (argv[4]).getIntValue() : 640);

    const juce::String filter = argc >= 2 ? juce::String (argv[1]) : juce::String();
    int ran = 0;

    for (const auto& test : brasier::tests::registry())
    {
        if (filter.isNotEmpty() && ! juce::String (test.name).containsIgnoreCase (filter))
            continue;

        test.run();
        ++ran;
    }

    const int failures = brasier::tests::failureCount();
    std::printf ("\n%d groupe(s) de tests, %s : %d echec(s)\n", ran, failures == 0 ? "OK" : "ECHEC", failures);
    return failures;
}
