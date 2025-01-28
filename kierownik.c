//kierownik.c 
 

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <errno.h>
#include <string.h>

#include "header.h"

static int shmid = -1;
static int semid = -1;
static shared_data_t *shm = (void*)-1;

static volatile sig_atomic_t stop_program = 0;
static volatile sig_atomic_t przyspiesz_odjazd = 0;

void sig_handler(int sig){
    if(sig == SIGINT){
        fprintf(stderr, "\n[KIEROWNIK] Otrzymano SIGINT, koncze...\n");
        stop_program = 1;
    } else if(sig == SIGUSR1){
        przyspiesz_odjazd = 1;
    } else if(sig == SIGUSR2){
        shm->blokada_wsiadania = 1;
        fprintf(stderr, "[KIEROWNIK] Blokada wsiadania!\n");
    }
}

int sem_op(int sem_id, int sem_num, int op){
    struct sembuf sb;
    sb.sem_num = sem_num;
    sb.sem_op = op;
    sb.sem_flg = 0;
    return semop(sem_id, &sb, 1);
}
int sem_lock(int sem_id, int sem_num){ return sem_op(sem_id, sem_num, -1); }
int sem_unlock(int sem_id, int sem_num){ return sem_op(sem_id, sem_num, 1); }

void cleanup(void){
    if(shm != (void*)-1){
        shmdt(shm);
        shm = (void*)-1;
    }
    if(shmid != -1){
        shmctl(shmid, IPC_RMID, NULL);
        shmid = -1;
    }
    if(semid != -1){
        semctl(semid, 0, IPC_RMID);
        semid = -1;
    }
}

// Funkcja zwracająca indeks wolnego pociągu
int get_free_train(void)
{
    while(!stop_program){
        time_t now = time(NULL);
        for(int i = 0; i < shm->N; i++){
            if(now >= shm->next_free_time[i]){
                return i;
            }
        }
        time_t min_t = 0;
        for(int i = 0; i < shm->N; i++){
            if(min_t == 0 || shm->next_free_time[i] < min_t){
                min_t = shm->next_free_time[i];
            }
        }
        time_t wait_s = (min_t > now) ? (min_t - now) : 1;
        while(wait_s > 0 && !stop_program){
            wait_s = sleep(wait_s);
        }
    }
    return -1;
}

int main(int argc, char *argv[])
{
    if(argc < 7){
        fprintf(stderr, "Uzycie: %s N P R T Ti TOT\n", argv[0]);
        return 1;
    }
    int N = atoi(argv[1]);
    int P = atoi(argv[2]);
    int R = atoi(argv[3]);
    int T = atoi(argv[4]);
    int Ti = atoi(argv[5]);
    int TOT = atoi(argv[6]);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sig_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGUSR2, &sa, NULL);

    key_t key = ftok(FTOK_PATH, FTOK_ID);
    if(key == -1){
        perror("ftok");
        return 1;
    }

    shmid = shmget(key, sizeof(shared_data_t), IPC_CREAT | IPC_EXCL | 0600);
    if(shmid == -1){
        perror("shmget");
        return 1;
    }
    shm = (shared_data_t*)shmat(shmid, NULL, 0);
    if((void*)shm == (void*)-1){
        perror("shmat");
        shmctl(shmid, IPC_RMID, NULL);
        return 1;
    }

    // Inicjalizacja danych współdzielonych
    shm->N = N; shm->P = P; shm->R = R; shm->T = T; shm->Ti = Ti; shm->total_pass = TOT;
    shm->pociag_na_stacji = 0;
    shm->liczba_pasazerow = 0;
    shm->liczba_rowerow = 0;
    shm->pozostali_pasazerowie = TOT;
    shm->blokada_wsiadania = 0;
    shm->current_train_id = -1;
    for(int i = 0; i < N; i++){
        shm->next_free_time[i] = 0;
    }
    shm->last_departed_train = -1;

    semid = semget(key, SEM_COUNT, IPC_CREAT | IPC_EXCL | 0600);
    if(semid == -1){
        perror("semget");
        cleanup();
        return 1;
    }
    semctl(semid, SEM_DATA, SETVAL, 1);
    semctl(semid, SEM_DOOR_BAGAZ, SETVAL, 1);
    semctl(semid, SEM_DOOR_ROWER, SETVAL, 1);

    // Uruchamiamy procesy pasażerów
    for(int i = 0; i < TOT; i++){
        pid_t pid = fork();
        if(pid < 0){
            perror("fork(passenger)");
            continue;
        }
        if(pid == 0){
            char bike[2];
            bike[0] = (i % 5 == 0) ? '1' : '0';
            bike[1] = '\0';
            execl("./pasazerowie", "pasazerowie", bike, NULL);
            perror("execl(pasazerowie)");
            _exit(1);
        }
    }

    printf(BLUE "[KIEROWNIK] Start: N=%d,P=%d,R=%d,T=%d,Ti=%d,TOT=%d\n" RESET,
           N, P, R, T, Ti, TOT);

    while(!stop_program){
        // Sprawdzamy, ilu pasażerów pozostało
        sem_lock(semid, SEM_DATA);
        int left = shm->pozostali_pasazerowie;
        if(left <= 0) {
            printf(BLUE "[KIEROWNIK] Wszyscy pasażerowie obsłużeni. Kończę.\n" RESET);
            shm->blokada_wsiadania = 1; // Informujemy pasażerów, że nie mogą wchodzić
            sem_unlock(semid, SEM_DATA);
            break; // wszyscy obsłużeni
        }
        sem_unlock(semid, SEM_DATA);

        // Jeżeli pociąg już stoi na stacji, czekamy
        sem_lock(semid, SEM_DATA);
        int st = shm->pociag_na_stacji;
        sem_unlock(semid, SEM_DATA);
        if(st == 1){
            while(!stop_program){
                sleep(1);
                sem_lock(semid, SEM_DATA);
                if(shm->pociag_na_stacji == 0){
                    sem_unlock(semid, SEM_DATA);
                    break;
                }
                sem_unlock(semid, SEM_DATA);
            }
        }
        if(stop_program) break;

        // Pobieramy wolny pociąg
        int train_i = get_free_train();
        if(train_i < 0) break;

        sem_lock(semid, SEM_DATA);
        shm->pociag_na_stacji = 1;
        shm->current_train_id = train_i;
        printf(BLUE "[KIEROWNIK] Pociąg %d wjechał na stację.\n" RESET, train_i + 1);
        sem_unlock(semid, SEM_DATA);

        // Czekamy T sekund lub do sygnału przyspieszenia odjazdu
        przyspiesz_odjazd = 0;
        int slp = T;
        while(slp > 0 && !stop_program && !przyspiesz_odjazd){
            slp = sleep(slp);
        }
        if(stop_program) break;

        // Odjazd pociągu
        sem_lock(semid, SEM_DATA);
        int pass = shm->liczba_pasazerow;
        int row = shm->liczba_rowerow;
        printf(BLUE "[KIEROWNIK] Pociąg %d odjeżdża. Zajęte: %d (rowery: %d)\n" RESET,
               train_i + 1, pass, row);

        shm->pociag_na_stacji = 0;
        shm->last_departed_train = train_i; // Sygnał dla pasażerów
        shm->current_train_id = -1;
        shm->liczba_pasazerow = 0;
        shm->liczba_rowerow = 0;
        shm->next_free_time[train_i] = time(NULL) + shm->Ti;
        sem_unlock(semid, SEM_DATA);
    }

    printf(BLUE "[KIEROWNIK] Koniec pracy, sprzątam.\n" RESET);
    // Czekamy na procesy pasażerów
    while(wait(NULL) > 0){}
    cleanup();
    return 0;
}