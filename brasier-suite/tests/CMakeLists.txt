# Tests hors-ligne (console) : instancient les processeurs sans hote et
# verifient latence, mix, bypass, auto-gain, robustesse et sauvegarde d'etat.
#
#   cmake -S . -B build -DBRASIER_BUILD_TESTS=ON
#   cmake --build build --config Release --target BrasierTests
#   build/tests/BrasierTests_artefacts/Release/BrasierTests(.exe)

juce_add_console_app(BrasierTests PRODUCT_NAME "BrasierTests")

target_sources(BrasierTests PRIVATE
    BrasierTests.cpp
    ${PROJECT_SOURCE_DIR}/plugins/Gain/Source/GainProcessor.cpp)

target_include_directories(BrasierTests PRIVATE ${PROJECT_SOURCE_DIR}/plugins)

target_link_libraries(BrasierTests
    PRIVATE
        brasier::core
        brasier::dsp
        brasier::ui
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        brasier_options
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

# runDispatchLoopUntil() (laisser le Timer reporter la latence) l'exige
target_compile_definitions(BrasierTests PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1)
