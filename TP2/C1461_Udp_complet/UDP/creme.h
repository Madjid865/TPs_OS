#ifndef CREME_H
#define CREME_H

#include <netinet/in.h>

/* ------------------------------------------------------------------ */
/*  Protocole BEUIP                                                    */
/* ------------------------------------------------------------------ */
#define BEUIP_PORT      9998
#define BEUIP_BCAST     "192.168.88.255"
#define BEUIP_MAGIC     "BEUIP"        /* octets 2-6 de chaque message */
#define BEUIP_MAXPSEUDO 32
#define BEUIP_MAXMSG    512
#define BEUIP_MAXTABLE  255

/*
 * Format d'un message BEUIP :
 *   octet 0      : code (caractère)
 *   octets 1-5   : "BEUIP"
 *   octets 6-fin : payload (pseudo, message, …)
 *
 * Codes :
 *   '0' – déconnexion (broadcast)
 *   '1' – identification broadcast
 *   '2' – accusé de réception (AR)
 *   '3' – demande de liste (client → serveur local)
 *   '4' – envoi message à un pseudo (client → serveur local)
 *   '5' – envoi message à tous (client → serveur local)
 *   '9' – message entrant (serveur distant → notre serveur)
 */
#define BEUIP_CODE_QUIT     '0'
#define BEUIP_CODE_IDENT    '1'
#define BEUIP_CODE_AR       '2'
#define BEUIP_CODE_LIST     '3'
#define BEUIP_CODE_SEND     '4'
#define BEUIP_CODE_ALL      '5'
#define BEUIP_CODE_MSG      '9'

/* ------------------------------------------------------------------ */
/*  Structure d'un couple (pseudo, adresse IP)                         */
/* ------------------------------------------------------------------ */
typedef struct {
    char            pseudo[BEUIP_MAXPSEUDO];
    struct in_addr  addr;
} BeuipPair;

/* ------------------------------------------------------------------ */
/*  Prototypes exportés                                                */
/* ------------------------------------------------------------------ */

/* Construction / envoi d'un message BEUIP */
int  beuip_send(int sock, struct sockaddr_in *dest,
                char code, const char *payload, int paylen);

/* Envoi en broadcast */
int  beuip_broadcast(int sock, char code,
                     const char *payload, int paylen);

/* Vérification de l'en-tête d'un message reçu */
int  beuip_check_header(const char *buf, int len, char *code_out);

/* Table des couples connus */
int  beuip_table_add(BeuipPair *table, int *nb,
                     const char *pseudo, struct in_addr addr);
int  beuip_table_find_by_pseudo(BeuipPair *table, int nb,
                                const char *pseudo, struct in_addr *addr_out);
int  beuip_table_find_by_addr(BeuipPair *table, int nb,
                              struct in_addr addr, char *pseudo_out);
void beuip_table_remove(BeuipPair *table, int *nb, struct in_addr addr);
void beuip_table_print(BeuipPair *table, int nb);

/* Création du socket UDP prêt pour le broadcast */
int  beuip_create_socket(void);

/* PID du processus serveur (géré par biceps) */
extern pid_t beuip_server_pid;

#endif /* CREME_H */
