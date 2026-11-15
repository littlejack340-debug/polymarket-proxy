#pragma once

#include <juce_dsp/juce_dsp.h>

namespace brasier::dsp
{
    /**
        Auto-gain : compense automatiquement le niveau du signal traite pour qu'il
        ait le meme niveau RMS moyen que le signal d'entree.

        Principe : on estime la puissance moyenne (moyenne des carres, lissee par
        un filtre a un pole d'environ 400 ms) de la reference (entree) et du wet.
        Le gain correcteur vaut sqrt(Pref / Pwet), borne a +/-24 dB, puis il est
        lisse (~150 ms) pour ne jamais "pomper" ni cliquer. En dessous d'un seuil
        (~ -70 dBFS) on gele le gain : les silences ne font pas remonter le bruit.

        Cela permet de comparer la couleur d'un effet sans etre trompe par le
        "plus fort = meilleur".
    */
    class AutoGain
    {
    public:
        void prepare (double sampleRate) noexcept;
        void reset() noexcept;

        /** Active / desactive : la transition est toujours lissee. */
        void setEnabled (bool shouldBeEnabled) noexcept { enabled = shouldBeEnabled; }
        bool isEnabled() const noexcept                 { return enabled; }

        /** Mode statique : au lieu de mesurer, applique un gain connu fourni par le
            plugin (ex. delay/reverb, ou une mesure RMS n'a pas de sens car le wet
            est une queue decalee dans le temps). */
        void setStaticMode (bool shouldBeStatic) noexcept { staticMode = shouldBeStatic; }
        void setStaticGain (float linearGain) noexcept    { staticGain = juce::jlimit (minGain, maxGain, linearGain); }

        /** Mesure reference et wet, puis applique la correction au wet (en place). */
        void process (const juce::dsp::AudioBlock<const float>& reference,
                      juce::dsp::AudioBlock<float>& wet) noexcept;

        /** Gain correcteur actuellement applique (lineaire). */
        float getCurrentGain() const noexcept { return currentGain; }

    private:
        static constexpr int   updateInterval = 16;      // recalcul de la cible tous les 16 echantillons
        static constexpr float silenceThreshold = 1.0e-7f; // ~ -70 dBFS en puissance
        static constexpr float maxGain = 15.848932f;      // +24 dB
        static constexpr float minGain = 0.0630957f;      // -24 dB

        float powerCoeff = 0.0f, gainCoeff = 0.0f;
        float refPower = 0.0f, wetPower = 0.0f;
        float measuredGain = 1.0f;   // derniere correction mesuree (gelee dans les silences)
        float currentGain = 1.0f;    // gain lisse effectivement applique
        float staticGain = 1.0f;
        int counter = 0;
        bool enabled = false;
        bool staticMode = false;
    };
}
