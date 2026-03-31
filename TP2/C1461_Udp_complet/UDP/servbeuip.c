#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>

#define PORT_BEUIP 9998
#define MAX_PEERS 255
#define BC_ADDR "192.168.88.255"

struct msg_beuip {
    char code;
    char entete[6];
    char pseudo[256];
};

/* Taille réelle : 1 (code) + 6 (entete) + strlen(pseudo) + 1 ('\0') */
#define MSG_SIZE(m) (7 + strlen((m).pseudo) + 1)

typedef struct {
    struct in_addr addr;
    char pseudo[64];
} Peer;

Peer table_utilisateurs[MAX_PEERS];
int nb_utilisateurs = 0;
int sid;
char *mon_pseudo;

void ajouter_peer(struct in_addr addr, char *pseudo) {
    /* Met à jour le pseudo si l'adresse existe déjà */
    for (int i = 0; i < nb_utilisateurs; i++) {
        if (table_utilisateurs[i].addr.s_addr == addr.s_addr) {
            strncpy(table_utilisateurs[i].pseudo, pseudo, 63);
            table_utilisateurs[i].pseudo[63] = '\0';
            return;
        }
    }
    if (nb_utilisateurs < MAX_PEERS) {
        table_utilisateurs[nb_utilisateurs].addr = addr;
        strncpy(table_utilisateurs[nb_utilisateurs].pseudo, pseudo, 63);
        table_utilisateurs[nb_utilisateurs].pseudo[63] = '\0';
#ifdef TRACE
        printf("[TRACE] Ajout table : %s (%s)\n", pseudo, inet_ntoa(addr));
#endif
        nb_utilisateurs++;
    }
}

/* Gère le départ propre (Code '0') */
void hand_depart(int sig __attribute__((unused))) {
    struct msg_beuip msg_fin;
    memset(&msg_fin, 0, sizeof(msg_fin));
    msg_fin.code = '0';
    strcpy(msg_fin.entete, "BEUIP");
    strncpy(msg_fin.pseudo, mon_pseudo, 255);

    struct sockaddr_in addr_bc;
    memset(&addr_bc, 0, sizeof(addr_bc));
    addr_bc.sin_family      = AF_INET;
    addr_bc.sin_port        = htons(PORT_BEUIP);
    addr_bc.sin_addr.s_addr = inet_addr(BC_ADDR);

    sendto(sid, &msg_fin, MSG_SIZE(msg_fin), 0,
           (struct sockaddr *)&addr_bc, sizeof(addr_bc));
    printf("\n[INFO] Signal de depart envoye. Bye !\n");
    close(sid);
    exit(0);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <pseudo>\n", argv[0]);
        return 1;
    }
    mon_pseudo = argv[1];
    signal(SIGINT, hand_depart);

    struct sockaddr_in sock_serv, sock_client;
    socklen_t len = sizeof(sock_client);
    struct msg_beuip msg;

    /* Création du socket UDP */
    sid = socket(AF_INET, SOCK_DGRAM, 0);
    if (sid < 0) { perror("socket"); return 1; }

    /* Autoriser le broadcast */
    int bc_enable = 1;
    setsockopt(sid, SOL_SOCKET, SO_BROADCAST, &bc_enable, sizeof(bc_enable));

    /* Bind sur toutes les interfaces */
    memset(&sock_serv, 0, sizeof(sock_serv));
    sock_serv.sin_family      = AF_INET;
    sock_serv.sin_port        = htons(PORT_BEUIP);
    sock_serv.sin_addr.s_addr = INADDR_ANY;
    if (bind(sid, (struct sockaddr *)&sock_serv, sizeof(sock_serv)) < 0) {
        perror("bind"); return 1;
    }

    /* Annonce d'arrivée (Code '1') */
    struct sockaddr_in addr_bc;
    memset(&addr_bc, 0, sizeof(addr_bc));
    addr_bc.sin_family      = AF_INET;
    addr_bc.sin_port        = htons(PORT_BEUIP);
    addr_bc.sin_addr.s_addr = inet_addr(BC_ADDR);

    struct msg_beuip msg_init;
    memset(&msg_init, 0, sizeof(msg_init));
    msg_init.code = '1';
    strcpy(msg_init.entete, "BEUIP");
    strncpy(msg_init.pseudo, mon_pseudo, 255);

    sendto(sid, &msg_init, MSG_SIZE(msg_init), 0,
           (struct sockaddr *)&addr_bc, sizeof(addr_bc));
#ifdef TRACE
    printf("[TRACE] Annonce broadcast envoyee avec pseudo : %s\n", mon_pseudo);
#endif

    printf("[INFO] Serveur BEUIP démarré avec le pseudo : %s\n", mon_pseudo);
    printf("[INFO] En attente de messages... (Ctrl+C pour quitter)\n");

    /* Buffer brut pour la réception — on détecte le format ensuite */
    char buf[512];

    while (1) {
        memset(buf, 0, sizeof(buf));
        int n = recvfrom(sid, buf, sizeof(buf) - 1, 0,
                         (struct sockaddr *)&sock_client, &len);
        if (n <= 0) continue;

        /* Remplir msg selon le format détecté :
         * Format A (avec entête) : code(1) + "BEUIP\0"(6) + pseudo
         * Format B (sans entête) : code(1) + pseudo
         * On détecte en regardant si buf[1..5] == "BEUIP"
         */
        memset(&msg, 0, sizeof(msg));
        msg.code = buf[0];

        if (n >= 6 && strncmp(buf + 1, "BEUIP", 5) == 0) {
            /* Format A : notre format natif */
            memcpy(msg.entete, buf + 1, 6);
            int pseudo_len = n - 7;
            if (pseudo_len > 0 && pseudo_len < 256)
                memcpy(msg.pseudo, buf + 7, pseudo_len);
#ifdef TRACE
            printf("[TRACE] Format A (avec entete), pseudo='%s'\n", msg.pseudo);
#endif
        } else {
            /* Format B : sans entête, pseudo commence à offset 1 */
            strcpy(msg.entete, "BEUIP");
            int pseudo_len = n - 1;
            if (pseudo_len > 0 && pseudo_len < 256)
                memcpy(msg.pseudo, buf + 1, pseudo_len);
#ifdef TRACE
            printf("[TRACE] Format B (sans entete), pseudo='%s'\n", msg.pseudo);
#endif
        }

        char *ip_client = inet_ntoa(sock_client.sin_addr);
#ifdef TRACE
        printf("[TRACE] Message recu de %s, code='%c'\n", ip_client, msg.code);
#endif

        switch (msg.code) {

            case '1': /* Présence broadcast : on ajoute et on répond */
                ajouter_peer(sock_client.sin_addr, msg.pseudo);
                {
                    struct msg_beuip ar;
                    memset(&ar, 0, sizeof(ar));
                    ar.code = '2';
                    strcpy(ar.entete, "BEUIP");
                    strncpy(ar.pseudo, mon_pseudo, 255);
                    sendto(sid, &ar, MSG_SIZE(ar), 0,
                           (struct sockaddr *)&sock_client, len);
#ifdef TRACE
                    printf("[TRACE] AR envoye a %s (%s)\n", msg.pseudo, ip_client);
#endif
                }
                break;

            case '2': /* Réception d'un AR : on ajoute le pair */
                ajouter_peer(sock_client.sin_addr, msg.pseudo);
#ifdef TRACE
                printf("[TRACE] AR recu de %s (%s)\n", msg.pseudo, ip_client);
#endif
                break;

            case '3': /* Commande LISTE — local uniquement */
                if (strcmp(ip_client, "127.0.0.1") != 0) break;
                printf("\n--- Table BEUIP (%d utilisateur(s)) ---\n", nb_utilisateurs);
                for (int i = 0; i < nb_utilisateurs; i++)
                    printf("  %s -> %s\n",
                           table_utilisateurs[i].pseudo,
                           inet_ntoa(table_utilisateurs[i].addr));
                printf("---\n");
                break;

            case '4': /* Envoyer message à un pseudo — local uniquement */
                if (strcmp(ip_client, "127.0.0.1") != 0) break;
                {
                    /* pseudo\0message dans msg.pseudo */
                    char *target = msg.pseudo;
                    char *text   = msg.pseudo + strlen(target) + 1;
                    int trouve   = 0;
                    for (int i = 0; i < nb_utilisateurs; i++) {
                        if (strcmp(table_utilisateurs[i].pseudo, target) == 0) {
                            struct msg_beuip m9;
                            memset(&m9, 0, sizeof(m9));
                            m9.code = '9';
                            strcpy(m9.entete, "BEUIP");
                            strncpy(m9.pseudo, text, 255);
                            struct sockaddr_in dest;
                            memset(&dest, 0, sizeof(dest));
                            dest.sin_family      = AF_INET;
                            dest.sin_port        = htons(PORT_BEUIP);
                            dest.sin_addr        = table_utilisateurs[i].addr;
                            sendto(sid, &m9, MSG_SIZE(m9), 0,
                                   (struct sockaddr *)&dest, sizeof(dest));
                            trouve = 1;
#ifdef TRACE
                            printf("[TRACE] Message envoye a %s : %s\n", target, text);
#endif
                            break;
                        }
                    }
                    if (!trouve)
                        printf("[WARN] Pseudo introuvable : %s\n", target);
                }
                break;

            case '5': /* Message à tout le monde — local uniquement */
                if (strcmp(ip_client, "127.0.0.1") != 0) break;
                for (int i = 0; i < nb_utilisateurs; i++) {
                    struct msg_beuip m9_all;
                    memset(&m9_all, 0, sizeof(m9_all));
                    m9_all.code = '9';
                    strcpy(m9_all.entete, "BEUIP");
                    strncpy(m9_all.pseudo, msg.pseudo, 255);
                    struct sockaddr_in dest;
                    memset(&dest, 0, sizeof(dest));
                    dest.sin_family      = AF_INET;
                    dest.sin_port        = htons(PORT_BEUIP);
                    dest.sin_addr        = table_utilisateurs[i].addr;
                    sendto(sid, &m9_all, MSG_SIZE(m9_all), 0,
                           (struct sockaddr *)&dest, sizeof(dest));
                }
#ifdef TRACE
                printf("[TRACE] Message broadcast envoye a %d pairs\n", nb_utilisateurs);
#endif
                break;

            case '9': /* Affichage d'un message reçu */
                {
                    char *exp = "Inconnu";
                    for (int i = 0; i < nb_utilisateurs; i++)
                        if (table_utilisateurs[i].addr.s_addr == sock_client.sin_addr.s_addr)
                            exp = table_utilisateurs[i].pseudo;
                    printf("Message de %s : %s\n", exp, msg.pseudo);
                }
                break;

            case '0': /* Quelqu'un quitte le réseau */
                for (int i = 0; i < nb_utilisateurs; i++) {
                    if (table_utilisateurs[i].addr.s_addr == sock_client.sin_addr.s_addr) {
                        printf("[INFO] %s a quitté le réseau.\n",
                               table_utilisateurs[i].pseudo);
                        /* Suppression par écrasement avec le dernier */
                        table_utilisateurs[i] = table_utilisateurs[nb_utilisateurs - 1];
                        nb_utilisateurs--;
                        break;
                    }
                }
                break;

            default:
#ifdef TRACE
                printf("[TRACE] Code inconnu '%c' ignoré.\n", msg.code);
#endif
                break;
        }
    }
}
