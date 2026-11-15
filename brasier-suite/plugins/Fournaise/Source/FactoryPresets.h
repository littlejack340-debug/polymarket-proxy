#pragma once

// Presets d'usine de Brasier Fournaise (valeurs en unites reelles).

#include "core/PresetHelpers.h"
#include "FournaiseProcessor.h"

namespace brasier::fournaise
{
    inline std::vector<core::FactoryPreset> makeFactoryPresets()
    {
        using namespace core;
        namespace c = core::ids;
        constexpr float tube = 0, transistor = 1, tape = 2, diode = 3;

        return {
            { "Init", {}, {} },

            // Voix lead : de la presence sans perdre l'intelligibilite
            { u8 ("Chaleur lampe"), categories::lead,
              { { ids::model, tube }, { ids::drive, 9 }, { ids::tone, 1 }, { ids::preLowCut, 90 }, { ids::postHighCut, 16000 }, { c::mix, 70 } } },
            { u8 ("Présence radio"), categories::lead,
              { { ids::model, transistor }, { ids::drive, 7 }, { ids::tone, 2.5f }, { ids::preLowCut, 220 }, { ids::preHighCut, 9500 }, { c::mix, 55 } } },
            { u8 ("Bande douce"), categories::lead,
              { { ids::model, tape }, { ids::drive, 8 }, { ids::tone, 0.5f }, { ids::preLowCut, 80 }, { c::mix, 100 } } },

            // Ad-libs : plus sale, plus filtre, plus de caractere
            { u8 ("Grain téléphone"), categories::adlibs,
              { { ids::model, diode }, { ids::drive, 18 }, { ids::preLowCut, 500 }, { ids::preHighCut, 3500 },
                { ids::postLowCut, 450 }, { ids::postHighCut, 4200 }, { c::mix, 100 } } },
            { u8 ("Cri saturé"), categories::adlibs,
              { { ids::model, transistor }, { ids::drive, 20 }, { ids::punish, 1 }, { ids::tone, 1 }, { ids::preLowCut, 200 }, { c::mix, 55 } } },
            { u8 ("Mégaphone"), categories::adlibs,
              { { ids::model, transistor }, { ids::drive, 24 }, { ids::preLowCut, 700 }, { ids::preHighCut, 4000 },
                { ids::postLowCut, 600 }, { ids::postHighCut, 3800 }, { ids::tone, 2 }, { c::mix, 100 } } },

            // 808 : saturer au-dessus du sub, garder le grave propre
            { u8 ("808 Grondement"), categories::bass808,
              { { ids::model, tube }, { ids::drive, 14 }, { ids::tone, -1 }, { ids::postHighCut, 9000 }, { c::mix, 100 } } },
            { u8 ("808 Sale, sub propre"), categories::bass808,
              { { ids::model, diode }, { ids::drive, 22 }, { ids::punish, 1 }, { ids::preLowCut, 120 }, { ids::postHighCut, 8000 }, { c::mix, 50 } } },
            { u8 ("808 Bande"), categories::bass808,
              { { ids::model, tape }, { ids::drive, 12 }, { ids::postHighCut, 10000 }, { c::mix, 100 } } },
            { u8 ("808 Distorsion drill"), categories::bass808,
              { { ids::model, transistor }, { ids::drive, 26 }, { ids::punish, 1 }, { ids::preLowCut, 60 }, { ids::postHighCut, 6500 },
                { ids::tone, -2 }, { c::mix, 65 } } },

            // Samples : patine et lo-fi
            { u8 ("Vinyle chaud"), categories::samples,
              { { ids::model, tape }, { ids::drive, 10 }, { ids::tone, -2 }, { ids::postLowCut, 40 }, { ids::postHighCut, 11000 }, { c::mix, 100 } } },
            { u8 ("Boucle lo-fi"), categories::samples,
              { { ids::model, tape }, { ids::drive, 16 }, { ids::tone, -3 }, { ids::preHighCut, 7000 }, { ids::postLowCut, 80 },
                { ids::postHighCut, 6000 }, { c::mix, 100 } } },
            { u8 ("Piano poussiéreux"), categories::samples,
              { { ids::model, tube }, { ids::drive, 12 }, { ids::tone, -1.5f }, { ids::postLowCut, 120 }, { ids::postHighCut, 7500 }, { c::mix, 100 } } },

            // Mix
            { u8 ("Colle de bus"), categories::mix,
              { { ids::model, tube }, { ids::drive, 4 }, { c::mix, 45 } } },
            { u8 ("Écrêteur de batterie"), categories::mix,
              { { ids::model, transistor }, { ids::drive, 10 }, { ids::preLowCut, 30 }, { c::mix, 80 } } },
        };
    }
}
