#pragma once

#include <memory>

#include <juce_gui_basics/juce_gui_basics.h>

#include "core/PresetManager.h"

namespace brasier::ui
{
    /** Barre de presets : < [liste] > et "Sauver" (presets utilisateur). */
    class PresetBar : public juce::Component
    {
    public:
        explicit PresetBar (core::PresetManager& manager);
        ~PresetBar() override;

        void refresh();
        void resized() override;

    private:
        void askNameAndSave();

        core::PresetManager& presets;
        juce::ComboBox list;
        juce::TextButton previous { "<" }, next { ">" }, save { "Sauver" };
        std::unique_ptr<juce::AlertWindow> saveDialog;
    };
}
