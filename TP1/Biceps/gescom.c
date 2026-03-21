#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include "gescom.h"

static char *Mots[MAXPAR];
static int NMots;

typedef struct {
    char *nom;
    int (*fonction)(int, char **);
} CommandeInterne;

static CommandeInterne TabComInt[NBMAXC];
static int NbComInt = 0;

int analyseCom(char *b) {
    char *token;
    char *delimiters = " \t\n";
    NMots = 0;
    while ((token = strsep(&b, delimiters)) != NULL) {
        if (*token != '\0') {
            if (NMots < MAXPAR - 1) {
                Mots[NMots] = strdup(token); // Utilisation de strdup
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

void ajouteCom(char *nom, int (*f)(int, char **)) {
    if (NbComInt < NBMAXC) {
        TabComInt[NbComInt].nom = nom;
        TabComInt[NbComInt].fonction = f;
        NbComInt++;
    }
}

/* Fonctions internes (gardées statiques si possible ou exportées) */
int Sortie(int n, char **p) { printf("Bye !\n"); exit(0); }
int changeDir(int n, char **p) {
    char *path = (n == 1) ? getenv("HOME") : p[1];
    if (chdir(path) != 0) perror("biceps: cd");
    return 0;
}
int afficheDir(int n, char **p) {
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd))) printf("%s\n", cwd);
    return 0;
}
int version(int n, char **p) { printf("biceps version 1.0 (gescom lib 1.0)\n"); return 0; }

void majComInt(void) {
    ajouteCom("exit", Sortie);
    ajouteCom("cd", changeDir);
    ajouteCom("pwd", afficheDir);
    ajouteCom("vers", version);
}

int execComInt(int n, char **p) {
    for (int i = 0; i < NbComInt; i++) {
        if (strcmp(p[0], TabComInt[i].nom) == 0) {
            TabComInt[i].fonction(n, p);
            return 1;
        }
    }
    return 0;
}

int execComExt(char **P) {
    pid_t pid = fork();
    if (pid == 0) {
        if (execvp(P[0], P) == -1) perror("biceps");
        exit(EXIT_FAILURE);
    } else if (pid > 0) waitpid(pid, NULL, 0);
    return 1;
}

void executeCommande(char *cmd) {
    if (cmd == NULL || *cmd == '\0') return;
    int n = analyseCom(cmd);
    if (n > 0) {
        if (!execComInt(n, Mots)) execComExt(Mots);
    }
    libereMots();
}
