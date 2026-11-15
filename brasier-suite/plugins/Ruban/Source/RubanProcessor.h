#pragma once

#include "core/EffectProcessorBase.h"
#include "dsp/DelayLine.h"
#include "dsp/EnvelopeFollower.h"
#include "dsp/LinearSmoother.h"
#include "dsp/Lfo.h"
#include "dsp/Svf.h"

namespace brasier::ruban
{
    namespace ids
    {
        inline constexpr const char* sync          = "sync";
        inline constexpr const char* time          = "time";
        inline constexpr const char* division      = "division";
        inline constexpr const char* feedback      = "feedback";
        inline constexpr const char* saturation    = "saturation";
        inline constexpr const char* lowCut        = "lowCut";
        inline constexpr const char* highCut       = "highCut";
        inline constexpr const char* wow           = "wow";
        inline constexpr const char* flutter       = "flutter";
        inline constexpr const char* mode          = "mode";
        inline constexpr const char* spread        = "spread";
        inline constexpr const char* duck          = "duck";
        inline constexpr const char* duckThreshold = "duckThreshold";
    }

    enum class StereoMode { mono = 0, stereo, pingPong };

    /**
        Brasier Ruban : delay a bande / analogique.

        Boucle de la "bande" (pour chaque canal) :

          entree --+--> [tete d'ecriture] ...... [tete de lecture : retard D module par wow/flutter]
                   ^                                              |
                   |                                              v
                   +---- feedback <-- saturation (xN) <-- high cut <-- low cut --> sortie wet --> ducking

        - La saturation est sur-echantillonnee DANS la boucle : chaque repetition
          repasse par la bande et se salit un peu plus, sans aliasing.
        - La latence des filtres d'oversampling est compensee en lisant la bande
          un peu plus tot : le plugin ne reporte aucune latence a l'hote.
        - Un changement de temps fait glisser la tete de lecture (glissement de
          pitch typique des echos a bande) au lieu de couper le son.
    */
    class RubanProcessor final : public core::EffectProcessorBase
    {
    public:
        RubanProcessor();

        bool hasEditor() const override { return true; }
        juce::AudioProcessorEditor* createEditor() override;

        static juce::StringArray getModeNames() { return { "Mono", "Stereo", "Ping-Pong" }; }

        static constexpr double minDelaySeconds = 0.01;
        static constexpr double maxDelaySeconds = 4.0;

    protected:
        void prepareEffect (const core::EffectSpec& spec) override;
        void resetEffect() override;
        void processEffect (juce::dsp::AudioBlock<float> block, const core::ProcessContext& ctx) override;
        float getStaticAutoGainDb() const override;

    private:
        static juce::AudioProcessorValueTreeState::ParameterLayout createParameters();

        double computeTargetDelaySeconds (const core::ProcessContext& ctx) const noexcept;
        void saturateLoop (juce::dsp::AudioBlock<float> loop, int numLoopChannels) noexcept;

        std::atomic<float>* syncParam = nullptr;
        std::atomic<float>* timeParam = nullptr;
        std::atomic<float>* divisionParam = nullptr;
        std::atomic<float>* feedbackParam = nullptr;
        std::atomic<float>* saturationParam = nullptr;
        std::atomic<float>* lowCutParam = nullptr;
        std::atomic<float>* highCutParam = nullptr;
        std::atomic<float>* wowParam = nullptr;
        std::atomic<float>* flutterParam = nullptr;
        std::atomic<float>* modeParam = nullptr;
        std::atomic<float>* spreadParam = nullptr;
        std::atomic<float>* duckParam = nullptr;
        std::atomic<float>* duckThresholdParam = nullptr;

        double baseRate = 44100.0;
        int subBlockSize = 16;

        dsp::DelayLine tape;                       // 2 canaux de "bande"
        juce::AudioBuffer<float> loopBuffer;       // signal lu, traite dans la boucle

        dsp::SmoothedSvf loopLowCut, loopHighCut;
        dsp::LinearSmoother<float> feedback, driveDb, spreadSamples, wowDepth, flutterDepth;
        double delaySamples = 0.0;                 // temps de retard lisse ("glissement" de la bande)
        float delayGlideCoeff = 0.0f;
        bool snapDelayTime = true;

        // Wow (lent, irregulier) et flutter (rapide), dephases a droite pour l'ampleur stereo
        dsp::Lfo wowSine[2], wowDrift[2], flutterA[2], flutterB[2];

        dsp::EnvelopeFollower duckEnvelope;
        float duckGain = 1.0f, duckTargetGain = 1.0f, duckSmoothCoeff = 0.0f;
    };
}
