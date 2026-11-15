#include "PresetManager.h"
#include "CommonParameters.h"

namespace brasier::core
{
    PresetManager::PresetManager (juce::AudioProcessorValueTreeState& s,
                                  juce::String name,
                                  std::vector<FactoryPreset> factoryPresets)
        : state (s), pluginName (std::move (name)), factory (std::move (factoryPresets))
    {
    }

    juce::File PresetManager::getUserDirectory() const
    {
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
            .getChildFile ("Brasier Audio")
            .getChildFile (pluginName)
            .getChildFile ("Presets");
    }

    void PresetManager::refreshUserPresets()
    {
        userPresets.clear();
        const auto dir = getUserDirectory();
        if (dir.isDirectory())
        {
            userPresets = dir.findChildFiles (juce::File::findFiles, false, juce::String ("*") + fileExtension);
            userPresets.sort();
        }
    }

    juce::StringArray PresetManager::getPresetNames() const
    {
        juce::StringArray names;
        for (const auto& p : factory)
            names.add (p.category.isNotEmpty() ? p.category + " - " + p.name : p.name);
        for (const auto& f : userPresets)
            names.add ("Perso - " + f.getFileNameWithoutExtension());
        return names;
    }

    juce::String PresetManager::getCurrentName() const
    {
        const auto names = getPresetNames();
        return juce::isPositiveAndBelow (currentIndex, names.size()) ? names[currentIndex] : juce::String::fromUTF8 ("Réglage libre");
    }

    bool PresetManager::isProtectedParameter (const juce::String& id) const
    {
        return id == ids::oversampling || id == ids::bypass;
    }

    void PresetManager::resetPluginParametersToDefault()
    {
        for (auto* p : state.processor.getParameters())
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
                if (! isProtectedParameter (ranged->getParameterID()))
                    ranged->setValueNotifyingHost (ranged->getDefaultValue());
    }

    void PresetManager::loadFactoryPreset (int index)
    {
        if (! juce::isPositiveAndBelow (index, getNumFactoryPresets()))
            return;

        resetPluginParametersToDefault();

        for (const auto& [id, value] : factory[static_cast<size_t> (index)].values)
            if (auto* p = state.getParameter (id))
                if (! isProtectedParameter (id))
                    p->setValueNotifyingHost (p->convertTo0to1 (value));

        currentIndex = index;
    }

    bool PresetManager::loadUserPreset (const juce::File& file)
    {
        const auto xml = juce::XmlDocument::parse (file);
        if (xml == nullptr || ! xml->hasTagName ("BrasierPreset"))
            return false;

        resetPluginParametersToDefault();

        for (const auto* child : xml->getChildWithTagNameIterator ("Param"))
        {
            const auto id = child->getStringAttribute ("id");
            if (auto* p = state.getParameter (id))
                if (! isProtectedParameter (id))
                    p->setValueNotifyingHost (p->convertTo0to1 (static_cast<float> (child->getDoubleAttribute ("value"))));
        }

        const auto userIndex = userPresets.indexOf (file);
        currentIndex = userIndex >= 0 ? getNumFactoryPresets() + userIndex : -1;
        return true;
    }

    bool PresetManager::saveUserPreset (const juce::String& name)
    {
        const auto clean = juce::File::createLegalFileName (name.trim());
        if (clean.isEmpty())
            return false;

        const auto dir = getUserDirectory();
        if (! dir.createDirectory())
            return false;

        juce::XmlElement xml ("BrasierPreset");
        xml.setAttribute ("plugin", pluginName);
        xml.setAttribute ("version", 1);

        for (auto* p : state.processor.getParameters())
        {
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
            {
                if (isProtectedParameter (ranged->getParameterID()))
                    continue;
                auto* child = xml.createNewChildElement ("Param");
                child->setAttribute ("id", ranged->getParameterID());
                child->setAttribute ("value", static_cast<double> (ranged->convertFrom0to1 (ranged->getValue())));
            }
        }

        const auto file = dir.getChildFile (clean + fileExtension);
        if (! xml.writeTo (file))
            return false;

        refreshUserPresets();
        currentIndex = getNumFactoryPresets() + userPresets.indexOf (file);
        return true;
    }

    void PresetManager::loadPreset (int index)
    {
        if (isFactory (index))
            loadFactoryPreset (index);
        else if (juce::isPositiveAndBelow (index - getNumFactoryPresets(), userPresets.size()))
            loadUserPreset (userPresets[index - getNumFactoryPresets()]);
    }

    void PresetManager::loadNext()
    {
        const int n = getNumPresets();
        if (n > 0)
            loadPreset ((currentIndex + 1 + n) % n);
    }

    void PresetManager::loadPrevious()
    {
        const int n = getNumPresets();
        if (n > 0)
            loadPreset ((currentIndex - 1 + n) % n);
    }
}
