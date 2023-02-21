#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>
#include "green_pass.h"

#define SERVERV_PORT 8890
#define CENTER_PORT 8888

int main() {
    // Crea il file descriptor di un nuovo socket utilizzando il protocollo TCP (`SOCK_STREAM`)
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("Errore durante la creazione del socket");
        exit(EXIT_FAILURE);
    }

    // Inizializza la struttura `sockaddr_in` con i dati necessari per il server
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET; // Il tipo di connessione, `AF_INET` indica l'utilizzo del protocollo IPv4
    server_address.sin_addr.s_addr = INADDR_ANY; // Utilizziamo l'indirizzo del server per accettare connessioni da qualsiasi indirizzo
    server_address.sin_port = htons(SERVERV_PORT); // Con `htons` convertiamo la costante `CENTER_PORT` in un formato a 16 bit

    // Assegna l'indirizzo al socket
    if (bind(server_fd, (struct sockaddr *) &server_address, sizeof(server_address)) < 0) {
        perror("Errore durante l'assegnazione dell'indirizzo al socket");
        exit(EXIT_FAILURE);
    }

    // Si mette in ascolto di nuove connessioni
    if (listen(server_fd, 1024) < 0) { // Il secondo parametro indica il numero massimo di richieste di connessione pendenti
        perror("Errore durante l'ascolto di nuove connessioni");
        exit(EXIT_FAILURE);
    }

    // Accettiamo nuove connessioni in modo indefinito
    while (1) {
        struct sockaddr_in client_address;
        int client_fd;

        /* Accetta una connessione in arrivo e restituisce il file descriptor del socket dedicato alla nuova
         * connessione. In caso di errore, la funzione restituisce un valore negativo. Il secondo argomento
         * è un puntatore a una struttura `sockaddr`. */
        int client_address_length = sizeof(client_address);
        if ((client_fd = accept(server_fd, (struct sockaddr *) &client_address, (socklen_t *) &client_address_length)) < 0) {
            perror("Errore durante la connessione di un nuovo client");
            exit(EXIT_FAILURE);
        }

        printf("Nuova connessione accettata\n");

        struct GreenPass green_pass;

        // Riceviamo il codice della tessera sanitaria dal client
        if (recv(client_fd, &green_pass, sizeof(green_pass), 0) < 0) {
            perror("Errore durante la ricezione del codice della tessera sanitaria dal client");
            exit(EXIT_FAILURE);
        }
        // Controlliamo se il codice della tessera sanitaria è già stato inserito
        bool tessera_sanitaria_presente = false;

        // Apriamo il file in lettura e scrittura, in modo da poter salvare le informazioni dei green pass validi
        FILE *green_pass_file = fopen("green_pass.txt", "w+");
        if (green_pass_file == NULL) {
            perror("Errore durante l'apertura del file");
            exit(EXIT_FAILURE);
        }

        char line[TESSERA_LENGTH + 1];
        while (fgets(line, sizeof(line), green_pass_file)) {
            if (strncmp(line, green_pass.tessera_sanitaria, TESSERA_LENGTH) == 0) {
                tessera_sanitaria_presente = true;
                break;
            }
        }
        // Scriviamo le informazioni sul file solo se la tessera sanitaria non è già presente
        if (!tessera_sanitaria_presente) {
            green_pass_file = fopen("green_pass.txt", "a");
            if (green_pass_file == NULL) {
                perror("Errore durante l'apertura del file");
                exit(EXIT_FAILURE);
            }

            struct tm *from_ptr = localtime(&green_pass.valid_from);
            struct tm *until_ptr = localtime(&green_pass.valid_until);

            char valid_from[11];
            strftime(valid_from, sizeof(valid_from), "%d/%m/%Y", from_ptr);
            char valid_until[11];
            strftime(valid_until, sizeof(valid_until), "%d/%m/%Y", until_ptr);
            printf("%s : %s : %s\n", green_pass.tessera_sanitaria, valid_from, valid_until);
            fprintf(green_pass_file, "%s : %s : %s\n", green_pass.tessera_sanitaria, valid_from, valid_until);
        }

        fclose(green_pass_file);

        // Inviamo la risposta al client
        int response = !tessera_sanitaria_presente;
        if (send(client_fd, &response, sizeof(response), 0) < 0) {
            perror("Errore durante l'invio della risposta al client");
            exit(EXIT_FAILURE);
        }

        //close(client_fd);
    }

    close(server_fd);
    return 0;
}

/* Funzione che si occupa di verificare se un green pass è valido. Riceve in ingresso un oggetto GreenPass
 * e restituisce un intero: uno se il green pass è valido, zero altrimenti. */
/*int check_green_pass(struct GreenPass green_pass) {
    time_t current_time;
    time(&current_time);
    if (current_time < green_pass.valid_from || current_time > green_pass.valid_until) {
        return 0;
    }
    return 1;
}*/