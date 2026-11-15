# Brasier Gain : plugin de test de l'etape 1 (gain + soft-clip optionnel).
# Il sert a valider toute la chaine commune : parametres, lissage, mix,
# auto-gain, oversampling et compensation de latence dans FL Studio.

brasier_add_plugin(BrasierGain
    PRODUCT_NAME    "Brasier Gain"
    PLUGIN_CODE     Bgai
    DESCRIPTION     "Gain utilitaire avec soft-clip sur-echantillonne"
    VST3_CATEGORIES Fx Tools
    SOURCES
        Source/GainProcessor.cpp
        Source/GainProcessor.h
        Source/PluginEntry.cpp)
