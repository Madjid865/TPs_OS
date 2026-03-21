CC=gcc
CFLAGS=-Wall -g
LDFLAGS=-lreadline

biceps: biceps.o gescom.o
	$(CC) -o biceps biceps.o gescom.o $(LDFLAGS)

biceps.o: biceps.c gescom.h
	$(CC) $(CFLAGS) -c biceps.c

gescom.o: gescom.c gescom.h
	$(CC) $(CFLAGS) -c gescom.c

clean:
	rm -f *.o biceps
