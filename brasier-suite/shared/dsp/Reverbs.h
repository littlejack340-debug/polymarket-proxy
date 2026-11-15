#pragma once

#include <array>
#include <vector>

#include <juce_core/juce_core.h>

#include "Lfo.h"
#include "OnePole.h"

namespace brasier::dsp
{
    /** Ligne de retard entiere minimaliste (taille puissance de 2) avec prises de sortie. */
    class SimpleDelay
    {
    public:
        void prepare (int maxLength)
        {
            const int size = juce::nextPowerOfTwo (juce::jmax (4, maxLength + 4));
            buffer.assign (static_cast<size_t> (size), 0.0f);
            mask = size - 1;
            head = 0;
        }

        void reset() noexcept { std::fill (buffer.begin(), buffer.end(), 0.0f); head = 0; }

        inline void push (float x) noexcept { buffer[static_cast<size_t> (head)] = x; head = (head + 1) & mask; }

        /** Valeur ecrite il y a 'delay' echantillons (delay >= 1). */
        inline float tap (int delay) const noexcept { return buffer[static_cast<size_t> ((head - delay) & mask)]; }

        /** Lecture fractionnaire lineaire. */
        inline float tapFractional (float delay) const noexcept
        {
            const auto i = static_cast<int> (delay);
            const float frac = delay - static_cast<float> (i);
            return tap (i) + frac * (tap (i + 1) - tap (i));
        }

    private:
        std::vector<float> buffer;
        int mask = 3, head = 0;
    };

    /** Passe-tout de Schroeder : y = (g + z^-D) / (1 + g z^-D). */
    class SchroederAllpass
    {
    public:
        void prepare (int length, float coefficient, int extraLength = 0)
        {
            delay = length;
            g = coefficient;
            line.prepare (length + extraLength + 4);
        }

        void reset() noexcept { line.reset(); }

        inline float process (float x) noexcept { return processWithDelay (x, static_cast<float> (delay)); }

        inline float processWithDelay (float x, float currentDelay) noexcept
        {
            const float delayed = line.tapFractional (currentDelay);   // ecrit il y a D echantillons
            const float v = x - g * delayed;
            line.push (v);
            return delayed + g * v;
        }

        inline float tap (int d) const noexcept { return line.tap (d); }

    private:
        SimpleDelay line;
        int delay = 1;
        float g = 0.5f;
    };

    //==============================================================================
    /**
        Reverb a plaque d'apres la structure publiee par J. Dattorro ("Effect
        Design, Part 1", JAES 1997) : 4 passe-tout de diffusion en entree, puis un
        "reservoir" en huit (deux branches croisees) avec passe-tout modules,
        amortissement des aigus et sorties prises en plusieurs points pour un
        stereo dense et decorrele.
    */
    class PlateReverb
    {
    public:
        void prepare (double sampleRate);
        void reset() noexcept;

        void setDecaySeconds (float rt60) noexcept;
        void setDamping (float amount01) noexcept { damping = juce::jlimit (0.0f, 0.95f, amount01 * 0.85f); }

        void process (float input, float& outLeft, float& outRight) noexcept;

    private:
        int scaled (int samplesAt29761) const noexcept;

        double sampleRate = 44100.0;
        std::array<SchroederAllpass, 4> inputDiffusers;
        SchroederAllpass modulatedLeft, modulatedRight, allpassLeft, allpassRight;
        SimpleDelay delayLeft1, delayLeft2, delayRight1, delayRight2;
        float dampLeft = 0.0f, dampRight = 0.0f;
        float damping = 0.3f, decay = 0.7f;
        float modulatedLeftLength = 0.0f, modulatedRightLength = 0.0f, excursion = 0.0f;
        Lfo modLeft, modRight;

        // Prises de sortie (deja mises a l'echelle de la frequence d'echantillonnage)
        std::array<int, 7> tapsLeft {}, tapsRight {};
        int lengthLeft1 = 0, lengthLeft2 = 0, lengthRight1 = 0, lengthRight2 = 0;
    };

    //==============================================================================
    /**
        Reverb a ressort : trois ressorts paralleles (comme un bac a 3 ressorts).
        Chaque ressort est une boucle :

            entree -> retard (~40 ms) -> chaine de 48 passe-tout "etires" -> passe-bas -> passe-haut -> x gain -> (boucle)

        Les passe-tout etires H(z) = (a + z^-K) / (1 + a z^-K) retardent davantage
        certaines frequences que d'autres (dispersion) : une impulsion ressort
        sous forme de "chirp", puis chaque aller-retour la disperse encore, d'ou
        le "boing" metallique typique. "Tension" regle la force de la dispersion.
    */
    class SpringReverb
    {
    public:
        void prepare (double sampleRate);
        void reset() noexcept;

        void setDecaySeconds (float rt60) noexcept;
        void setDamping (float amount01) noexcept;
        void setTension (float amount01) noexcept { coefficient = 0.42f + 0.33f * juce::jlimit (0.0f, 1.0f, amount01); }

        void process (float input, float& outLeft, float& outRight) noexcept;

    private:
        static constexpr int numSprings = 3;
        static constexpr int numStages = 48;

        struct Spring
        {
            SimpleDelay loop;
            std::vector<float> stageMemory;   // numStages x K
            float delaySamples = 1000.0f;
            float feedback = 0.5f;
            float lastOutput = 0.0f;
            OnePole lowpass, highpass;
            Lfo wander;
        };

        double sampleRate = 44100.0;
        int stretch = 4;          // K
        int stagePosition = 0;    // index circulaire commun 0..K-1
        float coefficient = 0.6f;
        std::array<Spring, numSprings> springs;
    };
}
