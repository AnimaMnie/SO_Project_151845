//header.h

#ifndef HEADER_H
#define HEADER_H

#include <time.h>

#define FTOK_PATH "kierownik.c"
#define FTOK_ID 'X'

// Enumy dla semaforów
enum {
    SEM_DATA = 0,
    SEM_DOOR_BAGAZ,
    SEM_DOOR_ROWER,
    SEM_COUNT
};

// ANSI kody kolorów
#define BLUE    "\033[1;34m"
#define GREEN   "\033[32m"
#define ORANGE  "\033[38;5;214m"
#define WHITE   "\033[37m"
#define RESET   "\033[0m"

// Struktura danych współdzielonych
typedef struct {
    int N, P, R, T, Ti, total_pass;
    int pociag_na_stacji;
    int liczba_pasazerow;
    int liczba_rowerow;
    int pozostali_pasazerowie;
    int blokada_wsiadania;
    int current_train_id;
    time_t next_free_time[100];
    int last_departed_train; 
} shared_data_t;

#endif // HEADER_H