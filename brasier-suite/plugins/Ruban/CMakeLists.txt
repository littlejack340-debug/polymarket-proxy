# Brasier Ruban : delay a bande / analogique.

brasier_add_plugin(BrasierRuban
    PRODUCT_NAME    "Brasier Ruban"
    PLUGIN_CODE     Brub
    DESCRIPTION     "Delay a bande avec saturation, wow/flutter et ducking"
    VST3_CATEGORIES Fx Delay
    SOURCES
        Source/RubanProcessor.cpp
        Source/RubanProcessor.h
        Source/PluginEntry.cpp)
