#pragma once

#include <cmath>
#include <vector>

#include <juce_core/juce_core.h>

namespace brasier::dsp
{
    /**
        Ligne de retard multicanal circulaire (taille puissance de 2) avec
        lecture fractionnaire par interpolation d'Hermite 4 points (cubique) :
        indispensable quand le temps de retard est module (wow, flutter,
        chorus...) sinon la lecture "en escalier" cree du bruit.

        Lecture et ecriture sont relatives a une tete d'ecriture : on peut lire
        un sous-bloc entier (offset 0..N-1) avant de l'ecrire, a condition que
        le retard lu depasse N + 3 echantillons.
    */
    class DelayLine
    {
    public:
        void prepare (int numChannels, int maxDelaySamples)
        {
            const int needed = juce::jmax (8, maxDelaySamples + 8);
            size = juce::nextPowerOfTwo (needed);
            mask = size - 1;
            channels = juce::jmax (1, numChannels);
            buffer.assign (static_cast<size_t> (channels * size), 0.0f);
            head = 0;
        }

        void reset() noexcept
        {
            std::fill (buffer.begin(), buffer.end(), 0.0f);
            head = 0;
        }

        int getMaxDelay() const noexcept { return size - 8; }

        /** Lit avec un retard fractionnaire (en echantillons) par rapport a la
            position (tete + offset). */
        inline float read (int channel, double delay, int offset = 0) const noexcept
        {
            const double pos = static_cast<double> (head + offset) - delay;
            const double floorPos = std::floor (pos);
            const auto i0 = static_cast<int> (floorPos);
            const auto t = static_cast<float> (pos - floorPos);

            const float* data = buffer.data() + channel * size;
            const float ym1 = data[(i0 - 1) & mask];
            const float y0  = data[i0 & mask];
            const float y1  = data[(i0 + 1) & mask];
            const float y2  = data[(i0 + 2) & mask];

            const float c1 = 0.5f * (y1 - ym1);
            const float c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
            const float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
            return ((c3 * t + c2) * t + c1) * t + y0;
        }

        /** Lecture entiere (sans interpolation). */
        inline float readInteger (int channel, int delay, int offset = 0) const noexcept
        {
            return buffer[static_cast<size_t> (channel * size + ((head + offset - delay) & mask))];
        }

        inline void write (int channel, float value, int offset = 0) noexcept
        {
            buffer[static_cast<size_t> (channel * size + ((head + offset) & mask))] = value;
        }

        /** Avance la tete d'ecriture apres avoir ecrit n echantillons. */
        inline void advance (int n = 1) noexcept { head = (head + n) & mask; }

    private:
        std::vector<float> buffer;
        int size = 8, mask = 7, channels = 1, head = 0;
    };
}
