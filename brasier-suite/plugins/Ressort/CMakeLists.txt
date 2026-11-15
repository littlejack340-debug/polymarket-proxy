# Brasier Ressort : reverb a ressort et a plaque.

brasier_add_plugin(BrasierRessort
    PRODUCT_NAME    "Brasier Ressort"
    PLUGIN_CODE     Bres
    DESCRIPTION     "Reverb a ressort et a plaque, couleur vintage"
    VST3_CATEGORIES Fx Reverb
    SOURCES
        Source/RessortProcessor.cpp
        Source/RessortProcessor.h
        Source/PluginEntry.cpp)
