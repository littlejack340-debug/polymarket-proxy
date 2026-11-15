#pragma once

#include <cmath>

#include "MathUtils.h"

namespace brasier::dsp
{
    /**
        Suiveur d'enveloppe (detecteur de niveau) a attaque / relachement
        separes, sur la valeur crete du signal. Sert au ducking du delay et a
        la modulation du filtre par l'enveloppe.
    */
    class EnvelopeFollower
    {
    public:
        void prepare (double newSampleRate) noexcept
        {
            sampleRate = newSampleRate;
            setTimes (attackSeconds, releaseSeconds);
            reset();
        }

        void setTimes (double attack, double release) noexcept
        {
            attackSeconds = attack;
            releaseSeconds = release;
            attackCoeff = onePoleCoefficient (sampleRate, attack);
            releaseCoeff = onePoleCoefficient (sampleRate, release);
        }

        void reset() noexcept { envelope = 0.0f; }

        inline float process (float input) noexcept
        {
            const float x = std::abs (input);
            const float coeff = x > envelope ? attackCoeff : releaseCoeff;
            envelope += coeff * (x - envelope);
            if (envelope < 1.0e-9f)
                envelope = 0.0f;
            return envelope;
        }

        float getEnvelope() const noexcept { return envelope; }

    private:
        double sampleRate = 44100.0;
        double attackSeconds = 0.005, releaseSeconds = 0.2;
        float attackCoeff = 1.0f, releaseCoeff = 1.0f;
        float envelope = 0.0f;
    };
}
