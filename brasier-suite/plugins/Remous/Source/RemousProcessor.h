#pragma once

#include "core/EffectProcessorBase.h"
#include "dsp/AnalogModulator.h"

namespace brasier::remous
{
    namespace ids
    {
        inline constexpr const char* mode      = "mode";
        inline constexpr const char* rate      = "rate";
        inline constexpr const char* sync      = "sync";
        inline constexpr const char* division  = "division";
        inline constexpr const char* depth     = "depth";
        inline constexpr const char* feedback  = "feedback";
        inline constexpr const char* spread    = "spread";
        inline constexpr const char* shape     = "shape";
        inline constexpr const char* character = "character";
    }

    /**
        Brasier Remous : chorus, flanger, phaser et tremolo a caractere analogique
        (voir dsp/AnalogModulator.h). Vitesse libre ou synchronisee au tempo.
        A Mix 100 % chaque mode sonne comme la pedale d'origine ; baisser le Mix
        attenue l'effet.
    */
    class RemousProcessor final : public core::EffectProcessorBase
    {
    public:
        RemousProcessor();

        bool hasEditor() const override { return true; }
        juce::AudioProcessorEditor* createEditor() override;

    protected:
        void prepareEffect (const core::EffectSpec& spec) override;
        void resetEffect() override;
        void processEffect (juce::dsp::AudioBlock<float> block, const core::ProcessContext& ctx) override;

    private:
        static juce::AudioProcessorValueTreeState::ParameterLayout createParameters();
        dsp::AnalogModulator::Settings readSettings (const core::ProcessContext& ctx) const noexcept;

        std::atomic<float>* modeParam = nullptr;
        std::atomic<float>* rateParam = nullptr;
        std::atomic<float>* syncParam = nullptr;
        std::atomic<float>* divisionParam = nullptr;
        std::atomic<float>* depthParam = nullptr;
        std::atomic<float>* feedbackParam = nullptr;
        std::atomic<float>* spreadParam = nullptr;
        std::atomic<float>* shapeParam = nullptr;
        std::atomic<float>* characterParam = nullptr;

        dsp::AnalogModulator modulator;
    };
}
