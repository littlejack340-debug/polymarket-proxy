#pragma once

#include <vector>

#include "core/EffectProcessorBase.h"
#include "dsp/EnvelopeFollower.h"
#include "dsp/LinearSmoother.h"
#include "dsp/Lfo.h"
#include "dsp/ResonantFilter.h"

namespace brasier::tamis
{
    namespace ids
    {
        inline constexpr const char* type        = "type";
        inline constexpr const char* slope       = "slope";
        inline constexpr const char* cutoff      = "cutoff";
        inline constexpr const char* resonance   = "resonance";
        inline constexpr const char* drive       = "drive";
        inline constexpr const char* lfoAmount   = "lfoAmount";
        inline constexpr const char* lfoShape    = "lfoShape";
        inline constexpr const char* lfoSync     = "lfoSync";
        inline constexpr const char* lfoRate     = "lfoRate";
        inline constexpr const char* lfoDivision = "lfoDivision";
        inline constexpr const char* envAmount   = "envAmount";
        inline constexpr const char* envAttack   = "envAttack";
        inline constexpr const char* envRelease  = "envRelease";
    }

    /**
        Brasier Tamis : filtre multimode a resonance saturee.

        frequence = cutoff x 2^(LFO x profondeur + enveloppe x profondeur)   (+/-4 octaves chacun)

        - LFO libre (Hz) ou synchronise (divisions de mesure), cale sur le transport
        - Envelope follower sur l'entree : le filtre s'ouvre quand le son tape
        - Drive avant le filtre et resonance qui sature : sur-echantillonnes
    */
    class TamisProcessor final : public core::EffectProcessorBase
    {
    public:
        TamisProcessor();

        bool hasEditor() const override { return true; }
        juce::AudioProcessorEditor* createEditor() override;

    protected:
        void prepareEffect (const core::EffectSpec& spec) override;
        void resetEffect() override;
        void processEffect (juce::dsp::AudioBlock<float> block, const core::ProcessContext& ctx) override;

    private:
        static juce::AudioProcessorValueTreeState::ParameterLayout createParameters();

        std::atomic<float>* typeParam = nullptr;
        std::atomic<float>* slopeParam = nullptr;
        std::atomic<float>* cutoffParam = nullptr;
        std::atomic<float>* resonanceParam = nullptr;
        std::atomic<float>* driveParam = nullptr;
        std::atomic<float>* lfoAmountParam = nullptr;
        std::atomic<float>* lfoShapeParam = nullptr;
        std::atomic<float>* lfoSyncParam = nullptr;
        std::atomic<float>* lfoRateParam = nullptr;
        std::atomic<float>* lfoDivisionParam = nullptr;
        std::atomic<float>* envAmountParam = nullptr;
        std::atomic<float>* envAttackParam = nullptr;
        std::atomic<float>* envReleaseParam = nullptr;

        double baseRate = 44100.0;

        dsp::ResonantFilter filter;
        dsp::Lfo lfo;
        dsp::EnvelopeFollower envelope;
        dsp::LinearSmoother<float> logCutoff, resonance, driveDb, lfoDepth, envDepth;

        // Coefficients calcules a la frequence de base, reutilises pour les sous-echantillons
        std::vector<dsp::ResonantFilter::Coeffs> coeffs;

        // Changement de type / pente : fondu sortant puis entrant (pas de clic)
        dsp::ResonantFilter::Mode activeMode = dsp::ResonantFilter::Mode::lowpass;
        bool activeSteep = false;
        float switchGain = 1.0f;
        float switchStep = 0.0f;
        bool switching = false;
    };
}
