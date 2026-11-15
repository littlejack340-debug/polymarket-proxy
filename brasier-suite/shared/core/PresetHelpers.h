#pragma once

#include "PresetManager.h"

namespace brasier::core
{
    /** Texte UTF-8 (accents) pour les noms de presets. */
    inline juce::String u8 (const char* text) { return juce::String::fromUTF8 (text); }

    /** Categories communes des presets d'usine, orientees production rap. */
    namespace categories
    {
        inline const juce::String lead    = "Voix lead";
        inline const juce::String adlibs  = "Ad-libs";
        inline const juce::String bass808 = "808";
        inline const juce::String samples = "Samples";
        inline const juce::String effects = "Effets";
        inline const juce::String mix     = "Mix";
    }
}
