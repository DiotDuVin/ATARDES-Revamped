#pragma once

#include "shared.h"
#include "enemies.h"
// true: les balles tirées par les tourelles seront affichées
#define BULLETS_ON false

#if BULLETS_ON

struct Bullet
{
    struct Enemy *target;
    float grid_x;
    float grid_y;
    float damage;
    float hit;
};
typedef struct BulletPool
{
    struct Bullet bullets[0xff];
    int count;
} BulletPool;

#endif

#define SHOP_SIZE 7 // nombre de tourelles dans la boutique

void showTowerSelection(int ligne, bool hasTurret, struct Turret selected_tower, int cash);
// Tourelle affichée à la ligne `ligne` (1 à SHOP_SIZE) de la boutique
enum TurretType shopTurretAt(int ligne);
const char *turretName(enum TurretType type);
void drawPanel(int x0, int y0, int w, int h, const char *border_color);
// Menu pause (Échap) : Reprendre, Tutoriel, Sauvegardes, Commandes, Quitter
#define PAUSE_ITEMS 5
enum PauseChoice
{
    PAUSE_RESUME = 1,
    PAUSE_TUTORIAL,
    PAUSE_SAVES,
    PAUSE_CONTROLS,
    PAUSE_QUIT,
};
void showPauseMenu(int ligne);
// Liste des touches du jeu (consultation seulement)
void showControls(void);
// Position et taille communes à toutes les fenêtres du jeu
void menuGeometry(int *x0, int *y0, int *w, int *h);
void showSaveScreen(int ligne);
struct Turret getTurretStruct(enum TurretType type);
int getTurretPrice(enum TurretType type, int level);

#if BULLETS_ON
void updateTowers(Grid grid, EnemyPool ep, BulletPool *bp, float dt, GameStats *gs);
void drawBullets(BulletPool bp);
void updateBullets(BulletPool *bp, float dt);
#else
void updateTowers(Grid grid, EnemyPool ep, float dt, GameStats *gs);
#endif