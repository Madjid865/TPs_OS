#ifndef GESCOM_H
#define GESCOM_H

#define MAXPAR  20
#define NBMAXC  20   /* augmente pour accueillir les nouvelles commandes */

/* Prototypes exportes */
int  analyseCom(char *b);
void libereMots(void);
void majComInt(void);
int  execComInt(int n, char **p);
int  execComExt(char **P);
void executeCommande(char *cmd);

#endif /* GESCOM_H */
