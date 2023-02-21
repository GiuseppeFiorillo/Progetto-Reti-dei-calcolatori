#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include "green_pass.h"

#define SERVERV_PORT 8890
#define THREAD_POOL_SIZE 10

volatile sig_atomic_t isRunning = 1;
pthread_mutex_t mutex;

void * handle_connection(void * arg);
void sigint_handler(int sig);

int main() {
    int serverV_sock, client_sock;
    struct sockaddr_in serverV_address, client_address;
    socklen_t client_address_length;
    pthread_t thread_pool[THREAD_POOL_SIZE];

    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    pthread_mutex_init(&mutex, NULL);

    // create server socket
    serverV_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (serverV_sock == -1) {
        perror("Errore nella creazione del socket");
        exit(EXIT_FAILURE);
    }

    //bind server socket to a port
    memset(&serverV_address, 0, sizeof(serverV_address));
    serverV_address.sin_family = AF_INET;
    serverV_address.sin_addr.s_addr = htonl(INADDR_ANY);
    serverV_address.sin_port = htons(SERVERV_PORT);
    if (bind(serverV_sock, (struct sockaddr *) &serverV_address, sizeof(serverV_address)) == -1) {
        perror("Errore nel binding");
        exit(EXIT_FAILURE);
    }

    if (listen(serverV_sock, SOMAXCONN) == -1) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    for (size_t i = 0; i < THREAD_POOL_SIZE; ++i) {
        if (pthread_create(&thread_pool[i], NULL, handle_connection, NULL) != 0) {
            perror("Errore nella creazione del thread");
            exit(EXIT_FAILURE);
        }
    }

    while (isRunning) {
        client_address_length = sizeof(client_address);
        client_sock = accept(serverV_sock, (struct sockaddr *) &client_address, &client_address_length);
        if (client_sock == -1) {
            perror("Errore durante la connessione di un nuovo client");
            continue;
        }

        // find first available thread in pool
        int thread_index = -1;
        for (size_t i = 0; i < THREAD_POOL_SIZE; ++i) {
            if (pthread_kill(thread_pool[i], 0) == ESRCH) {
                // thread is available
                thread_index = i;
                break;
            }
        }

        if (thread_index == -1) {
            // no threads availalbe, close connection
            close(client_sock);
        } else {
            // pass client socket to thread
            int * client_sock_ptr = malloc(sizeof(int));
            *client_sock_ptr = client_sock;
            if (pthread_create(&thread_pool[thread_index], NULL, handle_connection, client_sock_ptr) != 0) {
                perror("Errore nella creazione del thread");
                break;
            } else {
                // detach thread
                if (pthread_detach(thread_pool[thread_index]) != 0) {
                    perror("Errore nel detach");
                    break;
                }
            }
        }
    }

    pthread_mutex_destroy(&mutex);
    close(client_sock);

    return 0;
}


void * handle_connection(void * arg) {
    int client_sock;
    if (arg == NULL) {
        // handle error
        return NULL;
    } else {
        client_sock = *((int *) arg);
        free(arg);
    }

    // Receive the struct from the client
    struct GreenPass green_pass;

    int bytes_received = recv(client_sock, &green_pass, sizeof(green_pass), 0);
    if (bytes_received == -1) {
        perror("Errore nella ricezione del green pass dal client");
        close(client_sock);
        return NULL;
    } else if (bytes_received != sizeof(green_pass)) {
        close(client_sock);
        return NULL;
    }

    //lock mutex
    if (pthread_mutex_lock(&mutex) != 0) {
        perror("Errore nel lock del mutex");
        close(client_sock);
        return NULL;
    }

    // Open data file
    FILE *green_pass_file = fopen("green_pass.txt", "rb+");
    if (green_pass_file == NULL) {
        perror("Errore durante l'apertura del file");
        close(client_sock);
        pthread_mutex_unlock(&mutex);
        return NULL;
    }

    int response;
    // Scrivi il green pass su file
    switch (green_pass.service) {
        case 0: {
            rewind(green_pass_file);
            char buffer[TESSERA_LENGTH + 1];
            while (fgets(buffer, sizeof(buffer), green_pass_file)) {
                if (strncmp(buffer, green_pass.tessera_sanitaria, TESSERA_LENGTH) == 0) {
                    pthread_mutex_unlock(&mutex);
                    fflush(green_pass_file);
                    fclose(green_pass_file);

                    response = 0;
                    if (send(client_sock, &response, sizeof(int), 0) == -1) {
                        perror("Errore nell'invio della risposta al client");
                        close(client_sock);
                        return NULL;
                    }

                    close(client_sock);
                    return NULL;
                }
            }

            struct tm *from_ptr = localtime(&green_pass.valid_from);
            struct tm *until_ptr = localtime(&green_pass.valid_until);

            char valid_from[11];
            strftime(valid_from, sizeof(valid_from), "%d/%m/%Y", from_ptr);
            char valid_until[11];
            strftime(valid_until, sizeof(valid_until), "%d/%m/%Y", until_ptr);
            printf("%s : %s : %s\n", green_pass.tessera_sanitaria, valid_from, valid_until);
            fprintf(green_pass_file, "%s : %s : %s\n", green_pass.tessera_sanitaria, valid_from, valid_until);
            fflush(green_pass_file);
        }
    }
    pthread_mutex_unlock(&mutex);

    response = 1;
    if (send(client_sock, &response, sizeof(int), 0) == -1) {
        perror("Errore nell'invio della risposta al client");
        close(client_sock);
        return NULL;
    }

    fclose(green_pass_file);
    close(client_sock);
    return NULL;
}

void sigint_handler(int sig) {
    isRunning = 0;
}