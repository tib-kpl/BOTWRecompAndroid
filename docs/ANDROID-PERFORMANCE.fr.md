# Mesures de performances Android

[English](ANDROID-PERFORMANCE.md)

## Odin 2 Portal : le choix du pilote compte

Mesures du 26 octobre 2026 sur Odin 2 Portal (Snapdragon 8 Gen 2, Adreno 740, 8 Go de RAM, Android 13), avec
BOTW 1.0.0 et la même sauvegarde dans le couloir du Sanctuaire de la Renaissance. Link et la caméra restent immobiles.
Les chiffres ci-dessous excluent le chargement et la compilation initiale des shaders.

La comparaison utilise une résolution interne de 50 %, SGSR 1, aucun SMAA, la sortie à la résolution de l’écran et
le réglage 30 FPS.

| Mode AYN | Pilote Vulkan de l’application | Cadence stabilisée observée |
| --- | --- | --- |
| Standard | MrPurple T30, Mesa 26.3.0, git `62ac221a33` | environ 14,9 FPS |
| Performance | Même pilote T30 | environ 18 FPS |
| Performance | Banner Turnip `v27.0.0-20261025-r5`, paquet générique A6xx/A7xx, Mesa git `ab1502f` | 29,9–30,1 FPS |

Ces résultats concernent cette scène ; ils ne garantissent pas la même cadence dans tout le jeu ou sur une autre console.

Après rétablissement des préférences du joueur (résolution dynamique, FSR 1 et SMAA), le pilote corrigé donne environ
29–30 FPS dans le même couloir en mode Performance. Ces réglages sont distincts de la comparaison contrôlée du tableau.

### Pilote testé

L’archive utilisée est `Turnip-v27.0.0-20261025-r5.zip`, disponible dans les
[releases de The412Banner/Banners-Turnip](https://github.com/The412Banner/Banners-Turnip/releases/tag/v27.0.0-20261025-r5).
Il s’agit du paquet Android générique A6xx/A7xx. Les variantes A8xx, OneUI, Linux et Wayland n’ont pas été utilisées.
Importer le ZIP avec le sélecteur de pilote de l’application, puis redémarrer l’application. Les caches de shaders
peuvent devoir se reconstruire après un changement de pilote.
Les réglages de compatibilité sont restés sur Automatique ; placer tout le tas mémoire du jeu dans la mémoire GPU
n’a pas été nécessaire.

### Pourquoi l’ancien pilote ralentissait le jeu

Le profil relevait une attente importante dans la fonction `wait_timestamp_safe` de Turnip. L’examen du binaire T30
testé confirme qu’un délai nul ou déjà expiré est transmis directement à l’ioctl d’attente KGSL. Or le noyau interprète
zéro comme une attente indéfinie : une simple vérification de progression peut donc attendre la fin du travail GPU.

Le correctif Mesa MR 44838 remplace ce cas par une lecture du compteur des travaux terminés. Le projet qui compile
le pilote documente ce correctif amont et en vérifie la présence :
[origine du correctif KGSL](https://github.com/The412Banner/Banners-Turnip/blob/A8xx/patches/common/SOURCE).
Le changement de pilote supprime la synchronisation observée ; la même scène atteint alors son plafond de 30 FPS.

## Les tests à 30 et à 60 FPS sont distincts

Le jeu testé est en version 1.0.0. Même avec le réglage 60 FPS, cette scène reste à 30 FPS. L’adaptation de la cadence
et de l’horloge du jeu dans `runtime/hle/framepace.c` concerne actuellement la version 1.8.2. Un résultat de 30 FPS sur
la version 1.0.0 ne mesure donc pas le plafond matériel de la console une fois le jeu déplafonné. Une partie à 60 FPS
avec une vitesse de simulation correcte n’a pas encore été démontrée dans ce test.

## Profilage et expériences écartées

Le profilage détaillé de chaque dessin alourdit le travail du moteur. Pour les comparaisons, utiliser des relevés légers
des temps d’image après compilation des shaders, avec la même scène, les mêmes réglages, le même mode AYN et le même pilote.

`LARECOMP_TIMING_TRACE=1` distingue les attentes explicites, l’acquisition et la présentation des images, les soumissions
et les vérifications de progression GPU. Une vérification supposée non bloquante qui dure longtemps aide à repérer un
problème de pilote.

Avec T30, les comparaisons alternées des listes de descripteurs et d’un cache de copies à quatre entrées par groupe
n’ont pas montré de gain utile. Un prototype de copies GPU incrémentales réduit les copies CPU de buffers uniformes
d’environ 92 %, mais ralentit le résultat global d’environ 2 %. Ces variantes de cache et de copie ont été retirées.
Le correctif d’invalidation du cache et ses tests de régression sont conservés.

## BOTW 1.9.0 : pages de commandes NVN, 26 octobre 2026

Essai sur l’Odin 2 Portal avec Turnip `v27.0.0-20261025-r5`, cible 60 FPS, résolution dynamique, FSR1 et SMAA.
La valeur Android `performance_mode=2` est relevée avant et après chaque comparaison ; le propriétaire identifie
ce profil comme High Performance. Ces résultats ne constituent pas une référence en mode Standard.

Le profil CPU par échantillonnage attribue environ 4,7 % du temps à la recherche des pages de commandes et à ses
verrous. Ces pages ne sont jamais libérées : une publication atomique de leur clé permet de lire les pages déjà
créées sans verrou. Le verrou reste utilisé pour leur allocation. Les tests couvrent les collisions et 480 000
recherches concurrentes par huit threads ; le test du chemin rapide passe aussi sous ThreadSanitizer.

Comparaison alternée pendant une minute dans le sanctuaire, sans profilage détaillé par dessin :

| Chemin | Images mesurées | Débit calculé sur les temps d’image | Compteurs CPU du processus par seconde |
| --- | ---: | ---: | ---: |
| Recherche verrouillée | 1 200 | 38,37 FPS | 220,43 ticks/s |
| Lecture atomique | 1 133 | 38,99 FPS | 216,59 ticks/s |

Le gain est modeste sur cet essai : environ 1,6 % de cadence, avec environ 3,3 % de ticks CPU par image en moins.
Les quatre paires d’alternances complètes favorisent le chemin atomique. Cela ne démontre pas un gain équivalent
partout dans le jeu ni le maintien de 60 FPS.

Les essais de cache des constantes internes du pilote n’apportent pas de gain mesurable et sont retirés. Le découpage
actuel des longues passes est conservé (environ 38,8 FPS contre 37,1 sans découpage dans son essai). Rapprocher les
soumissions de 2 ms à 0,5 ms n’améliore pas la cadence. Aucun de ces essais ne réduit la qualité graphique.
