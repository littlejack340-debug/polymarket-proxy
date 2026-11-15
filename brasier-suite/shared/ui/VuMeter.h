#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace brasier::ui
{
    /**
        VU-metre a aiguille, cadran creme facon console.

        - Reference : 0 VU = -18 dBFS RMS (calage numerique courant)
        - Balistique VU : l'aiguille atteint ~99 % de la valeur en 300 ms
        - Echelle non lineaire d'origine (proportionnelle a la tension), -20 a +3 VU
        - LED de crete qui s'allume au-dela de -0,3 dBFS (tenue 1 s)
    */
    class VuMeter : public juce::Component
    {
    public:
        explicit VuMeter (juce::String caption);

        /** A appeler a cadence reguliere (ex. 30 Hz) avec le carre moyen du signal et la crete. */
        void update (float meanSquare, float peak, float deltaSeconds);

        void setAccent (juce::Colour c) { accent = c; repaint(); }

        void paint (juce::Graphics&) override;

        static constexpr float referenceDbfs = -18.0f;

    private:
        static float vuToPosition (float vu) noexcept;

        juce::String caption;
        juce::Colour accent { 0xffe0782f };
        float needleVu = -20.0f;
        float clipHold = 0.0f;
    };
}
