#pragma once

// Presets d'usine de Brasier Ruban (valeurs en unites reelles).
// Divisions : 4 = 1/16, 7 = 1/8, 8 = 1/8 pointee, 9 = 1/4 triolet, 10 = 1/4, 11 = 1/4 pointee, 13 = 1/2.

#include "core/PresetHelpers.h"
#include "RubanProcessor.h"

namespace brasier::ruban
{
    inline std::vector<core::FactoryPreset> makeFactoryPresets()
    {
        using namespace core;
        namespace c = core::ids;
        constexpr float mono = 0, stereo = 1, pingPong = 2;

        return {
            { "Init", {}, {} },

            // Voix lead : delay present mais qui s'efface sous la voix (ducking)
            { u8 ("Écho 1/4 discret"), categories::lead,
              { { ids::sync, 1 }, { ids::division, 10 }, { ids::feedback, 25 }, { ids::saturation, 20 }, { ids::lowCut, 300 },
                { ids::highCut, 4500 }, { ids::wow, 10 }, { ids::flutter, 5 }, { ids::mode, stereo }, { ids::duck, 60 },
                { ids::duckThreshold, -30 }, { c::mix, 20 } } },
            { u8 ("Slap rétro"), categories::lead,
              { { ids::sync, 0 }, { ids::time, 110 }, { ids::feedback, 8 }, { ids::saturation, 35 }, { ids::lowCut, 200 },
                { ids::highCut, 5000 }, { ids::mode, mono }, { c::mix, 18 } } },
            { u8 ("Refrain large"), categories::lead,
              { { ids::sync, 1 }, { ids::division, 8 }, { ids::feedback, 35 }, { ids::saturation, 25 }, { ids::lowCut, 350 },
                { ids::highCut, 6000 }, { ids::mode, stereo }, { ids::spread, 40 }, { ids::duck, 45 }, { ids::duckThreshold, -28 }, { c::mix, 22 } } },

            // Ad-libs : repetitions qui se baladent
            { u8 ("Ping-pong 1/8"), categories::adlibs,
              { { ids::sync, 1 }, { ids::division, 7 }, { ids::feedback, 45 }, { ids::saturation, 30 }, { ids::lowCut, 400 },
                { ids::highCut, 6000 }, { ids::mode, pingPong }, { ids::duck, 40 }, { c::mix, 30 } } },
            { u8 ("Lancer 1/8 pointée"), categories::adlibs,
              { { ids::sync, 1 }, { ids::division, 8 }, { ids::feedback, 55 }, { ids::saturation, 40 }, { ids::lowCut, 350 },
                { ids::highCut, 3500 }, { ids::wow, 25 }, { ids::mode, stereo }, { ids::spread, 30 }, { c::mix, 35 } } },
            { u8 ("Tunnel"), categories::adlibs,
              { { ids::sync, 1 }, { ids::division, 10 }, { ids::feedback, 85 }, { ids::saturation, 60 }, { ids::lowCut, 500 },
                { ids::highCut, 2500 }, { ids::wow, 35 }, { ids::flutter, 20 }, { c::mix, 30 } } },
            { u8 ("Triolet fantôme"), categories::adlibs,
              { { ids::sync, 1 }, { ids::division, 9 }, { ids::feedback, 40 }, { ids::saturation, 30 }, { ids::lowCut, 600 },
                { ids::highCut, 3000 }, { ids::mode, pingPong }, { ids::duck, 70 }, { ids::duckThreshold, -35 }, { c::mix, 30 } } },

            // 808 : un echo court au-dessus du sub
            { u8 ("808 Ombre"), categories::bass808,
              { { ids::sync, 1 }, { ids::division, 4 }, { ids::feedback, 15 }, { ids::saturation, 40 }, { ids::lowCut, 250 },
                { ids::highCut, 3000 }, { ids::mode, mono }, { c::mix, 15 } } },

            // Samples : bande usee, echos sales
            { u8 ("Bande usée"), categories::samples,
              { { ids::sync, 1 }, { ids::division, 11 }, { ids::feedback, 50 }, { ids::saturation, 70 }, { ids::lowCut, 150 },
                { ids::highCut, 3000 }, { ids::wow, 70 }, { ids::flutter, 50 }, { c::mix, 35 } } },
            { u8 ("Écho de cassette"), categories::samples,
              { { ids::sync, 1 }, { ids::division, 10 }, { ids::feedback, 30 }, { ids::saturation, 55 }, { ids::lowCut, 200 },
                { ids::highCut, 4000 }, { ids::wow, 45 }, { ids::flutter, 35 }, { c::mix, 25 } } },

            // Effets
            { u8 ("Dub infini"), categories::effects,
              { { ids::sync, 1 }, { ids::division, 13 }, { ids::feedback, 100 }, { ids::saturation, 50 }, { ids::lowCut, 200 },
                { ids::highCut, 2000 }, { ids::wow, 30 }, { c::mix, 40 } } },
            { u8 ("Auto-oscillation"), categories::effects,
              { { ids::sync, 0 }, { ids::time, 60 }, { ids::feedback, 108 }, { ids::saturation, 80 }, { ids::lowCut, 300 },
                { ids::highCut, 3500 }, { ids::wow, 20 }, { c::mix, 35 } } },
        };
    }
}
