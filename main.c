/************************************************************
 Projekt Temat-4 Pociągi z rowerami 
 Szymon Basta alb.151845
 ************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <string.h>

static pid_t children[2]; // [0] - kierownik, [1] - zawiadowca
static int child_count = 0;

// Obsługa SIGINT
void sigint_handler(int sig)
{
    (void)sig;
    printf("\n[MAIN] Otrzymano SIGINT, zabijam procesy potomne...\n");

    // Zabijamy procesy potomne
    for (int i = 0; i < child_count; i++) {
        if (children[i] > 0) {
            kill(children[i], SIGINT);
        }
    }

    // Czekamy na ich zakończenie
    for (int i = 0; i < child_count; i++) {
        if (children[i] > 0) {
            waitpid(children[i], NULL, 0);
        }
    }

    printf("[MAIN] Wszystkie procesy potomne zakończone. Koniec.\n");
    exit(0);
}

int main(void)
{
    // Parametry
    int N          = 3;      //Liczba pociągów dostępnych na stacji
    int P          = 100;    //Maksymalna liczba pasażerów
    int R          = 0;      //Maksymalna liczba osób z rowerami
    int T          = 5;      //Czas jaki pociąg przebywa na stacji
    int Ti         = 30;     //Czas powrotu pociągu na stację
    int TOTAL_PASS = 20;     //Całkowita liczba pasażerów

    printf("[MAIN] N=%d, P=%d, R=%d, T=%d, Ti=%d, TOTAL_PASS=%d\n",
           N, P, R, T, Ti, TOTAL_PASS);

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    // Uruchamiamy kierownika
    pid_t pid_kier = fork();
    if (pid_kier < 0) {
        perror("fork(kierownik)");
        return 1;
    }
    if (pid_kier == 0) {
        char n_str[32], p_str[32], r_str[32], t_str[32], ti_str[32], tot_str[32];
        snprintf(n_str,  sizeof(n_str),  "%d", N);
        snprintf(p_str,  sizeof(p_str),  "%d", P);
        snprintf(r_str,  sizeof(r_str),  "%d", R);
        snprintf(t_str,  sizeof(t_str),  "%d", T);
        snprintf(ti_str, sizeof(ti_str), "%d", Ti);
        snprintf(tot_str,sizeof(tot_str),"%d", TOTAL_PASS);

        execl("./kierownik", "kierownik",
              n_str, p_str, r_str, t_str, ti_str, tot_str,
              (char*)NULL);
        perror("execl(kierownik)");
        _exit(1);
    }
    children[0] = pid_kier;
    child_count++;

    // Uruchamiamy zawiadowcę
    pid_t pid_zaw = fork();
    if (pid_zaw < 0) {
        perror("fork(zawiadowca)");
        return 1;
    }
    if (pid_zaw == 0) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", pid_kier);
        execl("./zawiadowca", "zawiadowca", buf, NULL);
        perror("execl(zawiadowca)");
        _exit(1);
    }
    children[1] = pid_zaw;
    child_count++;

    // Czekamy, aż oba się zakończą
    while (child_count > 0){
        pid_t ended = wait(NULL);
        if(ended > 0){
            for (int i = 0; i < child_count; i++){
                if (children[i] == ended){
                    for (int j = i; j < child_count - 1; j++){
                        children[j] = children[j + 1];
                    }
                    child_count--;
                    break;
                }
            }
        } else {
            break;
        }
    }
    printf("[MAIN] Koniec.\n");

    return 0;
}