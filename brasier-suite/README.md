# Brasier Suite

Suite d'effets audio originale au format **VST3** (+ Standalone) pour FL Studio sous Windows,
écrite en **C++20** avec **JUCE 8** et **CMake**. Couleur analogique, pensée pour la production rap.

| Plugin | Rôle | État |
|---|---|---|
| **Brasier Gain** | gain + soft-clip, banc d'essai de l'infrastructure commune | ✅ étape 1 |
| **Brasier Fournaise** | saturateur multi-modèles (tube, transistor, bande, diode) | étape 2 |
| **Brasier Ruban** | delay à bande / analogique, sync tempo, ping-pong, ducking | étape 3 |
| **Brasier Mue** | pitch shift et formant shift indépendants, mode robot | étape 4 |
| **Brasier Tamis** | filtre multimode avec résonance saturée, LFO et envelope follower | étape 5 |
| **Brasier Remous** | chorus, flanger, phaser, tremolo | étape 5 |
| **Brasier Ressort** | reverb à ressort et plate | étape 5 |

---

## 1. Prérequis (Windows)

| Outil | Version | Installation |
|---|---|---|
| Visual Studio 2022 (Community suffit) | 17.x, charge de travail **« Développement Desktop en C++ »** (MSVC v143 + Windows SDK) | `winget install Microsoft.VisualStudio.2022.Community --override "--add Microsoft.VisualStudio.Workload.NativeDesktop --includeRecommended --passive"` |
| CMake | **3.22 minimum** (exigé par JUCE 8) | `winget install Kitware.CMake` |
| Git | toute version récente | `winget install Git.Git` |

Pour tout vérifier d'un coup (depuis le dossier `brasier-suite`) :

```powershell
powershell -ExecutionPolicy Bypass -File scripts\check-prereqs.ps1
```

> JUCE n'est pas à installer : CMake le télécharge tout seul (tag `8.0.17-security`) au premier build.
> Place le projet dans un chemin court (ex. `C:\dev\`) pour éviter la limite de 260 caractères de Windows.

## 2. Compiler

Méthode simple (script) :

```powershell
cd C:\dev\polymarket-proxy\brasier-suite
powershell -ExecutionPolicy Bypass -File scripts\build.ps1            # Release
powershell -ExecutionPolicy Bypass -File scripts\build.ps1 -Tests     # + tests hors-ligne
```

Commandes CMake équivalentes :

```powershell
cmake --preset vs2022                      # configure (télécharge JUCE la 1re fois)
cmake --build --preset release             # compile en Release
```

ou sans preset :

```powershell
cmake -S . -B build\vs2022 -G "Visual Studio 17 2022" -A x64
cmake --build build\vs2022 --config Release --parallel
```

Tu peux aussi ouvrir `build\vs2022\BrasierSuite.sln` dans Visual Studio.

Résultats :

```
build\vs2022\plugins\Gain\BrasierGain_artefacts\Release\VST3\Brasier Gain.vst3        <- dossier du plugin
build\vs2022\plugins\Gain\BrasierGain_artefacts\Release\Standalone\Brasier Gain.exe   <- appli autonome
```

## 3. Installer dans FL Studio

Le `.vst3` est un **dossier** (bundle) : copie le dossier entier `Brasier Gain.vst3` dans
`C:\Program Files\Common Files\VST3\`. Il faut les droits administrateur, et FL Studio doit être fermé.

```powershell
powershell -ExecutionPolicy Bypass -File scripts\install-vst3.ps1     # se relance en admin tout seul
```

Ou à la main, dans un PowerShell **administrateur** :

```powershell
Copy-Item -Recurse -Force "build\vs2022\plugins\Gain\BrasierGain_artefacts\Release\VST3\Brasier Gain.vst3" "C:\Program Files\Common Files\VST3\"
```

Puis dans FL Studio : *Options > Manage plugins > Find installed plugins*. Le plugin apparaît sous **Brasier Audio**.

Option : `cmake --preset vs2022 -DBRASIER_COPY_VST3=ON` copie automatiquement après chaque build
(il faut alors lancer le terminal ou Visual Studio en administrateur).

## 4. Structure

```
brasier-suite/
├── CMakeLists.txt            projet racine : JUCE (FetchContent), options, sous-dossiers
├── CMakePresets.json         presets vs2022 / vs2026 / ninja
├── cmake/BrasierPlugin.cmake brasier_add_plugin() : réglages communs à tous les plugins
├── shared/
│   ├── dsp/                  DSP pur, sans dépendance à AudioProcessor
│   │   ├── LinearSmoother.h      lissage des paramètres
│   │   ├── DryWetMixer.*         mix dry/wet compensé en latence (loi linéaire ou puissance constante)
│   │   ├── AutoGain.*            égalisation automatique du niveau RMS
│   │   ├── Oversampler.*         oversampling 1x/2x/4x/8x commutable sans allocation
│   │   └── MathUtils.h
│   ├── core/                 socle commun des plugins
│   │   ├── CommonParameters.*    Mix, Output, Auto Gain, Oversampling, Bypass + fabriques de paramètres
│   │   └── EffectProcessorBase.* AudioProcessor de base (latence, bypass, état, découpage des blocs)
│   └── ui/                   interface commune
│       ├── BrasierLookAndFeel.*  palette et potards (provisoire, design final à l'étape 6)
│       └── GenericKnobEditor.*   éditeur auto-généré et redimensionnable
├── plugins/
│   └── Gain/                 Brasier Gain (étape 1)
├── tests/                    tests hors-ligne (console)
└── scripts/                  check-prereqs.ps1, build.ps1, install-vst3.ps1
```

## 5. Chaîne de traitement commune

```
entrée ─┬─> [retard = latence du wet] ───────────────────────────────┐
        │                                                             ▼
        └─> processEffect()  ──>  auto-gain  ──────────────────>  Mix dry/wet ──> Output ──> sortie
             (oversampling sur
              les étages non linéaires)
```

Chaque plugin n'implémente que `prepareEffect()`, `resetEffect()` et `processEffect()`.
Le socle gère les paramètres communs, le lissage, l'oversampling, la latence reportée à l'hôte,
le bypass compensé et la sauvegarde de l'état.

Règles du thread audio : aucune allocation, aucun verrou, aucun appel système.

## 6. Tests

```powershell
powershell -ExecutionPolicy Bypass -File scripts\build.ps1 -Tests
```

Ils vérifient l'alignement dry/wet pour chaque facteur d'oversampling, le bypass exact au bit près,
les blocs de taille variable, l'auto-gain, la robustesse (NaN, saturation extrême), le changement
d'oversampling en cours de lecture et la sauvegarde de l'état.

## Licence

Projet personnel. JUCE 8 est utilisable gratuitement pour un projet personnel (licence JUCE
« Personal » ou AGPLv3) : voir https://juce.com/get-juce.
