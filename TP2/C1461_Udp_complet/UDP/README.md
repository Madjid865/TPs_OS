# TP2 - Protocole BEUIP / biceps v2

TP réseau système — Polytech Sorbonne 4A, mars 2026
Implémentation du protocole **BEUIP** (BEUI over IP) et intégration dans le shell **biceps**.

---

## Fichiers

| Fichier | Description |
|---|---|
| `servbeuip.c` | Serveur BEUIP — gère la table des pairs et les échanges réseau |
| `clibeuip.c` | Client de test manuel (codes 3/4/5) |
| `creme.h/c` | Librairie réseau — fonctions start/stop/liste/mess |
| `gescom.h/c` | Librairie shell — parsing et commandes internes |
| `biceps.c` | Shell interactif avec historique |

---

## Compilation

```bash
make          # compilation normale
make trace    # avec messages de debug (-DTRACE)
make clean    # nettoyage
```

---

## Utilisation

```bash
# Dans le shell biceps :
./biceps

biceps$ beuip start <pseudo>       # rejoindre le réseau
biceps$ mess liste                 # voir les pairs connectés
biceps$ mess <pseudo> <message>    # envoyer un message à quelqu'un
biceps$ mess tous <message>        # envoyer à tout le monde
biceps$ beuip stop                 # quitter le réseau
biceps$ exit                       # quitter biceps
```

---

## Réseau

- Port UDP : **9998**
- Broadcast : **192.168.88.255**
- Réseau TP : SSID `TPOSUSER`
