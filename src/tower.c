#include "tower.h"
#include "save.h"
#include <string.h>

// Tous les bouts de programmes avec des # comme #if,#else... sont des morceaux de programmes utilisés uniquement sous l'activation du parametre BULLETS_ON dans ./src/tower.h

#if BULLETS_ON
void updateTowers(Grid grid, EnemyPool ep, BulletPool *bp, float dt, GameStats *gs)
{
#else
void updateTowers(Grid grid, EnemyPool ep, float dt, GameStats *gs)
{
#endif
    for (int x = 0; x < grid.width; x++)
    {
        for (int y = 0; y < grid.height; y++) // Pour chaque cellules du terrain (de taille grid.width*grid.height)
        {
            if (grid.cells[x][y].hasTurret) // Si la case a une tourelle, on la mets a jour
            {
                int lvl = grid.cells[x][y].turret.lvl; // Son niveau
                if (grid.cells[x][y].turret.has_malus)
                {
                    grid.cells[x][y].turret.puissance_malus -= dt;
                    if (grid.cells[x][y].turret.puissance_malus <= 0)
                    {
                        grid.cells[x][y].turret.has_malus = false;
                    }
                }
                else
                {
                    grid.cells[x][y].turret.compteur += dt; // Son horloge interne / son compteur
                }
                if (grid.cells[x][y].turret.type == Mortier)
                {
                    if (grid.cells[x][y].turret.compteur_mortier <= 0 && grid.cells[x][y].turret.compteur_mortier != -1)
                    {
                        int w;
                        int h;
                        get_terminal_size(&w, &h);
                        fillBG(1, 1, w + 1, h + 1);
                        drawFullGrid(grid);
                        grid.cells[x][y].turret.compteur_mortier = -1;
                    }
                    else if (grid.cells[x][y].turret.compteur_mortier > 0)
                    {
                        grid.cells[x][y].turret.compteur_mortier -= dt;
                    }
                }
                if (!grid.cells[x][y].turret.has_malus)
                {
                    // Lorsque l'horloge interne dépasse la vitesse de rechargement (reload) de la tourelle, elle applique son effet :
                    if (grid.cells[x][y].turret.compteur >= grid.cells[x][y].turret.reload_delay[lvl])
                    {
                        int enemies_hit = 0;
                        if (grid.cells[x][y].turret.has_effect)
                        {
                            if (grid.cells[x][y].turret.effet == Money) // Génération d'argent (Banque)
                            {
                                gs->cash += grid.cells[x][y].turret.puissance_effet[lvl];
                                enemies_hit++;
                            }
                            else if (grid.cells[x][y].turret.effet == Stun) // Stun = Pétrification (Pétrificateur)
                            {
                                for (int i = 0; i < ep.count; i++)
                                {
                                    if (ep.enemies[i].state != ENEMY_STATE_ALIVE || ep.enemies[i].hp <= 0)
                                        continue;
                                    // Calcul du vecteur entre la tourelle et l’ennemi
                                    float dx = ep.enemies[i].grid_x - (x + 0.5); // on rajoute 0.5 afin que l'on calcule depuis le centre de la tourelle et pas depuis le coin supérieur gauche de la case.
                                    float dy = ep.enemies[i].grid_y - (y + 0.5);

                                    float d = sqrt(dx * dx + dy * dy);
                                    if (d <= grid.cells[x][y].turret.range_max[lvl] && d >= grid.cells[x][y].turret.range_min[lvl])
                                    {
                                        if (ep.enemies[i].type == ENEMY_BOSS_STUN && d <= ep.enemies[i].puissance_effet)
                                        {
                                            grid.cells[x][y].turret.has_malus = true;
                                            grid.cells[x][y].turret.puissance_malus = ep.enemies[i].puissance_effet;
                                        }
                                        else
                                        {
                                            grid.cells[x][y].turret.last_shot_dx = dx / d;
                                            grid.cells[x][y].turret.last_shot_dy = dy / d;
                                            ep.enemies[i].has_effect = true;
                                            ep.enemies[i].effet = Stun;
                                            ep.enemies[i].temps_rest = grid.cells[x][y].turret.puissance_effet[lvl];
                                            enemies_hit++;
                                            break;
                                        }
                                    }
                                }
                            }
                            else if (grid.cells[x][y].turret.effet == Slow) // Slow = Ralentissement (Freezer)
                            {
                                for (int i = 0; i < ep.count; i++)
                                {
                                    if (ep.enemies[i].state != ENEMY_STATE_ALIVE || ep.enemies[i].hp <= 0)
                                        continue;
                                    // Calcul du vecteur entre la tourelle et l’ennemi
                                    float dx = ep.enemies[i].grid_x - (x + 0.5);
                                    float dy = ep.enemies[i].grid_y - (y + 0.5);

                                    float d = sqrt(dx * dx + dy * dy);
                                    if (d <= grid.cells[x][y].turret.range_max[lvl] && d >= grid.cells[x][y].turret.range_min[lvl])
                                    {
                                        if (ep.enemies[i].type == ENEMY_BOSS_STUN && d <= ep.enemies[i].puissance_effet)
                                        {
                                            grid.cells[x][y].turret.has_malus = true;
                                            grid.cells[x][y].turret.puissance_malus = ep.enemies[i].puissance_effet;
                                        }
                                        else
                                        {
                                            grid.cells[x][y].turret.last_shot_dx = dx / d;
                                            grid.cells[x][y].turret.last_shot_dy = dy / d;
                                            ep.enemies[i].has_effect = true;
                                            ep.enemies[i].effet = Slow;
                                            ep.enemies[i].temps_rest = lvl + 1;
                                            ep.enemies[i].slow_factor = grid.cells[x][y].turret.puissance_effet[lvl];
                                            enemies_hit++;
                                        }
                                    }
                                }
                            }
                            else if (grid.cells[x][y].turret.effet == Fire) // Fire = dégats sur la durée (Inferno)
                            {
                                for (int i = 0; i < ep.count; i++)
                                {
                                    if (ep.enemies[i].state != ENEMY_STATE_ALIVE || ep.enemies[i].hp <= 0)
                                        continue;
                                    // Calcul du vecteur entre la tourelle et l’ennemi
                                    float dx = ep.enemies[i].grid_x - (x + 0.5);
                                    float dy = ep.enemies[i].grid_y - (y + 0.5);
                                    float d = sqrt(dx * dx + dy * dy);

                                    if (d <= grid.cells[x][y].turret.range_max[lvl] && d >= grid.cells[x][y].turret.range_min[lvl])
                                    {
                                        if (ep.enemies[i].type == ENEMY_BOSS_STUN && d <= ep.enemies[i].puissance_effet)
                                        {
                                            grid.cells[x][y].turret.has_malus = true;
                                            grid.cells[x][y].turret.puissance_malus = ep.enemies[i].puissance_effet;
                                        }
                                        else
                                        {
                                            grid.cells[x][y].turret.last_shot_dx = dx / d;
                                            grid.cells[x][y].turret.last_shot_dy = dy / d;
                                            ep.enemies[i].has_effect = true;
                                            ep.enemies[i].effet = Fire;
                                            ep.enemies[i].temps_rest = lvl + 1;
                                            ep.enemies[i].temps_recharge = grid.cells[x][y].turret.puissance_effet[lvl];
                                            ep.enemies[i].last_hit = 10;
                                            enemies_hit++;
                                        }
                                    }
                                }
                            }
                        }
                        else
                        {
                            for (int i = 0; i < ep.count; i++)
                            {
                                if (ep.enemies[i].state != ENEMY_STATE_ALIVE || ep.enemies[i].hp <= 0)
                                    continue;
                                // Calcul du vecteur entre la tourelle et l’ennemi
                                float dx = ep.enemies[i].grid_x - (x + 0.5);
                                float dy = ep.enemies[i].grid_y - (y + 0.5);
                                float d = sqrt(dx * dx + dy * dy);

                                // Test pour que l'ennemi soit dans la portée de la tourelle
                                if (d <= grid.cells[x][y].turret.range_max[lvl] && d >= grid.cells[x][y].turret.range_min[lvl])
                                {
                                    if (ep.enemies[i].type == ENEMY_BOSS_STUN && d <= ep.enemies[i].puissance_effet)
                                    {
                                        grid.cells[x][y].turret.has_malus = true;
                                        grid.cells[x][y].turret.puissance_malus = ep.enemies[i].puissance_effet;
                                    }
                                    else
                                    {
                                        float dx = ep.enemies[i].grid_x - x;
                                        float dy = ep.enemies[i].grid_y - y;
                                        float d = sqrt(dx * dx + dy * dy);

                                        grid.cells[x][y].turret.last_shot_dx = dx / d;
                                        grid.cells[x][y].turret.last_shot_dy = dy / d;
                                        int rapport_speed_damage = grid.cells[x][y].turret.compteur / grid.cells[x][y].turret.reload_delay[lvl];
                                        // Fonctionnement du splash damage (dégats de zone)
                                        if (grid.cells[x][y].turret.splash[lvl] != 0.0)
                                        {
                                            float d_min = grid.cells[x][y].turret.splash[lvl];
                                            for (int j = 0; j < ep.count; j++)
                                            {
                                                if (j != i && ep.enemies[j].state == ENEMY_STATE_ALIVE)
                                                {
                                                    float d_enemy = sqrtf(powf(ep.enemies[i].grid_x - ep.enemies[j].grid_x, 2) +
                                                                          powf(ep.enemies[i].grid_y - ep.enemies[j].grid_y, 2));
                                                    if (d_enemy < d_min)
                                                    {
                                                        ep.enemies[j].hp -= grid.cells[x][y].turret.damage[lvl] / 2 * rapport_speed_damage;
                                                    }
                                                }
                                            }
                                            ep.enemies[i].hp -= grid.cells[x][y].turret.damage[lvl] * rapport_speed_damage;
                                            int w;
                                            int h;
                                            get_terminal_size(&w, &h);
                                            scr_printf(COLOR_MORTIER_FIRING);
                                            drawRange(w, h, d_min, ep.enemies[i].grid_x, ep.enemies[i].grid_y, false);
                                            grid.cells[x][y].turret.compteur_mortier = 0.5;
                                            scr_printf(RESET);
                                        }
                                        else // Tir standard
                                        {
#if BULLETS_ON
                                            bp->bullets[bp->count].hit = false;
                                            bp->bullets[bp->count].grid_x = x + 0.5 + (dx / d) / 4;
                                            bp->bullets[bp->count].grid_y = y + 0.5 + (dy / d) / 4;
                                            bp->bullets[bp->count].target = &(ep.enemies[i]);
                                            bp->bullets[bp->count].damage = grid.cells[x][y].turret.damage[lvl] * rapport_speed_damage;
                                            bp->count++;
#else
                                            // Changements des points de vie de l'ennemi touché, perdant ainsi le nombre de dégats infligés par l'attaque
                                            ep.enemies[i].hp -= grid.cells[x][y].turret.damage[lvl] * rapport_speed_damage;
#endif
                                        }
                                        // Fonctionnement du nombre d'ennemi pouvant être touché
                                        enemies_hit++;
                                        if (enemies_hit >= grid.cells[x][y].turret.nb_ennemi[lvl])
                                            break;
                                    }
                                }
                            }
                        }
                        if (enemies_hit == 0)
                        {
                            grid.cells[x][y].turret.in_range = false;
                        }
                        else
                        {
                            grid.cells[x][y].turret.in_range = true;
                        }
                        // On garde le temps "en trop" pour le prochain tir : la cadence reste exacte quel que soit le nombre d'images par seconde
                        // (les tirs "en retard" ont déjà été comptés dans rapport_speed_damage)
                        grid.cells[x][y].turret.compteur = fmodf(grid.cells[x][y].turret.compteur, grid.cells[x][y].turret.reload_delay[lvl]);
                    }
                }
            }
        }
    }
}

#if BULLETS_ON
void defragBullets(BulletPool *bp)
{
    int right = 0;
    int left = -1;

    while (right < bp->count)
    {
        if (!bp->bullets[right].hit)
        {
            if (left != -1)
            {
                assert(left >= 0);
                // assert(left < bp->length);

                bp->bullets[left] = bp->bullets[right];
                left++;
            }
        }
        else if (left == -1)
        {
            left = right;
        }

        right++;
    }
    bp->count -= right - left;
    if (bp->count < 0)
        bp->count = 0;
}

void drawBullets(BulletPool bp)
{
    for (int i = 0; i < bp.count; i++)
    {
        int terminal_x = bp.bullets[i].grid_x * (CELL_WIDTH + GAP) + 3;
        int terminal_y = bp.bullets[i].grid_y * (CELL_HEIGHT + GAP / 2) + GRID_TOP;
        move_to(terminal_x, terminal_y);

        scr_printf("⌾");
    }
}
void updateBullets(BulletPool *bp, float dt)
{
    float defrag_needed = false;
    for (int i = 0; i < bp->count; i++)
    {
        if (bp->bullets[i].hit)
            continue;

        float dx = bp->bullets[i].target->grid_x - bp->bullets[i].grid_x;
        float dy = bp->bullets[i].target->grid_y - bp->bullets[i].grid_y;
        float d = sqrt(dx * dx + dy * dy);
        bp->bullets[i].grid_x += (dx / d) * 10 * dt;
        bp->bullets[i].grid_y += (dy / d) * 10 * dt;

        if (d <= 0.1)
        {
            bp->bullets[i].target->hp -= bp->bullets[i].damage;
            bp->bullets[i].hit = true;
            defrag_needed = true;
        }
    }
    if (defrag_needed)
        defragBullets(bp);
};

#endif

// Prix des tourelles
int getTurretPrice(enum TurretType type, int level)
{
    if (type == Sniper)
    {
        if (level == 0)
        {
            return 10;
        }
        if (level == 1)
        {
            return 40;
        }
        if (level == 2)
        {
            return 80;
        }
    }
    if (type == Inferno)
    {
        if (level == 0)
        {
            return 20;
        }
        if (level == 1)
        {
            return 60;
        }
    }
    if (type == Mortier)
    {
        if (level == 0)
        {
            return 35;
        }
        if (level == 1)
        {
            return 80;
        }
    }
    if (type == Gatling)
    {
        if (level == 0)
        {
            return 35;
        }
        if (level == 1)
        {
            return 100;
        }
    }
    if (type == Freezer)
    {
        if (level == 0)
        {
            return 25;
        }
        if (level == 1)
        {
            return 70;
        }
    }
    if (type == Petrificateur)
    {
        if (level == 0)
        {
            return 10;
        }
        if (level == 1)
        {
            return 40;
        }
    }
    if (type == Banque)
    {
        if (level == 0)
        {
            return 100;
        }
        if (level == 1)
        {
            return 200;
        }
    }

    return -1;
}

// Definition de toutes les caractéristiques de chaque tourelle
struct Turret getTurretStruct(enum TurretType type)
{
    struct Turret tur;
    memset(&tur, 0, sizeof(tur)); // les niveaux non définis valent 0 (avant : valeurs aléatoires de la mémoire)
    tur.in_range = false;
    tur.has_malus = false;
    tur.compteur_mortier = -1;
    if (type == Sniper)
    {
        tur.type = Sniper;
        tur.lvl = 0;
        tur.compteur = 0;
        tur.range_min[0] = 0;
        tur.range_min[1] = 0;
        tur.range_min[2] = 0;
        tur.range_max[0] = 2.5;
        tur.range_max[1] = 4.5;
        tur.range_max[2] = 100;
        tur.damage[0] = 1;
        tur.damage[1] = 1.3;
        tur.damage[2] = 1.6;
        tur.reload_delay[0] = 1;
        tur.reload_delay[1] = 0.75;
        tur.reload_delay[2] = 0.75;
        tur.splash[0] = 0.0;
        tur.splash[1] = 0.0;
        tur.splash[2] = 0.0;
        tur.nb_ennemi[0] = 1;
        tur.nb_ennemi[1] = 1;
        tur.nb_ennemi[2] = 1;
        tur.has_effect = false;
    }
    else if (type == Inferno)
    {
        tur.type = Inferno;
        tur.lvl = 0;
        tur.compteur = 0;
        tur.range_min[0] = 0;
        tur.range_min[1] = 0;
        tur.range_max[0] = 1.5;
        tur.range_max[1] = 2.5;
        tur.damage[0] = 0.5;
        tur.damage[1] = 0.5;
        tur.reload_delay[0] = 1;
        tur.reload_delay[1] = 1;
        tur.splash[0] = 0.0;
        tur.splash[1] = 0.0;
        tur.nb_ennemi[0] = 4;
        tur.nb_ennemi[1] = 8;
        tur.has_effect = true;
        tur.effet = Fire;
        tur.puissance_effet[0] = 0.5;
        tur.puissance_effet[1] = 0.5;
    }
    else if (type == Mortier)
    {
        tur.type = Mortier;
        tur.lvl = 0;
        tur.compteur = 0;
        tur.range_min[0] = 3;
        tur.range_min[1] = 2.5;
        tur.range_max[0] = 5.5;
        tur.range_max[1] = 6.5;
        tur.damage[0] = 2;   // 4/4 -> 1dps
        tur.damage[1] = 2.5; // 5/3 -> 1.42dps
        tur.reload_delay[0] = 4;
        tur.reload_delay[1] = 3.5;
        tur.splash[0] = 1.0;
        tur.splash[1] = 1.5;
        tur.nb_ennemi[0] = 1;
        tur.nb_ennemi[1] = 1;
        tur.has_effect = false;
    }
    else if (type == Gatling)
    {
        tur.type = Gatling;
        tur.lvl = 0;
        tur.compteur = 0;
        tur.range_min[0] = 0;
        tur.range_min[1] = 0;
        tur.range_max[0] = 1.5;
        tur.range_max[1] = 2;
        tur.damage[0] = 0.2;
        tur.damage[1] = 0.3;
        tur.reload_delay[0] = 0.05;
        tur.reload_delay[1] = 0.03;
        tur.splash[0] = 0;
        tur.splash[1] = 0;
        tur.nb_ennemi[0] = 1;
        tur.nb_ennemi[1] = 1;
        tur.has_effect = false;
    }
    else if (type == Petrificateur)
    {
        tur.type = Petrificateur;
        tur.lvl = 0;
        tur.compteur = 0;
        tur.range_min[0] = 0;
        tur.range_min[1] = 0;
        tur.range_max[0] = 1.5;
        tur.range_max[1] = 2;
        tur.damage[0] = 0;
        tur.damage[1] = 0;
        tur.reload_delay[0] = 2;
        tur.reload_delay[1] = 1.5;
        tur.splash[0] = 0;
        tur.splash[1] = 0;
        tur.nb_ennemi[0] = 1;
        tur.nb_ennemi[1] = 1;
        tur.has_effect = true;
        tur.effet = Stun;
        tur.puissance_effet[0] = 1;
        tur.puissance_effet[1] = 1;
    }
    else if (type == Freezer)
    {
        tur.type = Freezer;
        tur.lvl = 0;
        tur.compteur = 0;
        tur.range_min[0] = 0;
        tur.range_min[1] = 0;
        tur.range_max[0] = 1.5;
        tur.range_max[1] = 2.5;
        tur.damage[0] = 0;
        tur.damage[1] = 0;
        tur.reload_delay[0] = 3;
        tur.reload_delay[1] = 3;
        tur.splash[0] = 0;
        tur.splash[1] = 0;
        tur.nb_ennemi[0] = 100;
        tur.nb_ennemi[1] = 100;
        tur.has_effect = true;
        tur.effet = Slow;
        tur.puissance_effet[0] = 0.90;
        tur.puissance_effet[1] = 0.75;
    }
    else if (type == Banque)
    {
        tur.type = Banque;
        tur.lvl = 0;
        tur.compteur = 0;
        tur.reload_delay[0] = 5;
        tur.reload_delay[1] = 10;
        tur.has_effect = true;
        tur.effet = Money;
        tur.puissance_effet[0] = 2;
        tur.puissance_effet[1] = 6;
    }
    return tur;
}

/*
####################
## Fenêtres (menus) ##
####################
*/

// Ordre des tourelles dans la boutique (ligne 1 à 7)
static const enum TurretType SHOP_ORDER[SHOP_SIZE] = {Sniper, Inferno, Mortier, Gatling, Petrificateur, Freezer, Banque};

enum TurretType shopTurretAt(int ligne)
{
    return SHOP_ORDER[CLAMP(ligne, 1, SHOP_SIZE) - 1];
}

const char *turretName(enum TurretType type)
{
    switch (type)
    {
    case Sniper: return "Sniper";
    case Inferno: return "Inferno";
    case Mortier: return "Mortier";
    case Gatling: return "Gatling";
    case Petrificateur: return "Pétrificateur";
    case Freezer: return "Freezer";
    case Banque: return "Banque";
    }
    return "?";
}

static const char *turretColor(enum TurretType type)
{
    switch (type)
    {
    case Sniper: return COLOR_SNIPER_BASE;
    case Inferno: return COLOR_INFERNO_BASE;
    case Mortier: return "\033[38;5;250m";
    case Gatling: return "\033[38;5;245m";
    case Petrificateur: return COLOR_PETRIFICATEUR_FIRING;
    case Freezer: return COLOR_FREEZER_BASE;
    case Banque: return COLOR_BANQUE_MONEY;
    }
    return "";
}

static const char *turretDescription(enum TurretType type)
{
    switch (type)
    {
    case Sniper: return "Grande portée, gros dégâts, tir lent.";
    case Inferno: return "Brûle jusqu'à 4 ennemis proches.";
    case Mortier: return "Explosion de zone. Inutile de trop près.";
    case Gatling: return "Tir très rapide, courte portée.";
    case Petrificateur: return "Fige un ennemi sur place.";
    case Freezer: return "Ralentit les ennemis autour de lui.";
    case Banque: return "Rapporte de l'argent régulièrement.";
    }
    return "";
}

// Dessine une fenêtre (cadre + fond noir) de w x h caractères
void drawPanel(int x0, int y0, int w, int h, const char *border_color)
{
    for (int y = 0; y < h; y++)
    {
        move_to(x0, y0 + y);
        scr_printf(RESET "%s", border_color);
        for (int x = 0; x < w; x++)
        {
            if (x == 0 || x == w - 1)
                scr_printf("█");
            else if (y == 0)
                scr_printf("▀");
            else if (y == h - 1)
                scr_printf("▄");
            else
                scr_printf(" ");
        }
    }
    scr_printf(RESET);
}

// Une ligne de menu : flèche si sélectionnée, texte à gauche, valeur alignée à droite
static void menuLine(int x_left, int x_right, int y, bool selected, const char *label, const char *value)
{
    scr_printf(RESET);
    ui_text(x_left, y, selected ? COLOR_SELECTED_SLOT "▶" : " ");
    int arrow = ui_text_width("▶") + (big_text_mode ? 3 : 1);
    int value_x = x_right - ui_text_width(value);
    ui_text_fit(x_left + arrow, y, value_x - (x_left + arrow) - 2, label);
    ui_text(value_x, y, value);
    scr_printf(RESET);
}

/*
Toutes les fenêtres (boutique, amélioration, sauvegardes, fin de partie) ont la même taille,
pour que l'interface ne "saute" pas d'un menu à l'autre ou quand on change de sélection.
La taille est calculée pour le contenu le plus grand : la boutique avec la plus longue description.
*/
void menuGeometry(int *x0, int *y0, int *w, int *h)
{
    int line_h = ui_text_rows() + 1;
    *w = big_text_mode ? 150 : 64;
    int max_desc = 0;
    for (int i = 1; i <= SHOP_SIZE; i++)
        max_desc = MAX(max_desc, ui_text_wrapped_rows(*w - 8, 1, turretDescription(shopTurretAt(i))));
    *h = line_h * (SHOP_SIZE + 2) + max_desc + 11;
    *x0 = width_center() - *w / 2;
    *y0 = height_center() - *h / 2;
}

// Position de la première des deux lignes d'aide, en bas de la fenêtre
static int footerY(int y0, int h)
{
    return y0 + h - 2 - (ui_text_rows() + 1) - ui_text_rows();
}

// Ouvre une fenêtre standard avec son titre ; renvoie la ligne où commence le contenu
static int openMenu(const char *title, const char *border_color, int *left, int *right, int *x0_out, int *y0_out, int *h_out)
{
    int x0, y0, w, h;
    menuGeometry(&x0, &y0, &w, &h);
    drawPanel(x0, y0, w, h, border_color);
    *left = x0 + 4;
    *right = x0 + w - 5;
    if (x0_out)
        *x0_out = x0;
    if (y0_out)
        *y0_out = y0;
    if (h_out)
        *h_out = h;
    if (ui_text_width(title) <= w - 8)
        ui_text_centered(x0 + w / 2, y0 + 2, title);
    else
        ui_text_fit(x0 + 4, y0 + 2, w - 8, title);
    scr_printf(RESET);
    return y0 + 2 + ui_text_rows() + 2;
}

static void menuFooter(int left, int y0, int h, const char *line1, const char *line2)
{
    int fy = footerY(y0, h);
    scr_printf(RESET);
    int x0, y0_, w, h_;
    menuGeometry(&x0, &y0_, &w, &h_);
    int max_w = w - 8;
    ui_text_fit(left, fy, max_w, line1);
    ui_text_fit(left, fy + ui_text_rows() + 1, max_w, line2);
    scr_printf(RESET);
}

void showSaveScreen(int ligne)
{
    int line_h = ui_text_rows() + 1;
    int left, right, y0, h;
    int y = openMenu(COLOR_YELLOW "Sauvegardes", "\033[38;5;250m", &left, &right, NULL, &y0, &h);
    for (int slot = 0; slot < SAVE_SLOTS; slot++)
    {
        char label[32];
        snprintf(label, sizeof(label), slot == 0 ? "Emplacement 0 (auto)" : "Emplacement %d", slot);
        menuLine(left, right, y, ligne - 3 == slot, label, saveExists(slot) ? COLOR_GREEN "sauvegardé" : COLOR_GRAY "vide");
        y += line_h;
    }
    menuFooter(left, y0, h, COLOR_GRAY "Entrée : charger   Espace : sauver", COLOR_GRAY "Échap ou S : fermer");
}

void showTowerSelection(int ligne, bool hasTurret, struct Turret selectedTurret, int cash)
{
    int line_h = ui_text_rows() + 1;
    int left, right, x0, y0, h;

    if (hasTurret)
    {
        // Menu d'amélioration / vente
        char title[64];
        snprintf(title, sizeof(title), "%s%s  niv. %d", turretColor(selectedTurret.type), turretName(selectedTurret.type), selectedTurret.lvl + 1);
        int y = openMenu(title, turretColor(selectedTurret.type), &left, &right, &x0, &y0, &h);

        int next_price = getTurretPrice(selectedTurret.type, selectedTurret.lvl + 1);
        char value[32];
        if (next_price > 0)
        {
            snprintf(value, sizeof(value), "%s%d €", cash >= next_price ? COLOR_YELLOW : COLOR_GRAY, next_price);
            menuLine(left, right, y, ligne == 1, cash >= next_price ? "Améliorer" : COLOR_GRAY "Améliorer", value);
        }
        else
            menuLine(left, right, y, ligne == 1, COLOR_GRAY "Améliorer", COLOR_GRAY "niveau max");
        y += line_h;

        snprintf(value, sizeof(value), COLOR_GREEN "+%d €", (int)(getTurretPrice(selectedTurret.type, selectedTurret.lvl) * 0.8));
        menuLine(left, right, y, ligne == 2, "Vendre", value);
        y += line_h + 1;
        scr_printf(RESET);
        ui_text_wrapped(left, y, right - left, 1, turretDescription(selectedTurret.type));
        menuFooter(left, y0, h, COLOR_GRAY "Entrée : valider", COLOR_GRAY "Espace : fermer");
    }
    else
    {
        // Boutique
        int y = openMenu(COLOR_YELLOW "Boutique", COLOR_YELLOW, &left, &right, &x0, &y0, &h);
        for (int i = 1; i <= SHOP_SIZE; i++)
        {
            enum TurretType type = shopTurretAt(i);
            int price = getTurretPrice(type, 0);
            bool affordable = cash >= price;
            char label[64], value[32];
            snprintf(label, sizeof(label), "%s%s", affordable ? turretColor(type) : COLOR_GRAY, turretName(type));
            snprintf(value, sizeof(value), "%s%d €", affordable ? COLOR_YELLOW : COLOR_GRAY, price);
            menuLine(left, right, y, ligne == i, label, value);
            y += line_h;
        }
        y += 1;
        scr_printf(RESET);
        ui_text_wrapped(left, y, right - left, 1, turretDescription(shopTurretAt(ligne)));
        menuFooter(left, y0, h, COLOR_GRAY "Entrée : acheter", COLOR_GRAY "Espace : fermer");
    }
}

/*
##############
## Menu pause ##
##############
*/

static const char *PAUSE_LABELS[PAUSE_ITEMS] = {"Reprendre", "Tutoriel", "Sauvegardes", "Commandes", "Quitter"};

void showPauseMenu(int ligne)
{
    int line_h = ui_text_rows() + 1;
    int left, right, y0, h;
    int y = openMenu(COLOR_YELLOW "Pause", "\033[38;5;250m", &left, &right, NULL, &y0, &h);
    y += line_h;
    for (int i = 1; i <= PAUSE_ITEMS; i++)
    {
        menuLine(left, right, y, ligne == i, PAUSE_LABELS[i - 1], i == PAUSE_ITEMS ? COLOR_GRAY "sauvé en 0" : "");
        y += line_h + 1;
    }
    menuFooter(left, y0, h, COLOR_GRAY "Entrée : valider", COLOR_GRAY "Échap : reprendre");
}

void showControls(void)
{
    static const char *keys[][2] = {
        {"Flèches", "sélection"},
        {"Espace", "boutique"},
        {"Entrée", "valider"},
        {"O / L", "vitesse + / -"},
        {"S", "sauvegardes"},
        {"Échap", "pause"},
        {"Q", "quitter (sauvé en 0)"},
    };
    int line_h = ui_text_rows() + 1;
    int left, right, y0, h;
    int y = openMenu(COLOR_YELLOW "Commandes", "\033[38;5;250m", &left, &right, NULL, &y0, &h);
    int key_w = 0;
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++)
        key_w = MAX(key_w, ui_text_width(keys[i][0]));
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++)
    {
        char key[32];
        snprintf(key, sizeof(key), COLOR_YELLOW "%s", keys[i][0]);
        scr_printf(RESET);
        ui_text(left, y, key);
        scr_printf(RESET);
        int desc_x = left + key_w + (big_text_mode ? 8 : 3);
        ui_text_fit(desc_x, y, right - desc_x, keys[i][1]);
        y += line_h;
    }
    menuFooter(left, y0, h, "", COLOR_GRAY "Échap ou Entrée : retour");
}
