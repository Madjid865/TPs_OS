#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>

#define MAXPAR 20
#define NBMAXC 10 /* Nb max de commandes internes */

static char *Mots[MAXPAR]; /* le tableau des mots de la commande */
static int NMots;          /* nombre de mots de la commande */

/* Structure pour une commande interne */
typedef struct {
    char *nom;                             /* Nom de la commande */
    int (*fonction)(int, char **);         /* Pointeur sur la fonction associée */
} CommandeInterne;

static CommandeInterne TabComInt[NBMAXC];  /* Tableau des commandes internes */
static int NbComInt = 0;                   /* Nombre actuel de commandes internes */

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

void libereMots() {
    for (int i = 0; i < NMots; i++) {
        free(Mots[i]);
    }
}

/* Ajoute une commande au tableau */
void ajouteCom(char *nom, int (*f)(int, char **)) {
    if (NbComInt < NBMAXC) {
        TabComInt[NbComInt].nom = nom;
        TabComInt[NbComInt].fonction = f;
        NbComInt++;
    } else {
        fprintf(stderr, "Erreur : Trop de commandes internes\n");
        exit(1);
    }
}

/* La fonction pour la commande 'exit' */
int Sortie(int n, char **p) {
    printf("Bye !\n");
    exit(0);
}

/* Initialisation du tableau des commandes */
void majComInt(void) {
    ajouteCom("exit", Sortie);
    /* On pourra ajouter "cd" ou "pwd" plus tard ici */
}

/* Exécute la commande si elle est interne */
int execComInt(int n, char **p) {
    for (int i = 0; i < NbComInt; i++) {
        if (strcmp(p[0], TabComInt[i].nom) == 0) {
            TabComInt[i].fonction(n, p);
            return 1; /* C'est une commande interne */
        }
    }
    return 0; /* Pas une commande interne */
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
    
    // Initialisation des commandes internes
    majComInt();

    // Boucle interactive avec readline
    while (1) {
        ligne = readline(prompt);

        if (ligne == NULL) {
            printf("\nSortie de biceps... Bye !\n");
            break;
        }

        if (strlen(ligne) > 0) {
            add_history(ligne);
            
            /* Analyse de la ligne en mots */
            int n = analyseCom(ligne);

            if (n > 0) {
                /* Tentative d'exécution en tant que commande interne */
                /* On passe le nombre de mots et le tableau global Mots */
                if (!execComInt(n, Mots)) {
                    /* Si ce n'est pas une commande interne, on affiche simplement */
                    printf("%s : commande externe\n", Mots[0]);
                }
            }

            /* Nettoyage des mots copiés par copyString */
            libereMots();
        }

        // Libérer la mémoire de la ligne lue par readline
        free(ligne);
    }

    free(prompt);
    return 0;
}
