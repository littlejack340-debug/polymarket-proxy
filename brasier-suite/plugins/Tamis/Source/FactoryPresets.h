#pragma once

// Presets d'usine de Brasier Tamis (valeurs en unites reelles).
// Types : 0 LP, 1 HP, 2 BP, 3 Notch. Pente : 0 = 12 dB, 1 = 24 dB.
// Formes LFO : 0 sinus, 1 triangle, 2 scie+, 3 scie-, 4 carre, 5 S&H, 6 aleatoire.
// Divisions : 4 = 1/16, 7 = 1/8, 10 = 1/4, 13 = 1/2, 15 = 1/1, 16 = 2/1, 17 = 4/1.

#include "core/PresetHelpers.h"
#include "TamisProcessor.h"

namespace brasier::tamis
{
    inline std::vector<core::FactoryPreset> makeFactoryPresets()
    {
        using namespace core;
        namespace c = core::ids;
        constexpr float lp = 0, hp = 1, bp = 2, notch = 3;

        return {
            { "Init", {}, {} },

            // Samples / instrus
            { u8 ("Intro radio"), categories::samples,
              { { ids::type, bp }, { ids::cutoff, 1500 }, { ids::resonance, 35 }, { ids::drive, 4 } } },
            { u8 ("Intro étouffée"), categories::samples,
              { { ids::type, lp }, { ids::slope, 1 }, { ids::cutoff, 450 }, { ids::resonance, 20 } } },
            { u8 ("Montée 4 mesures"), categories::samples,
              { { ids::type, lp }, { ids::slope, 1 }, { ids::cutoff, 700 }, { ids::resonance, 45 }, { ids::lfoAmount, 70 },
                { ids::lfoShape, 2 }, { ids::lfoSync, 1 }, { ids::lfoDivision, 17 }, { ids::drive, 3 } } },
            { u8 ("Respiration lente"), categories::samples,
              { { ids::type, lp }, { ids::cutoff, 1800 }, { ids::resonance, 25 }, { ids::lfoAmount, 45 }, { ids::lfoShape, 0 },
                { ids::lfoSync, 1 }, { ids::lfoDivision, 15 } } },

            // Voix
            { u8 ("Voix téléphone"), categories::lead,
              { { ids::type, bp }, { ids::cutoff, 1800 }, { ids::resonance, 25 }, { ids::drive, 8 } } },
            { u8 ("Wah automatique"), categories::adlibs,
              { { ids::type, bp }, { ids::cutoff, 450 }, { ids::resonance, 55 }, { ids::envAmount, 70 }, { ids::envAttack, 5 },
                { ids::envRelease, 160 } } },
            { u8 ("Ad-lib qui s'ouvre"), categories::adlibs,
              { { ids::type, lp }, { ids::slope, 1 }, { ids::cutoff, 600 }, { ids::resonance, 30 }, { ids::envAmount, 60 },
                { ids::envAttack, 20 }, { ids::envRelease, 300 } } },

            // 808
            { u8 ("808 Wobble 1/8"), categories::bass808,
              { { ids::type, lp }, { ids::slope, 1 }, { ids::cutoff, 300 }, { ids::resonance, 40 }, { ids::lfoAmount, 50 },
                { ids::lfoShape, 1 }, { ids::lfoSync, 1 }, { ids::lfoDivision, 7 }, { ids::drive, 6 } } },
            { u8 ("808 Nettoyage infra"), categories::bass808,
              { { ids::type, hp }, { ids::cutoff, 28 }, { ids::resonance, 10 } } },
            { u8 ("808 Mordant"), categories::bass808,
              { { ids::type, lp }, { ids::cutoff, 900 }, { ids::resonance, 55 }, { ids::drive, 12 }, { ids::envAmount, 40 } } },

            // Effets
            { u8 ("Hachoir 1/16"), categories::effects,
              { { ids::type, lp }, { ids::slope, 1 }, { ids::cutoff, 200 }, { ids::resonance, 20 }, { ids::lfoAmount, 85 },
                { ids::lfoShape, 4 }, { ids::lfoSync, 1 }, { ids::lfoDivision, 4 } } },
            { u8 ("Encoche balayée"), categories::effects,
              { { ids::type, notch }, { ids::cutoff, 1000 }, { ids::resonance, 30 }, { ids::lfoAmount, 60 }, { ids::lfoSync, 1 },
                { ids::lfoDivision, 13 } } },
            { u8 ("Sifflement auto-oscillant"), categories::effects,
              { { ids::type, lp }, { ids::cutoff, 650 }, { ids::resonance, 100 }, { c::mix, 30 } } },
            { u8 ("Aléatoire 1/8"), categories::effects,
              { { ids::type, bp }, { ids::cutoff, 1200 }, { ids::resonance, 50 }, { ids::lfoAmount, 60 }, { ids::lfoShape, 5 },
                { ids::lfoSync, 1 }, { ids::lfoDivision, 7 } } },
        };
    }
}
