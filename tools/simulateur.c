// Simulateur de parties sans affichage : des "bots" jouent avec différentes stratégies,
// pour vérifier l'équilibrage du jeu. Usage : ./tools/equilibrage.sh (voir README)
#include "shared.h"
#include "grid.h"
#include "enemies.h"
#include "tower.h"
#include "effects.h"
#include <string.h>

static int path_cells_in_range(Grid *g, int x, int y, float range, float rmin)
{
    int n = 0;
    for (int i = 0; i < g->width; i++)
        for (int j = 0; j < g->height; j++)
            if (g->cells[i][j].type == CHEMIN)
            {
                float dx = i + 0.4f - (x + 0.5f), dy = j + 0.4f - (y + 0.5f), d = sqrtf(dx * dx + dy * dy);
                if (d <= range && d >= rmin) n++;
            }
    return n;
}

// Meilleur emplacement libre pour une tourelle (le plus de chemin à portée)
static bool best_slot(Grid *g, enum TurretType t, int *bx, int *by)
{
    struct Turret tur = getTurretStruct(t);
    int best = -1;
    for (int x = 0; x < g->width; x++)
        for (int y = 0; y < g->height; y++)
        {
            if (g->cells[x][y].type != TERRAIN || g->cells[x][y].hasTurret) continue;
            int score = t == Banque ? 100 - path_cells_in_range(g, x, y, 2, 0) : path_cells_in_range(g, x, y, tur.range_max[0], tur.range_min[0]);
            if (score > best) { best = score; *bx = x; *by = y; }
        }
    return best > 0;
}

int main(int argc, char **argv)
{
    const char *strategy = argc > 1 ? argv[1] : "mix";
    unsigned seed = argc > 2 ? atoi(argv[2]) : 1;
    int max_wave = argc > 3 ? atoi(argv[3]) : 80;
    scr_init(240, 80); // le moteur d'affichage absorbe les dessins faits pendant la simulation
    grid_top = 5;
    srand(seed);
    Grid grid = {0};
    grid.width = 11; grid.height = 7;
    allocateGridCells(&grid);
    genBasicPath(&grid);
    GameStats gs = {30, 20};
    Labels labels = {255, 0, malloc(sizeof(struct Label) * 255)};
    EnemyPool ep = AllocEnemyPool();
    WaveSystem ws;
    switchToWave(&ws, 0);

    // ordre d'achat selon la stratégie
    enum TurretType mono = Sniper;
    bool is_mono = true;
    if (!strcmp(strategy, "sniper")) mono = Sniper;
    else if (!strcmp(strategy, "gatling")) mono = Gatling;
    else if (!strcmp(strategy, "mortier")) mono = Mortier;
    else if (!strcmp(strategy, "inferno")) mono = Inferno;
    else is_mono = false;
    enum TurretType mix[] = {Sniper, Gatling, Sniper, Mortier, Freezer, Inferno, Gatling, Petrificateur, Mortier, Sniper};
    int mix_i = 0;
    bool bank_first = !strcmp(strategy, "banque");
    int banks_wanted = bank_first ? 2 : 0, banks = 0;
    // stratégie banque : 4 tourelles pour se défendre, puis 2 banques, puis le mélange

    float dt = 1.0f / 60, t = 0;
    int built = 0;
    while (gs.health > 0 && ws.current_wave_index < max_wave && t < 20000)
    {
        // Décisions du bot (une fois par seconde de jeu)
        if ((int)(t * 60) % 60 == 0)
        {
            for (int tries = 0; tries < 4; tries++)
            {
                enum TurretType want = (banks < banks_wanted && built >= 6) ? Banque : (is_mono ? mono : mix[mix_i % 10]);
                int price = getTurretPrice(want, 0), x, y;
                // améliorer la tourelle la moins chère à améliorer si c'est moins cher qu'un achat (ou plus de place)
                int ux = -1, uy = -1, uprice = 1 << 30;
                for (int i = 0; i < grid.width; i++)
                    for (int j = 0; j < grid.height; j++)
                        if (grid.cells[i][j].hasTurret && grid.cells[i][j].turret.type != Banque)
                        {
                            int p = getTurretPrice(grid.cells[i][j].turret.type, grid.cells[i][j].turret.lvl + 1);
                            if (p > 0 && p < uprice) { uprice = p; ux = i; uy = j; }
                        }
                bool slot = best_slot(&grid, want, &x, &y);
                if (slot && gs.cash >= price && (built < 6 || price <= uprice * 0.9f || ux < 0))
                {
                    gs.cash -= price;
                    grid.cells[x][y].turret = getTurretStruct(want);
                    grid.cells[x][y].hasTurret = true;
                    built++;
                    if (want == Banque) banks++; else mix_i++;
                }
                else if (ux >= 0 && gs.cash >= uprice)
                {
                    gs.cash -= uprice;
                    grid.cells[ux][uy].turret.lvl++;
                }
                else break;
            }
        }
        updateTowers(grid, ep, dt, &gs);
        updateLabels(&labels, dt);
        updateWaveSystem(&ws, grid, &ep, dt, &gs);
        updateEnemies(&ep, grid, &gs, &labels, dt);
        fx_update(dt);
        t += dt;
    }
    printf("%s %u vague=%d tourelles=%d temps=%.0fs\n", strategy, seed, ws.current_wave_index, built, t);
    return 0;
}
