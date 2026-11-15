#pragma once

#include <array>

#include <juce_core/juce_core.h>

namespace brasier::dsp
{
    /** Divisions de mesure pour les delays et LFO synchronises au tempo de l'hote.
        T = triolet (2/3), D = pointe (3/2). */
    namespace tempo
    {
        struct Division
        {
            const char* name;
            double quarterNotes;   // duree en noires
        };

        inline constexpr std::array<Division, 19> divisions {{
            { "1/64",  1.0 / 16.0 },
            { "1/32T", 1.0 / 12.0 },
            { "1/32",  1.0 / 8.0 },
            { "1/16T", 1.0 / 6.0 },
            { "1/16",  1.0 / 4.0 },
            { "1/16D", 3.0 / 8.0 },
            { "1/8T",  1.0 / 3.0 },
            { "1/8",   1.0 / 2.0 },
            { "1/8D",  3.0 / 4.0 },
            { "1/4T",  2.0 / 3.0 },
            { "1/4",   1.0 },
            { "1/4D",  3.0 / 2.0 },
            { "1/2T",  4.0 / 3.0 },
            { "1/2",   2.0 },
            { "1/2D",  3.0 },
            { "1/1",   4.0 },
            { "2/1",   8.0 },
            { "4/1",   16.0 },
            { "8/1",   32.0 },
        }};

        inline constexpr int quarterIndex = 10;   // "1/4"

        inline juce::StringArray divisionNames (int count = static_cast<int> (divisions.size()))
        {
            juce::StringArray names;
            for (int i = 0; i < juce::jmin (count, static_cast<int> (divisions.size())); ++i)
                names.add (divisions[static_cast<size_t> (i)].name);
            return names;
        }

        /** Duree d'une division en secondes. */
        inline double divisionSeconds (int index, double bpm) noexcept
        {
            const auto& d = divisions[static_cast<size_t> (juce::jlimit (0, static_cast<int> (divisions.size()) - 1, index))];
            return d.quarterNotes * 60.0 / juce::jmax (1.0, bpm);
        }

        /** Frequence (Hz) d'un LFO dont une periode dure une division. */
        inline double divisionHz (int index, double bpm) noexcept
        {
            return 1.0 / divisionSeconds (index, bpm);
        }
    }
}
