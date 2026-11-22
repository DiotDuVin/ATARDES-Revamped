#pragma once

/*
##########################
PIXEL ART (demi-caractères)
##########################

Chaque caractère du terminal est utilisé comme DEUX pixels superposés :
le caractère "▀" affiche sa couleur de texte en haut et sa couleur de fond en bas.
Comme un caractère est environ deux fois plus haut que large, ces pixels sont presque carrés.

Une case du plateau (18 x 9 caractères) devient ainsi une toile de 18 x 18 pixels
en 256 couleurs : c'est ce qui permet des tourelles et des ennemis bien plus détaillés,
des rotations continues (le canon suit l'ennemi) et des animations fluides.

Les dessins se font d'abord dans une "toile" (Canvas) en mémoire, puis la toile est
envoyée au moteur d'affichage avec canvas_draw.
*/

#include <stdbool.h>
#include "shared.h"
#include "enemies.h"

#define CANVAS_MAX 32
#define TRANSPARENT -1

typedef struct Canvas
{
    int w, h; // taille en pixels (h = 2 x nombre de lignes du terminal)
    short px[CANVAS_MAX * CANVAS_MAX];
} Canvas;

// Horloge des animations (en secondes, temps réel), mise à jour par la boucle de jeu
extern float anim_clock;

void canvas_init(Canvas *c, int w, int h);
void canvas_set(Canvas *c, int x, int y, short color);
short canvas_get(const Canvas *c, int x, int y);

// Envoie la toile au terminal, coin haut-gauche en (term_x, term_y).
// Les pixels transparents prennent la couleur `background` (TRANSPARENT : fond par défaut du terminal). Si skip_empty est vrai,
// les caractères entièrement transparents ne sont pas dessinés (on voit ce qu'il y avait dessous).
void canvas_draw(const Canvas *c, int term_x, int term_y, short background, bool skip_empty);

// Une case de terrain complète : emplacement vide ou tourelle, avec le cadre de sélection
void draw_terrain_cell(struct Cell cell);

// Aperçu d'une tourelle (pour le tutoriel et les menus) : 14 x 7 caractères, fond `background`
void draw_turret_preview(int term_x, int term_y, enum TurretType type, int lvl, short background);
// Même chose, agrandi `scale` fois (scale = 2 : 28 x 14 caractères)
void draw_turret_preview_scaled(int term_x, int term_y, enum TurretType type, int lvl, short background, int scale);

// Un ennemi, à sa position sur la grille
void draw_enemy(const struct Enemy *enemy, Grid grid);

// Dessine un pixel par-dessus ce qui est déjà affiché (px = colonne, py = 2 x ligne, +1 pour le bas du caractère)
void overlay_pixel(int px, int py, short color);
// Centre d'une case et d'un ennemi, en pixels
void cell_center_px(int x, int y, float *px, float *py);
void enemy_center_px(const struct Enemy *e, float *px, float *py);
// Cercle de portée en pointillés animés autour de la case (rien si range <= 0 ou portée infinie)
void draw_range_ring(int cell_x, int cell_y, float range, short color);
// Case du chemin (bus de données avec impulsions lumineuses)
void draw_path_cell(struct Cell cell, Grid grid);
