#include "PresetBar.h"

namespace brasier::ui
{
    PresetBar::PresetBar (core::PresetManager& manager) : presets (manager)
    {
        addAndMakeVisible (list);
        addAndMakeVisible (previous);
        addAndMakeVisible (next);
        addAndMakeVisible (save);

        list.setTextWhenNothingSelected (juce::String::fromUTF8 ("Réglage libre"));
        list.onChange = [this]
        {
            const int index = list.getSelectedId() - 1;
            if (index >= 0 && index != presets.getCurrentIndex())
                presets.loadPreset (index);
        };

        previous.onClick = [this] { presets.loadPrevious(); refresh(); };
        next.onClick = [this] { presets.loadNext(); refresh(); };
        save.onClick = [this] { askNameAndSave(); };

        previous.setTooltip ("Preset precedent");
        next.setTooltip ("Preset suivant");
        save.setTooltip ("Sauver le reglage actuel comme preset utilisateur");

        refresh();
    }

    PresetBar::~PresetBar() = default;

    void PresetBar::refresh()
    {
        presets.refreshUserPresets();
        list.clear (juce::dontSendNotification);

        const auto names = presets.getPresetNames();
        juce::String lastCategory;

        for (int i = 0; i < names.size(); ++i)
        {
            // En-tetes de categorie dans le menu deroulant
            const auto category = presets.isFactory (i) ? presets.getFactoryPreset (i).category : juce::String ("Perso");
            if (category != lastCategory)
            {
                list.addSectionHeading (category.isNotEmpty() ? category : juce::String ("Usine"));
                lastCategory = category;
            }

            const auto display = presets.isFactory (i) ? presets.getFactoryPreset (i).name : names[i].fromFirstOccurrenceOf (" - ", false, false);
            list.addItem (display, i + 1);
        }

        if (presets.getCurrentIndex() >= 0)
            list.setSelectedId (presets.getCurrentIndex() + 1, juce::dontSendNotification);
    }

    void PresetBar::resized()
    {
        auto r = getLocalBounds();
        const int h = r.getHeight();
        save.setBounds (r.removeFromRight (h * 2 + 12));
        r.removeFromRight (6);
        next.setBounds (r.removeFromRight (h));
        r.removeFromRight (4);
        previous.setBounds (r.removeFromLeft (h));
        r.removeFromLeft (4);
        list.setBounds (r);
    }

    void PresetBar::askNameAndSave()
    {
        saveDialog = std::make_unique<juce::AlertWindow> ("Sauver le preset", "Nom du preset :", juce::MessageBoxIconType::NoIcon, this);
        saveDialog->addTextEditor ("name", presets.getCurrentIndex() >= 0 ? presets.getCurrentName().fromLastOccurrenceOf (" - ", false, false) : "Mon preset");
        saveDialog->addButton ("Sauver", 1, juce::KeyPress (juce::KeyPress::returnKey));
        saveDialog->addButton ("Annuler", 0, juce::KeyPress (juce::KeyPress::escapeKey));

        saveDialog->enterModalState (true, juce::ModalCallbackFunction::create ([safeThis = juce::Component::SafePointer<PresetBar> (this)] (int result)
        {
            if (safeThis == nullptr || safeThis->saveDialog == nullptr)
                return;

            if (result == 1)
            {
                const auto name = safeThis->saveDialog->getTextEditorContents ("name");
                safeThis->presets.saveUserPreset (name);
                safeThis->refresh();
            }

            safeThis->saveDialog.reset();
        }), false);
    }
}
