# =============================================================================
#  Bibliotheques partagees de la suite.
#
#  Ce sont des cibles INTERFACE : leurs .cpp sont compiles dans chaque plugin
#  qui les utilise. C'est la methode recommandee avec JUCE, car les modules JUCE
#  sont eux-memes compiles par cible : une vraie lib statique dupliquerait JUCE.
#
#    brasier::dsp  -> algorithmes DSP purs (aucune dependance a AudioProcessor)
#    brasier::core -> socle commun des plugins (parametres, mix, latence...)
#    brasier::ui   -> LookAndFeel et composants d'interface communs
# =============================================================================

# ---- DSP ---------------------------------------------------------------------
add_library(brasier_dsp INTERFACE)
add_library(brasier::dsp ALIAS brasier_dsp)

target_sources(brasier_dsp INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/dsp/AutoGain.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/dsp/DryWetMixer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/dsp/Oversampler.cpp)

target_include_directories(brasier_dsp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(brasier_dsp INTERFACE juce::juce_dsp)

# ---- Core --------------------------------------------------------------------
add_library(brasier_core INTERFACE)
add_library(brasier::core ALIAS brasier_core)

target_sources(brasier_core INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/core/CommonParameters.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/core/EffectProcessorBase.cpp)

target_include_directories(brasier_core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(brasier_core INTERFACE brasier::dsp juce::juce_audio_processors)

# ---- UI ----------------------------------------------------------------------
add_library(brasier_ui INTERFACE)
add_library(brasier::ui ALIAS brasier_ui)

target_sources(brasier_ui INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/BrasierLookAndFeel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/GenericKnobEditor.cpp)

target_include_directories(brasier_ui INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(brasier_ui INTERFACE juce::juce_audio_processors juce::juce_gui_basics)
