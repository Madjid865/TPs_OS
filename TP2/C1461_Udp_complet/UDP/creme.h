#ifndef CREME_H
#define CREME_H

#include <sys/types.h>

/* creme v1.0 - Commandes Rapides pour l'Envoi de Messages Evolues
 * Librairie de gestion du protocole BEUIP
 */

#define PORT_BEUIP  9998
#define BC_ADDR     "192.168.88.255"
#define MAX_PEERS   255

struct msg_beuip {
    char code;
    char entete[6];
    char pseudo[256];
};

/* Taille reelle a envoyer : 1(code) + 6(entete) + strlen(pseudo) + 1('\0') */
#define MSG_SIZE(m) (7 + strlen((m).pseudo) + 1)

/* PID du processus serveur fils, 0 si non demarre */
extern pid_t beuip_pid;

/* Demarre le serveur BEUIP dans un processus fils.
 * Retourne 0 en cas de succes, -1 si deja demarre ou erreur. */
int creme_start(const char *pseudo);

/* Arrete le serveur BEUIP fils (envoie SIGINT -> broadcast code '0').
 * Retourne 0 en cas de succes, -1 si non demarre. */
int creme_stop(void);

/* Envoie la commande LISTE au serveur local (code '3'). */
void creme_liste(void);

/* Envoie un message a un pseudo via le serveur local (code '4'). */
void creme_msg_pseudo(const char *pseudo, const char *message);

/* Envoie un message a tout le monde via le serveur local (code '5'). */
void creme_msg_tous(const char *message);

#endif /* CREME_H */
