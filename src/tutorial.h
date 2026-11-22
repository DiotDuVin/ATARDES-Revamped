#pragma once

enum TutorialResult
{
    TUTORIAL_DONE, // le joueur a passé la dernière page
    TUTORIAL_BACK, // le joueur a appuyé sur Échap
    TUTORIAL_QUIT, // le joueur a appuyé sur Q
};

// Affiche le tutoriel (plusieurs pages, navigation au clavier).
enum TutorialResult run_tutorial(int term_width, int term_height);
