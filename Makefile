CC = gcc
CFLAGS = -Wall -pthread

# Reguła domyślna
all: kierownik pasazerowie zawiadowca main

# Kompilacja pliku kierownik.c
kierownik: kierownik.c header.h
	$(CC) $(CFLAGS) -o kierownik kierownik.c

# Kompilacja pliku pasazerowie.c
pasazerowie: pasazerowie.c header.h
	$(CC) $(CFLAGS) -o pasazerowie pasazerowie.c

# Kompilacja pliku zawiadowca.c
zawiadowca: zawiadowca.c header.h
	$(CC) $(CFLAGS) -o zawiadowca zawiadowca.c

# Kompilacja pliku main.c
main: main.c header.h
	$(CC) $(CFLAGS) -o main main.c

# Usuwanie plików wykonywalnych
clean:
	rm -f kierownik pasazerowie zawiadowca main
