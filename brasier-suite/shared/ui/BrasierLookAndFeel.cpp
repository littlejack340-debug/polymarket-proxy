#include "BrasierLookAndFeel.h"

namespace brasier::ui
{
    BrasierLookAndFeel::BrasierLookAndFeel (juce::Colour accentColour)
    {
        setAccent (accentColour);

        setColour (juce::ResizableWindow::backgroundColourId, colours::background);
        setColour (juce::Label::textColourId, colours::text);
        setColour (juce::Slider::textBoxTextColourId, colours::text);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxBackgroundColourId, colours::lcd);
        setColour (juce::Slider::textBoxHighlightColourId, colours::panelEdge);
        setColour (juce::ComboBox::backgroundColourId, colours::lcd);
        setColour (juce::ComboBox::outlineColourId, colours::panelEdge);
        setColour (juce::ComboBox::textColourId, colours::text);
        setColour (juce::PopupMenu::backgroundColourId, colours::panel);
        setColour (juce::PopupMenu::textColourId, colours::text);
        setColour (juce::PopupMenu::headerTextColourId, colours::textDim);
        setColour (juce::ToggleButton::textColourId, colours::text);
        setColour (juce::TextButton::buttonColourId, colours::panel);
        setColour (juce::TextButton::textColourOffId, colours::text);
        setColour (juce::TextEditor::backgroundColourId, colours::lcd);
        setColour (juce::TextEditor::textColourId, colours::text);
        setColour (juce::TextEditor::outlineColourId, colours::panelEdge);
        setColour (juce::AlertWindow::backgroundColourId, colours::faceplate);
        setColour (juce::AlertWindow::textColourId, colours::text);
    }

    void BrasierLookAndFeel::setAccent (juce::Colour c)
    {
        accent = c;
        setColour (juce::Slider::rotarySliderFillColourId, accent);
        setColour (juce::Slider::textBoxTextColourId, accent.brighter (0.4f));
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::ComboBox::arrowColourId, accent);
        setColour (juce::ToggleButton::tickColourId, accent);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, accent.withMultipliedSaturation (0.7f).darker (0.6f));
        setColour (juce::PopupMenu::highlightedTextColourId, colours::text);
    }

    juce::Font BrasierLookAndFeel::monoFont (float height)
    {
        return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), height, juce::Font::plain));
    }

    //==============================================================================
    void BrasierLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                               float pos, float startAngle, float endAngle, juce::Slider& slider)
    {
        const auto area = juce::Rectangle<int> (x, y, width, height).toFloat();
        const float radius = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f - 1.0f;
        const auto centre = area.getCentre();
        const float angle = startAngle + pos * (endAngle - startAngle);
        const bool bipolar = static_cast<bool> (slider.getProperties()[bipolarProperty]);
        const auto accentColour = slider.findColour (juce::Slider::rotarySliderFillColourId);
        const float alpha = slider.isEnabled() ? 1.0f : 0.4f;

        // 1. Graduations
        for (int i = 0; i <= 10; ++i)
        {
            const float a = startAngle + (endAngle - startAngle) * static_cast<float> (i) / 10.0f;
            const bool major = (i % 5) == 0;
            const float r1 = radius * (major ? 0.86f : 0.90f);
            const float r2 = radius * 0.99f;
            const juce::Point<float> p1 (centre.x + r1 * std::sin (a), centre.y - r1 * std::cos (a));
            const juce::Point<float> p2 (centre.x + r2 * std::sin (a), centre.y - r2 * std::cos (a));
            g.setColour (colours::textDim.withAlpha ((major ? 0.85f : 0.45f) * alpha));
            g.drawLine ({ p1, p2 }, major ? 1.6f : 1.0f);
        }

        // 2. Arc de valeur (piste + valeur)
        const float arcRadius = radius * 0.79f;
        const float arcWidth = juce::jmax (2.0f, radius * 0.07f);
        juce::Path track;
        track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
        g.setColour (colours::engraving.withAlpha (0.9f));
        g.strokePath (track, juce::PathStrokeType (arcWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const float fromAngle = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
        if (std::abs (angle - fromAngle) > 0.01f)
        {
            juce::Path value;
            value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                                 juce::jmin (fromAngle, angle), juce::jmax (fromAngle, angle), true);
            g.setColour (accentColour.withAlpha (0.35f * alpha));
            g.strokePath (value, juce::PathStrokeType (arcWidth * 2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (accentColour.withAlpha (alpha));
            g.strokePath (value, juce::PathStrokeType (arcWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // 3. Ombre portee du bouton
        const float body = radius * 0.66f;
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillEllipse (juce::Rectangle<float> (body * 2.1f, body * 2.1f).withCentre (centre.translated (0.0f, body * 0.12f)));

        // 4. Jupe moletee en bakelite
        juce::ColourGradient skirt (colours::knobBody.brighter (0.35f), centre.x - body, centre.y - body,
                                    colours::knobBody.darker (0.7f), centre.x + body, centre.y + body, false);
        g.setGradientFill (skirt);
        g.fillEllipse (juce::Rectangle<float> (body * 2.0f, body * 2.0f).withCentre (centre));

        g.setColour (juce::Colours::black.withAlpha (0.35f));
        for (int i = 0; i < 40; ++i)
        {
            const float a = juce::MathConstants<float>::twoPi * static_cast<float> (i) / 40.0f + angle;
            const juce::Point<float> p1 (centre.x + body * 0.86f * std::sin (a), centre.y - body * 0.86f * std::cos (a));
            const juce::Point<float> p2 (centre.x + body * 0.99f * std::sin (a), centre.y - body * 0.99f * std::cos (a));
            g.drawLine ({ p1, p2 }, 1.0f);
        }

        // 5. Capuchon metal brosse
        const float cap = body * 0.72f;
        juce::ColourGradient metal (colours::metal.brighter (0.2f), centre.x - cap * 0.7f, centre.y - cap,
                                    colours::metal.darker (0.9f), centre.x + cap * 0.6f, centre.y + cap, false);
        metal.addColour (0.5, colours::metal.darker (0.25f));
        g.setGradientFill (metal);
        g.fillEllipse (juce::Rectangle<float> (cap * 2.0f, cap * 2.0f).withCentre (centre));

        g.setColour (juce::Colours::white.withAlpha (0.06f));
        for (int i = 1; i < 5; ++i)
            g.drawEllipse (juce::Rectangle<float> (cap * 2.0f * i / 5.0f, cap * 2.0f * i / 5.0f).withCentre (centre), 0.8f);

        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawEllipse (juce::Rectangle<float> (cap * 2.0f, cap * 2.0f).withCentre (centre), 1.0f);

        // 6. Index de couleur
        const float indexWidth = juce::jmax (2.0f, body * 0.11f);
        juce::Path index;
        index.addRoundedRectangle (-indexWidth * 0.5f, -body * 0.97f, indexWidth, body * 0.55f, indexWidth * 0.5f);
        index.applyTransform (juce::AffineTransform::rotation (angle).translated (centre));
        g.setColour (accentColour.brighter (0.25f).withAlpha (alpha));
        g.fillPath (index);
    }

    juce::Label* BrasierLookAndFeel::createSliderTextBox (juce::Slider& slider)
    {
        auto* label = LookAndFeel_V4::createSliderTextBox (slider);
        label->setFont (monoFont (12.0f));
        label->setJustificationType (juce::Justification::centred);
        label->setColour (juce::Label::backgroundColourId, colours::lcd);
        label->setColour (juce::Label::textColourId, accent.brighter (0.35f));
        label->setColour (juce::Label::outlineColourId, colours::engraving);
        label->setColour (juce::Label::textWhenEditingColourId, colours::text);
        label->setColour (juce::TextEditor::backgroundColourId, colours::lcd);
        label->setColour (juce::TextEditor::textColourId, colours::text);
        label->setColour (juce::TextEditor::highlightColourId, accent.withAlpha (0.4f));
        return label;
    }

    juce::Slider::SliderLayout BrasierLookAndFeel::getSliderLayout (juce::Slider& slider)
    {
        auto layout = LookAndFeel_V4::getSliderLayout (slider);
        // Afficheur legerement plus etroit que le potard, centre
        if (slider.getTextBoxPosition() == juce::Slider::TextBoxBelow)
        {
            const auto bounds = slider.getLocalBounds();
            const int w = juce::jmin (bounds.getWidth(), juce::jmax (56, bounds.getWidth() * 3 / 4));
            layout.textBoxBounds = layout.textBoxBounds.withSizeKeepingCentre (w, layout.textBoxBounds.getHeight());
        }
        return layout;
    }

    //==============================================================================
    void BrasierLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool highlighted, bool)
    {
        auto bounds = button.getLocalBounds().toFloat().reduced (1.5f);
        const bool on = button.getToggleState();
        const float corner = juce::jmin (6.0f, bounds.getHeight() * 0.25f);

        // Plaque encastree
        g.setColour (colours::engraving);
        g.fillRoundedRectangle (bounds, corner);
        juce::ColourGradient face (colours::panel.brighter (on ? 0.12f : 0.05f), 0.0f, bounds.getY(),
                                   colours::panel.darker (0.3f), 0.0f, bounds.getBottom(), false);
        g.setGradientFill (face);
        g.fillRoundedRectangle (bounds.reduced (1.5f), corner);
        g.setColour ((on ? accent : colours::panelEdge).withAlpha (highlighted ? 1.0f : 0.8f));
        g.drawRoundedRectangle (bounds.reduced (1.5f), corner, 1.0f);

        // LED
        const float led = juce::jmin (bounds.getHeight() * 0.36f, 11.0f);
        const auto ledArea = juce::Rectangle<float> (led, led).withCentre ({ bounds.getX() + 8.0f + led * 0.5f, bounds.getCentreY() });
        if (on)
        {
            g.setColour (accent.withAlpha (0.35f));
            g.fillEllipse (ledArea.expanded (led * 0.5f));
        }
        juce::ColourGradient ledFill (on ? accent.brighter (0.6f) : colours::knobBody.brighter (0.1f), ledArea.getCentreX(), ledArea.getY(),
                                      on ? accent.darker (0.2f) : colours::engraving, ledArea.getCentreX(), ledArea.getBottom(), false);
        g.setGradientFill (ledFill);
        g.fillEllipse (ledArea);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawEllipse (ledArea, 0.8f);

        g.setColour (on ? colours::text : colours::textDim);
        g.setFont (juce::FontOptions (juce::jlimit (10.0f, 14.0f, bounds.getHeight() * 0.42f), juce::Font::bold));
        g.drawFittedText (button.getButtonText().toUpperCase(),
                          bounds.withTrimmedLeft (led + 12.0f).withTrimmedRight (4.0f).toNearestInt(),
                          juce::Justification::centred, 1);
    }

    void BrasierLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                                   bool highlighted, bool down)
    {
        auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (colours::engraving);
        g.fillRoundedRectangle (bounds, 4.0f);
        juce::ColourGradient face (colours::panel.brighter (down ? 0.0f : 0.15f), 0.0f, bounds.getY(),
                                   colours::panel.darker (down ? 0.1f : 0.3f), 0.0f, bounds.getBottom(), false);
        g.setGradientFill (face);
        g.fillRoundedRectangle (bounds.reduced (1.0f), 4.0f);
        g.setColour (highlighted ? accent : colours::panelEdge);
        g.drawRoundedRectangle (bounds.reduced (1.0f), 4.0f, 1.0f);
    }

    void BrasierLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button, bool, bool)
    {
        g.setColour (colours::text);
        g.setFont (juce::FontOptions (juce::jlimit (10.0f, 15.0f, button.getHeight() * 0.5f), juce::Font::bold));
        g.drawFittedText (button.getButtonText(), button.getLocalBounds().reduced (3, 0), juce::Justification::centred, 1);
    }

    //==============================================================================
    void BrasierLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                           int, int, int, int, juce::ComboBox& box)
    {
        const auto bounds = juce::Rectangle<float> (0.0f, 0.0f, static_cast<float> (width), static_cast<float> (height)).reduced (1.0f);
        g.setColour (colours::engraving);
        g.fillRoundedRectangle (bounds, 4.0f);
        g.setColour (colours::lcd);
        g.fillRoundedRectangle (bounds.reduced (1.5f), 3.0f);
        g.setColour (box.hasKeyboardFocus (true) ? accent : colours::panelEdge);
        g.drawRoundedRectangle (bounds.reduced (1.0f), 4.0f, 1.0f);

        // Fleche
        const float arrow = juce::jmin (8.0f, height * 0.25f);
        const juce::Point<float> c (bounds.getRight() - arrow - 6.0f, bounds.getCentreY());
        juce::Path p;
        p.addTriangle (c.x - arrow * 0.6f, c.y - arrow * 0.3f, c.x + arrow * 0.6f, c.y - arrow * 0.3f, c.x, c.y + arrow * 0.45f);
        g.setColour (accent);
        g.fillPath (p);
    }

    juce::Font BrasierLookAndFeel::getComboBoxFont (juce::ComboBox& box)
    {
        return monoFont (juce::jlimit (11.0f, 15.0f, box.getHeight() * 0.48f));
    }

    void BrasierLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
    {
        label.setBounds (6, 1, box.getWidth() - 26, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
        label.setColour (juce::Label::textColourId, accent.brighter (0.35f));
    }

    juce::Font BrasierLookAndFeel::getLabelFont (juce::Label& label)
    {
        if (dynamic_cast<juce::Slider*> (label.getParentComponent()) != nullptr)
            return monoFont (juce::jlimit (10.0f, 13.0f, label.getHeight() * 0.68f));

        return juce::Font (juce::FontOptions (juce::jlimit (10.0f, 16.0f, label.getHeight() * 0.75f), juce::Font::bold));
    }

    void BrasierLookAndFeel::drawLabel (juce::Graphics& g, juce::Label& label)
    {
        // Afficheur de valeur d'un potard : petit ecran LCD encastre
        if (dynamic_cast<juce::Slider*> (label.getParentComponent()) != nullptr && ! label.isBeingEdited())
        {
            const auto r = label.getLocalBounds().toFloat().reduced (0.5f);
            g.setColour (colours::engraving);
            g.fillRoundedRectangle (r, 3.0f);
            g.setColour (colours::lcd);
            g.fillRoundedRectangle (r.reduced (1.0f), 2.5f);
            g.setColour (juce::Colours::white.withAlpha (0.06f));
            g.drawLine (r.getX() + 3.0f, r.getBottom() - 0.5f, r.getRight() - 3.0f, r.getBottom() - 0.5f, 1.0f);

            g.setColour (accent.brighter (0.4f).withAlpha (label.isEnabled() ? 1.0f : 0.4f));
            g.setFont (getLabelFont (label));
            g.drawFittedText (label.getText(), label.getLocalBounds().reduced (3, 0), juce::Justification::centred, 1, 1.0f);
            return;
        }

        LookAndFeel_V4::drawLabel (g, label);
    }

    juce::Font BrasierLookAndFeel::getPopupMenuFont()
    {
        return juce::Font (juce::FontOptions (14.0f));
    }

    //==============================================================================
    void BrasierLookAndFeel::drawEngravedText (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area,
                                               juce::Font font, juce::Justification justification, juce::Colour colour)
    {
        g.setFont (font);
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.drawText (text, area.translated (0.0f, -1.0f), justification, false);
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawText (text, area.translated (0.0f, 1.0f), justification, false);
        g.setColour (colour);
        g.drawText (text, area, justification, false);
    }

    void BrasierLookAndFeel::drawScrew (juce::Graphics& g, juce::Point<float> centre, float radius, float angle)
    {
        juce::ColourGradient head (colours::metal.brighter (0.1f), centre.x - radius, centre.y - radius,
                                   colours::metal.darker (1.2f), centre.x + radius, centre.y + radius, false);
        g.setGradientFill (head);
        g.fillEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre));
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.drawEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre), 0.8f);

        for (float a : { angle, angle + juce::MathConstants<float>::halfPi })
        {
            const juce::Point<float> d (radius * 0.65f * std::cos (a), radius * 0.65f * std::sin (a));
            g.drawLine ({ centre - d, centre + d }, juce::jmax (1.0f, radius * 0.22f));
        }
    }
}
