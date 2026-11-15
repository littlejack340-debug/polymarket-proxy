# Brasier Suite

Suite d'effets audio originale au format **VST3** (+ Standalone) pour FL Studio sous Windows,
écrite en **C++20** avec **JUCE 8** et **CMake**. Couleur analogique, pensée pour la production rap.

| Plugin | Rôle |
|---|---|
| **Brasier Fournaise** | saturateur : tube, transistor, bande, diode, tone (tilt), filtres pré/post, mode Punish |
| **Brasier Ruban** | delay à bande : temps libre ou synchro, feedback saturé et filtré, wow & flutter, ping-pong, ducking |
| **Brasier Mue** | pitch ±12 et formant ±12 indépendants, mode robot, faible latence (Live ~20 ms) |
| **Brasier Tamis** | filtre LP/HP/BP/notch 12/24 dB, résonance qui sature, LFO synchronisé, envelope follower |
| **Brasier Remous** | chorus, flanger, phaser, tremolo à caractère analogique |
| **Brasier Ressort** | reverb à ressort et à plaque, pré-delay, damping, drive vintage |
| **Brasier Gain** | gain + soft-clip, outil de test de l'infrastructure commune |

Chaque plugin a : Mix dry/wet, Output, Auto Gain, Oversampling 1x/2x/4x/8x (sur les étages non
linéaires), Bypass compensé, latence reportée à l'hôte, presets d'usine et utilisateur, interface
redimensionnable avec VU-mètres.

---

## Démarrage rapide (Windows)

```powershell
git clone -b claude/vst3-audio-effects-suite-83v5qt https://github.com/littlejack340-debug/polymarket-proxy.git C:\dev\polymarket-proxy
cd C:\dev\polymarket-proxy\brasier-suite
powershell -ExecutionPolicy Bypass -File scripts\check-prereqs.ps1     # verifie VS 2022, CMake, Git
powershell -ExecutionPolicy Bypass -File scripts\build.ps1 -Install    # compile + copie les .vst3
```

Puis dans FL Studio : *Options > Manage plugins > Find installed plugins*. Les plugins sont rangés sous **Brasier Audio**.

## 1. Prérequis

| Outil | Version | Installation |
|---|---|---|
| Visual Studio **2022 ou 2026** (Community ou Build Tools) | avec les outils C++ (MSVC + Windows SDK) | `winget install Microsoft.VisualStudio.2022.BuildTools --override "--add Microsoft.VisualStudio.Workload.VCTools --includeRecommended --passive"` |
| CMake | **3.22 minimum** (JUCE 8), **4.2 minimum avec VS 2026** | `winget install Kitware.CMake` |
| Git | toute version récente | `winget install Git.Git` |

`scripts\check-prereqs.ps1` vérifie tout et donne la commande à lancer pour ce qui manque.
JUCE n'est pas à installer : CMake télécharge le tag `8.0.17-security` au premier build.
Place le projet dans un chemin court (ex. `C:\dev\`) à cause de la limite de 260 caractères de Windows.

## 2. Compiler

```powershell
cmake --preset vs2022              # configure (telecharge JUCE la 1re fois, quelques minutes)
cmake --build --preset release     # compile les 7 plugins en Release (VST3 + Standalone)
```

Ou sans preset : `cmake -S . -B build\vs2022 -G "Visual Studio 17 2022" -A x64` puis
`cmake --build build\vs2022 --config Release --parallel`. Le script `scripts\build.ps1` fait la même chose
(`-Tests` lance en plus les tests, `-Install` copie les plugins) et choisit tout seul Visual Studio 2022
ou 2026 (le dossier de build devient alors `build\vs2026`). La solution `build\vs20xx\BrasierSuite.sln`
s'ouvre aussi dans Visual Studio. Visual Studio 2026 à la main : `cmake --preset vs2026` (CMake 4.2+).

Les plugins sortent dans `build\vs2022\plugins\<Nom>\Brasier<Nom>_artefacts\Release\` :
`VST3\Brasier <Nom>.vst3` (dossier du plugin) et `Standalone\Brasier <Nom>.exe`.

## 3. Installer dans FL Studio

Chaque `.vst3` est un **dossier** : copie les dossiers entiers dans `C:\Program Files\Common Files\VST3\`
(droits administrateur, FL Studio fermé). `scripts\install-vst3.ps1` copie les 7 plugins et se relance
tout seul en administrateur. Ensuite : *Options > Manage plugins > Find installed plugins*.

Conseils d'utilisation dans FL Studio :
- **Latence** : chaque plugin déclare sa latence (oversampling, pitch shifter). FL Studio la compense
  automatiquement (PDC) : tes pistes restent calées, même avec Mix < 100 %.
- **Oversampling** : 2x ou 4x suffisent pour mixer ; monte à 8x pour l'export final si tu satures fort
  dans les aigus. Ce réglage n'est pas automatisable (il change la latence).
- **Brasier Mue en live** : mode *Live* (~20 ms, voix ≥ 100 Hz). Pour les samples et voix très graves :
  mode *Qualité* (~34 ms, ≥ 60 Hz).
- **Presets** : la barre en haut de chaque plugin, ou le menu des presets de FL (ils sont exposés comme
  programmes). « Sauver » crée un preset perso dans `Documents\Brasier Audio\<plugin>\Presets`.
- **Interface** : tire le coin bas-droit pour agrandir (70 % à 200 %, la taille est mémorisée).
  Double-clic sur un potard = valeur par défaut, Ctrl + glisser = réglage fin, clic sur l'afficheur
  = saisie d'une valeur.

## 4. Les plugins et leur DSP

### Brasier Fournaise : saturateur
```
entrée → low/high cut PRÉ → [×N : bosse Punish → drive → modèle → compensation] → DC block → tone (tilt) → low/high cut POST
```
- **Modèles** (courbes à pente 1 à l'origine, transparentes à bas niveau) :
  *Tube* = tanh décentrée (asymétrique → harmoniques paires, chaleur) ; *Transistor* = écrêtage cubique
  à plateau franc (harmoniques impaires, mordant) ; *Bande* = compression x/(1+|x|) entre une
  pré-emphase et une dé-emphase des aigus (les aigus saturent avant, comme sur un magnéto) ;
  *Diode* = écrêtage exponentiel asymétrique (fuzz).
- **Drive** 0–36 dB, compensé de moitié (le drive ajoute la moitié de ses dB). **Punish** : +15 dB de
  drive et +6 dB à 1,8 kHz avant la saturation (agressif, très présent).
- **Filtres pré** : choisissent ce qui sature (ex. low cut pré à 120 Hz : l'808 sature dans le haut, le sub
  reste propre). **Filtres post** : nettoient. En butée, ils sont réellement coupés (aucun déphasage).
- Oversampling 4x par défaut. Mesure : aliasing 16 dB sous le signal en 1x, 72 dB sous le signal en 8x.

### Brasier Ruban : delay à bande
```
entrée ─┬─► tête d'écriture ····· tête de lecture (retard D ± wow ± flutter) ─► low cut ─► high cut ─► saturation ×N ─┬─► sortie (ducking)
        └───────────────────────────── feedback ◄──────────────────────────────────────────────────────────────────────┘
```
- Temps **libre** (10 ms – 2 s) ou **synchronisé** (1/64 à 1/1, triolets, pointées).
- **Feedback** jusqu'à 110 % : la saturation dans la boucle empêche l'emballement, la bande auto-oscille.
- La **saturation** est sur-échantillonnée *dans* la boucle ; sa latence est compensée en lisant la bande
  plus tôt : échos au sample près, aucune latence reportée.
- **Wow** (dérive lente : sinus + aléatoire) et **flutter** (vibrato rapide), déphasés à droite.
  Changer le temps fait glisser la tête de lecture (glissement de pitch typique des échos à bande).
- **Modes** mono / stéréo (Spread : décalage du canal droit) / ping-pong.
- **Ducking** : le wet baisse quand la voix (l'entrée) dépasse le seuil, et remonte dans les silences.

### Brasier Mue : pitch / formant (TD-PSOLA)
- **Détection** : YIN sur le signal décimé à ~11 kHz (fonction de différence normalisée, interpolation
  parabolique, hystérésis contre les sauts d'octave).
- **Analyse** : des marques espacées d'une période découpent des grains de deux périodes (fenêtre de Hann).
- **Synthèse** : *pitch* = espacement des grains recollés (P / ratio) → les formants restent en place ;
  *formant* = chaque grain est compressé / étiré d'un facteur F → l'enveloppe spectrale bouge sans changer
  la note ; *robot* = grains émis à la fréquence d'une note fixe (C2–C5).
- Au réglage neutre la reconstruction est exacte (le signal ressort intact, retardé de la latence).
- Mesures : justesse à 0,1 cent près de −12 à +12 demi-tons sur signal de synthèse.

### Brasier Tamis : filtre multimode
- SVF en topologie TPT (stable même modulé à chaque échantillon), LP/HP/BP/notch, 12 ou 24 dB/oct.
- **Résonance qui sature** : l'intégrateur passe-bande est écrêté (tanh). En fin de course
  (au-delà de ~90 %), le filtre **auto-oscille** proprement, l'amplitude étant tenue par la saturation.
- Fréquence = cutoff × 2^(LFO × profondeur + enveloppe × profondeur), ±4 octaves chacun.
- **LFO** : 7 formes, libre (Hz) ou synchronisé (1/64 à 8 mesures), calé sur la position du transport.
- **Envelope follower** sur l'entrée (attaque / relâchement) : le filtre s'ouvre quand le son tape.

### Brasier Remous : modulation analogique
- **Chorus** 3 voix (retard ~12 ms, LFO déphasés d'un tiers de tour), façon BBD multi-voix.
- **Flanger** (0,25 à 5 ms, feedback ±), **Phaser** 6 étages passe-tout avec feedback,
  **Tremolo** optique (attaque rapide, relâchement lent ; Spread 100 % = auto-pan).
- **Character** : passe-bas type puce BBD, saturation dans le chemin du retard / du feedback,
  légère dérive aléatoire des LFO. Vitesse libre ou synchronisée.

### Brasier Ressort : reverb ressort / plaque
```
entrée → low/high cut → drive (préampli, ×N) → mono → pré-delay → ressort OU plaque → largeur stéréo
```
- **Plaque** : structure publiée par J. Dattorro (1997) : 4 passe-tout de diffusion, réservoir en huit
  (deux branches croisées) avec passe-tout modulés, amortissement des aigus, sorties décorrélées.
- **Ressort** : 3 ressorts parallèles ; chaque boucle contient 48 passe-tout « étirés »
  H(z) = (a + z⁻ᴷ)/(1 + a z⁻ᴷ) qui dispersent le son (les aigus arrivent après les graves : le « chirp »
  et le « boing » du ressort). *Tension* règle la dispersion.
- **Decay** 0,3–8 s (RT60 mesuré conforme), **pré-delay** 0–250 ms, **damping**, **width**.

## 5. Architecture commune

```
entrée ─┬─► [retard = latence du wet] ─────────────────────────────────┐
        │                                                              ▼
        └─► processEffect() ──► auto-gain ──────────────────────► Mix dry/wet ──► Output ──► sortie
             (oversampling sur les étages non linéaires)
```
`shared/core/EffectProcessorBase` gère tout ce qui est commun ; chaque plugin n'écrit que
`prepareEffect()`, `resetEffect()` et `processEffect()` :
- **Paramètres** APVTS automatisables, **lissés** (rampes de 20–30 ms, fréquences lissées en
  échelle log) : aucun clic ni « zipper noise ». Changements de modèle / type / mode en fondu.
- **Oversampling** : filtres demi-bande FIR à phase linéaire de JUCE, latence entière ; les moteurs
  2x/4x/8x sont tous alloués d'avance, donc changer de facteur ne fait aucune allocation.
- **Latence** : calculée dans le thread audio, reportée à l'hôte depuis le thread message
  (la fonction JUCE correspondante prend un verrou). Le dry est retardé d'autant.
- **Mix** linéaire (saturation, filtre, modulation) ou à puissance constante (delay, reverb, pitch).
- **Auto Gain** mesuré (RMS ~400 ms, ±24 dB, gelé dans les silences) ou fixe (delay, reverb).
- **Bypass** doux et compensé en latence ; blocs plus grands qu'annoncé découpés ; état sauvegardé.
- **Thread audio** : aucune allocation, aucun verrou, aucun appel système (le changement de preset
  demandé par l'hôte hors thread message est différé).

## 6. Presets d'usine (110)

| Plugin | Exemples |
|---|---|
| Fournaise | Chaleur lampe, Présence radio, Grain téléphone, Cri saturé, Mégaphone, 808 Grondement, 808 Sale sub propre, 808 Distorsion drill, Vinyle chaud, Boucle lo-fi, Colle de bus |
| Ruban | Écho 1/4 discret (ducking), Slap rétro, Refrain large, Ping-pong 1/8, Lancer 1/8 pointée, Tunnel, Triolet fantôme, 808 Ombre, Bande usée, Écho de cassette, Dub infini |
| Mue | Ad-lib aigu, Ad-lib démon, Doublures d'octave, Voix d'enfant, Épaisseur, Harmonies tierce / quinte, Robot C3 / grave / aigu, Sample accéléré (soul), Sample ralenti, Scène : ad-lib +12 |
| Tamis | Intro radio, Intro étouffée, Montée 4 mesures, Voix téléphone, Wah automatique, 808 Wobble 1/8, 808 Nettoyage infra, Hachoir 1/16, Sifflement auto-oscillant |
| Remous | Chorus large, Refrain épais, Chorus 80s, Flanger jet, Vibrato, Phaser vintage, Tremolo ampli 1/8, Auto-pan 1/4, Hachoir 1/16 |
| Ressort | Plaque voix, Plaque brillante, Petite pièce, Ressort lo-fi, Grande salle, Ressort surf, Rhodes vintage, 808 Halo sans sub, Nappe infinie |

Catégories : *Voix lead*, *Ad-libs*, *808*, *Samples*, *Effets*, *Mix*. Un preset ne touche jamais
à l'oversampling (ton choix qualité / CPU) ni au bypass.

## 7. Tests et performances

```powershell
powershell -ExecutionPolicy Bypass -File scripts\build.ps1 -Tests
```
- `BrasierTests` (32 groupes) : pour **chaque plugin** une batterie commune (automation aléatoire de tous
  les paramètres, bypass et Mix 0 % exacts au bit près, blocs de 1 à 4096 échantillons, silence, état,
  mono, 44,1 / 96 kHz) puis des mesures spécifiques : aliasing par FFT, réponses des filtres, timing des
  échos, ducking, justesse du pitch, formants, robot, RT60 par intégration de Schroeder, pré-delay,
  dispersion du ressort, validité des 110 presets, banc d'essai CPU.
- `BrasierHostTest build\vs2022\plugins` : charge les **vrais .vst3** avec l'hôte VST3 de JUCE (scan,
  traitement sous automation, état, programmes, ouverture de l'interface).

CPU mesuré (48 kHz, blocs de 256, stéréo, un cœur, machine de build Linux) :

| Plugin | Réglages par défaut | Oversampling 8x |
|---|---|---|
| Fournaise | 1,4 % | 2,4 % |
| Ruban | 0,9 % | 2,4 % |
| Mue | 0,4 % | – |
| Tamis | 0,8 % | 3,0 % |
| Remous | 1,6 % | 5,5 % |
| Ressort | 2,1 % | 3,9 % |

## 8. Structure

```
brasier-suite/
├── CMakeLists.txt             projet racine : JUCE (FetchContent), options, sous-dossiers
├── CMakePresets.json          presets vs2022 / vs2026 / ninja
├── cmake/BrasierPlugin.cmake  brasier_add_plugin() : réglages communs à tous les plugins
├── shared/
│   ├── dsp/                   DSP pur : lissage, mix, auto-gain, oversampling, SVF, filtres de coupure,
│   │                          filtre 1 pôle, courbes de saturation, lignes de retard, LFO, synchro tempo,
│   │                          suiveur d'enveloppe, YIN, PSOLA, filtre résonant, modulations, reverbs
│   ├── core/                  socle des plugins : EffectProcessorBase, paramètres communs, presets
│   └── ui/                    LookAndFeel, éditeur commun, VU-mètres, barre de presets
├── plugins/<Nom>/Source/      un dossier par plugin : processeur, presets d'usine, point d'entrée
├── tests/                     tests hors-ligne + hôte VST3 de test
└── scripts/                   check-prereqs.ps1, build.ps1, install-vst3.ps1
```

## Licence

Projet personnel. JUCE 8 est utilisable gratuitement pour un projet personnel (licence JUCE
« Personal » ou AGPLv3) : voir https://juce.com/get-juce.
