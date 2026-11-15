#pragma once

#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

namespace brasier::core
{
    /** Preset d'usine : valeurs en unites reelles (dB, Hz, %, index de choix...).
        Les parametres absents reprennent leur valeur par defaut. */
    struct FactoryPreset
    {
        juce::String name;
        juce::String category;   // ex. "Voix lead", "Ad-libs", "808", "Samples"
        std::vector<std::pair<juce::String, float>> values;
    };

    /**
        Presets d'usine + presets utilisateur (fichiers .brasierpreset en XML dans
        Documents/Brasier Audio/<plugin>/Presets).

        A n'utiliser que depuis le thread message (interface, changement de
        programme par l'hote) : chargement et sauvegarde passent par les
        parametres de l'APVTS (setValueNotifyingHost), donc l'automation et
        l'annulation de l'hote restent coherentes.

        Ne touche jamais a l'oversampling (choix de qualite / CPU de
        l'utilisateur) ni au bypass.
    */
    class PresetManager
    {
    public:
        PresetManager (juce::AudioProcessorValueTreeState& state,
                       juce::String pluginName,
                       std::vector<FactoryPreset> factoryPresets);

        int getNumFactoryPresets() const noexcept { return static_cast<int> (factory.size()); }
        const FactoryPreset& getFactoryPreset (int index) const { return factory[static_cast<size_t> (index)]; }

        /** Liste complete affichee dans l'interface : usine puis utilisateur. */
        juce::StringArray getPresetNames() const;
        int getNumPresets() const { return getNumFactoryPresets() + userPresets.size(); }

        void loadPreset (int index);
        void loadFactoryPreset (int index);
        bool loadUserPreset (const juce::File& file);
        bool saveUserPreset (const juce::String& name);
        void loadNext();
        void loadPrevious();

        int getCurrentIndex() const noexcept           { return currentIndex; }

        /** Restaure l'indice du preset courant sans toucher aux parametres (rechargement de projet). */
        void setCurrentIndexSilently (int index) noexcept { currentIndex = juce::jlimit (-1, getNumPresets() - 1, index); }
        juce::String getCurrentName() const;
        bool isFactory (int index) const noexcept      { return index >= 0 && index < getNumFactoryPresets(); }

        juce::File getUserDirectory() const;
        void refreshUserPresets();

        static constexpr const char* fileExtension = ".brasierpreset";

    private:
        void resetPluginParametersToDefault();
        bool isProtectedParameter (const juce::String& id) const;

        juce::AudioProcessorValueTreeState& state;
        juce::String pluginName;
        std::vector<FactoryPreset> factory;
        juce::Array<juce::File> userPresets;
        int currentIndex = -1;
    };
}
