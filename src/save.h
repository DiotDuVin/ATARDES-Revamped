#pragma once

#include "shared.h"
#include "enemies.h"

#include <string.h>
#include <sys/stat.h>
#include <dirent.h>

#define SAVE_SLOTS 7 // emplacements save_0 à save_6

struct SerializedGameData
{
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
