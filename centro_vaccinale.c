#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "green_pass.h"

#define SERVERV_ADDRESS "127.0.0.1"
#define SERVERV_PORT 8889
#define CENTER_PORT 8888

int main() {
    int centers_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (centers_sock == -1) {
        perror("Errore durante la creazione del socket del centro vaccinale");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in centers_addr;
    centers_addr.sin_family = AF_INET;
    centers_addr.sin_addr.s_addr = INADDR_ANY;
    centers_addr.sin_port = htons(CENTER_PORT);

    if (bind(centers_sock, (struct sockaddr *) &centers_addr, sizeof(centers_addr)) < 0) {
        perror("Errore durante il binding del socket del centro vaccinale");
        exit(EXIT_FAILURE);
    }

    listen(centers_sock, 1);

    int servv_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (servv_sock == -1) {
        perror("Errore durante la creazione del socket per ServerV");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in servv_addr;
    servv_addr.sin_family = AF_INET;
    servv_addr.sin_addr.s_addr = inet_addr(SERVERV_ADDRESS);
    servv_addr.sin_port = htons(SERVERV_PORT);

    if (connect(servv_sock, (struct sockaddr *) &servv_addr, sizeof(servv_addr)) < 0) {
        perror("Errore durante la connessione a ServerV");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in client_addr;
    int client_len = sizeof(client_addr);

    while (1) {
        int client_sock = accept(centers_sock, (struct sockaddr *) &client_addr, (socklen_t *) &client_len);
        if (client_sock < 0) {
            perror("Errore durante la connessione al client");
            exit(EXIT_FAILURE);
        }

        struct GreenPass green_pass;
        if (recv(client_sock, &green_pass, sizeof(green_pass), 0) < 0) {
            perror("Errore durante la ricezione del codice della tessera sanitaria dal client");
            close(client_sock);
            continue;
        }

        if (green_pass.tessera_sanitaria[0] == '\0') {
            close(client_sock);
            break;
        }

        green_pass.valid_from = time(NULL);
        green_pass.valid_until = (time(NULL) + 30 * 24 * 60 * 60);

        if (send(servv_sock, &green_pass, sizeof(green_pass), 0) < 0) {
            perror("Errore durante l'invio del codice della tessera sanitaria a ServerV");
            close(client_sock);
            continue;
        }

        close(client_sock);
    }

    close(servv_sock);

    return 0;
}