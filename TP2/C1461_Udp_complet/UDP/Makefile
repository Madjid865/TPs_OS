CC      = gcc
CFLAGS  = -Wall -Wextra -g
LDFLAGS = -lreadline

# Pour activer les traces : make trace

# ---------- Cibles principales ----------
all: biceps servbeuip clibeuip

# ---------- biceps (shell) ----------
biceps: biceps.o gescom.o creme.o
	$(CC) -o biceps biceps.o gescom.o creme.o $(LDFLAGS)

biceps.o: biceps.c gescom.h creme.h
	$(CC) $(CFLAGS) -c biceps.c

gescom.o: gescom.c gescom.h creme.h
	$(CC) $(CFLAGS) -c gescom.c

creme.o: creme.c creme.h
	$(CC) $(CFLAGS) -c creme.c

# ---------- servbeuip (serveur BEUIP) ----------
servbeuip: servbeuip.o
	$(CC) -o servbeuip servbeuip.o

servbeuip.o: servbeuip.c
	$(CC) $(CFLAGS) -c servbeuip.c

# ---------- clibeuip (client BEUIP) ----------
clibeuip: clibeuip.o
	$(CC) -o clibeuip clibeuip.o

clibeuip.o: clibeuip.c
	$(CC) $(CFLAGS) -c clibeuip.c

# ---------- compilation avec traces ----------
trace: CFLAGS += -DTRACE
trace: all

# ---------- nettoyage ----------
clean:
	rm -f *.o biceps servbeuip clibeuip

.PHONY: all trace clean
