#include "tutorial.h"
#include "shared.h"
#include "bigtext.h"

#include "tower.h"
#include "enemies.h"
#include "pixelart.h"

#include <string.h>

/*
##########
TUTORIEL
##########

Le contenu (textes et sprites) est décrit sous forme de chapitres et de blocs.
La mise en page est automatique : les textes sont coupés selon la largeur du terminal
et la taille du texte (grande police ou non), puis répartis sur autant de pages que nécessaire.

Pour ajouter du contenu, il suffit d'ajouter un bloc dans un des tableaux ci-dessous.
*/

/*
##############
## Sprites ##
##############
*/

static const char *spriteSpeed[5][7] = {{SPEED_CONTROL COLOR_TOWER_SLOT_BG "         ▄█   ",
                                                "       ▄███   ",
                                                "     ▄█████   ",
                                                "    ███████   ",
                                                "     ▀█████   ",
                                                "       ▀███   ",
                                                "         ▀█   "},
                                               {SPEED_CONTROL_2 COLOR_TOWER_SLOT_BG "   █▄         ",
                                                "   ███▄       ",
                                                "   █████▄     ",
                                                "   ███████    ",
                                                "   █████▀     ",
                                                "   ███▀       ",
                                                "   █▀         "},
                                               {SPEED_CONTROL_3 COLOR_TOWER_SLOT_BG "  █▄  █▄      ",
                                                "  ███▄▀██▄    ",
                                                "  █████▄▀██▄  ",
                                                "  ███████ ███ ",
                                                "  █████▀▄██▀  ",
                                                "  ███▀▄██▀    ",
                                                "  █▀  █▀      "},
                                               {SPEED_CONTROL_4 COLOR_TOWER_SLOT_BG "  ▄▄▄▀▀█▄     ",
                                                "▄▄▄▀██▄▀██▄   ",
                                                " ▀██▄▀██▄▀██▄ ",
                                                "   ███ ███ ███",
                                                " ▄██▀▄██▀▄██▀ ",
                                                "▀▀▀▄██▀▄██▀   ",
                                                "  ▀▀▀▄▄█▀     "},
                                               {SPEED_CONTROL_5 COLOR_TOWER_SLOT_BG "    ▄▄ ▀▀▄▄   ",
                                                " ▀▀▄▄ ▀█▄ ▀█▄ ",
                                                "▀█▄ ▀█▄ ▀█▄ █▄",
                                                "  ██  ██  ██ █",
                                                "▄█▀ ▄█▀ ▄█▀ █▀",
                                                " ▄▄▀▀ ▄█▀ ▄█▀ ",
                                                "    ▀▀ ▄▄▀▀   "}};

/*
###############
## Contenu ##
###############
*/

enum BlockType
{
    BLOCK_TEXT,   // sprite optionnel à gauche + titre optionnel + paragraphe
    BLOCK_SPEEDS, // les 5 icônes de vitesse
};

typedef struct TutoBlock
{
    enum BlockType type;
    const char **sprite; // inutilisé (les tourelles sont dessinées en pixel art à partir de `turret`)
    const char *heading_color;
    const char *heading;
    const char *text;
    int turret; // >= 0 : ajoute automatiquement les statistiques de cette tourelle (enum TurretType)
    int enemy;  // >= 0 : ajoute automatiquement les statistiques de cet ennemi (enum EnemyType)
} TutoBlock;

typedef struct TutoChapter
{
    const char *title;
    const TutoBlock *blocks;
    int block_count;
} TutoChapter;

#define SPRITE_ROWS 7
#define SPRITE_WIDTH 14
#define NONE -1

static const TutoBlock chapter_welcome[] = {
    {BLOCK_TEXT, NULL, NULL, NULL,
     "Au fond d'une salle serveur oubliée, un vieux terminal tourne encore. Des processus corrompus "
     "remontent le bus de données pour l'effacer : arrête-les avant qu'ils n'atteignent le bout du chemin.",
     NONE, NONE},
    {BLOCK_TEXT, NULL, COLOR_YELLOW, "Règles",
     "Chaque ennemi qui atteint la sortie te fait perdre des points de vie (❤). À 0, la partie est finie.\n"
     "Chaque ennemi détruit rapporte de l'argent (€), et chaque vague terminée rapporte 10 € de bonus.\n"
     "Les vagues sont de plus en plus fortes : des boss apparaissent aux vagues 10, 20 et 30.",
     NONE, NONE},
    {BLOCK_TEXT, NULL, COLOR_YELLOW, "Commandes",
     "Flèches : déplacer la sélection\n"
     "Espace : boutique (le jeu se met en pause)\n"
     "Entrée : acheter, améliorer ou vendre\n"
     "O / L : accélérer / ralentir (jusqu'à x16)\n"
     "S : sauvegarder ou charger une partie\n"
     "Échap : pause (tutoriel, sauvegardes, commandes)\n"
     "Q : quitter (la partie est sauvée dans l'emplacement 0)",
     NONE, NONE},
};

static const TutoBlock chapter_turrets[] = {
    {BLOCK_TEXT, NULL, COLOR_SNIPER_BASE, "Sniper",
     "Tir précis sur une seule cible. Au niveau 3, sa portée couvre toute la carte.", Sniper, NONE},
    {BLOCK_TEXT, NULL, COLOR_INFERNO_BASE, "Inferno",
     "Enflamme plusieurs ennemis autour de lui. Les flammes continuent de brûler après le tir.", Inferno, NONE},
    {BLOCK_TEXT, NULL, "\033[38;5;250m", "Mortier",
     "Obus explosifs qui touchent toute une zone, mais il ne peut pas viser les ennemis trop proches.", Mortier, NONE},
    {BLOCK_TEXT, NULL, "\033[38;5;245m", "Gatling",
     "Énorme cadence de tir, petits dégâts, courte portée. Idéale dans un virage.", Gatling, NONE},
    {BLOCK_TEXT, NULL, COLOR_FREEZER_BASE, "Freezer",
     "Ralentit tous les ennemis à portée.", Freezer, NONE},
    {BLOCK_TEXT, NULL, COLOR_PETRIFICATEUR_FIRING, "Pétrificateur",
     "Fige un ennemi sur place. Sans effet sur le Tank.", Petrificateur, NONE},
    {BLOCK_TEXT, NULL, COLOR_BANQUE_MONEY, "Banque",
     "Ne tire pas : elle rapporte de l'argent à intervalle régulier. À placer tôt !", Banque, NONE},
};

static const TutoBlock chapter_enemies[] = {
    {BLOCK_TEXT, NULL, "\033[38;5;34m", "Tux", "Le processus de base, lent et fragile. Présent dès la première vague.", NONE, ENEMY_TUX},
    {BLOCK_TEXT, NULL, COLOR_SPEED_BASE, "Speed", "Rapide mais très fragile. À partir de la vague 1.", NONE, ENEMY_SPEED},
    {BLOCK_TEXT, NULL, COLOR_HIGHTUX_BASE, "Tux couronné", "Un Tux plus résistant et plus rapide. À partir de la vague 2.", NONE, ENEMY_HIGHTUX},
    {BLOCK_TEXT, NULL, COLOR_SPIDER_EYES, "Araignée", "Solide, et coûte cher en vie si elle passe. À partir de la vague 2.", NONE, ENEMY_SPIDER},
    {BLOCK_TEXT, NULL, COLOR_HYPERSPEED_EYES, "Hyperspeed", "Le plus rapide de tous : il faut de la portée. À partir de la vague 2.", NONE, ENEMY_HYPERSPEED},
    {BLOCK_TEXT, NULL, "\033[38;5;27m", "Slime (boss)", "Lent mais costaud. Arrive à la vague 10.", NONE, ENEMY_SLIME_BOSS},
    {BLOCK_TEXT, NULL, COLOR_BOSS_STUN_LIGHTNING, "Boss électrique",
     "Désactive les tourelles proches de lui (rayon de 2,5 cases) pendant 2,5 s. Arrive à la vague 20.", NONE, ENEMY_BOSS_STUN},
    {BLOCK_TEXT, NULL, COLOR_TANK_BASE, "Tank (boss)",
     "Une montagne de PV, insensible au gel et à la pétrification. Arrive à la vague 30.", NONE, ENEMY_TANK},
};

static const TutoBlock chapter_tips[] = {
    {BLOCK_SPEEDS, NULL, NULL, NULL, NULL, NONE, NONE},
    {BLOCK_TEXT, NULL, COLOR_YELLOW, "Astuces",
     "Une tourelle sélectionnée affiche sa portée (cercle en pointillés).\n"
     "Vendre une tourelle rend 80 % de son prix.\n"
     "Le Mortier a une portée minimale : place-le loin du chemin.\n"
     "Contre le Boss électrique, garde des tourelles à longue portée.\n"
     "La Banque est rentable si elle est posée tôt dans la partie.",
     NONE, NONE},
    {BLOCK_TEXT, NULL, NULL, NULL, "Bonne chance !", NONE, NONE},
};

static const TutoChapter CHAPTERS[] = {
    {"Bienvenue dans ATARDES", chapter_welcome, sizeof(chapter_welcome) / sizeof(TutoBlock)},
    {"Les tourelles", chapter_turrets, sizeof(chapter_turrets) / sizeof(TutoBlock)},
    {"Les ennemis", chapter_enemies, sizeof(chapter_enemies) / sizeof(TutoBlock)},
    {"Astuces", chapter_tips, sizeof(chapter_tips) / sizeof(TutoBlock)},
};
#define CHAPTER_COUNT (int)(sizeof(CHAPTERS) / sizeof(TutoChapter))

/*
###########################
## Statistiques automatiques ##
###########################
Lues directement dans getTurretStruct / getTurretPrice / defEnemy :
si l'équilibrage du jeu change, le tutoriel reste juste.
*/

// Écrit un nombre sans zéros inutiles (2.50 -> "2.5", 1.00 -> "1")
static void fmt_number(char *out, size_t size, float v)
{
    snprintf(out, size, "%.2f", v);
    char *end = out + strlen(out) - 1;
    while (*end == '0')
        *end-- = '\0';
    if (*end == '.')
        *end = '\0';
}

// Remplace l'espace avant une unité (s, €, %, ❤, case/s) par une espace insécable,
// pour que "1 s" ne soit jamais coupé en fin de ligne
static void keep_units_together(char *text, size_t size)
{
    const char *units[] = {" s", " €", " %", " ❤", " PV", " case/s", " dégât"};
    char result[1024];
    size_t r = 0;
    for (size_t i = 0; text[i] != '\0' && r < sizeof(result) - 3; i++)
    {
        bool replaced = false;
        if (text[i] == ' ' && i > 0 && text[i - 1] >= '0' && text[i - 1] <= '9')
        {
            for (size_t u = 0; u < sizeof(units) / sizeof(units[0]); u++)
            {
                size_t len = strlen(units[u]);
                char after = text[i + len];
                if (strncmp(text + i, units[u], len) == 0 && (after == '\0' || after == ' ' || after == ',' || after == '\n' || after == 's'))
                {
                    result[r++] = (char)0xC2; // U+00A0 en UTF-8
                    result[r++] = (char)0xA0;
                    replaced = true;
                    break;
                }
            }
        }
        if (!replaced)
            result[r++] = text[i];
    }
    result[r] = '\0';
    snprintf(text, size, "%s", result);
}

static void turret_stats(enum TurretType type, char *out, size_t size)
{
    struct Turret t = getTurretStruct(type);
    out[0] = '\0';
    for (int lvl = 0; lvl < 3; lvl++)
    {
        int price = getTurretPrice(type, lvl);
        if (price <= 0)
            break;
        char range[16], value[16], delay[16], line[160];
        fmt_number(range, sizeof(range), t.range_max[lvl]);
        fmt_number(delay, sizeof(delay), t.reload_delay[lvl]);
        int n = snprintf(line, sizeof(line), "Niv. %d (%d €) : ", lvl + 1, price);
        switch (type)
        {
        case Sniper:
        case Gatling:
            fmt_number(value, sizeof(value), t.damage[lvl]);
            snprintf(line + n, sizeof(line) - n, "portée %s, %s dégât%s par tir, un tir toutes les %s s", t.range_max[lvl] >= 50 ? "infinie" : range, value, t.damage[lvl] > 1 ? "s" : "", delay);
            break;
        case Mortier:
        {
            char min[16], zone[16];
            fmt_number(value, sizeof(value), t.damage[lvl]);
            fmt_number(min, sizeof(min), t.range_min[lvl]);
            fmt_number(zone, sizeof(zone), t.splash[lvl]);
            snprintf(line + n, sizeof(line) - n, "portée %s à %s, %s dégâts, zone %s, toutes les %s s", min, range, value, zone, delay);
            break;
        }
        case Inferno:
            snprintf(line + n, sizeof(line) - n, "portée %s, brûle %d ennemis pendant %d s", range, t.nb_ennemi[lvl], lvl + 1);
            break;
        case Freezer:
            snprintf(line + n, sizeof(line) - n, "portée %s, ralentit de %d %% pendant %d s", range, (int)((1 - t.puissance_effet[lvl]) * 100 + 0.5), lvl + 1);
            break;
        case Petrificateur:
            fmt_number(value, sizeof(value), t.puissance_effet[lvl]);
            snprintf(line + n, sizeof(line) - n, "portée %s, fige pendant %s s, toutes les %s s", range, value, delay);
            break;
        case Banque:
            fmt_number(value, sizeof(value), t.puissance_effet[lvl]);
            snprintf(line + n, sizeof(line) - n, "+%s € toutes les %s s", value, delay);
            break;
        }
        strncat(out, "\n", size - strlen(out) - 1);
        strncat(out, line, size - strlen(out) - 1);
    }
}

static void enemy_stats(enum EnemyType type, char *out, size_t size)
{
    // defEnemy a besoin d'une grille pour la position de départ : une grille d'une case suffit
    struct Cell cell = {0};
    struct Cell *column = &cell;
    Grid grid = {0};
    grid.width = grid.height = 1;
    grid.cells = &column;
    struct Enemy e = defEnemy(grid, type, 0, 0);
    char speed[16];
    fmt_number(speed, sizeof(speed), e.speed);
    snprintf(out, size, "\n%d PV, vitesse %s case/s, retire %d ❤, rapporte %d €", (int)e.maxHP, speed, e.damage, e.money);
}

/*
#####################
## Mise en page ##
#####################
Chaque page est une liste d'éléments à dessiner avec leur position.
*/

enum ItemType
{
    ITEM_TITLE,   // titre de chapitre (texte centré)
    ITEM_SPRITE,  // sprite de tourelle
    ITEM_HEADING, // titre de bloc en couleur
    ITEM_LINE,    // une ligne de paragraphe
    ITEM_SPEEDS,  // icônes de vitesse
};

typedef struct PageItem
{
    enum ItemType type;
    int x, y;
    const char **sprite;
    const char *color;
    const char *text;
    int len;
} PageItem;

#define MAX_PAGES 40
#define MAX_ITEMS 120
#define MAX_LINES 200

static PageItem pages[MAX_PAGES][MAX_ITEMS];
static int item_counts[MAX_PAGES];
static int page_count;

// Zone de la fenêtre du tutoriel
static int box_x, box_y, box_w, box_h;
static int content_left, content_width, content_bottom;

static void add_item(int page, PageItem item)
{
    if (page < MAX_PAGES && item_counts[page] < MAX_ITEMS)
        pages[page][item_counts[page]++] = item;
}

// Texte complet de chaque bloc (description + statistiques générées), gardé en mémoire pour l'affichage
#define MAX_BLOCK_TEXT 1024
static char block_texts[CHAPTER_COUNT][16][MAX_BLOCK_TEXT];

static const char *block_text(int c, int b)
{
    const TutoBlock *block = &CHAPTERS[c].blocks[b];
    char *out = block_texts[c][b];
    snprintf(out, MAX_BLOCK_TEXT, "%s", block->text ? block->text : "");
    char stats[600] = "";
    if (block->turret >= 0)
        turret_stats(block->turret, stats, sizeof(stats));
    else if (block->enemy >= 0)
        enemy_stats(block->enemy, stats, sizeof(stats));
    strncat(out, stats, MAX_BLOCK_TEXT - strlen(out) - 1);
    keep_units_together(out, MAX_BLOCK_TEXT);
    return out;
}

static void layout(int term_width, int term_height)
{
    box_x = 3;
    box_y = 2;
    box_w = term_width - 5;
    box_h = term_height - 3;
    content_left = box_x + 5;
    content_width = box_w - 10;
    int footer_rows = ui_text_rows() + 2;
    content_bottom = box_y + box_h - 2 - footer_rows; // dernière ligne utilisable
    int page_top = box_y + 2;

    // En grande police, la dernière ligne de chaque lettre est à moitié vide :
    // pas besoin d'interligne supplémentaire, ça permet de mettre plus de texte par page
    int line_h = ui_text_rows();
    int gap = big_text_mode ? 3 : 1;
    int title_h = ui_text_rows() + gap;

    memset(item_counts, 0, sizeof(item_counts));
    page_count = 0;

    for (int c = 0; c < CHAPTER_COUNT; c++)
    {
        // Chaque chapitre commence sur une nouvelle page, avec son titre
        int page = page_count++;
        if (page >= MAX_PAGES)
            return;
        int y = page_top;
        add_item(page, (PageItem){ITEM_TITLE, term_width / 2, y, NULL, COLOR_YELLOW, CHAPTERS[c].title, 0});
        y += title_h;

        for (int b = 0; b < CHAPTERS[c].block_count && b < 16; b++)
        {
            const TutoBlock *block = &CHAPTERS[c].blocks[b];

            if (block->type == BLOCK_SPEEDS)
            {
                int h = ui_text_rows() + 1 + SPRITE_ROWS;
                if (y + h > content_bottom)
                {
                    page = page_count++;
                    y = page_top;
                }
                add_item(page, (PageItem){ITEM_SPEEDS, term_width / 2 - 49, y, NULL, NULL, NULL, 0});
                y += h + gap;
                continue;
            }

            // Bloc texte : sprite à gauche (si présent), titre puis paragraphe à droite
            const char *text = block_text(c, b);
            bool has_preview = block->turret >= 0; // aperçu de la tourelle à gauche du texte
            int text_x = content_left + (has_preview ? SPRITE_WIDTH + 4 : 0);
            int text_w = content_width - (text_x - content_left);
            int starts[MAX_LINES], lens[MAX_LINES];
            int n = ui_text_split(text_w, text, starts, lens, MAX_LINES);
            int heading_h = block->heading ? line_h + 1 : 0;

            // Si le bloc entier ne tient pas dans la fin de la page mais tient sur une page vide,
            // on le met sur la page suivante (évite les lignes isolées). Sinon on le coupe.
            int block_h = heading_h + n * line_h;
            if (has_preview && block_h < SPRITE_ROWS)
                block_h = SPRITE_ROWS;
            bool page_has_content = y > page_top + title_h || (y > page_top && item_counts[page] > 1);
            if (y + block_h > content_bottom && block_h <= content_bottom - page_top && page_has_content)
            {
                page = page_count++;
                y = page_top;
            }

            int block_top = y;
            if (has_preview)
                add_item(page, (PageItem){ITEM_SPRITE, content_left, y, NULL, NULL, NULL, block->turret});
            if (block->heading)
            {
                add_item(page, (PageItem){ITEM_HEADING, text_x, y, NULL, block->heading_color, block->heading, 0});
                y += heading_h;
            }
            for (int i = 0; i < n; i++)
            {
                if (y + ui_text_rows() > content_bottom)
                {
                    // Le paragraphe continue sur la page suivante
                    page = page_count++;
                    y = page_top;
                    block_top = y;
                }
                if (lens[i] > 0)
                {
                    // Les statistiques (tout ce qui suit le premier retour à la ligne de la description) sont en gris
                    const char *stats_start = strchr(text, '\n');
                    bool stat = (block->turret >= 0 || block->enemy >= 0) && stats_start != NULL && text + starts[i] > stats_start;
                    add_item(page, (PageItem){ITEM_LINE, text_x, y, NULL, stat ? "\033[38;5;246m" : NULL, text + starts[i], lens[i]});
                }
                y += line_h;
            }
            if (has_preview && y < block_top + SPRITE_ROWS)
                y = block_top + SPRITE_ROWS;
            y += gap;
        }
    }
    if (page_count > MAX_PAGES)
        page_count = MAX_PAGES;
}

/*
###############
## Affichage ##
###############
*/

static void draw_frame(void)
{
    // Efface l'écran puis dessine le cadre
    scr_printf(RESET);
    clear_screen();
    for (int y = 0; y < box_h; y++)
    {
        move_to(box_x, box_y + y);
        for (int x = 0; x < box_w; x++)
        {
            if (x == 0 || x == box_w - 1)
                scr_printf("█");
            else if (y == 0)
                scr_printf("▀");
            else if (y == box_h - 1)
                scr_printf("▄");
            else
                scr_printf(" ");
        }
    }
}

static void draw_sprite(int x, int y, const char **sprite, int rows)
{
    for (int i = 0; i < rows; i++)
    {
        move_to(x, y + i);
        scr_puts(sprite[i]);
    }
    scr_printf(RESET);
}

static void draw_page(int page, int term_width)
{
    draw_frame();

    if (page < 0)
    {
        // Page secrète (en revenant en arrière depuis la première page)
        ui_text_centered(term_width / 2, box_y + box_h / 2, "Yooo Easter Egg");
    }
    else
    {
        for (int i = 0; i < item_counts[page]; i++)
        {
            PageItem *it = &pages[page][i];
            switch (it->type)
            {
            case ITEM_TITLE:
            {
                char title[128];
                snprintf(title, sizeof(title), "%s%s", it->color, it->text);
                scr_printf(RESET);
                ui_text_centered(it->x, it->y, title);
                scr_printf(RESET);
                break;
            }
            case ITEM_SPRITE:
                draw_turret_preview(it->x, it->y, it->len, 0, TRANSPARENT); // rendu pixel art (voir pixelart.c)
                break;
            case ITEM_HEADING:
            {
                char heading[128];
                snprintf(heading, sizeof(heading), "%s%s", it->color ? it->color : "", it->text);
                scr_printf(RESET);
                ui_text(it->x, it->y, heading);
                scr_printf(RESET);
            }
                break;
            case ITEM_LINE:
                scr_printf(RESET);
                if (it->color)
                    scr_printf("%s", it->color);
                ui_text_span(it->x, it->y, it->text, it->len);
                scr_printf(RESET);
                break;
            case ITEM_SPEEDS:
                for (int s = 0; s < 5; s++)
                {
                    char label[8];
                    snprintf(label, sizeof(label), "x%d", 1 << s);
                    scr_printf(RESET);
                    ui_text_centered(it->x + s * 20 + 7, it->y, label);
                    draw_sprite(it->x + s * 20, it->y + ui_text_rows() + 1, spriteSpeed[s], SPRITE_ROWS);
                }
                break;
            }
        }
    }

    // Bas de la fenêtre : aide et numéro de page
    int footer_y = box_y + box_h - 2 - ui_text_rows();
    scr_printf(RESET);
    ui_text_fit(content_left, footer_y, content_width - 30, COLOR_GRAY "Entrée : suite   Retour : précédent   Échap : fermer");
    char number[32];
    snprintf(number, sizeof(number), COLOR_GRAY "%d/%d", page < 0 ? 0 : page + 1, page_count);
    ui_text(content_left + content_width - ui_text_width(number), footer_y, number);
    scr_printf(RESET);
    scr_flush();
}

enum TutorialResult run_tutorial(int term_width, int term_height)
{
    enum TutorialResult result = TUTORIAL_DONE;
    layout(term_width, term_height);
    int page = 0;
    draw_page(page, term_width);

    while (page < page_count)
    {
        int c = get_key_press();
        int previous = page;
        if (c == 10 || c == ' ')
            page++;
        else if (c == 8 || c == 127) // Retour arrière
            page = page > -1 ? page - 1 : -1;
        else if (c == 27) // flèches : \\033 [ C (droite) / D (gauche), ou Échap seul
        {
            msleep(5);
            if (get_key_press() == '[')
            {
                int arrow = get_key_press();
                if (arrow == 'C')
                    page++;
                else if (arrow == 'D')
                    page = page > -1 ? page - 1 : -1;
            }
            else
            {
                result = TUTORIAL_BACK;
                break;
            }
        }
        else if (c == 'q' || c == 'Q')
        {
            result = TUTORIAL_QUIT;
            break;
        }

        if (page != previous && page < page_count)
            draw_page(page, term_width);
        msleep(15); // inutile de vérifier le clavier plus souvent
    }
    scr_printf(RESET);
    clear_screen();
    return result;
}
