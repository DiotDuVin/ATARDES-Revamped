#include "shared.h"

#include "grid.h"
#include "enemies.h"
#include "tower.h"
#include "save.h"
#include "tutorial.h"

#include <string.h>
#include <sys/select.h>
#include <unistd.h>

/*
#################
ÉTAT DU PROGRAMME
#################
*/

Grid grid;
EnemyPool enemyPool;
GameStats gameStats;
Labels labels;
WaveSystem waveSystem;

#if BULLETS_ON
BulletPool bulletPool;
#endif

// est ce q'une cellule a été activé (shop ouvert)
bool selection_active = false;
// le menu des sauvegardes est ouvert
bool save_screen = false;
// le menu pause (Échap) est ouvert ; controls_view : on y affiche la liste des commandes
bool pause_menu = false;
bool controls_view = false;
// mis à vrai par "Quitter" dans le menu pause
bool quit_requested = false;
// vraie quand la range d'une tourelle est affichée (sur la quelle est placée le curseur), pour pouvoir effacer la range une fois qu'elle ne l'est plus
bool was_range_visible = false;
int prev_selected_x = 0; // permet de détecter un changement de sélection et de nettoyer la grille
int prev_selected_y = 0;
// Position de la selection sur la grille
int selected_cell_x = 0;
int selected_cell_y = 0;

// Position de la selection dans les menus (shop/upgrade)
int ligne = 1;

// Taille actuelle du terminal
int width = 0;
int height = 0;

// Compteur début jeu
time_t time_start = 0;

// Cette fonction permet de savoir que faire si la touche entrée a été préssée
void handle_enter_key()
{
    if (grid.cells[selected_cell_x][selected_cell_y].hasTurret) // permet de savoir si la cell selctionnée a déja une tourelle (cas de l'upgrade et de la vente)
    {
        int upgrade_price = getTurretPrice(grid.cells[selected_cell_x][selected_cell_y].turret.type, grid.cells[selected_cell_x][selected_cell_y].turret.lvl + 1);
        if (ligne == 1 && (upgrade_price <= 0 || gameStats.cash < upgrade_price))
            return; // amélioration impossible : le menu reste ouvert
        if (ligne == 1) // amélioration
        {
            // Upgrade
            gameStats.cash -= upgrade_price;
            grid.cells[selected_cell_x][selected_cell_y].turret.lvl++;
        }
        else if (ligne == 2) // revente
        {
            gameStats.cash += (int)(0.8 * getTurretPrice(grid.cells[selected_cell_x][selected_cell_y].turret.type, grid.cells[selected_cell_x][selected_cell_y].turret.lvl));
            grid.cells[selected_cell_x][selected_cell_y].hasTurret = false;
        }
    }
    else // Case vide : achat de la tourelle choisie dans la boutique
    {
        enum TurretType type = shopTurretAt(ligne);
        int price = getTurretPrice(type, 0);
        if (gameStats.cash < price)
            return; // pas assez d'argent : la boutique reste ouverte
        gameStats.cash -= price;
        grid.cells[selected_cell_x][selected_cell_y].turret = getTurretStruct(type);
        grid.cells[selected_cell_x][selected_cell_y].hasTurret = true;
    }
    selection_active = false;
    fillBG(1, 1, width + 1, height + 1);
    drawFullGrid(grid);
}

// Lit un caractère s'il arrive dans les `timeout_ms` millisecondes, sinon renvoie 0.
// (évite de bloquer le jeu si l'utilisateur appuie sur Échap seul)
int read_key_timeout(int timeout_ms)
{
    fd_set set;
    FD_ZERO(&set);
    FD_SET(STDIN_FILENO, &set);
    struct timeval tv = {0, timeout_ms * 1000};
    if (select(STDIN_FILENO + 1, &set, NULL, NULL, &tv) > 0)
        return getchar();
    return 0;
}

// Ferme tous les menus et redessine la grille
void closeMenus()
{
    selection_active = false;
    save_screen = false;
    pause_menu = false;
    controls_view = false;
    ligne = 1;
    fillBG(1, 1, width + 1, height + 1);
    drawFullGrid(grid);
}

// Échap : ferme le menu ouvert, ou ouvre le menu pause
void handle_escape()
{
    if (pause_menu && controls_view)
    {
        controls_view = false; // retour de la liste des commandes au menu pause
        ligne = PAUSE_CONTROLS;
    }
    else if (pause_menu || selection_active || save_screen)
        closeMenus();
    else
    {
        pause_menu = true;
        ligne = PAUSE_RESUME;
    }
}

// Entrée dans le menu pause
void handle_pause_enter()
{
    if (controls_view)
    {
        controls_view = false;
        ligne = PAUSE_CONTROLS;
        return;
    }
    switch (ligne)
    {
    case PAUSE_RESUME:
        closeMenus();
        break;
    case PAUSE_TUTORIAL:
    {
        enum TutorialResult result = run_tutorial(width, height);
        if (result == TUTORIAL_QUIT)
            quit_requested = true;
        // retour au menu pause : on redessine le jeu derrière
        fillBG(1, 1, width + 1, height + 1);
        drawFullGrid(grid);
        break;
    }
    case PAUSE_SAVES:
        pause_menu = false;
        save_screen = true;
        ligne = 3;
        fillBG(1, 1, width + 1, height + 1);
        drawFullGrid(grid);
        break;
    case PAUSE_CONTROLS:
        controls_view = true;
        fillBG(1, 1, width + 1, height + 1);
        drawFullGrid(grid);
        break;
    case PAUSE_QUIT:
        quit_requested = true;
        break;
    }
}

void handle_arrow_keys()
{
    // Une flèche envoie 3 caractères : \033 [ A/B/C/D. Le \033 a déjà été lu.
    if (read_key_timeout(20) != '[')
    {
        handle_escape(); // touche Échap seule
        return;
    }
    char c = read_key_timeout(20);
    int cell_x = selected_cell_x;
    int cell_y = selected_cell_y;
    bool up = c == 'A';
    bool down = c == 'B';
    bool right = c == 'C';
    bool left = c == 'D';
    if (!selection_active && !save_screen && !pause_menu)
    {
        // Mouvement de la sélection de cellule, en prenant en compte le chemin et les bords de la grid
        if (up)
        {
            while (cell_y > 1 && grid.cells[cell_x][cell_y - 1].type == CHEMIN)
                cell_y -= 1;

            if (cell_y > 0)
            {
                grid.cells[selected_cell_x][selected_cell_y].selected = false;
                selected_cell_y = cell_y - 1;
                grid.cells[selected_cell_x][selected_cell_y].selected = true;
            }
            else
            {
                cell_y = grid.height;
                while (cell_y > 1 && grid.cells[cell_x][cell_y - 1].type == CHEMIN)
                {
                    cell_y -= 1;
                }
                grid.cells[selected_cell_x][selected_cell_y].selected = false;
                selected_cell_y = cell_y - 1;
                grid.cells[selected_cell_x][selected_cell_y].selected = true;
            }
        }
        else if (down)
        {
            while (cell_y < grid.height - 1 && grid.cells[cell_x][cell_y + 1].type == CHEMIN)
                cell_y += 1;

            if (cell_y < grid.height - 1)
            {
                grid.cells[selected_cell_x][selected_cell_y].selected = false;
                selected_cell_y = cell_y + 1;
                grid.cells[selected_cell_x][selected_cell_y].selected = true;
            }
            else
            {
                cell_y = -1;
                while (cell_y < grid.height - 1 && grid.cells[cell_x][cell_y + 1].type == CHEMIN)
                {
                    cell_y += 1;
                }
                grid.cells[selected_cell_x][selected_cell_y].selected = false;
                selected_cell_y = cell_y + 1;
                grid.cells[selected_cell_x][selected_cell_y].selected = true;
            }
        }
        else if (right)
        {
            while (cell_x < grid.width - 1 && grid.cells[cell_x + 1][cell_y].type == CHEMIN)
                cell_x += 1;

            if (cell_x < grid.width - 1)
            {
                grid.cells[selected_cell_x][selected_cell_y].selected = false;
                selected_cell_x = cell_x + 1;
                grid.cells[selected_cell_x][selected_cell_y].selected = true;
            }
            else
            {
                cell_x = -1;
                while (cell_x < grid.width - 1 && grid.cells[cell_x + 1][cell_y].type == CHEMIN)
                {
                    cell_x += 1;
                }
                grid.cells[selected_cell_x][selected_cell_y].selected = false;
                selected_cell_x = cell_x + 1;
                grid.cells[selected_cell_x][selected_cell_y].selected = true;
            }
        }
        else if (left)
        {
            while (cell_x >= 1 && grid.cells[cell_x - 1][cell_y].type == CHEMIN)
                cell_x -= 1;

            if (cell_x > 0)
            {
                grid.cells[selected_cell_x][selected_cell_y].selected = false;
                selected_cell_x = cell_x - 1;
                grid.cells[selected_cell_x][selected_cell_y].selected = true;
            }
            else
            {
                cell_x = grid.width;
                while (cell_x > 1 && grid.cells[cell_x - 1][cell_y].type == CHEMIN)
                {
                    cell_x -= 1;
                }
                grid.cells[selected_cell_x][selected_cell_y].selected = false;
                selected_cell_x = cell_x - 1;
                grid.cells[selected_cell_x][selected_cell_y].selected = true;
            }
        }
        drawFullGrid(grid);
        scr_flush();
    }
    else
    {
        // Mouvement du curseur dans le shop
        if (down)
        {
            ligne += 1;
        }
        else if (up)
        {
            ligne -= 1;
        }
        // CLAMP(val, x, y) : garde val entre x et y
        if (pause_menu)
            ligne = CLAMP(ligne, 1, PAUSE_ITEMS);
        else if (save_screen) // 7 emplacements => lignes 3 à 9
            ligne = CLAMP(ligne, 3, 9);
        else if (grid.cells[selected_cell_x][selected_cell_y].hasTurret) // menu d'amélioration : améliorer ou vendre
            ligne = CLAMP(ligne, 1, 2); // (avant, le curseur pouvait descendre jusqu'à 7 et disparaître)
        else // boutique
            ligne = CLAMP(ligne, 1, SHOP_SIZE);
    }
}

struct termios base_t_settings;
void configTerminal()
{
    setbuf(stdin, NULL);

    // Écran alternatif : le jeu s'affiche sur un écran séparé, et le terminal
    // retrouve son contenu d'origine à la fin (comme vim ou htop).
    printf("\033[?1049h");
    hide_cursor();
    fflush(stdout);

    // https://man7.org/linux/man-pages/man3/termios.3.html
    tcgetattr(STDIN_FILENO, &base_t_settings);
    struct termios t_settings = base_t_settings;

    // Place le terminal en mode non canonique (entrées envoyées sans enter)
    t_settings.c_lflag &= ~(ICANON);
    // sans echo, les touches pressés ne sont pas affichées.
    t_settings.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL);

    tcsetattr(STDIN_FILENO, TCSANOW, &t_settings);
}

// Remet le terminal dans son état d'origine
void restoreTerminal()
{
    scr_shutdown();
    printf(RESET "\033[?1049l"); // quitte l'écran alternatif
    show_cursor();
    fflush(stdout);
    tcsetattr(STDIN_FILENO, TCSANOW, &base_t_settings);
}

// Cette fonction est exécutée automatiquement: a la fin du programme (fin normale, appel a exit(), interruption: CTRL+C)
bool freed = false;
void cleanup()
{
    if (!freed)
    {
        freed = true;
        restoreTerminal();

        struct timespec current_time;
        clock_gettime(CLOCK_MONOTONIC, &current_time);
        printf("Bye.\n Tu as tenu: %d vagues\n Pendant: %ld min\n", waveSystem.current_wave_index, (long)(current_time.tv_sec - time_start) / 60);

        freeGrid(grid);           // Libère les allocations dynamiques liées à la grille
        freeEnemyPool(enemyPool); // Libère les allocations dynamiques liées aux ennemis
        freeLabels(labels);       // Libère les allocations dynamiques liées aux labels
        fflush(stdout);
    }
}

// Mis à vrai par le signal SIGWINCH quand la taille du terminal change
volatile sig_atomic_t terminal_resized = 0;
void onResize(int sig)
{
    (void)sig;
    terminal_resized = 1;
}

// Redessine tout l'écran après un changement de taille du terminal
void handleResize()
{
    terminal_resized = 0;
    scr_shutdown();
    clear_screen();
    checkTerminalSize(&width, &height); // bloque tant que le terminal est trop petit
    clear_screen();
    fflush(stdout);
    if (!scr_init(width, height))
    {
        printCritical("Failed to allocate screen buffer");
        exit(EXIT_FAILURE);
    }
    fillBG(1, 1, width + 1, height + 1);
    drawFullGrid(grid);
}

// Fenêtre affichée quand la partie est perdue (même taille que les autres menus)
void showGameOver()
{
    int line_h = ui_text_rows() + 1;
    int x0, y0, w, h;
    menuGeometry(&x0, &y0, &w, &h);
    drawPanel(x0, y0, w, h, COLOR_RED);
    int cx = x0 + w / 2;
    int y = y0 + h / 2 - line_h * 2;
    ui_text_centered(cx, y, BOLD COLOR_RED "Partie terminée");
    y += line_h * 2;
    char text[64];
    snprintf(text, sizeof(text), "Tu as tenu %d vague%s", waveSystem.current_wave_index, waveSystem.current_wave_index > 1 ? "s" : "");
    ui_text_centered(cx, y, text);
    y += line_h * 2;
    ui_text_centered(cx, y, COLOR_GRAY "Entrée pour quitter");
    scr_printf(RESET);
    scr_flush();

    // On vide les touches déjà appuyées pendant la partie, puis on attend Entrée, Espace ou q
    while (get_key_press() != 0)
        ;
    while (true)
    {
        int c = get_key_press();
        if (c == 10 || c == ' ' || c == 'q' || c == 'Q')
            break;
        if (terminal_resized)
            handleResize();
        msleep(20);
    }
}

void onSignal(int sig)
{
    (void)sig;
    exit(EXIT_SUCCESS); // exit() appelle cleanup() grâce à atexit()
}

/**
 * Fonction principale du jeu.
 *
 * Initialise le jeu, gère les boucles de jeu, les evenements, les mises à jour, les affichages.
 *
 * Le jeu se termine lorsque la vie de l'utilisateur atteint 0.
 */
// Options de la ligne de commande
bool opt_play = false;  // --play : passe l'intro et le menu
bool opt_debug = false; // --debug : affiche les infos techniques (fps, nombre d'ennemis...)
bool opt_sim = false;   // --sim-wave [n] : simulateur de vagues
int opt_sim_n = 0;

void printUsage(const char *prog)
{
    printf("Utilisation : %s [options]\n", prog);
    printf("  --play          lance directement la partie (sans intro ni menu)\n");
    printf("  --petit-texte   texte de taille normale (pour les grands écrans)\n");
    printf("  --debug         affiche les infos techniques en bas de l'écran\n");
    printf("  --sim-wave [n]  simulateur de vagues (voir README)\n");
}

void parseArguments(int argc, char *argv[])
{
    for (int i = 1; i < argc; i++)
    {
        if (argv[i][0] == '\0')
            continue; // argument vide : ignoré
        if (strcmp(argv[i], "--play") == 0)
            opt_play = true;
        else if (strcmp(argv[i], "--petit-texte") == 0)
            big_text_mode = false;
        else if (strcmp(argv[i], "--debug") == 0)
            opt_debug = true;
        else if (strcmp(argv[i], "--sim-wave") == 0)
        {
            opt_sim = true;
            if (i + 1 < argc && argv[i + 1][0] >= '0' && argv[i + 1][0] <= '9')
                opt_sim_n = atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0)
        {
            printUsage(argv[0]);
            exit(EXIT_SUCCESS);
        }
        else
        {
            printf("Option inconnue : %s\n\n", argv[i]);
            printUsage(argv[0]);
            exit(EXIT_FAILURE);
        }
    }
}

// Bandeau d'informations en haut de l'écran : vague, vitesse, vie, argent
void drawHUD(int game_speed_control, float fps, long runtime)
{
    // Efface le bandeau
    fillBG(1, 1, width + 1, GRID_TOP);
    scr_printf(RESET COLOR_STANDARD_BG);

    char left[96], right[96];
    if (waveSystem.wave_timer > 0)
        snprintf(left, sizeof(left), BOLD "Vague %d" BOLD_RST COLOR_GRAY "  prochaine dans %.1fs", waveSystem.current_wave_index, waveSystem.wave_timer);
    else
        snprintf(left, sizeof(left), BOLD "Vague %d", waveSystem.current_wave_index);
    ui_text(3, 1, left);

    // À droite : vitesse (si accélérée), vie, argent
    char speed[16] = "";
    if (game_speed_control > 1)
        snprintf(speed, sizeof(speed), "x%d   ", game_speed_control);
    snprintf(right, sizeof(right), "\033[38;5;45m%s" COLOR_RED "❤ %d   " COLOR_YELLOW "€ %d", speed, gameStats.health, gameStats.cash);
    scr_printf(RESET COLOR_STANDARD_BG);
    ui_text(width - 2 - ui_text_width(right), 1, right);
    scr_printf(RESET);

    // Infos techniques, seulement avec --debug, en petit sur la dernière ligne
    if (opt_debug)
    {
        move_to(1, height);
        scr_printf(RESET COLOR_STANDARD_BG ERASE_LINE COLOR_GRAY "Ennemis: %d/%d | Labels: %d/%d | %d fps | %lds" RESET,
                   enemyPool.count, enemyPool.length, labels.count, labels.size, (int)fps, runtime);
    }
}

/*
Mise en place automatique du terminal.
Si le terminal n'a pas la bonne taille et qu'on est dans un environnement graphique,
le jeu se relance lui-même via jouer.sh, qui ouvre une fenêtre plein écran avec la bonne police.
Dans cette nouvelle fenêtre (ATARDES_LAUNCHED=1), si la taille n'est toujours pas bonne,
on indique à jouer.sh de combien corriger la police (fichier ATARDES_RATIO_FILE) et on quitte
avec le code 42 : jouer.sh rouvre alors une fenêtre avec la police corrigée.
*/
#define EXIT_RELAUNCH 42

bool terminalSizeOk(int w, int h)
{
    return w >= MIN_TERMINAL_WIDTH && h >= MIN_TERMINAL_HEIGHT && w <= MAX_TERMINAL_WIDTH && h <= MAX_TERMINAL_HEIGHT;
}

void autoSetupTerminal(int argc, char *argv[])
{
    int w, h;
    get_terminal_size(&w, &h);
    if (terminalSizeOk(w, h) || opt_sim)
        return;
    if (getenv("DISPLAY") == NULL && getenv("WAYLAND_DISPLAY") == NULL)
        return; // pas d'interface graphique (console texte, SSH) : on affichera le message habituel

    const char *launched = getenv("ATARDES_LAUNCHED");
    if (launched == NULL)
    {
        // Premier lancement depuis un terminal trop petit : on passe la main à jouer.sh (à côté de l'exécutable)
        char exe[4096];
        ssize_t n = readlink("/proc/self/exe", exe, sizeof(exe) - 16);
        if (n <= 0)
            return;
        exe[n] = '\0';
        char *slash = strrchr(exe, '/');
        if (slash == NULL)
            return;
        strcpy(slash + 1, "jouer.sh");
        if (access(exe, X_OK) != 0)
            return;
        char *args[64];
        int k = 0;
        args[k++] = exe;
        for (int i = 1; i < argc && k < 63; i++)
            args[k++] = argv[i];
        args[k] = NULL;
        printf("Le terminal fait %dx%d caractères (il en faut %dx%d) : ouverture d'une fenêtre adaptée...\n", w, h, MIN_TERMINAL_WIDTH, MIN_TERMINAL_HEIGHT);
        fflush(stdout);
        execv(exe, args);
        return; // execv a échoué : on continue normalement
    }

    // Déjà dans une fenêtre ouverte par jouer.sh. Le passage en plein écran peut prendre un instant :
    // on attend que la taille soit stable (même valeur 3 fois de suite) avant de décider (2 s au maximum).
    int stable = 0;
    for (int i = 0; i < 40 && stable < 3; i++)
    {
        msleep(50);
        int nw, nh;
        get_terminal_size(&nw, &nh);
        stable = (nw == w && nh == h) ? stable + 1 : 0;
        w = nw;
        h = nh;
    }
    if (terminalSizeOk(w, h))
        return;

    // Toujours pas la bonne taille : on calcule la correction de police nécessaire
    const char *ratio_file = getenv("ATARDES_RATIO_FILE");
    const char *retry = getenv("ATARDES_RETRY");
    if (ratio_file == NULL || (retry != NULL && atoi(retry) >= 2))
        return; // déjà corrigé deux fois : on laisse le message habituel
    // Facteur à appliquer à la police pour obtenir environ 240x77 caractères (un peu plus que le minimum)
    double ratio = MIN((double)w / (MIN_TERMINAL_WIDTH + 8), (double)h / (MIN_TERMINAL_HEIGHT + 3));
    FILE *f = fopen(ratio_file, "w");
    if (f == NULL)
        return;
    fprintf(f, "%.3f\n", ratio);
    fclose(f);
    exit(EXIT_RELAUNCH);
}

int main(int argc, char *argv[])
{
    parseArguments(argc, argv);
    autoSetupTerminal(argc, argv);
    // En grande police, on réserve des lignes en haut de l'écran pour le bandeau d'infos
    grid_top = big_text_mode ? 1 + HUD_ROWS_BIG : 2;

    // Enregistre la fonction cleanup pour qu'elle soit exécutée lors la terminaison du programme.
    atexit(cleanup);
    struct sigaction act = {0};
    act.sa_handler = &onSignal;
    sigemptyset(&act.sa_mask);
    sigaction(SIGINT, &act, NULL);
    sigaction(SIGTERM, &act, NULL);
    struct sigaction resize_act = {0};
    resize_act.sa_handler = &onResize;
    sigemptyset(&resize_act.sa_mask);
    sigaction(SIGWINCH, &resize_act, NULL);

    srand(time(NULL));

    // Timers du jeu
    struct timespec prev_time;
    clock_gettime(CLOCK_MONOTONIC, &prev_time);
    time_start = prev_time.tv_sec;

    /**
     * ##########################
     * Initialisation du terminal
     * ##########################
     */
    configTerminal();
    clear_screen();

    // Vérifie et bloque tant que la taille du terminal ne convient pas
    checkTerminalSize(&width, &height);
    clear_screen();
    fflush(stdout);

    // À partir d'ici, tout l'affichage passe par le moteur de rendu (double buffering)
    if (!scr_init(width, height))
    {
        printCritical("Failed to allocate screen buffer");
        exit(EXIT_FAILURE);
    }

    if (!opt_play && !opt_sim)
    {
        anim_debut(width, height);
    }

    fillBG(1, 1, width + 1, height + 1);

    /**
     * ##################################
     * Initialisation des éléments du jeu
     * ##################################
     */
    // Gestion de la grille et du chemin
    grid.width = (width - 2) / (CELL_WIDTH + 2);
    grid.height = (height - GRID_TOP + 2) / (CELL_HEIGHT + 1); // la dernière case doit finir avant le bas de l'écran
    allocateGridCells(&grid);
    // Génération du chemin
    genBasicPath(&grid);

    // Gestion des statistiques
    gameStats.cash = 30;
    gameStats.health = 20;

    // Gestion labels
    labels.size = 255;
    labels.labels = malloc(sizeof(struct Label) * labels.size);
    if (labels.labels == NULL)
    {
        printCritical("Failed to allocate label");
        // free de tout automatique dans main.c/void cleanup()
        exit(EXIT_FAILURE);
    }

    // Gestion ennemis
    enemyPool = AllocEnemyPool();
    switchToWave(&waveSystem, 0);

    /**
     * ###############################
     * Gestion des paramètres spéciaux
     * ###############################
     */

    // La première cellule est initialement sélectionnée
    grid.cells[0][0].selected = true;

    /**
     * ###############################
     * Gestion des paramètres spéciaux
     * ###############################
     */
    if (opt_sim)
    {
        restoreTerminal(); // la simulation affiche du texte normal, sans le moteur de rendu
        testWaveSystem(grid, &enemyPool, opt_sim_n, &gameStats);
        return 0;
    }

    // Affichage intégral de la grille
    drawFullGrid(grid);

    // Le chronomètre démarre au début de la partie (après l'intro et le tutoriel)
    clock_gettime(CLOCK_MONOTONIC, &prev_time);
    time_start = prev_time.tv_sec;

    // size_t est un int de capacité plus élevée.
    size_t frame_index = 0;
    int fps = 0;

    // entiers qui gèrent la vitesse du jeu
    int game_speed_control = 1;
    int compteur_animation_game_speed = 10;

    // Le jeu tourne tant que la vie n'atteint pas 0
    while (gameStats.health > 0)
    {
        // Gestion du temps
        struct timespec current_time;
        clock_gettime(CLOCK_MONOTONIC, &current_time);
        // Temps réel écoulé depuis l'image précédente (en secondes)
        float real_dt = (current_time.tv_nsec - prev_time.tv_nsec) / 1000000000.0 + (current_time.tv_sec - prev_time.tv_sec);
        prev_time = current_time;

        if (real_dt > 0)
            fps = fps * 0.9f + 0.1f * (1.0f / real_dt); // moyenne glissante pour un affichage stable

        // Si le jeu a été bloqué un moment (ex: terminal en arrière-plan), on évite un énorme saut dans le temps
        if (real_dt > MAX_REAL_DT)
            real_dt = MAX_REAL_DT;

        // Temps de jeu écoulé : c'est ici qu'agit la vitesse (x1 à x16).
        // Avant, le plafond était appliqué APRÈS cette multiplication, ce qui limitait la vitesse réelle à environ x4.
        float game_dt = real_dt * game_speed_control;

        // Pour garder une simulation précise à grande vitesse, on découpe le temps de jeu en petits pas
        int sim_steps = (int)ceilf(game_dt / MAX_SIM_STEP);
        if (sim_steps < 1)
            sim_steps = 1;
        float delta_t = game_dt / sim_steps;

        if (terminal_resized)
            handleResize();

        frame_index++;

        /*
        ###############################
        AFFICHAGE ET MISE A JOUR DU JEU
        ###############################
        */

        if (!selection_active && !save_screen && !pause_menu) // un menu est ouvert : le jeu est en pause
        {
            // On efface les zones où se trouvaient les ennemis à l'image précédente (et on redessine les tourelles)
#if BULLETS_ON
            if (frame_index % 50 == 0) // Avec les missiles, on efface tout l'écran régulièrement pour supprimer leurs traces
            {
                fillBG(1, 1, width + 1, height + 1);
                drawFullGrid(grid);
            }
            else
#endif
                clearUsedPath(grid, enemyPool);

            // Simulation du jeu, en plusieurs petits pas si la vitesse est élevée
            bool wave_ended = false;
            for (int step = 0; step < sim_steps && gameStats.health > 0; step++)
            {
                // Les tourelles tirent sur les ennemis à portée
#if BULLETS_ON
                updateTowers(grid, enemyPool, &bulletPool, delta_t, &gameStats);
                updateBullets(&bulletPool, delta_t);
#else
                updateTowers(grid, enemyPool, delta_t, &gameStats);
#endif
                // Mise à jour des labels (suppression des vieux)
                updateLabels(&labels, delta_t);
                // Apparition des ennemis de la vague
                if (updateWaveSystem(&waveSystem, grid, &enemyPool, delta_t, &gameStats) == -2)
                    wave_ended = true;
                // Déplacement des ennemis
                updateEnemies(&enemyPool, grid, &gameStats, &labels, delta_t);
            }
#if BULLETS_ON
            drawBullets(bulletPool);
#endif

            if (wave_ended)
            {
                fillBG(1, 1, width + 1, height + 1);
                drawFullGrid(grid);
            }
            drawLabels(labels);
            // Affichage des ennemis
            drawEnemies(enemyPool, grid);

            // Affichage de la portée des tourelles
            if ((was_range_visible)                    //
                && (prev_selected_x != selected_cell_x //
                    || prev_selected_y != selected_cell_y))
            {
                fillBG(1, 1, width + 1, height + 1);
                drawFullGrid(grid);
                was_range_visible = false;
                prev_selected_x = selected_cell_x;
                prev_selected_y = selected_cell_y;
            }
            if (grid.cells[selected_cell_x][selected_cell_y].hasTurret)
            {
                float range_min = grid.cells[selected_cell_x][selected_cell_y].turret.range_min[grid.cells[selected_cell_x][selected_cell_y].turret.lvl];
                float range_max = grid.cells[selected_cell_x][selected_cell_y].turret.range_max[grid.cells[selected_cell_x][selected_cell_y].turret.lvl];
                if (range_min > 0)
                    drawRange(width, height, range_min, selected_cell_x + 0.5, selected_cell_y + 0.5, false);
                if (range_max > 0)
                    drawRange(width, height, range_max, selected_cell_x + 0.5, selected_cell_y + 0.5, false);

                was_range_visible = true;
                prev_selected_x = selected_cell_x;
                prev_selected_y = selected_cell_y;
            }
        }

        // Icône en haut à droite : pause tant que la boutique est ouverte, sinon la vitesse actuelle.
        // (avant, l'état "pause" restait bloqué si on fermait la boutique en achetant une tourelle)
        if (compteur_animation_game_speed < delta_t || selection_active || pause_menu)
        {
            drawGameSpeed(game_speed_control, width, selection_active || pause_menu);
            compteur_animation_game_speed += 10;
        }

        /*
        ##############################
        AFFICHAGE DE LA RANGÉE DU HAUT
        ##############################
        */
        drawHUD(game_speed_control, fps, (long)(current_time.tv_sec - time_start));

        // Envoi de l'image complète au terminal (une seule écriture, seulement ce qui a changé)
        scr_flush();

        /*
        ###########################
        GESTION DES ENTREES CLAVIER
        ###########################
        */
        int c = get_key_press();
        if (c != 0)
        {
            // Les actions du menu des sauvegardes ne concernent que les touches pressées quand il était déjà ouvert
            // (sinon l'Entrée qui l'ouvre depuis le menu pause chargerait immédiatement une sauvegarde)
            bool save_was_open = save_screen;
            if (c == 'q' || c == 'Q')
            {
                saveProgress(grid, gameStats, waveSystem.current_wave_index, 0);
                break;
            }
            if ((c == 's' || c == 'S') && !selection_active && !pause_menu)
            {
                if (save_screen)
                {
                    fillBG(1, 1, width + 1, height + 1);
                    drawFullGrid(grid);
                    ligne = 3;
                }
                ligne = 3;
                save_screen = !save_screen;
            }
            else if (c == 'o' || c == 'O')
            {
                compteur_animation_game_speed = 0;
                if (game_speed_control > 8)
                {
                    game_speed_control = 16;
                }
                else
                {
                    game_speed_control *= 2;
                }
            }
            else if (c == 'l' || c == 'L')
            {
                compteur_animation_game_speed = 0;
                if (game_speed_control < 2)
                {
                    game_speed_control = 1;
                }
                else
                {
                    game_speed_control /= 2;
                }
            }
            else if (c == ' ' && !save_screen && !pause_menu) // Touche espace pressée
            {
                // Nettoyage
                if (selection_active)
                {
                    fillBG(1, 1, width + 1, height + 1);
                    drawFullGrid(grid);
                    ligne = 1;
                }
                ligne = 1;
                selection_active = !selection_active;
            }
            else if (c == 27) // Touches directionnelles commencent par le char (\033)
                handle_arrow_keys();

            else if (c == 10 && selection_active) // ENTER
                handle_enter_key();
            else if (c == 10 && pause_menu)
                handle_pause_enter();

            if (quit_requested)
            {
                saveProgress(grid, gameStats, waveSystem.current_wave_index, 0);
                break;
            }
            if (pause_menu)
            {
                if (controls_view)
                    showControls();
                else
                    showPauseMenu(ligne);
            }

            if (selection_active)
            {
                showTowerSelection(ligne, grid.cells[selected_cell_x][selected_cell_y].hasTurret, grid.cells[selected_cell_x][selected_cell_y].turret, gameStats.cash);
            }
            if (save_screen)
            {
                int slot = ligne - 3; // les emplacements save_0 à save_6 sont sur les lignes 3 à 9
                if (!save_was_open)
                    ; // le menu vient de s'ouvrir : rien à faire
                else if (c == 10) // Entrée : charger
                {
                    if (loadProgress(slot, &grid, &gameStats, &waveSystem, &enemyPool, &labels))
                    {
                        // La grille chargée peut être plus petite : on remet la sélection sur une case valide
                        selected_cell_x = 0;
                        selected_cell_y = 0;
                        prev_selected_x = prev_selected_y = 0;
                        grid.cells[0][0].selected = true;
                        fillBG(1, 1, width + 1, height + 1);
                        drawFullGrid(grid);
                        save_screen = false;
                    }
                }
                else if (c == ' ') // Espace : sauvegarder
                {
                    saveProgress(grid, gameStats, waveSystem.current_wave_index, slot);
                }
                if (save_screen)
                    showSaveScreen(ligne); // affiché après l'action pour que l'état des emplacements soit à jour
            }
        }

        // Limiteur d'images : on dort le temps restant pour ne pas dépasser TARGET_FPS images par seconde
        // (sinon, avec le nouveau moteur d'affichage très rapide, le jeu utiliserait 100% d'un processeur)
        struct timespec frame_end;
        clock_gettime(CLOCK_MONOTONIC, &frame_end);
        float frame_time = (frame_end.tv_nsec - current_time.tv_nsec) / 1000000000.0 + (frame_end.tv_sec - current_time.tv_sec);
        if (frame_time < 1.0 / TARGET_FPS)
            nsleep_seconds(1.0 / TARGET_FPS - frame_time);
    }

    /*
    ################
    FIN DE LA PARTIE
    ################
    */

    if (gameStats.health <= 0)
        showGameOver();

    // Les free se trouvent dans la fonction cleanup, appelée automatiquement à la sortie (atexit)
    return 0;
}
