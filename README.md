# ATARDES
**Antique Terminal : Absolute Routing Defense Epic Simulator**

Un *tower defense* en pixel art qui tourne entièrement dans un terminal. Écrit en C99, sans aucune dépendance : tout l'affichage est fait avec des caractères Unicode et des codes ANSI.

Au fond d'une salle serveur oubliée, un vieux terminal tourne encore. Des processus corrompus remontent le bus de données pour l'effacer : place des tourelles le long du chemin et tiens le plus de vagues possible.

## Fonctionnalités

- **7 tourelles** avec 2 ou 3 niveaux et un **choix de la cible** (premier, dernier, plus fort, plus faible, plus proche)
- **13 ennemis**, dont un guérisseur, une famille de *splitters* qui se divisent en morceaux, et 4 boss
- Vagues infinies de plus en plus difficiles, chemin généré aléatoirement à chaque partie
- **Pixel art** animé : canons qui suivent leur cible, flammes, givre, chemin parcouru d'impulsions lumineuses
- **Effets de tir** : éclairs, traçantes, explosions, anneaux de flammes, ondes de givre
- Boutique avec **statistiques détaillées** et comparaison avec le niveau suivant
- **Souris** et clavier
- Vitesse de jeu de x1 à x16, menu pause, 7 emplacements de sauvegarde, **meilleurs scores**
- Écran titre animé (une démo tourne en fond) avec reprise de la dernière partie
- Tutoriel intégré (règles, tourelles, ennemis, avec leurs statistiques)
- Affichage sans clignotement, à environ 140 images par seconde

## Installation

Prérequis : Linux ou macOS (ou WSL sous Windows), `gcc` et un terminal qui gère les 256 couleurs (c'est le cas de tous les terminaux récents).

```bash
git clone https://github.com/DiotDuVin/ATARDES-Revamped.git
cd ATARDES-Revamped
./build.sh
```

`build.sh` compile avec les *sanitizers* (détection des erreurs mémoire). Pour une compilation simple et optimisée :
```bash
gcc -O2 -o atrds.out -std=c99 src/*.c -lm
```

## Lancer le jeu

```bash
./jouer.sh
```

Le jeu a besoin d'un terminal d'au moins **232 x 74 caractères**. `jouer.sh` s'en occupe : il compile si besoin, puis ouvre une fenêtre de terminal **en plein écran** avec une taille de police adaptée à l'écran (kitty, alacritty, wezterm, konsole, gnome-terminal, mate-terminal, xfce4-terminal et xterm sont reconnus). Si la fenêtre obtenue n'a toujours pas la bonne taille, le jeu corrige la police et rouvre la fenêtre.

`./atrds.out` fait de même s'il est lancé depuis un terminal trop petit. Sans interface graphique (SSH, console texte), il faut agrandir le terminal ou dézoomer (`Ctrl -`).

Réglages du lanceur :
```bash
ATARDES_FONT_SIZE=6 ./jouer.sh        # forcer une taille de police (en points)
ATARDES_TERMINAL=xterm ./jouer.sh     # forcer un terminal
./jouer.sh --dry-run                  # afficher la commande sans la lancer
```

### Options

| Option           | Effet                                                        |
| ---------------- | ------------------------------------------------------------ |
| `--play`         | lance directement la partie (sans intro ni menu)             |
| `--petit-texte`  | texte de taille normale au lieu de la grande police          |
| `--debug`        | infos techniques en bas de l'écran (images/s, ennemis…)      |
| `--sim-wave [n]` | simulateur de vagues (voir [Développement](#développement))  |
| `--help`         | liste des options                                            |

## Commandes

| Clavier          | Souris                     | Action                                              |
| ---------------- | -------------------------- | --------------------------------------------------- |
| Flèches          | survol d'une case          | sélectionner une case                               |
| Espace           | clic sur une case          | ouvrir la boutique / le menu de la tourelle         |
| Entrée           | clic sur une ligne         | acheter, améliorer, vendre, valider                 |
| ← → (ligne Cible)| clic sur la ligne Cible    | changer le mode de ciblage                          |
| O / L            |                            | accélérer / ralentir (x1 à x16)                     |
| S                |                            | sauvegardes (Entrée : charger, Espace : sauver)     |
| Échap            | clic droit / clic à côté   | fermer le menu, ou ouvrir le menu pause             |
| Q                |                            | quitter (la partie est sauvée dans l'emplacement 0, « Continuer » dans le menu principal la reprend) |
|                  | molette                    | monter / descendre dans les menus                   |

## Le jeu

Tu commences avec 20 ❤ et 30 €. Chaque ennemi qui atteint la sortie retire des points de vie ; chaque ennemi détruit rapporte de l'argent, et chaque vague terminée rapporte 10 €.

### Tourelles

| Tourelle      | Prix (niveaux)  | Rôle                                                              |
| ------------- | --------------- | ----------------------------------------------------------------- |
| Sniper        | 15 / 50 / 110   | tir précis sur une cible, longue portée                           |
| Gatling       | 35 / 100        | tir très rapide, courte portée                                    |
| Mortier       | 35 / 80         | explosions de zone, mais portée minimale                          |
| Inferno       | 20 / 60         | brûle plusieurs ennemis autour d'elle                             |
| Freezer       | 25 / 70         | ralentit tous les ennemis à portée                                |
| Pétrificateur | 10 / 40         | fige un ennemi sur place                                          |
| Banque        | 80 / 180        | ne tire pas, rapporte de l'argent régulièrement                   |

Une tourelle se revend 80 % de son prix. Les statistiques exactes de chaque niveau sont affichées dans la boutique et dans le tutoriel.

### Ennemis

Tux, Speed, Tux couronné, Araignée, Hyperspeed, **Guérisseur** (soigne ses voisins), **Splitter** (se divise en 2 mini splitters), **Gros splitter** (se divise en 2 splitters) et 4 boss : Slime (vague 10), Boss électrique (vague 20, désactive les tourelles proches), Boss splitter (vague 25, se divise en 3 gros splitters) et Tank (vague 30, insensible au gel et à la pétrification).

### Sauvegardes et scores

Les parties sont sauvées dans `save/save_0.bin` à `save/save_6.bin`, et les 10 meilleurs scores dans `save/scores.txt`.

## Développement

### Organisation du code

| Fichier                     | Rôle                                                              |
| --------------------------- | ----------------------------------------------------------------- |
| `src/main.c`                | boucle de jeu, temps, entrées clavier et souris, menus, terminal  |
| `src/shared.c`, `shared.h`  | types communs (`Grid`, `Cell`, `Turret`…), constantes, utilitaires, menu de départ |
| `src/grid.c`                | grille, génération du chemin                                      |
| `src/enemies.c`             | ennemis (statistiques, déplacement, soin, division) et vagues     |
| `src/tower.c`               | tourelles (statistiques, prix, tirs, ciblage) et fenêtres de menus |
| `src/save.c`                | sauvegardes et meilleurs scores                                   |
| `src/render.c`              | moteur d'affichage (double buffering)                             |
| `src/pixelart.c`            | pixel art : tourelles, ennemis, cases, chemin, portée             |
| `src/effects.c`             | effets de tir                                                     |
| `src/bigtext.c`             | grande police pour les textes                                     |
| `src/intro.c`               | animation d'introduction                                          |
| `src/title.c`               | écran titre : menu principal et démo animée                       |
| `src/tutorial.c`            | contenu et mise en page du tutoriel                               |
| `jouer.sh`                  | lanceur plein écran                                               |
| `tools/`                    | simulateur d'équilibrage                                          |

### Affichage

Écrire directement dans un terminal fait clignoter l'écran. Le moteur `render.c` dessine donc chaque image dans un tableau en mémoire, la compare à ce qui est affiché, et n'envoie au terminal **que les caractères qui ont changé**, en une seule écriture. Le jeu peut ainsi redessiner tout le plateau à chaque image.

Le pixel art (`pixelart.c`) utilise chaque caractère comme **deux pixels** : `▀` affiche sa couleur de texte en haut et sa couleur de fond en bas. Une case de 18 x 9 caractères devient une image de 18 x 18 pixels presque carrés, en 256 couleurs. Les tourelles sont dessinées par programme (cercles, lignes, dégradés) ; les ennemis sont des sprites dont chaque lettre est une couleur, faciles à modifier.

Les effets de tir (`effects.c`) sont dessinés par-dessus le plateau, au demi-caractère près : le moteur relit ce qui est déjà affiché pour ne remplacer que le pixel du haut ou du bas.

Toutes les animations suivent le temps réel : elles ont la même vitesse sur tous les ordinateurs. À vitesse élevée, la simulation est découpée en pas de 1/60 s pour rester précise.

### Équilibrage

`tools/equilibrage.sh` simule des parties complètes sans affichage : des bots jouent avec différentes stratégies (une seule tourelle, un mélange, la Banque d'abord) sur plusieurs cartes, et le script affiche la vague moyenne atteinte.
```bash
./tools/equilibrage.sh 10
```
Le but : un mélange de tourelles doit tenir bien plus longtemps qu'une seule sorte de tourelle, et aucune stratégie ne doit écraser les autres. Les prix sont dans `TURRET_PRICES` (`tower.c`), les statistiques dans `getTurretStruct` (`tower.c`) et `defEnemy` (`enemies.c`), la difficulté des vagues dans `getWaveByIndex` (`enemies.c`).

`./atrds.out --sim-wave [n]` affiche la composition des vagues et l'enregistre dans `testing/waves.csv` ; `./testing/test.sh n` en fait des graphiques (nécessite matplotlib et pandas).

### Débogage

`./build.sh` compile avec AddressSanitizer et UndefinedBehaviorSanitizer : toute erreur mémoire est signalée immédiatement. `--debug` affiche le nombre d'images par seconde et d'ennemis.

## Auteurs

Rafael ([@rafalou38](https://github.com/rafalou38)), Rémi ([@diotduvin](https://github.com/diotduvin)) et Philémon ([@nomelip](https://github.com/nomelip)).

Version d'origine : [rafalou38/ATRDS](https://github.com/rafalou38/ATRDS).

## Licence

MIT, voir [LICENSE](./LICENSE).
