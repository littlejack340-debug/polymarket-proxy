#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace brasier::ui
{
    /** Palette commune de la suite (provisoire : le design final arrive a l'etape 6). */
    namespace colours
    {
        inline const juce::Colour background   { 0xff1c1a19 };  // charbon
        inline const juce::Colour panel        { 0xff2a2725 };
        inline const juce::Colour panelEdge    { 0xff3d3935 };
        inline const juce::Colour accent       { 0xffe0782f };  // braise
        inline const juce::Colour accentDim    { 0xff7a4423 };
        inline const juce::Colour text         { 0xffece3d6 };  // creme
        inline const juce::Colour textDim      { 0xff9c9286 };
        inline const juce::Colour knobBody     { 0xff35312e };
    }

    class BrasierLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        BrasierLookAndFeel();

        void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                               float sliderPosProportional, float rotaryStartAngle,
                               float rotaryEndAngle, juce::Slider&) override;

        void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

        juce::Font getLabelFont (juce::Label&) override;
    };
}
