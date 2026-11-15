#pragma once

#include <memory>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "BrasierLookAndFeel.h"

namespace brasier::ui
{
    /**
        Editeur generique et redimensionnable : cree automatiquement un potard
        par parametre continu, un menu par parametre a choix et un interrupteur
        par parametre on/off, relies a l'APVTS par des attachments.

        Sert d'interface de travail pendant le developpement du DSP ; chaque plugin
        recevra son interface dediee a l'etape 6.
    */
    class GenericKnobEditor : public juce::AudioProcessorEditor
    {
    public:
        struct Options
        {
            juce::String title;
            juce::String subtitle;
            juce::StringArray footerParameterIds;   // parametres affiches dans le bandeau du bas
            juce::StringArray hiddenParameterIds;   // ex. "bypass" (deja gere par l'hote)
        };

        GenericKnobEditor (juce::AudioProcessor& owner,
                           juce::AudioProcessorValueTreeState& state,
                           Options options);
        ~GenericKnobEditor() override;

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        struct Control
        {
            juce::String parameterId;
            bool isFooter = false;
            std::unique_ptr<juce::Label> label;
            std::unique_ptr<juce::Component> component;
            std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAttachment;
            std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> buttonAttachment;
            std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> comboAttachment;
        };

        void addControlFor (juce::RangedAudioParameter& parameter, bool isFooter);
        void layoutRow (std::vector<Control*>& row, juce::Rectangle<int> area);

        juce::AudioProcessorValueTreeState& state;
        Options options;
        BrasierLookAndFeel lookAndFeel;
        std::vector<std::unique_ptr<Control>> controls;

        static constexpr int baseWidth = 640;
        static constexpr int baseHeight = 360;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GenericKnobEditor)
    };
}
