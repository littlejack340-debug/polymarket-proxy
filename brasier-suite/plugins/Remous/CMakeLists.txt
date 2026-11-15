# Brasier Remous : chorus, flanger, phaser, tremolo analogiques.

brasier_add_plugin(BrasierRemous
    PRODUCT_NAME    "Brasier Remous"
    PLUGIN_CODE     Brem
    DESCRIPTION     "Chorus, flanger, phaser et tremolo a caractere analogique"
    VST3_CATEGORIES Fx Modulation
    SOURCES
        Source/RemousProcessor.cpp
        Source/RemousProcessor.h
        Source/PluginEntry.cpp)
