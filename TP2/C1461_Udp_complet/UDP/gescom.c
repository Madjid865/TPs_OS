#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include "gescom.h"
#include "creme.h"

static char *Mots[MAXPAR];
static int   NMots;

typedef struct {
    char *nom;
    int (*fonction)(int, char **);
} CommandeInterne;

static CommandeInterne TabComInt[NBMAXC];
static int NbComInt = 0;

/* ------------------------------------------------------------------ */
/* Analyse de la ligne de commande                                    */
/* ------------------------------------------------------------------ */
int analyseCom(char *b)
{
    char *token;
    char *delimiters = " \t\n";
    NMots = 0;
    while ((token = strsep(&b, delimiters)) != NULL) {
        if (*token != '\0') {
            if (NMots < MAXPAR - 1) {
                Mots[NMots] = strdup(token);
                NMots++;
            }
        }
    }
    Mots[NMots] = NULL;
    return NMots;
}

void libereMots(void)
{
    for (int i = 0; i < NMots; i++) {
        free(Mots[i]);
        Mots[i] = NULL;
    }
    NMots = 0;
}

/* ------------------------------------------------------------------ */
/* Enregistrement des commandes internes                              */
/* ------------------------------------------------------------------ */
void ajouteCom(char *nom, int (*f)(int, char **))
{
    if (NbComInt < NBMAXC) {
        TabComInt[NbComInt].nom      = nom;
        TabComInt[NbComInt].fonction = f;
        NbComInt++;
    }
}

/* ------------------------------------------------------------------ */
/* Commandes internes historiques                                     */
/* ------------------------------------------------------------------ */
int Sortie(int n, char **p)
{
    (void)n; (void)p;
    /* Si le serveur tourne encore, on l'arrete proprement */
    if (beuip_pid != 0) creme_stop();
    printf("Bye !\n");
    exit(0);
}

int changeDir(int n, char **p)
{
    char *path = (n == 1) ? getenv("HOME") : p[1];
    if (chdir(path) != 0) perror("biceps: cd");
    return 0;
}

int afficheDir(int n, char **p)
{
    (void)n; (void)p;
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd))) printf("%s\n", cwd);
    return 0;
}

int version(int n, char **p)
{
    (void)n; (void)p;
    printf("biceps version 2.0 (gescom 1.1 / creme 1.0)\n");
    return 0;
}

/* ------------------------------------------------------------------ */
/* Commande : beuip start <pseudo>                                    */
/*            beuip stop                                              */
/* ------------------------------------------------------------------ */
int cmd_beuip(int n, char **p)
{
    if (n < 2) {
        printf("Usage:\n");
        printf("  beuip start <pseudo>   demarre le serveur BEUIP\n");
        printf("  beuip stop             arrete  le serveur BEUIP\n");
        return 0;
    }

    if (strcmp(p[1], "start") == 0) {
        if (n < 3) {
            printf("beuip start : pseudo manquant\n");
            return 0;
        }
        creme_start(p[2]);

    } else if (strcmp(p[1], "stop") == 0) {
        creme_stop();

    } else {
        printf("beuip : sous-commande inconnue '%s'\n", p[1]);
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Commande : mess liste                                              */
/*            mess <pseudo> <message>                                 */
/*            mess tous <message>                                     */
/* ------------------------------------------------------------------ */
int cmd_mess(int n, char **p)
{
    if (n < 2) {
        printf("Usage:\n");
        printf("  mess liste                  affiche les pairs connectes\n");
        printf("  mess <pseudo> <message>     envoie un message a un pseudo\n");
        printf("  mess tous <message>         envoie un message a tout le monde\n");
        return 0;
    }

    if (strcmp(p[1], "liste") == 0) {
        creme_liste();

    } else if (strcmp(p[1], "tous") == 0) {
        if (n < 3) { printf("mess tous : message manquant\n"); return 0; }
        /* Reconstituer le message si plusieurs mots */
        char message[256] = "";
        for (int i = 2; i < n; i++) {
            if (i > 2) strncat(message, " ", sizeof(message) - strlen(message) - 1);
            strncat(message, p[i], sizeof(message) - strlen(message) - 1);
        }
        creme_msg_tous(message);

    } else {
        /* mess <pseudo> <message...> */
        if (n < 3) { printf("mess : message manquant\n"); return 0; }
        char message[256] = "";
        for (int i = 2; i < n; i++) {
            if (i > 2) strncat(message, " ", sizeof(message) - strlen(message) - 1);
            strncat(message, p[i], sizeof(message) - strlen(message) - 1);
        }
        creme_msg_pseudo(p[1], message);
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Initialisation de la table des commandes internes                  */
/* ------------------------------------------------------------------ */
void majComInt(void)
{
    ajouteCom("exit",  Sortie);
    ajouteCom("cd",    changeDir);
    ajouteCom("pwd",   afficheDir);
    ajouteCom("vers",  version);
    ajouteCom("beuip", cmd_beuip);
    ajouteCom("mess",  cmd_mess);
}

/* ------------------------------------------------------------------ */
/* Execution des commandes                                            */
/* ------------------------------------------------------------------ */
int execComInt(int n, char **p)
{
    for (int i = 0; i < NbComInt; i++) {
        if (strcmp(p[0], TabComInt[i].nom) == 0) {
            TabComInt[i].fonction(n, p);
            return 1;
        }
    }
    return 0;
}

int execComExt(char **P)
{
    pid_t pid = fork();
    if (pid == 0) {
        if (execvp(P[0], P) == -1) perror("biceps");
        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        waitpid(pid, NULL, 0);
    }
    return 1;
}

void executeCommande(char *cmd)
{
    if (cmd == NULL || *cmd == '\0') return;
    int n = analyseCom(cmd);
    if (n > 0) {
        if (!execComInt(n, Mots)) execComExt(Mots);
    }
    libereMots();
}
