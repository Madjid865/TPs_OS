/*
 * biceps.c – Bel Interpréteur de Commandes des Elèves de Polytech Sorbonne
 * Version 2.0 – avec support du protocole BEUIP via la librairie creme
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>

#include "gescom.h"
#include "creme.h"

/* ------------------------------------------------------------------ */
/*  Version                                                            */
/* ------------------------------------------------------------------ */
#define BICEPS_VERSION "2.0"
#define HISTORY_FILE   ".biceps_history"

/* ------------------------------------------------------------------ */
/*  Gestion du signal SIGINT (Ctrl-C)                                  */
/* ------------------------------------------------------------------ */
static void sigint_handler(int sig)
{
    (void)sig;
    /* On ne quitte pas – on réaffiche juste le prompt */
    printf("\n(Utilisez 'exit' ou Ctrl-D pour quitter)\n");
    rl_on_new_line();
    rl_replace_line("", 0);
    rl_redisplay();
}

/* ================================================================== */
/*  Commandes internes                                                 */
/* ================================================================== */

/* ---- exit --------------------------------------------------------- */
static int cmd_exit(int n, char *p[])
{
    (void)n; (void)p;
    printf("Au revoir !\n");
    /* Arrêter le serveur BEUIP s'il tourne */
    if (beuip_server_pid > 0) {
        kill(beuip_server_pid, SIGTERM);
        waitpid(beuip_server_pid, NULL, 0);
    }
    /* Sauvegarder l'historique */
    write_history(HISTORY_FILE);
    exit(0);
}

/* ---- cd ----------------------------------------------------------- */
static int cmd_cd(int n, char *p[])
{
    const char *dir = (n >= 2) ? p[1] : getenv("HOME");
    if (!dir) dir = "/";
    if (chdir(dir) < 0)
        perror("cd");
    return 0;
}

/* ---- pwd ---------------------------------------------------------- */
static int cmd_pwd(int n, char *p[])
{
    (void)n; (void)p;
    char buf[4096];
    if (!getcwd(buf, sizeof(buf)))
        perror("getcwd");
    else
        printf("%s\n", buf);
    return 0;
}

/* ---- vers --------------------------------------------------------- */
static int cmd_vers(int n, char *p[])
{
    (void)n; (void)p;
    printf("biceps version %s\n", BICEPS_VERSION);
    return 0;
}

/* ---- help --------------------------------------------------------- */
static int cmd_help(int n, char *p[])
{
    (void)n; (void)p;
    listeComInt();
    return 0;
}

/* ================================================================== */
/*  Commandes BEUIP                                                    */
/* ================================================================== */

/*
 * beuip start <pseudo>   – lance le serveur BEUIP en fils
 * beuip stop             – arrête le serveur BEUIP
 */
static int cmd_beuip(int n, char *p[])
{
    if (n < 2) {
        fprintf(stderr, "Usage: beuip start <pseudo> | beuip stop\n");
        return 1;
    }

    /* ---- beuip start <pseudo> ------------------------------------ */
    if (strcmp(p[1], "start") == 0) {
        if (n < 3) {
            fprintf(stderr, "Usage: beuip start <pseudo>\n");
            return 1;
        }
        if (beuip_server_pid > 0) {
            printf("[BEUIP] Serveur déjà actif (PID %d)\n",
                   (int)beuip_server_pid);
            return 0;
        }

        pid_t pid = fork();
        if (pid < 0) { perror("fork"); return 1; }

        if (pid == 0) {
            /* Fils : exécuter servbeuip */
            char *args[] = { "servbeuip", p[2], NULL };
            execvp("./servbeuip", args);
            /* Fallback si non trouvé dans le répertoire courant */
            execvp("servbeuip", args);
            perror("execvp servbeuip");
            exit(EXIT_FAILURE);
        }

        beuip_server_pid = pid;
        printf("[BEUIP] Serveur démarré (PID %d, pseudo : %s)\n",
               (int)pid, p[2]);
        return 0;
    }

    /* ---- beuip stop ---------------------------------------------- */
    if (strcmp(p[1], "stop") == 0) {
        if (beuip_server_pid <= 0) {
            printf("[BEUIP] Aucun serveur actif\n");
            return 0;
        }
        kill(beuip_server_pid, SIGTERM);
        waitpid(beuip_server_pid, NULL, 0);
        printf("[BEUIP] Serveur arrêté\n");
        beuip_server_pid = -1;
        return 0;
    }

    fprintf(stderr, "beuip: sous-commande inconnue '%s'\n", p[1]);
    return 1;
}

/*
 * Envoie une commande au serveur local et attend optionnellement une reponse.
 * wait_reply=1 : attend une reponse texte et l'affiche.
 */
static int send_local_recv(char code, const char *payload, int paylen,
                           int wait_reply)
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) { perror("socket"); return -1; }

    /* Timeout de reception : 1 seconde */
    struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    /* Bind sur un port ephemere pour recevoir la reponse */
    struct sockaddr_in local;
    memset(&local, 0, sizeof(local));
    local.sin_family      = AF_INET;
    local.sin_port        = 0;
    local.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    bind(sock, (struct sockaddr *)&local, sizeof(local));

    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family      = AF_INET;
    dest.sin_port        = htons(BEUIP_PORT);
    dest.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    int ret = beuip_send(sock, &dest, code, payload, paylen);

    if (wait_reply && ret >= 0) {
        char buf[4096];
        int  n = recvfrom(sock, buf, sizeof(buf) - 1, 0, NULL, NULL);
        if (n > 0) {
            buf[n] = '\0';
            printf("%s", buf);
            fflush(stdout);
        }
    }

    close(sock);
    return ret < 0 ? -1 : 0;
}

/*
 * mess list/liste         - affiche la liste des pseudos presents
 * mess <pseudo> <message> - envoie un message a un pseudo
 * mess all/tous <message> - envoie un message a tout le monde
 */
static int cmd_mess(int n, char *p[])
{
    if (n < 2) {
        fprintf(stderr,
                "Usage:\n"
                "  mess list\n"
                "  mess <pseudo> <message>\n"
                "  mess all <message>\n");
        return 1;
    }

    if (beuip_server_pid <= 0) {
        fprintf(stderr, "mess: serveur BEUIP non demarre"
                " (utilisez 'beuip start <pseudo>')\n");
        return 1;
    }

    /* ---- mess list / mess liste ---------------------------------- */
    if (strcmp(p[1], "list") == 0 || strcmp(p[1], "liste") == 0) {
        return send_local_recv(BEUIP_CODE_LIST, NULL, 0, 1);
    }

    /* ---- mess all / mess tous <message> -------------------------- */
    if (strcmp(p[1], "all") == 0 || strcmp(p[1], "tous") == 0) {
        if (n < 3) {
            fprintf(stderr, "mess all: message manquant\n");
            return 1;
        }
        char msg[BEUIP_MAXMSG] = {0};
        for (int i = 2; i < n; i++) {
            if (i > 2) strncat(msg, " ", sizeof(msg) - strlen(msg) - 1);
            strncat(msg, p[i], sizeof(msg) - strlen(msg) - 1);
        }
        return send_local_recv(BEUIP_CODE_ALL, msg, (int)strlen(msg), 0);
    }

    /* ---- mess <pseudo> <message> --------------------------------- */
    if (n < 3) {
        fprintf(stderr, "mess: message manquant\n");
        return 1;
    }

    char pseudo[BEUIP_MAXPSEUDO];
    strncpy(pseudo, p[1], BEUIP_MAXPSEUDO - 1);
    pseudo[BEUIP_MAXPSEUDO - 1] = '\0';

    char msg[BEUIP_MAXMSG] = {0};
    for (int i = 2; i < n; i++) {
        if (i > 2) strncat(msg, " ", sizeof(msg) - strlen(msg) - 1);
        strncat(msg, p[i], sizeof(msg) - strlen(msg) - 1);
    }

    /* Payload = pseudo\0message */
    int   plen    = (int)strlen(pseudo) + 1 + (int)strlen(msg);
    char *payload2 = malloc(plen + 1);
    if (!payload2) { perror("malloc"); return 1; }
    memcpy(payload2, pseudo, strlen(pseudo) + 1);
    memcpy(payload2 + strlen(pseudo) + 1, msg, strlen(msg));

    int ret = send_local_recv(BEUIP_CODE_SEND, payload2, plen, 0);
    free(payload2);
    return ret;
}

/* ================================================================== */
/*  majComInt – enregistrement de toutes les commandes internes        */
/* ================================================================== */
static void majComInt_local(void)
{
    ajouteCom("exit",  cmd_exit);
    ajouteCom("cd",    cmd_cd);
    ajouteCom("pwd",   cmd_pwd);
    ajouteCom("vers",  cmd_vers);
    ajouteCom("help",  cmd_help);
    ajouteCom("beuip", cmd_beuip);
    ajouteCom("mess",  cmd_mess);
}

/* ================================================================== */
/*  Fabrication du prompt                                              */
/* ================================================================== */
static char *make_prompt(void)
{
    char *user = getenv("USER");
    char  host[256] = "localhost";
    gethostname(host, sizeof(host) - 1);

    uid_t uid  = getuid();
    char  suf  = (uid == 0) ? '#' : '$';

    /* format : "user@host$ " */
    size_t len = (user ? strlen(user) : 7) + 1 + strlen(host) + 3;
    char  *prompt = malloc(len + 1);
    if (!prompt) return strdup("biceps$ ");

    snprintf(prompt, len + 1, "%s@%s%c ",
             user ? user : "nobody", host, suf);
    return prompt;
}

/* ================================================================== */
/*  Exécution d'une ligne simple (sans ';')                           */
/* ================================================================== */
static void exec_ligne(char *ligne)
{
    int n = analyseCom(ligne);
    if (n <= 0) return;

#ifdef TRACE
    fprintf(stderr, "[TRACE] %d mots : ", NMots);
    for (int i = 0; i < NMots; i++)
        fprintf(stderr, "'%s' ", Mots[i]);
    fprintf(stderr, "\n");
#endif

    if (!execComInt(NMots, Mots))
        execComExt(Mots);
}

/* ================================================================== */
/*  main                                                               */
/* ================================================================== */
int main(void)
{
    /* Ignorer SIGINT (Ctrl-C) */
    signal(SIGINT, sigint_handler);

    /* Enregistrer les commandes internes */
    majComInt_local();

#ifdef TRACE
    listeComInt();
#endif

    /* Charger l'historique */
    using_history();
    read_history(HISTORY_FILE);

    /* Boucle principale */
    char *prompt = make_prompt();

    while (1) {
        char *ligne = readline(prompt);

        /* Ctrl-D → EOF → sortie propre */
        if (!ligne) {
            printf("\nAu revoir !\n");
            if (beuip_server_pid > 0) {
                kill(beuip_server_pid, SIGTERM);
                waitpid(beuip_server_pid, NULL, 0);
            }
            write_history(HISTORY_FILE);
            break;
        }

        /* Ignorer les lignes vides */
        if (*ligne == '\0') { free(ligne); continue; }

        /* Ajouter à l'historique (pas de doublons consécutifs) */
        HIST_ENTRY *last = history_get(history_length);
        if (!last || strcmp(last->line, ligne) != 0)
            add_history(ligne);

        /* Découper sur ';' pour les commandes séquentielles */
        char *saveptr = NULL;
        char *copie   = strdup(ligne);
        char *token   = strtok_r(copie, ";", &saveptr);
        while (token) {
            /* Sauter les espaces en tête */
            while (*token == ' ' || *token == '\t') token++;
            if (*token != '\0')
                exec_ligne(token);
            token = strtok_r(NULL, ";", &saveptr);
        }
        free(copie);
        free(ligne);
    }

    free(prompt);
    return 0;
}
