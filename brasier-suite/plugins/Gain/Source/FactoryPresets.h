#pragma once

// Presets d'usine de Brasier Gain (outil de test).

#include "core/PresetHelpers.h"
#include "GainProcessor.h"

namespace brasier::gain
{
    inline std::vector<core::FactoryPreset> makeFactoryPresets()
    {
        using namespace core;
        return {
            { "Init", {}, {} },
            { u8 ("+6 dB avec soft-clip"), categories::mix, { { ids::gain, 6 }, { ids::clip, 1 } } },
            { u8 ("-6 dB"), categories::mix, { { ids::gain, -6 } } },
        };
    }
}
