#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT_BEUIP 9998
#define MAX_PEERS 255
#define BC_ADDR "192.168.88.255"

/* Structure du message BEUIP demandée */
struct msg_beuip {
    char code;          /* '1' broadcast, '2' AR */
    char entete[6];     /* "BEUIP" */
    char pseudo[64];    /* Pseudo de l'expéditeur */
};

/* Table pour stocker les utilisateurs (IP + Pseudo) */
typedef struct {
    struct in_addr addr;
    char pseudo[64];
} Peer;

Peer table_utilisateurs[MAX_PEERS];
int nb_utilisateurs = 0;

/* Fonction pour ajouter un utilisateur sans doublon */
void ajouter_peer(struct in_addr addr, char *pseudo) {
    for (int i = 0; i < nb_utilisateurs; i++) {
        if (table_utilisateurs[i].addr.s_addr == addr.s_addr) return;
    }
    if (nb_utilisateurs < MAX_PEERS) {
        table_utilisateurs[nb_utilisateurs].addr = addr;
        strncpy(table_utilisateurs[nb_utilisateurs].pseudo, pseudo, 63);
        printf("[INFO] Nouveau venu : %s (%s)\n", pseudo, inet_ntoa(addr));
        nb_utilisateurs++;
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <votre_pseudo>\n", argv[0]);
        return 1;
    }

    int sid;
    struct sockaddr_in sock_serv, sock_client;
    socklen_t len = sizeof(sock_client);
    struct msg_beuip msg;

    /* 1. Création du socket UDP */
    if ((sid = socket(AF_INET, SOCK_DGRAM, 0)) < 0) { perror("socket"); return 2; }

    /* 2. Activation du mode Broadcast */
    int bc_enable = 1;
    setsockopt(sid, SOL_SOCKET, SO_BROADCAST, &bc_enable, sizeof(bc_enable));

    /* 3. Bind sur le port 9998 */
    memset(&sock_serv, 0, sizeof(sock_serv));
    sock_serv.sin_family = AF_INET;
    sock_serv.sin_port = htons(PORT_BEUIP);
    sock_serv.sin_addr.s_addr = INADDR_ANY;

    if (bind(sid, (struct sockaddr *)&sock_serv, sizeof(sock_serv)) == -1) {
        perror("bind"); return 3;
    }

    /* 4. Envoi du message de présence en broadcast */
    struct sockaddr_in addr_bc;
    addr_bc.sin_family = AF_INET;
    addr_bc.sin_port = htons(PORT_BEUIP);
    addr_bc.sin_addr.s_addr = inet_addr(BC_ADDR);

    msg.code = '1';
    strcpy(msg.entete, "BEUIP");
    strncpy(msg.pseudo, argv[1], 63);
    
    sendto(sid, &msg, sizeof(msg), 0, (struct sockaddr *)&addr_bc, sizeof(addr_bc));
    printf("Serveur BEUIP lance sous le pseudo : %s\n", argv[1]);

    /* 5. Boucle d'attente */
    while (1) {
        int n = recvfrom(sid, &msg, sizeof(msg), 0, (struct sockaddr *)&sock_client, &len);
        if (n > 0) {
            /* Vérification de l'entête "BEUIP" */
            if (strncmp(msg.entete, "BEUIP", 5) == 0) {
                ajouter_peer(sock_client.sin_addr, msg.pseudo);

                /* Si c'est un broadcast ('1'), on répond par un AR ('2') */
                if (msg.code == '1') {
                    struct msg_beuip ar_msg = {'2', "BEUIP", ""};
                    strncpy(ar_msg.pseudo, argv[1], 63);
                    sendto(sid, &ar_msg, sizeof(ar_msg), 0, (struct sockaddr *)&sock_client, len);
                }
            }

            if (msg.code == '3') { 
                /* Commande liste : Affichage local des utilisateurs */
                printf("\n--- Liste des utilisateurs connectés (%d) ---\n", nb_utilisateurs);
                for (int i = 0; i < nb_utilisateurs; i++) {
                    printf("%d: %s [%s]\n", i+1, table_utilisateurs[i].pseudo, inet_ntoa(table_utilisateurs[i].addr));
                }
            } 
            else if (msg.code == '4') {
                /* Message à un pseudo : recherche de l'IP correspondante */
                char *dest_pseudo = msg.pseudo;
                char *contenu = msg.pseudo + strlen(dest_pseudo) + 1; // Le message est après le \0
                
                for (int i = 0; i < nb_utilisateurs; i++) {
                    if (strcmp(table_utilisateurs[i].pseudo, dest_pseudo) == 0) {
                        /* On change le code en '9' pour l'envoi final */
                        struct msg_beuip msg_final = {'9', "BEUIP", ""};
                        strcpy(msg_final.pseudo, contenu); 
                        sendto(sid, &msg_final, sizeof(msg_final), 0, (struct sockaddr *)&table_utilisateurs[i].addr, len);
                    }
                }
            }
            else if (msg.code == '9') {
                /* Réception d'un message distant : on affiche l'expéditeur */
                printf("Message reçu : %s\n", msg.pseudo);
            }            
        }
    }
    return 0;
}
