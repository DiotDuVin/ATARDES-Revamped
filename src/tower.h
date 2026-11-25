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

// Modes de ciblage (dans quel ordre une tourelle choisit les ennemis à portée)
enum TargetMode
{
    TARGET_FIRST,  // le plus avancé vers la sortie
    TARGET_LAST,   // le moins avancé
    TARGET_STRONG, // le plus de PV
    TARGET_WEAK,   // le moins de PV
    TARGET_CLOSE,  // le plus proche de la tourelle
    TARGET_MODE_COUNT,
};
#define TARGET_MAX 512 // nombre maximal d'ennemis pris en compte pour un tir
const char *targetModeName(int mode);
bool turretHasTargeting(enum TurretType type);
int sortTargets(Grid grid, EnemyPool ep, int x, int y, int mode, int order[TARGET_MAX]);

// Durée de la brûlure de l'Inferno selon son niveau (en secondes)
// Nombre maximal d'ennemis touchés par une explosion de Mortier, en plus de sa cible
#define MORTAR_SPLASH_MAX 3

#define INFERNO_BURN_TIME(lvl) ((lvl) + 2)

#define SHOP_SIZE 7 // nombre de tourelles dans la boutique

void showTowerSelection(int ligne, bool hasTurret, struct Turret selected_tower, int cash);
// Tourelle affichée à la ligne `ligne` (1 à SHOP_SIZE) de la boutique
enum TurretType shopTurretAt(int ligne);
const char *turretName(enum TurretType type);
void drawPanel(int x0, int y0, int w, int h, const char *border_color);
// Menu pause (Échap) : Reprendre, Tutoriel, Sauvegardes, Commandes, Quitter
#define PAUSE_ITEMS 6
enum PauseChoice
{
    PAUSE_RESUME = 1,
    PAUSE_TUTORIAL,
    PAUSE_SAVES,
    PAUSE_CONTROLS,
    PAUSE_SCORES,
    PAUSE_QUIT,
};
void showPauseMenu(int ligne);
// Liste des touches du jeu (consultation seulement)
void showControls(void);
// Meilleurs scores
void showScores(void);
void drawScoresList(int left, int right, int y, int max_rows, int highlight);
// Panneau de statistiques (niveau actuel -> suivant) à droite de la boutique et du menu d'amélioration
extern bool menu_side_panel;
int sidePanelWidth(void);
void showStatsPanel(enum TurretType type, int lvl);
// Souris : position dans les menus
bool menuContains(int mx, int my);
int menuLineAt(int mx, int my, int skip, int count);
// Position et taille communes à toutes les fenêtres du jeu
void menuGeometry(int *x0, int *y0, int *w, int *h);
void showSaveScreen(int ligne);
// Fenêtre de choix pour un emplacement de sauvegarde
#define SAVE_ACTIONS 3
enum SaveAction
{
    SAVE_ACTION_LOAD,
    SAVE_ACTION_SAVE,
    SAVE_ACTION_CANCEL,
};
void showSaveDialog(int slot, int choice);
bool saveSlotLoadable(int slot);
bool saveDialogContains(int mx, int my);
int saveDialogLineAt(int mx, int my); // ligne (SaveAction) sous la souris, -1 si aucune
struct Turret getTurretStruct(enum TurretType type);
int getTurretPrice(enum TurretType type, int level);

#if BULLETS_ON
void updateTowers(Grid grid, EnemyPool ep, BulletPool *bp, float dt, GameStats *gs);
void drawBullets(BulletPool bp);
void updateBullets(BulletPool *bp, float dt);
#else
void updateTowers(Grid grid, EnemyPool ep, float dt, GameStats *gs);
#endif