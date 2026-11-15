#pragma once

#include <algorithm>
#include <cmath>

namespace brasier::dsp
{
    /**
        Lissage lineaire d'un parametre : quand la cible change, la valeur rejoint
        la nouvelle cible en une rampe de duree fixe (ex. 20 ms). Evite les clics
        et le "zipper noise" lors des mouvements de potard ou de l'automation.

        Entierement temps reel : aucune allocation, aucun verrou.
    */
    template <typename Type = float>
    class LinearSmoother
    {
    public:
        /** Fixe la duree de rampe. Une rampe en cours est terminee (la valeur
            saute a la cible) : appelable quand la frequence d'echantillonnage
            change, par ex. au changement de facteur d'oversampling. */
        void prepare (double sampleRate, double rampSeconds) noexcept
        {
            rampLength = std::max (1, static_cast<int> (std::lround (sampleRate * rampSeconds)));
            countdown = 0;
            current = target;
        }

        /** Saut immediat, sans rampe (initialisation, reset). */
        void setCurrentAndTarget (Type value) noexcept
        {
            current = target = value;
            countdown = 0;
        }

        void setTarget (Type newTarget) noexcept
        {
            if (! (newTarget < target) && ! (target < newTarget))   // egalite exacte
                return;

            target = newTarget;
            countdown = rampLength;
            step = (target - current) / static_cast<Type> (rampLength);
        }

        Type getNext() noexcept
        {
            if (countdown <= 0)
                return target;

            if (--countdown == 0)
                current = target;
            else
                current += step;

            return current;
        }

        /** Avance de numSamples d'un coup (quand on n'a besoin que de la valeur finale). */
        void skip (int numSamples) noexcept
        {
            if (countdown <= 0)
                return;

            if (numSamples >= countdown)
            {
                current = target;
                countdown = 0;
            }
            else
            {
                current += step * static_cast<Type> (numSamples);
                countdown -= numSamples;
            }
        }

        bool isSmoothing() const noexcept { return countdown > 0; }
        Type getCurrent() const noexcept  { return current; }
        Type getTarget() const noexcept   { return target; }

    private:
        Type current {}, target {}, step {};
        int rampLength = 1;
        int countdown = 0;
    };
}
