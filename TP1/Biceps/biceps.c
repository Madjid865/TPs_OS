#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <sys/wait.h>

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

int execComExt(char **P) {
    pid_t pid;
    int status;

    pid = fork(); /* Création du processus fils  */

    if (pid == 0) {
        /* On est dans le processus FILS */
        /* execvp cherche le programme dans le PATH et utilise le tableau Mots  */
        if (execvp(P[0], P) == -1) {
            perror("biceps"); /* Affiche l'erreur si la commande n'existe pas */
        }
        exit(EXIT_FAILURE); /* On quitte le fils si execvp a échoué */
    } 
    else if (pid < 0) {
        /* Erreur lors du fork */
        perror("biceps (fork)");
    } 
    else {
        /* On est dans le processus PÈRE */
        /* Il attend que le fils termine son exécution  */
        waitpid(pid, &status, 0);
    }
    return 1;
}

/* Commande interne 'cd' */
int changeDir(int n, char **p) {
    char *path;

    // Si on tape juste "cd", on va dans le répertoire HOME
    if (n == 1) {
        path = getenv("HOME");
    } else {
        path = p[1];
    }

    // Tentative de changement de répertoire
    if (chdir(path) != 0) {
        perror("biceps: cd"); // Affiche l'erreur si le dossier n'existe pas
        return 1;
    }

    return 0;
}

/* Commande interne 'pwd' */
int afficheDir(int n, char **p) {
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("%s\n", cwd);
    } else {
        perror("biceps: pwd");
        return 1;
    }
    return 0;
}

/* Commande interne 'vers' */
int version(int n, char **p) {
    printf("biceps version 1.00\n");
    return 0;
}

/* Initialisation du tableau des commandes */
void majComInt(void) {
    ajouteCom("exit", Sortie);
    ajouteCom("cd", changeDir);
    ajouteCom("pwd", afficheDir);
    ajouteCom("vers", version);
}

/* Exécute une seule commande */
void executeCommande(char *cmd) {
    if (cmd == NULL || *cmd == '\0') return;

    int n = analyseCom(cmd);
    if (n > 0) {
        if (!execComInt(n, Mots)) {
            execComExt(Mots);
        }
    }
    libereMots(); // On libère après chaque commande du bloc ;
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
            
            char *ptr_ligne = ligne; // strsep modifie le pointeur, on garde une copie
            char *commande_seule;

            /* Découpage par point-virgule */
            while ((commande_seule = strsep(&ptr_ligne, ";")) != NULL) {
                /* On exécute chaque morceau séquentiellement */
                executeCommande(commande_seule);
            }
        }

        // Libérer la mémoire de la ligne lue par readline
        free(ligne);
    }

    free(prompt);
    return 0;
}
