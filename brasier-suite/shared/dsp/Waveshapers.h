#pragma once

#include <cmath>

namespace brasier::dsp
{
    /**
        Fonctions de transfert non lineaires (a utiliser sur-echantillonnees).
        Toutes ont une pente de 1 a l'origine : a faible niveau elles sont
        transparentes, la couleur n'apparait qu'en poussant le drive.
    */
    namespace shapers
    {
        /** tanh rapide (approximation de Pade 3/2), exacte a ~2 % pres, lisse
            jusqu'a la derivee premiere (pas de cassure a |x| = 3). */
        constexpr float fastTanh (float x) noexcept
        {
            if (x > 3.0f)  return 1.0f;
            if (x < -3.0f) return -1.0f;
            const float x2 = x * x;
            return x * (27.0f + x2) / (27.0f + 9.0f * x2);
        }

        /** Tube (triode) : tanh decentree. La polarisation rend la courbe
            asymetrique : la demi-alternance positive sature plus tot que la
            negative, ce qui produit surtout des harmoniques PAIRES (chaleur). */
        inline float tube (float x) noexcept
        {
            // Constantes calculees avec la MEME fonction que celle utilisee ci-dessous :
            // tube(0) vaut exactement 0 (aucun continu) et la pente a l'origine vaut 1.
            constexpr float bias = 0.3f;
            constexpr float h = 1.0e-3f;
            constexpr float atBias = fastTanh (bias);
            constexpr float slopeNorm = 2.0f * h / (fastTanh (bias + h) - fastTanh (bias - h));
            return (fastTanh (x + bias) - atBias) * slopeNorm;
        }

        /** Transistor : ecretage cubique a genou dur et plateau franc, symetrique :
            harmoniques IMPAIRES, attaque plus mordante que le tube. */
        inline float transistor (float x) noexcept
        {
            constexpr float limit = 1.5f;
            if (x >= limit)  return 1.0f;
            if (x <= -limit) return -1.0f;
            return x - (4.0f / 27.0f) * x * x * x;
        }

        /** Bande magnetique : compression tres progressive x / (1 + |x|),
            qui commence tot et ne "casse" jamais (a combiner avec pre/de-emphase). */
        inline float tape (float x) noexcept
        {
            return x / (1.0f + std::abs (x));
        }

        /** Diode : ecretage exponentiel asymetrique (genou net, plateaux
            differents selon la polarite), son plus "fuzz" et agressif. */
        inline float diode (float x) noexcept
        {
            constexpr float positive = 1.0f;
            constexpr float negative = 1.8f;
            return x >= 0.0f ? (1.0f - std::exp (-positive * x)) / positive
                             : -(1.0f - std::exp (negative * x)) / negative;
        }
    }
}
