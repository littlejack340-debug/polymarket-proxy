#pragma once

#include <memory>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "BrasierLookAndFeel.h"
#include "PresetBar.h"
#include "VuMeter.h"
#include "core/EffectProcessorBase.h"

namespace brasier::ui
{
    /** Texte UTF-8 (accents) pour l'interface. */
    inline juce::String fr (const char* utf8) { return juce::String::fromUTF8 (utf8); }

    /** Description d'un controle de l'interface. */
    struct ControlSpec
    {
        enum class Style { knob, smallKnob, toggle, selector, largeKnob };

        juce::String parameterId;
        Style style = Style::knob;
        bool bipolar = false;        // arc de valeur partant du centre
        juce::String label {};       // vide = nom du parametre

        // Grise le controle selon un autre parametre : actif si round(valeur de enabledBy) == enabledValue
        juce::String enabledBy {};
        int enabledValue = 1;
    };

    /** Panneau titre contenant des controles (sur une ou plusieurs lignes). */
    struct SectionSpec
    {
        juce::String title;
        std::vector<ControlSpec> controls;
        float width = 1.0f;          // largeur relative dans sa rangee
        int rows = 1;
    };

    /** Description complete de l'interface d'un plugin. */
    struct EditorSpec
    {
        juce::String pluginName;     // ex. "FOURNAISE"
        juce::String tagline;        // ex. "saturateur analogique"
        juce::Colour accent { colours::accentDefault };
        std::vector<std::vector<SectionSpec>> rows;   // rangees de sections
    };

    /**
        Editeur commun de la suite : facade "materiel" dessinee a la taille de
        base (1000 x 600) puis mise a l'echelle (70 % a 200 %) par une
        transformation affine : tout reste vectoriel et net a toutes les tailles.

          - en-tete : marque, nom du plugin, barre de presets
          - corps   : sections decrites par EditorSpec
          - pied    : VU-metres entree / sortie + Mix, Output, Auto Gain,
                      Oversampling, Bypass
    */
    class BrasierEditor : public juce::AudioProcessorEditor,
                          private juce::Timer
    {
    public:
        BrasierEditor (core::EffectProcessorBase& processor, EditorSpec spec);
        ~BrasierEditor() override;

        void paint (juce::Graphics&) override;
        void resized() override;

        static constexpr int baseWidth = 1000;
        static constexpr int baseHeight = 600;

    private:
        struct Control
        {
            ControlSpec spec;
            juce::String name;
            std::unique_ptr<juce::Label> caption;
            std::unique_ptr<juce::Component> component;
            std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAttachment;
            std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> buttonAttachment;
            std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> comboAttachment;
        };

        class Content : public juce::Component
        {
        public:
            explicit Content (BrasierEditor& owner);
            void paint (juce::Graphics&) override;
            void resized() override;

        private:
            void renderBackground (juce::Graphics&);

            BrasierEditor& editor;
            juce::Image cache;
            float cacheScale = 0.0f;
        };

        Control* createControl (const ControlSpec& spec);
        void layoutControlsIn (std::vector<Control*>& controls, juce::Rectangle<int> area, int rows);
        void layoutContent();
        void timerCallback() override;
        void updateEnablement();

        core::EffectProcessorBase& processorRef;
        EditorSpec spec;
        BrasierLookAndFeel lookAndFeel;
        juce::TooltipWindow tooltips { this, 600 };

        Content content { *this };
        PresetBar presetBar;
        VuMeter inputMeter { "ENTREE" }, outputMeter { "SORTIE" };

        std::vector<std::unique_ptr<Control>> controls;
        std::vector<std::vector<std::vector<Control*>>> sectionControls;   // [rangee][section][controle]
        std::vector<Control*> footerControls;

        // Geometrie (coordonnees de base), utilisee aussi pour dessiner la facade
        std::vector<std::vector<juce::Rectangle<int>>> sectionBounds;
        juce::Rectangle<int> headerBounds, bodyBounds, footerBounds, metersBounds, commonBounds;

        int lastPresetIndex = -2;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BrasierEditor)
    };
}
