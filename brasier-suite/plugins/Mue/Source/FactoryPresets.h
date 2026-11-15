#pragma once

// Presets d'usine de Brasier Mue (valeurs en unites reelles).
// Notes robot : 0 = C2, 7 = G2, 12 = C3, 17 = F3, 24 = C4.

#include "core/PresetHelpers.h"
#include "MueProcessor.h"

namespace brasier::mue
{
    inline std::vector<core::FactoryPreset> makeFactoryPresets()
    {
        using namespace core;
        namespace c = core::ids;
        constexpr float live = 0, quality = 1;

        return {
            { "Init", {}, {} },

            // Ad-libs
            { u8 ("Ad-lib aigu"), categories::adlibs,     { { ids::pitch, 7 }, { ids::formant, 2 } } },
            { u8 ("Ad-lib démon"), categories::adlibs,    { { ids::pitch, -12 }, { ids::formant, -4 } } },
            { u8 ("Doublure octave grave"), categories::adlibs, { { ids::pitch, -12 }, { c::mix, 35 } } },
            { u8 ("Doublure octave aiguë"), categories::adlibs, { { ids::pitch, 12 }, { ids::formant, -1 }, { c::mix, 30 } } },
            { u8 ("Voix d'enfant"), categories::adlibs,   { { ids::pitch, 5 }, { ids::formant, 6 } } },

            // Voix lead
            { u8 ("Épaisseur"), categories::lead,         { { ids::pitch, 0.12f }, { ids::formant, -0.3f }, { c::mix, 30 } } },
            { u8 ("Formant plus clair"), categories::lead, { { ids::formant, 2.5f } } },
            { u8 ("Formant plus grave"), categories::lead, { { ids::formant, -2.5f } } },
            { u8 ("Harmonie tierce"), categories::lead,   { { ids::pitch, 4 }, { c::mix, 35 } } },
            { u8 ("Harmonie quinte"), categories::lead,   { { ids::pitch, 7 }, { c::mix, 35 } } },

            // Effets
            { u8 ("Robot C3"), categories::effects,       { { ids::robot, 1 }, { ids::robotNote, 12 } } },
            { u8 ("Robot grave"), categories::effects,    { { ids::robot, 1 }, { ids::robotNote, 7 }, { ids::formant, -2 } } },
            { u8 ("Robot aigu"), categories::effects,     { { ids::robot, 1 }, { ids::robotNote, 24 }, { ids::formant, 2 } } },

            // Samples (mode Qualite : voix et instruments graves)
            { u8 ("Sample accéléré (soul)"), categories::samples, { { ids::pitch, 5 }, { ids::formant, 5 }, { ids::latency, quality } } },
            { u8 ("Sample ralenti"), categories::samples,  { { ids::pitch, -5 }, { ids::formant, -5 }, { ids::latency, quality } } },
            { u8 ("Sample transposé"), categories::samples, { { ids::pitch, -3 }, { ids::latency, quality } } },
            { u8 ("Scène : ad-lib +12"), categories::adlibs, { { ids::pitch, 12 }, { ids::formant, 1 }, { ids::latency, live }, { c::mix, 40 } } },
        };
    }
}
