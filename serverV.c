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

#define SERVERV_PORT 8890
#define THREAD_POOL_SIZE 10

volatile sig_atomic_t isRunning = 1;
pthread_mutex_t mutex;

void * handle_connection(void * arg);
void sigint_handler(int sig);

int main() {
    /* Crea i file descriptor per le socket */
    int serverV_sock, client_sock;
    /* Crea le struct `sockaddr_in` per stabilire la connessione */
    struct sockaddr_in serverV_address, client_address;
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
    /* Inizializza un thread di tipo mutex */
    pthread_mutex_init(&mutex, NULL);
    /* Inizializza la socket per la connessione */
    serverV_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (serverV_sock == -1) {
        perror("Errore nella creazione del socket");
        exit(EXIT_FAILURE);
    }
    /* Imposta tutti i byte di `serverV_address` a 0 */
    memset(&serverV_address, 0, sizeof(serverV_address));
    /* Inizializza tutti i campi della struct `sockaddr_in` */
    serverV_address.sin_family = AF_INET;
    serverV_address.sin_addr.s_addr = htonl(INADDR_ANY); // ascolta su tutte le interfacce di rete disponibili
    serverV_address.sin_port = htons(SERVERV_PORT); // si mette in ascolto sulla porta 8890
    /* Esegue la bind per impostare tutti i campi della struct `sockaddr_in` */
    if (bind(serverV_sock, (struct sockaddr *) &serverV_address, sizeof(serverV_address)) == -1) {
        perror("Errore nel binding");
        exit(EXIT_FAILURE);
    }
    /* Si mette in ascolto per le connessioni */
    if (listen(serverV_sock, SOMAXCONN) == -1) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    /* Vengono creati i thread presenti all'interno del pool */
    for (size_t i = 0; i < THREAD_POOL_SIZE; ++i) {
        if (pthread_create(&thread_pool[i], NULL, handle_connection, NULL) != 0) {
            perror("Errore nella creazione del thread");
            exit(EXIT_FAILURE);
        }
    }
    while (isRunning) {
        client_address_length = sizeof(client_address);
        /* Accetta le connessioni */
        client_sock = accept(serverV_sock, (struct sockaddr *) &client_address, &client_address_length);
        if (client_sock == -1) {
            perror("Errore durante la connessione di un nuovo client");
            continue;
        }
        /* Trova il primo thread disponibile all'interno del pool */
        int thread_index = -1;
        for (int i = 0; i < THREAD_POOL_SIZE; ++i) {
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
            int * client_sock_ptr = malloc(sizeof(int));
            *client_sock_ptr = client_sock;
            if (pthread_create(&thread_pool[thread_index], NULL, handle_connection, client_sock_ptr) != 0) {
                perror("Errore nella creazione del thread");
                break;
            } else {
                /* Stacca il thread in modo che possa essere eseguito in background */
                if (pthread_detach(thread_pool[thread_index]) != 0) {
                    perror("Errore nel detach");
                    break;
                }
            }
        }
    }
    /* Distrugge il thread una volta terminata l'esecuzione */
    pthread_mutex_destroy(&mutex);
    /* Chiude la connessione con il socket del client */
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
    /* Riceve la struct `GreenPass` dalla `client_sock` */
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
    /* Lock del mutex */
    if (pthread_mutex_lock(&mutex) != 0) {
        perror("Errore nel lock del mutex");
        close(client_sock);
        return NULL;
    }
    /* Apre un file di testo per eseguire delle operazioni di lettura e scrittura */
    FILE *green_pass_file = fopen("green_pass.txt", "w+");
    if (green_pass_file == NULL) {
        perror("Errore durante l'apertura del file");
        close(client_sock);
        pthread_mutex_unlock(&mutex);
        return NULL;
    }
    int response;
    /* Scrittura del GreenPass sul file */
    switch (green_pass.service) {
        case 0: {
            rewind(green_pass_file); // riavvolge il file fino all'inizio
            char buffer[TESSERA_LENGTH + 1]; // buffer per leggere le righe del file
            while (fgets(buffer, sizeof(buffer), green_pass_file)) { // legge ogni riga del file fino alla fine
                if (strncmp(buffer, green_pass.tessera_sanitaria, TESSERA_LENGTH) == 0) { // verifica se la tessera sanitaria è già presente all'interno del file
                    pthread_mutex_unlock(&mutex); // sblocca il mutex
                    fflush(green_pass_file); // svuota il buffer di scrittura
                    fclose(green_pass_file); // chiude il file
                    response = 0; // caso in cui la tessera sanitaria sia già presente nel file
                    /* Invia la risposta al `client_sock` */
                    if (send(client_sock, &response, sizeof(int), 0) == -1) {
                        perror("Errore nell'invio della risposta al client");
                        close(client_sock);
                        return NULL;
                    }
                    close(client_sock); // chiude la `client_sock`
                    return NULL;
                }
            }
            /* Formatta le date di validità del GreenPass */
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
    /* Unlock del mutex */
    pthread_mutex_unlock(&mutex);
    /* Invio della risposta al `client_sock` */
    response = 1;
    if (send(client_sock, &response, sizeof(int), 0) == -1) {
        perror("Errore nell'invio della risposta al client");
        close(client_sock);
        return NULL;
    }
    /* Chiusura del file descriptor collegato al file */
    fclose(green_pass_file);
    /* Chiusura della connessione con il `client_sock` */
    close(client_sock);
    return NULL;
}

void sigint_handler(int sig) {
    isRunning = 0;
}
