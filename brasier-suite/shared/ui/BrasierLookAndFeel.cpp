#include "BrasierLookAndFeel.h"

namespace brasier::ui
{
    BrasierLookAndFeel::BrasierLookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, colours::background);
        setColour (juce::Label::textColourId, colours::text);
        setColour (juce::Slider::textBoxTextColourId, colours::text);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, colours::panel);
        setColour (juce::Slider::rotarySliderFillColourId, colours::accent);
        setColour (juce::Slider::rotarySliderOutlineColourId, colours::panelEdge);
        setColour (juce::ComboBox::backgroundColourId, colours::panel);
        setColour (juce::ComboBox::outlineColourId, colours::panelEdge);
        setColour (juce::ComboBox::textColourId, colours::text);
        setColour (juce::ComboBox::arrowColourId, colours::accent);
        setColour (juce::PopupMenu::backgroundColourId, colours::panel);
        setColour (juce::PopupMenu::textColourId, colours::text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::accentDim);
        setColour (juce::ToggleButton::textColourId, colours::text);
        setColour (juce::ToggleButton::tickColourId, colours::accent);
    }

    void BrasierLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                               float pos, float startAngle, float endAngle, juce::Slider&)
    {
        const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
        const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();
        const auto angle  = startAngle + pos * (endAngle - startAngle);
        const auto arcRadius = radius - 2.0f;
        const auto trackWidth = juce::jmax (2.0f, radius * 0.09f);

        // Piste de fond + arc de valeur
        juce::Path track;
        track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
        g.setColour (colours::panelEdge);
        g.strokePath (track, juce::PathStrokeType (trackWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path value;
        value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, angle, true);
        g.setColour (colours::accent);
        g.strokePath (value, juce::PathStrokeType (trackWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Corps du bouton
        const auto bodyRadius = radius * 0.72f;
        juce::ColourGradient body (colours::knobBody.brighter (0.25f), centre.x, centre.y - bodyRadius,
                                   colours::knobBody.darker (0.4f), centre.x, centre.y + bodyRadius, false);
        g.setGradientFill (body);
        g.fillEllipse (juce::Rectangle<float> (bodyRadius * 2.0f, bodyRadius * 2.0f).withCentre (centre));

        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawEllipse (juce::Rectangle<float> (bodyRadius * 2.0f, bodyRadius * 2.0f).withCentre (centre), 1.0f);

        // Index
        juce::Path pointer;
        const auto pointerLength = bodyRadius * 0.75f;
        const auto pointerWidth  = juce::jmax (2.0f, bodyRadius * 0.12f);
        pointer.addRoundedRectangle (-pointerWidth * 0.5f, -bodyRadius + 3.0f, pointerWidth, pointerLength * 0.55f, pointerWidth * 0.5f);
        pointer.applyTransform (juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
        g.setColour (colours::text);
        g.fillPath (pointer);
    }

    void BrasierLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                               bool highlighted, bool)
    {
        auto bounds = button.getLocalBounds().toFloat().reduced (2.0f);
        const auto on = button.getToggleState();

        g.setColour (on ? colours::accentDim : colours::panel);
        g.fillRoundedRectangle (bounds, 4.0f);
        g.setColour (on ? colours::accent : (highlighted ? colours::textDim : colours::panelEdge));
        g.drawRoundedRectangle (bounds, 4.0f, 1.2f);

        // Petite LED
        const auto ledSize = juce::jmin (10.0f, bounds.getHeight() * 0.4f);
        const auto led = juce::Rectangle<float> (ledSize, ledSize)
                             .withCentre ({ bounds.getX() + 6.0f + ledSize * 0.5f, bounds.getCentreY() });
        g.setColour (on ? colours::accent : colours::knobBody.darker (0.3f));
        g.fillEllipse (led);

        g.setColour (colours::text);
        g.setFont (juce::FontOptions (13.0f));
        g.drawFittedText (button.getButtonText(),
                          bounds.withTrimmedLeft (ledSize + 10.0f).toNearestInt(),
                          juce::Justification::centred, 1);
    }

    juce::Font BrasierLookAndFeel::getLabelFont (juce::Label& label)
    {
        return juce::Font (juce::FontOptions (juce::jmax (11.0f, label.getHeight() * 0.7f)));
    }
}
