#include "pixelart.h"
#include "tower.h"

#include <string.h>

float anim_clock = 0;

/*
############
## Toile ##
############
*/

void canvas_init(Canvas *c, int w, int h)
{
    c->w = w < CANVAS_MAX ? w : CANVAS_MAX;
    c->h = h < CANVAS_MAX ? h : CANVAS_MAX;
    for (int i = 0; i < CANVAS_MAX * CANVAS_MAX; i++)
        c->px[i] = TRANSPARENT;
}

void canvas_set(Canvas *c, int x, int y, short color)
{
    if (x >= 0 && y >= 0 && x < c->w && y < c->h)
        c->px[y * CANVAS_MAX + x] = color;
}

short canvas_get(const Canvas *c, int x, int y)
{
    if (x >= 0 && y >= 0 && x < c->w && y < c->h)
        return c->px[y * CANVAS_MAX + x];
    return TRANSPARENT;
}

// Écrit un caractère représentant deux pixels (haut et bas) ; une couleur négative = fond par défaut du terminal
static void put_two_pixels(int tx, int ty, short top, short bottom)
{
    if (!scr_is_active())
        return;
    if (top == bottom)
        scr_put_cell(tx, ty, " ", -1, top);
    else if (bottom < 0)
        scr_put_cell(tx, ty, "▀", top, -1); // pixel du haut seulement
    else if (top < 0)
        scr_put_cell(tx, ty, "▄", bottom, -1); // pixel du bas seulement
    else
        scr_put_cell(tx, ty, "▀", top, bottom); // ▀ : couleur du texte en haut, du fond en bas
}

void canvas_draw(const Canvas *c, int term_x, int term_y, short background, bool skip_empty)
{
    for (int row = 0; row * 2 < c->h; row++)
        for (int x = 0; x < c->w; x++)
        {
            short top = canvas_get(c, x, row * 2);
            short bottom = canvas_get(c, x, row * 2 + 1);
            if (top == TRANSPARENT && bottom == TRANSPARENT && skip_empty)
                continue; // on laisse ce qu'il y avait dessous
            if (top == TRANSPARENT)
                top = background;
            if (bottom == TRANSPARENT)
                bottom = background;
            put_two_pixels(term_x + x, term_y + row, top, bottom);
        }
}

/*
#####################
## Formes de base ##
#####################
Coordonnées en pixels ; (cx, cy) est un centre "continu" (8.5 = milieu entre les pixels 8 et 9).
*/

static float dist(float x, float y, float cx, float cy)
{
    return sqrtf((x + 0.5f - cx) * (x + 0.5f - cx) + (y + 0.5f - cy) * (y + 0.5f - cy));
}

static void fill_circle(Canvas *c, float cx, float cy, float r, short color)
{
    for (int y = (int)(cy - r - 1); y <= (int)(cy + r + 1); y++)
        for (int x = (int)(cx - r - 1); x <= (int)(cx + r + 1); x++)
            if (dist(x, y, cx, cy) <= r)
                canvas_set(c, x, y, color);
}

static void ring(Canvas *c, float cx, float cy, float r_in, float r_out, short color)
{
    for (int y = (int)(cy - r_out - 1); y <= (int)(cy + r_out + 1); y++)
        for (int x = (int)(cx - r_out - 1); x <= (int)(cx + r_out + 1); x++)
        {
            float d = dist(x, y, cx, cy);
            if (d <= r_out && d > r_in)
                canvas_set(c, x, y, color);
        }
}

static void line(Canvas *c, float x0, float y0, float x1, float y1, short color)
{
    float len = sqrtf((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0));
    int steps = (int)(len * 3) + 1;
    for (int i = 0; i <= steps; i++)
    {
        float t = (float)i / steps;
        canvas_set(c, (int)floorf(x0 + (x1 - x0) * t), (int)floorf(y0 + (y1 - y0) * t), color);
    }
}

// Petit nombre pseudo-aléatoire stable (même résultat pour les mêmes entrées) : pour le bruit des flammes, etc.
static unsigned int hash3(int a, int b, int c)
{
    unsigned int h = (unsigned int)a * 73856093u ^ (unsigned int)b * 19349663u ^ (unsigned int)c * 83492791u;
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    h ^= h >> 15;
    return h;
}

// Dessine un sprite défini par des lettres ; chaque lettre est traduite en couleur par `palette`
static void blit_sprite(Canvas *c, const char *const rows[], int nrows, int ox, int oy, bool flip, short (*palette)(char))
{
    for (int y = 0; y < nrows; y++)
    {
        int w = (int)strlen(rows[y]);
        for (int x = 0; x < w; x++)
        {
            short color = palette(rows[y][x]);
            if (color != TRANSPARENT)
                canvas_set(c, ox + (flip ? w - 1 - x : x), oy + y, color);
        }
    }
}

/*
#######################
## Cases et tourelles ##
#######################
Une case fait 18 x 9 caractères, soit 18 x 18 pixels. Centre : (9, 9).
*/

#define CELL_PX_W CELL_WIDTH
#define CELL_PX_H (CELL_HEIGHT * 2)
#define CX 9.0f
#define CY 9.0f

// Emplacement vide : dalle avec un léger relief (bord clair en haut à gauche, sombre en bas à droite)
static void draw_slot(Canvas *c, bool selected)
{
    for (int y = 0; y < c->h; y++)
        for (int x = 0; x < c->w; x++)
        {
            short color = 236;
            if (x == 0 || y == 0)
                color = 238;
            else if (x == c->w - 1 || y == c->h - 1)
                color = 234;
            canvas_set(c, x, y, color);
        }
    if (selected)
    {
        // cadre jaune fin, plus épais dans les coins
        for (int x = 0; x < c->w; x++)
        {
            canvas_set(c, x, 0, 221);
            canvas_set(c, x, c->h - 1, 221);
        }
        for (int y = 0; y < c->h; y++)
        {
            canvas_set(c, 0, y, 221);
            canvas_set(c, c->w - 1, y, 221);
        }
        int corners[4][2] = {{1, 1}, {c->w - 2, 1}, {1, c->h - 2}, {c->w - 2, c->h - 2}};
        for (int i = 0; i < 4; i++)
        {
            canvas_set(c, corners[i][0], corners[i][1], 228);
            canvas_set(c, corners[i][0] + (i % 2 == 0 ? 1 : -1), corners[i][1], 228);
            canvas_set(c, corners[i][0], corners[i][1] + (i < 2 ? 1 : -1), 228);
        }
    }
}

// Socle rond commun aux tourelles, éclairé depuis le haut à gauche
static void draw_platform(Canvas *c, short fill, short light, short dark)
{
    for (int y = 0; y < c->h; y++)
        for (int x = 0; x < c->w; x++)
        {
            float d = dist(x, y, CX, CY);
            if (d > 7.6f)
                continue;
            if (d > 6.6f)
            {
                // bord éclairé en haut à gauche, dans l'ombre en bas à droite, ton intermédiaire entre les deux
                float side = (x + 0.5f - CX) + (y + 0.5f - CY);
                canvas_set(c, x, y, side < -3.0f ? light : (side > 3.0f ? dark : fill + 2));
            }
            else if (d > 5.9f)
                canvas_set(c, x, y, dark);
            else
                canvas_set(c, x, y, fill);
        }
}

// Points dorés indiquant le niveau, en bas du socle
static void draw_level_pips(Canvas *c, int lvl)
{
    int n = lvl + 1;
    int start = 9 - n;
    for (int i = 0; i < n; i++)
    {
        canvas_set(c, start + i * 2, 16, 220);
        canvas_set(c, start + i * 2, 15, 178);
    }
}

// Direction du dernier tir (vers le haut si la tourelle n'a jamais tiré)
static void aim_direction(const struct Turret *t, float *dx, float *dy)
{
    float len = sqrtf(t->last_shot_dx * t->last_shot_dx + t->last_shot_dy * t->last_shot_dy);
    if (len < 0.01f)
    {
        *dx = 0;
        *dy = -1;
        return;
    }
    *dx = t->last_shot_dx / len;
    *dy = t->last_shot_dy / len;
}

// Vrai juste après un tir
static bool just_fired(const struct Turret *t, float window)
{
    float reload = t->reload_delay[t->lvl];
    return t->in_range && t->compteur < MIN(window, reload * 0.5f);
}

static void draw_sniper(Canvas *c, const struct Turret *t)
{
    short head[3] = {35, 42, 49}, head_dark[3] = {29, 36, 43}, head_light[3] = {78, 85, 122};
    int lvl = t->lvl;
    draw_platform(c, 237, 243, 234);
    float dx, dy;
    aim_direction(t, &dx, &dy);
    float length = 6.5f + lvl;
    // ombre du canon puis canon
    line(c, CX + 0.6f, CY + 0.9f, CX + dx * length + 0.6f, CY + dy * length + 0.9f, 234);
    line(c, CX, CY, CX + dx * length, CY + dy * length, 252);
    if (lvl >= 1)
        line(c, CX - dy * 0.9f, CY + dx * 0.9f, CX + dx * (length - 2) - dy * 0.9f, CY + dy * (length - 2) + dx * 0.9f, 246);
    canvas_set(c, (int)floorf(CX + dx * length), (int)floorf(CY + dy * length), 196); // pointe rouge
    if (just_fired(t, 0.12f))
    {
        fill_circle(c, CX + dx * (length + 1.2f), CY + dy * (length + 1.2f), 1.0f, 226);
        canvas_set(c, (int)floorf(CX + dx * (length + 1.2f)), (int)floorf(CY + dy * (length + 1.2f)), 231);
    }
    // tête
    fill_circle(c, CX, CY, 3.3f, head_dark[lvl]);
    fill_circle(c, CX - 0.3f, CY - 0.3f, 2.6f, head[lvl]);
    canvas_set(c, 7, 7, head_light[lvl]);
    if (lvl == 2)
        canvas_set(c, 9, 9, 196); // viseur
    draw_level_pips(c, lvl);
}

static void draw_gatling(Canvas *c, const struct Turret *t)
{
    int lvl = t->lvl;
    draw_platform(c, 237, 243, 234);
    if (lvl >= 1)
        ring(c, CX, CY, 5.9f, 6.6f, 161); // liseré rouge au niveau 2
    float dx, dy;
    aim_direction(t, &dx, &dy);
    float px = -dy, py = dx; // perpendiculaire
    float length = 6.0f + lvl;
    int spin = t->in_range ? (int)(anim_clock * 24) % 3 : 0;
    for (int b = -1; b <= 1; b++)
    {
        short color = (b + 1 == spin) ? 255 : (lvl ? 250 : 247);
        line(c, CX + px * b, CY + py * b, CX + dx * length + px * b, CY + dy * length + py * b, color);
    }
    if (t->in_range)
    {
        // flammes de bouche qui clignotent pendant le tir continu
        short flash = ((int)(anim_clock * 30) % 2) ? 226 : 208;
        fill_circle(c, CX + dx * (length + 1.3f), CY + dy * (length + 1.3f), 1.1f, flash);
    }
    fill_circle(c, CX, CY, 3.4f, 240);
    fill_circle(c, CX - 0.4f, CY - 0.4f, 2.4f, 244);
    canvas_set(c, 9, 9, lvl ? 161 : 236);
    draw_level_pips(c, lvl);
}

static void draw_mortier(Canvas *c, const struct Turret *t)
{
    int lvl = t->lvl;
    draw_platform(c, 237, 243, 234);
    bool firing = t->compteur_mortier > 0;
    bool cooling = !firing && t->in_range && t->compteur < t->reload_delay[lvl] * 0.35f;
    if (lvl >= 1)
    {
        // boulons de renfort
        int bolts[4][2] = {{4, 4}, {13, 4}, {4, 13}, {13, 13}};
        for (int i = 0; i < 4; i++)
            canvas_set(c, bolts[i][0], bolts[i][1], 250);
    }
    // tube vu de dessus
    ring(c, CX, CY, 3.3f, 5.0f, cooling ? 67 : (lvl ? 247 : 244));
    ring(c, CX, CY, 4.3f, 5.0f, cooling ? 24 : 240);
    fill_circle(c, CX, CY, 3.3f, 232);
    if (firing)
    {
        fill_circle(c, CX, CY, 3.0f, 202);
        fill_circle(c, CX, CY, 2.0f, 214);
        fill_circle(c, CX, CY, 1.0f, 230);
        // fumée
        for (int i = 0; i < 6; i++)
        {
            unsigned int h = hash3(i, (int)(anim_clock * 10), 7);
            canvas_set(c, 4 + h % 10, 2 + (h >> 8) % 5, 245 + (h >> 16) % 3);
        }
    }
    draw_level_pips(c, lvl);
}

static void draw_inferno(Canvas *c, const struct Turret *t)
{
    int lvl = t->lvl;
    draw_platform(c, 236, 242, 233);
    ring(c, CX, CY, 4.3f, 5.6f, lvl ? 130 : 94); // vasque en bronze
    fill_circle(c, CX, CY, 4.3f, 52);
    // flammes : bruit qui change 12 fois par seconde, plus chaud au centre et vers le haut
    static const short heat[] = {52, 88, 124, 160, 196, 202, 208, 214, 220, 226, 229};
    int frame = (int)(anim_clock * 12);
    for (int y = 1; y < 16; y++)
        for (int x = 3; x < 15; x++)
        {
            float d = dist(x, y, CX, CY + 1.0f);
            float noise = (hash3(x, y, frame) % 1000) / 1000.0f;
            float v = 1.0f - d / 4.8f + noise * 0.45f + (CY - y) * 0.05f;
            if (d < 4.8f && v > 0.25f)
            {
                int idx = (int)(v * 10);
                canvas_set(c, x, y, heat[CLAMP(idx, 0, 10)]);
            }
        }
    // jets de flammes au moment du tir (4 directions, 8 au niveau 2)
    if (t->in_range && t->compteur < t->reload_delay[lvl] * 0.45f)
    {
        int jets = lvl ? 8 : 4;
        for (int j = 0; j < jets; j++)
        {
            float a = (lvl ? 0 : PI / 4) + j * 2 * PI / jets;
            float jx = cosf(a), jy = sinf(a);
            for (float r = 4.5f; r < 8.6f; r += 0.5f)
            {
                short color = r < 6 ? 226 : (r < 7.3f ? 208 : 160);
                if (hash3(j, (int)(r * 2), frame) % 5 != 0)
                    canvas_set(c, (int)floorf(CX + jx * r), (int)floorf(CY + jy * r), color);
            }
        }
    }
    draw_level_pips(c, lvl);
}

static void draw_freezer(Canvas *c, const struct Turret *t)
{
    int lvl = t->lvl;
    draw_platform(c, 236, 243, 234);
    fill_circle(c, CX, CY, 5.8f, lvl ? 24 : 23);
    // flocon à 6 branches qui tourne lentement
    float a0 = anim_clock * 0.6f;
    short arm = lvl ? 87 : 51;
    for (int i = 0; i < 6; i++)
    {
        float a = a0 + i * PI / 3;
        float ax = cosf(a), ay = sinf(a);
        line(c, CX, CY, CX + ax * 5.6f, CY + ay * 5.6f, arm);
        for (int s = -1; s <= 1; s += 2)
        {
            float b = a + s * PI / 4;
            float bx = CX + ax * 3.4f, by = CY + ay * 3.4f;
            line(c, bx, by, bx + cosf(b) * 1.8f, by + sinf(b) * 1.8f, 117);
            if (lvl)
            {
                float b2x = CX + ax * 5.0f, b2y = CY + ay * 5.0f;
                line(c, b2x, b2y, b2x + cosf(b) * 1.2f, b2y + sinf(b) * 1.2f, 159);
            }
        }
        canvas_set(c, (int)floorf(CX + ax * 5.6f), (int)floorf(CY + ay * 5.6f), 195);
    }
    fill_circle(c, CX, CY, 1.1f, 231);
    // onde de froid au moment du tir
    float wave = t->reload_delay[lvl] * 0.35f;
    if (t->in_range && t->compteur < wave)
    {
        float r = 2.5f + 6.0f * (t->compteur / wave);
        ring(c, CX, CY, r - 0.7f, r, r < 6 ? 159 : 117);
    }
    draw_level_pips(c, lvl);
}

static void draw_petrificateur(Canvas *c, const struct Turret *t)
{
    int lvl = t->lvl;
    draw_platform(c, 235, 241, 233);
    ring(c, CX, CY, 5.4f, 6.2f, lvl ? 34 : 28); // cercle de runes
    // pierres en orbite
    int stones = lvl ? 6 : 4;
    for (int i = 0; i < stones; i++)
    {
        float a = anim_clock * 1.5f + i * 2 * PI / stones;
        canvas_set(c, (int)floorf(CX + cosf(a) * 5.8f), (int)floorf(CY + sinf(a) * 5.8f), lvl ? 118 : 82);
        float a2 = a - 0.35f;
        canvas_set(c, (int)floorf(CX + cosf(a2) * 5.8f), (int)floorf(CY + sinf(a2) * 5.8f), lvl ? 70 : 64);
    }
    // orbe central qui pulse
    float pulse = 0.5f + 0.5f * sinf(anim_clock * 4);
    fill_circle(c, CX, CY, 2.8f, 130);
    fill_circle(c, CX, CY, 2.2f, pulse > 0.5f ? 214 : 178);
    fill_circle(c, CX - 0.4f, CY - 0.4f, 1.0f, pulse > 0.7f ? 229 : 220);
    if (t->in_range && t->compteur < 0.25f)
        ring(c, CX, CY, 3.0f, 4.0f, 226); // éclair de pétrification
    draw_level_pips(c, lvl);
}

static void draw_banque(Canvas *c, const struct Turret *t)
{
    int lvl = t->lvl;
    short body = lvl ? 130 : 94, top = lvl ? 172 : 130, edge = 58, trim = lvl ? 220 : 136;
    // coffre-fort
    for (int y = 4; y <= 15; y++)
        for (int x = 3; x <= 14; x++)
        {
            bool corner = (x == 3 || x == 14) && (y == 4 || y == 15);
            if (corner)
                continue;
            short color = body;
            if (y <= 5)
                color = top;
            if (x == 3 || x == 14 || y == 15)
                color = edge;
            canvas_set(c, x, y, color);
        }
    canvas_set(c, 4, 6, trim);
    canvas_set(c, 13, 6, trim);
    canvas_set(c, 4, 14, trim);
    canvas_set(c, 13, 14, trim);
    // porte ronde avec cadran
    ring(c, 8.5f, 10.0f, 2.6f, 3.6f, 178);
    fill_circle(c, 8.5f, 10.0f, 2.6f, 136);
    float a = anim_clock * 0.8f;
    for (int i = 0; i < 3; i++)
    {
        float b = a + i * 2 * PI / 3;
        line(c, 8.5f, 10.0f, 8.5f + cosf(b) * 2.4f, 10.0f + sinf(b) * 2.4f, 94);
    }
    canvas_set(c, 8, 9, 220);
    // pièces qui jaillissent juste avant de rapporter de l'argent
    float p = t->compteur / t->reload_delay[lvl];
    if (p > 0.8f)
    {
        float rise = (p - 0.8f) / 0.2f;
        int y = 3 - (int)(rise * 3);
        canvas_set(c, 6, y, 226);
        canvas_set(c, 9, y - 1, 220);
        canvas_set(c, 12, y, 226);
    }
    draw_level_pips(c, lvl);
}

static void draw_turret(Canvas *c, const struct Turret *t)
{
    switch (t->type)
    {
    case Sniper: draw_sniper(c, t); break;
    case Gatling: draw_gatling(c, t); break;
    case Mortier: draw_mortier(c, t); break;
    case Inferno: draw_inferno(c, t); break;
    case Freezer: draw_freezer(c, t); break;
    case Petrificateur: draw_petrificateur(c, t); break;
    case Banque: draw_banque(c, t); break;
    }
}

void draw_terrain_cell(struct Cell cell)
{
    Canvas c;
    canvas_init(&c, CELL_PX_W, CELL_PX_H);
    draw_slot(&c, cell.selected);
    if (cell.hasTurret)
    {
        draw_turret(&c, &cell.turret);
        if (cell.selected)
        {
            // le cadre de sélection reste visible par-dessus la tourelle
            Canvas frame;
            canvas_init(&frame, CELL_PX_W, CELL_PX_H);
            draw_slot(&frame, true);
            for (int y = 0; y < c.h; y++)
                for (int x = 0; x < c.w; x++)
                    if (x <= 1 || y <= 1 || x >= c.w - 2 || y >= c.h - 2)
                        if (canvas_get(&frame, x, y) >= 221)
                            canvas_set(&c, x, y, canvas_get(&frame, x, y));
        }
    }
    int terminal_x = cell.x * (CELL_WIDTH + GAP) + 3;
    int terminal_y = cell.y * (CELL_HEIGHT + GAP / 2) + GRID_TOP;
    canvas_draw(&c, terminal_x, terminal_y, 236, false);
}

void draw_turret_preview(int term_x, int term_y, enum TurretType type, int lvl, short background)
{
    draw_turret_preview_scaled(term_x, term_y, type, lvl, background, 1);
}

void draw_turret_preview_scaled(int term_x, int term_y, enum TurretType type, int lvl, short background, int scale)
{
    if (scale < 1)
        scale = 1;
    // La tourelle est dessinée sur une toile de case (18x18) puis recadrée au centre (14x14)
    Canvas full, crop;
    canvas_init(&full, CELL_PX_W, CELL_PX_H);
    struct Turret t = getTurretStruct(type);
    t.lvl = lvl;
    t.in_range = true;
    t.compteur = t.reload_delay[lvl] * 0.9f;
    t.last_shot_dx = 0.7f;
    t.last_shot_dy = -0.7f;
    draw_turret(&full, &t);
    // recadrage au centre (14 x 14 pixels), agrandi `scale` fois (chaque pixel devient un carré de scale x scale)
    canvas_init(&crop, 14 * scale, 14 * scale);
    for (int y = 0; y < 14 * scale; y++)
        for (int x = 0; x < 14 * scale; x++)
            canvas_set(&crop, x, y, canvas_get(&full, x / scale + 2, y / scale + 2));
    canvas_draw(&crop, term_x, term_y, background, false);
}

/*
#############
## Ennemis ##
#############
Sprites en pixel art, une lettre = une couleur (voir enemy_palette). 2 images d'animation de marche.
*/

static short enemy_palette(char ch)
{
    switch (ch)
    {
    case 'k': return 233; // noir
    case 'n': return 60;  // bleu ardoise (corps du Tux, visible sur le chemin sombre)
    case 'K': return 238; // gris foncé
    case 'w': return 255; // blanc
    case 'W': return 248; // gris clair
    case 'o': return 208; // orange
    case 'y': return 226; // jaune
    case 'Y': return 220; // or
    case 'r': return 196; // rouge
    case 'R': return 124; // rouge foncé
    case 'g': return 70;  // vert
    case 'G': return 22;  // vert foncé
    case 'h': return 28;  // vert moyen
    case 'c': return 51;  // cyan
    case 'C': return 37;  // cyan foncé
    case 'b': return 33;  // bleu
    case 'B': return 18;  // bleu foncé
    case 'l': return 117; // bleu clair
    case 'p': return 135; // violet
    case 'P': return 54;  // violet foncé
    case 'm': return 199; // rose
    case 't': return 43;  // turquoise
    case 'T': return 30;  // turquoise foncé
    default: return TRANSPARENT;
    }
}

typedef struct EnemySprite
{
    int rows;
    const char *frames[2][12];
} EnemySprite;

static const EnemySprite SPRITE_TUX = {8, {
    {"..nnnn..", ".nnnnnn.", ".nwrnrn.", ".nwwwwn.", "nnnoownn", "nnwwwwnn", ".nwwwwn.", ".oo..oo."},
    {"..nnnn..", ".nnnnnn.", ".nwrnrn.", ".nwwwwn.", "nnnoownn", "nnwwwwnn", ".nwwwwn.", "..oooo.."},
}};

static const EnemySprite SPRITE_HIGHTUX = {9, {
    {".Y.Y.Y..", ".YYYYY..", ".tttttt.", ".twrtrt.", ".twwwwt.", "tttoowtt", "ttwwwwtt", ".twwwwt.", ".oo..oo."},
    {".Y.Y.Y..", ".YYYYY..", ".tttttt.", ".twrtrt.", ".twwwwt.", "tttoowtt", "ttwwwwtt", ".twwwwt.", "..oooo.."},
}};

static const EnemySprite SPRITE_SPEED = {6, {
    {"..yyy.", ".yyyyy", "yykyky", "yyyyyy", ".yYyY.", "y....y"},
    {"..yyy.", ".yyyyy", "yykyky", "yyyyyy", ".yYyY.", ".y..y."},
}};

static const EnemySprite SPRITE_HYPERSPEED = {7, {
    {".....yyy.", "W...yyyyy", "...yylyly", "WW.yyyyyy", "...yyyyyy", "W...yYyY.", "....y...y"},
    {".....yyy.", "...Wyyyyy", "W..yylyly", ".W.yyyyyy", "W..yyyyyy", "....yYyY.", ".....y.y."},
}};

static const EnemySprite SPRITE_SPIDER = {7, {
    {"p..PPPP..p", ".p.PppP.p.", "..PpppppP.", "ppprpprppp", "..PppppP..", ".p.PppP.p.", "p........p"},
    {".p.PPPP.p.", "p..PppP..p", "..PpppppP.", "ppprpprppp", "..PppppP..", "p..PppP..p", ".p......p."},
}};

static const EnemySprite SPRITE_SLIME = {11, {
    {"...Y.YY.Y...", "...YYYYYY...", "..bbbbbbbb..", ".bbblbbbbbb.", ".bblbbbbbbb.", "bbbwkbbbwkbb", "bbbkkbbbkkbb", "bbbbbbbbbbbb", "bbbbmmmmbbbb", ".bbbbbbbbbb.", "..BBBBBBBB.."},
    {"...Y.YY.Y...", "...YYYYYY...", "...bbbbbb...", "..bblbbbbb..", ".bblbbbbbbb.", ".bbwkbbbwkb.", "bbbkkbbbkkbb", "bbbbbbbbbbbb", "bbbbmmmmbbbb", "bbbbbbbbbbbb", ".BBBBBBBBBB."},
}};

static const EnemySprite SPRITE_TANK = {8, {
    {".....hhhh.....", "....hhrhhkkkkk", "....hhhhh.....", ".hhhhhhhhhhhh.", "hhhggggggggghh", "hGGGGGGGGGGGGh", "KkKkKkKkKkKkKK", ".KKKKKKKKKKKK."},
    {".....hhhh.....", "....hhrhhkkkkk", "....hhhhh.....", ".hhhhhhhhhhhh.", "hhhggggggggghh", "hGGGGGGGGGGGGh", "kKkKkKkKkKkKkK", ".KKKKKKKKKKKK."},
}};

static const EnemySprite *enemy_sprite(enum EnemyType type)
{
    switch (type)
    {
    case ENEMY_TUX: return &SPRITE_TUX;
    case ENEMY_HIGHTUX: return &SPRITE_HIGHTUX;
    case ENEMY_SPEED: return &SPRITE_SPEED;
    case ENEMY_HYPERSPEED: return &SPRITE_HYPERSPEED;
    case ENEMY_SPIDER: return &SPRITE_SPIDER;
    case ENEMY_SLIME_BOSS: return &SPRITE_SLIME;
    case ENEMY_TANK: return &SPRITE_TANK;
    default: return NULL; // le boss électrique est dessiné par programme
    }
}

// Boss électrique : orbe jaune parcouru d'éclairs, avec des étincelles
static void draw_boss_stun(Canvas *c, int oy)
{
    int frame = (int)(anim_clock * 10);
    fill_circle(c, 6.0f, oy + 5.0f, 5.0f, 178);
    fill_circle(c, 6.0f, oy + 5.0f, 4.2f, 220);
    fill_circle(c, 5.0f, oy + 4.0f, 1.5f, 229);
    // éclair en zigzag qui change à chaque image
    int x = 3 + hash3(frame, 1, 2) % 2;
    for (int y = oy + 2; y < oy + 9; y++)
    {
        canvas_set(c, x, y, 51);
        x += (hash3(frame, y, 3) % 3) - 1;
        x = CLAMP(x, 2, 9);
    }
    canvas_set(c, 4, oy + 4, 233);
    canvas_set(c, 8, oy + 4, 233);
    for (int i = 0; i < 4; i++)
    {
        unsigned int h = hash3(frame, i, 9);
        canvas_set(c, h % 12, oy + (h >> 8) % 10, 195);
    }
}

void draw_enemy(const struct Enemy *enemy, Grid grid)
{
    (void)grid; // la position vient de grid_x / grid_y
    const EnemySprite *sprite = enemy_sprite(enemy->type);
    int w = sprite ? (int)strlen(sprite->frames[0][0]) : 12;
    int h = sprite ? sprite->rows : 10;
    bool show_bar = enemy->hp < enemy->maxHP || enemy->is_boss;

    // Position du centre de l'ennemi, en caractères (le centre d'une case est à x+9, y+4.5)
    int cx = (int)enemy->grid_x, cy = (int)enemy->grid_y;
    float center_x = cx * (CELL_WIDTH + GAP) + 3 + (CELL_WIDTH + GAP) * (enemy->grid_x - cx) + 1;
    float center_y = cy * (CELL_HEIGHT + GAP / 2) + GRID_TOP + (CELL_HEIGHT + GAP / 2) * (enemy->grid_y - cy) + 0.5f;

    // En pixels (2 par ligne) : on peut placer l'ennemi au demi-caractère près verticalement
    int bar_h = show_bar ? 2 : 0;
    int total_h = h + bar_h;
    int top_px = (int)lroundf(center_y * 2 - total_h / 2.0f);
    int term_x = (int)lroundf(center_x - w / 2.0f);
    int offset = top_px & 1; // si le haut tombe au milieu d'un caractère, on décale la toile d'un pixel
    int term_y = (top_px - offset) / 2;

    Canvas c;
    canvas_init(&c, w, total_h + offset);

    // Barre de vie : une ligne d'un pixel au-dessus du sprite
    if (show_bar)
    {
        float ratio = CLAMP(enemy->hp / enemy->maxHP, 0.0f, 1.0f);
        short color = ratio > 0.6f ? 46 : (ratio > 0.3f ? 226 : 196);
        int filled = (int)lroundf(ratio * w);
        for (int x = 0; x < w; x++)
            canvas_set(&c, x, offset, x < filled ? color : 238);
    }

    int oy = offset + bar_h;
    bool facing_left = enemy->next_cell.x < cx;
    int frame = ((int)(anim_clock * (4 + enemy->speed * 4))) % 2;
    if (sprite)
        blit_sprite(&c, sprite->frames[frame], h, 0, oy, facing_left, enemy_palette);
    else
        draw_boss_stun(&c, oy);

    // Effets des tourelles sur l'ennemi
    if (enemy->has_effect)
    {
        for (int y = oy; y < oy + h; y++)
            for (int x = 0; x < w; x++)
            {
                short p = canvas_get(&c, x, y);
                if (p == TRANSPARENT)
                    continue;
                if (enemy->effet == Stun && enemy->type != ENEMY_TANK)
                    canvas_set(&c, x, y, 243 + (p % 4)); // pétrifié : tout devient gris pierre
                else if (enemy->effet == Slow && enemy->type != ENEMY_TANK && (x + y) % 3 == 0)
                    canvas_set(&c, x, y, 159); // ralenti : givre
            }
        if (enemy->effet == Fire)
        {
            int f = (int)(anim_clock * 12);
            for (int i = 0; i < 3; i++)
            {
                unsigned int hsh = hash3(f, i, (int)(enemy->grid_x * 10));
                canvas_set(&c, hsh % w, oy + (hsh >> 8) % (h / 2 + 1), (hsh >> 4) % 2 ? 202 : 214);
            }
        }
    }

    canvas_draw(&c, term_x, term_y, 234, true); // 234 : couleur du sol du chemin
}

/*
###########################
## Superposition au pixel près ##
###########################
Pour dessiner par-dessus ce qui est déjà à l'écran (effets de tir, cercles de portée), on relit
le caractère existant dans le buffer, on en déduit ses deux pixels (haut et bas), on remplace
celui qui nous intéresse, puis on réécrit le caractère.
Coordonnées : px = colonne du terminal, py = 2 x ligne du terminal (+1 pour la moitié basse).
*/

void overlay_pixel(int px, int py, short color)
{
    int tx = px, ty = py / 2;
    char glyph[16];
    short fg, bg;
    if (py < 0 || !scr_peek(tx, ty, glyph, sizeof(glyph), &fg, &bg))
        return;
    short top, bottom;
    if (strcmp(glyph, "▀") == 0)
        top = fg, bottom = bg;
    else if (strcmp(glyph, "▄") == 0)
        top = bg, bottom = fg;
    else if (strcmp(glyph, "█") == 0)
        top = bottom = fg;
    else
        top = bottom = bg; // espace (ou autre caractère) : on garde la couleur de fond
    if (py % 2 == 0)
        top = color;
    else
        bottom = color;
    put_two_pixels(tx, ty, top, bottom);
}

// Centre d'une case (tourelle) en pixels
void cell_center_px(int x, int y, float *px, float *py)
{
    *px = x * (CELL_WIDTH + GAP) + 3 + 8.5f;
    *py = 2.0f * (y * (CELL_HEIGHT + GAP / 2) + GRID_TOP) + 9.0f;
}

// Centre d'un ennemi en pixels (même calcul que draw_enemy)
void enemy_center_px(const struct Enemy *e, float *px, float *py)
{
    int cx = (int)e->grid_x, cy = (int)e->grid_y;
    *px = cx * (CELL_WIDTH + GAP) + 3 + (CELL_WIDTH + GAP) * (e->grid_x - cx) + 1;
    *py = 2.0f * (cy * (CELL_HEIGHT + GAP / 2) + GRID_TOP + (CELL_HEIGHT + GAP / 2) * (e->grid_y - cy) + 0.5f);
}

/*
###################
## Cercle de portée ##
###################
Pointillés qui tournent lentement. Une case = 20 pixels dans les deux directions.
*/
void draw_range_ring(int cell_x, int cell_y, float range, short color)
{
    if (range <= 0 || range > 30)
        return; // pas de portée (Banque) ou portée infinie (Sniper niveau 3)
    float cx, cy;
    cell_center_px(cell_x, cell_y, &cx, &cy);
    float r = range * (CELL_WIDTH + GAP);
    int steps = (int)(2 * PI * r * 1.5f) + 8;
    int last_x = -999, last_y = -999;
    for (int i = 0; i < steps; i++)
    {
        float a = 2 * PI * i / steps;
        float along = a * r; // distance parcourue sur le cercle, en pixels
        if (((int)(along / 3 + anim_clock * 6)) % 2 != 0)
            continue; // trous des pointillés (qui avancent avec le temps)
        int x = (int)lroundf(cx + r * cosf(a));
        int y = (int)lroundf(cy + r * sinf(a));
        if (x == last_x && y == last_y)
            continue;
        last_x = x;
        last_y = y;
        if (y < 2 * GRID_TOP - 2)
            continue; // pas sur le bandeau d'infos
        overlay_pixel(x, y, color);
    }
}

/*
##########
## Chemin ##
##########
Le chemin est un "bus de données" : sol sombre légèrement texturé, bordures fines, et une piste
au centre sur laquelle circulent des impulsions lumineuses, du départ vers la sortie.
La case dessine sa zone et l'espace qui la précède (20 x 11 caractères = 20 x 22 pixels).
*/

#define PATH_GROUND 234
#define PATH_TEXTURE 235
#define PATH_BORDER 240
#define BOARD_BG 233
#define TRACE 23
#define PULSE 43
#define PULSE_TAIL 30

static bool is_path(Grid grid, int x, int y)
{
    return x >= 0 && y >= 0 && x < grid.width && y < grid.height && grid.cells[x][y].type == CHEMIN;
}

void draw_path_cell(struct Cell cell, Grid grid)
{
    Canvas c;
    canvas_init(&c, CELL_WIDTH + 2, (CELL_HEIGHT + 2) * 2);
    int W = c.w, H = c.h;
    bool up = is_path(grid, cell.x, cell.y - 1), down = is_path(grid, cell.x, cell.y + 1);
    bool left = is_path(grid, cell.x - 1, cell.y) || cell.x == 0;                  // le départ est ouvert à gauche
    bool right = is_path(grid, cell.x + 1, cell.y) || cell.x == grid.width - 1;   // la sortie est ouverte à droite

    // Sol, avec une texture alignée sur tout le plateau (elle se raccorde d'une case à l'autre)
    int gx0 = cell.x * (CELL_WIDTH + GAP), gy0 = cell.y * (CELL_HEIGHT + GAP / 2) * 2;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            canvas_set(&c, x, y, ((gx0 + x) % 4 == 1 && (gy0 + y) % 4 == 1) ? PATH_TEXTURE : PATH_GROUND);

    // Bordures sur les côtés sans chemin
    for (int y = 0; y < H; y++)
    {
        if (!left)
            canvas_set(&c, 0, y, PATH_BORDER);
        if (!right)
            canvas_set(&c, W - 1, y, PATH_BORDER);
    }
    for (int x = 0; x < W; x++)
    {
        if (!up)
        {
            canvas_set(&c, x, 0, BOARD_BG);
            canvas_set(&c, x, 1, PATH_BORDER);
        }
        if (!down)
        {
            canvas_set(&c, x, H - 1, BOARD_BG);
            canvas_set(&c, x, H - 2, PATH_BORDER);
        }
    }
    // Coins intérieurs (le chemin tourne) : petit point de bordure
    if (up && left && !is_path(grid, cell.x - 1, cell.y - 1) && cell.x > 0)
        canvas_set(&c, 0, 0, PATH_BORDER), canvas_set(&c, 0, 1, PATH_BORDER);
    if (up && right && !is_path(grid, cell.x + 1, cell.y - 1) && cell.x < grid.width - 1)
        canvas_set(&c, W - 1, 0, PATH_BORDER), canvas_set(&c, W - 1, 1, PATH_BORDER);
    if (down && left && !is_path(grid, cell.x - 1, cell.y + 1) && cell.x > 0)
        canvas_set(&c, 0, H - 1, PATH_BORDER), canvas_set(&c, 0, H - 2, PATH_BORDER);
    if (down && right && !is_path(grid, cell.x + 1, cell.y + 1) && cell.x < grid.width - 1)
        canvas_set(&c, W - 1, H - 1, PATH_BORDER), canvas_set(&c, W - 1, H - 2, PATH_BORDER);

    // Piste centrale : de l'entrée au centre, puis du centre à la sortie
    float mx = 10, my = 11; // centre de la case dans la toile
    float sides[4][2] = {{0, my}, {W - 1, my}, {mx, 0}, {mx, H - 1}}; // gauche, droite, haut, bas
    int entry = -1, exit_side = -1;
    int nx[4] = {cell.x - 1, cell.x + 1, cell.x, cell.x}, ny[4] = {cell.y, cell.y, cell.y - 1, cell.y + 1};
    for (int s = 0; s < 4; s++)
    {
        if (!is_path(grid, nx[s], ny[s]))
            continue;
        int idx = grid.cells[nx[s]][ny[s]].index;
        if (idx == cell.index - 1)
            entry = s;
        else if (idx == cell.index + 1)
            exit_side = s;
    }
    if (entry < 0 && cell.x == 0)
        entry = 0; // départ : on arrive par la gauche
    if (exit_side < 0 && cell.x == grid.width - 1)
        exit_side = 1; // sortie : on part par la droite
    int segments[2] = {entry, exit_side};
    for (int k = 0; k < 2; k++)
    {
        int s = segments[k];
        if (s < 0)
            continue;
        float sx = sides[s][0], sy = sides[s][1];
        float len = sqrtf((sx - mx) * (sx - mx) + (sy - my) * (sy - my));
        int steps = (int)(len * 2) + 1;
        for (int i = 0; i <= steps; i++)
        {
            float t = (float)i / steps;
            // distance depuis l'entrée de la case : de 0 à 10 sur le premier segment, de 10 à 20 sur le second
            float local = k == 0 ? (1 - t) * 10 : 10 + t * 10;
            float d = cell.index * 20 + local - anim_clock * 14; // les impulsions avancent avec le temps
            float m = fmodf(d, 40);
            if (m < 0)
                m += 40;
            short color = m > 37.5f ? PULSE : (m > 33 ? PULSE_TAIL : TRACE);
            int x = (int)lroundf(mx + (sx - mx) * t), y = (int)lroundf(my + (sy - my) * t);
            canvas_set(&c, x, y, color);
            // piste de 2 pixels de large
            if (s <= 1)
                canvas_set(&c, x, y + 1, color == PULSE ? PULSE : TRACE);
            else
                canvas_set(&c, x + 1, y, color == PULSE ? PULSE : TRACE);
        }
    }
    int tx = cell.x * (CELL_WIDTH + GAP) + 3 - 1;
    int ty = cell.y * (CELL_HEIGHT + GAP / 2) + GRID_TOP - 1;
    canvas_draw(&c, tx, ty, BOARD_BG, false);
}
