#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "creme.h"

/* PID du fils servbeuip, 0 si non demarre */
pid_t beuip_pid = 0;

/* ------------------------------------------------------------------ */
/* Fonction interne : ouvre un socket UDP et envoie un message local  */
/* ------------------------------------------------------------------ */
static void envoie_local(struct msg_beuip *msg, size_t taille)
{
    int sid = socket(AF_INET, SOCK_DGRAM, 0);
    if (sid < 0) { perror("creme: socket"); return; }

    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family      = AF_INET;
    dest.sin_port        = htons(PORT_BEUIP);
    dest.sin_addr.s_addr = inet_addr("127.0.0.1");

    sendto(sid, msg, taille, 0, (struct sockaddr *)&dest, sizeof(dest));
    close(sid);
}

/* ------------------------------------------------------------------ */
/* creme_start : fork + exec servbeuip                                */
/* ------------------------------------------------------------------ */
int creme_start(const char *pseudo)
{
    if (beuip_pid != 0) {
        printf("[BEUIP] Serveur deja demarre (pid %d)\n", beuip_pid);
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("creme: fork");
        return -1;
    }

    if (pid == 0) {
        /* Fils : on execute servbeuip dans le meme repertoire */
        execlp("./servbeuip", "servbeuip", pseudo, NULL);
        /* Si execlp echoue, essai dans le PATH */
        execlp("servbeuip", "servbeuip", pseudo, NULL);
        perror("creme: execlp servbeuip");
        exit(EXIT_FAILURE);
    }

    /* Pere : on memorise le PID du fils */
    beuip_pid = pid;
#ifdef TRACE
    printf("[TRACE] servbeuip demarre, pid=%d, pseudo=%s\n", beuip_pid, pseudo);
#endif
    return 0;
}

/* ------------------------------------------------------------------ */
/* creme_stop : envoie SIGINT au fils -> hand_depart -> code '0'      */
/* ------------------------------------------------------------------ */
int creme_stop(void)
{
    if (beuip_pid == 0) {
        printf("[BEUIP] Aucun serveur en cours d'execution\n");
        return -1;
    }

    kill(beuip_pid, SIGINT);
    waitpid(beuip_pid, NULL, 0);
    beuip_pid = 0;
#ifdef TRACE
    printf("[TRACE] servbeuip arrete\n");
#endif
    return 0;
}

/* ------------------------------------------------------------------ */
/* creme_liste : code '3'                                             */
/* ------------------------------------------------------------------ */
void creme_liste(void)
{
    struct msg_beuip msg;
    memset(&msg, 0, sizeof(msg));
    msg.code = '3';
    strcpy(msg.entete, "BEUIP");
    /* pseudo vide : MSG_SIZE = 7 + 0 + 1 = 8 */
    envoie_local(&msg, MSG_SIZE(msg));
}

/* ------------------------------------------------------------------ */
/* creme_msg_pseudo : code '4'  ->  pseudo\0message\0                */
/* ------------------------------------------------------------------ */
void creme_msg_pseudo(const char *pseudo, const char *message)
{
    struct msg_beuip msg;
    memset(&msg, 0, sizeof(msg));
    msg.code = '4';
    strcpy(msg.entete, "BEUIP");

    size_t plen = strlen(pseudo);
    if (plen > 127) { fprintf(stderr, "creme: pseudo trop long\n"); return; }
    strncpy(msg.pseudo, pseudo, 127);
    strncpy(msg.pseudo + plen + 1, message, 255 - (int)plen - 1);

    /* Taille manuelle car double chaine */
    size_t mlen = strlen(message);
    size_t taille = 7 + plen + 1 + mlen + 1;
    envoie_local(&msg, taille);
}

/* ------------------------------------------------------------------ */
/* creme_msg_tous : code '5'                                          */
/* ------------------------------------------------------------------ */
void creme_msg_tous(const char *message)
{
    struct msg_beuip msg;
    memset(&msg, 0, sizeof(msg));
    msg.code = '5';
    strcpy(msg.entete, "BEUIP");
    strncpy(msg.pseudo, message, 255);
    envoie_local(&msg, MSG_SIZE(msg));
}
