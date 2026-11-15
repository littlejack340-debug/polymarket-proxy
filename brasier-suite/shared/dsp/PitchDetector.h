#pragma once

#include <vector>

#include "Svf.h"

namespace brasier::dsp
{
    /**
        Detecteur de hauteur YIN (de Cheveigne & Kawahara, 2002), temps reel.

        Pour economiser le CPU, le signal est filtre (passe-bas 4 poles a 2,5 kHz)
        puis decime a ~11 kHz : la fondamentale d'une voix est toujours bien en
        dessous. Sur la derniere fenetre on calcule :

            d(tau)  = somme (x[j] - x[j + tau])^2          fonction de difference
            d'(tau) = d(tau) * tau / somme(d(1..tau))      normalisee (cumul moyen)

        La periode est le premier minimum de d' sous un seuil (0,15), affine par
        interpolation parabolique. Un minimum trop haut = son non voise (souffle,
        consonne). Une hysteresis evite les sauts d'octave isoles.

        Allocation dans prepare() uniquement.
    */
    class PitchDetector
    {
    public:
        /** lowestHz fixe la taille maximale des buffers (ne pas descendre en dessous ensuite). */
        void prepare (double sampleRate, double lowestHz, double highestHz);
        void reset() noexcept;

        /** Change la plage de recherche (lowestHz >= celui de prepare). Temps reel. */
        void setRange (double lowestHz, double highestHz) noexcept;

        /** Pousse un echantillon a la frequence de base. */
        inline void push (float x) noexcept
        {
            const float filtered = antiAlias2.process (aaCoeffs, antiAlias1.process (aaCoeffs, x));
            if (++decimationCounter >= decimation)
            {
                decimationCounter = 0;
                ring[static_cast<size_t> (writeIndex)] = filtered;
                writeIndex = (writeIndex + 1) & ringMask;
            }
        }

        /** Analyse la fenetre la plus recente et met a jour la periode. */
        void analyse() noexcept;

        /** Periode courante en echantillons a la frequence de base. */
        float getPeriod() const noexcept   { return period; }
        bool isVoiced() const noexcept     { return voiced; }
        float getConfidence() const noexcept { return confidence; }

    private:
        double sampleRate = 44100.0;
        int decimation = 4;
        int decimationCounter = 0;

        SvfCoeffs aaCoeffs;
        SvfState antiAlias1, antiAlias2;

        std::vector<float> ring;
        int ringMask = 0, writeIndex = 0;

        std::vector<float> window, difference;
        int minLag = 2, maxLag = 64, preparedMaxLag = 64;

        float period = 300.0f;          // a la frequence de base
        float candidate = 0.0f;
        int candidateCount = 0;
        bool voiced = false;
        float confidence = 0.0f;
        int samplesSinceVoiced = 0;
    };
}
