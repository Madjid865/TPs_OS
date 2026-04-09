/*
 * creme.c – Commandes Rapides pour l'Envoi de Messages Evolués
 * Implémentation du protocole BEUIP v1
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "creme.h"

/* PID du serveur BEUIP lancé par biceps */
pid_t beuip_server_pid = -1;

/* ------------------------------------------------------------------ */
/*  beuip_create_socket                                                */
/*  Crée un socket UDP configuré pour le broadcast, bindé sur         */
/*  BEUIP_PORT (INADDR_ANY).                                           */
/*  Retourne le descripteur ou -1 en cas d'erreur.                    */
/* ------------------------------------------------------------------ */
int beuip_create_socket(void)
{
    int sock, opt = 1;
    struct sockaddr_in addr;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) { perror("socket"); return -1; }

    /* Autoriser le broadcast */
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt)) < 0) {
        perror("setsockopt SO_BROADCAST");
        close(sock);
        return -1;
    }

    /* Réutiliser le port immédiatement après fermeture */
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt SO_REUSEADDR");
        close(sock);
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(BEUIP_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(sock);
        return -1;
    }

    return sock;
}

/* ------------------------------------------------------------------ */
/*  beuip_send                                                         */
/*  Envoie un message BEUIP formaté à dest.                           */
/*  payload peut être NULL (paylen = 0).                              */
/* ------------------------------------------------------------------ */
int beuip_send(int sock, struct sockaddr_in *dest,
               char code, const char *payload, int paylen)
{
    /* buf = code (1) + "BEUIP" (5) + payload */
    int   total = 1 + 5 + paylen;
    char *buf   = malloc(total + 1);
    int   ret;

    if (!buf) { perror("malloc"); return -1; }

    buf[0] = code;
    memcpy(buf + 1, BEUIP_MAGIC, 5);
    if (paylen > 0 && payload)
        memcpy(buf + 6, payload, paylen);

    ret = sendto(sock, buf, total, 0,
                 (struct sockaddr *)dest, sizeof(*dest));
    if (ret < 0)
        perror("sendto");

    free(buf);
    return ret;
}

/* ------------------------------------------------------------------ */
/*  beuip_broadcast                                                    */
/*  Envoie un message BEUIP en broadcast sur BEUIP_BCAST:BEUIP_PORT   */
/* ------------------------------------------------------------------ */
int beuip_broadcast(int sock, char code, const char *payload, int paylen)
{
    struct sockaddr_in bcast;
    memset(&bcast, 0, sizeof(bcast));
    bcast.sin_family      = AF_INET;
    bcast.sin_port        = htons(BEUIP_PORT);
    inet_pton(AF_INET, BEUIP_BCAST, &bcast.sin_addr);
    return beuip_send(sock, &bcast, code, payload, paylen);
}

/* ------------------------------------------------------------------ */
/*  beuip_check_header                                                 */
/*  Vérifie que buf contient bien l'en-tête BEUIP.                    */
/*  Retourne 1 (ok) ou 0 (invalide).                                  */
/*  code_out est rempli avec le code du message si ok.                */
/* ------------------------------------------------------------------ */
int beuip_check_header(const char *buf, int len, char *code_out)
{
    if (len < 6) return 0;
    if (memcmp(buf + 1, BEUIP_MAGIC, 5) != 0) return 0;
    if (code_out) *code_out = buf[0];
    return 1;
}

/* ------------------------------------------------------------------ */
/*  beuip_table_add                                                    */
/*  Ajoute (pseudo, addr) dans la table si absent.                    */
/*  Retourne 1 si ajouté, 0 si déjà présent, -1 si table pleine.     */
/* ------------------------------------------------------------------ */
int beuip_table_add(BeuipPair *table, int *nb,
                    const char *pseudo, struct in_addr addr)
{
    /* Vérifier doublons */
    for (int i = 0; i < *nb; i++) {
        if (table[i].addr.s_addr == addr.s_addr) {
            /* Mettre à jour le pseudo si nécessaire */
            strncpy(table[i].pseudo, pseudo, BEUIP_MAXPSEUDO - 1);
            return 0;
        }
    }
    if (*nb >= BEUIP_MAXTABLE) return -1;

    strncpy(table[*nb].pseudo, pseudo, BEUIP_MAXPSEUDO - 1);
    table[*nb].pseudo[BEUIP_MAXPSEUDO - 1] = '\0';
    table[*nb].addr = addr;
    (*nb)++;
    return 1;
}

/* ------------------------------------------------------------------ */
/*  beuip_table_find_by_pseudo                                         */
/*  Cherche l'adresse IP associée à un pseudo.                        */
/*  Retourne 1 si trouvé, 0 sinon.                                    */
/* ------------------------------------------------------------------ */
int beuip_table_find_by_pseudo(BeuipPair *table, int nb,
                               const char *pseudo, struct in_addr *addr_out)
{
    for (int i = 0; i < nb; i++) {
        if (strcmp(table[i].pseudo, pseudo) == 0) {
            if (addr_out) *addr_out = table[i].addr;
            return 1;
        }
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/*  beuip_table_find_by_addr                                           */
/*  Cherche le pseudo associé à une adresse IP.                       */
/*  Retourne 1 si trouvé, 0 sinon.                                    */
/* ------------------------------------------------------------------ */
int beuip_table_find_by_addr(BeuipPair *table, int nb,
                              struct in_addr addr, char *pseudo_out)
{
    for (int i = 0; i < nb; i++) {
        if (table[i].addr.s_addr == addr.s_addr) {
            if (pseudo_out)
                strncpy(pseudo_out, table[i].pseudo, BEUIP_MAXPSEUDO);
            return 1;
        }
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/*  beuip_table_remove                                                 */
/*  Supprime le couple dont l'adresse IP est addr.                    */
/* ------------------------------------------------------------------ */
void beuip_table_remove(BeuipPair *table, int *nb, struct in_addr addr)
{
    for (int i = 0; i < *nb; i++) {
        if (table[i].addr.s_addr == addr.s_addr) {
            /* Décaler les éléments suivants */
            for (int j = i; j < *nb - 1; j++)
                table[j] = table[j + 1];
            (*nb)--;
            return;
        }
    }
}

/* ------------------------------------------------------------------ */
/*  beuip_table_print                                                  */
/*  Affiche le contenu de la table.                                   */
/* ------------------------------------------------------------------ */
void beuip_table_print(BeuipPair *table, int nb)
{
    char ip[INET_ADDRSTRLEN];
    printf("Utilisateurs présents (%d) :\n", nb);
    for (int i = 0; i < nb; i++) {
        inet_ntop(AF_INET, &table[i].addr, ip, sizeof(ip));
        printf("  %-20s %s\n", table[i].pseudo, ip);
    }
}
