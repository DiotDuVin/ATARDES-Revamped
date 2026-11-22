#pragma once

#define LOGO_ROWS 9
#define LOGO_WIDTH 88

// Logo "ATARDES" en grandes lettres et sous-titre (utilisés par l'intro et le menu)
extern const char *ATARDES_LOGO[LOGO_ROWS];
extern const char *ATARDES_SUBTITLE;

// Position du logo à l'écran (la même dans l'intro et le menu, pour une transition sans saut)
void logo_position(int term_width, int term_height, int *x, int *y);
void draw_logo(int x0, int y0, int color);
void draw_subtitle(int term_width, int term_height);

// Animation d'intro (environ 3 secondes, n'importe quelle touche la passe)
void run_intro(int term_width, int term_height);
