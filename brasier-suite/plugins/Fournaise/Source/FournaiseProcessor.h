#pragma once

#include <array>

#include "core/EffectProcessorBase.h"
#include "dsp/CutFilter.h"
#include "dsp/LinearSmoother.h"
#include "dsp/OnePole.h"
#include "dsp/Svf.h"

namespace brasier::fournaise
{
    namespace ids
    {
        inline constexpr const char* model      = "model";
        inline constexpr const char* drive      = "drive";
        inline constexpr const char* tone       = "tone";
        inline constexpr const char* punish     = "punish";
        inline constexpr const char* preLowCut  = "preLowCut";
        inline constexpr const char* preHighCut = "preHighCut";
        inline constexpr const char* postLowCut = "postLowCut";
        inline constexpr const char* postHighCut= "postHighCut";
    }

    enum class Model { tube = 0, transistor, tape, diode };

    /**
        Brasier Fournaise : saturateur analogique.

        entree -> low/high cut PRE -> [ x N : punish (cloche) -> drive -> modele -> compensation ] -> DC block
               -> tone (tilt) -> low/high cut POST -> sortie

        Les filtres "pre" choisissent ce qui sature (ex. couper le grave avant
        de saturer une 808 garde le sub propre), les filtres "post" nettoient le
        resultat. Seul l'etage non lineaire tourne sur-echantillonne.
    */
    class FournaiseProcessor final : public core::EffectProcessorBase
    {
    public:
        FournaiseProcessor();

        bool hasEditor() const override { return true; }
        juce::AudioProcessorEditor* createEditor() override;

        static juce::StringArray getModelNames() { return { "Tube", "Transistor", "Bande", "Diode" }; }

    protected:
        void prepareEffect (const core::EffectSpec& spec) override;
        void resetEffect() override;
        void processEffect (juce::dsp::AudioBlock<float> block, const core::ProcessContext& ctx) override;

    private:
        static juce::AudioProcessorValueTreeState::ParameterLayout createParameters();

        void setOversampledRate (double rate) noexcept;
        void updateTargets() noexcept;
        void saturate (juce::dsp::AudioBlock<float> up) noexcept;
        float shape (Model m, int channel, float x) noexcept;

        // Parametres
        std::atomic<float>* modelParam = nullptr;
        std::atomic<float>* driveParam = nullptr;
        std::atomic<float>* toneParam = nullptr;
        std::atomic<float>* punishParam = nullptr;
        std::atomic<float>* preLowParam = nullptr;
        std::atomic<float>* preHighParam = nullptr;
        std::atomic<float>* postLowParam = nullptr;
        std::atomic<float>* postHighParam = nullptr;

        double baseRate = 44100.0;
        double osRate = 44100.0;

        // Frequence de base
        dsp::CutFilter preLow, preHigh, postLow, postHigh;
        dsp::SmoothedSvf tiltLow, tiltHigh;
        std::array<dsp::DcBlocker, 2> dcBlockers;

        // Frequence sur-echantillonnee
        dsp::SmoothedSvf punishBell;
        dsp::SmoothedSvf tapePreEmphasis, tapeDeEmphasis;
        dsp::LinearSmoother<float> driveDb, punishAmount, modelFade;
        Model currentModel = Model::tube;
        Model previousModel = Model::tube;
        bool tapeFiltersActive = false;
    };
}
