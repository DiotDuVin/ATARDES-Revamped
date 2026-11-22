#include "bigtext.h"
#include "render.h"
#include "shared.h"

#include <string.h>

bool big_text_mode = true;

/*
###########
## Police ##
###########
Chaque glyphe fait 3 pixels de large et 5 de haut ('#' = allumé).
*/

typedef struct Glyph
{
    unsigned int codepoint;
    const char *rows[5];
} Glyph;

static const Glyph FONT[] = {
    {'A', {"###", "#.#", "###", "#.#", "#.#"}},
    {'B', {"##.", "#.#", "##.", "#.#", "##."}},
    {'C', {"###", "#..", "#..", "#..", "###"}},
    {'D', {"##.", "#.#", "#.#", "#.#", "##."}},
    {'E', {"###", "#..", "##.", "#..", "###"}},
    {'F', {"###", "#..", "##.", "#..", "#.."}},
    {'G', {"###", "#..", "#.#", "#.#", "###"}},
    {'H', {"#.#", "#.#", "###", "#.#", "#.#"}},
    {'I', {"###", ".#.", ".#.", ".#.", "###"}},
    {'J', {"..#", "..#", "..#", "#.#", "###"}},
    {'K', {"#.#", "#.#", "##.", "#.#", "#.#"}},
    {'L', {"#..", "#..", "#..", "#..", "###"}},
    {'M', {"#.#", "###", "###", "#.#", "#.#"}},
    {'N', {"##.", "#.#", "#.#", "#.#", "#.#"}},
    {'O', {".#.", "#.#", "#.#", "#.#", ".#."}},
    {'P', {"###", "#.#", "###", "#..", "#.."}},
    {'Q', {".#.", "#.#", "#.#", "##.", ".##"}},
    {'R', {"##.", "#.#", "##.", "#.#", "#.#"}},
    {'S', {".##", "#..", ".#.", "..#", "##."}},
    {'T', {"###", ".#.", ".#.", ".#.", ".#."}},
    {'U', {"#.#", "#.#", "#.#", "#.#", "###"}},
    {'V', {"#.#", "#.#", "#.#", "#.#", ".#."}},
    {'W', {"#.#", "#.#", "###", "###", "#.#"}},
    {'X', {"#.#", "#.#", ".#.", "#.#", "#.#"}},
    {'Y', {"#.#", "#.#", "###", ".#.", ".#."}},
    {'Z', {"###", "..#", ".#.", "#..", "###"}},
    {'0', {"###", "#.#", "#.#", "#.#", "###"}},
    {'1', {".#.", "##.", ".#.", ".#.", "###"}},
    {'2', {"###", "..#", "###", "#..", "###"}},
    {'3', {"###", "..#", ".##", "..#", "###"}},
    {'4', {"#.#", "#.#", "###", "..#", "..#"}},
    {'5', {"###", "#..", "###", "..#", "###"}},
    {'6', {"###", "#..", "###", "#.#", "###"}},
    {'7', {"###", "..#", "..#", ".#.", ".#."}},
    {'8', {"###", "#.#", "###", "#.#", "###"}},
    {'9', {"###", "#.#", "###", "..#", "###"}},
    {'.', {"...", "...", "...", "...", ".#."}},
    {',', {"...", "...", "...", ".#.", "#.."}},
    {':', {"...", ".#.", "...", ".#.", "..."}},
    {';', {"...", ".#.", "...", ".#.", "#.."}},
    {'!', {".#.", ".#.", ".#.", "...", ".#."}},
    {'?', {"###", "..#", ".##", "...", ".#."}},
    {'\'', {".#.", ".#.", "...", "...", "..."}},
    {'"', {"#.#", "#.#", "...", "...", "..."}},
    {'-', {"...", "...", "###", "...", "..."}},
    {'+', {"...", ".#.", "###", ".#.", "..."}},
    {'/', {"..#", "..#", ".#.", "#..", "#.."}},
    {'(', {".#.", "#..", "#..", "#..", ".#."}},
    {')', {".#.", "..#", "..#", "..#", ".#."}},
    {'[', {"##.", "#..", "#..", "#..", "##."}},
    {']', {".##", "..#", "..#", "..#", ".##"}},
    {'<', {"..#", ".#.", "#..", ".#.", "..#"}},
    {'>', {"#..", ".#.", "..#", ".#.", "#.."}},
    {'=', {"...", "###", "...", "###", "..."}},
    {'*', {"#.#", ".#.", "#.#", "...", "..."}},
    {'#', {"#.#", "###", "#.#", "###", "#.#"}},
    {'&', {".#.", "#.#", ".#.", "#.#", ".##"}},
    {'_', {"...", "...", "...", "...", "###"}},
    {'%', {"#.#", "..#", ".#.", "#..", "#.#"}},
    {'$', {".##", "##.", ".#.", ".##", "##."}},
    {0x20AC /* € */, {".##", "#..", "###", "#..", ".##"}},
    {0x2764 /* ❤ */, {"#.#", "###", "###", ".#.", "..."}},
    {0x2665 /* ♥ */, {"#.#", "###", "###", ".#.", "..."}},
    {0x25B6 /* ▶ */, {"#..", "##.", "###", "##.", "#.."}},
    {0x2192 /* → */, {"...", "..#", "###", "..#", "..."}},
    {0x2190 /* ← */, {"...", "#..", "###", "#..", "..."}},
    {0x2191 /* ↑ */, {".#.", "###", ".#.", ".#.", "..."}},
    {0x2193 /* ↓ */, {"...", ".#.", ".#.", "###", ".#."}},
    {' ', {"...", "...", "...", "...", "..."}},
    {0xA0 /* espace insécable : jamais coupé en fin de ligne */, {"...", "...", "...", "...", "..."}},
};

static const Glyph *find_glyph(unsigned int cp)
{
    for (size_t i = 0; i < sizeof(FONT) / sizeof(FONT[0]); i++)
        if (FONT[i].codepoint == cp)
            return &FONT[i];
    return NULL;
}

// Lit un caractère UTF-8 ; renvoie le nombre d'octets lus
static int utf8_next(const char *s, unsigned int *cp)
{
    const unsigned char *u = (const unsigned char *)s;
    if (u[0] < 0x80)
    {
        *cp = u[0];
        return 1;
    }
    int len = (u[0] & 0xE0) == 0xC0 ? 2 : (u[0] & 0xF0) == 0xE0 ? 3 : (u[0] & 0xF8) == 0xF0 ? 4 : 1;
    *cp = len == 2 ? (u[0] & 0x1F) : len == 3 ? (u[0] & 0x0F) : (u[0] & 0x07);
    for (int i = 1; i < len; i++)
    {
        if ((u[i] & 0xC0) != 0x80)
        {
            *cp = '?';
            return i;
        }
        *cp = (*cp << 6) | (u[i] & 0x3F);
    }
    return len;
}

// Convertit minuscules et lettres accentuées vers les majuscules de la police
static unsigned int normalize(unsigned int cp)
{
    if (cp >= 'a' && cp <= 'z')
        return cp - 'a' + 'A';
    switch (cp)
    {
    case 0xE0: case 0xE2: case 0xE4: case 0xC0: case 0xC2: case 0xC4: // à â ä
        return 'A';
    case 0xE9: case 0xE8: case 0xEA: case 0xEB: case 0xC9: case 0xC8: case 0xCA: case 0xCB: // é è ê ë
        return 'E';
    case 0xEE: case 0xEF: case 0xCE: case 0xCF: // î ï
        return 'I';
    case 0xF4: case 0xF6: case 0xD4: case 0xD6: // ô ö
        return 'O';
    case 0xF9: case 0xFB: case 0xFC: case 0xD9: case 0xDB: case 0xDC: // ù û ü
        return 'U';
    case 0xE7: case 0xC7: // ç
        return 'C';
    case 0x2019: case 0x2018: // ’ ‘
        return '\'';
    case 0xAB: case 0xBB: case 0x201C: case 0x201D: // « » “ ”
        return '"';
    case 0x2026: // …
        return '.';
    default:
        return cp;
    }
}

// Colonnes allumées d'un glyphe : les signes de ponctuation fins sont rognés pour un meilleur espacement
static void glyph_columns(const Glyph *g, int *first, int *last)
{
    *first = 0;
    *last = 2;
    unsigned int cp = g->codepoint;
    bool narrow = cp == '.' || cp == ',' || cp == ':' || cp == ';' || cp == '!' || cp == '\'' || cp == '(' || cp == ')';
    if (!narrow)
        return;
    *first = 3;
    *last = -1;
    for (int r = 0; r < 5; r++)
        for (int c = 0; c < 3; c++)
            if (g->rows[r][c] == '#')
            {
                if (c < *first)
                    *first = c;
                if (c > *last)
                    *last = c;
            }
    if (*last < *first)
        *first = *last = 1;
}

// Saute une séquence d'échappement ANSI ; renvoie sa longueur (0 si ce n'en est pas une)
static int escape_length(const char *s)
{
    if (s[0] != '\033')
        return 0;
    int i = 1;
    if (s[i] == '[')
    {
        i++;
        while (s[i] != '\0' && !((s[i] >= 'A' && s[i] <= 'Z') || (s[i] >= 'a' && s[i] <= 'z')))
            i++;
        if (s[i] != '\0')
            i++;
    }
    return i;
}

/*
##################
## Mesure et dessin ##
##################
*/

int ui_text_rows(void)
{
    return big_text_mode ? 3 : 1;
}

// Largeur d'un caractère en grande police (espace entre les lettres compris)
static int big_char_width(unsigned int cp)
{
    const Glyph *g = find_glyph(normalize(cp));
    if (g == NULL)
        g = find_glyph('?');
    int first, last;
    glyph_columns(g, &first, &last);
    return last - first + 2;
}

// Largeur de `len` octets de texte
static int width_of(const char *text, size_t len)
{
    int w = 0;
    size_t i = 0;
    while (i < len && text[i] != '\0')
    {
        int esc = escape_length(text + i);
        if (esc > 0)
        {
            i += esc;
            continue;
        }
        unsigned int cp;
        i += utf8_next(text + i, &cp);
        if (cp == 0xFE0F)
            continue; // sélecteur de variante d'emoji : invisible
        w += big_text_mode ? big_char_width(cp) : 1;
    }
    if (big_text_mode && w > 0)
        w -= 1; // pas d'espace après la dernière lettre
    return w;
}

int ui_text_width(const char *text)
{
    return width_of(text, strlen(text));
}

// Dessine `len` octets de texte en grande police
static int big_draw(int x, int y, const char *text, size_t len)
{
    unsigned long base_attr = scr_attr_save();
    int width = 0;
    for (int row = 0; row < 3; row++)
    {
        scr_attr_restore(base_attr);
        move_to(x, y + row);
        int col = 0;
        size_t i = 0;
        while (i < len && text[i] != '\0')
        {
            int esc = escape_length(text + i);
            if (esc > 0)
            {
                char seq[32];
                int n = esc < 31 ? esc : 31;
                memcpy(seq, text + i, n);
                seq[n] = '\0';
                scr_puts(seq); // les couleurs sont appliquées à chaque ligne du glyphe
                i += esc;
                continue;
            }
            unsigned int cp;
            i += utf8_next(text + i, &cp);
            if (cp == 0xFE0F)
                continue;
            const Glyph *g = find_glyph(normalize(cp));
            if (g == NULL)
                g = find_glyph('?');
            int first, last;
            glyph_columns(g, &first, &last);

            // Deux lignes de pixels par ligne du terminal : ▀ = haut, ▄ = bas, █ = les deux
            const char *top = row * 2 < 5 ? g->rows[row * 2] : "...";
            const char *bottom = row * 2 + 1 < 5 ? g->rows[row * 2 + 1] : "...";
            for (int c = first; c <= last; c++)
            {
                bool t = top[c] == '#', b = bottom[c] == '#';
                scr_puts(t && b ? "█" : t ? "▀" : b ? "▄" : " ");
                col++;
            }
            if (i < len && text[i] != '\0')
            {
                scr_puts(" "); // espace entre deux lettres
                col++;
            }
        }
        width = col;
    }
    return width;
}

static int draw_span(int x, int y, const char *text, size_t len)
{
    if (big_text_mode)
        return big_draw(x, y, text, len);
    move_to(x, y);
    char buffer[512];
    size_t n = len < sizeof(buffer) - 1 ? len : sizeof(buffer) - 1;
    memcpy(buffer, text, n);
    buffer[n] = '\0';
    scr_puts(buffer);
    return width_of(text, len);
}

int ui_text(int x, int y, const char *text)
{
    return draw_span(x, y, text, strlen(text));
}

int ui_text_centered(int center_x, int y, const char *text)
{
    int w = ui_text_width(text);
    return ui_text(center_x - w / 2, y, text);
}

/*
###########################
## Texte sur plusieurs lignes ##
###########################
On coupe entre les mots. Les codes couleur rencontrés sont mémorisés pour être
réappliqués au début de chaque nouvelle ligne.
*/

static int wrap(int x, int y, int max_width, int line_gap, const char *text, bool draw)
{
    int rows = 0;
    const char *line_start = text;
    char colors[256] = ""; // codes couleur actifs au début de la ligne courante

    while (*line_start != '\0')
    {
        // Cherche la plus longue suite de mots qui tient dans max_width
        const char *p = line_start;
        const char *line_end = line_start;
        while (*p != '\0' && *p != '\n')
        {
            const char *word_end = p;
            while (*word_end == ' ')
                word_end++;
            while (*word_end != '\0' && *word_end != ' ' && *word_end != '\n')
                word_end++;
            if (width_of(line_start, word_end - line_start) > max_width && line_end != line_start)
                break;
            line_end = word_end;
            p = word_end;
            if (width_of(line_start, line_end - line_start) > max_width)
                break; // mot plus long que la ligne entière : on le laisse dépasser
        }
        if (line_end == line_start && *p == '\n')
            line_end = p; // ligne vide

        if (draw)
        {
            unsigned long attr = scr_attr_save();
            scr_puts(colors);
            draw_span(x, y + rows, line_start, line_end - line_start);
            scr_attr_restore(attr);
        }
        // Mémorise les codes couleur de cette ligne pour la suivante
        for (const char *q = line_start; q < line_end; q++)
        {
            int esc = escape_length(q);
            if (esc > 0)
            {
                if (strncmp(q, RESET, strlen(RESET)) == 0)
                    colors[0] = '\0';
                else if (strlen(colors) + esc < sizeof(colors))
                    strncat(colors, q, esc);
                q += esc - 1;
            }
        }
        rows += ui_text_rows() + line_gap;

        line_start = line_end;
        while (*line_start == ' ')
            line_start++;
        if (*line_start == '\n')
            line_start++;
    }
    return rows > 0 ? rows - line_gap : 0;
}

int ui_text_wrapped(int x, int y, int max_width, int line_gap, const char *text)
{
    return wrap(x, y, max_width, line_gap, text, true);
}

int ui_text_wrapped_rows(int max_width, int line_gap, const char *text)
{
    return wrap(0, 0, max_width, line_gap, text, false);
}

int ui_text_split(int max_width, const char *text, int *starts, int *lens, int max_lines)
{
    int count = 0;
    const char *line_start = text;
    while (*line_start != '\0' && count < max_lines)
    {
        const char *p = line_start;
        const char *line_end = line_start;
        while (*p != '\0' && *p != '\n')
        {
            const char *word_end = p;
            while (*word_end == ' ')
                word_end++;
            while (*word_end != '\0' && *word_end != ' ' && *word_end != '\n')
                word_end++;
            if (width_of(line_start, word_end - line_start) > max_width && line_end != line_start)
                break;
            line_end = word_end;
            p = word_end;
            if (width_of(line_start, line_end - line_start) > max_width)
                break;
        }
        if (line_end == line_start && *p == '\n')
            line_end = p;
        starts[count] = (int)(line_start - text);
        lens[count] = (int)(line_end - line_start);
        count++;
        line_start = line_end;
        while (*line_start == ' ')
            line_start++;
        if (*line_start == '\n')
            line_start++;
    }
    return count;
}

int ui_text_span(int x, int y, const char *text, int len)
{
    return draw_span(x, y, text, (size_t)len);
}

int ui_text_width_raw(const char *text)
{
    int w = 0;
    size_t i = 0;
    while (text[i] != '\0')
    {
        int esc = escape_length(text + i);
        if (esc > 0)
        {
            i += esc;
            continue;
        }
        unsigned int cp;
        i += utf8_next(text + i, &cp);
        if (cp != 0xFE0F)
            w++;
    }
    return w;
}

int ui_text_fit(int x, int y, int max_width, const char *text)
{
    if (ui_text_width(text) <= max_width || !big_text_mode)
        return ui_text(x, y, text);
    // Trop long pour la grande police : on l'écrit en taille normale, centré dans la hauteur de la ligne
    big_text_mode = false;
    int w = ui_text(x, y + 1, text);
    big_text_mode = true;
    return w;
}
