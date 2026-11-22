#include "effects.h"
#include "pixelart.h"

#include <string.h>

enum FxType
{
    FX_BOLT,      // Sniper : éclair + impact
    FX_TRACER,    // Gatling : traçante
    FX_EXPLOSION, // Mortier : boule de feu, onde de choc, débris, fumée
    FX_FLAME,     // Inferno : anneau de flammes
    FX_FROST,     // Freezer : onde de givre
    FX_STUN,      // Pétrificateur : rayon + cercle de pierre
    FX_COINS,     // Banque : pièces et gain
};

typedef struct Fx
{
    enum FxType type;
    float x0, y0;  // origine (pixels)
    float x1, y1;  // cible (pixels)
    float radius;  // rayon (pixels)
    float t;       // temps écoulé
    float duration;
    int seed;      // pour varier les étincelles d'un effet à l'autre
    int amount;    // gain affiché (Banque)
} Fx;

#define MAX_FX 160
static Fx effects[MAX_FX];
static int fx_count = 0;
static int fx_seed = 0;

static Fx *fx_new(enum FxType type, float duration)
{
    if (fx_count >= MAX_FX)
        return NULL; // trop d'effets en même temps (vitesse x16) : on ignore les nouveaux
    Fx *fx = &effects[fx_count++];
    memset(fx, 0, sizeof(*fx));
    fx->type = type;
    fx->duration = duration;
    fx->seed = fx_seed++;
    return fx;
}

/*
## Déclenchement ##
*/

void fx_sniper_shot(int x, int y, const struct Enemy *target)
{
    Fx *fx = fx_new(FX_BOLT, 0.22f);
    if (!fx)
        return;
    cell_center_px(x, y, &fx->x0, &fx->y0);
    enemy_center_px(target, &fx->x1, &fx->y1);
}

void fx_gatling_shot(int x, int y, const struct Enemy *target)
{
    Fx *fx = fx_new(FX_TRACER, 0.07f);
    if (!fx)
        return;
    cell_center_px(x, y, &fx->x0, &fx->y0);
    enemy_center_px(target, &fx->x1, &fx->y1);
    // la traçante part du bout du canon, pas du centre de la tourelle
    float dx = fx->x1 - fx->x0, dy = fx->y1 - fx->y0, len = sqrtf(dx * dx + dy * dy);
    if (len > 7)
    {
        fx->x0 += dx / len * 7;
        fx->y0 += dy / len * 7;
    }
}

void fx_mortar_explosion(const struct Enemy *target, float splash_radius)
{
    Fx *fx = fx_new(FX_EXPLOSION, 0.7f);
    if (!fx)
        return;
    enemy_center_px(target, &fx->x1, &fx->y1);
    fx->radius = splash_radius * (CELL_WIDTH + GAP);
}

void fx_flame_ring(int x, int y, float range)
{
    Fx *fx = fx_new(FX_FLAME, 0.45f);
    if (!fx)
        return;
    cell_center_px(x, y, &fx->x0, &fx->y0);
    fx->radius = range * (CELL_WIDTH + GAP);
}

void fx_frost_wave(int x, int y, float range)
{
    Fx *fx = fx_new(FX_FROST, 0.5f);
    if (!fx)
        return;
    cell_center_px(x, y, &fx->x0, &fx->y0);
    fx->radius = range * (CELL_WIDTH + GAP);
}

void fx_stun_beam(int x, int y, const struct Enemy *target)
{
    Fx *fx = fx_new(FX_STUN, 0.4f);
    if (!fx)
        return;
    cell_center_px(x, y, &fx->x0, &fx->y0);
    enemy_center_px(target, &fx->x1, &fx->y1);
}

void fx_coins(int x, int y, int amount)
{
    Fx *fx = fx_new(FX_COINS, 0.9f);
    if (!fx)
        return;
    cell_center_px(x, y, &fx->x0, &fx->y0);
    fx->amount = amount;
}

void fx_update(float dt)
{
    int kept = 0;
    for (int i = 0; i < fx_count; i++)
    {
        effects[i].t += dt;
        if (effects[i].t < effects[i].duration)
            effects[kept++] = effects[i];
    }
    fx_count = kept;
}

void fx_clear(void)
{
    fx_count = 0;
}

/*
## Dessin ##
*/

static unsigned int fx_hash(int a, int b, int c)
{
    unsigned int h = (unsigned int)a * 73856093u ^ (unsigned int)b * 19349663u ^ (unsigned int)c * 83492791u;
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    h ^= h >> 15;
    return h;
}

static void px(float x, float y, short color)
{
    if (y < 2 * GRID_TOP - 2)
        return; // jamais sur le bandeau d'infos
    overlay_pixel((int)lroundf(x), (int)lroundf(y), color);
}

static void fx_line(float x0, float y0, float x1, float y1, short color)
{
    float len = sqrtf((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0));
    int steps = (int)(len * 1.5f) + 1;
    int lx = -999, ly = -999;
    for (int i = 0; i <= steps; i++)
    {
        float t = (float)i / steps;
        int x = (int)lroundf(x0 + (x1 - x0) * t), y = (int)lroundf(y0 + (y1 - y0) * t);
        if (x != lx || y != ly)
            px(x, y, color);
        lx = x, ly = y;
    }
}

// Anneau d'épaisseur `width`, la couleur de chaque pixel est choisie par `pick` (bruit, dégradé...)
static void fx_ring(float cx, float cy, float r, float width, const Fx *fx, short (*pick)(const Fx *, int, int, float))
{
    if (r <= 0)
        return;
    int steps = (int)(2 * PI * (r + width) * 1.6f) + 8;
    for (float w = 0; w < width; w += 0.8f)
    {
        float rr = r - w;
        if (rr <= 0)
            break;
        for (int i = 0; i < steps; i++)
        {
            float a = 2 * PI * i / steps;
            int x = (int)lroundf(cx + rr * cosf(a)), y = (int)lroundf(cy + rr * sinf(a));
            short color = pick(fx, x, y, w / width);
            if (color >= 0)
                px(x, y, color);
        }
    }
}

static float ease_out(float p)
{
    p = p < 0 ? 0 : (p > 1 ? 1 : p);
    return 1 - (1 - p) * (1 - p);
}

static short pick_flame(const Fx *fx, int x, int y, float depth)
{
    static const short heat[] = {160, 196, 202, 208, 214, 220, 226};
    float p = fx->t / fx->duration;
    unsigned int h = fx_hash(x, y, (int)(fx->t * 30) + fx->seed);
    if (h % 100 < (unsigned)(30 + 50 * p))
        return -1; // flammes irrégulières, de plus en plus clairsemées
    int idx = (int)((1 - depth) * 4 + (1 - p) * 3 + (h % 3)) - 1;
    return heat[idx < 0 ? 0 : (idx > 6 ? 6 : idx)];
}

static short pick_frost(const Fx *fx, int x, int y, float depth)
{
    float p = fx->t / fx->duration;
    unsigned int h = fx_hash(x, y, fx->seed);
    if (h % 100 < (unsigned)(20 + 60 * p))
        return -1;
    if (h % 17 == 0)
        return 231; // éclats brillants
    return depth < 0.4f ? 195 : (p < 0.5f ? 159 : 117);
}

static void draw_bolt(const Fx *fx)
{
    float p = fx->t / fx->duration;
    float dx = fx->x1 - fx->x0, dy = fx->y1 - fx->y0, len = sqrtf(dx * dx + dy * dy);
    if (len < 1)
        return;
    float nx = -dy / len, ny = dx / len; // perpendiculaire
    if (p < 0.45f)
    {
        // l'éclair : halo bleu électrique de chaque côté, cœur blanc
        if (p < 0.3f)
        {
            fx_line(fx->x0 + nx, fx->y0 + ny, fx->x1 + nx, fx->y1 + ny, 45);
            fx_line(fx->x0 - nx, fx->y0 - ny, fx->x1 - nx, fx->y1 - ny, 45);
        }
        fx_line(fx->x0 + dx / len * 6, fx->y0 + dy / len * 6, fx->x1, fx->y1, p < 0.2f ? 231 : 195);
    }
    // impact : anneau qui grandit et étincelles
    float r = 1 + 5 * ease_out(p);
    for (int i = 0; i < 8; i++)
    {
        float a = (fx_hash(fx->seed, i, 1) % 628) / 100.0f;
        float d = 1.5f + (2 + fx_hash(fx->seed, i, 2) % 4) * ease_out(p);
        short color = p < 0.4f ? 229 : (p < 0.7f ? 220 : 208);
        px(fx->x1 + cosf(a) * d, fx->y1 + sinf(a) * d, color);
    }
    if (p < 0.5f)
    {
        px(fx->x1, fx->y1, 231);
        int ring_steps = 20;
        for (int i = 0; i < ring_steps; i++)
        {
            float a = 2 * PI * i / ring_steps;
            px(fx->x1 + cosf(a) * r, fx->y1 + sinf(a) * r, p < 0.25f ? 230 : 214);
        }
    }
}

static void draw_tracer(const Fx *fx)
{
    fx_line(fx->x0, fx->y0, fx->x1, fx->y1, fx->seed % 2 ? 214 : 208);
    px(fx->x1, fx->y1, 226);
}

static void draw_explosion(const Fx *fx)
{
    float p = fx->t / fx->duration;
    float rmax = fx->radius;
    float cx = fx->x1, cy = fx->y1;
    int frame = (int)(fx->t * 24);

    // 1. onde de choc : un cercle clair qui part vite vers l'extérieur
    if (p < 0.35f)
    {
        float r = rmax * 1.3f * ease_out(p / 0.35f);
        int steps = (int)(2 * PI * r * 1.5f) + 8;
        for (int i = 0; i < steps; i++)
        {
            float a = 2 * PI * i / steps;
            if (fx_hash(i, fx->seed, frame) % 3 == 0)
                continue;
            px(cx + r * cosf(a), cy + r * sinf(a), p < 0.15f ? 230 : 223);
        }
    }

    // 2. boule de feu (puis fumée) : disque bruité dont la couleur refroidit du centre vers le bord et avec le temps
    float fire_r = rmax * (0.35f + 0.65f * ease_out(MIN(1.0f, p * 2.5f)));
    static const short fire[] = {231, 230, 229, 226, 220, 214, 208, 202, 196, 160, 124};
    static const short smoke[] = {250, 247, 244, 241, 239};
    for (int y = (int)(cy - fire_r); y <= (int)(cy + fire_r); y++)
        for (int x = (int)(cx - fire_r); x <= (int)(cx + fire_r); x++)
        {
            float d = sqrtf((x - cx) * (x - cx) + (y - cy) * (y - cy)) / fire_r;
            if (d > 1)
                continue;
            unsigned int h = fx_hash(x, y, frame + fx->seed * 7);
            float noise = (h % 100) / 100.0f;
            if (p > 0.55f)
            {
                // fumée : de plus en plus clairsemée
                if (noise < (p - 0.55f) * 2.2f + d * 0.5f)
                    continue;
                px(x, y, smoke[(int)(d * 3 + (p - 0.55f) * 4) > 4 ? 4 : (int)(d * 3 + (p - 0.55f) * 4)]);
            }
            else
            {
                if (d > 0.75f && noise < 0.4f)
                    continue; // bord irrégulier
                int idx = (int)(d * 6 + p * 9 + noise * 2);
                px(x, y, fire[idx > 10 ? 10 : idx]);
            }
        }

    // 3. débris projetés
    if (p < 0.8f)
        for (int i = 0; i < 12; i++)
        {
            float a = (fx_hash(fx->seed, i, 5) % 628) / 100.0f;
            float speed = 0.8f + (fx_hash(fx->seed, i, 6) % 60) / 100.0f;
            float d = rmax * 1.4f * speed * ease_out(p);
            px(cx + cosf(a) * d, cy + sinf(a) * d + p * p * 6, i % 3 ? 208 : 94); // les débris retombent un peu
        }
}

static void draw_flame(const Fx *fx)
{
    float p = fx->t / fx->duration;
    float r = 5 + (fx->radius - 5) * ease_out(p);
    fx_ring(fx->x0, fx->y0, r, 3.5f, fx, pick_flame);
}

static void draw_frost(const Fx *fx)
{
    float p = fx->t / fx->duration;
    float r = 4 + (fx->radius - 4) * ease_out(p);
    fx_ring(fx->x0, fx->y0, r, 2.5f, fx, pick_frost);
}

static void draw_stun(const Fx *fx)
{
    float p = fx->t / fx->duration;
    // rayon en zigzag ambré, qui s'éteint vite
    if (p < 0.5f)
    {
        float dx = fx->x1 - fx->x0, dy = fx->y1 - fx->y0, len = sqrtf(dx * dx + dy * dy);
        if (len > 1)
        {
            float nx = -dy / len, ny = dx / len;
            int seg = (int)(len / 4) + 1;
            float lx = fx->x0, ly = fx->y0;
            for (int i = 1; i <= seg; i++)
            {
                float t = (float)i / seg;
                float off = i == seg ? 0 : ((int)(fx_hash(fx->seed, i, (int)(fx->t * 20)) % 5) - 2);
                float x = fx->x0 + dx * t + nx * off, y = fx->y0 + dy * t + ny * off;
                fx_line(lx, ly, x, y, p < 0.25f ? 229 : 214);
                lx = x, ly = y;
            }
        }
    }
    // cercle de pierre qui se referme sur la cible
    float r = 7 * (1 - ease_out(p)) + 2;
    int steps = 24;
    for (int i = 0; i < steps; i++)
    {
        float a = 2 * PI * i / steps + p * 2;
        if (i % 3 == 0)
            continue;
        px(fx->x1 + cosf(a) * r, fx->y1 + sinf(a) * r, i % 2 ? 82 : 245);
    }
}

static void draw_coins(const Fx *fx)
{
    float p = fx->t / fx->duration;
    // trois pièces qui montent et s'écartent
    for (int i = 0; i < 3; i++)
    {
        float x = fx->x0 + (i - 1) * (2 + 4 * p);
        float y = fx->y0 - 8 - 10 * ease_out(p) + (i == 1 ? -2 : 0);
        short color = ((int)(fx->t * 12) + i) % 2 ? 226 : 220;
        px(x, y, color);
        px(x, y + 1, 178);
    }
    // gain écrit au-dessus du coffre
    if (p < 0.85f)
    {
        int tx = (int)fx->x0 - 2, ty = (int)((fx->y0 - 14 - 8 * ease_out(p)) / 2);
        if (ty >= GRID_TOP)
        {
            move_to(tx, ty);
            scr_printf(COLOR_STANDARD_BG COLOR_YELLOW "+%d€" RESET, fx->amount);
        }
    }
}

void fx_draw(void)
{
    for (int i = 0; i < fx_count; i++)
    {
        switch (effects[i].type)
        {
        case FX_BOLT: draw_bolt(&effects[i]); break;
        case FX_TRACER: draw_tracer(&effects[i]); break;
        case FX_EXPLOSION: draw_explosion(&effects[i]); break;
        case FX_FLAME: draw_flame(&effects[i]); break;
        case FX_FROST: draw_frost(&effects[i]); break;
        case FX_STUN: draw_stun(&effects[i]); break;
        case FX_COINS: draw_coins(&effects[i]); break;
        }
    }
}
