# =============================================================================
#  Brasier Suite : suite d'effets audio VST3 (JUCE 8, C++20)
# =============================================================================
cmake_minimum_required(VERSION 3.22)

project(BrasierSuite VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Runtime MSVC statique (/MT) : les .vst3 ne dependent pas du redistribuable VC++.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Dossiers Visual Studio propres (JUCE range ses cibles dans des sous-dossiers)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# -----------------------------------------------------------------------------
#  Options
# -----------------------------------------------------------------------------
set(BRASIER_FORMATS VST3 Standalone CACHE STRING "Formats de plugin a construire")
option(BRASIER_COPY_VST3 "Copier les .vst3 dans le dossier VST3 systeme apres le build (droits admin requis sous Windows)" OFF)
option(BRASIER_BUILD_TESTS "Construire les tests hors-ligne (console)" OFF)
set(BRASIER_JUCE_TAG "8.0.17-security" CACHE STRING "Tag git de JUCE a recuperer")

# -----------------------------------------------------------------------------
#  JUCE via FetchContent
#  Astuce : -DFETCHCONTENT_SOURCE_DIR_JUCE=C:/chemin/vers/JUCE pour utiliser
#  une copie locale et eviter le telechargement.
# -----------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        ${BRASIER_JUCE_TAG}
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE)
FetchContent_MakeAvailable(JUCE)

# -----------------------------------------------------------------------------
#  Bibliotheques partagees + plugins
# -----------------------------------------------------------------------------
include(cmake/BrasierPlugin.cmake)

add_subdirectory(shared)
add_subdirectory(plugins/Gain)
add_subdirectory(plugins/Fournaise)
add_subdirectory(plugins/Ruban)

if(BRASIER_BUILD_TESTS)
    add_subdirectory(tests)
endif()
