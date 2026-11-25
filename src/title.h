#pragma once

/*
############
ÉCRAN TITRE
############

Intro animée, puis menu principal : Continuer, Nouvelle partie, Tutoriel, Meilleurs scores, Quitter.
En bas de l'écran, une démo tourne en boucle avec le vrai moteur du jeu : des ennemis remontent
un bout de chemin et des tourelles leur tirent dessus.
*/

enum TitleChoice
{
    TITLE_NEW_GAME, // nouvelle partie
    TITLE_CONTINUE, // reprendre la partie de l'emplacement 0 (sauvée en quittant avec Q)
};

// Affiche l'intro puis le menu ; ne revient que pour lancer une partie (Quitter termine le programme)
enum TitleChoice title_screen(int term_width, int term_height);

// Ligne du logo dans l'écran titre (l'intro le place au même endroit, pour une transition sans saut)
int title_logo_y(int term_width, int term_height);
