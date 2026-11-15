#pragma once

#include "core/EffectProcessorBase.h"
#include "dsp/LinearSmoother.h"

namespace brasier::gain
{
    namespace ids
    {
        inline constexpr const char* gain = "gain";
        inline constexpr const char* clip = "clip";
    }

    /**
        Plugin de test : gain lisse + soft-clip tanh optionnel sur-echantillonne.

        Le chemin d'oversampling est toujours actif (meme clip coupe) pour que la
        latence reportee a l'hote reste constante quel que soit le reglage.
    */
    class GainProcessor final : public core::EffectProcessorBase
    {
    public:
        GainProcessor();

        bool hasEditor() const override { return true; }
        juce::AudioProcessorEditor* createEditor() override;

    protected:
        void prepareEffect (const core::EffectSpec& spec) override;
        void resetEffect() override;
        void processEffect (juce::dsp::AudioBlock<float> block, const core::ProcessContext& ctx) override;

    private:
        static juce::AudioProcessorValueTreeState::ParameterLayout createParameters();

        std::atomic<float>* gainParam = nullptr;
        std::atomic<float>* clipParam = nullptr;

        dsp::LinearSmoother<float> gainSmoother;   // gain lineaire, a la frequence de base
        dsp::LinearSmoother<float> clipSmoother;   // 0 = propre, 1 = soft-clip, a la frequence sur-echantillonnee
    };
}
