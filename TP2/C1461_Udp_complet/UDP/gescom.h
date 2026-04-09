#ifndef GESCOM_H
#define GESCOM_H

#define NBMAXC 20  /* Nb maxi de commandes internes */

/* Structure d'une commande interne */
typedef struct {
    char *nom;
    int (*fonction)(int, char *[]);
} Commande;

/* Variables globales exportées */
extern char **Mots;   /* tableau des mots de la commande */
extern int   NMots;   /* nombre de mots de la commande   */

/* Prototypes */
int   analyseCom(char *b);
void  ajouteCom(const char *nom, int (*f)(int, char *[]));
void  majComInt(void);
void  listeComInt(void);
int   execComInt(int N, char **P);
int   execComExt(char **P);

#endif /* GESCOM_H */
