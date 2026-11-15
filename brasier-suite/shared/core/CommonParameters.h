#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace brasier::core
{
    /** Identifiants des parametres communs a tous les plugins de la suite.
        Ne jamais les renommer : ils servent a relire les projets et presets. */
    namespace ids
    {
        inline constexpr const char* mix          = "mix";
        inline constexpr const char* output       = "output";
        inline constexpr const char* autoGain     = "autogain";
        inline constexpr const char* oversampling = "oversampling";
        inline constexpr const char* bypass       = "bypass";
    }

    /** Version des identifiants de parametres (juce::ParameterID). */
    inline constexpr int parameterVersion = 1;

    struct CommonParameterOptions
    {
        bool  withOversampling = true;   // le plugin a des etages non lineaires
        int   defaultOversamplingIndex = 1;  // 0 = 1x, 1 = 2x, 2 = 4x, 3 = 8x
        float defaultMixPercent = 100.0f;
    };

    /** Ajoute Mix, Output, Auto-gain, Oversampling et Bypass a la fin du layout. */
    void addCommonParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout,
                              const CommonParameterOptions& options);

    /** Identifiants des parametres communs (pour la section "pied" des interfaces). */
    juce::StringArray getCommonParameterIds();

    //==============================================================================
    // Fabriques de parametres avec un affichage soigne (dB, %, Hz...)

    std::unique_ptr<juce::AudioParameterFloat> makeDecibelParameter (const juce::String& id,
                                                                     const juce::String& name,
                                                                     float minDb, float maxDb, float defaultDb);

    std::unique_ptr<juce::AudioParameterFloat> makePercentParameter (const juce::String& id,
                                                                     const juce::String& name,
                                                                     float defaultPercent,
                                                                     float maxPercent = 100.0f);

    /** Temps en millisecondes, echelle logarithmique, affiche "350 ms" ou "1.25 s". */
    std::unique_ptr<juce::AudioParameterFloat> makeTimeParameter (const juce::String& id,
                                                                  const juce::String& name,
                                                                  float minMs, float maxMs, float defaultMs);

    /** Parametre continu generique avec unite et nombre de decimales affichees. */
    std::unique_ptr<juce::AudioParameterFloat> makeFloatParameter (const juce::String& id,
                                                                   const juce::String& name,
                                                                   juce::NormalisableRange<float> range,
                                                                   float defaultValue,
                                                                   const juce::String& unit,
                                                                   int decimals);

    std::unique_ptr<juce::AudioParameterFloat> makeFrequencyParameter (const juce::String& id,
                                                                       const juce::String& name,
                                                                       float minHz, float maxHz, float defaultHz);

    std::unique_ptr<juce::AudioParameterChoice> makeChoiceParameter (const juce::String& id,
                                                                     const juce::String& name,
                                                                     const juce::StringArray& choices,
                                                                     int defaultIndex,
                                                                     bool automatable = true);

    std::unique_ptr<juce::AudioParameterBool> makeBoolParameter (const juce::String& id,
                                                                 const juce::String& name,
                                                                 bool defaultValue);
}
