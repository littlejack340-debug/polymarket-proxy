#include "CommonParameters.h"

namespace brasier::core
{
    namespace
    {
        juce::ParameterID pid (const juce::String& id) { return { id, parameterVersion }; }

        juce::String formatDb (float value)
        {
            if (value <= -99.9f)
                return "-inf dB";

            return (value > 0.0f ? "+" : "") + juce::String (value, 1) + " dB";
        }

        juce::String formatHz (float value)
        {
            if (value >= 1000.0f)
                return juce::String (value / 1000.0f, value >= 10000.0f ? 1 : 2) + " kHz";

            return juce::String (juce::roundToInt (value)) + " Hz";
        }

        float parseHz (const juce::String& text)
        {
            const auto t = text.trim().toLowerCase();
            const auto v = t.getFloatValue();
            return t.contains ("k") ? v * 1000.0f : v;
        }
    }

    std::unique_ptr<juce::AudioParameterFloat> makeDecibelParameter (const juce::String& id, const juce::String& name,
                                                                     float minDb, float maxDb, float defaultDb)
    {
        return std::make_unique<juce::AudioParameterFloat> (
            pid (id), name,
            juce::NormalisableRange<float> (minDb, maxDb, 0.01f),
            defaultDb,
            juce::AudioParameterFloatAttributes()
                .withLabel ("dB")
                .withStringFromValueFunction ([] (float v, int) { return formatDb (v); })
                .withValueFromStringFunction ([] (const juce::String& t) { return t.getFloatValue(); }));
    }

    std::unique_ptr<juce::AudioParameterFloat> makePercentParameter (const juce::String& id, const juce::String& name,
                                                                     float defaultPercent)
    {
        return std::make_unique<juce::AudioParameterFloat> (
            pid (id), name,
            juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
            defaultPercent,
            juce::AudioParameterFloatAttributes()
                .withLabel ("%")
                .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + " %"; })
                .withValueFromStringFunction ([] (const juce::String& t) { return t.getFloatValue(); }));
    }

    std::unique_ptr<juce::AudioParameterFloat> makeFrequencyParameter (const juce::String& id, const juce::String& name,
                                                                       float minHz, float maxHz, float defaultHz)
    {
        // Echelle logarithmique : le milieu de course tombe sur la moyenne geometrique.
        juce::NormalisableRange<float> range (minHz, maxHz);
        range.setSkewForCentre (std::sqrt (minHz * maxHz));

        return std::make_unique<juce::AudioParameterFloat> (
            pid (id), name, range, defaultHz,
            juce::AudioParameterFloatAttributes()
                .withLabel ("Hz")
                .withStringFromValueFunction ([] (float v, int) { return formatHz (v); })
                .withValueFromStringFunction ([] (const juce::String& t) { return parseHz (t); }));
    }

    std::unique_ptr<juce::AudioParameterChoice> makeChoiceParameter (const juce::String& id, const juce::String& name,
                                                                     const juce::StringArray& choices, int defaultIndex,
                                                                     bool automatable)
    {
        return std::make_unique<juce::AudioParameterChoice> (
            pid (id), name, choices, defaultIndex,
            juce::AudioParameterChoiceAttributes().withAutomatable (automatable));
    }

    std::unique_ptr<juce::AudioParameterBool> makeBoolParameter (const juce::String& id, const juce::String& name,
                                                                 bool defaultValue)
    {
        return std::make_unique<juce::AudioParameterBool> (
            pid (id), name, defaultValue,
            juce::AudioParameterBoolAttributes()
                .withStringFromValueFunction ([] (bool v, int) { return v ? juce::String ("On") : juce::String ("Off"); }));
    }

    //==============================================================================
    void addCommonParameters (juce::AudioProcessorValueTreeState::ParameterLayout& layout,
                              const CommonParameterOptions& options)
    {
        layout.add (makePercentParameter (ids::mix, "Mix", options.defaultMixPercent));
        layout.add (makeDecibelParameter (ids::output, "Output", -24.0f, 24.0f, 0.0f));
        layout.add (makeBoolParameter (ids::autoGain, "Auto Gain", false));

        if (options.withOversampling)
        {
            // Non automatisable : changer de facteur modifie la latence du plugin.
            layout.add (makeChoiceParameter (ids::oversampling, "Oversampling",
                                             { "1x", "2x", "4x", "8x" },
                                             juce::jlimit (0, 3, options.defaultOversamplingIndex),
                                             false));
        }

        layout.add (makeBoolParameter (ids::bypass, "Bypass", false));
    }

    juce::StringArray getCommonParameterIds()
    {
        return { ids::mix, ids::output, ids::autoGain, ids::oversampling, ids::bypass };
    }
}
