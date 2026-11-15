#pragma once

// Presets d'usine de Brasier Ressort (valeurs en unites reelles). Types : 0 ressort, 1 plaque.

#include "core/PresetHelpers.h"
#include "RessortProcessor.h"

namespace brasier::ressort
{
    inline std::vector<core::FactoryPreset> makeFactoryPresets()
    {
        using namespace core;
        namespace c = core::ids;
        constexpr float spring = 0, plate = 1;

        return {
            { "Init", {}, {} },

            { u8 ("Plaque voix"), categories::lead,
              { { ids::type, plate }, { ids::decay, 1.6f }, { ids::predelay, 30 }, { ids::damping, 45 }, { ids::lowCut, 250 },
                { ids::highCut, 8000 }, { ids::drive, 2 }, { c::mix, 18 } } },
            { u8 ("Plaque brillante"), categories::lead,
              { { ids::type, plate }, { ids::decay, 2.2f }, { ids::predelay, 45 }, { ids::damping, 20 }, { ids::lowCut, 300 },
                { ids::highCut, 12000 }, { c::mix, 15 } } },
            { u8 ("Petite pièce"), categories::lead,
              { { ids::type, plate }, { ids::decay, 0.7f }, { ids::predelay, 10 }, { ids::damping, 55 }, { ids::lowCut, 200 },
                { ids::highCut, 7000 }, { c::mix, 14 } } },

            { u8 ("Ressort lo-fi"), categories::adlibs,
              { { ids::type, spring }, { ids::decay, 2.5f }, { ids::predelay, 10 }, { ids::damping, 50 }, { ids::tension, 60 },
                { ids::lowCut, 300 }, { ids::highCut, 5000 }, { ids::drive, 6 }, { c::mix, 30 } } },
            { u8 ("Grande salle"), categories::adlibs,
              { { ids::type, plate }, { ids::decay, 5.0f }, { ids::predelay, 60 }, { ids::damping, 40 }, { ids::lowCut, 200 },
                { ids::highCut, 7000 }, { c::mix, 30 } } },

            { u8 ("Ressort surf"), categories::samples,
              { { ids::type, spring }, { ids::decay, 3.0f }, { ids::damping, 25 }, { ids::tension, 80 }, { ids::drive, 5 }, { c::mix, 35 } } },
            { u8 ("Ambiance de boucle"), categories::samples,
              { { ids::type, plate }, { ids::decay, 1.2f }, { ids::predelay, 15 }, { ids::damping, 60 }, { ids::highCut, 6000 }, { c::mix, 20 } } },
            { u8 ("Rhodes vintage"), categories::samples,
              { { ids::type, spring }, { ids::decay, 1.8f }, { ids::damping, 40 }, { ids::tension, 45 }, { ids::lowCut, 180 },
                { ids::highCut, 6500 }, { ids::drive, 4 }, { c::mix, 25 } } },

            { u8 ("808 Halo sans sub"), categories::bass808,
              { { ids::type, plate }, { ids::decay, 1.5f }, { ids::predelay, 20 }, { ids::lowCut, 400 }, { ids::highCut, 6000 }, { c::mix, 12 } } },

            { u8 ("Ressort qui claque"), categories::effects,
              { { ids::type, spring }, { ids::decay, 4.0f }, { ids::tension, 100 }, { ids::damping, 10 }, { ids::drive, 10 }, { c::mix, 50 } } },
            { u8 ("Nappe infinie"), categories::effects,
              { { ids::type, plate }, { ids::decay, 8.0f }, { ids::predelay, 80 }, { ids::damping, 30 }, { ids::lowCut, 300 },
                { ids::highCut, 9000 }, { ids::width, 100 }, { c::mix, 40 } } },
        };
    }
}
