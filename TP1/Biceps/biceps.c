#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <signal.h>
#include "gescom.h"

#define HIST_FILE ".biceps_history"

int main(int argc, char *argv[]) {
    char hostname[256], *user, *prompt, *ligne;
    char symbole;

    if (gethostname(hostname, sizeof(hostname)) != 0) strcpy(hostname, "unknown");
    user = getenv("USER");
    if (!user) user = "user";
    symbole = (getuid() == 0) ? '#' : '$';

    prompt = malloc(strlen(user) + strlen(hostname) + 4);
    sprintf(prompt, "%s@%s%c ", user, hostname, symbole);
    
    read_history(HIST_FILE);
    signal(SIGINT, SIG_IGN);
    majComInt();

    while (1) {
        ligne = readline(prompt);
        if (ligne == NULL) {
            printf("\nSortie de biceps... Bye !\n");
            break;
        }
        if (strlen(ligne) > 0) {
            add_history(ligne);
            char *ptr_ligne = ligne, *cmd;
            while ((cmd = strsep(&ptr_ligne, ";")) != NULL) {
                executeCommande(cmd);
            }
        }
        free(ligne);
    }

    write_history(HIST_FILE);
    free(prompt);
    return 0;
}
