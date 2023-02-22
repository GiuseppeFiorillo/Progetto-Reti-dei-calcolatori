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
    if (argc != 2) {
        printf("Parametri in input insufficienti.\nEseguire seguendo la sintassi: ./client <CODICE_TESSERA>\n");
        exit(EXIT_FAILURE);
    }

    /* Crea il file descriptor di un nuovo socket utilizzando il protocollo TCP (`SOCK_STREAM`).
     * In caso di errore, la funzione socket restituisce -1 */
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
        perror("Errore durante la connessione al server");
        close(client_sock);
        exit(EXIT_FAILURE);
    }

    struct GreenPass green_pass;
    strncpy(green_pass.tessera_sanitaria, argv[1], TESSERA_LENGTH);
    for (size_t i = 0; green_pass.tessera_sanitaria[i] != '\0'; ++i) {
        green_pass.tessera_sanitaria[i] = toupper(green_pass.tessera_sanitaria[i]);
    }
    green_pass.service = VALIDATION_GP;

    // Invio del codice al serverG
    if (send(client_sock, &green_pass, sizeof(green_pass), 0) == -1) {
        perror("Errore durante l'invio del codice della tessera sanitaria al centro vaccinale");
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