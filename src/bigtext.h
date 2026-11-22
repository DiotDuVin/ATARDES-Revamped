#pragma once

/*
##############################
GRANDE POLICE (texte lisible)
##############################

Le jeu demande un terminal d'au moins 232x74 caractères, donc une toute petite police :
le texte normal devient vite illisible.

Ce module dessine le texte avec une police en pixels de 3x5, où chaque "pixel" est un
demi-caractère (▀ ▄ █). Une lettre occupe 4 colonnes et 3 lignes du terminal :
elle apparaît environ 3 fois plus grande qu'un caractère normal.

La police ne contient que des majuscules (les minuscules et les accents sont convertis).
Les codes couleur ANSI (COLOR_..., RESET) peuvent être mis directement dans le texte.

Si le jeu est lancé avec --petit-texte, ces fonctions affichent du texte normal :
le reste du code n'a pas à s'en soucier.
*/

#include <stdbool.h>

// Vrai par défaut ; faux avec l'option --petit-texte
extern bool big_text_mode;

// Nombre de lignes du terminal occupées par une ligne de texte (3 en grande police, 1 sinon)
int ui_text_rows(void);

// Largeur (en colonnes du terminal) qu'occuperait le texte
int ui_text_width(const char *text);

// Dessine le texte avec son coin haut-gauche en (x, y). Renvoie la largeur dessinée.
int ui_text(int x, int y, const char *text);

// Dessine le texte centré horizontalement autour de center_x. Renvoie la largeur.
int ui_text_centered(int center_x, int y, const char *text);

// Dessine le texte en le coupant entre les mots pour ne pas dépasser max_width colonnes.
// line_gap : lignes vides ajoutées entre deux lignes de texte.
// Renvoie le nombre de lignes du terminal utilisées.
int ui_text_wrapped(int x, int y, int max_width, int line_gap, const char *text);

// Nombre de lignes du terminal qu'occuperait ui_text_wrapped (sans rien dessiner)
int ui_text_wrapped_rows(int max_width, int line_gap, const char *text);

// Découpe le texte (sans codes couleur) en lignes de max_width colonnes au plus.
// Pour chaque ligne i : début = text + starts[i], longueur = lens[i] octets. Renvoie le nombre de lignes.
int ui_text_split(int max_width, const char *text, int *starts, int *lens, int max_lines);

// Dessine les `len` premiers octets de `text`
int ui_text_span(int x, int y, const char *text, int len);

// Largeur du texte affiché en taille normale (1 colonne par caractère, codes couleur ignorés)
int ui_text_width_raw(const char *text);

// Comme ui_text, mais si le texte dépasse max_width colonnes en grande police,
// il est écrit en taille normale (il ne sort jamais du cadre prévu)
int ui_text_fit(int x, int y, int max_width, const char *text);
