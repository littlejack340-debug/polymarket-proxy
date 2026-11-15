#pragma once

// Presets d'usine de Brasier Remous (valeurs en unites reelles).
// Modes : 0 chorus, 1 flanger, 2 phaser, 3 tremolo. Formes : 0 sinus, 1 triangle, 2 scie+, 3 scie-, 4 carre.

#include "core/PresetHelpers.h"
#include "RemousProcessor.h"

namespace brasier::remous
{
    inline std::vector<core::FactoryPreset> makeFactoryPresets()
    {
        using namespace core;
        namespace c = core::ids;
        constexpr float chorus = 0, flanger = 1, phaser = 2, tremolo = 3;

        return {
            { "Init", {}, {} },

            { u8 ("Chorus large"), categories::lead,
              { { ids::mode, chorus }, { ids::depth, 35 }, { ids::rate, 0.6f }, { ids::spread, 80 }, { ids::character, 25 }, { c::mix, 40 } } },
            { u8 ("Refrain épais"), categories::lead,
              { { ids::mode, chorus }, { ids::depth, 50 }, { ids::rate, 0.4f }, { ids::spread, 100 }, { ids::character, 35 }, { c::mix, 35 } } },

            { u8 ("Chorus 80s"), categories::adlibs,
              { { ids::mode, chorus }, { ids::depth, 70 }, { ids::rate, 1.2f }, { ids::character, 60 }, { c::mix, 60 } } },
            { u8 ("Flanger jet"), categories::adlibs,
              { { ids::mode, flanger }, { ids::depth, 80 }, { ids::rate, 0.15f }, { ids::feedback, 70 }, { ids::character, 30 } } },
            { u8 ("Vibrato"), categories::adlibs,
              { { ids::mode, chorus }, { ids::depth, 45 }, { ids::rate, 5.5f }, { ids::spread, 0 }, { ids::character, 10 }, { c::mix, 100 } } },

            { u8 ("Phaser vintage"), categories::samples,
              { { ids::mode, phaser }, { ids::depth, 60 }, { ids::rate, 0.35f }, { ids::feedback, 40 }, { ids::character, 40 } } },
            { u8 ("Tremolo ampli 1/8"), categories::samples,
              { { ids::mode, tremolo }, { ids::depth, 55 }, { ids::sync, 1 }, { ids::division, 7 }, { ids::shape, 0 }, { ids::character, 30 } } },
            { u8 ("Auto-pan 1/4"), categories::samples,
              { { ids::mode, tremolo }, { ids::depth, 45 }, { ids::sync, 1 }, { ids::division, 10 }, { ids::spread, 100 }, { ids::shape, 1 } } },
            { u8 ("Phaser lent 2 mesures"), categories::samples,
              { { ids::mode, phaser }, { ids::depth, 75 }, { ids::sync, 1 }, { ids::division, 16 }, { ids::feedback, 55 }, { ids::character, 50 } } },

            { u8 ("808 Largeur (chorus doux)"), categories::bass808,
              { { ids::mode, chorus }, { ids::depth, 20 }, { ids::rate, 0.3f }, { ids::spread, 70 }, { ids::character, 40 }, { c::mix, 25 } } },

            { u8 ("Hachoir 1/16"), categories::effects,
              { { ids::mode, tremolo }, { ids::depth, 100 }, { ids::sync, 1 }, { ids::division, 4 }, { ids::shape, 4 }, { ids::character, 0 } } },
            { u8 ("Flanger négatif"), categories::effects,
              { { ids::mode, flanger }, { ids::depth, 60 }, { ids::rate, 0.25f }, { ids::feedback, -80 }, { ids::character, 50 } } },
        };
    }
}
