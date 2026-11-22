#include "shared.h"
#include "tutorial.h"
#include "intro.h"

int grid_top = 2; // modifié au démarrage selon la taille du texte

int msleep(long ms) // Fonction de repos (temps pendant lequel le terminal ne lis pas la suite du programme), ici en millisecondes
{
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000;
    return nanosleep(&ts, &ts);
}

// Pause précise en secondes (msleep arrondit à la milliseconde)
void nsleep_seconds(double seconds)
{
    if (seconds <= 0)
        return;
    struct timespec ts;
    ts.tv_sec = (time_t)seconds;
    ts.tv_nsec = (long)((seconds - ts.tv_sec) * 1e9);
    nanosleep(&ts, NULL);
}

void updateLabels(Labels *labels, float dt) // Mets a jour les labels (l'argent drop par les monstres)
{
    for (int i = 0; i < labels->count; i++)
    {
        labels->labels[i].counter += dt;
    }
    int right = 0;
    int left = -1;

    // On parcourt le tableau des labels, on supprime les labels qui ont
    // dépassé leur durée de vie, et on décale les labels restants
    // vers la gauche.
    while (right < labels->count)
    {
        if (labels->labels[right].counter < labels->labels[right].duration)
        {
            // Si le label est encore valide et que celui de gauche est un un trou, on déplace celui de droite vers la gauche.
            if (left != -1)
            {
                // On copie le label de droite dans le label de gauche,
                // car on va libérer la mémoire du label de gauche.
                assert(left >= 0);
                labels->labels[left] = labels->labels[right];
                left++;
            }
        }
        else if (left == -1)
        {
            // Si c'est le premier label invalide, on le note comme étant
            // le premier label que l'on va supprimer.
            left = right;
        }

        right++;
    }
    if (left != -1)
    {
        // On met a jour le compteur du nombre de labels,
        // car on a supprimé des labels.
        labels->count -= right - left;
    }
    if (labels->count < 0)
        labels->count = 0;
};

void drawLabels(Labels labels) // affiche tous les labels
{
    for (int i = 0; i < labels.count; i++)
    {
        move_to(labels.labels[i].x, labels.labels[i].y);
        scr_printf(COLOR_STANDARD_BG COLOR_YELLOW "%d" RESET, labels.labels[i].text);
    }
};

void freeLabels(Labels labels) // nettoie le tableau des labels
{
    scr_printf(COLOR_GRAY " $ " RESET "Freeing %s%d%s labels:\t", COLOR_YELLOW, labels.count, RESET);

    free(labels.labels);

    scr_printf("%s Done %s\n", COLOR_GREEN, RESET);
};

/*
#############
##    IO   ##
#############
*/

// Lecture des inputs de l'utilisateur
char get_key_press()
{
    int k = 0;
    // Récupère le nombre de characters en attente d’être lus
    ioctl(STDIN_FILENO, FIONREAD, &k);

    if (k > 0)
        return getchar();
    else
        return 0;
}

// Les fonctions suivantes sont claires de par leurs noms (les "\033[" indiquent des actions sur l'affichage (voir README et les codes ANSI))
void move_to(int x, int y)
{
    scr_printf("\033[%d;%dH", y, x);
}

void clear_screen()
{
    scr_printf("\033[2J");
}

void clear_line()
{
    scr_printf("\033[2K");
}

void hide_cursor()
{
    scr_printf("\033[?25l");
}

void show_cursor()
{
    scr_printf("\033[?25h");
}

void printCritical(char *errorMessage)
{
    scr_shutdown(); // le message doit s'afficher directement dans le terminal, pas dans le buffer
    printf("%s%sCRITICAL%s: %s %s %s\n", COLOR_RED, UNDERLINE, UNDERLINE_RST, BOLD, errorMessage, RESET);
}

// Récupération de la taille du terminale
void get_terminal_size(int *width, int *height)
{
    struct winsize w = {0};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) != 0)
        w.ws_col = w.ws_row = 0; // pas un terminal (sortie redirigée)

    *width = w.ws_col;
    *height = w.ws_row;
}

// Centre horizontal / vertical du terminal (pour centrer les fenêtres)
int width_center()
{
    int w, h;
    get_terminal_size(&w, &h);
    return w / 2;
}

int height_center()
{
    int w, h;
    get_terminal_size(&w, &h);
    return h / 2;
}

// Oblige l'utilisateur à se mettre dans une bonne configuration de terminal pour jouer
void checkTerminalSize(int *width, int *height)
{
    get_terminal_size(width, height);

    while ((*width < MIN_TERMINAL_WIDTH || *height < MIN_TERMINAL_HEIGHT) || (*width > MAX_TERMINAL_WIDTH || *height > MAX_TERMINAL_HEIGHT))
    {
        bool too_small = *width < MIN_TERMINAL_WIDTH || *height < MIN_TERMINAL_HEIGHT;
        char text[512];
        if (too_small)
            snprintf(text, sizeof(text),
                     COLOR_YELLOW "Terminal trop petit" RESET "\n\n"
                     "Colonnes : %d / %d\nLignes : %d / %d\n\n"
                     "Dézoome avec Ctrl -" "\n" COLOR_GRAY "(ou Ctrl molette)" RESET "\n\n"
                     "Astuce : ./jouer.sh ouvre le jeu en plein écran à la bonne taille",
                     *width, MIN_TERMINAL_WIDTH, *height, MIN_TERMINAL_HEIGHT);
        else
            snprintf(text, sizeof(text),
                     COLOR_YELLOW "Terminal trop grand" RESET "\n\n"
                     "Colonnes : %d / %d max\nLignes : %d / %d max\n\n"
                     "Zoome avec Ctrl +",
                     *width, MAX_TERMINAL_WIDTH, *height, MAX_TERMINAL_HEIGHT);

        printf(RESET "\033[2J");
        ui_text_wrapped(3, 2, *width - 4, 1, text);
        printf(RESET);
        fflush(stdout);

        msleep(200);
        get_terminal_size(width, height);
    }
}

// Dessine les cercles de portée des tourelles
void drawRange(int term_width, int term_height, float range, float grid_x, float grid_y, bool fill)
{
    scr_printf(COLOR_STANDARD_BG);

    for (int y = 0; y < term_height; y++)
    {
        for (int x = 0; x < term_width; x++)
        {
            float current_grid_x = (x - 3.0f) / (CELL_WIDTH + GAP);
            float current_grid_y = (y - (float)GRID_TOP) / (CELL_HEIGHT + GAP / 2);

            float dx = grid_x - current_grid_x;
            float dy = grid_y - current_grid_y;
            float d = sqrt(dx * dx + dy * dy);

            if (d < range + 0.05 && (d > range - 0.06 || fill))
            {
                int term_x = x;
                int term_y = y;
                if (term_x > 0 && term_y > 0 && term_x < term_width && term_y < term_height)
                {
                    move_to(term_x, term_y);
                    scr_printf("•");
                }
            }
        }
    }
}

// Petit dessin du changement de vitesse dans le coin en haut à droite
void drawGameSpeed(int game_speed_anim, int term_width, int game_speed_anim_pause)
{
    char *sprite[6][7] = {{SPEED_CONTROL COLOR_TOWER_SLOT_BG "         ▄█   ",
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
                           "    ▀▀ ▄▄▀▀   "},
                          {SPEED_CONTROL_PAUSE COLOR_TOWER_SLOT_BG "  ▄▄▄    ▄▄▄  ",
                           "  ███    ███  ",
                           "  ███    ███  ",
                           "  ███    ███  ",
                           "  ███    ███  ",
                           "  ███    ███  ",
                           "  ▀▀▀    ▀▀▀  "}};
    if (game_speed_anim > 8)
    {
        game_speed_anim = 16;
    }

    if (game_speed_anim_pause == 1)
    {
        for (int i = 0; i < 7; i++)
        {
            move_to(term_width - (CELL_WIDTH * 2) + 7, GRID_TOP + 1 + i);
            scr_puts(sprite[5][i]);
        }
    }
    else
    {
        for (int i = 0; i < 7; i++)
        {
            move_to(term_width - (CELL_WIDTH * 2) + 7, GRID_TOP + 1 + i);
            scr_puts(sprite[(int)log2(game_speed_anim)][i]);
        }
    }
}

// Animation de début de jeu / tutoriel (il ne s'agit que de boucle qui font se déplacer le curseur afin que l'annimation se produise)

// Intro animée puis menu principal (Tutoriel / Jeu)
void anim_debut(int term_width, int term_height)
{
    run_intro(term_width, term_height); // voir intro.c

    char *spriteTuto[2][9] = {
        {COLOR_GRAY "▗▄▄▄▖▗▖ ▗▖▗▄▄▄▖▗▄▖ ▗▄▄▖ ▗▄▄▄▖▗▄▄▄▖▗▖        ",
         COLOR_GRAY "  █  ▐▌ ▐▌  █ ▐▌ ▐▌▐▌ ▐▌  █  ▐▌   ▐▌        ",
         COLOR_GRAY "  █  ▐▌ ▐▌  █ ▐▌ ▐▌▐▛▀▚▖  █  ▐▛▀▀▘▐▌        ",
         COLOR_GRAY "  █  ▝▚▄▞▘  █ ▝▚▄▞▘▐▌ ▐▌▗▄█▄▖▐▙▄▄▖▐▙▄▄▖     ",
         "                                            ",
         COLOR_GREEN "█▄      ▗▖▗▄▄▄▖▗▖ ▗▖                        ",
         COLOR_GREEN "███▄    ▐▌▐▌   ▐▌ ▐▌                        ",
         COLOR_GREEN "███▀    ▐▌▐▛▀▀▘▐▌ ▐▌                        ",
         COLOR_GREEN "█▀   ▗▄▄▞▘▐▙▄▄▖▝▚▄▞▘                        "},
        {COLOR_GREEN "█▄   ▗▄▄▄▖▗▖ ▗▖▗▄▄▄▖▗▄▖ ▗▄▄▖ ▗▄▄▄▖▗▄▄▄▖▗▖   ",
         COLOR_GREEN "███▄   █  ▐▌ ▐▌  █ ▐▌ ▐▌▐▌ ▐▌  █  ▐▌   ▐▌   ",
         COLOR_GREEN "███▀   █  ▐▌ ▐▌  █ ▐▌ ▐▌▐▛▀▚▖  █  ▐▛▀▀▘▐▌   ",
         COLOR_GREEN "█▀     █  ▝▚▄▞▘  █ ▝▚▄▞▘▐▌ ▐▌▗▄█▄▖▐▙▄▄▖▐▙▄▄▖",
         "                                            ",
         COLOR_GRAY "   ▗▖▗▄▄▄▖▗▖ ▗▖                             ",
         COLOR_GRAY "   ▐▌▐▌   ▐▌ ▐▌                             ",
         COLOR_GRAY "   ▐▌▐▛▀▀▘▐▌ ▐▌                             ",
         COLOR_GRAY "▗▄▄▞▘▐▙▄▄▖▝▚▄▞▘                             "}};

    int tutorial_variable = 1;

    while (true)
    {
        int c = get_key_press();
        if (c == 'A')
        {
            tutorial_variable = 1;
        }
        else if (c == 'B')
        {
            tutorial_variable = 0;
        }

        int logo_x, logo_y;
        logo_position(term_width, term_height, &logo_x, &logo_y);
        scr_printf(RESET);
        clear_screen();
        draw_logo(logo_x, logo_y, 42);
        draw_subtitle(term_width, term_height);
        for (int i = 0; i < 9; i++)
        {
            move_to(term_width / 2 - 22, logo_y + LOGO_ROWS + 7 + i);
            scr_puts(spriteTuto[tutorial_variable][i]);
        }
        scr_printf(RESET);
        ui_text_centered(term_width / 2, term_height - 1 - ui_text_rows(), COLOR_GRAY "↑ ↓ choisir   Entrée valider   Q quitter");
        scr_printf(RESET);
        scr_flush();
        if (c == 'q')
        {
            // free de tout automatique dans main.c/void cleanup()
            exit(EXIT_SUCCESS);
        }
        if (c == 10 && tutorial_variable == 0)
        {
            break;
        }
        if (c == 10 && tutorial_variable == 1)
        {
            enum TutorialResult result = run_tutorial(term_width, term_height); // voir tutorial.c
            if (result == TUTORIAL_QUIT)
                exit(EXIT_SUCCESS);
            if (result == TUTORIAL_DONE)
                break; // tutoriel terminé : on lance la partie
            // Échap : retour au menu
        }
        msleep(15); // inutile de vérifier le clavier plus souvent
    }

    scr_printf(RESET);
    clear_screen();
}