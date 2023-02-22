#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include "green_pass.h"
#include "addresses.h"

#define THREAD_POOL_SIZE 10

volatile sig_atomic_t isRunning = 1;

void * handle_connection(void * arg);
void sigint_handler(int sig);

int main() {
    int serverG_sock, client_sock;
    struct sockaddr_in serverG_address, client_address;
    socklen_t client_address_length;
    pthread_t thread_pool[THREAD_POOL_SIZE];

    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    // Creazione del socket per il serverG
    serverG_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (serverG_sock == -1) {
        perror("Errore nella creazione del socket");
        exit(1);
    }

    // binding
    memset(&serverG_address, 0, sizeof(serverG_address));
    serverG_address.sin_family = AF_INET;
    serverG_address.sin_addr.s_addr = htonl(INADDR_ANY);
    serverG_address.sin_port = htons(SERVERG_PORT);
    if (bind(serverG_sock, (struct sockaddr *) &serverG_address, sizeof(serverG_address)) == -1) {
        perror("Errore nel binding");
        exit(EXIT_FAILURE);
    }

    // In ascolto
    if (listen(serverG_sock, SOMAXCONN) == -1) {
        perror("Errore nel listen");
        exit(EXIT_FAILURE);
    }

    // initialize thread pool
    for (int i = 0; i < THREAD_POOL_SIZE; i++) {
        if (pthread_create(&thread_pool[i], NULL, handle_connection, NULL) != 0) {
            perror("pthread_create");
            exit(1);
        }
    }

    // wait for connections and assing to threads in pool
    while (isRunning) {
        client_address_length = sizeof (client_address);
        client_sock = accept(serverG_sock, (struct sockaddr *) &client_address, &client_address_length);
        if (client_sock == -1) {
            perror("Errore nell'accept"); // output da cambiare
            continue;
        }

        // find first available thread in pool
        int thread_index = -1;
        for (int i = 0; i < THREAD_POOL_SIZE; i++) {
            if (pthread_kill(thread_pool[i], 0) == ESRCH) {
                // thread is available
                thread_index = i;
                break;
            }
        }

        if (thread_index == -1) {
            // no threads available, close connection
            close(client_sock);
        } else {
            // pass client socket to thread
            int *client_sock_ptr = malloc(sizeof(int));
            *client_sock_ptr = client_sock;
            if (pthread_create(&thread_pool[thread_index], NULL, handle_connection, client_sock_ptr) != 0) {
                perror("Errore nella creazione del thread");
                break;
            } else {
                // detach thread so it can run in the background
                if (pthread_detach(thread_pool[thread_index]) != 0) {
                    perror("Errore nel detach del thread");
                    break;
                }
            }
        }
    }

    return 0;
}

void * handle_connection(void * arg) {
    // Get client socket
    int client_sock;
    if (arg == NULL) {
        // handle error
        return NULL;
    } else {
        client_sock = *((int *) arg);
        free(arg);
    }

    // Ricevo dati dal client
    struct GreenPass green_pass;
    if (recv(client_sock, &green_pass, sizeof(green_pass), 0) == -1) {
        perror("Errore nella ricezione del green pass");
        close(client_sock);
        return NULL;
    }

    printf("%s\n", green_pass.tessera_sanitaria);

    // Connessione al serverV
    int serverV_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (serverV_sock == -1) {
        perror("Errore nella creazione del socket per il serverV");
        close(client_sock);
        return NULL;
    }

    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = inet_addr(LOCAL_HOST);
    server_address.sin_port = htons(SERVERV_PORT);

    if (connect(serverV_sock, (struct sockaddr *) &server_address, sizeof(server_address)) == -1) {
        perror("Errore durante la connessione al serverV");
        close(client_sock);
        close(serverV_sock);
        return NULL;
    }

    if (green_pass.service == CHECK_GP) {
        printf("Servizio richiesto: verifica di validità\n");
    } else if (green_pass.service == VALIDATION_GP) {
        printf("Servizio richiesto: validazione\n");
    } else {
        close(client_sock);
        close(serverV_sock);
        return NULL;
    }

    if (send(serverV_sock, &green_pass, sizeof(green_pass), 0) == -1) {
        perror("Errore nell'invio del green pass al serverV");
        close(client_sock);
        close(serverV_sock);
        return NULL;
    }

    int response;
    if (recv(serverV_sock, &response, sizeof(int), 0) == -1) {
        perror("Errore nella ricezione della risposta dal serverV");
        close(client_sock);
        close(serverV_sock);
        return NULL;
    }

    if (send(client_sock, &response, sizeof(int), 0) == -1) {
        perror("Errore nell'invio della risposta al client");
        close(client_sock);
        close(serverV_sock);
        return NULL;
    }

    // Chiusura dei socket
    close(serverV_sock);
    close(client_sock);
    return NULL;
}

void sigint_handler(int sig) {
    isRunning = 0;
}