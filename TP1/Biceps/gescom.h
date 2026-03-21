#ifndef GESCOM_H
#define GESCOM_H

#define MAXPAR 20
#define NBMAXC 10

/* Prototypes exportés */
int analyseCom(char *b);
void libereMots();
void majComInt(void);
int execComInt(int n, char **p);
int execComExt(char **P);
void executeCommande(char *cmd);

#endif
