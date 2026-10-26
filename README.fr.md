# BOTWRecompAndroid

[English](README.md) | **Français** | [Português](README.pt-BR.md)

> [!NOTE]
> **Objectif de ce fork**
>
> Ce fork de [mamcjr/BOTWRecompAndroid](https://github.com/mamcjr/BOTWRecompAndroid) facilite le téléchargement des
> versions Android et rend les menus de l’application accessibles dans davantage de langues. Ses ajouts actuels sont :
>
> - **Releases APK automatiques :** chaque push sur `main` dont la compilation réussit produit un APK Android signé,
>   publié en préversion sur [la page Releases de ce fork](https://github.com/tib-kpl/BOTWRecompAndroid/releases).
> - **Menus en français :** traduction de l’accueil, des paramètres, des commandes, des dialogues et des raccourcis
>   Android, en complément des menus anglais et portugais existants. L’interface suit la langue de l’application
>   définie dans Android.
> - **Préparation des futures traductions :** des catalogues séparés, des ressources de texte Android et des contrôles
>   dans la CI facilitent l’ajout de langues. Voir le [guide de traduction](docs/TRANSLATIONS.md) (en anglais).
>
> - **Tests sur consoles Android :** un pilote Turnip corrigé permet de tenir 30 FPS sur l’Odin 2 Portal dans la scène
>   testée de BOTW 1.0.0. Voir [les réglages mesurés et la version du pilote](docs/ANDROID-PERFORMANCE.fr.md).
> - **Prise en charge du patch BOTW 1.9.0 (expérimentale) :** écran titre, chargement des sauvegardes et monde ouvert
>   testés sur l’Odin 2 Portal. L’installation dans `v190` conserve ses propres sauvegardes et paramètres.
>   Le rendu et les performances restent à améliorer ; choisir 60 FPS ne garantit pas de les maintenir.
> - **Commandes et démarrage :** les associations de boutons sont utilisées dans le jeu et les menus de l’application ;
>   un écran de chargement traduit accompagne le lancement. L’écran large reste réservé à la 1.8.2.
>
> Le moteur de recompilation et la prise en charge existante du jeu proviennent des projets d’origine cités plus bas.
> Les traductions décrites ici concernent l’interface de l’application ; la langue des textes du jeu dispose de son
> propre réglage.
>
> Voir [les releases Android en CI](docs/ANDROID-RELEASES.md) pour les détails de compilation et de signature
> (documentation en anglais).

Une recompilation statique de *The Legend of Zelda: Breath of the Wild* (Nintendo Switch, versions **1.0.0, 1.6.0,
1.8.2 et 1.9.0**) pour Android. **La prise en charge de la 1.9.0 est expérimentale.** Il ne s’agit **pas d’émulation** :
le code ARM64 du jeu est transformé en C, compilé pour votre
téléphone, et ses commandes graphiques (NVN) sont traduites en Vulkan.

Le projet repose sur le moteur de **[LAS Recomp](https://github.com/peiikai/LASRecomp)** de peiikai, une recompilation
de *Link’s Awakening*, et sur son portage Android, **[LASRecompAndroid](https://github.com/mamcjr/LASRecompAndroid)**.

> [!WARNING]
> **Bêta.** Les essais du projet d’origine se concentrent sur le POCO F7 Ultra (Adreno 830, pilote Mesa Turnip).
> Ce fork est également testé sur l’Odin 2 Portal ; les mesures sont référencées plus haut.
> Des bugs sont à prévoir. Le plan et l’avancement des différentes phases sont dans
> [docs/BOTW-PORT.md](docs/BOTW-PORT.md) (en portugais).

## Plateforme prise en charge

Android est la seule plateforme cible de l’application. Les portages et les paquets pour ordinateur ont été
abandonnés le 22 octobre 2026. Windows, Linux et macOS peuvent toujours servir d’environnements de développement
avec le SDK/NDK Android.

## État du projet

- **Jouable**, de l’écran titre au monde ouvert, avec sauvegardes, sur le POCO F7 Ultra : versions 1.0.0 et 1.6.0 à
  30 images/s ; version 1.8.2 à 30 ou jusqu’à 60 images/s (environ 40 à 47 en monde ouvert avec un pilote Turnip).
- **Version 1.8.2 :** portugais du Brésil, écran large (Vidéo → Format d’image → « Déverrouillé » : le champ de vision
  s’élargit et l’interface se déplace vers les bords de l’écran) et gestion du rythme des images de SavageWindRecomp.
- **Version 1.9.0 (expérimentale) :** écran titre, chargement des sauvegardes et jeu en monde ouvert testés sur
  l’Odin 2 Portal. Les lignes noires et les défauts de rendu du terrain sont encore en cours d’analyse, avec un
  [signalement similaire dans le projet d’origine, nº 21](https://github.com/mamcjr/BOTWRecompAndroid/issues/21).
  Les performances varient selon la scène ; l’option 60 FPS ne garantit pas 60 FPS constants.
  L’écran large reste limité à la 1.8.2.
- **Vidéo :** résolution interne, mise à l’échelle AMD FSR 1 / Snapdragon GSR 1, résolution de sortie, SMAA et
  stabilisation temporelle expérimentale facultative (désactivée par défaut).
- **Application :** importation des fichiers du jeu depuis le téléphone, avec un dossier par version et sélection
  dans la page Fichiers du jeu ; commandes tactiles, manettes, mods, menus anglais, français et portugais, choix du
  pilote Vulkan (Turnip via adrenotools) et options de compatibilité des pilotes.
- **Limites connues :** l’installation depuis le téléphone compile le code du jeu sur celui-ci ; cette opération
  n’a pas encore été chronométrée ni testée avec BOTW (prévoir une longue attente et de la chauffe). Les GPU autres
  que les Adreno 8xx utilisent des méthodes plus prudentes et plus lentes. Les miniatures des sauvegardes créées
  dans l’application sont noires et l’audio utilise encore un mixeur simplifié.

> [!IMPORTANT]
> Comme LAS Recomp, le projet d’origine est développé presque entièrement avec l’IA Claude d’Anthropic, via
> Claude Code, sous la direction, les tests et la relecture d’un humain.

> [!NOTE]
> **Aucun fichier du jeu n’est inclus.** Vous devez disposer de votre propre copie extraite légalement de
> *Breath of the Wild* 1.0.0, 1.6.0, 1.8.2 ou 1.9.0, avec les dossiers `exefs` et `romfs`. Aucune instruction permettant
> d’obtenir ces fichiers ne sera fournie.

### Utiliser le patch 1.9.0

Importez les dossiers `exefs` et `romfs` correspondants de votre copie du jeu, avec le patch **1.9.0 déjà appliqué**.
L’APK importe les fichiers extraits ; il n’applique pas directement un patch à un XCI ou un NSP.
La version est détectée automatiquement et installée dans `v190` sur le stockage choisi, y compris la carte SD.
Les versions installées se sélectionnent dans **Paramètres → Fichiers**. Chaque version conserve ses propres
sauvegardes et paramètres ; installer la 1.9.0 ne migre pas les sauvegardes d’une autre version.

## Compatibilité des manettes

**Commandes intégrées — AYN Thor, Odin 2 et Retroid :** les boutons standard, les deux sticks et les gâchettes
devraient fonctionner grâce à la prise en charge des manettes Android de SDL3, si l’appareil est compatible avec
l’APK. **L’Odin 2 Portal a été testée**, y compris les boutons de validation et de retour réaffectés dans les menus
de l’application ; l’AYN Thor et les Retroid restent à tester. Ouvrez **Paramètres → Commandes** pour vérifier la
détection et réaffecter les boutons du jeu et des menus. La disposition par défaut est celle de Nintendo : **A à droite, B en bas**.

## Installation (bêta)

1. Téléchargez l’APK depuis [Releases](https://github.com/tib-kpl/BOTWRecompAndroid/releases) et installez-le
   (Android 11 ou ultérieur, ARM64, Vulkan ; environ 30 Go d’espace libre par version du jeu).
2. Pour stocker les données du jeu sur une carte SD, ouvrez d’abord **Paramètres → Fichiers → Stockage des données
   du jeu** et sélectionnez la carte. Le choix est conservé au prochain lancement ; changer de stockage redémarre
   l’application et ne déplace pas les fichiers existants.
3. Choisissez **Importer les fichiers du jeu**, puis sélectionnez le dossier contenant
   `exefs` et `romfs`. La version est détectée à partir des fichiers. L’application copie les fichiers nécessaires
   et compile le code du jeu sur le téléphone.
4. Sur les téléphones équipés d’un GPU Adreno, un pilote Mesa Turnip (**Paramètres → Fichiers → Pilote Vulkan**)
   fait tourner le jeu beaucoup plus vite que le pilote du système. Si l’application s’arrête au démarrage avec
   « Vulkan driver is too old », faites un appui long sur son icône et choisissez **Choisir un pilote** pour en
   sélectionner un avant le lancement du jeu.

## Tests sur Android (développement)

Avec le SDK/NDK Android, un téléphone connecté via adb et votre copie extraite du jeu dans `Dump/` :

```bash
cd android/botwrecompandroid && ./gradlew assembleDebug  # l’application
android/build_gamelib.sh Dump/01007EF00011E000/exefs     # le code du jeu (environ une heure)
android/push_game.sh Dump/01007EF00011E000               # copie les fichiers et le code du jeu sur le téléphone
android/run_boot.sh 120                                # lance le jeu, journal dans android/out/boot.log
```

## Crédits

- [peiikai](https://github.com/peiikai) pour [LAS Recomp](https://github.com/peiikai/LASRecomp), le recompilateur et
  le moteur d’exécution sur lesquels repose ce projet (MIT, voir [LICENSE](LICENSE)). Le README d’origine est
  conservé dans [docs/las/](docs/las/).
- [LASRecompAndroid](https://github.com/mamcjr/LASRecompAndroid), son portage Android : l’application, la compilation
  du code du jeu sur le téléphone, les commandes tactiles, les mods, la compatibilité des pilotes et les menus portugais.
- [Revivedx](https://github.com/Revivedx) pour [SavageWindRecomp](https://github.com/Revivedx/SavageWindRecomp) (MIT),
  la référence pour la version 1.8.2 : rythme des images, résolution interne, correctifs du noyau pour le SDK 20, setjmp.
- [Nycolazs](https://github.com/Nycolazs) pour LASRecomp-Android-Plus (MIT) : compatibilité des pilotes Vulkan.
- La décompilation [zeldaret/botw](https://github.com/zeldaret/botw) et [open-ead/sead](https://github.com/open-ead/sead),
  pour les connaissances sur le code du jeu.
- Le lanceur de Dusklight, pour le style de l’écran d’accueil.
- Mise à l’échelle et anticrénelage : [AMD FidelityFX FSR 1](https://github.com/GPUOpen-Effects/FidelityFX-FSR) (MIT),
  [Qualcomm Snapdragon GSR](https://github.com/SnapdragonStudios/snapdragon-gsr) (BSD-3-Clause),
  [SMAA](https://github.com/iryoku/smaa) (MIT).
- Bibliothèques : [SDL](https://libsdl.org) (zlib), [Dear ImGui](https://github.com/ocornut/imgui) (MIT),
  [libadrenotools](https://github.com/bylaws/libadrenotools) (BSD-2-Clause), [volk](https://github.com/zeux/volk) (MIT),
  [Vulkan Memory Allocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator) (MIT),
  [shaderc](https://github.com/google/shaderc) (Apache 2.0), [FFmpeg](https://ffmpeg.org) (LGPL),
  [LZ4](https://github.com/lz4/lz4) (BSD-2-Clause), [Capstone](https://www.capstone-engine.org) (BSD-3-Clause),
  [Unicorn](https://www.unicorn-engine.org) (GPLv2, compilé depuis ses sources par `android/deps/build_unicorn.sh`),
  [Python](https://www.python.org) (PSF) et [Zig](https://ziglang.org) (MIT) pour la compilation sur le téléphone.
- Le pilote Turnip de [Mesa](https://www.mesa3d.org) (MIT), chargé par les joueurs eux-mêmes.
- Polices : [Baloo 2](https://github.com/EkType/Baloo2) (OFL), DejaVu Sans (licence Bitstream Vera). Le robot Android
  est reproduit ou modifié à partir d’une œuvre créée et partagée par Google (CC BY 3.0).
- Projet d’origine réalisé avec [Claude Code](https://claude.com/claude-code) (Anthropic).

*The Legend of Zelda* et *Breath of the Wild* sont des marques de Nintendo. Ce projet n’est ni affilié à Nintendo,
ni approuvé par Nintendo.
