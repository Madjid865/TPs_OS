#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT_BEUIP 9998

struct msg_beuip {
    char code;
    char entete[6];
    char pseudo[256];
};

#define MSG_SIZE(m) (7 + strlen((m).pseudo) + 1)

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage:\n");
        printf("  %s 3                      -> liste des pairs\n", argv[0]);
        printf("  %s 4 <pseudo> <message>   -> message à un pseudo\n", argv[0]);
        printf("  %s 5 <message>            -> message à tout le monde\n", argv[0]);
        return 1;
    }

    char code = argv[1][0];

    /* Vérification des paramètres selon le code */
    if (code == '4' && argc < 4) {
        fprintf(stderr, "Erreur: code 4 requiert <pseudo> et <message>\n");
        return 1;
    }
    if (code == '5' && argc < 3) {
        fprintf(stderr, "Erreur: code 5 requiert <message>\n");
        return 1;
    }

    int sid = socket(AF_INET, SOCK_DGRAM, 0);
    if (sid < 0) { perror("socket"); return 1; }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family      = AF_INET;
    serv_addr.sin_port        = htons(PORT_BEUIP);
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    struct msg_beuip msg;
    memset(&msg, 0, sizeof(msg));
    msg.code = code;
    strcpy(msg.entete, "BEUIP");

    if (code == '4') {
        /* pseudo\0message dans le champ pseudo */
        size_t plen = strlen(argv[2]);
        if (plen > 127) {
            fprintf(stderr, "Erreur: pseudo trop long (max 127 chars)\n");
            return 1;
        }
        strncpy(msg.pseudo, argv[2], 127);
        /* Le message commence juste après le '\0' du pseudo */
        strncpy(msg.pseudo + plen + 1, argv[3], 255 - (int)plen - 1);

    } else if (code == '5') {
        strncpy(msg.pseudo, argv[2], 255);

    }
    /* Pour le code '3', le champ pseudo reste vide */

    /* Pour le code '4' : pseudo\0message\0 — strlen s'arrête au 1er '\0',
       il faut calculer la taille manuellement */
    size_t send_size;
    if (code == '4') {
        size_t plen = strlen(msg.pseudo);
        size_t mlen = strlen(msg.pseudo + plen + 1);
        send_size = 7 + plen + 1 + mlen + 1;
    } else {
        send_size = MSG_SIZE(msg);
    }
    sendto(sid, &msg, send_size, 0,
           (struct sockaddr *)&serv_addr, sizeof(serv_addr));

    close(sid);
    return 0;
}
