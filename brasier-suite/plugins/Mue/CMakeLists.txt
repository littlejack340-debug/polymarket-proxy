# Brasier Mue : pitch shift / formant shift independants, mode robot.

brasier_add_plugin(BrasierMue
    PRODUCT_NAME    "Brasier Mue"
    PLUGIN_CODE     Bmue
    DESCRIPTION     "Pitch et formant independants, mode robot, faible latence"
    VST3_CATEGORIES Fx Pitch Shift
    SOURCES
        Source/MueProcessor.cpp
        Source/MueProcessor.h
        Source/PluginEntry.cpp)
