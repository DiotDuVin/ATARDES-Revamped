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

int saveWave(int slot)
{
    char path[64];
    savePath(path, sizeof(path), slot);
    FILE *file = fopen(path, "rb");
    if (file == NULL)
        return -1;
    struct SerializedGameData data;
    bool ok = fread(&data, sizeof(data), 1, file) == 1 && data.magic == SAVE_MAGIC && data.version == SAVE_VERSION //
              && data.waveIndex >= 0 && data.waveIndex <= 100000 && data.gameStats.health > 0;
    fclose(file);
    return ok ? data.waveIndex : -1;
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
    data.magic = SAVE_MAGIC;
    data.version = SAVE_VERSION;
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

// Vérifie qu'une case chargée depuis un fichier est cohérente (fichier abîmé ou d'une autre version)
static bool cellIsValid(const struct Cell *cell, int x, int y)
{
    if (cell->x != x || cell->y != y || (cell->type != CHEMIN && cell->type != TERRAIN))
        return false;
    if (!cell->hasTurret)
        return true;
    const struct Turret *t = &cell->turret;
    if (cell->type != TERRAIN || t->type < Sniper || t->type > Gatling || t->lvl < 0 || t->lvl > 2 || t->target_mode < 0 || t->target_mode >= 5)
        return false;
    // Statistiques du niveau actuel (le "!" refuse aussi les valeurs NaN)
    return t->reload_delay[t->lvl] > 0 && t->range_max[t->lvl] >= 0 && t->nb_ennemi[t->lvl] >= 0;
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
        || data.magic != SAVE_MAGIC || data.version != SAVE_VERSION      //
        || data.waveIndex < 0 || data.waveIndex > 100000                 //
        || data.gameStats.health <= 0                                    //
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

    // Contenu des cases et position du chemin
    for (int x = 0; ok && x < loaded.width; x++)
        for (int y = 0; ok && y < loaded.height; y++)
            ok = cellIsValid(&loaded.cells[x][y], x, y);
    ok = ok && loaded.start_x == 0 && loaded.start_y >= 0 && loaded.start_y < loaded.height //
         && loaded.cells[loaded.start_x][loaded.start_y].type == CHEMIN;
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
    numberPath(grid); // ordre de parcours du chemin (animation des impulsions)

    *gameStats = data.gameStats;
    switchToWave(waveSystem, data.waveIndex);
    enemyPool->count = 0;
    labels->count = 0;
    return true;
}

/*
## Meilleurs scores ##
*/

static const char *scores_path = "./save/scores.txt";

// Ordre du classement : plus de vagues d'abord ; à égalité, la partie la plus rapide
static int compareScores(const void *a, const void *b)
{
    const Score *sa = a, *sb = b;
    if (sa->wave != sb->wave)
        return sb->wave - sa->wave;
    return sa->seconds - sb->seconds;
}

int loadScores(Score scores[MAX_SCORES])
{
    FILE *f = fopen(scores_path, "r");
    if (f == NULL)
        return 0;
    int n = 0;
    Score s;
    while (n < MAX_SCORES && fscanf(f, "%d %d %10s", &s.wave, &s.seconds, s.date) == 3)
        if (s.wave >= 0 && s.seconds >= 0)
            scores[n++] = s;
    fclose(f);
    qsort(scores, n, sizeof(Score), compareScores);
    return n;
}

int recordScore(int wave, int seconds)
{
    Score scores[MAX_SCORES + 1];
    int n = loadScores(scores);

    Score current = {wave, seconds, ""};
    time_t now = time(NULL);
    strftime(current.date, sizeof(current.date), "%Y-%m-%d", localtime(&now));
    scores[n++] = current;
    qsort(scores, n, sizeof(Score), compareScores);

    // rang de la partie qui vient de se terminer (la première égale trouvée)
    int rank = -1;
    for (int i = 0; i < n && i < MAX_SCORES; i++)
        if (scores[i].wave == wave && scores[i].seconds == seconds && strcmp(scores[i].date, current.date) == 0)
        {
            rank = i;
            break;
        }

    struct stat st;
    if (stat(save_dir, &st) != 0)
        mkdir(save_dir, 0775);
    FILE *f = fopen(scores_path, "w");
    if (f == NULL)
        return -1;
    for (int i = 0; i < n && i < MAX_SCORES; i++)
        fprintf(f, "%d %d %s\n", scores[i].wave, scores[i].seconds, scores[i].date);
    fclose(f);
    return rank;
}
