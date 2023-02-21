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
    if(argc != 2) {
        printf("Parametri in input insufficienti.\nEseguire seguendo la sintassi: ./clientS <CODICE_TESSERA>\n");
        exit(EXIT_FAILURE);
    }

    int client_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (client_sock == -1) {
        perror("Errore durante la creazione del socket");
        exit(EXIT_FAILURE);
    }

    // Connessione al serverG
    struct sockaddr_in serverG_address;
    serverG_address.sin_family = AF_INET;
    serverG_address.sin_port = htons(SERVERG_PORT);
    serverG_address.sin_addr.s_addr = inet_addr(LOCAL_HOST);
    if (connect(client_sock, (struct sockaddr *) &serverG_address, sizeof(serverG_address)) == -1) {
        perror("Errore durante la connessione al serverG");
        close(client_sock);
        exit(EXIT_FAILURE);
    }

    struct GreenPass check_green_pass;
    strncpy(check_green_pass.tessera_sanitaria, argv[1], TESSERA_LENGTH);
    // Convertiamo la stringa ricevuta in una maiuscola
    for (size_t i = 0; check_green_pass.tessera_sanitaria[i] != '\0'; i++) {
        check_green_pass.tessera_sanitaria[i] = toupper(check_green_pass.tessera_sanitaria[i]);
    }
    check_green_pass.service = CHECK_GP;

    // Invio del green pass al serverG
    if (send(client_sock, &check_green_pass, sizeof(check_green_pass), 0) == -1) {
        perror("Errore nell'invio al serverG");
        close(client_sock);
        exit(EXIT_FAILURE);
    }

    int response;
    if (recv(client_sock, &response, sizeof(response), 0) < 0) {
        perror("Errore durante la ricezione della risposta dal centro vaccinale");
        exit(EXIT_FAILURE);
    }

    // Controlla se il Green Pass è valido o meno
    if (response == 0) {
        printf("Green pass non valido\n");
    } else {
        printf("Green pass valido\n");
    }

    close(client_sock);

    return 0;
}