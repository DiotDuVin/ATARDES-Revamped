#include "save.h"

static const char *save_dir = "./save";

// Écrit dans `out` le chemin du fichier de l'emplacement `slot`
static void savePath(char *out, size_t size, int slot)
{
    snprintf(out, size, "%s/save_%d.bin", save_dir, slot);
}

bool saveExists(int slot)
{
    char path[64];
    savePath(path, sizeof(path), slot);
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

bool saveProgress(Grid grid, GameStats gameStats, int waveIndex, int slot)
{
    if (slot < 0 || slot >= SAVE_SLOTS)
        return false;

    // Création du dossier de sauvegarde s'il n'existe pas
    struct stat folder_stats;
    if (stat(save_dir, &folder_stats) != 0)
    {
        if (mkdir(save_dir, 0775) != 0)
            return false;
    }
    else if (!S_ISDIR(folder_stats.st_mode))
    {
        return false;
    }

    char path[64];
    savePath(path, sizeof(path), slot);
    FILE *file = fopen(path, "wb");
    if (file == NULL)
        return false; // pas de message : on est en plein jeu, il casserait l'affichage

    // Données générales (le pointeur vers les cases n'a pas de sens dans un fichier)
    struct SerializedGameData data;
    memset(&data, 0, sizeof(data));
    data.grid = grid;
    data.grid.cells = NULL;
    data.gameStats = gameStats;
    data.waveIndex = waveIndex;
    bool ok = fwrite(&data, sizeof(data), 1, file) == 1;

    // Contenu de la grille, colonne par colonne
    for (int x = 0; ok && x < grid.width; x++)
        ok = fwrite(grid.cells[x], sizeof(struct Cell), grid.height, file) == (size_t)grid.height;

    // fclose écrit réellement les données sur le disque (avant, le fichier n'était jamais fermé :
    // une sauvegarde rechargée dans la même partie pouvait être incomplète)
    if (fclose(file) != 0)
        ok = false;
    return ok;
}

bool loadProgress(int slot, Grid *grid, GameStats *gameStats, WaveSystem *waveSystem, EnemyPool *enemyPool, Labels *labels)
{
    if (slot < 0 || slot >= SAVE_SLOTS)
        return false;

    char path[64];
    savePath(path, sizeof(path), slot);
    FILE *file = fopen(path, "rb");
    if (file == NULL)
        return false;

    struct SerializedGameData data;
    if (fread(&data, sizeof(data), 1, file) != 1                         //
        || data.grid.width <= 0 || data.grid.height <= 0                 //
        || data.grid.width > grid->width || data.grid.height > grid->height) // la grille doit tenir dans le terminal actuel
    {
        fclose(file);
        return false;
    }

    // On lit d'abord tout dans une nouvelle grille : si le fichier est abîmé, la partie en cours n'est pas touchée
    Grid loaded = data.grid;
    allocateGridCells(&loaded);
    bool ok = true;
    for (int x = 0; ok && x < loaded.width; x++)
        ok = fread(loaded.cells[x], sizeof(struct Cell), loaded.height, file) == (size_t)loaded.height;
    fclose(file);

    if (!ok)
    {
        for (int x = 0; x < loaded.width; x++)
            free(loaded.cells[x]);
        free(loaded.cells);
        return false;
    }

    for (int x = 0; x < loaded.width; x++)
        for (int y = 0; y < loaded.height; y++)
            loaded.cells[x][y].selected = false;

    // Tout est valide : on remplace la partie en cours
    for (int x = 0; x < grid->width; x++)
        free(grid->cells[x]);
    free(grid->cells);
    *grid = loaded;

    *gameStats = data.gameStats;
    switchToWave(waveSystem, data.waveIndex);
    enemyPool->count = 0;
    labels->count = 0;
    return true;
}
