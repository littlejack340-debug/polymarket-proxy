# =============================================================================
#  brasier_add_plugin(<cible>
#      PRODUCT_NAME   "Brasier Xxx"      # nom affiche dans FL Studio
#      PLUGIN_CODE    Bxxx               # 4 caracteres, unique dans la suite
#      DESCRIPTION    "..."
#      VST3_CATEGORIES Fx Distortion ...
#      SOURCES        Source/a.cpp ...)
#
#  Cree le plugin (VST3 + Standalone par defaut) avec les reglages communs a
#  toute la suite et le lie aux bibliotheques partagees core / dsp / ui.
# =============================================================================

set(BRASIER_COMPANY_NAME      "Brasier Audio")
set(BRASIER_MANUFACTURER_CODE "Brsr")

# Reglages communs a tous les plugins (et aux tests), portes par une cible
# INTERFACE liee en PUBLIC pour atteindre aussi les wrappers VST3/Standalone.
add_library(brasier_options INTERFACE)
target_compile_definitions(brasier_options INTERFACE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1)
if(MSVC)
    # Sources en UTF-8 (accents dans les commentaires / textes d'interface)
    target_compile_options(brasier_options INTERFACE /utf-8)
endif()

function(brasier_add_plugin target)
    set(one_value_args PRODUCT_NAME PLUGIN_CODE DESCRIPTION)
    set(multi_value_args SOURCES VST3_CATEGORIES)
    cmake_parse_arguments(ARG "" "${one_value_args}" "${multi_value_args}" ${ARGN})

    if(NOT ARG_PRODUCT_NAME OR NOT ARG_PLUGIN_CODE)
        message(FATAL_ERROR "brasier_add_plugin(${target}) : PRODUCT_NAME et PLUGIN_CODE sont obligatoires")
    endif()

    if(NOT ARG_VST3_CATEGORIES)
        set(ARG_VST3_CATEGORIES Fx)
    endif()

    string(TOLOWER "${target}" target_lower)

    juce_add_plugin(${target}
        PRODUCT_NAME                "${ARG_PRODUCT_NAME}"
        COMPANY_NAME                "${BRASIER_COMPANY_NAME}"
        BUNDLE_ID                   "com.brasieraudio.${target_lower}"
        DESCRIPTION                 "${ARG_DESCRIPTION}"
        PLUGIN_MANUFACTURER_CODE    ${BRASIER_MANUFACTURER_CODE}
        PLUGIN_CODE                 ${ARG_PLUGIN_CODE}
        FORMATS                     ${BRASIER_FORMATS}
        IS_SYNTH                    FALSE
        NEEDS_MIDI_INPUT            FALSE
        NEEDS_MIDI_OUTPUT           FALSE
        IS_MIDI_EFFECT              FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        VST3_CATEGORIES             ${ARG_VST3_CATEGORIES}
        COPY_PLUGIN_AFTER_BUILD     ${BRASIER_COPY_VST3})

    target_sources(${target} PRIVATE ${ARG_SOURCES})

    target_link_libraries(${target}
        PRIVATE
            brasier::core
            brasier::dsp
            brasier::ui
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            brasier_options
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endfunction()
