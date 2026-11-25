#pragma once

#include "shared.h"
#include "enemies.h"

#include <string.h>
#include <sys/stat.h>
#include <dirent.h>

#define SAVE_SLOTS 7 // emplacements save_0 à save_6

// Signature et version du format : une sauvegarde d'une autre version du jeu est refusée proprement
#define SAVE_MAGIC 0x44525441 // "ATRD"
#define SAVE_VERSION 3 // 3 : ajout du mode de ciblage des tourelles

struct SerializedGameData
{
    int magic;
    int version;
    Grid grid; // tout le contenu de la grid avec cells en NULL
    GameStats gameStats;
    int waveIndex;
};

// Sauvegarde la partie dans l'emplacement `slot` (fichier ./save/save_<slot>.bin)
bool saveProgress(Grid grid, GameStats gameStats, int waveIndex, int slot);
// Charge l'emplacement `slot`. Renvoie false (sans rien modifier) si le fichier est absent ou invalide.
bool loadProgress(int slot, Grid *grid, GameStats *gameStats, WaveSystem *waveSystem, EnemyPool *enemyPool, Labels *labels);
// Vrai si l'emplacement contient une sauvegarde
bool saveExists(int slot);
// Vague en cours dans la sauvegarde de l'emplacement `slot` (comme affichée dans le jeu), -1 si absente ou d'une autre version
int saveWave(int slot);

/*
## Meilleurs scores ##
Fichier texte ./save/scores.txt, une partie par ligne : "vagues secondes date".
*/
#define MAX_SCORES 10

typedef struct Score
{
    int wave;    // vagues tenues
    int seconds; // durée de la partie
    char date[11]; // AAAA-MM-JJ
} Score;

// Lit les scores (triés, meilleur en premier). Renvoie leur nombre.
int loadScores(Score scores[MAX_SCORES]);
// Enregistre une partie terminée. Renvoie son rang (0 = record) ou -1 si elle n'entre pas dans le classement.
int recordScore(int wave, int seconds);
