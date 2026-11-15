#include "BrasierEditor.h"

namespace brasier::ui
{
    namespace
    {
        constexpr int headerHeight = 78;
        constexpr int footerHeight = 150;
        constexpr int margin = 14;
        constexpr int sectionGap = 10;
        constexpr int sectionTitleHeight = 22;
        constexpr int captionHeight = 16;
        constexpr int textBoxHeight = 18;

        juce::Font titleFont (float height, float tracking = 0.0f, bool bold = true)
        {
            return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain).withKerningFactor (tracking));
        }
    }

    //==============================================================================
    BrasierEditor::Content::Content (BrasierEditor& owner) : editor (owner)
    {
        setOpaque (true);
    }

    void BrasierEditor::Content::resized()
    {
        cache = {};
    }

    void BrasierEditor::Content::paint (juce::Graphics& g)
    {
        // Facade rendue une fois a la resolution physique, puis reutilisee
        const float scale = juce::jmax (0.5f, g.getInternalContext().getPhysicalPixelScaleFactor());
        if (cache.isNull() || std::abs (scale - cacheScale) > 0.01f)
        {
            cache = juce::Image (juce::Image::ARGB,
                                 juce::roundToInt (getWidth() * scale), juce::roundToInt (getHeight() * scale), true);
            juce::Graphics cg (cache);
            cg.addTransform (juce::AffineTransform::scale (scale));
            renderBackground (cg);
            cacheScale = scale;
        }

        g.drawImageTransformed (cache, juce::AffineTransform::scale (1.0f / cacheScale));
    }

    void BrasierEditor::Content::renderBackground (juce::Graphics& g)
    {
        const auto& e = editor;
        const auto accent = e.spec.accent;
        const auto full = getLocalBounds().toFloat();

        // 1. Facade : degrade + texture brossee
        juce::ColourGradient plate (colours::faceplateHi, 0.0f, 0.0f, colours::faceplate.darker (0.25f), 0.0f, full.getBottom(), false);
        g.setGradientFill (plate);
        g.fillAll();

        juce::Random random (0xB4A51E4);
        for (int y = 0; y < getHeight(); y += 2)
        {
            g.setColour (juce::Colours::white.withAlpha (0.008f + 0.022f * random.nextFloat()));
            g.fillRect (0.0f, static_cast<float> (y), full.getWidth(), 1.0f);
        }

        // 2. En-tete : braise, marque, nom, slogan
        const auto header = e.headerBounds.toFloat();
        const juce::Point<float> ember (header.getX() + 30.0f, header.getCentreY() + 2.0f);
        for (int i = 4; i > 0; --i)
        {
            g.setColour (accent.withAlpha (0.07f * static_cast<float> (i)));
            g.fillEllipse (juce::Rectangle<float> (14.0f + 7.0f * static_cast<float> (i), 14.0f + 7.0f * static_cast<float> (i)).withCentre (ember));
        }
        juce::ColourGradient emberFill (accent.brighter (0.8f), ember.x, ember.y - 8.0f, accent.darker (0.4f), ember.x, ember.y + 8.0f, false);
        g.setGradientFill (emberFill);
        g.fillEllipse (juce::Rectangle<float> (16.0f, 16.0f).withCentre (ember));

        BrasierLookAndFeel::drawEngravedText (g, "BRASIER AUDIO", { header.getX() + 56.0f, header.getY() + 12.0f, 300.0f, 16.0f },
                                              titleFont (11.0f, 0.35f), juce::Justification::centredLeft, colours::textDim);
        BrasierLookAndFeel::drawEngravedText (g, e.spec.pluginName, { header.getX() + 54.0f, header.getY() + 26.0f, 420.0f, 40.0f },
                                              titleFont (34.0f, 0.12f), juce::Justification::centredLeft, colours::text);

        const auto nameWidth = juce::GlyphArrangement::getStringWidth (titleFont (34.0f, 0.12f), e.spec.pluginName);
        BrasierLookAndFeel::drawEngravedText (g, e.spec.tagline, { header.getX() + 66.0f + nameWidth, header.getY() + 38.0f, 260.0f, 22.0f },
                                              titleFont (13.0f, 0.05f, false), juce::Justification::centredLeft, accent.withAlpha (0.9f));

        // Filet d'accent sous l'en-tete
        const float lineY = header.getBottom() - 1.0f;
        g.setColour (accent.withAlpha (0.15f));
        g.fillRect (0.0f, lineY - 2.0f, full.getWidth(), 5.0f);
        g.setColour (accent);
        g.fillRect (0.0f, lineY, full.getWidth(), 1.5f);

        // 3. Sections
        auto drawPanel = [&g, accent] (juce::Rectangle<float> r, const juce::String& title)
        {
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.fillRoundedRectangle (r.translated (0.0f, 2.0f), 6.0f);
            juce::ColourGradient fill (colours::panel.brighter (0.06f), 0.0f, r.getY(), colours::panel.darker (0.25f), 0.0f, r.getBottom(), false);
            g.setGradientFill (fill);
            g.fillRoundedRectangle (r, 6.0f);
            g.setColour (colours::engraving);
            g.drawRoundedRectangle (r, 6.0f, 1.5f);
            g.setColour (juce::Colours::white.withAlpha (0.05f));
            g.drawRoundedRectangle (r.reduced (1.5f), 5.0f, 1.0f);

            if (title.isNotEmpty())
            {
                const auto font = titleFont (11.0f, 0.22f);
                const auto text = title.toUpperCase();
                const auto titleArea = juce::Rectangle<float> (r.getX() + 12.0f, r.getY() + 5.0f, r.getWidth() - 24.0f, 14.0f);
                BrasierLookAndFeel::drawEngravedText (g, text, titleArea, font, juce::Justification::centredLeft, accent.brighter (0.15f));
                const float textWidth = juce::GlyphArrangement::getStringWidth (font, text);
                g.setColour (colours::panelEdge.withAlpha (0.7f));
                g.fillRect (titleArea.getX() + textWidth + 8.0f, titleArea.getCentreY(), titleArea.getWidth() - textWidth - 8.0f, 1.0f);
            }
        };

        for (size_t r = 0; r < e.spec.rows.size(); ++r)
            for (size_t s = 0; s < e.spec.rows[r].size(); ++s)
                drawPanel (e.sectionBounds[r][s].toFloat(), e.spec.rows[r][s].title);

        // 4. Pied : niveaux + sortie
        const auto footer = e.footerBounds.toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.fillRect (footer);
        g.setColour (colours::engraving);
        g.fillRect (footer.getX(), footer.getY(), footer.getWidth(), 1.5f);
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.fillRect (footer.getX(), footer.getY() + 1.5f, footer.getWidth(), 1.0f);

        drawPanel (e.metersBounds.toFloat(), "Niveaux");
        drawPanel (e.commonBounds.toFloat(), "Sortie");

        // 5. Vis de facade et signature
        const float screwAngles[] = { 0.3f, 1.1f, 2.0f, 0.7f };
        const juce::Point<float> screws[] = { { 12.0f, 12.0f }, { full.getRight() - 12.0f, 12.0f },
                                              { 12.0f, full.getBottom() - 12.0f }, { full.getRight() - 12.0f, full.getBottom() - 12.0f } };
        for (int i = 0; i < 4; ++i)
            BrasierLookAndFeel::drawScrew (g, screws[i], 5.5f, screwAngles[i]);

        BrasierLookAndFeel::drawEngravedText (g, "BRASIER AUDIO  -  " + e.spec.pluginName + "  -  V" + juce::String (BRASIER_VERSION_STRING),
                                              { full.getRight() - 330.0f, full.getBottom() - 18.0f, 300.0f, 12.0f },
                                              titleFont (9.0f, 0.2f, false), juce::Justification::centredRight, colours::textDim.withAlpha (0.6f));
    }

    //==============================================================================
    BrasierEditor::BrasierEditor (core::EffectProcessorBase& p, EditorSpec s)
        : juce::AudioProcessorEditor (p),
          processorRef (p),
          spec (std::move (s)),
          lookAndFeel (spec.accent),
          presetBar (p.getPresetManager())
    {
        setLookAndFeel (&lookAndFeel);
        addAndMakeVisible (content);
        content.addAndMakeVisible (presetBar);
        content.addAndMakeVisible (inputMeter);
        content.addAndMakeVisible (outputMeter);
        inputMeter.setAccent (spec.accent);
        outputMeter.setAccent (spec.accent);

        // Controles du corps
        sectionControls.resize (spec.rows.size());
        for (size_t r = 0; r < spec.rows.size(); ++r)
        {
            sectionControls[r].resize (spec.rows[r].size());
            for (size_t sec = 0; sec < spec.rows[r].size(); ++sec)
                for (const auto& c : spec.rows[r][sec].controls)
                    if (auto* control = createControl (c))
                        sectionControls[r][sec].push_back (control);
        }

        // Controles communs du pied
        using Style = ControlSpec::Style;
        for (const auto& c : { ControlSpec { core::ids::mix, Style::smallKnob },
                               ControlSpec { core::ids::output, Style::smallKnob, true },
                               ControlSpec { core::ids::autoGain, Style::toggle, false, "Auto Gain" },
                               ControlSpec { core::ids::oversampling, Style::selector, false, "Oversampling" },
                               ControlSpec { core::ids::bypass, Style::toggle } })
            if (auto* control = createControl (c))
                footerControls.push_back (control);

        layoutContent();
        updateEnablement();

        setResizable (true, true);
        setResizeLimits (baseWidth * 7 / 10, baseHeight * 7 / 10, baseWidth * 2, baseHeight * 2);
        if (auto* sizeConstrainer = getConstrainer())
            sizeConstrainer->setFixedAspectRatio (static_cast<double> (baseWidth) / baseHeight);

        const float scale = juce::jlimit (0.7f, 2.0f, processorRef.getEditorScale());
        setSize (juce::roundToInt (baseWidth * scale), juce::roundToInt (baseHeight * scale));

        startTimerHz (30);
    }

    BrasierEditor::~BrasierEditor()
    {
        stopTimer();
        setLookAndFeel (nullptr);
    }

    BrasierEditor::Control* BrasierEditor::createControl (const ControlSpec& cs)
    {
        auto& state = processorRef.getValueTreeState();
        auto* parameter = state.getParameter (cs.parameterId);
        if (parameter == nullptr)
            return nullptr;

        auto control = std::make_unique<Control>();
        control->spec = cs;
        control->name = cs.label.isNotEmpty() ? cs.label : parameter->getName (32);

        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (parameter))
        {
            auto combo = std::make_unique<juce::ComboBox>();
            combo->addItemList (choice->choices, 1);
            combo->setTooltip (control->name);
            control->comboAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, cs.parameterId, *combo);
            control->component = std::move (combo);
        }
        else if (dynamic_cast<juce::AudioParameterBool*> (parameter) != nullptr)
        {
            auto button = std::make_unique<juce::ToggleButton> (control->name);
            button->setTooltip (control->name);
            control->buttonAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, cs.parameterId, *button);
            control->component = std::move (button);
        }
        else
        {
            auto slider = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
            slider->setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
            slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, textBoxHeight);
            slider->getProperties().set (bipolarProperty, cs.bipolar);
            slider->setDoubleClickReturnValue (true, static_cast<double> (parameter->convertFrom0to1 (parameter->getDefaultValue())));
            slider->setVelocityModeParameters (0.35, 1, 0.0, true, juce::ModifierKeys::ctrlModifier);   // Ctrl = reglage fin
            slider->setTooltip (control->name + "  (double-clic : valeur par defaut, Ctrl : reglage fin)");
            control->sliderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, cs.parameterId, *slider);
            control->component = std::move (slider);
        }

        if (cs.style != ControlSpec::Style::toggle)
        {
            control->caption = std::make_unique<juce::Label> (juce::String(), control->name.toUpperCase());
            control->caption->setJustificationType (juce::Justification::centred);
            control->caption->setColour (juce::Label::textColourId, colours::textDim);
            control->caption->setInterceptsMouseClicks (false, false);
            content.addAndMakeVisible (*control->caption);
        }

        content.addAndMakeVisible (*control->component);
        controls.push_back (std::move (control));
        return controls.back().get();
    }

    void BrasierEditor::layoutControlsIn (std::vector<Control*>& list, juce::Rectangle<int> area, int rows)
    {
        if (list.empty())
            return;

        rows = juce::jlimit (1, static_cast<int> (list.size()), rows);
        const int perRow = (static_cast<int> (list.size()) + rows - 1) / rows;
        const int rowHeight = area.getHeight() / rows;
        const int cellWidth = area.getWidth() / perRow;

        for (int row = 0; row < rows; ++row)
        {
            const int first = row * perRow;
            const int count = juce::jmin (perRow, static_cast<int> (list.size()) - first);
            if (count <= 0)
                break;

            auto rowArea = area.withTrimmedTop (row * rowHeight).withHeight (rowHeight);
            rowArea = rowArea.withSizeKeepingCentre (cellWidth * count, rowArea.getHeight());

            for (int i = 0; i < count; ++i)
            {
                auto* c = list[static_cast<size_t> (first + i)];
                auto cell = rowArea.removeFromLeft (cellWidth).reduced (4, 2);
                using Style = ControlSpec::Style;

                if (c->spec.style == Style::toggle)
                {
                    c->component->setBounds (cell.withSizeKeepingCentre (juce::jmin (cell.getWidth() - 8, 130), 30));
                    continue;
                }

                if (c->caption != nullptr)
                    c->caption->setBounds (cell.removeFromTop (captionHeight));

                if (c->spec.style == Style::selector)
                {
                    c->component->setBounds (cell.withSizeKeepingCentre (juce::jmin (cell.getWidth() - 8, 150), 28));
                    continue;
                }

                const int maxKnob = c->spec.style == Style::smallKnob ? 62 : (c->spec.style == Style::largeKnob ? 132 : 92);
                const int knob = juce::jlimit (36, maxKnob, juce::jmin (cell.getWidth() - 8, cell.getHeight() - textBoxHeight - 6));
                const int width = juce::jmax (knob, 76);
                c->component->setBounds (cell.withSizeKeepingCentre (width, knob + textBoxHeight + 4));
            }
        }
    }

    void BrasierEditor::layoutContent()
    {
        const auto all = juce::Rectangle<int> (0, 0, baseWidth, baseHeight);
        headerBounds = all.withHeight (headerHeight);
        footerBounds = all.withTrimmedTop (baseHeight - footerHeight);
        bodyBounds = all.withTrimmedTop (headerHeight).withTrimmedBottom (footerHeight).reduced (margin, sectionGap);

        presetBar.setBounds (baseWidth - margin - 16 - 440, 24, 440, 30);

        // Corps : rangees de sections
        sectionBounds.assign (spec.rows.size(), {});
        const int numRows = juce::jmax (1, static_cast<int> (spec.rows.size()));
        const int rowHeight = (bodyBounds.getHeight() - sectionGap * (numRows - 1)) / numRows;

        for (size_t r = 0; r < spec.rows.size(); ++r)
        {
            auto rowArea = bodyBounds.withTrimmedTop (static_cast<int> (r) * (rowHeight + sectionGap)).withHeight (rowHeight);
            float totalWeight = 0.0f;
            for (const auto& s : spec.rows[r])
                totalWeight += s.width;

            const int available = rowArea.getWidth() - sectionGap * (static_cast<int> (spec.rows[r].size()) - 1);
            for (size_t s = 0; s < spec.rows[r].size(); ++s)
            {
                const bool last = s + 1 == spec.rows[r].size();
                const int w = last ? rowArea.getWidth() : juce::roundToInt (available * spec.rows[r][s].width / totalWeight);
                const auto bounds = rowArea.removeFromLeft (w);
                rowArea.removeFromLeft (sectionGap);
                sectionBounds[r].push_back (bounds);
                layoutControlsIn (sectionControls[r][s], bounds.withTrimmedTop (sectionTitleHeight).reduced (8, 6), spec.rows[r][s].rows);
            }
        }

        // Pied : VU-metres puis controles communs
        auto footer = footerBounds.reduced (margin, 12);
        metersBounds = footer.removeFromLeft (360);
        footer.removeFromLeft (sectionGap);
        commonBounds = footer;

        auto meters = metersBounds.withTrimmedTop (sectionTitleHeight).reduced (10, 4);
        const int meterWidth = (meters.getWidth() - 10) / 2;
        inputMeter.setBounds (meters.removeFromLeft (meterWidth));
        meters.removeFromLeft (10);
        outputMeter.setBounds (meters.removeFromLeft (meterWidth));

        layoutControlsIn (footerControls, commonBounds.withTrimmedTop (sectionTitleHeight).reduced (8, 0), 1);
    }

    void BrasierEditor::paint (juce::Graphics& g)
    {
        g.fillAll (colours::background);
    }

    void BrasierEditor::resized()
    {
        const float scale = static_cast<float> (getWidth()) / static_cast<float> (baseWidth);
        content.setTransform (juce::AffineTransform::scale (scale));
        content.setBounds (0, 0, baseWidth, baseHeight);
        processorRef.setEditorScale (scale);
    }

    void BrasierEditor::updateEnablement()
    {
        auto& state = processorRef.getValueTreeState();
        for (auto& c : controls)
        {
            if (c->spec.enabledBy.isEmpty())
                continue;
            if (auto* value = state.getRawParameterValue (c->spec.enabledBy))
            {
                const bool enabled = juce::roundToInt (value->load()) == c->spec.enabledValue;
                if (c->component->isEnabled() != enabled)
                {
                    c->component->setEnabled (enabled);
                    c->component->setAlpha (enabled ? 1.0f : 0.45f);
                    if (c->caption != nullptr)
                        c->caption->setAlpha (enabled ? 1.0f : 0.45f);
                }
            }
        }
    }

    void BrasierEditor::timerCallback()
    {
        updateEnablement();

        constexpr float dt = 1.0f / 30.0f;
        const auto levels = processorRef.getMeterLevels();
        inputMeter.update (0.5f * (levels.inputMeanSquare[0] + levels.inputMeanSquare[1]), 0.0f, dt);
        outputMeter.update (0.5f * (levels.outputMeanSquare[0] + levels.outputMeanSquare[1]), levels.outputPeak, dt);

        // Preset change depuis l'hote (menu des programmes de FL Studio)
        const int index = processorRef.getPresetManager().getCurrentIndex();
        if (index != lastPresetIndex)
        {
            lastPresetIndex = index;
            presetBar.refresh();
        }
    }
}
