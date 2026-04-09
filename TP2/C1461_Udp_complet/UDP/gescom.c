/*
 * gescom.c – Gestion des commandes internes et externes pour biceps
 * Librairie gescom v1.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include "gescom.h"

/* ------------------------------------------------------------------ */
/*  Variables globales (privées au module sauf déclaration extern)     */
/* ------------------------------------------------------------------ */
char **Mots  = NULL;
int   NMots  = 0;

static Commande TabCom[NBMAXC];
static int      NbCom = 0;

/* ------------------------------------------------------------------ */
/*  analyseCom – découpe la ligne en mots (séparateurs : ' ' '\t' '\n')*/
/*  Retourne le nombre de mots trouvés.                                */
/* ------------------------------------------------------------------ */
int analyseCom(char *b)
{
    char *copie, *p, *mot;
    int   n = 0;

    /* Libérer l'éventuel tableau précédent */
    if (Mots) {
        for (int i = 0; i < NMots; i++)
            free(Mots[i]);
        free(Mots);
        Mots  = NULL;
        NMots = 0;
    }

    if (!b || *b == '\0')
        return 0;

    copie = strdup(b);
    if (!copie) { perror("strdup"); return 0; }

    /* 1er passage : compter les mots */
    p = copie;
    while ((mot = strsep(&p, " \t\n")) != NULL)
        if (*mot != '\0') n++;

    if (n == 0) { free(copie); return 0; }

    /* Allouer le tableau (n+1 pour le NULL terminal requis par execvp) */
    Mots = malloc((n + 1) * sizeof(char *));
    if (!Mots) { perror("malloc"); free(copie); return 0; }

    /* 2e passage : remplir le tableau */
    strcpy(copie, b);        /* restaurer la copie (strsep l'a modifiée) */
    p = copie;
    int idx = 0;
    while ((mot = strsep(&p, " \t\n")) != NULL) {
        if (*mot == '\0') continue;
        Mots[idx] = strdup(mot);
        if (!Mots[idx]) { perror("strdup"); break; }
        idx++;
    }
    Mots[idx] = NULL;   /* sentinelle pour execvp */
    NMots = idx;

    free(copie);
    return NMots;
}

/* ------------------------------------------------------------------ */
/*  ajouteCom – enregistre une commande interne dans le tableau        */
/* ------------------------------------------------------------------ */
void ajouteCom(const char *nom, int (*f)(int, char *[]))
{
    if (NbCom >= NBMAXC) {
        fprintf(stderr, "ajouteCom: tableau plein (NBMAXC=%d) !\n", NBMAXC);
        exit(EXIT_FAILURE);
    }
    TabCom[NbCom].nom      = strdup(nom);
    TabCom[NbCom].fonction = f;
    NbCom++;
}

/* ------------------------------------------------------------------ */
/*  listeComInt – affiche toutes les commandes internes enregistrées   */
/* ------------------------------------------------------------------ */
void listeComInt(void)
{
    printf("Commandes internes (%d) :\n", NbCom);
    for (int i = 0; i < NbCom; i++)
        printf("  %s\n", TabCom[i].nom);
}

/* ------------------------------------------------------------------ */
/*  execComInt – exécute une commande interne si elle existe           */
/*  Retourne 1 si trouvée, 0 sinon.                                   */
/* ------------------------------------------------------------------ */
int execComInt(int N, char **P)
{
    if (N <= 0 || !P || !P[0]) return 0;

    for (int i = 0; i < NbCom; i++) {
        if (strcmp(P[0], TabCom[i].nom) == 0) {
#ifdef TRACE
            fprintf(stderr, "[TRACE] commande interne : %s\n", P[0]);
#endif
            TabCom[i].fonction(N, P);
            return 1;
        }
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/*  execComExt – fork + execvp pour lancer une commande externe        */
/* ------------------------------------------------------------------ */
int execComExt(char **P)
{
    pid_t pid;
    int   status;

    if (!P || !P[0]) return -1;

#ifdef TRACE
    fprintf(stderr, "[TRACE] commande externe : %s\n", P[0]);
#endif

    pid = fork();
    if (pid < 0) {
        perror("fork");
        return -1;
    }

    if (pid == 0) {
        /* fils */
        execvp(P[0], P);
        /* execvp ne retourne qu'en cas d'erreur */
        fprintf(stderr, "biceps: %s: commande introuvable\n", P[0]);
        exit(EXIT_FAILURE);
    }

    /* père */
    if (waitpid(pid, &status, 0) < 0)
        perror("waitpid");

    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}
