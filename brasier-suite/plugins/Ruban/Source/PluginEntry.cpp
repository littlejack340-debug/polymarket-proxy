#include "RubanProcessor.h"

// Point d'entree appele par les wrappers JUCE (VST3, Standalone)
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new brasier::ruban::RubanProcessor();
}
