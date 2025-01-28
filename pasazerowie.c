//pasazerowie.c

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <errno.h>
#include <string.h>

#include "header.h"

// Funkcja obsługująca semop z kontrolą błędów
int sem_op_checked(int semid, int semnum, int op)
{
    struct sembuf sb;
    sb.sem_num = semnum;
    sb.sem_op  = op;
    sb.sem_flg = 0;
    if(semop(semid, &sb, 1) == -1){
        if(errno == EIDRM || errno == EINVAL){
            // Semafor usunięty / invalid => kierownik zakończył
            return -2; 
        }
        // Inny błąd
        return -1; 
    }
    return 0;
}

// Funkcje pomocnicze
int sem_lock(int semid, int semnum){
    int ret = sem_op_checked(semid, semnum, -1);
    return ret;
}
int sem_unlock(int semid, int semnum){
    int ret = sem_op_checked(semid, semnum, 1);
    return ret;
}

int main(int argc, char *argv[])
{
    int mam_rower = 0;
    if(argc > 1){
        mam_rower = (atoi(argv[1]) != 0);
    }

    key_t key = ftok(FTOK_PATH, FTOK_ID);
    if(key == -1){
        perror("ftok pasazer");
        exit(1);
    }
    int shmid = shmget(key, 0, 0);
    if(shmid == -1){
        perror("shmget pasazer");
        exit(1);
    }
    shared_data_t *shm = (shared_data_t*) shmat(shmid, NULL, 0);
    if((void*)shm == (void*)-1){
        perror("shmat pasazer");
        exit(1);
    }
    int semid = semget(key, SEM_COUNT, 0);
    if(semid == -1){
        perror("semget pasazer");
        shmdt(shm);
        exit(1);
    }

    while(1){
        // Lock SEM_DATA
        int ret = sem_lock(semid, SEM_DATA);
        if(ret == -2){
            // Semafor usunięty => kierownik zakończył
            break;
        } else if(ret == -1){
            perror("sem_lock(DATA)");
            shmdt(shm);
            exit(1);
        }

        if(shm->blokada_wsiadania){
            printf("[PASAZER %d] Wsiadanie zablokowane - koncze.\n", getpid());
            sem_unlock(semid, SEM_DATA);
            break;
        }

        if(shm->pociag_na_stacji == 1){
            // Pociąg jest na stacji, sprawdzamy miejsce
            int ok = 0;
            if(!mam_rower && (shm->liczba_pasazerow < shm->P)){
                ok = 1;
            }
            if(mam_rower && (shm->liczba_pasazerow < shm->P && shm->liczba_rowerow < shm->R)){
                ok = 1;
            }
            if(ok){
                int train_id = shm->current_train_id;
                // Zwolnij SEM_DATA
                if(sem_unlock(semid, SEM_DATA) == -2){
                    break;
                }

                // Drzwi
                int door_sem = mam_rower ? SEM_DOOR_ROWER : SEM_DOOR_BAGAZ;
                ret = sem_lock(semid, door_sem);
                if(ret == -2) break;
                if(ret == -1){
                    perror("sem_lock(door)");
                    shmdt(shm);
                    exit(1);
                }

                ret = sem_lock(semid, SEM_DATA);
                if(ret == -2){
                    // Semafor usunięty
                    sem_unlock(semid, door_sem);
                    break;
                } else if(ret == -1){
                    perror("sem_lock(DATA2)");
                    sem_unlock(semid, door_sem);
                    shmdt(shm);
                    exit(1);
                }

                // Ponownie sprawdzamy
                if(shm->blokada_wsiadania){
                    printf(WHITE "[PASAZER %d] Wsiadanie zablokowane - koncze.\n" RESET, getpid());
                    sem_unlock(semid, SEM_DATA);
                    sem_unlock(semid, door_sem);
                    break;
                }
                if(shm->pociag_na_stacji == 0){
                    printf(WHITE "[PASAZER %d] Pociąg odjechał - próbuję dalej.\n" RESET, getpid());
                    sem_unlock(semid, SEM_DATA);
                    sem_unlock(semid, door_sem);
                    sleep(1);
                    continue;
                }
                if(!mam_rower && (shm->liczba_pasazerow >= shm->P)){
                    printf(WHITE "[PASAZER %d] Pociąg pełny - czekam na kolejny.\n" RESET, getpid());
                    sem_unlock(semid, SEM_DATA);
                    sem_unlock(semid, door_sem);
                    sleep(1);
                    continue;
                }
                if(mam_rower && ((shm->liczba_pasazerow >= shm->P) || (shm->liczba_rowerow >= shm->R))){
                    printf(WHITE "[PASAZER %d] Pociąg pełny - czekam na kolejny.\n" RESET, getpid());
                    sem_unlock(semid, SEM_DATA);
                    sem_unlock(semid, door_sem);
                    sleep(1);
                    continue;
                }

                // Wsiadamy
                shm->liczba_pasazerow++;
                shm->pozostali_pasazerowie--;
                if(mam_rower){
                    shm->liczba_rowerow++;
                    printf(GREEN "[PASAZER %d] Wsiadłem z rowerem. W pociągu: %d (rowery: %d)\n" RESET,
                           getpid(), shm->liczba_pasazerow, shm->liczba_rowerow);
                } else {
                    printf(GREEN "[PASAZER %d] Wsiadłem z bagażem. W pociągu: %d (rowery: %d)\n" RESET,
                           getpid(), shm->liczba_pasazerow, shm->liczba_rowerow);
                }
                sem_unlock(semid, SEM_DATA);
                sem_unlock(semid, door_sem);

                // Czekamy, aż last_departed_train == train_id
                while(1){
                    sleep(1);
                    ret = sem_lock(semid, SEM_DATA);
                    if(ret == -2){
                        break;
                    } else if(ret == -1){
                        perror("sem_lock wait");
                        shmdt(shm);
                        exit(1);
                    }

                    if(shm->last_departed_train == train_id){
                        printf(ORANGE "[PASAZER %d] Pociąg %d odjechał - koncze.\n" RESET,
                               getpid(), train_id + 1);
                        sem_unlock(semid, SEM_DATA);
                        shmdt(shm);
                        return 0;
                    }
                    sem_unlock(semid, SEM_DATA);
                }
                break;
            } else {
                // Pociąg jest, ale brak miejsca
                printf(WHITE "[PASAZER %d] Pociąg pełny - czekam na kolejny.\n" RESET, getpid());
                sem_unlock(semid, SEM_DATA);
            }
        } else {
            sem_unlock(semid, SEM_DATA);
        }
        sleep(1);
    }
    // Wyjście
    shmdt(shm);
  
    exit(0);
}