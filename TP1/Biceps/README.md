# Biceps Shell (v1.0)

Biceps est un interpreteur de commandes (Shell) developpe en C dans le cadre du TP de systemes d'exploitation. L'objectif est de concevoir un shell fonctionnel capable de gerer des processus, des commandes internes et une interface interactive.

## Objectifs du projet
* Gestion des processus : Creation de processus fils via fork() et execution de programmes via execvp().
* Analyse de commandes : Decoupage de chaines de caracteres (parsing) pour separer les commandes et leurs arguments.
* Commandes Internes : Implementation de commandes gerees directement par le shell (cd, pwd, exit, vers).
* Sequentialite : Support de l'execution de plusieurs commandes separees par des points-virgules (;).
* Interface utilisateur : Utilisation de la bibliotheque readline pour un prompt dynamique et un historique des commandes.

## Fonctionnalites
* Prompt personnalise : format utilisateur@machine$.
* Commandes externes : Execution de programmes systemes (ls, cat, grep, etc.).
* Navigation : Commande cd fonctionnelle pour changer de repertoire de travail.
* Historique permanent : Sauvegarde des commandes dans le fichier .biceps_history.
* Gestion des signaux : Ignorance du signal SIGINT (Ctrl+C) pour preserver l'execution du shell.
* Sortie propre : Gestion du signal EOF (Ctrl+D) pour quitter le programme.

## Structure du code
* biceps.c : Contient la boucle principale et la gestion de l'interface utilisateur.
* gescom.c : Regroupe la logique d'analyse (parsing) et les fonctions d'execution.
* gescom.h : Fichier d'en-tete definissant les prototypes et les structures de la librairie.
* Makefile : Automatisation de la compilation et de l'edition de liens.

## Installation et Utilisation

1. Compilation du projet :
   make

2. Lancement du shell :
   ./biceps

3. Exemple de commande sequentielle :
   ls -l ; pwd ; vers

4. Quitter le programme :
   Taper "exit" ou utiliser la combinaison de touches Ctrl+D.

## Limitations (Version 1.0)
Cette version ne gere pas encore les redirections (>, <), les tubes (pipes |) ainsi que les variables d'environnement complexes.
