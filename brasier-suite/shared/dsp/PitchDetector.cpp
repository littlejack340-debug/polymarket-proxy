#include "PitchDetector.h"

namespace brasier::dsp
{
    void PitchDetector::prepare (double newSampleRate, double lowestHz, double highestHz)
    {
        sampleRate = newSampleRate;
        decimation = juce::jmax (1, static_cast<int> (std::lround (sampleRate / 11025.0)));
        aaCoeffs = makeSvfCoeffs (SvfType::lowpass, sampleRate, 2500.0f, 0.70710678f);

        const double decimatedRate = sampleRate / decimation;
        preparedMaxLag = static_cast<int> (std::ceil (decimatedRate / lowestHz)) + 2;

        const int ringSize = juce::nextPowerOfTwo (2 * preparedMaxLag + 8);
        ring.assign (static_cast<size_t> (ringSize), 0.0f);
        ringMask = ringSize - 1;

        window.assign (static_cast<size_t> (2 * preparedMaxLag + 2), 0.0f);
        difference.assign (static_cast<size_t> (preparedMaxLag + 2), 0.0f);

        setRange (lowestHz, highestHz);
        reset();
    }

    void PitchDetector::reset() noexcept
    {
        std::fill (ring.begin(), ring.end(), 0.0f);
        antiAlias1.reset();
        antiAlias2.reset();
        writeIndex = 0;
        decimationCounter = 0;
        period = static_cast<float> (sampleRate / 150.0);
        candidate = 0.0f;
        candidateCount = 0;
        voiced = false;
        confidence = 0.0f;
        samplesSinceVoiced = 0;
    }

    void PitchDetector::setRange (double lowestHz, double highestHz) noexcept
    {
        const double decimatedRate = sampleRate / decimation;
        maxLag = juce::jlimit (4, preparedMaxLag, static_cast<int> (std::ceil (decimatedRate / lowestHz)));
        minLag = juce::jlimit (2, maxLag - 2, static_cast<int> (std::floor (decimatedRate / highestHz)));
    }

    void PitchDetector::analyse() noexcept
    {
        // Fenetre : les (maxLag + W) derniers echantillons decimes, W = maxLag
        const int W = maxLag;
        const int total = W + maxLag;
        int idx = (writeIndex - total) & ringMask;
        double energy = 0.0;

        for (int i = 0; i < total; ++i)
        {
            const float v = ring[static_cast<size_t> (idx)];
            window[static_cast<size_t> (i)] = v;
            energy += static_cast<double> (v) * v;
            idx = (idx + 1) & ringMask;
        }

        // Silence : on garde la derniere periode, non voise
        if (energy < 1.0e-7 * total)
        {
            voiced = false;
            confidence = 0.0f;
            return;
        }

        // Fonction de difference + normalisation par la moyenne cumulee
        const float* x = window.data();
        difference[0] = 1.0f;
        double runningSum = 0.0;
        for (int tau = 1; tau <= maxLag; ++tau)
        {
            float d = 0.0f;
            for (int j = 0; j < W; ++j)
            {
                const float delta = x[j] - x[j + tau];
                d += delta * delta;
            }
            runningSum += d;
            difference[static_cast<size_t> (tau)] = runningSum > 0.0 ? static_cast<float> (d * tau / runningSum) : 1.0f;
        }

        // Premier minimum sous le seuil, sinon minimum global
        constexpr float threshold = 0.15f;
        int best = -1;
        for (int tau = minLag; tau < maxLag; ++tau)
        {
            if (difference[static_cast<size_t> (tau)] < threshold)
            {
                while (tau + 1 < maxLag && difference[static_cast<size_t> (tau + 1)] < difference[static_cast<size_t> (tau)])
                    ++tau;
                best = tau;
                break;
            }
        }

        if (best < 0)
        {
            best = minLag;
            for (int tau = minLag + 1; tau < maxLag; ++tau)
                if (difference[static_cast<size_t> (tau)] < difference[static_cast<size_t> (best)])
                    best = tau;
        }

        const float dMin = difference[static_cast<size_t> (best)];
        confidence = juce::jlimit (0.0f, 1.0f, 1.0f - dMin);

        if (dMin > 0.35f)
        {
            voiced = false;   // souffle, consonne : on conserve la derniere periode
            return;
        }

        // Interpolation parabolique autour du minimum
        float refined = static_cast<float> (best);
        if (best > 1 && best < maxLag)
        {
            const float a = difference[static_cast<size_t> (best - 1)];
            const float b = difference[static_cast<size_t> (best)];
            const float c = difference[static_cast<size_t> (best + 1)];
            const float denom = a - 2.0f * b + c;
            if (std::abs (denom) > 1.0e-9f)
                refined += juce::jlimit (-0.5f, 0.5f, 0.5f * (a - c) / denom);
        }

        const float newPeriod = refined * static_cast<float> (decimation);

        // Hysteresis : un saut de plus de 18 % doit etre confirme deux fois de suite
        if (! voiced || std::abs (newPeriod - period) < 0.18f * period)
        {
            period = newPeriod;
            candidateCount = 0;
        }
        else if (candidateCount > 0 && std::abs (newPeriod - candidate) < 0.18f * candidate)
        {
            if (++candidateCount >= 2)
            {
                period = newPeriod;
                candidateCount = 0;
            }
        }
        else
        {
            candidate = newPeriod;
            candidateCount = 1;
        }

        voiced = true;
    }
}
