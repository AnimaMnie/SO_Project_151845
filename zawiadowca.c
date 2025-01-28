//zawiadowca.c


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <sys/select.h>
#include <termios.h>

static volatile sig_atomic_t stop = 0;

// Obsługa sygnału SIGINT
void sigint_handler(int sig) {
    (void)sig;
    stop = 1;
    printf("[ZAWIADOWCA] Otrzymano SIGINT, koncze.\n");
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uzycie: %s <PID_KIEROWNIKA>\n", argv[0]);
        return 1;
    }
    pid_t kierownik_pid = atoi(argv[1]);
    if (kierownik_pid <= 0) {
        fprintf(stderr, "Bledny PID.\n");
        return 1;
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    printf("[ZAWIADOWCA] Start. Kierownik PID=%d\n", kierownik_pid);
    printf("Podaj '1'(przyspieszenie), '2'(blokada), 'q'(koniec)\n");

    // Ustawienie stdin w tryb niekanoniczny
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt); // Zapisujemy stary stan
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO); // Wyłączamy tryb kanoniczny i echo
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    while (!stop) {
        char buf[32];

        // Sprawdzenie, czy proces kierownika wciąż działa
        if (kill(kierownik_pid, 0) == -1) {
            if (errno == ESRCH) {
                // Proces kierownika nie istnieje
                printf("[ZAWIADOWCA] Kierownik zakonczyl prace. Koncze.\n");
                break;
            }
        }

        // Użycie select() do nieblokującego odczytu z stdin
        fd_set set;
        struct timeval timeout;
        FD_ZERO(&set);
        FD_SET(STDIN_FILENO, &set);

        timeout.tv_sec = 1; // Timeout 1 sekunda
        timeout.tv_usec = 0;

        int rv = select(STDIN_FILENO + 1, &set, NULL, NULL, &timeout);

        if (rv == -1) {
            perror("select");
            break;
        } else if (rv == 0) {
            // Timeout - sprawdzamy ponownie, czy kierownik działa
            continue;
        } else {
            // Dostępne dane wejściowe
            // Odczyt znaku
            ssize_t num_read = read(STDIN_FILENO, buf, 1);
            if (num_read <= 0) {
                if (stop) break; // Zatrzymanie po SIGINT
                continue;
            }

            char input = buf[0];

            if (input == '1') {
                if (kill(kierownik_pid, SIGUSR1) == -1) {
                    perror("kill(SIGUSR1)");
                } else {
                    printf("Wyslano SIGUSR1 (przyspieszenie).\n");
                }
            } else if (input == '2') {
                if (kill(kierownik_pid, SIGUSR2) == -1) {
                    perror("kill(SIGUSR2)");
                } else {
                    printf("Wyslano SIGUSR2 (blokada).\n");
                }
            } else if (input == 'q') {
                printf("[ZAWIADOWCA] Koniec.\n");
                stop = 1;
            } else {
                printf("Nieznana komenda.\n");
            }
        }
    }

    // Przywracamy stary stan terminala
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

    return 0;
}