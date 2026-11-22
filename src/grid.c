#include "grid.h"
#include "pixelart.h"
#include "enemies.h"
#include <string.h>

void allocateGridCells(Grid *grid)
{
    struct Cell **cells;
    cells = (struct Cell **)malloc(sizeof(struct Cell *) * grid->width); // Première coordonnée: x / width(largeur)
    grid->cells = cells;

    if (cells == NULL)
    {
        printCritical("Failed to allocate grid");
        // free de tout automatique dans main.c/void cleanup()
        exit(EXIT_FAILURE);
    }
    else
    {
        memset(cells, 0, sizeof(struct Cell *) * grid->width);
    }
    for (int x = 0; x < grid->width; x++)
    {

        cells[x] = (struct Cell *)malloc(sizeof(struct Cell) * grid->height); // deuxième coordonnée: y / height(hauteur)
        if (cells[x] == NULL)
        {
            printCritical("Failed to allocate grid col");
            // free de tout automatique dans main.c/void cleanup()
            exit(EXIT_FAILURE);
        }

        for (int y = 0; y < grid->height; y++) // Initialisation d'un terrain vierge de x*y
        {
            cells[x][y].x = x;
            cells[x][y].y = y;
            cells[x][y].type = TERRAIN;
            cells[x][y].visited = false;
            cells[x][y].selected = false;
            cells[x][y].hasTurret = false;
            cells[x][y].drawn = false;
            cells[x][y].has_effect = false;
            cells[x][y].effect = 0;
            cells[x][y].effect_counter = -1;
        }
    }
}

void freeGrid(Grid grid)
{
    scr_printf(COLOR_GRAY);
    scr_printf(" $ ");
    scr_printf(RESET);
    scr_printf("Freeing %s%dx%d%s grid:\t", COLOR_YELLOW, grid.width, grid.height, RESET);

    if (grid.cells != NULL)
    {
        for (int x = 0; x < grid.width; x++)
        {
            if (grid.cells[x] != NULL)
                free(grid.cells[x]);
        }

        free(grid.cells);
    }

    scr_printf("%s Done %s\n", COLOR_GREEN, RESET);
}

// Numérote les cases du chemin dans l'ordre de parcours (0 au départ), en suivant le chemin depuis le départ.
// Sert à l'animation des impulsions qui circulent vers la sortie.
void numberPath(Grid *grid)
{
    int x = grid->start_x, y = grid->start_y, prev_x = -1, prev_y = -1;
    for (int index = 0; index < grid->width * grid->height; index++)
    {
        grid->cells[x][y].index = index;
        int dirs[4][2] = {{1, 0}, {0, -1}, {0, 1}, {-1, 0}};
        bool moved = false;
        for (int d = 0; d < 4 && !moved; d++)
        {
            int nx = x + dirs[d][0], ny = y + dirs[d][1];
            if (nx < 0 || ny < 0 || nx >= grid->width || ny >= grid->height || (nx == prev_x && ny == prev_y))
                continue;
            if (grid->cells[nx][ny].type == CHEMIN)
            {
                prev_x = x, prev_y = y;
                x = nx, y = ny;
                moved = true;
            }
        }
        if (!moved)
            break; // bout du chemin
    }
}

void genBasicPath(Grid *grid) // Génération du terrain par rapport a la seed (rand())

{
    int chemin_x = 1;
    int chemin_y = (rand() % (grid->height));
    if (chemin_y == grid->height - 1) // Rectifications du random pour ne pas être sur les extrémités hautes et basses
        chemin_y = chemin_y - 1;

    if (chemin_y == 0)
        chemin_y = chemin_y + 1;

    struct Cell **cells = grid->cells;

    grid->start_x = 0;
    grid->start_y = chemin_y;

    cells[0][chemin_y].type = CHEMIN; // Les 2 premiers chemins sont à la même hauteur fixée aléatoirement
    cells[1][chemin_y].type = CHEMIN;

    int **historique; // Création d'un historique du chemin qui permet de ne pas tourner en boucle lors de sa génération
    historique = (int **)malloc(HISTORY_SIZE * (sizeof(int *)));
    if (historique == NULL)
    {
        printCritical("Failed to allocate historique");
        // free de tout automatique dans main.c/void cleanup()
        exit(EXIT_FAILURE);
    }

    memset(historique, 0, HISTORY_SIZE * (sizeof(int *)));
    for (size_t i = 0; i < HISTORY_SIZE; i++)
    {
        historique[i] = (int *)malloc(2 * sizeof(int));
        if (historique[i] == NULL)
        {
            printCritical("Failed to allocate an indice of historique");
            for (size_t j = 0; j <= i; j++)
                free(historique[j]);

            // free de tout automatique dans main.c/void cleanup()
            exit(EXIT_FAILURE);
        }
    }

    int history_index = 0;
    // Le point de départ de l'historique est la deuxième case du chemin (au cas où l'on y reviendrait en arrière)
    historique[0][0] = chemin_x;
    historique[0][1] = chemin_y;

    while (chemin_x != grid->width - 1) // Conditions pour la création d'une case chemin (POur ne pas que les chemins soient collés)
    {
        bool posible_bas = chemin_y < grid->height - 2                         //
                           && cells[chemin_x - 1][chemin_y + 1].type != CHEMIN // bas gauche
                           && cells[chemin_x + 1][chemin_y + 1].type != CHEMIN // bas droite
                           && cells[chemin_x][chemin_y + 1].type != CHEMIN     // bas
                           && cells[chemin_x - 1][chemin_y + 2].type != CHEMIN // bas bas gauche
                           && cells[chemin_x][chemin_y + 2].type != CHEMIN     // bas bas
                           && !cells[chemin_x][chemin_y + 1].visited;

        bool possible_haut = chemin_y > 1                                        //
                             && cells[chemin_x - 1][chemin_y - 1].type != CHEMIN // haut gauche
                             && cells[chemin_x + 1][chemin_y - 1].type != CHEMIN // haut droit
                             && cells[chemin_x][chemin_y - 1].type != CHEMIN     // haut
                             && cells[chemin_x][chemin_y - 2].type != CHEMIN     // haut haut
                             && cells[chemin_x - 1][chemin_y - 2].type != CHEMIN // haut haut gauche
                             && !cells[chemin_x][chemin_y - 1].visited;

        bool possible_droite = cells[chemin_x + 1][chemin_y + 1].type != CHEMIN                                 // droite bas
                               && cells[chemin_x + 1][chemin_y - 1].type != CHEMIN                              // droite haut
                               && cells[chemin_x + 1][chemin_y].type != CHEMIN                                  // droite
                               && (chemin_x == grid->width - 2 || cells[chemin_x + 2][chemin_y].type != CHEMIN) // droite droite
                               && !cells[chemin_x + 1][chemin_y].visited;

        bool possible_gauche = chemin_x > 1                                        //
                               && cells[chemin_x - 1][chemin_y + 1].type != CHEMIN // gauche bas
                               && cells[chemin_x - 1][chemin_y - 1].type != CHEMIN // gauche haut
                               && cells[chemin_x - 1][chemin_y].type != CHEMIN     // gauche
                               && cells[chemin_x - 2][chemin_y].type != CHEMIN     // gauche gauche
                               && cells[chemin_x - 2][chemin_y + 1].type != CHEMIN // gauche gauche haut
                               && cells[chemin_x - 2][chemin_y - 1].type != CHEMIN // gauche gauche bas
                               && !cells[chemin_x - 1][chemin_y].visited;

        if (!posible_bas && !possible_haut && !possible_droite && !possible_gauche) // Si aucun chemin n'est possible, on remonte dans l'historique jusqu'à ce qu'il le soit
        {
            cells[chemin_x][chemin_y].type = TERRAIN;
            history_index -= 1;
            chemin_x = historique[history_index][0];
            chemin_y = historique[history_index][1];
            cells[chemin_x][chemin_y].index = history_index;
            continue;
        }

        while (true) // Application des chemins
        {
            int random_chemin = rand() % 7;
            if (random_chemin <= 1 && posible_bas)
            {
                chemin_y++;
                break;
            }
            else if (random_chemin <= 3 && possible_haut)
            {
                chemin_y--;
                break;
            }
            else if (random_chemin <= 5 && possible_gauche)
            {
                chemin_x--;
                break;
            }
            else if (random_chemin == 6 && possible_droite)
            {
                chemin_x++;
                break;
            }
        }
        // Mise à jour de l'historique
        history_index++;
        assert(history_index < HISTORY_SIZE);

        historique[history_index][0] = chemin_x;
        historique[history_index][1] = chemin_y;

        cells[chemin_x][chemin_y].type = CHEMIN;
        cells[chemin_x][chemin_y].visited = true;
        cells[chemin_x][chemin_y].index = history_index;
    }

    grid->end_x = chemin_x;
    grid->end_y = chemin_y;

    numberPath(grid);

    for (size_t i = 0; i < HISTORY_SIZE; i++) // Libération de l'historique à la fin de la création de chemin
    {
        free(historique[i]);
    }
    free(historique);
}
void fillBG(int xmin, int ymin, int xmax, int ymax) // Remplit la zone avec la couleur de fond du plateau
{
    if (xmax <= xmin)
        return;
    // Une ligne d'espaces écrite d'un coup (bien plus rapide qu'un caractère à la fois)
    char spaces[1024];
    int n = MIN(xmax - xmin, (int)sizeof(spaces) - 1);
    memset(spaces, ' ', n);
    spaces[n] = '\0';
    scr_printf(COLOR_STANDARD_BG);
    for (int y = ymin; y < ymax; y++)
    {
        move_to(xmin, y);
        scr_puts(spaces);
    }
    scr_printf(RESET);
}
void drawCell(struct Cell cell, Grid grid) // Dessine les cellules selon leur status
{
    if (cell.type == TERRAIN) // Emplacement vide ou tourelle : dessiné en pixel art (voir pixelart.c)
    {
        draw_terrain_cell(cell);
        return;
    }
    if (cell.type == CHEMIN) // Case du chemin : dessinée en pixel art (voir pixelart.c)
        draw_path_cell(cell, grid);
}

void drawFullGrid(Grid grid) // Dessine la grille en entière
{

    for (int x = 0; x < grid.width; x++)
    {
        for (int y = 0; y < grid.height; y++)
        {
            drawCell(grid.cells[x][y], grid);
        }
    }
}
void clearPath(Grid grid) // Dessine uniquement les chemins
{
    for (int x = 0; x < grid.width; x++)
    {
        for (int y = 0; y < grid.height; y++)
        {
            if (grid.cells[x][y].type == CHEMIN)
                drawCell(grid.cells[x][y], grid);
        }
    }
}
