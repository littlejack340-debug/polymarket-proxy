#pragma once

#include <array>
#include <vector>

#include <juce_dsp/juce_dsp.h>

#include "DelayLine.h"
#include "LinearSmoother.h"
#include "Lfo.h"
#include "OnePole.h"
#include "Svf.h"

namespace brasier::dsp
{
    /**
        Modulations a caractere analogique : chorus, flanger, phaser, tremolo.

        - Chorus  : 3 voix par canal, retard ~12 ms module par des LFO dephases
                    (1/3 de tour), comme un chorus a BBD multi-voix.
        - Flanger : retard tres court (0,25 a 5 ms) module, avec feedback +/- :
                    filtre en peigne qui balaie.
        - Phaser  : 6 etages passe-tout du 1er ordre dont la frequence balaie
                    exponentiellement, avec feedback : creux qui se deplacent.
        - Tremolo : modulation d'amplitude avec le "retard" d'une cellule
                    photo-resistive (attaque rapide, relachement lent), comme un
                    tremolo optique d'ampli. A 100 % de spread = auto-pan.

        "Character" ajoute la couleur analogique : passe-bas type BBD (les
        puces a retard analogiques perdent les aigus), saturation douce dans le
        chemin du retard / du feedback et une legere derive aleatoire des LFO.

        Le traitement tourne a la frequence sur-echantillonnee (saturation) ;
        les LFO sont calcules a la frequence de base puis interpoles.
    */
    class AnalogModulator
    {
    public:
        enum class Mode { chorus = 0, flanger, phaser, tremolo };

        static juce::StringArray getModeNames() { return { "Chorus", "Flanger", "Phaser", "Tremolo" }; }

        struct Settings
        {
            Mode mode = Mode::chorus;
            double rateHz = 0.8;
            float depth = 0.5f;          // 0..1
            float feedback = 0.0f;       // -0.95..0.95 (flanger, phaser)
            float spread = 0.5f;         // 0..1 : dephasage du LFO droit (0 a 180 degres)
            float character = 0.3f;      // 0..1
            Lfo::Shape shape = Lfo::Shape::sine;
        };

        void prepare (double baseSampleRate, int maxBlockSize, int maxOversamplingFactor);
        void reset() noexcept;

        void setSettings (const Settings& s) noexcept;

        /** Recale les LFO sur le transport de l'hote (modulation synchronisee). */
        void syncToPpq (double ppqPosition, double cyclesPerQuarter) noexcept;

        /** Etape 1 (frequence de base) : calcule les LFO du bloc. */
        void computeModulation (int numSamples, double oversampledRate) noexcept;

        /** Etape 2 : traite le bloc sur-echantillonne (factor = facteur courant). */
        void processOversampled (juce::dsp::AudioBlock<float> up, int factor, double oversampledRate) noexcept;

    private:
        static constexpr int numVoices = 3;
        static constexpr int numSlots = numVoices * 2;   // voix x canaux
        static constexpr int numPhaserStages = 6;

        float lfoValue (int slot, size_t upIndex, int factor) const noexcept;
        void updateCharacter (double oversampledRate) noexcept;

        double baseRate = 44100.0;
        Settings settings;
        Mode activeMode = Mode::chorus;

        std::array<Lfo, numSlots> lfos;
        Lfo driftLfo;
        std::array<std::vector<float>, numSlots> lfoTable;   // [slot][i], i = 0..n (0 = dernier du bloc precedent)
        int tableLength = 0;

        DelayLine delay;
        std::array<SvfState, 2 * numVoices> bbdFilters {};
        SvfCoeffs bbdCoeffs;
        double coeffRate = 0.0;
        float coeffCharacter = -1.0f;

        std::array<std::array<float, numPhaserStages>, 2> phaserStates {};
        std::array<float, 2> feedbackSample {};

        std::array<float, 2> opticalGain { 1.0f, 1.0f };
        float opticalAttack = 0.0f, opticalRelease = 0.0f;

        LinearSmoother<float> depth, feedback, character;

        // Changement de mode : fondu sortant, bascule au bloc suivant (tables coherentes), fondu entrant
        float modeGain = 1.0f;
        bool switching = false;
        bool switchReady = false;
        float appliedSpread = -1.0f;
    };
}
