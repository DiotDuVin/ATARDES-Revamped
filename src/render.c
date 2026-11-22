#include "render.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

/*
##############
## Structures ##
##############
*/

#define GLYPH_MAX 16 // octets max pour un caractère UTF-8 + éventuels caractères combinants

#define ATTR_BOLD 1
#define ATTR_UNDERLINE 2

#define COLOR_DEFAULT -1 // couleur par défaut du terminal

typedef struct Attr
{
    short fg;            // -1 = défaut, 0..255 = palette 256 couleurs
    short bg;            // idem
    unsigned char flags; // ATTR_BOLD | ATTR_UNDERLINE
} Attr;

enum CellWidth
{
    CELL_NARROW = 0,   // caractère normal (1 colonne)
    CELL_WIDE_HEAD = 1, // première moitié d'un caractère large (emoji : 2 colonnes)
    CELL_WIDE_TAIL = 2, // seconde moitié d'un caractère large (rien à afficher)
};

typedef struct ScreenCell
{
    char glyph[GLYPH_MAX]; // caractère UTF-8, terminé par '\0'
    Attr attr;
    unsigned char width; // enum CellWidth
} ScreenCell;

typedef struct Screen
{
    bool active;
    int width;
    int height;
    ScreenCell *back;  // ce que le jeu a dessiné
    ScreenCell *front; // ce que le terminal affiche réellement

    // "Curseur virtuel" utilisé par scr_printf (coordonnées 1-based comme le terminal)
    int cursor_x;
    int cursor_y;
    Attr attr; // attributs courants (couleurs), modifiés par les codes ANSI

    // Buffer de sortie réutilisé entre les images (évite un malloc à chaque image)
    char *out;
    size_t out_len;
    size_t out_cap;
} Screen;

static Screen screen = {0};

static const Attr DEFAULT_ATTR = {COLOR_DEFAULT, COLOR_DEFAULT, 0};

/*
###########
## Utils ##
###########
*/

static bool attr_equal(Attr a, Attr b)
{
    return a.fg == b.fg && a.bg == b.bg && a.flags == b.flags;
}

static bool cell_equal(const ScreenCell *a, const ScreenCell *b)
{
    return a->width == b->width && attr_equal(a->attr, b->attr) && strcmp(a->glyph, b->glyph) == 0;
}

static ScreenCell *back_at(int x, int y) // x, y en 1-based
{
    return &screen.back[(y - 1) * screen.width + (x - 1)];
}

static bool in_bounds(int x, int y)
{
    return x >= 1 && y >= 1 && x <= screen.width && y <= screen.height;
}

static void set_blank(ScreenCell *cell, Attr attr)
{
    cell->glyph[0] = ' ';
    cell->glyph[1] = '\0';
    cell->attr = attr;
    cell->width = CELL_NARROW;
}

/*
 * Largeur d'affichage d'un caractère Unicode (codepoint).
 * 0 : caractère "combinant" qui se colle au précédent (ex: sélecteur de variante d'emoji)
 * 2 : caractère large (emojis)
 * 1 : le reste
 * C'est une version simplifiée, suffisante pour les caractères utilisés dans le jeu.
 */
static int codepoint_width(unsigned int cp)
{
    if ((cp >= 0x0300 && cp <= 0x036F) // accents combinants
        || cp == 0x200D                // "zero width joiner"
        || (cp >= 0xFE00 && cp <= 0xFE0F)) // sélecteurs de variante
        return 0;

    if ((cp >= 0x1F300 && cp <= 0x1FAFF) // emojis
        || (cp >= 0x2E80 && cp <= 0xA4CF) // CJK
        || (cp >= 0x231A && cp <= 0x231B) // ⌚ ⌛
        || (cp >= 0x23E9 && cp <= 0x23F3)) // ⏩ ... ⏳
        return 2;

    return 1;
}

// Lit un caractère UTF-8 à partir de s. Renvoie le nombre d'octets lus et écrit le codepoint dans *cp.
static int utf8_decode(const unsigned char *s, unsigned int *cp)
{
    if (s[0] < 0x80)
    {
        *cp = s[0];
        return 1;
    }
    int len;
    if ((s[0] & 0xE0) == 0xC0)
    {
        len = 2;
        *cp = s[0] & 0x1F;
    }
    else if ((s[0] & 0xF0) == 0xE0)
    {
        len = 3;
        *cp = s[0] & 0x0F;
    }
    else if ((s[0] & 0xF8) == 0xF0)
    {
        len = 4;
        *cp = s[0] & 0x07;
    }
    else
    {
        *cp = '?'; // octet invalide
        return 1;
    }
    for (int i = 1; i < len; i++)
    {
        if ((s[i] & 0xC0) != 0x80)
        {
            *cp = '?'; // séquence tronquée
            return i;
        }
        *cp = (*cp << 6) | (s[i] & 0x3F);
    }
    return len;
}

/*
#########################
## Écriture dans le buffer ##
#########################
*/

// Si on écrase la moitié d'un caractère large, on efface l'autre moitié pour ne pas laisser de demi-emoji.
static void break_wide_char(int x, int y)
{
    ScreenCell *cell = back_at(x, y);
    if (cell->width == CELL_WIDE_TAIL && x > 1)
        set_blank(back_at(x - 1, y), back_at(x - 1, y)->attr);
    else if (cell->width == CELL_WIDE_HEAD && x < screen.width)
        set_blank(back_at(x + 1, y), back_at(x + 1, y)->attr);
}

// Place un caractère (bytes, len octets) à la position du curseur virtuel puis avance le curseur.
static void put_glyph(const char *bytes, int len, int width)
{
    int x = screen.cursor_x;
    int y = screen.cursor_y;

    if (width == 0)
    {
        // Caractère combinant : on l'ajoute au caractère précédent
        if (in_bounds(x - 1, y))
        {
            ScreenCell *prev = back_at(x - 1, y);
            if (prev->width == CELL_WIDE_TAIL && x - 2 >= 1)
                prev = back_at(x - 2, y);
            size_t cur = strlen(prev->glyph);
            if (cur + len < GLYPH_MAX)
            {
                memcpy(prev->glyph + cur, bytes, len);
                prev->glyph[cur + len] = '\0';
            }
        }
        return;
    }

    if (in_bounds(x, y))
    {
        break_wide_char(x, y);
        ScreenCell *cell = back_at(x, y);
        memcpy(cell->glyph, bytes, len);
        cell->glyph[len] = '\0';
        cell->attr = screen.attr;
        cell->width = CELL_NARROW;

        if (width == 2)
        {
            if (x + 1 <= screen.width)
            {
                break_wide_char(x + 1, y);
                cell->width = CELL_WIDE_HEAD;
                ScreenCell *tail = back_at(x + 1, y);
                tail->glyph[0] = '\0';
                tail->attr = screen.attr;
                tail->width = CELL_WIDE_TAIL;
            }
            else
            {
                set_blank(cell, screen.attr); // pas la place pour un emoji en bout de ligne
            }
        }
    }
    screen.cursor_x += width;
}

static void clear_rows(int y_from, int y_to)
{
    for (int y = y_from; y <= y_to; y++)
        for (int x = 1; x <= screen.width; x++)
            set_blank(back_at(x, y), (Attr){COLOR_DEFAULT, screen.attr.bg, 0});
}

// Interprète les paramètres d'un code couleur "\033[...m" (SGR)
static void apply_sgr(const int *params, int count)
{
    if (count == 0)
    {
        screen.attr = DEFAULT_ATTR;
        return;
    }
    for (int i = 0; i < count; i++)
    {
        int p = params[i];
        if (p == 0)
            screen.attr = DEFAULT_ATTR;
        else if (p == 1)
            screen.attr.flags |= ATTR_BOLD;
        else if (p == 22)
            screen.attr.flags &= ~ATTR_BOLD;
        else if (p == 4)
            screen.attr.flags |= ATTR_UNDERLINE;
        else if (p == 24)
            screen.attr.flags &= ~ATTR_UNDERLINE;
        else if (p >= 30 && p <= 37)
            screen.attr.fg = p - 30;
        else if (p >= 90 && p <= 97)
            screen.attr.fg = p - 90 + 8;
        else if (p == 39)
            screen.attr.fg = COLOR_DEFAULT;
        else if (p >= 40 && p <= 47)
            screen.attr.bg = p - 40;
        else if (p >= 100 && p <= 107)
            screen.attr.bg = p - 100 + 8;
        else if (p == 49)
            screen.attr.bg = COLOR_DEFAULT;
        else if ((p == 38 || p == 48) && i + 2 < count && params[i + 1] == 5)
        {
            // Couleur de la palette 256 : 38;5;n (texte) ou 48;5;n (fond)
            short color = (short)params[i + 2];
            if (p == 38)
                screen.attr.fg = color;
            else
                screen.attr.bg = color;
            i += 2;
        }
        else if ((p == 38 || p == 48) && i + 1 < count && params[i + 1] == 2)
        {
            i += 4; // couleurs 24 bits non gérées : on ignore r;g;b
        }
        // les autres codes sont ignorés
    }
}

// Interprète une séquence "\033[" ... à partir de s (pointant juste après le '[').
// Renvoie le nombre d'octets consommés.
static int parse_csi(const char *s)
{
    int params[16];
    int count = 0;
    int value = 0;
    bool has_value = false;
    bool private_mode = false; // séquences "\033[?..." (ex: cacher le curseur)
    int i = 0;

    if (s[i] == '?')
    {
        private_mode = true;
        i++;
    }

    while (s[i] != '\0')
    {
        char c = s[i];
        if (c >= '0' && c <= '9')
        {
            value = value * 10 + (c - '0');
            has_value = true;
        }
        else if (c == ';')
        {
            if (count < 16)
                params[count++] = value;
            value = 0;
            has_value = false;
        }
        else
        {
            // Caractère final de la séquence
            if ((has_value || count > 0) && count < 16)
                params[count++] = value;
            i++;

            if (private_mode)
                return i; // ex: \033[?25l, ignoré dans le buffer

            switch (c)
            {
            case 'm':
                apply_sgr(params, count);
                break;
            case 'H':
            case 'f':
                screen.cursor_y = count >= 1 && params[0] > 0 ? params[0] : 1;
                screen.cursor_x = count >= 2 && params[1] > 0 ? params[1] : 1;
                break;
            case 'J': // effacement de l'écran (on ne gère que le cas complet \033[2J)
                clear_rows(1, screen.height);
                break;
            case 'K': // effacement de la ligne courante
                if (screen.cursor_y >= 1 && screen.cursor_y <= screen.height)
                    clear_rows(screen.cursor_y, screen.cursor_y);
                break;
            default:
                break; // séquence non gérée : ignorée
            }
            return i;
        }
        i++;
    }
    return i; // séquence tronquée
}

static void write_to_buffer(const char *s)
{
    while (*s != '\0')
    {
        if (*s == '\033')
        {
            s++;
            if (*s == '[')
                s += 1 + parse_csi(s + 1);
            else if (*s != '\0')
                s++; // autre séquence d'échappement : ignorée
            continue;
        }
        if (*s == '\n')
        {
            screen.cursor_x = 1;
            screen.cursor_y++;
            s++;
            continue;
        }
        if (*s == '\r')
        {
            screen.cursor_x = 1;
            s++;
            continue;
        }
        if (*s == '\t')
        {
            int next = ((screen.cursor_x - 1) / 8 + 1) * 8 + 1;
            while (screen.cursor_x < next)
                put_glyph(" ", 1, 1);
            s++;
            continue;
        }
        if ((unsigned char)*s < 0x20)
        {
            s++; // autres caractères de contrôle ignorés
            continue;
        }

        unsigned int cp;
        int len = utf8_decode((const unsigned char *)s, &cp);
        put_glyph(s, len, codepoint_width(cp));
        s += len;
    }
}

// Vrai si le caractère est composé de plusieurs codepoints (ex: emoji + sélecteur de variante)
static bool glyph_is_composed(const char *glyph)
{
    unsigned int cp;
    int len = utf8_decode((const unsigned char *)glyph, &cp);
    return glyph[len] != '\0';
}

/*
######################
## Envoi au terminal ##
######################
*/

static void out_append(const char *data, size_t len)
{
    if (screen.out_len + len > screen.out_cap)
    {
        size_t new_cap = screen.out_cap * 2 + len + 1024;
        char *new_out = realloc(screen.out, new_cap);
        if (new_out == NULL)
            return; // pas de mémoire : l'image sera incomplète mais le jeu continue
        screen.out = new_out;
        screen.out_cap = new_cap;
    }
    memcpy(screen.out + screen.out_len, data, len);
    screen.out_len += len;
}

static void out_str(const char *s)
{
    out_append(s, strlen(s));
}

static void out_attr(Attr attr)
{
    char seq[64];
    int n = snprintf(seq, sizeof(seq), "\033[0%s%s", (attr.flags & ATTR_BOLD) ? ";1" : "", (attr.flags & ATTR_UNDERLINE) ? ";4" : "");
    if (attr.fg != COLOR_DEFAULT)
        n += snprintf(seq + n, sizeof(seq) - n, ";38;5;%d", attr.fg);
    if (attr.bg != COLOR_DEFAULT)
        n += snprintf(seq + n, sizeof(seq) - n, ";48;5;%d", attr.bg);
    snprintf(seq + n, sizeof(seq) - n, "m");
    out_str(seq);
}

// Écrit tout le buffer de sortie dans le terminal, même si write() n'écrit qu'une partie à la fois.
static void out_flush(void)
{
    fflush(stdout); // au cas où du texte aurait été écrit avec printf avant
    size_t written = 0;
    while (written < screen.out_len)
    {
        ssize_t n = write(STDOUT_FILENO, screen.out + written, screen.out_len - written);
        if (n < 0)
        {
            if (errno == EINTR)
                continue;
            break;
        }
        written += (size_t)n;
    }
    screen.out_len = 0;
}

/*
###############
## API publique ##
###############
*/

bool scr_init(int width, int height)
{
    scr_shutdown();
    if (width <= 0 || height <= 0)
        return false;

    screen.back = malloc(sizeof(ScreenCell) * width * height);
    screen.front = malloc(sizeof(ScreenCell) * width * height);
    if (screen.back == NULL || screen.front == NULL)
    {
        free(screen.back);
        free(screen.front);
        screen.back = screen.front = NULL;
        return false;
    }
    screen.width = width;
    screen.height = height;
    screen.cursor_x = 1;
    screen.cursor_y = 1;
    screen.attr = DEFAULT_ATTR;

    for (int i = 0; i < width * height; i++)
        set_blank(&screen.back[i], DEFAULT_ATTR);
    screen.active = true;
    scr_invalidate();
    return true;
}

void scr_shutdown(void)
{
    if (!screen.active)
        return;
    out_str("\033[0m");
    out_flush();
    free(screen.back);
    free(screen.front);
    free(screen.out);
    screen = (Screen){0};
}

bool scr_is_active(void)
{
    return screen.active;
}

void scr_invalidate(void)
{
    if (!screen.active)
        return;
    // Un caractère impossible dans le front buffer force la réécriture de chaque case
    for (int i = 0; i < screen.width * screen.height; i++)
    {
        screen.front[i].glyph[0] = '\x01';
        screen.front[i].glyph[1] = '\0';
    }
}

unsigned long scr_attr_save(void)
{
    // fg et bg (de -1 à 255) sont décalés de 1 pour être positifs, puis rangés dans un seul entier
    return ((unsigned long)(screen.attr.fg + 1) << 16) | ((unsigned long)(screen.attr.bg + 1) << 4) | screen.attr.flags;
}

void scr_attr_restore(unsigned long saved)
{
    screen.attr.fg = (short)((saved >> 16) & 0x1FF) - 1;
    screen.attr.bg = (short)((saved >> 4) & 0x1FF) - 1;
    screen.attr.flags = saved & 0xF;
}

void scr_puts(const char *str)
{
    if (!screen.active)
    {
        fputs(str, stdout);
        return;
    }
    write_to_buffer(str);
}

int scr_printf(const char *format, ...)
{
    va_list args;
    va_start(args, format);

    if (!screen.active)
    {
        int n = vprintf(format, args);
        va_end(args);
        return n;
    }

    char small[1024];
    va_list copy;
    va_copy(copy, args);
    int n = vsnprintf(small, sizeof(small), format, copy);
    va_end(copy);

    if (n >= (int)sizeof(small))
    {
        // Texte trop long pour le buffer local : allocation temporaire
        char *big = malloc((size_t)n + 1);
        if (big != NULL)
        {
            vsnprintf(big, (size_t)n + 1, format, args);
            write_to_buffer(big);
            free(big);
        }
    }
    else if (n > 0)
    {
        write_to_buffer(small);
    }
    va_end(args);
    return n;
}

void scr_present(void)
{
    if (!screen.active)
        return;

    // Demande au terminal de n'afficher l'image qu'une fois complète (ignoré par les terminaux qui ne le gèrent pas)
    out_str("\033[?2026h");

    int term_x = -1, term_y = -1; // position réelle du curseur du terminal (-1 = inconnue)
    Attr term_attr = {-2, -2, 0xff}; // attributs actuels du terminal (inconnus au départ)

    for (int y = 1; y <= screen.height; y++)
    {
        for (int x = 1; x <= screen.width; x++)
        {
            int i = (y - 1) * screen.width + (x - 1);
            ScreenCell *b = &screen.back[i];
            ScreenCell *f = &screen.front[i];

            if (b->width == CELL_WIDE_TAIL)
                continue; // déjà traité avec la première moitié

            bool changed = !cell_equal(b, f);
            if (b->width == CELL_WIDE_HEAD && x < screen.width)
                changed = changed || !cell_equal(&screen.back[i + 1], &screen.front[i + 1]);
            if (!changed)
                continue;

            if (term_x != x || term_y != y)
            {
                char seq[32];
                snprintf(seq, sizeof(seq), "\033[%d;%dH", y, x);
                out_str(seq);
            }
            if (!attr_equal(term_attr, b->attr))
            {
                out_attr(b->attr);
                term_attr = b->attr;
            }
            out_str(b->glyph[0] != '\0' ? b->glyph : " ");

            *f = *b;
            int advance = 1;
            if (b->width == CELL_WIDE_HEAD && x < screen.width)
            {
                screen.front[i + 1] = screen.back[i + 1];
                advance = 2;
            }
            term_x = x + advance;
            term_y = y;

            // Les emojis et caractères composés n'ont pas la même largeur selon les terminaux.
            // Pour ne pas décaler le reste de la ligne, on repositionne le curseur explicitement après eux.
            if (b->width == CELL_WIDE_HEAD || glyph_is_composed(b->glyph))
                term_x = -1;
        }
    }

    if (term_x == -1 && term_y == -1)
    {
        screen.out_len = 0; // rien n'a changé : on n'envoie rien au terminal
        return;
    }
    out_str("\033[0m\033[?2026l");
    out_flush();
}

void scr_flush(void)
{
    if (screen.active)
        scr_present();
    else
        fflush(stdout);
}
