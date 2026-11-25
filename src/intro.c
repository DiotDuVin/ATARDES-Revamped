#include "intro.h"
#include "shared.h"
#include "title.h"

#include <string.h>

/*
#################
ANIMATION D'INTRO
#################

Environ 3 secondes, à 60 images par seconde, avec un temps mesuré (et non un nombre d'images) :
la vitesse est la même quel que soit l'ordinateur ou le terminal.

  1. allumage d'un écran cathodique : une ligne lumineuse s'étire puis s'ouvre en hauteur
  2. le logo se matérialise : une "tête de balayage" passe de gauche à droite,
     précédée de bruit (░▒▓) et suivie du logo définitif
  3. le logo s'illumine puis prend sa couleur finale
  4. le sous-titre s'écrit lettre par lettre

Une touche ou un clic passe l'animation (les simples mouvements de souris sont ignorés).
*/

const char *ATARDES_LOGO[LOGO_ROWS] = {
    "   ▄████████     ███        ▄████████    ▄████████ ████████▄     ▄████████    ▄████████ ",
    "   ███    ███ ▀█████████▄   ███    ███   ███    ███ ███   ▀███   ███    ███   ███    ███",
    "   ███    ███    ▀███▀▀██   ███    ███   ███    ███ ███    ███   ███    █▀    ███    █▀ ",
    "   ███    ███     ███   ▀   ███    ███  ▄███▄▄▄▄██▀ ███    ███  ▄███▄▄▄       ███       ",
    " ▀███████████     ███     ▀███████████ ▀▀███▀▀▀▀▀   ███    ███ ▀▀███▀▀▀     ▀███████████",
    "   ███    ███     ███       ███    ███ ▀███████████ ███    ███   ███    █▄           ███",
    "   ███    ███     ███       ███    ███   ███    ███ ███   ▄███   ███    ███    ▄█    ███",
    "   ███    █▀     ▄████▀     ███    █▀    ███    ███ ████████▀    ██████████  ▄████████▀ ",
    "                                         ███    ███                                     ",
};

const char *ATARDES_SUBTITLE = "Antique Terminal : Absolute Routing Defense Epic Simulator";

// Durées des étapes (en secondes)
#define T_LINE 0.30  // la ligne s'étire
#define T_OPEN 0.55  // l'écran s'ouvre en hauteur
#define T_LOGO 1.75  // fin du balayage du logo
#define T_GLOW 2.10  // fin de l'illumination
#define T_SUB 2.90   // fin de l'écriture du sous-titre
#define T_END 3.20

#define LOGO_COLOR 42 // vert (palette 256 couleurs)

void logo_position(int term_width, int term_height, int *x, int *y)
{
    *x = term_width / 2 - LOGO_WIDTH / 2;
    *y = title_logo_y(term_width, term_height); // même place que dans l'écran titre
}

// Découpe une ligne UTF-8 du logo en caractères (un par colonne)
static int split_glyphs(const char *line, const char *glyphs[], int lens[], int max)
{
    int n = 0;
    while (*line != '\0' && n < max)
    {
        int len = ((unsigned char)*line < 0x80) ? 1 : ((unsigned char)*line < 0xE0) ? 2 : ((unsigned char)*line < 0xF0) ? 3 : 4;
        glyphs[n] = line;
        lens[n] = len;
        n++;
        line += len;
    }
    return n;
}

static double now_seconds(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

static double clamp01(double v)
{
    return v < 0 ? 0 : v > 1 ? 1 : v;
}

// Accélération douce au début et à la fin
static double ease(double v)
{
    v = clamp01(v);
    return v * v * (3 - 2 * v);
}

static void put_glyph(int x, int y, const char *glyph, int len, int color)
{
    char buffer[8];
    memcpy(buffer, glyph, len);
    buffer[len] = '\0';
    move_to(x, y);
    scr_printf("\033[38;5;%dm%s", color, buffer);
}

void draw_logo(int x0, int y0, int color)
{
    for (int r = 0; r < LOGO_ROWS; r++)
    {
        move_to(x0, y0 + r);
        scr_printf("\033[38;5;%dm%s" RESET, color, ATARDES_LOGO[r]);
    }
}

static void draw_frame(double t, int w, int h)
{
    scr_printf(RESET);
    clear_screen();
    int mid = h / 2;

    // 1. Allumage : une ligne blanche qui s'étire depuis le centre
    if (t < T_LINE)
    {
        int half = (int)(ease(t / T_LINE) * w / 2);
        move_to(w / 2 - half, mid);
        scr_printf("\033[38;5;255m");
        for (int x = 0; x < half * 2; x++)
            scr_puts("▀");
        scr_printf(RESET);
        return;
    }

    // 2. L'écran s'ouvre en hauteur, rempli d'une lueur verte qui s'éteint
    if (t < T_OPEN)
    {
        double p = (t - T_LINE) / (T_OPEN - T_LINE);
        int half_h = (int)(ease(p) * h / 2);
        int greens[] = {157, 120, 77, 71, 65, 29, 23, 22};
        int color = greens[(int)(p * 7.99)];
        scr_printf("\033[38;5;%dm", color);
        for (int y = mid - half_h; y <= mid + half_h; y++)
        {
            if (y < 1 || y > h)
                continue;
            move_to(1, y);
            // Bruit pseudo-aléatoire qui ne change que toutes les 50 ms (moins de données à envoyer au terminal)
            unsigned int frame = (unsigned int)(t * 20);
            for (int x = 0; x < w; x++)
            {
                unsigned int hash = (unsigned int)x * 73856093u ^ (unsigned int)y * 19349663u ^ frame * 83492791u;
                scr_puts((int)(hash % 100) < 60 * (1 - p) + 5 ? "░" : " ");
            }
        }
        scr_printf(RESET);
        return;
    }

    int lx, ly;
    logo_position(w, h, &lx, &ly);

    // 3. Balayage du logo
    if (t < T_LOGO)
    {
        double p = ease((t - T_OPEN) / (T_LOGO - T_OPEN));
        int head = (int)(p * (LOGO_WIDTH + 16)) - 8;
        for (int r = 0; r < LOGO_ROWS; r++)
        {
            const char *glyphs[LOGO_WIDTH + 8];
            int lens[LOGO_WIDTH + 8];
            int n = split_glyphs(ATARDES_LOGO[r], glyphs, lens, LOGO_WIDTH + 8);
            for (int c = 0; c < n; c++)
            {
                bool filled = !(lens[c] == 1 && glyphs[c][0] == ' ');
                if (c < head - 5)
                {
                    if (filled)
                        put_glyph(lx + c, ly + r, glyphs[c], lens[c], LOGO_COLOR);
                }
                else if (c < head)
                {
                    if (filled)
                        put_glyph(lx + c, ly + r, glyphs[c], lens[c], 231); // tête de balayage : blanc
                }
                else if (c < head + 10 && rand() % 100 < 35)
                {
                    const char *noise[] = {"░", "▒", "▓"};
                    put_glyph(lx + c, ly + r, noise[rand() % 3], 3, 28 + rand() % 3);
                }
            }
        }
        scr_printf(RESET);
        return;
    }

    // 4. Illumination : le logo passe du blanc au vert
    if (t < T_GLOW)
    {
        int glow[] = {231, 194, 157, 121, 85, 49, LOGO_COLOR};
        double p = (t - T_LOGO) / (T_GLOW - T_LOGO);
        draw_logo(lx, ly, glow[(int)(clamp01(p) * 6.99)]);
        return;
    }

    // 5. Sous-titre écrit lettre par lettre, avec un curseur
    draw_logo(lx, ly, LOGO_COLOR);
    int total = (int)strlen(ATARDES_SUBTITLE);
    int shown = (int)(clamp01((t - T_GLOW) / (T_SUB - T_GLOW)) * total);
    char partial[128];
    snprintf(partial, sizeof(partial), "%.*s", shown, ATARDES_SUBTITLE);
    int sx = w / 2 - total / 2;
    move_to(sx, ly + LOGO_ROWS + 1);
    scr_printf(COLOR_GRAY "%s", partial);
    if (((int)(t * 6)) % 2 == 0 || shown < total)
        scr_printf("\033[38;5;42m█");
    scr_printf(RESET);
}

void run_intro(int term_width, int term_height)
{
    double start = now_seconds();
    while (true)
    {
        double t = now_seconds() - start;
        if (t >= T_END)
            break;
        // Une touche ou un clic passe l'animation ; les mouvements de souris (très nombreux) sont ignorés
        bool skip = false;
        for (int n = 0; n < 64 && !skip; n++)
        {
            InputEvent ev = poll_input();
            if (ev.type == INPUT_NONE)
                break;
            skip = ev.type == INPUT_KEY || ev.type == INPUT_ESCAPE || ev.type == INPUT_ARROW || is_mouse_click(ev);
        }
        if (skip)
            break;
        draw_frame(t, term_width, term_height);
        scr_flush();
        nsleep_seconds(1.0 / 60);
    }
}

void draw_subtitle(int term_width, int term_height)
{
    int lx, ly;
    logo_position(term_width, term_height, &lx, &ly);
    move_to(term_width / 2 - (int)strlen(ATARDES_SUBTITLE) / 2, ly + LOGO_ROWS + 1);
    scr_printf(COLOR_GRAY "%s" RESET, ATARDES_SUBTITLE);
}
