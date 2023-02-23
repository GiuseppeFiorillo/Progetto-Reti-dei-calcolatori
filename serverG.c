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
    /* Crea i file descriptor per le socket */
    int serverG_sock, client_sock;
    /* Crea le struct `sockaddr_in` per stabilire le connessioni */
    struct sockaddr_in serverG_address, client_address;
    socklen_t client_address_length;
    /* Crea un array utilizzato per memorizzare un pool di thread che possono
     * essere utilizzati per eseguire operazioni in parallelo */
    pthread_t thread_pool[THREAD_POOL_SIZE];
    /* Imposta un gestore di segnali per il segnale SIGINT */
    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    /* Creazione della socket per il serverG */
    serverG_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (serverG_sock == -1) {
        perror("Errore nella creazione del socket");
        exit(1);
    }

    /* Imposta tutti i byte di `serverG_address` a 0 */
    memset(&serverG_address, 0, sizeof(serverG_address));
    /* Inizializza tutti i campi della struct `sockaddr_in` */
    serverG_address.sin_family = AF_INET; // utilizza un protocollo IPv4
    serverG_address.sin_addr.s_addr = htonl(INADDR_ANY); // ascolta su tutte le interfacce di rete disponibili
    serverG_address.sin_port = htons(SERVERG_PORT); // si mette in ascolto sulla porta 8100
    /* Esegue la bind per impostare tutti i campi della struct `sockaddr_in` */
    if (bind(serverG_sock, (struct sockaddr *) &serverG_address, sizeof(serverG_address)) == -1) {
        perror("Errore nel binding");
        exit(EXIT_FAILURE);
    }
    /* Si mette in ascolto per le connessioni */
    if (listen(serverG_sock, SOMAXCONN) == -1) {
        perror("Errore nel listen");
        exit(EXIT_FAILURE);
    }
    /* Vengono creati i thread presenti all'interno del pool */
    for (int i = 0; i < THREAD_POOL_SIZE; i++) {
        if (pthread_create(&thread_pool[i], NULL, handle_connection, NULL) != 0) {
            perror("pthread_create");
            exit(1);
        }
    }

    while (isRunning) {
        client_address_length = sizeof (client_address);
        /* Accetta le connessioni */
        client_sock = accept(serverG_sock, (struct sockaddr *) &client_address, &client_address_length);
        if (client_sock == -1) {
            perror("Errore nell'accept"); // output da cambiare
            continue;
        }
        /* Trova il primo thread disponibile all'interno del pool */
        int thread_index = -1;
        for (int i = 0; i < THREAD_POOL_SIZE; i++) {
            if (pthread_kill(thread_pool[i], 0) == ESRCH) {
                thread_index = i;
                break;
            }
        }
        if (thread_index == -1) {
            /* Se non sono disponibili thread, chiude la connessione */
            close(client_sock);
        } else {
            /* Se sono disponibili, passa la socket del client al thread */
            int *client_sock_ptr = malloc(sizeof(int));
            *client_sock_ptr = client_sock;
            if (pthread_create(&thread_pool[thread_index], NULL, handle_connection, client_sock_ptr) != 0) {
                perror("Errore nella creazione del thread");
                break;
            } else {
                /* Stacca il thread in modo che possa essere eseguito in background */
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
    int client_sock;
    if (arg == NULL) {
        // handle error
        return NULL;
    } else {
        client_sock = *((int *) arg);
        free(arg);
    }

    /* Riceve la struct `GreenPass` dalla `client_sock` */
    struct GreenPass green_pass;
    if (recv(client_sock, &green_pass, sizeof(green_pass), 0) == -1) {
        perror("Errore nella ricezione del green pass");
        close(client_sock);
        return NULL;
    }

    printf("%s\n", green_pass.tessera_sanitaria);

    /* Creazione della socket per il server V */
    int serverV_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (serverV_sock == -1) {
        perror("Errore nella creazione del socket per il serverV");
        close(client_sock);
        return NULL;
    }
    /* Crea la struct `sockaddr_in` per stabilire la connessione */
    struct sockaddr_in server_address;
    /* Imposta tutti i byte di `server_address` a 0 */
    memset(&server_address, 0, sizeof(server_address));
    /* Inizializza tutti i campi della struct `sockaddr_in` */
    server_address.sin_family = AF_INET; // utilizza un protocollo IPv4
    server_address.sin_addr.s_addr = inet_addr(LOCAL_HOST); // ascolta sull'interfaccia locale di rete
    server_address.sin_port = htons(SERVERV_PORT); // si mette in ascolto sulla porta 8890

    /* Connessione con il server V */
    if (connect(serverV_sock, (struct sockaddr *) &server_address, sizeof(server_address)) == -1) {
        perror("Errore durante la connessione al serverV");
        close(client_sock); // chiude la socket del client
        close(serverV_sock); // chiude la socket del server V
        return NULL;
    }

    /* Controllo del servizio che è stato richiesto al server */
    if (green_pass.service == CHECK_GP) {
        printf("Servizio richiesto: verifica di validità\n");
    } else if (green_pass.service == VALIDATION_GP) {
        printf("Servizio richiesto: validazione\n");
    } else {
        close(client_sock); // chiude la socket del client
        close(serverV_sock); // chiude la socket del server
        return NULL;
    }

    /* Invia la struct `GreenPass` al server V */
    if (send(serverV_sock, &green_pass, sizeof(green_pass), 0) == -1) {
        perror("Errore nell'invio del green pass al serverV");
        close(client_sock); // chiude la socket del client
        close(serverV_sock); // chiude la socket del server
        return NULL;
    }

    int response;
    /* Si mette in attesa della risposta dal server V */
    if (recv(serverV_sock, &response, sizeof(int), 0) == -1) {
        perror("Errore nella ricezione della risposta dal serverV");
        close(client_sock); // chiude la socket del client
        close(serverV_sock); // chiude la socket del server
        return NULL;
    }

    /* Invia la risposta ricevuta dal server al client */
    if (send(client_sock, &response, sizeof(int), 0) == -1) {
        perror("Errore nell'invio della risposta al client");
        close(client_sock); // chiude la socket del client
        close(serverV_sock); // chiude la socket del server
        return NULL;
    }

    close(serverV_sock); // chiude la socket del server
    close(client_sock); // chiude la socket del client
    return NULL;
}

void sigint_handler(int sig) {
    isRunning = 0;
}