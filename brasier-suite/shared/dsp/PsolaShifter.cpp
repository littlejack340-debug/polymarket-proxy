#include "PsolaShifter.h"
#include "MathUtils.h"

namespace brasier::dsp
{
    namespace
    {
        constexpr double highestHz = 1000.0;
        constexpr int maxGrainsPerSample = 8;   // garde-fou (ne doit jamais etre atteint)
    }

    int PsolaShifter::latencyFor (LatencyMode mode) const noexcept
    {
        return static_cast<int> (std::ceil (2.0 * sampleRate / lowestHzFor (mode))) + 8;
    }

    void PsolaShifter::prepare (double newSampleRate, int numChannels)
    {
        sampleRate = newSampleRate;
        numChans = juce::jlimit (1, maxChannels, numChannels);

        detector.prepare (sampleRate, lowestHzFor (LatencyMode::quality), highestHz);
        analysisHop = juce::jmax (32, static_cast<int> (std::lround (sampleRate * 0.005)));

        const double longestPeriod = sampleRate / lowestHzFor (LatencyMode::quality);
        const auto ringSize = juce::nextPowerOfTwo (latencyFor (LatencyMode::quality)
                                                    + static_cast<int> (std::ceil (6.0 * longestPeriod)) + 64);
        ringMask = ringSize - 1;

        for (auto& b : input)  b.assign (static_cast<size_t> (ringSize), 0.0f);
        for (auto& b : output) b.assign (static_cast<size_t> (ringSize), 0.0f);

        setLatencyMode (latencyMode);
        reset();
    }

    void PsolaShifter::reset() noexcept
    {
        for (auto& b : input)  std::fill (b.begin(), b.end(), 0.0f);
        for (auto& b : output) std::fill (b.begin(), b.end(), 0.0f);
        detector.reset();
        hopCounter = 0;
        now = 0;
        markCount = 0;
        lastMarkPosition = 0.0;
        nextSynthesis = 0.0;
        nextMarkForIdentity = -1;
    }

    void PsolaShifter::setLatencyMode (LatencyMode mode) noexcept
    {
        latencyMode = mode;
        latency = latencyFor (mode);
        maxPeriod = static_cast<float> (sampleRate / lowestHzFor (mode));
        detector.setRange (lowestHzFor (mode), highestHz);
    }

    //==============================================================================
    inline float PsolaShifter::readInput (int channel, double absolutePosition) const noexcept
    {
        const double floorPos = std::floor (absolutePosition);
        const auto i0 = static_cast<int64_t> (floorPos);
        const auto t = static_cast<float> (absolutePosition - floorPos);
        const float* data = input[static_cast<size_t> (channel)].data();

        const float ym1 = data[(i0 - 1) & ringMask];
        const float y0  = data[i0 & ringMask];
        const float y1  = data[(i0 + 1) & ringMask];
        const float y2  = data[(i0 + 2) & ringMask];

        const float c1 = 0.5f * (y1 - ym1);
        const float c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
        const float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
        return ((c3 * t + c2) * t + c1) * t + y0;
    }

    void PsolaShifter::createMarks() noexcept
    {
        const float period = juce::jlimit (static_cast<float> (sampleRate / highestHz), maxPeriod, detector.getPeriod());
        const auto nowD = static_cast<double> (now);

        // Rattrapage (debut, changement de mode) : on repart juste derriere le present
        if (nowD - lastMarkPosition > 4.0 * maxPeriod)
            lastMarkPosition = nowD - 2.0 * period - 3.0;

        // Une marque est validee quand tout son grain (+/- P) est arrive
        while (lastMarkPosition + 2.0 * period + 3.0 <= nowD)
        {
            const double position = lastMarkPosition + period;
            marks[static_cast<size_t> (markCount % markCapacity)] = { position, period };
            ++markCount;
            lastMarkPosition = position;
        }
    }

    int64_t PsolaShifter::findNearestMark (double position) const noexcept
    {
        const int64_t available = juce::jmin (markCount, static_cast<int64_t> (markCapacity));
        int64_t best = markCount - 1;
        double bestDistance = 1.0e30;

        for (int64_t i = markCount - 1; i >= markCount - available; --i)
        {
            const double distance = std::abs (marks[static_cast<size_t> (i % markCapacity)].position - position);
            if (distance < bestDistance)
            {
                bestDistance = distance;
                best = i;
            }
            else if (marks[static_cast<size_t> (i % markCapacity)].position < position)
            {
                break;   // positions croissantes : on s'eloigne
            }
        }

        return best;
    }

    void PsolaShifter::placeGrain (const Mark& mark, double centre, float spacing, int64_t emitIndex) noexcept
    {
        const float F = formantRatio;
        const double halfLength = mark.period * juce::jmin (1.0f, 1.0f / F);   // demi-grain en sortie
        const auto gain = static_cast<float> (std::sqrt (spacing / halfLength));
        const double invHalf = 1.0 / halfLength;

        const auto first = juce::jmax (emitIndex + 1, static_cast<int64_t> (std::ceil (centre - halfLength)));
        const auto last = static_cast<int64_t> (std::floor (centre + halfLength));

        for (int64_t j = first; j <= last; ++j)
        {
            const double k = static_cast<double> (j) - centre;
            const auto w = gain * static_cast<float> (0.5 + 0.5 * std::cos (juce::MathConstants<double>::pi * k * invHalf));
            const double readPosition = mark.position + F * k;   // compression / etirement du grain = formants

            for (int ch = 0; ch < numChans; ++ch)
                output[static_cast<size_t> (ch)][static_cast<size_t> (j & ringMask)] += w * readInput (ch, readPosition);
        }
    }

    void PsolaShifter::scheduleGrains (int64_t emitIndex) noexcept
    {
        if (markCount == 0)
        {
            nextSynthesis = static_cast<double> (emitIndex + 1);
            return;
        }

        const bool identity = ! robot && std::abs (pitchRatio - 1.0f) < 1.0e-4f;
        const float F = formantRatio;

        for (int guard = 0; guard < maxGrainsPerSample; ++guard)
        {
            if (identity)
            {
                // Hauteur inchangee : les grains sont recolles exactement aux marques
                // d'analyse -> reconstruction parfaite (seuls les formants changent).
                if (nextMarkForIdentity < 0 || nextMarkForIdentity < markCount - markCapacity)
                    nextMarkForIdentity = findNearestMark (nextSynthesis);

                if (nextMarkForIdentity >= markCount)
                    break;

                const auto& mark = markAt (nextMarkForIdentity);
                const double halfLength = mark.period * juce::jmin (1.0f, 1.0f / F);

                if (mark.position - halfLength > static_cast<double> (emitIndex + 1))
                    break;

                const float spacing = nextMarkForIdentity + 1 < markCount
                    ? static_cast<float> (markAt (nextMarkForIdentity + 1).position - mark.position)
                    : mark.period;

                placeGrain (mark, mark.position, spacing, emitIndex);
                nextSynthesis = mark.position + mark.period;
                ++nextMarkForIdentity;
            }
            else
            {
                // Grain entierement dans le passe (reprise apres un changement) : on se recale
                if (nextSynthesis + maxPeriod < static_cast<double> (emitIndex))
                    nextSynthesis = static_cast<double> (emitIndex + 1);

                const auto& mark = markAt (findNearestMark (nextSynthesis));
                const double halfLength = mark.period * juce::jmin (1.0f, 1.0f / F);

                if (nextSynthesis - halfLength > static_cast<double> (emitIndex + 1))
                    break;

                const float spacing = robot ? static_cast<float> (sampleRate / robotHz)
                                            : mark.period / pitchRatio;

                placeGrain (mark, nextSynthesis, spacing, emitIndex);
                nextSynthesis += spacing;
                nextMarkForIdentity = -1;
            }
        }
    }

    //==============================================================================
    void PsolaShifter::process (juce::dsp::AudioBlock<float> block) noexcept
    {
        const auto numSamples = block.getNumSamples();
        const int channels = juce::jmin (numChans, static_cast<int> (block.getNumChannels()));
        const float monoScale = 1.0f / static_cast<float> (channels);

        for (size_t i = 0; i < numSamples; ++i)
        {
            const int64_t writeIndex = now;
            float mono = 0.0f;

            for (int ch = 0; ch < channels; ++ch)
            {
                const float x = block.getChannelPointer (static_cast<size_t> (ch))[i];
                input[static_cast<size_t> (ch)][static_cast<size_t> (writeIndex & ringMask)] = x;
                mono += x;
            }

            detector.push (mono * monoScale);
            if (++hopCounter >= analysisHop)
            {
                hopCounter = 0;
                detector.analyse();
            }

            ++now;
            createMarks();

            const int64_t emitIndex = writeIndex - latency;
            scheduleGrains (emitIndex);

            for (int ch = 0; ch < channels; ++ch)
            {
                auto& out = output[static_cast<size_t> (ch)][static_cast<size_t> (emitIndex & ringMask)];
                block.getChannelPointer (static_cast<size_t> (ch))[i] = emitIndex >= 0 ? out : 0.0f;
                out = 0.0f;
            }
        }
    }
}
