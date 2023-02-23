#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "green_pass.h"
#include "addresses.h"

int main(int argc, char * argv[]) {
    /* Controlla i parametri ricevuti da riga di comando */
    if(argc != 2) {
        printf("Parametri in input insufficienti.\nEseguire seguendo la sintassi: ./clientS <CODICE_TESSERA>\n");
        exit(EXIT_FAILURE);
    }

    /* Inizializza la socket per la connessione */
    int client_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (client_sock == -1) {
        perror("Errore durante la creazione del socket");
        exit(EXIT_FAILURE);
    }

    /* Crea la struct `sockaddr_in` per stabilire la connessione */
    struct sockaddr_in serverG_address;
    /* Inizializza tutti i campi della struct `sockaddr_in` */
    serverG_address.sin_family = AF_INET; // utilizza un protocollo IPv4
    serverG_address.sin_addr.s_addr = inet_addr(LOCAL_HOST); // ascolta sull'interfaccia di rete locale
    serverG_address.sin_port = htons(SERVERG_PORT); // si mette in ascolto sulla porta 8100
    /* Connessione con il server G */
    if (connect(client_sock, (struct sockaddr *) &serverG_address, sizeof(serverG_address)) == -1) {
        perror("Errore durante la connessione al serverG");
        close(client_sock);
        exit(EXIT_FAILURE);
    }

    struct GreenPass check_green_pass;
    strncpy(check_green_pass.tessera_sanitaria, argv[1], TESSERA_LENGTH);
    /* Convertiamo la stringa ricevuta in una stringa interamente maiuscola */
    for (size_t i = 0; check_green_pass.tessera_sanitaria[i] != '\0'; i++) {
        check_green_pass.tessera_sanitaria[i] = toupper(check_green_pass.tessera_sanitaria[i]);
    }
    /* Imposta il servizio che vuole richiedere al server */
    check_green_pass.service = CHECK_GP;

    /* Invio del green pass al serverG */
    if (send(client_sock, &check_green_pass, sizeof(check_green_pass), 0) == -1) {
        perror("Errore nell'invio al serverG");
        close(client_sock); // chiude la socket del client
        exit(EXIT_FAILURE);
    }

    int response;
    /* Aspetta di ricevere una risposta dal server */
    if (recv(client_sock, &response, sizeof(response), 0) < 0) {
        perror("Errore durante la ricezione della risposta dal centro vaccinale");
        exit(EXIT_FAILURE);
    }

    /* Controlla se il Green Pass è valido o meno */
    printf("%d\n", response);
    if (response == 0) {
        printf("Green pass scaduto\n");
    } else if (response == 1) {
        printf("Green pass valido\n");
    } else if (response == -1){
        printf("Green pass non valido\n");
    } else {
        printf("Green pass non trovato\n");
    }

    close(client_sock); // chiude la socket del client

    return 0;
}