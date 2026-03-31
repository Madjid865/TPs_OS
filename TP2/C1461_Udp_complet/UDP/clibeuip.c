#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT_BEUIP 9998

struct msg_beuip {
    char code;
    char entete[6];
    char pseudo[256]; // On élargit pour contenir pseudo + message
};

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <code: 3, 4 ou 5> [pseudo] [message]\n", argv[0]);
        return 1;
    }

    int sid = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT_BEUIP);
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    struct msg_beuip msg = { argv[1][0], "BEUIP", "" };

    if (msg.code == '4') {
        /* On concatène pseudo + \0 + message */
        int p_len = strlen(argv[2]);
        strcpy(msg.pseudo, argv[2]);
        strcpy(msg.pseudo + p_len + 1, argv[3]);
    } else if (msg.code == '5') {
        strcpy(msg.pseudo, argv[2]);
    }

    sendto(sid, &msg, sizeof(msg), 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    return 0;
}
