#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace brasier::ui
{
    /** Palette commune de la suite : charbon, creme, braise. Chaque plugin ajoute sa couleur d'accent. */
    namespace colours
    {
        inline const juce::Colour background   { 0xff17151a };
        inline const juce::Colour faceplate    { 0xff26231f };
        inline const juce::Colour faceplateHi  { 0xff332f2a };
        inline const juce::Colour panel        { 0xff1d1b18 };
        inline const juce::Colour panelEdge    { 0xff4a443c };
        inline const juce::Colour engraving    { 0xff0d0c0b };
        inline const juce::Colour text         { 0xffece3d2 };
        inline const juce::Colour textDim      { 0xffa39886 };
        inline const juce::Colour lcd          { 0xff0f0e0d };
        inline const juce::Colour knobBody     { 0xff2b2826 };
        inline const juce::Colour metal        { 0xffb9b3a8 };
        inline const juce::Colour accentDefault{ 0xffe0782f };
    }

    /** Propriete de Slider : arc de valeur partant du centre (parametres bipolaires). */
    inline const juce::Identifier bipolarProperty { "bipolar" };

    /**
        Look "materiel" original de la suite Brasier :
          - potards en bakelite avec jupe moletee, capuchon metal brosse,
            index de la couleur d'accent, graduations et arc de valeur
          - interrupteurs a LED, selecteurs encastres, afficheurs type LCD
    */
    class BrasierLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        explicit BrasierLookAndFeel (juce::Colour accentColour = colours::accentDefault);

        void setAccent (juce::Colour c);
        juce::Colour getAccent() const noexcept { return accent; }

        void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                               float sliderPosProportional, float rotaryStartAngle,
                               float rotaryEndAngle, juce::Slider&) override;

        juce::Label* createSliderTextBox (juce::Slider&) override;
        juce::Slider::SliderLayout getSliderLayout (juce::Slider&) override;

        void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

        void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                                   bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
        void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;

        void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                           int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
        juce::Font getComboBoxFont (juce::ComboBox&) override;
        void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

        juce::Font getLabelFont (juce::Label&) override;
        void drawLabel (juce::Graphics&, juce::Label&) override;
        juce::Font getPopupMenuFont() override;

        /** Texte grave dans la facade (ombre + lumiere). */
        static void drawEngravedText (juce::Graphics&, const juce::String& text, juce::Rectangle<float> area,
                                      juce::Font font, juce::Justification justification, juce::Colour colour);

        /** Vis cruciforme de facade. */
        static void drawScrew (juce::Graphics&, juce::Point<float> centre, float radius, float angle);

        static juce::Font monoFont (float height);

    private:
        juce::Colour accent;
    };
}
