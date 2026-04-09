/*
 * servbeuip.c – Serveur du protocole BEUIP v1
 *
 * Usage : servbeuip <pseudo>
 *
 * Ce programme est destiné à être lancé comme processus fils par biceps
 * via la commande interne « beuip start <pseudo> ».
 *
 * Signaux gérés :
 *   SIGTERM / SIGINT → envoi d'un message '0' broadcast puis sortie propre.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "creme.h"

/* ------------------------------------------------------------------ */
/*  Variables globales du serveur                                      */
/* ------------------------------------------------------------------ */
static int       g_sock   = -1;
static BeuipPair g_table[BEUIP_MAXTABLE];
static int       g_nb     = 0;
static char      g_pseudo[BEUIP_MAXPSEUDO];

/* ------------------------------------------------------------------ */
/*  Gestionnaire de signal : quitter proprement                        */
/* ------------------------------------------------------------------ */
static void sig_handler(int sig)
{
    (void)sig;
    printf("\n[BEUIP] Arrêt du serveur (%s)...\n", g_pseudo);

    if (g_sock >= 0) {
        /* Prévenir les autres */
        beuip_broadcast(g_sock, BEUIP_CODE_QUIT,
                        g_pseudo, (int)strlen(g_pseudo));
        close(g_sock);
    }
    exit(0);
}

/* ------------------------------------------------------------------ */
/*  main                                                               */
/* ------------------------------------------------------------------ */
int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <pseudo>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    strncpy(g_pseudo, argv[1], BEUIP_MAXPSEUDO - 1);
    g_pseudo[BEUIP_MAXPSEUDO - 1] = '\0';

    /* Création du socket */
    g_sock = beuip_create_socket();
    if (g_sock < 0) exit(EXIT_FAILURE);

    /* Enregistrement des signaux */
    signal(SIGTERM, sig_handler);
    signal(SIGINT,  sig_handler);

    /* Annonce broadcast : on est là ! */
    if (beuip_broadcast(g_sock, BEUIP_CODE_IDENT,
                        g_pseudo, (int)strlen(g_pseudo)) < 0) {
        fprintf(stderr, "[BEUIP] Impossible d'envoyer le broadcast initial\n");
    } else {
        printf("[BEUIP] Serveur démarré (pseudo : %s)\n", g_pseudo);
    }

    /* ---------------------------------------------------------------- */
    /*  Boucle principale de réception                                  */
    /* ---------------------------------------------------------------- */
    char               buf[BEUIP_MAXMSG + 16];
    struct sockaddr_in src;
    socklen_t          srclen;
    char               code;
    int                n;

    while (1) {
        srclen = sizeof(src);
        n = recvfrom(g_sock, buf, sizeof(buf) - 1, 0,
                     (struct sockaddr *)&src, &srclen);
        if (n < 0) { perror("recvfrom"); continue; }
        buf[n] = '\0';

        /* Vérification en-tête */
        if (!beuip_check_header(buf, n, &code)) {
#ifdef TRACE
            fprintf(stderr, "[TRACE] message invalide ignoré\n");
#endif
            continue;
        }

        /* Payload commence à l'octet 6 */
        char *payload = buf + 6;
        int   plen    = n - 6;
        char  src_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &src.sin_addr, src_ip, sizeof(src_ip));

#ifdef TRACE
        fprintf(stderr, "[TRACE] reçu code='%c' de %s payload='%.*s'\n",
                code, src_ip, plen, payload);
#endif

        switch (code) {

        /* ---- '1' : identification ---------------------------------- */
        case BEUIP_CODE_IDENT: {
            /* Ignorer nos propres broadcasts */
            /* (on pourrait vérifier l'IP locale mais c'est complexe;
               on se contente de ne pas s'ajouter nous-mêmes si déjà présent) */
            char pseudo_tmp[BEUIP_MAXPSEUDO] = {0};
            if (plen > 0 && plen < BEUIP_MAXPSEUDO) {
                memcpy(pseudo_tmp, payload, plen);
                pseudo_tmp[plen] = '\0';
            }

            int r = beuip_table_add(g_table, &g_nb, pseudo_tmp, src.sin_addr);
            if (r == 1)
                printf("[BEUIP] Nouveau : %s (%s)\n", pseudo_tmp, src_ip);

            /* Envoyer un AR avec notre pseudo (sauf à nous-mêmes) */
            if (strcmp(pseudo_tmp, g_pseudo) != 0) {
                beuip_send(g_sock, &src, BEUIP_CODE_AR,
                           g_pseudo, (int)strlen(g_pseudo));
            }
            break;
        }

        /* ---- '2' : accusé de réception ----------------------------- */
        case BEUIP_CODE_AR: {
            char pseudo_tmp[BEUIP_MAXPSEUDO] = {0};
            if (plen > 0 && plen < BEUIP_MAXPSEUDO) {
                memcpy(pseudo_tmp, payload, plen);
                pseudo_tmp[plen] = '\0';
            }
            int r = beuip_table_add(g_table, &g_nb, pseudo_tmp, src.sin_addr);
            if (r == 1)
                printf("[BEUIP] AR de : %s (%s)\n", pseudo_tmp, src_ip);
            break;
        }

        /* ---- '0' : déconnexion ------------------------------------- */
        case BEUIP_CODE_QUIT: {
            char pseudo_tmp[BEUIP_MAXPSEUDO] = {0};
            if (plen > 0 && plen < BEUIP_MAXPSEUDO) {
                memcpy(pseudo_tmp, payload, plen);
                pseudo_tmp[plen] = '\0';
            }
            printf("[BEUIP] Déconnexion de : %s (%s)\n", pseudo_tmp, src_ip);
            beuip_table_remove(g_table, &g_nb, src.sin_addr);
            break;
        }

        /* ---- '3' : liste (commande locale uniquement) -------------- */
        case BEUIP_CODE_LIST: {
            /* Sécurité : accepter uniquement depuis 127.0.0.1 */
            if (src.sin_addr.s_addr != htonl(INADDR_LOOPBACK)) {
                fprintf(stderr, "[BEUIP] commande '3' refusée (source : %s)\n",
                        src_ip);
                break;
            }
            /* Construire la réponse texte et la renvoyer au client local */
            char resp[4096] = {0};
            char tmp[256];
            char ip_str[INET_ADDRSTRLEN];
            snprintf(resp, sizeof(resp),
                     "\nUtilisateurs présents (%d) :\n", g_nb);
            for (int i = 0; i < g_nb; i++) {
                inet_ntop(AF_INET, &g_table[i].addr, ip_str, sizeof(ip_str));
                const char *ps = (g_table[i].pseudo[0] != '\0')
                                 ? g_table[i].pseudo : "(inconnu)";
                snprintf(tmp, sizeof(tmp), "  %-20s %s\n", ps, ip_str);
                strncat(resp, tmp, sizeof(resp) - strlen(resp) - 1);
            }
            /* Envoyer via UDP au port d'écoute du client (src.sin_port) */
            sendto(g_sock, resp, strlen(resp), 0,
                   (struct sockaddr *)&src, sizeof(src));
            break;
        }

        /* ---- '4' : envoyer message à un pseudo (commande locale) --- */
        case BEUIP_CODE_SEND: {
            if (src.sin_addr.s_addr != htonl(INADDR_LOOPBACK)) {
                fprintf(stderr, "[BEUIP] commande '4' refusée (source : %s)\n",
                        src_ip);
                break;
            }
            /* payload = pseudo\0message */
            char *dest_pseudo = payload;
            char *msg         = memchr(payload, '\0', plen);
            if (!msg || msg + 1 >= payload + plen) {
                fprintf(stderr, "[BEUIP] format '4' invalide\n");
                break;
            }
            msg++; /* pointe sur le message */

            struct in_addr dest_addr;
            if (!beuip_table_find_by_pseudo(g_table, g_nb,
                                            dest_pseudo, &dest_addr)) {
                printf("[BEUIP] Pseudo '%s' introuvable\n", dest_pseudo);
                break;
            }
            struct sockaddr_in dest;
            memset(&dest, 0, sizeof(dest));
            dest.sin_family = AF_INET;
            dest.sin_port   = htons(BEUIP_PORT);
            dest.sin_addr   = dest_addr;

            int msglen = (int)strlen(msg);
            beuip_send(g_sock, &dest, BEUIP_CODE_MSG, msg, msglen);
            printf("[BEUIP] Message envoyé à %s\n", dest_pseudo);
            break;
        }

        /* ---- '5' : message à tout le monde (commande locale) ------- */
        case BEUIP_CODE_ALL: {
            if (src.sin_addr.s_addr != htonl(INADDR_LOOPBACK)) {
                fprintf(stderr, "[BEUIP] commande '5' refusée (source : %s)\n",
                        src_ip);
                break;
            }
            /* payload = le message */
            char msg[BEUIP_MAXMSG] = {0};
            if (plen > 0 && plen < BEUIP_MAXMSG) {
                memcpy(msg, payload, plen);
                msg[plen] = '\0';
            }
            struct sockaddr_in dest;
            memset(&dest, 0, sizeof(dest));
            dest.sin_family = AF_INET;
            dest.sin_port   = htons(BEUIP_PORT);

            for (int i = 0; i < g_nb; i++) {
                /* Ne pas s'envoyer à soi-même */
                dest.sin_addr = g_table[i].addr;
                beuip_send(g_sock, &dest, BEUIP_CODE_MSG,
                           msg, (int)strlen(msg));
            }
            printf("[BEUIP] Message envoyé à tous (%d destinataires)\n", g_nb);
            break;
        }

        /* ---- '9' : message reçu d'un autre utilisateur ------------ */
        case BEUIP_CODE_MSG: {
            char expediteur[BEUIP_MAXPSEUDO] = "inconnu";
            beuip_table_find_by_addr(g_table, g_nb,
                                     src.sin_addr, expediteur);
            char msg[BEUIP_MAXMSG] = {0};
            if (plen > 0 && plen < BEUIP_MAXMSG) {
                memcpy(msg, payload, plen);
                msg[plen] = '\0';
            }
            printf("\n*** Message de %s : %s\n", expediteur, msg);
            break;
        }

        default:
#ifdef TRACE
            fprintf(stderr, "[TRACE] code inconnu : '%c'\n", code);
#endif
            break;
        }
    }

    /* Jamais atteint */
    return 0;
}
