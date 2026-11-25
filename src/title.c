#include "title.h"
#include "shared.h"
#include "grid.h"
#include "tower.h"
#include "enemies.h"
#include "effects.h"
#include "pixelart.h"
#include "save.h"
#include "tutorial.h"
#include "intro.h"
#include "bigtext.h"
#include "render.h"

#include <string.h>

/*
############
ÉCRAN TITRE
############

Mise en page (de haut en bas) : logo, sous-titre, fenêtre du menu, record, puis la démo
qui occupe le bas de l'écran (2 ou 3 rangées de cases selon la hauteur du terminal).

La démo utilise une vraie grille du jeu : une rangée de chemin entourée de tourelles.
Pour la dessiner en bas de l'écran, on décale temporairement grid_top (la ligne où commence
le plateau) pendant sa mise à jour et son affichage.
*/

enum TitleItem
{
    ITEM_CONTINUE,
    ITEM_NEW_GAME,
    ITEM_TUTORIAL,
    ITEM_SCORES,
    ITEM_QUIT,
    ITEM_COUNT,
};

static const char *ITEM_LABELS[ITEM_COUNT] = {"Continuer", "Nouvelle partie", "Tutoriel", "Meilleurs scores", "Quitter"};

#define TITLE_FPS 60
#define TITLE_HELP COLOR_GRAY "↑ ↓ choisir   Entrée valider"
#define DEMO_SLIDE_TIME 0.6 // durée de l'apparition de la démo (elle monte depuis le bas de l'écran)

typedef struct Layout
{
    int logo_x, logo_y, subtitle_y;
    int panel_x, panel_y, panel_w, panel_h;
    int items_y; // ligne de la première option
    int line_h;  // hauteur d'une option (texte + espace)
    int record_y;
    int demo_rows; // rangées de cases de la démo
    int demo_top;  // ligne où commence la démo
} Layout;

static int demoRows(int term_height)
{
    return term_height >= 100 ? 3 : 2;
}

static void computeLayout(int w, int h, Layout *L)
{
    int rows = ui_text_rows();
    L->line_h = rows + 1;
    L->demo_rows = demoRows(h);
    L->demo_top = h - L->demo_rows * (CELL_HEIGHT + GAP / 2) + 1;

    L->panel_w = MAX(big_text_mode ? 100 : 46, ui_text_width(TITLE_HELP) + 8);
    L->panel_h = 2 + ITEM_COUNT * L->line_h - 1 + 2 + rows + 2; // marges, options, aide en bas

    // Le bloc (logo, sous-titre, menu, record) est centré dans l'espace au-dessus de la démo
    int block = LOGO_ROWS + 2 + 2 + L->panel_h + 1 + rows;
    int top = 2, bottom = L->demo_top - 3;
    L->logo_y = top + MAX(0, (bottom - top - block) / 2);
    L->logo_x = w / 2 - LOGO_WIDTH / 2;
    L->subtitle_y = L->logo_y + LOGO_ROWS + 1;
    L->panel_x = w / 2 - L->panel_w / 2;
    L->panel_y = L->subtitle_y + 3;
    L->items_y = L->panel_y + 2;
    L->record_y = L->panel_y + L->panel_h + 1;
}

int title_logo_y(int term_width, int term_height)
{
    Layout L;
    computeLayout(term_width, term_height, &L);
    return L.logo_y;
}

/*
## Démo ##
*/

typedef struct Demo
{
    Grid grid;
    EnemyPool pool;
    Labels labels;
    GameStats stats;
    float spawn_timer;
    float boss_timer;
} Demo;

static void demoInit(Demo *d, int term_width, int term_height)
{
    memset(d, 0, sizeof(*d));
    d->grid.width = (term_width - 2) / (CELL_WIDTH + GAP);
    d->grid.height = demoRows(term_height);
    allocateGridCells(&d->grid);

    // Chemin tout droit sur la rangée du milieu (ou la 2e rangée sur 2)
    int path_y = 1;
    for (int x = 0; x < d->grid.width; x++)
        d->grid.cells[x][path_y].type = CHEMIN;
    d->grid.start_x = 0;
    d->grid.start_y = path_y;
    d->grid.end_x = d->grid.width - 1;
    d->grid.end_y = path_y;
    numberPath(&d->grid);

    // Tourelles variées, avec des emplacements vides entre elles
    struct
    {
        int type; // -1 : emplacement vide
        int lvl;
    } top_row[] = {{-1, 0}, {Sniper, 1}, {Inferno, 1}, {-1, 0}, {Gatling, 0}, {Freezer, 1}, {-1, 0}, {Mortier, 1}, {Petrificateur, 0}, {Banque, 0}, {-1, 0}, {Gatling, 1}, {Sniper, 2}, {-1, 0}},
      bottom_row[] = {{Freezer, 0}, {-1, 0}, {Gatling, 1}, {Mortier, 0}, {-1, 0}, {Sniper, 2}, {Inferno, 0}, {-1, 0}, {Banque, 1}, {Petrificateur, 1}, {-1, 0}, {Inferno, 1}};
    int top_n = sizeof(top_row) / sizeof(top_row[0]), bottom_n = sizeof(bottom_row) / sizeof(bottom_row[0]);
    for (int x = 0; x < d->grid.width; x++)
    {
        int t = top_row[x % top_n].type;
        if (t >= 0)
        {
            d->grid.cells[x][0].turret = getTurretStruct((enum TurretType)t);
            d->grid.cells[x][0].turret.lvl = top_row[x % top_n].lvl;
            d->grid.cells[x][0].hasTurret = true;
        }
        if (d->grid.height > 2 && (t = bottom_row[x % bottom_n].type) >= 0)
        {
            d->grid.cells[x][2].turret = getTurretStruct((enum TurretType)t);
            d->grid.cells[x][2].turret.lvl = bottom_row[x % bottom_n].lvl;
            d->grid.cells[x][2].hasTurret = true;
        }
    }

    d->pool = AllocEnemyPool();
    d->labels.size = 255;
    d->labels.labels = malloc(sizeof(struct Label) * d->labels.size);
    if (d->labels.labels == NULL)
    {
        printCritical("Failed to allocate label");
        exit(EXIT_FAILURE);
    }
    d->stats.health = 1000000; // les ennemis qui passent ne comptent pas
    d->spawn_timer = 0.3f;
    d->boss_timer = 20;
    fx_clear();
}

static void demoFree(Demo *d)
{
    for (int x = 0; x < d->grid.width; x++)
        free(d->grid.cells[x]);
    free(d->grid.cells);
    freeEnemyPool(d->pool);
    free(d->labels.labels);
    fx_clear(); // la partie commence sans effets en cours
}

static void demoUpdate(Demo *d, float dt)
{
    // Ennemis tirés au hasard (poids : les plus courants reviennent plus souvent)
    static const enum EnemyType types[] = {ENEMY_TUX, ENEMY_TUX, ENEMY_TUX, ENEMY_SPEED, ENEMY_SPEED, ENEMY_SPIDER, ENEMY_SPIDER, ENEMY_HIGHTUX,
                                           ENEMY_HEALER, ENEMY_SPLITTER, ENEMY_SPLITTER, ENEMY_BIG_SPLITTER, ENEMY_HYPERSPEED};
    d->spawn_timer -= dt;
    if (d->spawn_timer <= 0)
    {
        struct Enemy *e = addEnemy(d->grid, &d->pool, types[rand() % (sizeof(types) / sizeof(types[0]))], d->grid.start_x, d->grid.start_y);
        e->hp = e->maxHP = e->maxHP * 4; // plus résistants que dans le jeu : ils traversent tout l'écran
        d->spawn_timer = 0.5f + (rand() % 100) / 100.0f;
    }
    d->boss_timer -= dt;
    if (d->boss_timer <= 0)
    {
        addEnemy(d->grid, &d->pool, ENEMY_SLIME_BOSS, d->grid.start_x, d->grid.start_y);
        d->boss_timer = 30;
    }

#if BULLETS_ON
    updateTowers(d->grid, d->pool, NULL, dt, &d->stats);
#else
    updateTowers(d->grid, d->pool, dt, &d->stats);
#endif
    updateLabels(&d->labels, dt);
    updateEnemies(&d->pool, d->grid, &d->stats, &d->labels, dt);
    fx_update(dt);
    d->stats.health = 1000000;
}

static void demoDraw(Demo *d, int term_width, int term_height)
{
    fillBG(1, grid_top - 1, MIN(term_width + 1, d->grid.width * (CELL_WIDTH + GAP) + 3), term_height + 1);
    drawFullGrid(d->grid);
    drawLabels(d->labels);
    drawEnemies(d->pool, d->grid);
    fx_draw();
}

/*
## Menu ##
*/

static void drawMenu(const Layout *L, int selected, int continue_wave, const char *record)
{
    drawPanel(L->panel_x, L->panel_y, L->panel_w, L->panel_h, "\033[38;5;29m");
    int left = L->panel_x + 3, right = L->panel_x + L->panel_w - 4;
    for (int i = 0; i < ITEM_COUNT; i++)
    {
        int y = L->items_y + i * L->line_h;
        bool disabled = i == ITEM_CONTINUE && continue_wave < 0;
        if (i == selected)
        {
            // Barre de sélection (en grande police, elle déborde d'une ligne au-dessus du texte)
            scr_printf("\033[48;5;22m");
            for (int r = big_text_mode ? -1 : 0; r < ui_text_rows(); r++)
            {
                move_to(left, y + r);
                for (int x = left; x <= right; x++)
                    scr_puts(" ");
            }
        }
        char text[96];
        if (i == selected)
            ui_text(left + 2, y, "\033[38;5;46m▶");
        const char *color = disabled ? "\033[38;5;239m" : i == selected ? "\033[38;5;231m" : COLOR_GRAY;
        snprintf(text, sizeof(text), "%s%s", color, ITEM_LABELS[i]);
        ui_text(left + 2 + ui_text_width("▶") + (big_text_mode ? 4 : 2), y, text);
        if (i == ITEM_CONTINUE && continue_wave >= 0)
        {
            snprintf(text, sizeof(text), "%sVague %d", i == selected ? "\033[38;5;120m" : "\033[38;5;65m", continue_wave);
            ui_text(right - 1 - ui_text_width(text), y, text);
        }
        scr_printf(RESET);
    }
    int help_y = L->panel_y + L->panel_h - 2 - ui_text_rows();
    ui_text_centered(L->panel_x + L->panel_w / 2, help_y, TITLE_HELP);
    scr_printf(RESET);
    if (record[0] != '\0')
        ui_text_centered(L->panel_x + L->panel_w / 2, L->record_y, record);
    scr_printf(RESET);
}

// Option sous la souris, -1 si aucune
static int itemAt(const Layout *L, int mx, int my)
{
    if (mx < L->panel_x || mx >= L->panel_x + L->panel_w || my < L->items_y - (big_text_mode ? 1 : 0))
        return -1;
    int i = (my - L->items_y + (big_text_mode ? 1 : 0)) / L->line_h;
    return i < ITEM_COUNT ? i : -1;
}

// Fenêtre des meilleurs scores, par-dessus l'écran titre
static void drawScoresWindow(int w, int h)
{
    int line_h = ui_text_rows() + 1;
    int pw = big_text_mode ? 130 : 60;
    Score scores[MAX_SCORES];
    int count = loadScores(scores);
    int rows = MIN(MAX(count, 1), (h - 8 - 2 * line_h - 6) / line_h); // la fenêtre s'adapte au nombre de scores
    int ph = 2 + ui_text_rows() + 2 + rows * line_h + 1 + ui_text_rows() + 2;
    int x0 = w / 2 - pw / 2, y0 = h / 2 - ph / 2;
    drawPanel(x0, y0, pw, ph, "\033[38;5;250m");
    ui_text_centered(w / 2, y0 + 2, COLOR_YELLOW "Meilleurs scores");
    scr_printf(RESET);
    drawScoresList(x0 + 4, x0 + pw - 5, y0 + 2 + ui_text_rows() + 2, rows, -1);
    ui_text_centered(w / 2, y0 + ph - 2 - ui_text_rows(), COLOR_GRAY "Échap ou Entrée : retour");
    scr_printf(RESET);
}

static int nextItem(int item, int step, int continue_wave)
{
    do
        item = (item + step + ITEM_COUNT) % ITEM_COUNT;
    while (item == ITEM_CONTINUE && continue_wave < 0);
    return item;
}

static double now_seconds(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

enum TitleChoice title_screen(int term_width, int term_height)
{
    flush_input(); // touches tapées pendant le lancement
    run_intro(term_width, term_height); // voir intro.c

    Layout L;
    computeLayout(term_width, term_height, &L);
    int saved_grid_top = grid_top;

    int continue_wave = saveWave(0);
    int selected = continue_wave >= 0 ? ITEM_CONTINUE : ITEM_NEW_GAME;
    bool scores_open = false;

    // Record affiché sous le menu
    char record[64] = "";
    Score best[MAX_SCORES];
    if (loadScores(best) > 0)
        snprintf(record, sizeof(record), COLOR_YELLOW "Record : vague %d", best[0].wave);

    Demo demo;
    demoInit(&demo, term_width, term_height);

    double start = now_seconds(), previous = start;
    enum TitleChoice choice = TITLE_NEW_GAME;
    bool done = false;
    while (!done)
    {
        double now = now_seconds();
        float dt = (float)(now - previous);
        previous = now;
        if (dt > 0.1f)
            dt = 0.1f;
        anim_clock += dt;

        // Tous les événements en attente (la souris en envoie beaucoup quand elle bouge)
        int activate = -1; // option à lancer
        for (int n = 0; n < 64; n++)
        {
            InputEvent ev = poll_input();
            if (ev.type == INPUT_NONE)
                break;
            if (scores_open)
            {
                if (ev.type == INPUT_ESCAPE || (ev.type == INPUT_KEY && (ev.key == 10 || ev.key == ' ')) || is_mouse_click(ev))
                    scores_open = false;
                else if (ev.type == INPUT_KEY && (ev.key == 'q' || ev.key == 'Q'))
                    activate = ITEM_QUIT;
                continue;
            }
            if (ev.type == INPUT_ARROW && (ev.key == 'A' || ev.key == 'B'))
                selected = nextItem(selected, ev.key == 'A' ? -1 : 1, continue_wave);
            else if (ev.type == INPUT_MOUSE && ev.pressed && (ev.key == MOUSE_WHEEL_UP || ev.key == MOUSE_WHEEL_DOWN))
                selected = nextItem(selected, ev.key == MOUSE_WHEEL_UP ? -1 : 1, continue_wave);
            else if (ev.type == INPUT_MOUSE && ev.pressed && (ev.key & MOUSE_MOTION_FLAG))
            {
                int item = itemAt(&L, ev.x, ev.y); // survol : l'option est sélectionnée
                if (item >= 0 && !(item == ITEM_CONTINUE && continue_wave < 0))
                    selected = item;
            }
            else if (is_mouse_click(ev) && ev.key == MOUSE_LEFT)
            {
                int item = itemAt(&L, ev.x, ev.y); // clic : l'option est lancée
                if (item >= 0 && !(item == ITEM_CONTINUE && continue_wave < 0))
                    selected = activate = item;
            }
            else if (ev.type == INPUT_KEY && (ev.key == 10 || ev.key == ' '))
                activate = selected;
            else if (ev.type == INPUT_KEY && (ev.key == 'q' || ev.key == 'Q'))
                activate = ITEM_QUIT;
            if (activate >= 0)
                break;
        }

        switch (activate)
        {
        case ITEM_CONTINUE:
            choice = TITLE_CONTINUE;
            done = true;
            break;
        case ITEM_NEW_GAME:
            done = true;
            break;
        case ITEM_TUTORIAL:
        {
            grid_top = saved_grid_top;
            enum TutorialResult result = run_tutorial(term_width, term_height); // voir tutorial.c
            if (result == TUTORIAL_QUIT)
                exit(EXIT_SUCCESS); // free de tout automatique dans main.c/void cleanup()
            if (result == TUTORIAL_DONE)
                done = true; // tutoriel terminé : on lance une nouvelle partie
            previous = now_seconds(); // le temps passé dans le tutoriel ne compte pas pour la démo
            break;
        }
        case ITEM_SCORES:
            scores_open = true;
            break;
        case ITEM_QUIT:
            exit(EXIT_SUCCESS);
        default:
            break;
        }
        if (done)
            break;

        // Démo (en bas de l'écran, elle monte depuis le bas pendant DEMO_SLIDE_TIME)
        double t = now - start;
        double p = t < DEMO_SLIDE_TIME ? t / DEMO_SLIDE_TIME : 1;
        p = 1 - (1 - p) * (1 - p) * (1 - p);
        grid_top = L.demo_top + (int)((1 - p) * (term_height - L.demo_top + 2));
        float step = dt;
        while (step > 0)
        {
            float s = step > 1.0f / 60 ? 1.0f / 60 : step;
            demoUpdate(&demo, s);
            step -= s;
        }

        scr_printf(RESET);
        clear_screen();
        demoDraw(&demo, term_width, term_height);
        grid_top = saved_grid_top;

        draw_logo(L.logo_x, L.logo_y, 42);
        draw_subtitle(term_width, term_height);
        drawMenu(&L, selected, continue_wave, record);
        if (scores_open)
            drawScoresWindow(term_width, term_height);
        scr_flush();

        // Cadence fixe : on dort jusqu'à l'image suivante
        double frame_end = now + 1.0 / TITLE_FPS;
        double remaining = frame_end - now_seconds();
        if (remaining > 0)
            nsleep_seconds(remaining);
    }

    grid_top = saved_grid_top;
    demoFree(&demo);
    scr_printf(RESET);
    clear_screen();
    return choice;
}
