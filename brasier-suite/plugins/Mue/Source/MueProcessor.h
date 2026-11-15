#pragma once

#include "core/EffectProcessorBase.h"
#include "dsp/PsolaShifter.h"

namespace brasier::mue
{
    namespace ids
    {
        inline constexpr const char* pitch     = "pitch";
        inline constexpr const char* formant   = "formant";
        inline constexpr const char* robot     = "robot";
        inline constexpr const char* robotNote = "robotNote";
        inline constexpr const char* latency   = "latency";
    }

    /**
        Brasier Mue : pitch shift et formant shift independants, mode robot.

        Moteur TD-PSOLA (voir dsp/PsolaShifter.h) :
          - Pitch   +/-12 demi-tons : espacement des grains
          - Formant +/-12 demi-tons : compression / etirement de chaque grain
            (voix plus "petite" / plus "grosse" sans changer la note)
          - Robot : la sortie chante une note fixe quelle que soit l'entree
          - Latence Live (~20 ms, voix >= 100 Hz) ou Qualite (~34 ms, >= 60 Hz)
    */
    class MueProcessor final : public core::EffectProcessorBase
    {
    public:
        MueProcessor();

        bool hasEditor() const override { return true; }
        juce::AudioProcessorEditor* createEditor() override;

        static juce::StringArray getNoteNames();
        static constexpr int lowestRobotNote = 36;   // C2

        const dsp::PsolaShifter& getShifter() const noexcept { return shifter; }

    protected:
        void prepareEffect (const core::EffectSpec& spec) override;
        void resetEffect() override;
        void processEffect (juce::dsp::AudioBlock<float> block, const core::ProcessContext& ctx) override;
        int getEffectLatencySamples() const override    { return shifter.getLatencySamples(); }
        int getMaxEffectLatencySamples() const override { return shifter.getMaxLatencySamples(); }
        void updateEffectLatency() override;

    private:
        static juce::AudioProcessorValueTreeState::ParameterLayout createParameters();

        std::atomic<float>* pitchParam = nullptr;
        std::atomic<float>* formantParam = nullptr;
        std::atomic<float>* robotParam = nullptr;
        std::atomic<float>* robotNoteParam = nullptr;
        std::atomic<float>* latencyParam = nullptr;

        dsp::PsolaShifter shifter;
    };
}
