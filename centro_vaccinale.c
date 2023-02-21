#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <ctype.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include "green_pass.h"

#define CENTER_PORT 8888
#define SERVERV_ADDRESS "127.0.0.1"
#define SERVERV_PORT 8890
#define THREAD_POOL_SIZE 10

volatile sig_atomic_t isRunning = 1;

void * handle_connection(void * arg);
void sigint_handler(int sig);


int main() {
    /* Imposto un gestore di segnale per il segnale SIGINT */
    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);
    /* Inizializza la socket per la connessione */
    int serverV_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (serverV_sock == -1) {
        perror("Errore nella creazione del socket per serverV");
        exit(EXIT_FAILURE);
    }
    /* Crea le struct `sockaddr_in` per stabilire la connessione */
    struct sockaddr_in serverV_address;
    /* Imposta tutti i byte di serverV_address a 0 */
    memset(&serverV_address, 0, sizeof(serverV_address));
    /* Inizializza tutti i campi della struct `sockaddr_in` */
    serverV_address.sin_family = AF_INET;
    serverV_address.sin_addr.s_addr = htonl(INADDR_ANY); // ascolta su tutte le interfacce di rete disponibili
    serverV_address.sin_port = htons(CENTER_PORT); // si mette in ascolto sulla porta 8888
    /* Esegue la bind per impostare tutti i campi della struct `sockaddr_in` */
    if (bind(serverV_sock, (struct sockaddr *) &serverV_address, sizeof(serverV_address)) == -1) {
        perror("Errore durante il binding del socket del centro vaccinale");
        exit(EXIT_FAILURE);
    }
    /* Si mette in ascolto per le connessioni */
    if (listen(serverV_sock, SOMAXCONN) == -1) {
        perror("Errore durante il listening delle connessioni");
        exit(EXIT_FAILURE);
    }
    /* Vengono creati i thread presenti all'interno del pool */
    pthread_t thread_pool[THREAD_POOL_SIZE];
    for (size_t i = 0; i < THREAD_POOL_SIZE; ++i) {
        if (pthread_create(&thread_pool[i], NULL, handle_connection, NULL) != 0) {
            perror("Errore nella creazione dei thread");
            exit(EXIT_FAILURE);
        }
    }
    /* Crea la struct `sockaddr_in` e la socket per il client */
    int client_sock;
    struct sockaddr_in client_address;
    socklen_t client_address_length;
    /* Aspetta la connessione e la assegna a un thread presente nel pool */
    while (isRunning) {
        client_address_length = sizeof(client_address);
        client_sock = accept(serverV_sock, (struct sockaddr *) &client_address, &client_address_length);
        if (client_sock == -1) {
            perror("Errore durante la connessione al client");
            continue;
        }
        /* Trova il primo thread disponibile nel pool */
        int thread_index = -1;
        for (int i = 0; i < THREAD_POOL_SIZE; ++i) {
            if (pthread_kill(thread_pool[i], 0) == ESRCH) {
                // thread is available
                thread_index = i;
                break;
            }
        }
        if (thread_index == -1) {
            /* Se non sono presenti thread, chiude la connessione */
            close(client_sock);
        } else {
            /* Se sono presenti, invia il socket del client al thread */
            int * client_sock_ptr = malloc(sizeof(int));
            *client_sock_ptr = client_sock;
            if (pthread_create(&thread_pool[thread_index], NULL, handle_connection, client_sock_ptr) != 0) {
                perror("Errore nella creazione del thread");
                free(client_sock_ptr);
                break;
            } else {
                /* Stacca il thread in modo che possa essere eseguito in background */
                if (pthread_detach(thread_pool[thread_index]) != 0) {
                    free(client_sock_ptr);
                    perror("Errore nel detach del thread");
                    break;
                }
            }
        }
    }
    printf("\nFine\n");
    /* Chiude la connessione con il socket del client */
    close(client_sock);
    return 0;
}

void * handle_connection(void * arg) {
    int client_sock; // get client socket
    if (arg == NULL) {
        return NULL;
    } else {
        client_sock = *((int *) arg);
        free(arg);
    }
    /* Riceve la struttura `GreenPass` dal `client_sock` */
    struct GreenPass green_pass;
    if(recv(client_sock, &green_pass, sizeof(green_pass), 0) == -1) {
        perror("Errore durante la ricezione della tessera sanitaria dal client");
        close(client_sock);
        return NULL;
    }
    printf("%s\n", green_pass.tessera_sanitaria);
    /* Imposta la data d'inizio e di fine del GreenPass */
    green_pass.valid_from = time(NULL);
    green_pass.valid_until = (time(NULL) + 30 * 24 * 60 * 60);
    green_pass.service = 0;
    /* Crea il file descriptor di un nuovo socket utilizzando il protocollo TCP (`SOCK_STREAM`).
     * In caso di errore, la funzione socket restituisce -1 */
    int serverV_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (serverV_sock == -1) {
        perror("Errore durante la creazione del socket per serverV");
        close(client_sock);
        return NULL;
    }
    // Crea un indirizzo per il socket del serverV
    struct sockaddr_in serverV_address;
    memset(&serverV_address, 0, sizeof(serverV_address));
    serverV_address.sin_family = AF_INET; // Il tipo di connessione, `AF_INET` indica l'utilizzo del protocollo IPv4
    serverV_address.sin_addr.s_addr = inet_addr(SERVERV_ADDRESS);
    serverV_address.sin_port = htons(SERVERV_PORT); /* Con `htons` convertiamo la costante `SERVERV_PORT`
    * in un formato a 16 bit */

    /* Connessione al `serverV_sock` */
    if (connect(serverV_sock, (struct sockaddr *) &serverV_address, sizeof(serverV_address)) == -1) {
        perror("Errore durante la connessione a serverV");
        close(client_sock);
        close(serverV_sock);
        return NULL;
    }
    /* Invio dei dati al server V */
    if (send(serverV_sock, &green_pass, sizeof(green_pass), 0) == -1) {
        perror("Errore durante l'invio del green pass a serverV");
        close(client_sock);
        close(serverV_sock);
        return NULL;
    }
    printf("Green pass inviato correttamente.\n");
    /* Invia la risposta al Server V */
    int response;
    if (recv(serverV_sock, &response, sizeof(int), 0) == -1) {
        perror("Errore durante la ricezione della risposta da servervV");
        close(client_sock);
        close(serverV_sock);
        return NULL;
    }
    printf("response: %d\n", response);
    if (response) {
        /* Nel caso in cui `response` valga 1, allora comunica l'avvenuta creazione del Green Pass al client */
        if (send(client_sock, "success", sizeof("success"), 0) == -1) {
            perror("Errore durante l'invio della risposta al client");
            close(client_sock);
            close(serverV_sock);
            return NULL;
        } else {
            /* Nel caso in cui invece valga 0, comunica che il Green Pass esiste al client */
            if (send(client_sock, "already exists", sizeof("already exists"), 0) == -1) {
                perror("Errore durante l'invio della risposta al client");
                close(client_sock);
                close(serverV_sock);
                return NULL;
            }
        }
    }
    // Chiusura dei socket
    close(client_sock);
    close(serverV_sock);
    return NULL;
}

void sigint_handler(int sig) {
    isRunning = 0;
}