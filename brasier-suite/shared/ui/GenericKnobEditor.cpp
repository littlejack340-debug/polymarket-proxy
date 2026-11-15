#include "GenericKnobEditor.h"

namespace brasier::ui
{
    GenericKnobEditor::GenericKnobEditor (juce::AudioProcessor& owner,
                                          juce::AudioProcessorValueTreeState& s,
                                          Options o)
        : juce::AudioProcessorEditor (owner), state (s), options (std::move (o))
    {
        setLookAndFeel (&lookAndFeel);

        // Parametres du plugin d'abord, puis ceux du bandeau (communs) dans l'ordre demande
        for (auto* p : owner.getParameters())
        {
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
            {
                const auto& id = ranged->getParameterID();

                if (options.hiddenParameterIds.contains (id) || options.footerParameterIds.contains (id))
                    continue;

                addControlFor (*ranged, false);
            }
        }

        for (const auto& id : options.footerParameterIds)
            if (! options.hiddenParameterIds.contains (id))
                if (auto* ranged = state.getParameter (id))
                    addControlFor (*ranged, true);

        setResizable (true, true);
        setResizeLimits (baseWidth * 3 / 4, baseHeight * 3 / 4, baseWidth * 5 / 2, baseHeight * 5 / 2);
        if (auto* sizeConstrainer = getConstrainer())
            sizeConstrainer->setFixedAspectRatio (static_cast<double> (baseWidth) / baseHeight);

        setSize (baseWidth, baseHeight);
    }

    GenericKnobEditor::~GenericKnobEditor()
    {
        setLookAndFeel (nullptr);
    }

    void GenericKnobEditor::addControlFor (juce::RangedAudioParameter& parameter, bool isFooter)
    {
        auto control = std::make_unique<Control>();
        control->parameterId = parameter.getParameterID();
        control->isFooter = isFooter;

        control->label = std::make_unique<juce::Label> (juce::String(), parameter.getName (32).toUpperCase());
        control->label->setJustificationType (juce::Justification::centred);
        control->label->setColour (juce::Label::textColourId, colours::textDim);
        addAndMakeVisible (*control->label);

        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (&parameter))
        {
            auto combo = std::make_unique<juce::ComboBox>();
            combo->addItemList (choice->choices, 1);
            control->comboAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
                state, control->parameterId, *combo);
            control->component = std::move (combo);
        }
        else if (dynamic_cast<juce::AudioParameterBool*> (&parameter) != nullptr)
        {
            auto button = std::make_unique<juce::ToggleButton> (parameter.getName (16));
            control->buttonAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
                state, control->parameterId, *button);
            control->label->setText ({}, juce::dontSendNotification);   // le texte est sur le bouton
            control->component = std::move (button);
        }
        else
        {
            auto slider = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag,
                                                          juce::Slider::TextBoxBelow);
            slider->setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
            slider->setDoubleClickReturnValue (true, static_cast<double> (
                parameter.convertFrom0to1 (parameter.getDefaultValue())));
            slider->setPopupDisplayEnabled (false, false, nullptr);
            control->sliderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                state, control->parameterId, *slider);
            control->component = std::move (slider);
        }

        addAndMakeVisible (*control->component);
        controls.push_back (std::move (control));
    }

    void GenericKnobEditor::paint (juce::Graphics& g)
    {
        const float scale = static_cast<float> (getWidth()) / baseWidth;
        auto bounds = getLocalBounds().toFloat();

        g.fillAll (colours::background);

        // En-tete
        auto header = bounds.removeFromTop (48.0f * scale);
        g.setColour (colours::panel);
        g.fillRect (header);
        g.setColour (colours::accent);
        g.fillRect (header.removeFromBottom (2.0f * scale));

        g.setColour (colours::text);
        g.setFont (juce::FontOptions (22.0f * scale, juce::Font::bold));
        g.drawText (options.title, header.reduced (16.0f * scale, 0.0f), juce::Justification::centredLeft);

        g.setColour (colours::textDim);
        g.setFont (juce::FontOptions (12.0f * scale));
        g.drawText (options.subtitle, header.reduced (16.0f * scale, 0.0f), juce::Justification::centredRight);

        // Bandeau du bas
        auto footer = bounds.removeFromBottom (110.0f * scale);
        g.setColour (colours::panel);
        g.fillRect (footer);
        g.setColour (colours::panelEdge);
        g.fillRect (footer.removeFromTop (1.0f));
    }

    void GenericKnobEditor::layoutRow (std::vector<Control*>& row, juce::Rectangle<int> area)
    {
        if (row.empty())
            return;

        const int cellWidth = area.getWidth() / static_cast<int> (row.size());
        const int labelHeight = juce::jmax (14, area.getHeight() / 7);

        for (auto* c : row)
        {
            auto cell = area.removeFromLeft (cellWidth).reduced (6, 4);
            auto labelArea = cell.removeFromTop (labelHeight);

            if (auto* slider = dynamic_cast<juce::Slider*> (c->component.get()))
            {
                c->label->setBounds (labelArea);
                slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false,
                                         juce::jmin (cell.getWidth(), 90), juce::jmax (16, labelHeight + 2));
                const int side = juce::jmin (cell.getWidth(), cell.getHeight());
                slider->setBounds (cell.withSizeKeepingCentre (side, cell.getHeight()));
            }
            else
            {
                c->label->setBounds (labelArea);
                const int h = juce::jmin (cell.getHeight(), juce::jmax (24, labelHeight * 2));
                c->component->setBounds (cell.withSizeKeepingCentre (juce::jmin (cell.getWidth(), 120), h));
            }
        }
    }

    void GenericKnobEditor::resized()
    {
        const float scale = static_cast<float> (getWidth()) / baseWidth;
        auto bounds = getLocalBounds();

        bounds.removeFromTop (juce::roundToInt (48.0f * scale));
        auto footer = bounds.removeFromBottom (juce::roundToInt (110.0f * scale)).reduced (juce::roundToInt (8.0f * scale));
        auto main = bounds.reduced (juce::roundToInt (12.0f * scale));

        std::vector<Control*> mainControls, footerControls;
        for (auto& c : controls)
            (c->isFooter ? footerControls : mainControls).push_back (c.get());

        // Zone principale : rangees d'au plus 5 controles
        constexpr size_t perRow = 5;
        const auto numRows = juce::jmax<size_t> (1, (mainControls.size() + perRow - 1) / perRow);
        const int rowHeight = main.getHeight() / static_cast<int> (numRows);

        for (size_t r = 0; r < numRows; ++r)
        {
            std::vector<Control*> row;
            for (size_t i = r * perRow; i < juce::jmin (mainControls.size(), (r + 1) * perRow); ++i)
                row.push_back (mainControls[i]);

            layoutRow (row, main.removeFromTop (rowHeight));
        }

        layoutRow (footerControls, footer);
    }
}
