#pragma once

#include "core/EffectProcessorBase.h"
#include "dsp/CutFilter.h"
#include "dsp/DelayLine.h"
#include "dsp/LinearSmoother.h"
#include "dsp/OnePole.h"
#include "dsp/Reverbs.h"

namespace brasier::ressort
{
    namespace ids
    {
        inline constexpr const char* type     = "type";
        inline constexpr const char* decay    = "decay";
        inline constexpr const char* predelay = "predelay";
        inline constexpr const char* damping  = "damping";
        inline constexpr const char* tension  = "tension";
        inline constexpr const char* lowCut   = "lowCut";
        inline constexpr const char* highCut  = "highCut";
        inline constexpr const char* drive    = "drive";
        inline constexpr const char* width    = "width";
    }

    enum class ReverbType { spring = 0, plate };

    /**
        Brasier Ressort : reverb a ressort et a plaque, couleur vintage.

        entree -> low/high cut -> drive (preampli a lampe, sur-echantillonne) -> somme mono
               -> pre-delay -> ressort OU plaque -> largeur stereo -> wet
    */
    class RessortProcessor final : public core::EffectProcessorBase
    {
    public:
        RessortProcessor();

        bool hasEditor() const override { return true; }
        juce::AudioProcessorEditor* createEditor() override;

    protected:
        void prepareEffect (const core::EffectSpec& spec) override;
        void resetEffect() override;
        void processEffect (juce::dsp::AudioBlock<float> block, const core::ProcessContext& ctx) override;
        float getStaticAutoGainDb() const override;

    private:
        static juce::AudioProcessorValueTreeState::ParameterLayout createParameters();

        std::atomic<float>* typeParam = nullptr;
        std::atomic<float>* decayParam = nullptr;
        std::atomic<float>* predelayParam = nullptr;
        std::atomic<float>* dampingParam = nullptr;
        std::atomic<float>* tensionParam = nullptr;
        std::atomic<float>* lowCutParam = nullptr;
        std::atomic<float>* highCutParam = nullptr;
        std::atomic<float>* driveParam = nullptr;
        std::atomic<float>* widthParam = nullptr;

        double baseRate = 44100.0;
        dsp::CutFilter lowCut, highCut;
        std::array<dsp::DcBlocker, 2> dcBlockers;
        dsp::DelayLine predelayLine;
        dsp::LinearSmoother<float> predelaySamples, driveDb, width;
        dsp::SpringReverb spring;
        dsp::PlateReverb plate;

        ReverbType activeType = ReverbType::spring;
        float typeGain = 1.0f, typeStep = 0.0f;
        bool switching = false;
    };
}
