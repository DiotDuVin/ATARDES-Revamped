#pragma once

/*
##############
EFFETS DE TIR
##############

Animations courtes déclenchées par les tirs des tourelles : éclair du Sniper, traçantes de la Gatling,
anneau de flammes de l'Inferno, explosion du Mortier, onde de givre du Freezer, rayon du Pétrificateur,
pièces de la Banque.

Les effets sont gardés dans une liste, avancent avec le temps de jeu (ils se figent pendant la pause)
et sont dessinés à chaque image par-dessus le plateau et les ennemis, au pixel près (voir pixelart.c).
*/

#include "shared.h"
#include "enemies.h"

// Déclenchement (x, y : case de la tourelle)
void fx_sniper_shot(int x, int y, const struct Enemy *target);
void fx_gatling_shot(int x, int y, const struct Enemy *target);
void fx_mortar_explosion(const struct Enemy *target, float splash_radius);
void fx_flame_ring(int x, int y, float range);
void fx_frost_wave(int x, int y, float range);
void fx_stun_beam(int x, int y, const struct Enemy *target);
void fx_coins(int x, int y, int amount);

void fx_update(float dt); // fait avancer et supprime les effets terminés
void fx_draw(void);       // dessine les effets en cours
void fx_clear(void);      // supprime tous les effets (chargement d'une partie)
