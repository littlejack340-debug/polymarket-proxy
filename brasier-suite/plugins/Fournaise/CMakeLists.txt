# Brasier Fournaise : saturateur analogique (tube, transistor, bande, diode).

brasier_add_plugin(BrasierFournaise
    PRODUCT_NAME    "Brasier Fournaise"
    PLUGIN_CODE     Bfou
    DESCRIPTION     "Saturateur analogique multi-modeles"
    VST3_CATEGORIES Fx Distortion
    SOURCES
        Source/FournaiseProcessor.cpp
        Source/FournaiseProcessor.h
        Source/PluginEntry.cpp)
