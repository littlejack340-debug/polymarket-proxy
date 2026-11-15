#pragma once

#include <atomic>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "CommonParameters.h"
#include "PresetManager.h"
#include "dsp/AutoGain.h"
#include "dsp/DryWetMixer.h"
#include "dsp/LinearSmoother.h"
#include "dsp/Oversampler.h"

namespace brasier::core
{
    /** Informations donnees a l'effet au moment de la preparation (hors thread audio). */
    struct EffectSpec
    {
        double sampleRate = 44100.0;
        int maxBlockSize = 512;
        int numChannels = 2;
        int maxOversamplingFactor = dsp::Oversampler::maxFactor;
    };

    /** Contexte passe a chaque bloc audio. */
    struct ProcessContext
    {
        double sampleRate = 44100.0;         // frequence de base (celle de l'hote)
        double oversampledRate = 44100.0;    // sampleRate * facteur d'oversampling courant
        int oversamplingFactor = 1;
        bool oversamplingChanged = false;    // vrai sur le premier bloc apres un changement de facteur
        int numChannels = 2;

        // Transport de l'hote (tempo pour les delays/LFO synchronises)
        double bpm = 120.0;
        double ppqPosition = 0.0;
        bool isPlaying = false;
    };

    /**
        Socle commun de tous les plugins de la suite.

        Il gere tout ce qui est identique d'un plugin a l'autre, pour que chaque
        plugin n'ait plus qu'a implementer son DSP "wet" :

          entree -> [dry retarde de la latence] ---------------------------+
                 \                                                          |
                  -> processEffect() (avec oversampling optionnel) -> auto-gain -> mix -> output -> sortie

        - parametres communs (Mix, Output, Auto Gain, Oversampling, Bypass) via APVTS
        - lissage des parametres communs (aucun clic)
        - oversampling 1x/2x/4x/8x pre-alloue, commutable sans allocation
        - calcul et report de la latence a l'hote (PDC de FL Studio)
        - bypass "doux" compense en latence
        - decoupage des blocs trop grands envoyes par l'hote
        - sauvegarde / rechargement de l'etat

        Regles pour le thread audio (processEffect) : pas d'allocation, pas de
        verrou, pas d'appel systeme. Tout doit etre alloue dans prepareEffect().
    */
    class EffectProcessorBase : public juce::AudioProcessor,
                                private juce::Timer
    {
    public:
        enum class AutoGainMode
        {
            measured,   // mesure RMS entree / wet (saturation, filtre, modulation, pitch)
            fixed       // gain fourni par getStaticAutoGainDb() (delay, reverb)
        };

        struct Options
        {
            juce::String name;
            CommonParameterOptions common;
            dsp::DryWetMixer::MixLaw mixLaw = dsp::DryWetMixer::MixLaw::linear;
            AutoGainMode autoGainMode = AutoGainMode::measured;
            double tailSeconds = 0.0;

            /** Vrai si l'effet utilise l'oversampler a l'interieur d'une boucle et
                compense lui-meme sa latence (delay a bande) : rien n'est alors
                reporte a l'hote pour l'oversampling. */
            bool oversamplingLatencyHandledByEffect = false;

            /** Presets d'usine du plugin (aussi exposes comme "programmes" a l'hote). */
            std::vector<FactoryPreset> factoryPresets;
        };

        /** Niveaux mesures sur le dernier bloc (pour les VU-metres), en carre moyen lineaire. */
        struct MeterLevels
        {
            float inputMeanSquare[2] {};
            float outputMeanSquare[2] {};
            float outputPeak = 0.0f;
        };

        EffectProcessorBase (Options options,
                             juce::AudioProcessorValueTreeState::ParameterLayout pluginParameters);
        ~EffectProcessorBase() override;

        juce::AudioProcessorValueTreeState& getValueTreeState() noexcept { return apvts; }
        const Options& getOptions() const noexcept                       { return options; }
        PresetManager& getPresetManager() noexcept                       { return presets; }

        /** Lecture des niveaux (thread message, interface). */
        MeterLevels getMeterLevels() const noexcept;

        /** Taille de l'interface memorisee dans l'etat du plugin (thread message). */
        float getEditorScale() const;
        void setEditorScale (float scale);

        //==============================================================================
        const juce::String getName() const override   { return options.name; }
        void prepareToPlay (double sampleRate, int samplesPerBlock) override;
        void releaseResources() override {}
        void reset() override;
        bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

        using juce::AudioProcessor::processBlock;
        using juce::AudioProcessor::processBlockBypassed;
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
        void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

        juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParameter; }

        double getTailLengthSeconds() const override { return options.tailSeconds; }
        bool acceptsMidi() const override            { return false; }
        bool producesMidi() const override           { return false; }
        bool isMidiEffect() const override           { return false; }

        int getNumPrograms() override;
        int getCurrentProgram() override;
        void setCurrentProgram (int index) override;
        const juce::String getProgramName (int index) override;
        void changeProgramName (int, const juce::String&) override  {}

        void getStateInformation (juce::MemoryBlock& destData) override;
        void setStateInformation (const void* data, int sizeInBytes) override;

    protected:
        //==============================================================================
        // A implementer par chaque plugin

        /** Allocation et preparation du DSP (hors thread audio). Les buffers qui
            tournent a la frequence sur-echantillonnee doivent etre dimensionnes pour
            spec.maxOversamplingFactor, pour que le changement de facteur soit sans allocation. */
        virtual void prepareEffect (const EffectSpec& spec) = 0;

        /** Remise a zero des etats internes (lignes de retard, filtres...). */
        virtual void resetEffect() = 0;

        /** Traitement du signal wet, en place, a la frequence de base.
            Pour les etages non lineaires :
                auto up = oversampler().processUp (block);
                ... traitement de 'up' a ctx.oversampledRate ...
                oversampler().processDown (block); */
        virtual void processEffect (juce::dsp::AudioBlock<float> block, const ProcessContext& ctx) = 0;

        /** Latence propre a l'effet (hors oversampling), en echantillons. Elle peut
            changer en cours de lecture (ex. mode Live / Qualite du pitch shifter) :
            le socle la relit a chaque bloc, juste apres updateEffectLatency(). */
        virtual int getEffectLatencySamples() const { return 0; }

        /** Latence maximale possible de l'effet (dimensionne la ligne de retard du dry). */
        virtual int getMaxEffectLatencySamples() const { return getEffectLatencySamples(); }

        /** Appele en debut de bloc (thread audio) : l'effet peut y appliquer un
            reglage qui change sa latence, avant que le socle ne la relise. */
        virtual void updateEffectLatency() {}

        /** Gain d'auto-gain (dB) en mode AutoGainMode::fixed. Appele a chaque bloc
            depuis le thread audio : doit etre un simple calcul. */
        virtual float getStaticAutoGainDb() const { return 0.0f; }

        dsp::Oversampler& oversampler() noexcept { return overSampler; }

        juce::AudioProcessorValueTreeState apvts;

    private:
        static BusesProperties makeBuses();
        static juce::AudioProcessorValueTreeState::ParameterLayout
            buildLayout (juce::AudioProcessorValueTreeState::ParameterLayout pluginParameters,
                         const CommonParameterOptions& common);

        void processInternal (juce::AudioBuffer<float>& buffer, bool forceBypass) noexcept;
        void processChunk (juce::dsp::AudioBlock<float> block) noexcept;
        void measureLevels (const juce::AudioBuffer<float>& buffer, int numChannels, bool input) noexcept;
        void updateHostInfo() noexcept;
        void updateOversampling() noexcept;
        void updateLatency() noexcept;
        int computeTotalLatency() const noexcept;
        void timerCallback() override;

        Options options;

        // Parametres communs (pointeurs vers les valeurs atomiques de l'APVTS)
        std::atomic<float>* mixParam = nullptr;
        std::atomic<float>* outputParam = nullptr;
        std::atomic<float>* autoGainParam = nullptr;
        std::atomic<float>* oversamplingParam = nullptr;
        std::atomic<float>* bypassValue = nullptr;
        juce::AudioParameterBool* bypassParameter = nullptr;

        dsp::Oversampler overSampler;
        dsp::DryWetMixer dryWet;
        dsp::AutoGain autoGain;
        dsp::LinearSmoother<float> outputGain;

        ProcessContext context;
        int preparedBlockSize = 0;
        bool isPrepared = false;

        // Latence calculee dans le thread audio, reportee a l'hote par le thread message
        std::atomic<int> pendingLatency { 0 };

        // Changement de programme demande hors du thread message : applique par le Timer
        std::atomic<int> pendingProgram { -1 };

        // VU-metres (ecrits par le thread audio, lus par l'interface)
        std::atomic<float> inputLevel[2] {}, outputLevel[2] {}, outputPeak { 0.0f };

        PresetManager presets;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EffectProcessorBase)
    };
}
