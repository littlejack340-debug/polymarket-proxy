#include "MueProcessor.h"

#include "ui/GenericKnobEditor.h"

namespace brasier::mue
{
    namespace
    {
        core::EffectProcessorBase::Options makeOptions()
        {
            core::EffectProcessorBase::Options o;
            o.name = "Brasier Mue";
            o.common.withOversampling = false;   // aucun etage non lineaire
            o.mixLaw = dsp::DryWetMixer::MixLaw::equalPower;   // dry et voix decalee sont decorreles
            o.autoGainMode = core::EffectProcessorBase::AutoGainMode::measured;
            return o;
        }

        juce::String formatSemitones (float v)
        {
            return (v > 0.0f ? "+" : "") + juce::String (v, 2) + " st";
        }
    }

    juce::StringArray MueProcessor::getNoteNames()
    {
        static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        juce::StringArray result;
        for (int note = lowestRobotNote; note <= lowestRobotNote + 36; ++note)
            result.add (juce::String (names[note % 12]) + juce::String (note / 12 - 1));
        return result;
    }

    juce::AudioProcessorValueTreeState::ParameterLayout MueProcessor::createParameters()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;

        auto semitoneParameter = [] (const char* id, const char* name)
        {
            return std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { id, core::parameterVersion }, name,
                juce::NormalisableRange<float> (-12.0f, 12.0f, 0.01f), 0.0f,
                juce::AudioParameterFloatAttributes()
                    .withLabel ("st")
                    .withStringFromValueFunction ([] (float v, int) { return formatSemitones (v); })
                    .withValueFromStringFunction ([] (const juce::String& t) { return t.getFloatValue(); }));
        };

        layout.add (semitoneParameter (ids::pitch, "Pitch"));
        layout.add (semitoneParameter (ids::formant, "Formant"));
        layout.add (core::makeBoolParameter (ids::robot, "Robot", false));
        layout.add (core::makeChoiceParameter (ids::robotNote, "Robot Note", getNoteNames(), 12));   // C3
        layout.add (core::makeChoiceParameter (ids::latency, "Latency", { "Live (20 ms)", "Qualite (34 ms)" }, 0, false));
        return layout;
    }

    MueProcessor::MueProcessor()
        : core::EffectProcessorBase (makeOptions(), createParameters())
    {
        pitchParam     = apvts.getRawParameterValue (ids::pitch);
        formantParam   = apvts.getRawParameterValue (ids::formant);
        robotParam     = apvts.getRawParameterValue (ids::robot);
        robotNoteParam = apvts.getRawParameterValue (ids::robotNote);
        latencyParam   = apvts.getRawParameterValue (ids::latency);
    }

    juce::AudioProcessorEditor* MueProcessor::createEditor()
    {
        ui::GenericKnobEditor::Options o;
        o.title = "BRASIER  MUE";
        o.subtitle = "pitch / formant";
        o.footerParameterIds = core::getCommonParameterIds();
        o.hiddenParameterIds = { core::ids::bypass };
        return new ui::GenericKnobEditor (*this, apvts, o);
    }

    void MueProcessor::prepareEffect (const core::EffectSpec& spec)
    {
        shifter.prepare (spec.sampleRate, spec.numChannels);
        updateEffectLatency();
    }

    void MueProcessor::resetEffect()
    {
        shifter.reset();
    }

    void MueProcessor::updateEffectLatency()
    {
        const auto mode = latencyParam->load() > 0.5f ? dsp::PsolaShifter::LatencyMode::quality
                                                      : dsp::PsolaShifter::LatencyMode::live;
        if (mode != shifter.getLatencyMode())
            shifter.setLatencyMode (mode);
    }

    void MueProcessor::processEffect (juce::dsp::AudioBlock<float> block, const core::ProcessContext&)
    {
        const int note = lowestRobotNote + static_cast<int> (robotNoteParam->load());
        const auto robotHz = static_cast<float> (440.0 * std::pow (2.0, (note - 69) / 12.0));

        shifter.setPitchRatio (std::exp2 (pitchParam->load() / 12.0f));
        shifter.setFormantRatio (std::exp2 (formantParam->load() / 12.0f));
        shifter.setRobot (robotParam->load() > 0.5f, robotHz);
        shifter.process (block);
    }
}
