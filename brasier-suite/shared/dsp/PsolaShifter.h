#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include <juce_dsp/juce_dsp.h>

#include "PitchDetector.h"

namespace brasier::dsp
{
    /**
        Pitch shifter / formant shifter TD-PSOLA (Time-Domain Pitch-Synchronous
        Overlap-Add), pense pour la voix et une faible latence.

        1. Analyse : YIN donne la periode P. On pose des "marques d'analyse"
           espacees de P dans le signal d'entree : chaque marque est le centre d'un
           grain de deux periodes (fenetre de Hann +/-P), qui contient en gros une
           reponse du conduit vocal (les formants).

        2. Synthese : on recolle des grains a un autre espacement.
              - Pitch : espacement de sortie = P / ratio (grains plus serres = plus aigu).
                Les formants ne bougent pas car le contenu des grains est intact.
              - Formant : chaque grain est compresse / etire dans le temps d'un
                facteur F avant d'etre recolle. Son spectre (l'enveloppe = les
                formants) est decale de F, sans changer l'espacement (la hauteur).
              - Robot : espacement de sortie fixe (note choisie), quelle que soit
                la hauteur chantee.

        Latence : un grain doit etre entierement recu avant d'etre recolle, soit
        environ deux periodes de la plus grave voix detectable :
              Live    : voix >= 100 Hz -> ~20 ms
              Qualite : voix >=  60 Hz -> ~34 ms

        Stereo : la hauteur est detectee sur la somme mono, les deux canaux
        utilisent les memes marques (l'image stereo reste coherente).
    */
    class PsolaShifter
    {
    public:
        enum class LatencyMode { live = 0, quality };

        static constexpr int maxChannels = 2;

        void prepare (double sampleRate, int numChannels);
        void reset() noexcept;

        /** Temps reel (les buffers sont dimensionnes pour le mode Qualite). */
        void setLatencyMode (LatencyMode mode) noexcept;
        LatencyMode getLatencyMode() const noexcept { return latencyMode; }

        int getLatencySamples() const noexcept    { return latency; }
        int getMaxLatencySamples() const noexcept { return latencyFor (LatencyMode::quality); }

        void setPitchRatio (float ratio) noexcept     { pitchRatio = juce::jlimit (0.25f, 4.0f, ratio); }
        void setFormantRatio (float ratio) noexcept   { formantRatio = juce::jlimit (0.25f, 4.0f, ratio); }
        void setRobot (bool enabled, float frequencyHz) noexcept
        {
            robot = enabled;
            robotHz = juce::jlimit (20.0f, 2000.0f, frequencyHz);
        }

        /** Traitement en place, echantillon par echantillon (independant de la taille des blocs). */
        void process (juce::dsp::AudioBlock<float> block) noexcept;

        float getDetectedPeriod() const noexcept { return detector.getPeriod(); }
        bool isVoiced() const noexcept           { return detector.isVoiced(); }

    private:
        struct Mark
        {
            double position = 0.0;   // en echantillons absolus d'entree
            float period = 300.0f;
        };

        int latencyFor (LatencyMode mode) const noexcept;
        double lowestHzFor (LatencyMode mode) const noexcept { return mode == LatencyMode::live ? 100.0 : 60.0; }

        inline float readInput (int channel, double absolutePosition) const noexcept;
        void createMarks() noexcept;
        void scheduleGrains (int64_t emitIndex) noexcept;
        int64_t findNearestMark (double position) const noexcept;   // index absolu
        const Mark& markAt (int64_t absoluteIndex) const noexcept { return marks[static_cast<size_t> (absoluteIndex % markCapacity)]; }
        void placeGrain (const Mark& mark, double centre, float spacing, int64_t emitIndex) noexcept;

        double sampleRate = 44100.0;
        int numChans = 2;
        LatencyMode latencyMode = LatencyMode::live;
        int latency = 0;
        float maxPeriod = 800.0f;

        PitchDetector detector;
        int analysisHop = 256, hopCounter = 0;

        // Anneaux d'entree et de sortie (overlap-add), indexes en temps absolu
        std::array<std::vector<float>, maxChannels> input, output;
        int64_t ringMask = 0;
        int64_t now = 0;

        // Marques d'analyse (anneau, ordre croissant)
        static constexpr int markCapacity = 128;
        std::array<Mark, markCapacity> marks {};
        int64_t markCount = 0;
        double lastMarkPosition = 0.0;

        double nextSynthesis = 0.0;
        int64_t nextMarkForIdentity = -1;

        float pitchRatio = 1.0f, formantRatio = 1.0f, robotHz = 130.81f;
        bool robot = false;
    };
}
