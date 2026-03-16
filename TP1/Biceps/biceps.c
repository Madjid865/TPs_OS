#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>

#define MAXPAR 20

static char *Mots[MAXPAR]; /* le tableau des mots de la commande */
static int NMots;          /* nombre de mots de la commande */

/* Copie dynamique d'une chaîne */
char *copyString(char *s) {
    char *copy = malloc(strlen(s) + 1);
    if (copy) {
        strcpy(copy, s);
    }
    return copy;
}

/* Analyse la ligne et remplit le tableau Mots */
int analyseCom(char *b) {
    char *token;
    char *delimiters = " \t\n"; /* Séparateurs : espace, tab, newline */
    
    NMots = 0;

    /* Utilise strsep pour découper la chaîne */
    while ((token = strsep(&b, delimiters)) != NULL) {
        if (*token != '\0') { /* Ignore les séparateurs consécutifs */
            if (NMots < MAXPAR - 1) {
                Mots[NMots] = copyString(token); 
                NMots++;
            }
        }
    }
    Mots[NMots] = NULL;
    return NMots;
}

int main(int argc, char *argv[]) {
    char hostname[256];
    char *user;
    char *prompt;
    char *ligne;
    char symbole;

    // Récupérer le nom de la machine
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        strcpy(hostname, "unknown");
    }

    // Récupérer l'utilisateur via l'environnement
    user = getenv("USER");
    if (user == NULL) user = "user";

    // Déterminer le symbole (# pour root, $ sinon) 
    // getuid() == 0 signifie qu'on est root
    symbole = (getuid() == 0) ? '#' : '$';

    // Allouer dynamiquement le prompt : user + @ + machine + symbole + espace + \0 
    // la taille = strlen(user) + 1 + strlen(hostname) + 1 + 1 (espace) + 1 (\0)
    prompt = malloc(strlen(user) + strlen(hostname) + 4);
    
    if (prompt == NULL) {
        perror("Erreur malloc prompt");
        return 1;
    }

    // Assemblage du prompt 
    sprintf(prompt, "%s@%s%c ", user, hostname, symbole);

    // Boucle interactive avec readline
    while (1) {
        ligne = readline(prompt);

        if (ligne == NULL) {
            printf("\nSortie de biceps... Bye !\n");
            break;
        }

        // Si la ligne n'est pas vide, on l'affiche et on l'ajoute à l'historique pour utiliser la flèche du haut
        if (strlen(ligne) > 0) {
            add_history(ligne);
            printf("Vous avez tapé : %s\n", ligne);
        }

        // Libérer la mémoire de la ligne lue
        free(ligne);
    }

    free(prompt);
    return 0;
}
