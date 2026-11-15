# Brasier Tamis : filtre multimode resonant, LFO synchronise et envelope follower.

brasier_add_plugin(BrasierTamis
    PRODUCT_NAME    "Brasier Tamis"
    PLUGIN_CODE     Btam
    DESCRIPTION     "Filtre multimode a resonance saturee, LFO et enveloppe"
    VST3_CATEGORIES Fx Filter
    SOURCES
        Source/TamisProcessor.cpp
        Source/TamisProcessor.h
        Source/PluginEntry.cpp)
