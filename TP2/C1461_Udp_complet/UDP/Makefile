# ============================================================
# Makefile – biceps v2 + gescom + creme + servbeuip
# ============================================================

CC      = gcc
CFLAGS  = -Wall -Wextra -g
# Décommenter pour activer les traces :
# CFLAGS += -DTRACE

LDFLAGS_BICEPS   = -lreadline -lgescom -lcreme -L.
LDFLAGS_SERV     = -lcreme -L.

# Librairies statiques produites
LIB_GESCOM = libgescom.a
LIB_CREME  = libcreme.a

.PHONY: all clean

# ---- Cibles principales -------------------------------------
all: $(LIB_GESCOM) $(LIB_CREME) biceps servbeuip

# ---- libgescom.a --------------------------------------------
$(LIB_GESCOM): gescom.o
	ar rcs $@ $^

gescom.o: gescom.c gescom.h
	$(CC) $(CFLAGS) -c $< -o $@

# ---- libcreme.a ---------------------------------------------
$(LIB_CREME): creme.o
	ar rcs $@ $^

creme.o: creme.c creme.h
	$(CC) $(CFLAGS) -c $< -o $@

# ---- biceps -------------------------------------------------
biceps: biceps.o $(LIB_GESCOM) $(LIB_CREME)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS_BICEPS)

biceps.o: biceps.c gescom.h creme.h
	$(CC) $(CFLAGS) -c $< -o $@

# ---- servbeuip ----------------------------------------------
servbeuip: servbeuip.o $(LIB_CREME)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS_SERV)

servbeuip.o: servbeuip.c creme.h
	$(CC) $(CFLAGS) -c $< -o $@

# ---- Nettoyage ----------------------------------------------
clean:
	rm -f *.o $(LIB_GESCOM) $(LIB_CREME) biceps servbeuip
