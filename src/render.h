#pragma once

/*
##########################
MOTEUR D'AFFICHAGE (render)
##########################

Principe du "double buffering" :

Au lieu d'écrire directement dans le terminal (lent, et chaque écriture est
visible immédiatement, ce qui provoque le clignotement), tout le code de
dessin écrit dans un tableau en mémoire : le "back buffer".

Quand l'image est terminée, scr_present() compare le back buffer avec ce qui
est réellement affiché à l'écran (le "front buffer") et n'envoie au terminal
QUE les caractères qui ont changé, en UNE SEULE écriture.

Conséquences :
  - plus de clignotement : effacer puis redessiner une zone ne coûte rien,
    seul le résultat final est envoyé ;
  - beaucoup moins de données envoyées au terminal (souvent quelques
    centaines d'octets par image au lieu de plusieurs centaines de Ko) ;
  - plus d'artefacts : ce qui est en mémoire est exactement ce qui est affiché.

Le code de dessin existant n'a presque pas changé : il suffit d'utiliser
scr_printf() à la place de printf() et scr_flush() à la place de
fflush(stdout). scr_printf() comprend les codes ANSI utilisés par le jeu
(couleurs, déplacement du curseur, effacement de l'écran ou d'une ligne).

Tant que le moteur n'est pas initialisé (ou après scr_shutdown()),
scr_printf() et scr_flush() se comportent exactement comme printf() et
fflush(stdout). On peut donc les utiliser partout sans risque.
*/

#include <stdbool.h>

// Alloue les buffers pour un terminal de width x height caractères et active le moteur.
// Renvoie false si l'allocation échoue.
bool scr_init(int width, int height);

// Désactive le moteur et libère les buffers. scr_printf redevient un printf normal.
void scr_shutdown(void);

// Vrai si le moteur est actif.
bool scr_is_active(void);

// Équivalent de printf(), mais écrit dans le back buffer quand le moteur est actif.
int scr_printf(const char *format, ...) __attribute__((format(printf, 1, 2)));

// Écrit une chaîne telle quelle (sans interprétation des % comme printf).
void scr_puts(const char *str);

// Envoie au terminal les différences entre le back buffer et l'écran (une seule écriture).
void scr_present(void);

// scr_present() si le moteur est actif, sinon fflush(stdout).
void scr_flush(void);

// Mémorise / restaure les couleurs courantes de scr_printf (utile pour dessiner sur plusieurs lignes)
unsigned long scr_attr_save(void);
void scr_attr_restore(unsigned long saved);

// Force le prochain scr_present() à redessiner tout l'écran
// (utile si le terminal a été modifié par un autre moyen).
void scr_invalidate(void);
