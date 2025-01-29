CC = gcc
CFLAGS = -Wall -pthread

all: kierownik pasazerowie zawiadowca main

kierownik: kierownik.c header.h
	$(CC) $(CFLAGS) -o kierownik kierownik.c

pasazerowie: pasazerowie.c header.h
	$(CC) $(CFLAGS) -o pasazerowie pasazerowie.c

zawiadowca: zawiadowca.c header.h
	$(CC) $(CFLAGS) -o zawiadowca zawiadowca.c

main: main.c header.h
	$(CC) $(CFLAGS) -o main main.c

clean:
	rm -f kierownik pasazerowie zawiadowca main
